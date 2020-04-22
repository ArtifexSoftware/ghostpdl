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

/* Page-level operations for the PDF interpreter */

#include "pdf_int.h"
#include "plmain.h"
#include "pdf_stack.h"
#include "pdf_page.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"
#include "pdf_gstate.h"
#include "pdf_misc.h"
#include "pdf_optcontent.h"
#include "pdf_device.h"
#include "pdf_annot.h"
#include "pdf_check.h"

static int pdfi_process_page_contents(pdf_context *ctx, pdf_dict *page_dict)
{
    int i, code = 0;
    pdf_obj *o, *o1;

    code = pdfi_dict_get(ctx, page_dict, "Contents", &o);
    if (code == gs_error_undefined)
        /* Don't throw an error if there are no contents, just render nothing.... */
        return 0;
    if (code < 0)
        return code;

    if (o->type == PDF_INDIRECT) {
        if (((pdf_indirect_ref *)o)->ref_object_num == page_dict->object_num)
            return_error(gs_error_circular_reference);

        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o)->ref_object_num, ((pdf_indirect_ref *)o)->ref_generation_num, &o1);
        pdfi_countdown(o);
        if (code < 0) {
            if (code == gs_error_VMerror)
                return code;
            return 0;
        }
        o = o1;
    }

    code = pdfi_gsave(ctx);
    if (code < 0) {
        pdfi_countdown(o);
        return code;
    }

    ctx->decrypt_strings = false;
    if (o->type == PDF_ARRAY) {
        pdf_array *a = (pdf_array *)o;

        for (i=0;i < pdfi_array_size(a); i++) {
            pdf_indirect_ref *r;
            code = pdfi_array_get_no_deref(ctx, a, i, (pdf_obj **)&r);
            if (code < 0)
                goto page_error;
            if (r->type == PDF_DICT) {
                code = pdfi_interpret_content_stream(ctx, (pdf_dict *)r, page_dict);
                pdfi_countdown(r);
                if (code < 0)
                    goto page_error;
            } else {
                if (r->type != PDF_INDIRECT) {
                        pdfi_countdown(r);
                        code = gs_note_error(gs_error_typecheck);
                        goto page_error;
                } else {
                    if (r->ref_object_num == page_dict->object_num) {
                        pdfi_countdown(r);
                        code = gs_note_error(gs_error_circular_reference);
                        goto page_error;
                    }
                    code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
                    pdfi_countdown(r);
                    if (code < 0) {
                        if (code != gs_error_VMerror || ctx->pdfstoponerror == false)
                            code = 0;
                        goto page_error;
                    }
                    if (o1->type != PDF_DICT) {
                        pdfi_countdown(o1);
                        code = gs_note_error(gs_error_typecheck);
                        goto page_error;
                    }
                    code = pdfi_interpret_content_stream(ctx, (pdf_dict *)o1, page_dict);
                    pdfi_countdown(o1);
                    if (code < 0) {
                        if (code == gs_error_VMerror || ctx->pdfstoponerror == true)
                            goto page_error;
                    }
                }
            }
        }
    } else {
        if (o->type == PDF_DICT) {
            code = pdfi_interpret_content_stream(ctx, (pdf_dict *)o, page_dict);
        } else {
            pdfi_countdown(o);
            ctx->decrypt_strings = true;
            return_error(gs_error_typecheck);
        }
    }
page_error:
    ctx->decrypt_strings = true;
    pdfi_clearstack(ctx);
    pdfi_grestore(ctx);
    pdfi_countdown(o);
    return code;
}

