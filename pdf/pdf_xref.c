/* Copyright (C) 2018-2024 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
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

static int read_xref_stream_entries(pdf_context *ctx, pdf_c_stream *s, int64_t first, int64_t last, int64_t *W)
{
    uint i, j;
    uint64_t field_width = 0;
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
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

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
        if (entry->object_num != 0 && !entry->free)
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
static int pdfi_check_xref_stream(pdf_context *ctx);
/* These two routines are recursive.... */
static int pdfi_read_xref_stream_dict(pdf_context *ctx, pdf_c_stream *s, int obj_num);

static int pdfi_process_xref_stream(pdf_context *ctx, pdf_stream *stream_obj, pdf_c_stream *s)
{
    pdf_c_stream *XRefStrm;
    int code, i;
    pdf_dict *sdict = NULL;
    pdf_name *n;
    pdf_array *a;
    int64_t size;
    int64_t num;
    int64_t W[3] = {0, 0, 0};
    int objnum;
    bool known = false;

    if (pdfi_type_of(stream_obj) != PDF_STREAM)
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
    if (size < 1)
        return 0;

    if (size < 0 || size > floor((double)ARCH_MAX_SIZE_T / (double)sizeof(xref_entry)))
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
        ctx->xref_table->UID = ctx->ref_UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif
        pdfi_countup(ctx->xref_table);

        pdfi_countdown(ctx->Trailer);

        ctx->Trailer = sdict;
        pdfi_countup(sdict);
    } else {
        if (size > ctx->xref_table->xref_size)
            return_error(gs_error_rangecheck);

        code = pdfi_merge_dicts(ctx, ctx->Trailer, sdict);
        if (code < 0 && (code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_BADXREF, "pdfi_process_xref_stream", NULL)) < 0) {
            goto exit;
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
        if (code < 0 || W[i] < 0) {
            pdfi_countdown(a);
            pdfi_close_file(ctx, XRefStrm);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            if (W[i] < 0)
                code = gs_note_error(gs_error_rangecheck);
            return code;
        }
    }
    pdfi_countdown(a);

    /* W[0] is either:
     * 0 (no type field) or a single byte with the type.
     * W[1] is either:
     * The object number of the next free object, the byte offset of this object in the file or the object5 number of the object stream where this object is stored.
     * W[2] is either:
     * The generation number to use if this object is used again, the generation number of the object or the index of this object within the object stream.
     *
     * Object and generation numbers are limited to unsigned 64-bit values, as are bytes offsets in the file, indexes of objects within the stream likewise (actually
     * most of these are generally 32-bit max). So we can limit the field widths to 8 bytes, enough to hold a 64-bit number.
     * Even if a later version of the spec makes these larger (which seems unlikely!) we still cna't cope with integers > 64-bits.
     */
    if (W[0] > 1 || W[1] > 8 || W[2] > 8) {
        pdfi_close_file(ctx, XRefStrm);
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    code = pdfi_dict_get_type(ctx, sdict, "Index", PDF_ARRAY, (pdf_obj **)&a);
    if (code == gs_error_undefined) {
        code = read_xref_stream_entries(ctx, XRefStrm, 0, size - 1, W);
        if (code < 0) {
            pdfi_close_file(ctx, XRefStrm);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    } else {
        int64_t start, size;

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
            if (code < 0 || start < 0) {
                pdfi_countdown(a);
                pdfi_close_file(ctx, XRefStrm);
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }

            code = pdfi_array_get_int(ctx, a, (uint64_t)i+1, &size);
            if (code < 0) {
                pdfi_countdown(a);
                pdfi_close_file(ctx, XRefStrm);
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }

            if (size < 1)
                continue;

            if (start + size >= ctx->xref_table->xref_size) {
                code = resize_xref(ctx, start + size);
                if (code < 0) {
                    pdfi_countdown(a);
                    pdfi_close_file(ctx, XRefStrm);
                    pdfi_countdown(ctx->xref_table);
                    ctx->xref_table = NULL;
                    return code;
                }
            }

            code = read_xref_stream_entries(ctx, XRefStrm, start, start + size - 1, W);
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

    code = pdfi_read_bare_int(ctx, ctx->main_stream, &objnum);
    if (code == 1) {
        if (pdfi_check_xref_stream(ctx))
            return pdfi_read_xref_stream_dict(ctx, s, objnum);
    }

    code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
    if (code < 0)
        return code;
    if (code == TOKEN_XREF) {
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_PREV_NOT_XREF_STREAM, "pdfi_process_xref_stream", NULL)) < 0) {
            goto exit;
        }
        /* Read old-style xref table */
        return(read_xref(ctx, ctx->main_stream));
    }
