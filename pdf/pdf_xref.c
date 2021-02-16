/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* xref parsing */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_xref.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_repair.h"

static int resize_xref(pdf_context *ctx, uint64_t new_size)
{
    xref_entry *new_xrefs;

    /* Although we can technically handle object numbers larger than this, on some systems (32-bit Windows)
     * memset is limited to a (signed!) integer for the size of memory to clear. We could deal
     * with this by clearing the memory in blocks, but really, this is almost certainly a
     * corrupted file or something.
     */
    if (new_size >= (0x7ffffff / sizeof(xref_entry)))
        return_error(gs_error_rangecheck);

    new_xrefs = (xref_entry *)gs_alloc_bytes(ctx->memory, (new_size) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
    if (new_xrefs == NULL){
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return_error(gs_error_VMerror);
    }
    memset(new_xrefs, 0x00, (new_size) * sizeof(xref_entry));
    memcpy(new_xrefs, ctx->xref_table->xref, ctx->xref_table->xref_size * sizeof(xref_entry));
    gs_free_object(ctx->memory, ctx->xref_table->xref, "reallocated xref entries");
    ctx->xref_table->xref = new_xrefs;
    ctx->xref_table->xref_size = new_size;
    return 0;
}

static int read_xref_stream_entries(pdf_context *ctx, pdf_c_stream *s, uint64_t first, uint64_t last, uint64_t *W)
{
    uint i, j;
    uint field_width = 0;
    uint32_t type = 0;
    uint64_t objnum = 0, gen = 0;
    byte *Buffer;
    int64_t bytes = 0;
    xref_entry *entry;

    /* Find max number of bytes to be read */
    field_width = W[0];
    if (W[1] > field_width)
        field_width = W[1];
    if (W[2] > field_width)
        field_width = W[2];

    Buffer = gs_alloc_bytes(ctx->memory, field_width, "read_xref_stream_entry working buffer");
    for (i=first;i<=last; i++){
        /* Defaults if W[n] = 0 */
        type = 1;
        objnum = gen = 0;

        if (W[0] != 0) {
            type = 0;
            bytes = pdfi_read_bytes(ctx, Buffer, 1, W[0], s);
            if (bytes < W[0]){
                gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer (error)");
                return_error(gs_error_ioerror);
            }
            for (j=0;j<W[0];j++)
                type = (type << 8) + Buffer[j];
        }

        if (W[1] != 0) {
            bytes = pdfi_read_bytes(ctx, Buffer, 1, W[1], s);
            if (bytes < W[1]){
                gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry free working buffer (error)");
                return_error(gs_error_ioerror);
            }
            for (j=0;j<W[1];j++)
                objnum = (objnum << 8) + Buffer[j];
        }

        if (W[2] != 0) {
            bytes = pdfi_read_bytes(ctx, Buffer, 1, W[2], s);
            if (bytes < W[2]){
                gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer (error)");
                return_error(gs_error_ioerror);
            }
            for (j=0;j<W[2];j++)
                gen = (gen << 8) + Buffer[j];
        }

        entry = &ctx->xref_table->xref[i];
        if (entry->object_num != 0)
            continue;

        entry->compressed = false;
        entry->free = false;
        entry->object_num = i;
        entry->cache = NULL;

        switch(type) {
            case 0:
                entry->free = true;
                entry->u.uncompressed.offset = objnum;         /* For free objects we use the offset to store the object number of the next free object */
                entry->u.uncompressed.generation_num = gen;    /* And the generation number is the numebr to use if this object is used again */
                break;
            case 1:
                entry->u.uncompressed.offset = objnum;
                entry->u.uncompressed.generation_num = gen;
                break;
            case 2:
                entry->compressed = true;
                entry->u.compressed.compressed_stream_num = objnum;   /* The object number of the compressed stream */
                entry->u.compressed.object_index = gen;               /* And the index of the object within the stream */
                break;
            default:
                gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer");
                return_error(gs_error_rangecheck);
                break;
        }
    }
    gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer");
    return 0;
}

/* Forward definition */
static int read_xref(pdf_context *ctx, pdf_c_stream *s);
/* These two routines are recursive.... */
static int pdfi_read_xref_stream_dict(pdf_context *ctx, pdf_c_stream *s);

static int pdfi_process_xref_stream(pdf_context *ctx, pdf_stream *stream_obj, pdf_c_stream *s)
{
    pdf_c_stream *XRefStrm;
    int code, i;
    pdf_dict *sdict = NULL;
    pdf_name *n;
    pdf_array *a;
    int64_t size;
    int64_t num;
    int64_t W[3];
    bool known = false;

    if (stream_obj->type != PDF_STREAM)
        return_error(gs_error_typecheck);

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &sdict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_type(ctx, sdict, "Type", PDF_NAME, (pdf_obj **)&n);
    if (code < 0)
        return code;

    if (n->length != 4 || memcmp(n->data, "XRef", 4) != 0) {
        pdfi_countdown(n);
        return_error(gs_error_syntaxerror);
    }
    pdfi_countdown(n);

    code = pdfi_dict_get_int(ctx, sdict, "Size", &size);
    if (code < 0)
        return code;

    if (size < 0)
        return_error(gs_error_rangecheck);

    /* If this is the first xref stream then allocate the xref table and store the trailer */
    if (ctx->xref_table == NULL) {
        ctx->xref_table = (xref_table_t *)gs_alloc_bytes(ctx->memory, sizeof(xref_table_t), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL) {
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table_t));
        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, size * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            gs_free_object(ctx->memory, ctx->xref_table, "failed to allocate xref table entries");
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table->xref, 0x00, size * sizeof(xref_entry));
        ctx->xref_table->ctx = ctx;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = size;
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif
        pdfi_countup(ctx->xref_table);

        ctx->Trailer = sdict;
        pdfi_countup(sdict);
    } else {
        code = pdfi_merge_dicts(ctx, ctx->Trailer, sdict);
        if (code < 0) {
            if (code == gs_error_VMerror || ctx->args.pdfstoponerror)
                return code;
        }
    }

    pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, stream_obj), SEEK_SET);

    /* Bug #691220 has a PDF file with a compressed XRef, the stream dictionary has
     * a /DecodeParms entry for the stream, which has a /Colors value of 5, which makes
     * *no* sense whatever. If we try to apply a Predictor then we end up in a loop trying
     * to read 5 colour samples. Rather than meddles with more parameters to the filter
     * code, we'll just remove the Colors entry from the DecodeParms dictionary,
     * because it is nonsense. This means we'll get the (sensible) default value of 1.
     */
    code = pdfi_dict_known(ctx, sdict, "DecodeParms", &known);
    if (code < 0)
        return code;

    if (known) {
        pdf_dict *DP;
        double f;
        pdf_obj *name;

        code = pdfi_dict_get_type(ctx, sdict, "DecodeParms", PDF_DICT, (pdf_obj **)&DP);
        if (code < 0)
            return code;

        code = pdfi_dict_knownget_number(ctx, DP, "Colors", &f);
        if (code < 0) {
            pdfi_countdown(DP);
            return code;
        }
        if (code > 0 && f != (double)1)
        {
            code = pdfi_name_alloc(ctx, (byte *)"Colors", 6, &name);
            if (code < 0) {
                pdfi_countdown(DP);
                return code;
            }
            pdfi_countup(name);

            code = pdfi_dict_delete_pair(ctx, DP, (pdf_name *)name);
            pdfi_countdown(name);
            if (code < 0) {
                pdfi_countdown(DP);
                return code;
            }
        }
        pdfi_countdown(DP);
    }

    code = pdfi_filter_no_decryption(ctx, stream_obj, s, &XRefStrm, false);
    if (code < 0) {
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    code = pdfi_dict_get_type(ctx, sdict, "W", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0) {
        pdfi_close_file(ctx, XRefStrm);
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    if (pdfi_array_size(a) != 3) {
        pdfi_countdown(a);
        pdfi_close_file(ctx, XRefStrm);
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return_error(gs_error_rangecheck);
    }
    for (i=0;i<3;i++) {
        code = pdfi_array_get_int(ctx, a, (uint64_t)i, (int64_t *)&W[i]);
        if (code < 0) {
            pdfi_countdown(a);
            pdfi_close_file(ctx, XRefStrm);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    }
    pdfi_countdown(a);

    code = pdfi_dict_get_type(ctx, sdict, "Index", PDF_ARRAY, (pdf_obj **)&a);
    if (code == gs_error_undefined) {
        code = read_xref_stream_entries(ctx, XRefStrm, 0, size - 1, (uint64_t *)W);
        if (code < 0) {
            pdfi_close_file(ctx, XRefStrm);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    } else {
        int64_t start, end;

        if (code < 0) {
            pdfi_close_file(ctx, XRefStrm);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }

        if (pdfi_array_size(a) & 1) {
            pdfi_countdown(a);
            pdfi_close_file(ctx, XRefStrm);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_rangecheck);
        }

        for (i=0;i < pdfi_array_size(a);i+=2){
            code = pdfi_array_get_int(ctx, a, (uint64_t)i, &start);
            if (code < 0) {
                pdfi_countdown(a);
                pdfi_close_file(ctx, XRefStrm);
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }

            code = pdfi_array_get_int(ctx, a, (uint64_t)i+1, &end);
            if (code < 0) {
                pdfi_countdown(a);
                pdfi_countdown(start);
                pdfi_close_file(ctx, XRefStrm);
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }

            if (start + end >= ctx->xref_table->xref_size) {
                code = resize_xref(ctx, start + end);
                if (code < 0)
                    return code;
            }

            code = read_xref_stream_entries(ctx, XRefStrm, start, start + end - 1, (uint64_t *)W);
            if (code < 0) {
                pdfi_countdown(a);
                pdfi_close_file(ctx, XRefStrm);
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }
        }
    }
    pdfi_countdown(a);

    pdfi_close_file(ctx, XRefStrm);

    code = pdfi_dict_get_int(ctx, sdict, "Prev", &num);
    if (code == gs_error_undefined)
        return 0;

    if (code < 0)
        return code;

    if (num < 0 || num > ctx->main_stream_length)
        return_error(gs_error_rangecheck);

    if (pdfi_loop_detector_check_object(ctx, num) == true)
        return_error(gs_error_circular_reference);
    else {
        code = pdfi_loop_detector_add_object(ctx, num);
        if (code < 0)
            return code;
    }

    if(ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Reading /Prev xref\n");

    pdfi_seek(ctx, s, num, SEEK_SET);

    code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
    if (code < 0)
        return code;

    if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == TOKEN_XREF) {
        /* Read old-style xref table */
        pdfi_pop(ctx, 1);
        return(read_xref(ctx, ctx->main_stream));
    } else
        code = pdfi_read_xref_stream_dict(ctx, s);

    return code;
}

static int pdfi_read_xref_stream_dict(pdf_context *ctx, pdf_c_stream *s)
{
    int code;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading PDF 1.5+ xref stream\n");

    if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_INT) {
        /* Its an integer, lets try for index gen obj as a XRef stream */
        code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);

        if (code < 0)
            return(pdfi_repair_file(ctx));

        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
            /* Second element is not an integer, not a valid xref */
            pdfi_pop(ctx, 1);
            return(pdfi_repair_file(ctx));
        }

        code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
        if (code < 0) {
            pdfi_pop(ctx, 1);
            return code;
        }

        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
            /* Second element is not an integer, not a valid xref */
            pdfi_pop(ctx, 2);
            return(pdfi_repair_file(ctx));
        } else {
            int obj_num, gen_num;

            pdf_keyword *keyword = (pdf_keyword *)ctx->stack_top[-1];

            if (keyword->key != TOKEN_OBJ) {
                pdfi_pop(ctx, 3);
                return(pdfi_repair_file(ctx));
            }
            /* pop the 'obj', generation and object numbers */
            pdfi_pop(ctx, 1);
            gen_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
            pdfi_pop(ctx, 1);
            obj_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
            pdfi_pop(ctx, 1);

            do {
                code = pdfi_read_token(ctx, ctx->main_stream, obj_num, gen_num);
                if (code < 0)
                    return pdfi_repair_file(ctx);

                if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD) {
                    keyword = (pdf_keyword *)ctx->stack_top[-1];
                    if (keyword->key == TOKEN_STREAM) {
                        pdf_dict *dict;
                        pdf_stream *sdict = NULL;
                        int64_t Length;

                        /* Remove the 'stream' token from the stack, should leave a dictionary object on the stack */
                        pdfi_pop(ctx, 1);
                        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_DICT) {
                            pdfi_pop(ctx, 1);
                            return pdfi_repair_file(ctx);
                        }
                        dict = (pdf_dict *)ctx->stack_top[-1];

                        /* Convert the dict into a stream (sdict comes back with at least one ref) */
                        code = pdfi_obj_dict_to_stream(ctx, dict, &sdict);
                        if (code < 0) {
                            pdfi_pop(ctx, 1);
                            /* TODO: should I return code instead of trying to repair?
                             * Normally the above routine should not fail so something is
                             * probably seriously fubar.
                             */
                            return pdfi_repair_file(ctx);
                        }
                        /* Pop off the dict */
                        pdfi_pop(ctx, 1);
                        dict = NULL;

                        /* Init the stuff for the stream */
                        sdict->stream_offset = pdfi_unread_tell(ctx);
                        sdict->object_num = obj_num;
                        sdict->generation_num = gen_num;

                        code = pdfi_dict_get_int(ctx, sdict->stream_dict, "Length", &Length);
                        if (code < 0) {
                            /* TODO: Not positive this will actually have a length -- just use 0 */
                            dmprintf1(ctx->memory, "Xref Stream object %u missing mandatory keyword /Length\n", obj_num);
                            ctx->pdf_errors |= E_PDF_BADSTREAM;
                            code = 0;
                            Length = 0;
                        }
                        sdict->Length = Length;
                        sdict->length_valid = true;

                        code = pdfi_process_xref_stream(ctx, sdict, ctx->main_stream);
                        if (code < 0) {
                            pdfi_countdown(sdict);
                            return (pdfi_repair_file(ctx));
                        }
                        pdfi_countdown(sdict);
                        break;
                    }
                    if (keyword->key == TOKEN_ENDOBJ) {
                        /* Something went wrong, this is not a stream dictionary */
                        pdfi_pop(ctx, 3);
                        return(pdfi_repair_file(ctx));
                        break;
                    }
                }
            } while(1);
        }
    } else {
        /* Not an 'xref' and not an integer, so not a valid xref */
        return(pdfi_repair_file(ctx));
    }
    return 0;
}