/* Render one page (including annotations) (see pdf_main.ps/showpagecontents) */
static int pdfi_process_one_page(pdf_context *ctx, pdf_dict *page_dict)
{
    stream_save local_entry_save;
    int code, code1;

    /* Save the current stream state, for later cleanup, in a local variable */
    local_save_stream_state(ctx, &local_entry_save);
    initialise_stream_save(ctx);

    code = pdfi_process_page_contents(ctx, page_dict);

    /* Put our state back the way it was before we ran the contents
     * and check if the stream had problems
     */
#ifdef PROBE_STREAMS
    if (ctx->pgs->level > ctx->current_stream_save.gsave_level ||
        pdfi_count_stack(ctx) > ctx->current_stream_save.stack_count)
        code = ((pdf_context *)0)->first_page;
#endif

    cleanup_context_interpretation(ctx, &local_entry_save);
    local_restore_stream_state(ctx, &local_entry_save);

    code1 = pdfi_do_annotations(ctx, page_dict);
    if (code > 0)
        code = code1;

    /* TODO: Handle "ShowAcroForm" goes here */

    return code;
}

/* See pdf_PDF2PS_matrix and .pdfshowpage_Install */
static void pdfi_set_ctm(pdf_context *ctx)
{
    gs_matrix mat;

    /* Adjust for UserUnits */
    mat.xx = ctx->UserUnit;
    mat.xy = 0;
    mat.yx = 0;
    mat.yy = ctx->UserUnit;
    mat.tx = 0;
    mat.ty = 0;
    gs_concat(ctx->pgs, &mat);

    /* We need to make sure the default matrix is properly set.
     * If we do gs_initgraphics() later (such as for annotations)
     * then it uses this default matrix if it is set.
     * Needed for page rotations to work correctly with Annotations.
     */
    gs_setdefaultmatrix(ctx->pgs, &ctm_only(ctx->pgs));

}

static int pdfi_set_media_size(pdf_context *ctx, pdf_dict *page_dict)
{
    gs_c_param_list list;
    gs_param_float_array fa;
    pdf_array *a = NULL, *default_media = NULL;
    float fv[2];
    double d[4];
    int code;
    uint64_t i;
    int64_t rotate = 0;
    double userunit = 1.0;

    code = pdfi_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&default_media);
    if (code < 0) {
        code = gs_erasepage(ctx->pgs);
        return 0;
    }

    if (ctx->usecropbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "CropBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->useartbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "ArtBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->usebleedbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "BleedBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->usetrimbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (a == NULL)
        a = default_media;

    if (!ctx->nouserunit) {
        (void)pdfi_dict_knownget_number(ctx, page_dict, "UserUnit", &userunit);
    }
    ctx->UserUnit = userunit;

    for (i=0;i<4;i++) {
        code = pdfi_array_get_number(ctx, a, i, &d[i]);
        d[i] *= userunit;
    }
    pdfi_countdown(a);

    normalize_rectangle(d);
    memcpy(ctx->PageSize, d, 4 * sizeof(double));

    code = pdfi_dict_get_int(ctx, page_dict, "Rotate", &rotate);

    rotate = rotate % 360;

    switch(rotate) {
        default:
        case 0:
        case 360:
        case -180:
        case 180:
            fv[0] = (float)(d[2] - d[0]);
            fv[1] = (float)(d[3] - d[1]);
            break;
        case -90:
        case 90:
        case -270:
        case 270:
            fv[1] = (float)(d[2] - d[0]);
            fv[0] = (float)(d[3] - d[1]);
            break;
    }

    fa.persistent = false;
    fa.data = fv;
    fa.size = 2;

    /* ----- setup specific device parameters ----- */
    gs_c_param_list_write(&list, ctx->memory);

    /* Setup the device transparency mode for this page */
    code = param_write_bool((gs_param_list *)&list, "PageUsesTransparency",
                            &ctx->page_has_transparency);

    code = param_write_float_array((gs_param_list *)&list, ".MediaSize", &fa);
    if (code >= 0)
    {
        gx_device *dev = gs_currentdevice(ctx->pgs);

        gs_c_param_list_read(&list);
        code = gs_putdeviceparams(dev, (gs_param_list *)&list);
        if (code < 0) {
            gs_c_param_list_release(&list);
            return code;
        }
    }
    gs_c_param_list_release(&list);
    /* ----- end setup specific device parameters ----- */

    /* Resets the default matrix to NULL before doing initgraphics, because
     * otherwise initgraphics would keep old matrix.
     * (see pdfi_set_ctm())
     */
    gs_setdefaultmatrix(ctx->pgs, NULL);
    gs_initgraphics(ctx->pgs);

    switch(rotate) {
        case 0:
            break;
        case -270:
        case 90:
            gs_translate(ctx->pgs, 0, fv[1]);
            gs_rotate(ctx->pgs, -90);
            break;
        case -180:
        case 180:
            gs_translate(ctx->pgs, fv[0], fv[1]);
            gs_rotate(ctx->pgs, 180);
            break;
        case -90:
        case 270:
            gs_translate(ctx->pgs, fv[0], 0);
            gs_rotate(ctx->pgs, 90);
            break;
        default:
            break;
    }

    gs_translate(ctx->pgs, d[0] * -1, d[1] * -1);

    code = gs_erasepage(ctx->pgs);
    return 0;
}

