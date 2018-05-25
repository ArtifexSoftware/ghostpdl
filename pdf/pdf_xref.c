/* Copyright (C) 2001-2018 Artifex Software, Inc.
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
        pdf_countdown(ctx->xref_table);
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

static int read_xref_stream_entries(pdf_context *ctx, pdf_stream *s, uint64_t first, uint64_t last, uint64_t *W)
{
    uint i, j = 0;
    uint32_t type = 0;
    uint64_t objnum = 0, gen = 0;
    byte *Buffer;
    int64_t bytes = 0;
    xref_entry *entry;

    if (W[0] > W[1]) {
        if (W[0] > W[2]) {
            j = W[2];
        } else {
            j = W[2];
        }
    } else {
        if (W[1] > W[2]) {
            j = W[1];
        } else {
            j = W[2];
        }
    }

    Buffer = gs_alloc_bytes(ctx->memory, j, "read_xref_stream_entry working buffer");
    for (i=first;i<=last; i++){
        type = objnum = gen = 0;

        bytes = pdf_read_bytes(ctx, Buffer, 1, W[0], s);
        if (bytes < W[0]){
            gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer (error)");
            return_error(gs_error_ioerror);
        }
        for (j=0;j<W[0];j++)
            type = (type << 8) + Buffer[j];
        bytes = pdf_read_bytes(ctx, Buffer, 1, W[1], s);
        if (bytes < W[1]){
            gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry free working buffer (error)");
            return_error(gs_error_ioerror);
        }
        for (j=0;j<W[1];j++)
            objnum = (objnum << 8) + Buffer[j];
        bytes = pdf_read_bytes(ctx, Buffer, 1, W[2], s);
        if (bytes < W[2]){
            gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer (error)");
            return_error(gs_error_ioerror);
        }
        for (j=0;j<W[2];j++)
            gen = (gen << 8) + Buffer[j];

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

/* These two routines are recursive.... */
static int pdf_read_xref_stream_dict(pdf_context *ctx, pdf_stream *s);