static int skip_to_digit(pdf_context *ctx, pdf_c_stream *s, unsigned int limit)
{
    byte c;
    int bytes, read = 0;

    do {
        bytes = pdfi_read_bytes(ctx, &c, 1, 1, s);
        if (bytes == 0)
            return_error(gs_error_ioerror);
        if (c >= 0x30 && c <= 0x39) {
            pdfi_unread(ctx, s, &c, 1);
            break;
        }
        read += bytes;
    }while (read < limit);
    return read;
}

static int read_digits(pdf_context *ctx, pdf_c_stream *s, byte *Buffer, unsigned int limit)
{
    int bytes, read = 0;

    do {
        bytes = pdfi_read_bytes(ctx, &Buffer[read], 1, 1, s);
        if (bytes == 0)
            return_error(gs_error_ioerror);
        if (Buffer[read] < 0x30 || Buffer[read] > 0x39) {
            pdfi_unread(ctx, s, &Buffer[read], 1);
            break;
        }
        read += bytes;
    }while (read < limit);
    return read;
}


static int read_xref_entry_slow(pdf_context *ctx, pdf_c_stream *s, gs_offset_t *offset, uint32_t *generation_num, unsigned char *free)
{
    byte Buffer[20];
    int code, read = 0, bytes;

    /* First off, find a number. If we don't find one, and read 20 bytes, throw an error */
    code = skip_to_digit(ctx, s, 20);
    if (code < 0)
        return code;
    read += code;

    /* Now read a number */
    code = read_digits(ctx, s, (byte *)&Buffer,  (read > 10 ? 20 - read : 10));
    if (code < 0)
        return code;
    Buffer[code] = 0x00;
    read += code;

    *offset = atol((const char *)Buffer);

    /* find next number */
    code = skip_to_digit(ctx, s, 20 - read);
    if (code < 0)
        return code;
    read += code;

    /* and read it */
    code = read_digits(ctx, s, (byte *)&Buffer,  (read > 15 ? 20 - read : 5));
    if (code < 0)
        return code;
    Buffer[code] = 0x00;
    read += code;

    *generation_num = atol((const char *)Buffer);

    do {
        bytes = pdfi_read_bytes(ctx, &Buffer[0], 1, 1, s);
        if (bytes == 0)
            return_error(gs_error_ioerror);
        read += bytes;
        if (Buffer[0] == 0x09 || Buffer[0] == 0x20)
            continue;
        if (Buffer[0] == 'n' || Buffer[0] == 'f') {
            *free = Buffer[0];
            break;
        } else {
            return_error(gs_error_syntaxerror);
        }
    } while (read < 20);
    if (read >= 20)
        return_error(gs_error_syntaxerror);

    do {
        bytes = pdfi_read_bytes(ctx, &Buffer[0], 1, 1, s);
        read += bytes;
        if (Buffer[0] == 0x20 || Buffer[0] == 0x09 || Buffer[0] == 0x0d || Buffer[0] == 0x0a)
            continue;
    } while (read < 20);
    return 0;
}