exit:
    return_error(gs_error_syntaxerror);
}

static int pdfi_check_xref_stream(pdf_context *ctx)
{
    gs_offset_t offset;
    int gen_num, code = 0;

    offset = pdfi_unread_tell(ctx);

    code = pdfi_read_bare_int(ctx, ctx->main_stream, &gen_num);
    if (code <= 0) {
        code = 0;
        goto exit;
    }

    /* Try to read 'obj' */
    code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
    if (code <= 0) {
        code = 0;
        goto exit;
    }

    /* Third element must be obj, or it's not a valid xref */
    if (code != TOKEN_OBJ)
        code = 0;
    else
        code = 1;

exit:
    pdfi_seek(ctx, ctx->main_stream, offset, SEEK_SET);
    return code;
}

static int pdfi_read_xref_stream_dict(pdf_context *ctx, pdf_c_stream *s, int obj_num)
{
    int code;
    int gen_num;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading PDF 1.5+ xref stream\n");

    /* We have the obj_num. Lets try for obj_num gen obj as a XRef stream */
    code = pdfi_read_bare_int(ctx, ctx->main_stream, &gen_num);
    if (code <= 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_BADXREFSTREAM, "pdfi_read_xref_stream_dict", "")) < 0) {
            return code;
        }
        return(pdfi_repair_file(ctx));
    }

    /* Try to read 'obj' */
    code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_syntaxerror);

    /* Third element must be obj, or it's not a valid xref */
    if (code != TOKEN_OBJ) {
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_rangecheck), NULL, E_PDF_BAD_XREFSTMOFFSET, "pdfi_read_xref_stream_dict", "")) < 0) {
            return code;
        }
        return(pdfi_repair_file(ctx));
    }

    do {
        code = pdfi_read_token(ctx, ctx->main_stream, obj_num, gen_num);
        if (code <= 0) {
            if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_BADXREFSTREAM, "pdfi_read_xref_stream_dict", NULL)) < 0) {
                return code;
            }
            return pdfi_repair_file(ctx);
        }

        if (pdfi_count_stack(ctx) >= 2 && pdfi_type_of(ctx->stack_top[-1]) == PDF_FAST_KEYWORD) {
            uintptr_t keyword = (uintptr_t)ctx->stack_top[-1];
            if (keyword == TOKEN_STREAM) {
                pdf_dict *dict;
                pdf_stream *sdict = NULL;
                int64_t Length;

                /* Remove the 'stream' token from the stack, should leave a dictionary object on the stack */
                pdfi_pop(ctx, 1);
                if (pdfi_type_of(ctx->stack_top[-1]) != PDF_DICT) {
                    if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_BADXREFSTREAM, "pdfi_read_xref_stream_dict", NULL)) < 0) {
                        return code;
                    }
                    return pdfi_repair_file(ctx);
                }
                dict = (pdf_dict *)ctx->stack_top[-1];

                /* Convert the dict into a stream (sdict comes back with at least one ref) */
                code = pdfi_obj_dict_to_stream(ctx, dict, &sdict, true);
                /* Pop off the dict */
                pdfi_pop(ctx, 1);
                if (code < 0) {
                    if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_BADXREFSTREAM, "pdfi_read_xref_stream_dict", NULL)) < 0) {
                        return code;
                    }
                    /* TODO: should I return code instead of trying to repair?
                     * Normally the above routine should not fail so something is
                     * probably seriously fubar.
                     */
                    return pdfi_repair_file(ctx);
                }
                dict = NULL;

                /* Init the stuff for the stream */
                sdict->stream_offset = pdfi_unread_tell(ctx);
                sdict->object_num = obj_num;
                sdict->generation_num = gen_num;

                code = pdfi_dict_get_int(ctx, sdict->stream_dict, "Length", &Length);
                if (code < 0) {
                    /* TODO: Not positive this will actually have a length -- just use 0 */
                    (void)pdfi_set_error_var(ctx, 0, NULL, E_PDF_BADSTREAM, "pdfi_read_xref_stream_dict", "Xref Stream object %u missing mandatory keyword /Length\n", obj_num);
                    code = 0;
                    Length = 0;
                }
                sdict->Length = Length;
                sdict->length_valid = true;

                code = pdfi_process_xref_stream(ctx, sdict, ctx->main_stream);
                pdfi_countdown(sdict);
                if (code < 0) {
                    pdfi_set_error(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_PREV_NOT_XREF_STREAM, "pdfi_read_xref_stream_dict", NULL);
                    return code;
                }
                break;
            } else if (keyword == TOKEN_ENDOBJ) {
                /* Something went wrong, this is not a stream dictionary */
                if ((code = pdfi_set_error_var(ctx, 0, NULL, E_PDF_BADSTREAM, "pdfi_read_xref_stream_dict", "Xref Stream object %u missing mandatory keyword /Length\n", obj_num)) < 0) {
                    return code;
                }
                return(pdfi_repair_file(ctx));
            }
        }
    } while(1);
    return 0;
}