static int pdf_process_xref_stream(pdf_context *ctx, pdf_dict *d, pdf_stream *s)
{
    pdf_stream *XRefStrm;
    int code, i;
    pdf_name *n;
    pdf_array *a;
    int64_t size;
    int64_t num;
    int64_t W[3];

    code = pdf_dict_get_type(ctx, d, "Type", PDF_NAME, (pdf_obj **)&n);
    if (code < 0)
        return code;

    if (n->length != 4 || memcmp(n->data, "XRef", 4) != 0) {
        pdf_countdown(n);
        return_error(gs_error_syntaxerror);
    }
    pdf_countdown(n);

    code = pdf_dict_get_int(ctx, d, "Size", &size);
    if (code < 0)
        return code;

    if (size < 0)
        return_error(gs_error_rangecheck);

    /* If this is the first xref stream then allocate the xref table and store the trailer */
    if (ctx->xref_table == NULL) {
        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL) {
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table));
        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, size * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            gs_free_object(ctx->memory, ctx->xref_table, "failed to allocate xref table entries");
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table->xref, 0x00, size * sizeof(xref_entry));
        ctx->xref_table->memory = ctx->memory;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = size;
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif
        pdf_countup(ctx->xref_table);

        ctx->Trailer = d;
        pdf_countup(d);
    } else {
        code = pdf_merge_dicts(ctx->Trailer, d);
        if (code < 0) {
            if (code == gs_error_VMerror || ctx->pdfstoponerror)
                return code;
        }
    }

    pdf_seek(ctx, ctx->main_stream, d->stream_offset, SEEK_SET);
    code = pdf_filter(ctx, d, s, &XRefStrm, false);
    if (code < 0) {
        pdf_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    code = pdf_dict_get_type(ctx, d, "W", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0) {
        pdf_close_file(ctx, XRefStrm);
        pdf_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    if (a->entries != 3) {
        pdf_countdown(a);
        pdf_close_file(ctx, XRefStrm);
        pdf_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return_error(gs_error_rangecheck);
    }
    for (i=0;i<3;i++) {
        code = pdf_array_get_int(ctx, a, (uint64_t)i, (int64_t *)&W[i]);
        if (code < 0) {
            pdf_countdown(a);
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    }
    pdf_countdown(a);

    code = pdf_dict_get_type(ctx, d, "Index", PDF_ARRAY, (pdf_obj **)&a);
    if (code == gs_error_undefined) {
        code = read_xref_stream_entries(ctx, XRefStrm, 0, size - 1, (uint64_t *)W);
        if (code < 0) {
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    } else {
        int64_t start, end;

        if (code < 0) {
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }

        if (a->entries & 1) {
            pdf_countdown(a);
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_rangecheck);
        }

        for (i=0;i < a->entries;i+=2){
            code = pdf_array_get_int(ctx, a, (uint64_t)i, &start);
            if (code < 0) {
                pdf_countdown(a);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }

            code = pdf_array_get_int(ctx, a, (uint64_t)i+1, &end);
            if (code < 0) {
                pdf_countdown(a);
                pdf_countdown(start);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown(ctx->xref_table);
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
                pdf_countdown(a);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }
        }
    }
    pdf_countdown(a);

    pdf_close_file(ctx, XRefStrm);

    code = pdf_dict_get_int(ctx, d, "Prev", &num);
    if (code == gs_error_undefined)
        return 0;

    if (code < 0)
        return code;

    if (num < 0 || num > ctx->main_stream_length)
        return_error(gs_error_rangecheck);

    if (pdf_loop_detector_check_object(ctx, num) == true)
        return_error(gs_error_circular_reference);
    else {
        code = pdf_loop_detector_add_object(ctx, num);
        if (code < 0)
            return code;
    }

    if(ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading /Prev xref\n");

    pdf_seek(ctx, s, num, SEEK_SET);

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0)
        return code;

    code = pdf_read_xref_stream_dict(ctx, s);

    return code;
}

static int pdf_read_xref_stream_dict(pdf_context *ctx, pdf_stream *s)
{
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading PDF 1.5+ xref stream\n");

    if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_INT) {
        /* Its an integer, lets try for index gen obj as a XRef stream */
        code = pdf_read_token(ctx, ctx->main_stream);

        if (code < 0)
            return(pdf_repair_file(ctx));

        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
            /* Second element is not an integer, not a valid xref */
            pdf_pop(ctx, 1);
            return(pdf_repair_file(ctx));
        }

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0) {
            pdf_pop(ctx, 1);
            return code;
        }

        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
            /* Second element is not an integer, not a valid xref */
            pdf_pop(ctx, 2);
            return(pdf_repair_file(ctx));
        } else {
            int obj_num, gen_num;

            pdf_keyword *keyword = (pdf_keyword *)ctx->stack_top[-1];

            if (keyword->key != PDF_OBJ) {
                pdf_pop(ctx, 3);
                return(pdf_repair_file(ctx));
            }
            /* pop the 'obj', generation and object numbers */
            pdf_pop(ctx, 1);
            gen_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
            pdf_pop(ctx, 1);
            obj_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
            pdf_pop(ctx, 1);

            do {
                code = pdf_read_token(ctx, ctx->main_stream);
                if (code < 0)
                    return pdf_repair_file(ctx);

                if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD) {
                    keyword = (pdf_keyword *)ctx->stack_top[-1];
                    if (keyword->key == PDF_STREAM) {
                        pdf_dict *d;

                        /* Remove the 'stream' token from the stack, should leave a dictionary object on the stack */
                        pdf_pop(ctx, 1);
                        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_DICT) {
                            pdf_pop(ctx, 1);
                            return pdf_repair_file(ctx);
                        }
                        d = (pdf_dict *)ctx->stack_top[-1];
                        d->stream_offset = pdf_tell(ctx);

                        d->object_num = obj_num;
                        d->generation_num = gen_num;
                        pdf_countup(d);
                        pdf_pop(ctx, 1);

                        code = pdf_process_xref_stream(ctx, d, ctx->main_stream);
                        if (code < 0) {
                            pdf_countdown(d);
                            return (pdf_repair_file(ctx));
                        }
                        pdf_countdown(d);
                        break;
                    }
                    if (keyword->key == PDF_ENDOBJ) {
                        /* Something went wrong, this is not a stream dictionary */
                        pdf_pop(ctx, 3);
                        return(pdf_repair_file(ctx));
                        break;
                    }
                }
            } while(1);

            /* We should now have pdf_object, endobj on the stack, pop the endobj */
            pdf_pop(ctx, 1);
        }
    } else {
        /* Not an 'xref' and not an integer, so not a valid xref */
        return(pdf_repair_file(ctx));
    }
    return 0;
}