static int write_offset(byte *B, gs_offset_t o, unsigned int g, unsigned char free)
{
    byte b[20], *ptr = B;
    int index = 0;

    gs_sprintf((char *)b, "%"PRId64"", o);
    if (strlen((const char *)b) > 10)
        return_error(gs_error_rangecheck);
    for(index=0;index < 10 - strlen((const char *)b); index++) {
        *ptr++ = 0x30;
    }
    memcpy(ptr, b, strlen((const char *)b));
    ptr += strlen((const char *)b);
    *ptr++ = 0x20;

    gs_sprintf((char *)b, "%d", g);
    if (strlen((const char *)b) > 5)
        return_error(gs_error_rangecheck);
    for(index=0;index < 5 - strlen((const char *)b);index++) {
        *ptr++ = 0x30;
    }
    memcpy(ptr, b, strlen((const char *)b));
    ptr += strlen((const char *)b);
    *ptr++ = 0x20;
    *ptr++ = free;
    *ptr++ = 0x20;
    *ptr++ = 0x0d;
    return 0;
}

static int read_xref_section(pdf_context *ctx, pdf_c_stream *s, uint64_t *section_start, uint64_t *section_size)
{
    int code = 0, i, j;
    pdf_obj *o = NULL;
    uint64_t start = 0, size = 0;
    int64_t bytes = 0;
    char Buffer[21];

    *section_start = *section_size = 0;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading xref section\n");

    code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);

    if (code < 0)
        return code;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    o = ctx->stack_top[-1];
    if (o->type == PDF_KEYWORD)
        return 0;

    if (o->type != PDF_INT) {
        /* element is not an integer, not a valid xref */
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }

    *section_start = start = ((pdf_num *)o)->value.i;

    code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        return code;
    }

    o = ctx->stack_top[-1];
    if (o->type != PDF_INT) {
        /* element is not an integer, not a valid xref */
        pdfi_pop(ctx, 2);
        return_error(gs_error_typecheck);
    }
    *section_size = size = ((pdf_num *)o)->value.i;
    pdfi_pop(ctx, 2);

    if (ctx->args.pdfdebug)
        dmprintf2(ctx->memory, "\n%% Section starts at %d and has %d entries\n", (unsigned int) start, (unsigned int)size);

    if (size > 0) {
        if (ctx->xref_table == NULL) {
            ctx->xref_table = (xref_table_t *)gs_alloc_bytes(ctx->memory, sizeof(xref_table_t), "read_xref_stream allocate xref table");
            if (ctx->xref_table == NULL)
                return_error(gs_error_VMerror);
            memset(ctx->xref_table, 0x00, sizeof(xref_table_t));

            ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, (start + size) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
            if (ctx->xref_table->xref == NULL){
                gs_free_object(ctx->memory, ctx->xref_table, "free xref table on error allocating entries");
                ctx->xref_table = NULL;
                return_error(gs_error_VMerror);
            }
#if REFCNT_DEBUG
            ctx->xref_table->UID = ctx->UID++;
            dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif

            memset(ctx->xref_table->xref, 0x00, (start + size) * sizeof(xref_entry));
            ctx->xref_table->ctx = ctx;
            ctx->xref_table->type = PDF_XREF_TABLE;
            ctx->xref_table->xref_size = start + size;
            pdfi_countup(ctx->xref_table);
        } else {
            if (start + size > ctx->xref_table->xref_size) {
                code = resize_xref(ctx, start + size);
                if (code < 0)
                    return code;
            }
        }
    }

    pdfi_skip_white(ctx, s);
    for (i=0;i< size;i++){
        xref_entry *entry = &ctx->xref_table->xref[i + start];
        unsigned char free;
        gs_offset_t off;
        unsigned int gen;

        bytes = pdfi_read_bytes(ctx, (byte *)Buffer, 1, 20, s);
        if (bytes < 20)
            return_error(gs_error_ioerror);
        j = 19;
        while (Buffer[j] != 0x0D && Buffer[j] != 0x0A) {
            pdfi_unread(ctx, s, (byte *)&Buffer[j], 1);
            if (--j < 0) {
                dmprintf(ctx->memory, "Invalid xref entry, line terminator missing.\n");
                code = read_xref_entry_slow(ctx, s, &off, &gen, &free);
                if (code < 0)
                    return code;
                code = write_offset((byte *)Buffer, off, gen, free);
                if (code < 0)
                    return code;
                break;
            }
        }
        Buffer[j] = 0x00;
        if (entry->object_num != 0)
            continue;

        if (sscanf(Buffer, "%ld %d %c", &entry->u.uncompressed.offset, &entry->u.uncompressed.generation_num, &free) != 3) {
            dmprintf(ctx->memory, "Invalid xref entry, incorrect format.\n");
            pdfi_unread(ctx, s, (byte *)Buffer, 20);
            code = read_xref_entry_slow(ctx, s, &off, &gen, &free);
            if (code < 0)
                return code;
            code = write_offset((byte *)Buffer, off, gen, free);
            if (code < 0)
                return code;
        }

        entry->compressed = false;
        entry->object_num = i + start;
        if (free == 'f')
            entry->free = true;
        if(free == 'n')
            entry->free = false;
    }

    return 0;
}