static int skip_to_digit(pdf_context *ctx, pdf_c_stream *s, unsigned int limit)
{
    int c, read = 0;

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            return_error(gs_error_ioerror);
        if (c >= '0' && c <= '9') {
            pdfi_unread_byte(ctx, s, (byte)c);
            return read;
        }
        read++;
    } while (read < limit);

    return read;
}

static int read_digits(pdf_context *ctx, pdf_c_stream *s, byte *Buffer, int limit)
{
    int c, read = 0;

    /* Since the "limit" is a value calculated by the caller,
       it's easier to check it in one place (here) than before
       every call.
     */
    if (limit <= 0)
        return_error(gs_error_syntaxerror);

    /* We assume that Buffer always has limit+1 bytes available, so we can
     * safely terminate it. */

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            return_error(gs_error_ioerror);
        if (c < '0' || c > '9') {
            pdfi_unread_byte(ctx, s, c);
            break;
        }
        *Buffer++ = (byte)c;
        read++;
    } while (read < limit);
    *Buffer = 0;

    return read;
}


static int read_xref_entry_slow(pdf_context *ctx, pdf_c_stream *s, gs_offset_t *offset, uint32_t *generation_num, unsigned char *free)
{
    byte Buffer[20];
    int c, code, read = 0;

    /* First off, find a number. If we don't find one, and read 20 bytes, throw an error */
    code = skip_to_digit(ctx, s, 20);
    if (code < 0)
        return code;
    read += code;

    /* Now read a number */
    code = read_digits(ctx, s, (byte *)&Buffer,  (read > 10 ? 20 - read : 10));
    if (code < 0)
        return code;
    read += code;

    *offset = atol((const char *)Buffer);

    /* find next number */
    code = skip_to_digit(ctx, s, 20 - read);
    if (code < 0)
        return code;
    read += code;

    /* and read it */
    code = read_digits(ctx, s, (byte *)&Buffer, (read > 15 ? 20 - read : 5));
    if (code < 0)
        return code;
    read += code;

    *generation_num = atol((const char *)Buffer);

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            return_error(gs_error_ioerror);
        read ++;
        if (c == 0x09 || c == 0x20)
            continue;
        if (c == 'n' || c == 'f') {
            *free = (unsigned char)c;
            break;
        } else {
            return_error(gs_error_syntaxerror);
        }
    } while (read < 20);
    if (read >= 20)
        return_error(gs_error_syntaxerror);

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            return_error(gs_error_syntaxerror);
        read++;
        if (c == 0x20 || c == 0x09 || c == 0x0d || c == 0x0a)
            continue;
    } while (read < 20);
    return 0;
}

