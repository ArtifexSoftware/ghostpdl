/* Copyright (C) 2020-2024 Artifex Software, Inc.
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

/* Routines to attempt repair of PDF files in the event of an error */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "strmio.h"
#include "stream.h"
#include "pdf_deref.h"
#include "pdf_dict.h"
#include "pdf_file.h"
#include "pdf_misc.h"
#include "pdf_repair.h"

static int pdfi_repair_add_object(pdf_context *ctx, int64_t obj, int64_t gen, gs_offset_t offset)
{
    /* Although we can handle object numbers larger than this, on some systems (32-bit Windows)
     * memset is limited to a (signed!) integer for the size of memory to clear. We could deal
     * with this by clearing the memory in blocks, but really, this is almost certainly a
     * corrupted file or something.
     */
    if (obj >= 0x7ffffff / sizeof(xref_entry) || obj < 1 || gen < 0 || offset < 0)
        return_error(gs_error_rangecheck);

    if (ctx->xref_table == NULL) {
        ctx->xref_table = (xref_table_t *)gs_alloc_bytes(ctx->memory, sizeof(xref_table_t), "repair xref table");
        if (ctx->xref_table == NULL) {
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table_t));
        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, (obj + 1) * sizeof(xref_entry), "repair xref table");
        if (ctx->xref_table->xref == NULL){
            gs_free_object(ctx->memory, ctx->xref_table, "failed to allocate xref table entries for repair");
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table->xref, 0x00, (obj + 1) * sizeof(xref_entry));
        ctx->xref_table->ctx = ctx;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = obj + 1;
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->ref_UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif
        pdfi_countup(ctx->xref_table);
    } else {
        if (ctx->xref_table->xref_size < (obj + 1)) {
            xref_entry *new_xrefs;

            new_xrefs = (xref_entry *)gs_alloc_bytes(ctx->memory, (obj + 1) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
            if (new_xrefs == NULL){
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return_error(gs_error_VMerror);
            }
            memset(new_xrefs, 0x00, (obj + 1) * sizeof(xref_entry));
            memcpy(new_xrefs, ctx->xref_table->xref, ctx->xref_table->xref_size * sizeof(xref_entry));
            gs_free_object(ctx->memory, ctx->xref_table->xref, "reallocated xref entries");
            ctx->xref_table->xref = new_xrefs;
            ctx->xref_table->xref_size = obj + 1;
        }
    }
    ctx->xref_table->xref[obj].compressed = false;
    ctx->xref_table->xref[obj].free = false;
    ctx->xref_table->xref[obj].object_num = obj;
    ctx->xref_table->xref[obj].u.uncompressed.generation_num = gen;
    ctx->xref_table->xref[obj].u.uncompressed.offset = offset;
    return 0;
}