static int read_xref(pdf_context *ctx, pdf_c_stream *s)
{
    int code = 0;
    pdf_obj **o = NULL;
    pdf_keyword *k;
    pdf_dict *d = NULL;
    uint64_t size = 0, max_obj = 0;
    int64_t num;

    do {
        uint64_t section_start, section_size;

        o = ctx->stack_top;
        code = read_xref_section(ctx, s, &section_start, &section_size);
        if (code < 0)
            return code;

        if (section_start + section_size > max_obj)
            max_obj = section_start + section_size;

        if (ctx->stack_top - o > 0) {
            k = (pdf_keyword *)ctx->stack_top[-1];
            if(k->type != PDF_KEYWORD || k->key != TOKEN_TRAILER)
                return_error(gs_error_syntaxerror);
            else {
                pdfi_pop(ctx, 1);
                break;
            }
        }
    } while (1);

    code = pdfi_read_dict(ctx, ctx->main_stream, 0, 0);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    if (d->type != PDF_DICT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }

    if (ctx->Trailer == NULL) {
        ctx->Trailer = d;
        pdfi_countup(d);
    } else {
        code = pdfi_merge_dicts(ctx, ctx->Trailer, d);
        if (code < 0) {
            if (code == gs_error_VMerror || ctx->args.pdfstoponerror)
                return code;
        }
    }

    /* We have the Trailer dictionary. First up check for hybrid files. These have the initial
     * xref starting at 0 and size of 0. In this case the /Size entry in the trailer dictionary
     * must tell us how large the xref is, and we need to allocate our xref table anyway.
     */
    if (ctx->xref_table == NULL && size == 0) {
        int64_t size;

        code = pdfi_dict_get_int(ctx, d, "Size", &size);
        if (code < 0) {
            pdfi_pop(ctx, 2);
            return code;
        }
        if (size < 0) {
            pdfi_pop(ctx, 2);
            return_error(gs_error_rangecheck);
        }

        ctx->xref_table = (xref_table_t *)gs_alloc_bytes(ctx->memory, sizeof(xref_table_t), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL) {
            pdfi_pop(ctx, 2);
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table_t));
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif

        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, size * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            pdfi_pop(ctx, 2);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }

        memset(ctx->xref_table->xref, 0x00, size * sizeof(xref_entry));
        ctx->xref_table->ctx = ctx;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = size;
        pdfi_countup(ctx->xref_table);
    }

    /* Now check if this is a hybrid file. */
    if (ctx->Trailer == d) {
        code = pdfi_dict_get_int(ctx, d, "XRefStm", &num);
        if (code < 0 && code != gs_error_undefined) {
            pdfi_pop(ctx, 2);
            return code;
        }
        if (code == 0)
            ctx->is_hybrid = true;
    } else
        code = gs_error_undefined;

    if (code == 0 && ctx->prefer_xrefstm) {
        if (ctx->args.pdfdebug)
            dmprintf(ctx->memory, "%% File is a hybrid, containing xref table and xref stream. Using the stream.\n");

        pdfi_pop(ctx, 2);

        if (pdfi_loop_detector_check_object(ctx, num) == true)
            return_error(gs_error_circular_reference);
        else {
            code = pdfi_loop_detector_add_object(ctx, num);
            if (code < 0)
                return code;
        }

        pdfi_loop_detector_mark(ctx);
        /* Because of the way the code works when we read a file which is a pure
         * xref stream file, we need to read the first integer of 'x y obj'
         * because the xref stream decoding code expects that to be on the stack.
         */
        pdfi_seek(ctx, s, num, SEEK_SET);

        code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
        if (code < 0) {
            pdfi_loop_detector_cleartomark(ctx);
            return code;
        }

        code = pdfi_read_xref_stream_dict(ctx, ctx->main_stream);
        if (code < 0) {
            pdfi_loop_detector_cleartomark(ctx);
            return code;
        }

        pdfi_loop_detector_cleartomark(ctx);
    }

    /* Not a hybrid file, so now check if this is a modified file and has
     * previous xref entries.
     */
    /* But first, check if the highest subsection + size exceeds the /Size in the
     * trailer dictionary and set a warning flag if it does
     */
    code = pdfi_dict_get_int(ctx, d, "Size", &num);
    if (code < 0) {
        return code;
    }
    if (max_obj > num)
        ctx->pdf_warnings |= W_PDF_BAD_XREF_SIZE;

    code = pdfi_dict_get_int(ctx, d, "Prev", &num);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        if (code == gs_error_undefined)
            return 0;
        else
            return code;
    }
    pdfi_pop(ctx, 1);

    if (num < 0 || num > ctx->main_stream_length)
        return_error(gs_error_rangecheck);

    if (pdfi_loop_detector_check_object(ctx, num) == true)
        return_error(gs_error_circular_reference);
    else {
        code = pdfi_loop_detector_add_object(ctx, num);
        if (code < 0)
            return code;
    }

    code = pdfi_seek(ctx, s, num, SEEK_SET);
    if (code < 0)
        return code;

    code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
    if (code < 0)
        return(code);

    if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == TOKEN_XREF) {
        /* Read old-style xref table */
        pdfi_pop(ctx, 1);
        return(read_xref(ctx, ctx->main_stream));
    } else {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
}