static int write_offset(byte *B, gs_offset_t o, unsigned int g, unsigned char free)
{
    byte b[20], *ptr = B;
    int index = 0;

    gs_snprintf((char *)b, sizeof(b), "%"PRIdOFFSET"", o);
    if (strlen((const char *)b) > 10)
        return_error(gs_error_rangecheck);
    for(index=0;index < 10 - strlen((const char *)b); index++) {
        *ptr++ = 0x30;
    }
    memcpy(ptr, b, strlen((const char *)b));
    ptr += strlen((const char *)b);
    *ptr++ = 0x20;

    gs_snprintf((char *)b, sizeof(b), "%d", g);
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
    int start = 0;
    int size = 0;
    int64_t bytes = 0;
    char Buffer[21];

    *section_start = *section_size = 0;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading xref section\n");

    code = pdfi_read_bare_int(ctx, ctx->main_stream, &start);
    if (code < 0) {
        /* Not an int, might be a keyword */
        code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
        if (code < 0)
            return code;

        if (code != TOKEN_TRAILER) {
            /* element is not an integer, and not a keyword - not a valid xref */
            return_error(gs_error_typecheck);
        }
        return 1;
    }

    if (start < 0)
        return_error(gs_error_rangecheck);

    *section_start = start;

    code = pdfi_read_bare_int(ctx, ctx->main_stream, &size);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_syntaxerror);

    /* Zero sized xref sections are valid; see the file attached to
     * bug 704947 for an example. */
    if (size < 0)
        return_error(gs_error_rangecheck);

    *section_size = size;

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
            ctx->xref_table->UID = ctx->ref_UID++;
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
        if ((Buffer[19] != 0x0a && Buffer[19] != 0x0d) || (Buffer[18] != 0x0d && Buffer[18] != 0x0a && Buffer[18] != 0x20))
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_XREF_ENTRY_SIZE, "read_xref_section", NULL);
        while (Buffer[j] != 0x0D && Buffer[j] != 0x0A) {
            pdfi_unread_byte(ctx, s, (byte)Buffer[j]);
            if (--j < 0) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_XREF_ENTRY_NO_EOL, "read_xref_section", NULL);
                dmprintf(ctx->memory, "Invalid xref entry, line terminator missing.\n");
                code = read_xref_entry_slow(ctx, s, &off, &gen, &free);
                if (code < 0)
                    return code;
                code = write_offset((byte *)Buffer, off, gen, free);
                if (code < 0)
                    return code;
                j = 19;
                break;
            }
        }
        Buffer[j] = 0x00;
        if (entry->object_num != 0)
            continue;

        if (sscanf(Buffer, "%"PRIdOFFSET" %d %c", &entry->u.uncompressed.offset, &entry->u.uncompressed.generation_num, &free) != 3) {
            pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_XREF_ENTRY_FORMAT, "read_xref_section", NULL);
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
        if (entry->object_num == 0) {
            if (!entry->free) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_XREF_OBJECT0_NOT_FREE, "read_xref_section", NULL);
            }
        }
    }

    return 0;
}