/* Setup default transfer functions */
static void pdfi_setup_transfers(pdf_context *ctx)
{
    if (ctx->pgs->set_transfer.red == 0x00) {
        ctx->DefaultTransfers[0].proc = gs_identity_transfer;
        memset(ctx->DefaultTransfers[0].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->DefaultTransfers[0].proc = ctx->pgs->set_transfer.red->proc;
        memcpy(ctx->DefaultTransfers[0].values, ctx->pgs->set_transfer.red->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->set_transfer.green == 0x00) {
        ctx->DefaultTransfers[1].proc = gs_identity_transfer;
        memset(ctx->DefaultTransfers[1].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->DefaultTransfers[1].proc = ctx->pgs->set_transfer.green->proc;
        memcpy(ctx->DefaultTransfers[1].values, ctx->pgs->set_transfer.green->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->set_transfer.blue == 0x00) {
        ctx->DefaultTransfers[2].proc = gs_identity_transfer;
        memset(ctx->DefaultTransfers[2].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->DefaultTransfers[2].proc = ctx->pgs->set_transfer.blue->proc;
        memcpy(ctx->DefaultTransfers[2].values, ctx->pgs->set_transfer.blue->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->set_transfer.gray == 0x00) {
        ctx->DefaultTransfers[3].proc = gs_identity_transfer;
        memset(ctx->DefaultTransfers[3].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->DefaultTransfers[3].proc = ctx->pgs->set_transfer.gray->proc;
        memcpy(ctx->DefaultTransfers[3].values, ctx->pgs->set_transfer.gray->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->black_generation == 0x00) {
        ctx->DefaultBG.proc = gs_identity_transfer;
        memset(ctx->DefaultBG.values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->DefaultBG.proc = ctx->pgs->black_generation->proc;
        memcpy(ctx->DefaultBG.values, ctx->pgs->black_generation->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->undercolor_removal == 0x00) {
        ctx->DefaultUCR.proc = gs_identity_transfer;
        memset(ctx->DefaultUCR.values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->DefaultUCR.proc = ctx->pgs->undercolor_removal->proc;
        memcpy(ctx->DefaultUCR.values, ctx->pgs->undercolor_removal->values, transfer_map_size * sizeof(frac));
    }
}

int pdfi_page_render(pdf_context *ctx, uint64_t page_num)
{
    int code, code1=0;
    uint64_t page_offset = 0;
    pdf_dict *page_dict = NULL;
    bool page_group_known = false;
    pdf_dict *group_dict = NULL;

    if (page_num > ctx->num_pages)
        return_error(gs_error_rangecheck);

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Processing Page %"PRIi64" content stream\n", page_num + 1);

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_loop_detector_add_object(ctx, ctx->Pages->object_num);
    if (code < 0) {
        pdfi_loop_detector_cleartomark(ctx);
        goto exit2;
    }

    code = pdfi_get_page_dict(ctx, ctx->Pages, page_num, &page_offset, &page_dict, NULL);
    pdfi_loop_detector_cleartomark(ctx);
    if (code < 0) {
        if (code == gs_error_VMerror || ctx->pdfstoponerror)
            goto exit2;
        code = 0;
        goto exit2;
    }

    if (code > 0) {
        code = gs_note_error(gs_error_unknownerror);
        goto exit2;
    }

    code = pdfi_check_page(ctx, page_dict, true);
    if (code < 0) goto exit2;

    dbgmprintf1(ctx->memory, "Current page transparency setting is %d", ctx->page_has_transparency);
    if (ctx->spot_capable_device)
        dbgmprintf1(ctx->memory, ", spots=%d\n", ctx->page_num_spots);
    else
        dbgmprintf(ctx->memory, "\n");

    code = pdfi_set_media_size(ctx, page_dict);
    if (code < 0) goto exit2;
    pdfi_set_ctm(ctx);

    code = pdfi_dict_knownget_type(ctx, page_dict, "Group", PDF_DICT, (pdf_obj **)&group_dict);
    if (code < 0) goto exit2;
    if (group_dict != NULL)
        page_group_known = true;

    pdfi_countdown(ctx->CurrentPageDict);
    ctx->CurrentPageDict = page_dict;
    pdfi_countup(ctx->CurrentPageDict);

    code = gs_setstrokeconstantalpha(ctx->pgs, 1.0);
    code = gs_setfillconstantalpha(ctx->pgs, 1.0);
    code = gs_setalphaisshape(ctx->pgs, 0);
    code = gs_setblendmode(ctx->pgs, BLEND_MODE_Compatible);
    code = gs_settextknockout(ctx->pgs, true);
    code = gs_settextspacing(ctx->pgs, (double)0.0);
    code = gs_settextleading(ctx->pgs, (double)0.0);
    gs_settextrenderingmode(ctx->pgs, 0);
    code = gs_setwordspacing(ctx->pgs, (double)0.0);
    code = gs_settexthscaling(ctx->pgs, (double)100.0);
    code = gs_setsmoothness(ctx->pgs, 0.02); /* Match gs code */
    ctx->TextBlockDepth = 0;

    pdfi_setup_transfers(ctx);

    /* Set whether device needs OP support
     * This needs to be before transparency device is pushed, if applicable
     */
    pdfi_trans_set_needs_OP(ctx);
    pdfi_device_set_flags(ctx);
    pdfi_oc_init(ctx);

    pdfi_gsave(ctx);

    if (ctx->page_has_transparency) {
        if (code >= 0) {
            /* We don't retain the PDF14 device */
            code = gs_push_pdf14trans_device(ctx->pgs, false, false);
            if (code >= 0) {
                if (page_group_known) {
                    code = pdfi_trans_begin_page_group(ctx, page_dict, group_dict);
                    /* If setting the page group failed for some reason, abandon the page group, but continue with the page */
                    if (code < 0)
                        page_group_known = false;
                }
            } else {
                /* Couldn't push the transparency compositor.
                 * This is probably fatal, but attempt to recover by abandoning transparency
                 */
                ctx->page_has_transparency = false;
            }
        } else {
            /* Couldn't gsave round the transparency compositor.
             * This is probably fatal, but attempt to recover by abandoning transparency
             */
            ctx->page_has_transparency = false;
        }
    }

    /* Init a base_pgs graphics state for Patterns
     * (this has to be after transparency device pushed, if applicable)
     */
    pdfi_set_DefaultQState(ctx, ctx->pgs);

    /* Render one page (including annotations) */
    code = pdfi_process_one_page(ctx, page_dict);

    if (ctx->page_has_transparency && page_group_known) {
        code1 = pdfi_trans_end_group(ctx);
    }

    pdfi_countdown(ctx->CurrentPageDict);
    ctx->CurrentPageDict = NULL;

    if (ctx->page_has_transparency) {
        if (code1 < 0) {
            (void)gs_abort_pdf14trans_device(ctx->pgs);
            goto exit1;
        }

        code = gs_pop_pdf14trans_device(ctx->pgs, false);
        if (code < 0) {
            goto exit1;
        }
    }

 exit1:
    pdfi_grestore(ctx);
 exit2:
    pdfi_countdown(page_dict);
    pdfi_countdown(group_dict);

    if (code == 0)
        code = pl_finish_page(ctx->memory->gs_lib_ctx->top_of_system, ctx->pgs, 1, true);
    return code;
}