static int read_xref_section(pdf_context *ctx, pdf_stream *s)
{
    int code = 0, i, j;
    pdf_obj *o = NULL;
    uint64_t start = 0, size = 0;
    int64_t bytes = 0;
    char Buffer[21];

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading xref section\n");

    code = pdf_read_token(ctx, ctx->main_stream);

    if (code < 0)
        return code;

    if (ctx->stack_top - ctx->stack_bot < 1)
        return_error(gs_error_stackunderflow);

    o = ctx->stack_top[-1];
    if (o->type == PDF_KEYWORD)
        return 0;

    if (o->type != PDF_INT) {
        /* element is not an integer, not a valid xref */
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }

    start = ((pdf_num *)o)->value.i;

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0) {
        pdf_pop(ctx, 1);
        return code;
    }

    o = ctx->stack_top[-1];
    if (o->type != PDF_INT) {
        /* element is not an integer, not a valid xref */
        pdf_pop(ctx, 2);
        return_error(gs_error_typecheck);
    }
    size = ((pdf_num *)o)->value.i;
    pdf_pop(ctx, 2);

    if (ctx->pdfdebug)
        dmprintf2(ctx->memory, "\n%% Section starts at %d and has %d entries\n", (unsigned int) start, (unsigned int)size);

    if (size > 0) {
        if (ctx->xref_table == NULL) {
            ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
            if (ctx->xref_table == NULL)
                return_error(gs_error_VMerror);
            memset(ctx->xref_table, 0x00, sizeof(xref_table));

            ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, (start + size) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
            if (ctx->xref_table->xref == NULL){
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return_error(gs_error_VMerror);
            }
#if REFCNT_DEBUG
            ctx->xref_table->UID = ctx->UID++;
            dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif

            memset(ctx->xref_table->xref, 0x00, (start + size) * sizeof(xref_entry));
            ctx->xref_table->memory = ctx->memory;
            ctx->xref_table->type = PDF_XREF_TABLE;
            ctx->xref_table->xref_size = start + size;
            pdf_countup(ctx->xref_table);
        } else {
            if (start + size > ctx->xref_table->xref_size) {
                code = resize_xref(ctx, start + size);
                if (code < 0)
                    return code;
            }
        }
    }

    skip_white(ctx, s);
    for (i=0;i< size;i++){
        xref_entry *entry = &ctx->xref_table->xref[i + start];
        unsigned char free;

        bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 20, s);
        if (bytes < 20)
            return_error(gs_error_ioerror);
        j = 19;
        while (Buffer[j] != 0x0D && Buffer[j] != 0x0A) {
            pdf_unread(ctx, s, (byte *)&Buffer[j], 1);
            j--;
        }
        Buffer[j] = 0x00;
        if (entry->object_num != 0)
            continue;

        sscanf(Buffer, "%ld %d %c", &entry->u.uncompressed.offset, &entry->u.uncompressed.generation_num, &free);
        entry->compressed = false;
        entry->object_num = i + start;
        if (free == 'f')
            entry->free = true;
        if(free == 'n')
            entry->free = false;
    }

    return 0;
}