static int read_xref(pdf_context *ctx, pdf_c_stream *s)
{
    int code = 0;
    pdf_dict *d = NULL;
    uint64_t max_obj = 0;
    int64_t num, XRefStm = 0;
    int obj_num;
    bool known = false;

    if (ctx->repaired)
        return 0;

    do {
        uint64_t section_start, section_size;

        code = read_xref_section(ctx, s, &section_start, &section_size);
        if (code < 0)
            return code;

        if (section_size > 0 && section_start + section_size - 1 > max_obj)
            max_obj = section_start + section_size - 1;

        /* code == 1 => read_xref_section ended with a trailer. */
    } while (code != 1);

    code = pdfi_read_dict(ctx, ctx->main_stream, 0, 0);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    if (pdfi_type_of(d) != PDF_DICT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    pdfi_countup(d);
    pdfi_pop(ctx, 1);

    /* We don't want to pollute the Trailer dictionary with any XRefStm key/value pairs
     * which will happen when we do pdfi_merge_dicts(). So we get any XRefStm here and
     * if there was one, remove it from the dictionary before we merge with the
     * primary trailer.
     */
    code = pdfi_dict_get_int(ctx, d, "XRefStm", &XRefStm);
    if (code < 0 && code != gs_error_undefined)
        goto error;

    if (code == 0) {
        code = pdfi_dict_delete(ctx, d, "XRefStm");
        if (code < 0)
            goto error;
    }

    if (ctx->Trailer == NULL) {
        ctx->Trailer = d;
        pdfi_countup(d);
    } else {
        code = pdfi_merge_dicts(ctx, ctx->Trailer, d);
        if (code < 0) {
            if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_BADXREF, "read_xref", "")) < 0) {
                return code;
            }
        }
    }

    /* Check if the highest subsection + size exceeds the /Size in the
     * trailer dictionary and set a warning flag if it does
     */
    code = pdfi_dict_get_int(ctx, d, "Size", &num);
    if (code < 0)
        goto error;

    if (max_obj >= num)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_XREF_SIZE, "read_xref", NULL);

    /* Check if this is a modified file and has any
     * previous xref entries.
     */
    code = pdfi_dict_known(ctx, d, "Prev", &known);
    if (known) {
        code = pdfi_dict_get_int(ctx, d, "Prev", &num);
        if (code < 0)
            goto error;

        if (num < 0 || num > ctx->main_stream_length) {
            code = gs_note_error(gs_error_rangecheck);
            goto error;
        }

        if (pdfi_loop_detector_check_object(ctx, num) == true) {
            code = gs_note_error(gs_error_circular_reference);
            goto error;
        }
        else {
            code = pdfi_loop_detector_add_object(ctx, num);
            if (code < 0)
                goto error;
        }

        code = pdfi_seek(ctx, s, num, SEEK_SET);
        if (code < 0)
            goto error;

        if (!ctx->repaired) {
            code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
            if (code < 0)
                goto error;

            if (code == 0) {
                code = gs_note_error(gs_error_syntaxerror);
                goto error;
            }
        } else {
            code = 0;
            goto error;
        }

        if ((intptr_t)(ctx->stack_top[-1]) == (intptr_t)TOKEN_XREF) {
            /* Read old-style xref table */
            pdfi_pop(ctx, 1);
            code = read_xref(ctx, ctx->main_stream);
            if (code < 0)
                goto error;
        } else {
            pdfi_pop(ctx, 1);
            code = gs_note_error(gs_error_typecheck);
            goto error;
        }
    }

    /* Now check if this is a hybrid file. */
    if (XRefStm != 0) {
        ctx->is_hybrid = true;

        if (ctx->args.pdfdebug)
            dmprintf(ctx->memory, "%% File is a hybrid, containing xref table and xref stream. Reading the stream.\n");


        if (pdfi_loop_detector_check_object(ctx, XRefStm) == true) {
            code = gs_note_error(gs_error_circular_reference);
            goto error;
        }
        else {
            code = pdfi_loop_detector_add_object(ctx, XRefStm);
            if (code < 0)
                goto error;
        }

        code = pdfi_loop_detector_mark(ctx);
        if (code < 0)
            goto error;

        /* Because of the way the code works when we read a file which is a pure
         * xref stream file, we need to read the first integer of 'x y obj'
         * because the xref stream decoding code expects that to be on the stack.
         */
        pdfi_seek(ctx, s, XRefStm, SEEK_SET);

        code = pdfi_read_bare_int(ctx, ctx->main_stream, &obj_num);
        if (code < 0) {
            pdfi_set_error(ctx, 0, NULL, E_PDF_BADXREFSTREAM, "read_xref", "");
            pdfi_loop_detector_cleartomark(ctx);
            goto error;
        }

        code = pdfi_read_xref_stream_dict(ctx, ctx->main_stream, obj_num);
        /* We could just fall through to the exit here, but choose not to in order to avoid possible mistakes in future */
        if (code < 0) {
            pdfi_loop_detector_cleartomark(ctx);
            goto error;
        }

        pdfi_loop_detector_cleartomark(ctx);
    } else
        code = 0;