int pdfi_repair_file(pdf_context *ctx)
{
    int code = 0;
    gs_offset_t offset, saved_offset;
    int64_t object_num = 0, generation_num = 0;
    int i;
    gs_offset_t outer_saved_offset[3];

    if (ctx->repaired) {
        pdfi_set_error(ctx, 0, NULL, E_PDF_UNREPAIRABLE, "pdfi_repair_file", (char *)"%% Trying to repair file for second time -- unrepairable");
        return_error(gs_error_undefined);
    }

    saved_offset = pdfi_unread_tell(ctx);

    ctx->repaired = true;
    if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_ioerror), NULL, E_PDF_REPAIRED, "pdfi_repair_file", NULL)) < 0)
        return code;

    ctx->repairing = true;

    pdfi_clearstack(ctx);

    if(ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Error encountered in opening PDF file, attempting repair\n");

    /* First try to locate a %PDF header. If we can't find one, abort this, the file is too broken
     * and may not even be a PDF file.
     */
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_SET);
    {
        static const char test[] = "%PDF";
        int index = 0;

        do {
            int c = pdfi_read_byte(ctx, ctx->main_stream);
            if (c < 0)
                goto exit;

            if (c == test[index])
                index++;
            else
                index = 0;
        } while (index < 4);
        if (index != 4) {
            code = gs_note_error(gs_error_undefined);
            goto exit;
        }
        pdfi_unread(ctx, ctx->main_stream, (byte *)test, 4);
        pdfi_skip_comment(ctx, ctx->main_stream);
    }
    if (ctx->main_stream->eof == true) {
        code = gs_note_error(gs_error_ioerror);
        goto exit;
    }

    /* First pass, identify all the objects of the form x y obj */

    do {
        code = pdfi_skip_white(ctx, ctx->main_stream);
        if (code < 0) {
            if (code != gs_error_VMerror && code != gs_error_ioerror) {
                pdfi_clearstack(ctx);
                continue;
            } else
                goto exit;
        }
        offset = pdfi_unread_tell(ctx);
        outer_saved_offset[0] = outer_saved_offset[1] = outer_saved_offset[2] = 0;
        do {
            outer_saved_offset[0] = outer_saved_offset[1];
            outer_saved_offset[1] = outer_saved_offset[2];
            outer_saved_offset[2] = pdfi_unread_tell(ctx);

            object_num = 0;

            code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
            if (code < 0) {
                if (code != gs_error_VMerror && code != gs_error_ioerror) {
                    pdfi_clearstack(ctx);
                    continue;
                } else
                    goto exit;
            }
            if (pdfi_count_stack(ctx) > 0) {
                if (pdfi_type_of(ctx->stack_top[-1]) == PDF_FAST_KEYWORD) {
                    pdf_obj *k = ctx->stack_top[-1];
                    pdf_num *n;

                    if (k == PDF_TOKEN_AS_OBJ(TOKEN_OBJ)) {
                        gs_offset_t saved_offset[3];

                        offset = outer_saved_offset[0];

                        saved_offset[0] = saved_offset[1] = saved_offset[2] = 0;

                        if (pdfi_count_stack(ctx) < 3 || pdfi_type_of(ctx->stack_top[-3]) != PDF_INT || pdfi_type_of(ctx->stack_top[-2]) != PDF_INT) {
                            pdfi_clearstack(ctx);
                            continue;
                        }
                        n = (pdf_num *)ctx->stack_top[-3];
                        object_num = n->value.i;
                        n = (pdf_num *)ctx->stack_top[-2];
                        generation_num = n->value.i;
                        pdfi_clearstack(ctx);

                        do {
                            /* move all the saved offsets up by one */
                            saved_offset[0] = saved_offset[1];
                            saved_offset[1] = saved_offset[2];
                            saved_offset[2] = pdfi_unread_tell(ctx);

                            code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
                            if (code < 0) {
                                if (code != gs_error_VMerror && code != gs_error_ioerror)
                                    continue;
                                goto exit;
                            }
                            if (code == 0 && ctx->main_stream->eof)
                                break;

                            if (pdfi_type_of(ctx->stack_top[-1]) == PDF_FAST_KEYWORD) {
                                pdf_obj *k = ctx->stack_top[-1];

                                if (k == PDF_TOKEN_AS_OBJ(TOKEN_OBJ)) {
                                    /* Found obj while looking for endobj, store the existing 'obj'
                                     * and start afresh.
                                     */
                                    code = pdfi_repair_add_object(ctx, object_num, generation_num, offset);
                                    if (pdfi_count_stack(ctx) < 3 || pdfi_type_of(ctx->stack_top[-3]) != PDF_INT || pdfi_type_of(ctx->stack_top[-2]) != PDF_INT) {
                                        pdfi_clearstack(ctx);
                                        break;
                                    }
                                    n = (pdf_num *)ctx->stack_top[-3];
                                    object_num = n->value.i;
                                    n = (pdf_num *)ctx->stack_top[-2];
                                    generation_num = n->value.i;
                                    pdfi_clearstack(ctx);
                                    offset = saved_offset[0];
                                    continue;
                                }

                                if (k == PDF_TOKEN_AS_OBJ(TOKEN_ENDOBJ)) {
                                    code = pdfi_repair_add_object(ctx, object_num, generation_num, offset);
                                    if (code < 0)
                                        goto exit;
                                    pdfi_clearstack(ctx);
                                    break;
                                } else {
                                    if (k == PDF_TOKEN_AS_OBJ(TOKEN_STREAM)) {
                                        static const char test[] = "endstream";
                                        int index = 0;

                                        do {
                                            int c = pdfi_read_byte(ctx, ctx->main_stream);
                                            if (c == EOFC)
                                                break;
                                            if (c < 0)
                                                goto exit;
                                            if (c == test[index])
                                                index++;
                                            else if (c == test[0]) /* Pesky 'e' appears twice */
                                                index = 1;
                                            else
                                                index = 0;
                                        } while (index < 9);
                                        do {
                                            code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
                                            if (code == gs_error_VMerror || code == gs_error_ioerror)
                                                goto exit;
                                            if (code < 0) {
                                                /* Something went wrong and we couldn't read a token, consume one byte and retry */
                                                (void)pdfi_read_byte(ctx, ctx->main_stream);
                                            } else {
                                                if (code == TOKEN_ENDOBJ || code == TOKEN_INVALID_KEY) {
                                                    code = pdfi_repair_add_object(ctx, object_num, generation_num, offset);
                                                    if (code == gs_error_VMerror || code == gs_error_ioerror)
                                                        goto exit;
                                                    break;
                                                }
                                            }
                                        } while(ctx->main_stream->eof == false);
                                        pdfi_clearstack(ctx);
                                        break;
                                    } else {
                                        pdfi_clearstack(ctx);
                                    }
                                }
                            }
                        } while(1);
                        break;
                    } else {
                        if (k == PDF_TOKEN_AS_OBJ(TOKEN_ENDOBJ)) {
                            pdfi_clearstack(ctx);
                        } else
                            if (k == PDF_TOKEN_AS_OBJ(TOKEN_STARTXREF)) {
                                code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
                                if (code < 0 && code != gs_error_VMerror && code != gs_error_ioerror)
                                    continue;
                                if (code < 0)
                                    goto exit;
                                pdfi_clearstack(ctx);
                            } else {
                                if (k == PDF_TOKEN_AS_OBJ(TOKEN_TRAILER)) {
                                    code = pdfi_read_bare_object(ctx, ctx->main_stream, 0, 0, 0);
                                    if (code == 0 && pdfi_count_stack(ctx) > 0 && pdfi_type_of(ctx->stack_top[-1]) == PDF_DICT) {
                                        if (ctx->Trailer) {
                                            pdf_dict *d = (pdf_dict *)ctx->stack_top[-1];
                                            bool known = false;

                                            code = pdfi_dict_known(ctx, d, "Root", &known);
                                            if (code == 0 && known) {
                                                pdfi_countdown(ctx->Trailer);
                                                ctx->Trailer = (pdf_dict *)ctx->stack_top[-1];
                                                pdfi_countup(ctx->Trailer);
                                            }
                                        } else {
                                            ctx->Trailer = (pdf_dict *)ctx->stack_top[-1];
                                            pdfi_countup(ctx->Trailer);
                                        }
                                    }
                                }
                                pdfi_clearstack(ctx);
                            }
                    }
                    code = pdfi_skip_white(ctx, ctx->main_stream);
                    if (code < 0) {
                        if (code != gs_error_VMerror && code != gs_error_ioerror) {
                            pdfi_clearstack(ctx);
                            continue;
                        } else
                            goto exit;
                    }
                }
                if (pdfi_count_stack(ctx) > 0 && pdfi_type_of(ctx->stack_top[-1]) != PDF_INT)
                    pdfi_clearstack(ctx);
            }
        } while (ctx->main_stream->eof == false);
    } while(ctx->main_stream->eof == false);

    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_SET);
    ctx->main_stream->eof = false;

    /* Second pass, examine every object we have located to see if its an ObjStm */
    if (ctx->xref_table == NULL || ctx->xref_table->xref_size < 1) {
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

    for (i=1;i < ctx->xref_table->xref_size;i++) {
        if (ctx->xref_table->xref[i].object_num != 0) {
            /* At this stage, all the objects we've found must be uncompressed */
            if (ctx->xref_table->xref[i].u.uncompressed.offset > ctx->main_stream_length) {
                /* This can only happen if we had read an xref table before we tried to repair
                 * the file, and the table has entries we didn't find in the file. So
                 * mark the entry as free, and offset of 0, and just carry on.
                 */
                ctx->xref_table->xref[i].free = 1;
                ctx->xref_table->xref[i].u.uncompressed.offset = 0;
                continue;
            }

            pdfi_seek(ctx, ctx->main_stream, ctx->xref_table->xref[i].u.uncompressed.offset, SEEK_SET);
            do {
                code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
                if (ctx->main_stream->eof == true || (code < 0 && code != gs_error_ioerror && code != gs_error_VMerror)) {
                    /* object offset is beyond EOF or object is broken (possibly due to multiple xref
                     * errors) ignore the error and carry on, if the object gets used then we will
                     * error out at that point.
                     */
                    code = 0;
                    break;
                }
                if (code < 0)
                    goto exit;
                if (pdfi_type_of(ctx->stack_top[-1]) == PDF_FAST_KEYWORD) {
                    pdf_obj *k = ctx->stack_top[-1];

                    if (k == PDF_TOKEN_AS_OBJ(TOKEN_OBJ)) {
                        continue;
                    }
                    if (k == PDF_TOKEN_AS_OBJ(TOKEN_ENDOBJ)) {
                        if (pdfi_count_stack(ctx) > 1) {
                            if (pdfi_type_of(ctx->stack_top[-2]) == PDF_DICT) {
                                pdf_dict *d = (pdf_dict *)ctx->stack_top[-2];
                                pdf_obj *o = NULL;

                                code = pdfi_dict_knownget_type(ctx, d, "Type", PDF_NAME, &o);
                                if (code < 0) {
                                    pdfi_clearstack(ctx);
                                    continue;
                                }
                                if (code > 0) {
                                    pdf_name *n = (pdf_name *)o;

                                    if (pdfi_name_is(n, "Catalog")) {
                                        pdfi_countdown(ctx->Root); /* In case it was already set */
                                        ctx->Root = (pdf_dict *)ctx->stack_top[-2];
                                        pdfi_countup(ctx->Root);
                                    }
                                }
                                pdfi_countdown(o);
                            }
                        }
                        pdfi_clearstack(ctx);
                        break;
                    }
                    if (k == PDF_TOKEN_AS_OBJ(TOKEN_STREAM)) {
                        pdf_dict *d;
                        pdf_name *n = NULL;

                        if (pdfi_count_stack(ctx) <= 1) {
                            pdfi_clearstack(ctx);
                            break;;
                        }
                        d = (pdf_dict *)ctx->stack_top[-2];
                        if (pdfi_type_of(d) != PDF_DICT) {
                            pdfi_clearstack(ctx);
                            break;;
                        }
                        code = pdfi_dict_knownget_type(ctx, d, "Type", PDF_NAME, (pdf_obj **)&n);
                        if (code < 0) {
                            if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_UNREPAIRABLE, "pdfi_repair_file", NULL)) < 0) {
                                pdfi_clearstack(ctx);
                                goto exit;
                            }
                        }
                        if (code > 0) {
                            if (pdfi_name_is(n, "ObjStm")) {
                                int64_t N;
                                int obj_num, offset;
                                int j;
                                pdf_c_stream *compressed_stream;
                                pdf_stream *stream;

                                offset = pdfi_unread_tell(ctx);
                                pdfi_seek(ctx, ctx->main_stream, offset, SEEK_SET);

                                code = pdfi_obj_dict_to_stream(ctx, d, &stream, true);
                                if (code == 0)
                                    code = pdfi_filter(ctx, stream, ctx->main_stream, &compressed_stream, false);

                                pdfi_countdown(stream);

                                if (code == 0) {
                                    code = pdfi_dict_get_int(ctx, d, "N", &N);
                                    if (code == 0) {
                                        for (j=0;j < N; j++) {
                                            code = pdfi_read_bare_int(ctx, compressed_stream, &obj_num);
                                            if (code <= 0)
                                                break;
                                            else {
                                                code = pdfi_read_bare_int(ctx, compressed_stream, &offset);
                                                if (code > 0) {
                                                    if (obj_num < 1) {
                                                        pdfi_close_file(ctx, compressed_stream);
                                                        pdfi_countdown(n);
                                                        pdfi_clearstack(ctx);
                                                        code = gs_note_error(gs_error_rangecheck);
                                                        goto exit;
                                                    }
                                                    if (obj_num >= ctx->xref_table->xref_size)
                                                        code = pdfi_repair_add_object(ctx, obj_num, 0, 0);

                                                    if (code >= 0) {
                                                        ctx->xref_table->xref[obj_num].compressed = true;
                                                        ctx->xref_table->xref[obj_num].free = false;
                                                        ctx->xref_table->xref[obj_num].object_num = obj_num;
                                                        ctx->xref_table->xref[obj_num].u.compressed.compressed_stream_num = i;
                                                        ctx->xref_table->xref[obj_num].u.compressed.object_index = j;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    pdfi_close_file(ctx, compressed_stream);
                                }
                                if (code < 0) {
                                    if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_UNREPAIRABLE, "pdfi_repair_file", NULL)) < 0) {
                                        pdfi_countdown(n);
                                        pdfi_clearstack(ctx);
                                        goto exit;
                                    }
                                }
                            }
                        }
                        pdfi_countdown(n);
                        pdfi_clearstack(ctx);
                        break;
                    }
                }
            } while (1);
        }
    }

exit:
    if (code > 0)
        code = 0;
    pdfi_seek(ctx, ctx->main_stream, saved_offset, SEEK_SET);
    ctx->main_stream->eof = false;
    ctx->repairing = false;
    return code;
}