static int read_xref(pdf_context *ctx, pdf_stream *s)
{
    int code = 0;
    pdf_obj **o = NULL;
    pdf_keyword *k;
    pdf_dict *d = NULL;
    uint64_t size = 0;
    int64_t num;

    do {
        o = ctx->stack_top;
        code = read_xref_section(ctx, s);
        if (code < 0)
            return code;
        if (ctx->stack_top - o > 0) {
            k = (pdf_keyword *)ctx->stack_top[-1];
            if(k->type != PDF_KEYWORD || k->key != PDF_TRAILER)
                return_error(gs_error_syntaxerror);
            else {
                pdf_pop(ctx, 1);
                break;
            }
        }
    } while (1);

    code = pdf_read_dict(ctx, ctx->main_stream);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    if (d->type != PDF_DICT) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }

    if (ctx->Trailer == NULL) {
        ctx->Trailer = d;
        pdf_countup(d);
    } else {
        code = pdf_merge_dicts(ctx->Trailer, d);
        if (code < 0) {
            if (code == gs_error_VMerror || ctx->pdfstoponerror)
                return code;
        }
    }

    /* We have the Trailer dictionary. First up check for hybrid files. These have the initial
     * xref starting at 0 and size of 0. In this case the /Size entry in the trailer dictionary
     * must tell us how large the xref is, and we need to allocate our xref table anyway.
     */
    if (ctx->xref_table == NULL && size == 0) {
        int64_t size;

        code = pdf_dict_get_int(ctx, d, "Size", &size);
        if (code < 0) {
            pdf_pop(ctx, 2);
            return code;
        }
        if (size < 0) {
            pdf_pop(ctx, 2);
            return_error(gs_error_rangecheck);
        }

        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL) {
            pdf_pop(ctx, 2);
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table));
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif

        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, size * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            pdf_pop(ctx, 2);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }

        memset(ctx->xref_table->xref, 0x00, size * sizeof(xref_entry));
        ctx->xref_table->memory = ctx->memory;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = size;
        pdf_countup(ctx->xref_table);
    }

    /* Now check if this is a hybrid file. */
    if (ctx->Trailer == d) {
        code = pdf_dict_get_int(ctx, d, "XRefStm", &num);
        if (code < 0 && code != gs_error_undefined) {
            pdf_pop(ctx, 2);
            return code;
        }
        if (code == 0)
            ctx->is_hybrid = true;
    } else
        code = gs_error_undefined;

    if (code == 0 && ctx->prefer_xrefstm) {
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% File is a hybrid, containing xref table and xref stream. Using the stream.\n");

        pdf_pop(ctx, 2);

        if (pdf_loop_detector_check_object(ctx, num) == true)
            return_error(gs_error_circular_reference);
        else {
            code = pdf_loop_detector_add_object(ctx, num);
            if (code < 0)
                return code;
        }
        /* Because of the way the code works when we read a file which is a pure
         * xref stream file, we need to read the first integer of 'x y obj'
         * because the xref stream decoding code expects that to be on the stack.
         */
        pdf_seek(ctx, s, num, SEEK_SET);

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0)
            return code;

        code = pdf_read_xref_stream_dict(ctx, ctx->main_stream);
        if (code < 0)
            return code;
    }

    /* Not a hybrid file, so now check if this is a modified file and has
     * previous xref entries.
     */
    code = pdf_dict_get_int(ctx, d, "Prev", &num);
    if (code < 0) {
        pdf_pop(ctx, 2);
        if (code == gs_error_undefined)
            return 0;
        else
            return code;
    }
    pdf_pop(ctx, 1);

    if (num < 0 || num > ctx->main_stream_length)
        return_error(gs_error_rangecheck);

    if (pdf_loop_detector_check_object(ctx, num) == true)
        return_error(gs_error_circular_reference);
    else {
        code = pdf_loop_detector_add_object(ctx, num);
        if (code < 0)
            return code;
    }

    code = pdf_seek(ctx, s, num, SEEK_SET);
    if (code < 0)
        return code;

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0)
        return(code);

    if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == PDF_XREF) {
        /* Read old-style xref table */
        pdf_pop(ctx, 1);
        return(read_xref(ctx, ctx->main_stream));
    } else {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
}

int pdf_read_xref(pdf_context *ctx)
{
    byte *Buffer = NULL;
    int code = 0;

    Buffer = gs_alloc_bytes(ctx->memory, BUF_SIZE, "PDF interpreter - allocate working buffer for file validation");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    code = pdf_init_loop_detector(ctx);
    if (code < 0)
        return code;

    code = pdf_loop_detector_add_object(ctx, ctx->startxref);
    if (code < 0)
        return code;

    if (ctx->startxref != 0) {
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% Trying to read 'xref' token for xref table, or 'int int obj' for an xref stream\n");

        if (ctx->startxref > ctx->main_stream_length - 5) {
            dmprintf(ctx->memory, "startxref offset is beyond end of file.\n");
            ctx->pdf_errors |= E_PDF_BADSTARTXREF;
            (void)pdf_free_loop_detector(ctx);
            return(pdf_repair_file(ctx));
        }

        /* Read the xref(s) */
        pdf_seek(ctx, ctx->main_stream, ctx->startxref, SEEK_SET);

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0) {
            dmprintf(ctx->memory, "Failed to read any token at the startxref location\n");
            ctx->pdf_errors |= E_PDF_BADSTARTXREF;
            (void)pdf_free_loop_detector(ctx);
            return(pdf_repair_file(ctx));
        }

        if (ctx->stack_top - ctx->stack_bot < 1)
            return_error(gs_error_undefined);

        if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == PDF_XREF) {
            /* Read old-style xref table */
            pdf_pop(ctx, 1);
            code = read_xref(ctx, ctx->main_stream);
            if (code < 0) {
                (void)pdf_free_loop_detector(ctx);
                return(pdf_repair_file(ctx));
            }
        } else {
            code = pdf_read_xref_stream_dict(ctx, ctx->main_stream);
            if (code < 0){
                (void)pdf_free_loop_detector(ctx);
                return(pdf_repair_file(ctx));
            }
        }
    } else {
        /* Attempt to repair PDF file */
        (void)pdf_free_loop_detector(ctx);
        return(pdf_repair_file(ctx));
    }

    if(ctx->pdfdebug && ctx->xref_table) {
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
    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    gs_free_object(ctx->memory, Buffer, "PDF interpreter - allocate working buffer for file validation");

    code = pdf_free_loop_detector(ctx);
    if (code < 0)
        return code;

    return 0;
}