error:
    pdfi_countdown(d);
    return code;
}

int pdfi_read_xref(pdf_context *ctx)
{
    int code = 0;
    int obj_num;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    if (ctx->startxref == 0)
        goto repair;

    code = pdfi_loop_detector_add_object(ctx, ctx->startxref);
    if (code < 0)
        goto exit;

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Trying to read 'xref' token for xref table, or 'int int obj' for an xref stream\n");

    if (ctx->startxref > ctx->main_stream_length - 5) {
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_rangecheck), NULL, E_PDF_BADSTARTXREF, "pdfi_read_xref", (char *)"startxref offset is beyond end of file")) < 0)
            goto exit;

        goto repair;
    }
    if (ctx->startxref < 0) {
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_rangecheck), NULL, E_PDF_BADSTARTXREF, "pdfi_read_xref", (char *)"startxref offset is before start of file")) < 0)
            goto exit;

        goto repair;
    }

    /* Read the xref(s) */
    pdfi_seek(ctx, ctx->main_stream, ctx->startxref, SEEK_SET);

    /* If it starts with an int, it's an xref stream dict */
    code = pdfi_read_bare_int(ctx, ctx->main_stream, &obj_num);
    if (code == 1) {
        if (pdfi_check_xref_stream(ctx)) {
            code = pdfi_read_xref_stream_dict(ctx, ctx->main_stream, obj_num);
            if (code < 0)
                goto repair;
        } else
            goto repair;
    } else {
        /* If not, it had better start 'xref', and be an old-style xref table */
        code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
        if (code != TOKEN_XREF) {
            if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_BADSTARTXREF, "pdfi_read_xref", (char *)"Failed to read any token at the startxref location")) < 0)
                goto exit;

            goto repair;
        }

        code = read_xref(ctx, ctx->main_stream);
        if (code < 0)
            goto repair;
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
                gs_snprintf(Buffer, sizeof(Buffer), "%"PRId64"", entry->object_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_snprintf(Buffer, sizeof(Buffer), "%ld", entry->u.compressed.compressed_stream_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_snprintf(Buffer, sizeof(Buffer), "%ld", entry->u.compressed.object_index);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);
            }
            else {
                dmprintf(ctx->memory, " ");

                gs_snprintf(Buffer, sizeof(Buffer), "%ld", entry->object_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_snprintf(Buffer, sizeof(Buffer), "%"PRIdOFFSET"", entry->u.uncompressed.offset);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_snprintf(Buffer, sizeof(Buffer), "%ld", entry->u.uncompressed.generation_num);
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

    if (code < 0)
        return code;

    return 0;

repair:
    (void)pdfi_loop_detector_cleartomark(ctx);
    if (!ctx->repaired && !ctx->args.pdfstoponerror)
        return(pdfi_repair_file(ctx));
    return 0;
}