int pdfi_read_xref(pdf_context *ctx)
{
    int code = 0;
    bool do_repair = false;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    if (ctx->startxref != 0) {
        code = pdfi_loop_detector_add_object(ctx, ctx->startxref);
        if (code < 0)
            goto exit;

        if (ctx->args.pdfdebug)
            dmprintf(ctx->memory, "%% Trying to read 'xref' token for xref table, or 'int int obj' for an xref stream\n");

        if (ctx->startxref > ctx->main_stream_length - 5) {
            dmprintf(ctx->memory, "startxref offset is beyond end of file.\n");
            ctx->pdf_errors |= E_PDF_BADSTARTXREF;
            do_repair = true;
            goto exit;
        }


        /* Read the xref(s) */
        pdfi_seek(ctx, ctx->main_stream, ctx->startxref, SEEK_SET);

        code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
        if (code < 0) {
            dmprintf(ctx->memory, "Failed to read any token at the startxref location\n");
            ctx->pdf_errors |= E_PDF_BADSTARTXREF;
            do_repair = true;
            goto exit;
        }

        if (pdfi_count_stack(ctx) < 1) {
            code = gs_note_error(gs_error_undefined);
            goto exit;
        }

        if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == TOKEN_XREF) {
            /* Read old-style xref table */
            pdfi_pop(ctx, 1);
            code = read_xref(ctx, ctx->main_stream);
            if (code < 0) {
                do_repair = true;
                goto exit;
            }
        } else {
            code = pdfi_read_xref_stream_dict(ctx, ctx->main_stream);
            if (code < 0){
                do_repair = true;
                goto exit;
            }
        }
    } else {
        /* Attempt to repair PDF file */
        do_repair = true;
        goto exit;
    }

    if(ctx->args.pdfdebug && ctx->xref_table) {
        int i, j;
        xref_entry *entry;
        char Buffer[32];

        dmprintf(ctx->memory, "\n%% Dumping xref table\n");
        for (i=0;i < ctx->xref_table->xref_size;i++) {
            entry = &ctx->xref_table->xref[i];
            if(entry->compressed) {
                dmprintf(ctx->memory, "*");
                gs_sprintf(Buffer, "%ld", entry->object_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.compressed.compressed_stream_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.compressed.object_index);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);
            }
            else {
                dmprintf(ctx->memory, " ");

                gs_sprintf(Buffer, "%ld", entry->object_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.uncompressed.offset);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.uncompressed.generation_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);
            }
            if (entry->free)
                dmprintf(ctx->memory, "f\n");
            else
                dmprintf(ctx->memory, "n\n");
        }
    }
    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n");

 exit:
    (void)pdfi_loop_detector_cleartomark(ctx);
    if (do_repair)
        return(pdfi_repair_file(ctx));

    if (code < 0)
        return code;

    return 0;
}
