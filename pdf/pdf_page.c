/* Copyright (C) 2019-2023 Artifex Software, Inc.
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

/* Page-level operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_doc.h"
#include "pdf_deref.h"
#include "pdf_page.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "pdf_misc.h"
#include "pdf_optcontent.h"
#include "pdf_device.h"
#include "pdf_annot.h"
#include "pdf_check.h"
#include "pdf_mark.h"
#include "pdf_font.h"

#include "gscoord.h"        /* for gs_concat() and others */
#include "gspaint.h"        /* For gs_erasepage() */
#include "gsstate.h"        /* For gs_initgraphics() */
#include "gspath2.h"        /* For gs_rectclip() */

static int pdfi_process_page_contents(pdf_context *ctx, pdf_dict *page_dict)
{
    int i, code = 0;
    pdf_obj *o, *o1;

#if PURGE_CACHE_PER_PAGE
    pdfi_purge_obj_cache(ctx);
#endif

    code = pdfi_dict_get(ctx, page_dict, "Contents", &o);
    if (code == gs_error_undefined)
        /* Don't throw an error if there are no contents, just render nothing.... */
        return 0;
    if (code < 0)
        return code;

    if (pdfi_type_of(o) == PDF_INDIRECT) {
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

    ctx->encryption.decrypt_strings = false;
    if (pdfi_type_of(o) == PDF_ARRAY) {
        pdf_array *a = (pdf_array *)o;

        for (i=0;i < pdfi_array_size(a); i++) {
            pdf_indirect_ref *r;
            code = pdfi_array_get_no_deref(ctx, a, i, (pdf_obj **)&r);
            if (code < 0)
                goto page_error;
            if (pdfi_type_of (r) == PDF_STREAM) {
                code = pdfi_interpret_content_stream(ctx, NULL, (pdf_stream *)r, page_dict);
                pdfi_countdown(r);
                if (code < 0)
                    goto page_error;
            } else {
                if (pdfi_type_of(r) != PDF_INDIRECT) {
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
                        if (code != gs_error_VMerror || ctx->args.pdfstoponerror == false)
                            code = 0;
                        goto page_error;
                    }
                    if (pdfi_type_of(o1) != PDF_STREAM) {
                        pdfi_countdown(o1);
                        code = gs_note_error(gs_error_typecheck);
                        goto page_error;
                    }
                    code = pdfi_interpret_content_stream(ctx, NULL, (pdf_stream *)o1, page_dict);
                    pdfi_countdown(o1);
                    if (code < 0) {
                        if (code == gs_error_VMerror || ctx->args.pdfstoponerror == true)
                            goto page_error;
                    }
                }
            }
        }
    } else {
        if (pdfi_type_of(o) == PDF_STREAM) {
            code = pdfi_interpret_content_stream(ctx, NULL, (pdf_stream *)o, page_dict);
        } else
            code = gs_note_error(gs_error_typecheck);
    }
page_error:
    ctx->encryption.decrypt_strings = true;
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
#if PROBE_STREAMS
    if (ctx->pgs->level > ctx->current_stream_save.gsave_level ||
        pdfi_count_stack(ctx) > ctx->current_stream_save.stack_count)
        code = ((pdf_context *)0)->first_page;
#endif

    cleanup_context_interpretation(ctx, &local_entry_save);
    local_restore_stream_state(ctx, &local_entry_save);

    local_save_stream_state(ctx, &local_entry_save);
    initialise_stream_save(ctx);

    code1 = pdfi_do_annotations(ctx, page_dict);
    if (code >= 0) code = code1;

    cleanup_context_interpretation(ctx, &local_entry_save);
    local_restore_stream_state(ctx, &local_entry_save);

    local_save_stream_state(ctx, &local_entry_save);
    initialise_stream_save(ctx);

    code1 = pdfi_do_acroform(ctx, page_dict);
    if (code >= 0) code = code1;

    cleanup_context_interpretation(ctx, &local_entry_save);
    local_restore_stream_state(ctx, &local_entry_save);

    if (ctx->text.BlockDepth != 0) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_UNBLANACED_BT, "pdfi_process_one_page", "");
        ctx->text.BlockDepth = 0;
    }
    return code;
}

/* See pdf_PDF2PS_matrix and .pdfshowpage_Install */
static void pdfi_set_ctm(pdf_context *ctx)
{
    gs_matrix mat;

    /* Adjust for page.UserUnits */
    mat.xx = ctx->page.UserUnit;
    mat.xy = 0;
    mat.yx = 0;
    mat.yy = ctx->page.UserUnit;
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

/* Get .MediaSize from the device to setup page.Size in context */
static int pdfi_get_media_size(pdf_context *ctx, pdf_dict *page_dict)
{
    pdf_array *a = NULL, *default_media = NULL;
    double d[4];
    int code = 0;
    uint64_t i;
    double userunit = 1.0;

    code = pdfi_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&default_media);
    if (code < 0) {
        pdfi_set_warning(ctx, code, NULL, W_PDF_BAD_MEDIABOX, "pdfi_get_media_size", NULL);
        code = gs_erasepage(ctx->pgs);
        return 0;
    }

    if (ctx->args.usecropbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "CropBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->args.useartbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "ArtBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->args.usebleedbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "BleedBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->args.usetrimbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "TrimBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (a == NULL) {
        a = default_media;
        pdfi_countup(a);
    }

    if (!ctx->args.nouserunit && !ctx->device_state.PassUserUnit) {
        (void)pdfi_dict_knownget_number(ctx, page_dict, "UserUnit", &userunit);
    }
    ctx->page.UserUnit = userunit;

    for (i=0;i<4;i++) {
        code = pdfi_array_get_number(ctx, a, i, &d[i]);
        d[i] *= userunit;
    }
    pdfi_countdown(a);
    pdfi_countdown(default_media);

    normalize_rectangle(d);
    memcpy(ctx->page.Size, d, 4 * sizeof(double));

    return code;
}

static int pdfi_set_media_size(pdf_context *ctx, pdf_dict *page_dict)
{
    gs_c_param_list list;
    gs_param_float_array fa;
    pdf_array *a = NULL, *default_media = NULL;
    float fv[2];
    double d[4], d_crop[4];
    gs_rect bbox;
    int code, do_crop = false;
    uint64_t i;
    int64_t rotate = 0;
    double userunit = 1.0;

    code = pdfi_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&default_media);
    if (code < 0) {
        pdfi_set_warning(ctx, code, NULL, W_PDF_BAD_MEDIABOX, "pdfi_get_media_size", NULL);
        code = gs_erasepage(ctx->pgs);
        return 0;
    }

    if (ctx->args.usecropbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "CropBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->args.useartbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "ArtBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->args.usebleedbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "BleedBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->args.usetrimbox) {
        if (a != NULL)
            pdfi_countdown(a);
        (void)pdfi_dict_get_type(ctx, page_dict, "TrimBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (a == NULL) {
        code = pdfi_dict_get_type(ctx, page_dict, "CropBox", PDF_ARRAY, (pdf_obj **)&a);
        if (code >= 0 && pdfi_array_size(a) >= 4) {
            pdf_obj *box_obj = NULL;

            for (i=0;i<4;i++) {
                code = pdfi_array_get_no_store_R(ctx, a, i, &box_obj);
                if (code >= 0) {
                    code = pdfi_obj_to_real(ctx, box_obj, &d_crop[i]);
                    pdfi_countdown(box_obj);
                }
                if (code < 0)
                    break;
            }
            pdfi_countdown(a);
            if (code >= 0) {
                normalize_rectangle(d_crop);
                memcpy(ctx->page.Crop, d_crop, 4 * sizeof(double));
                do_crop = true;
            }
        }
        a = default_media;
    }

    if (!ctx->args.nouserunit) {
        if (ctx->device_state.PassUserUnit) {
            double unit = 1.0;
            (void)pdfi_dict_knownget_number(ctx, page_dict, "UserUnit", &unit);
            (void)pdfi_device_set_param_float(ctx->pgs->device, "UserUnit", unit);
        } else {
            (void)pdfi_dict_knownget_number(ctx, page_dict, "UserUnit", &userunit);
        }
    }
    ctx->page.UserUnit = userunit;

    for (i=0;i<4;i++) {
        pdf_obj *box_obj = NULL;

        code = pdfi_array_get_no_store_R(ctx, a, i, &box_obj);
        if (code >= 0) {
            code = pdfi_obj_to_real(ctx, box_obj, &d[i]);
            pdfi_countdown(box_obj);
        }

        if (code < 0) {
            pdfi_countdown(a);
            pdfi_set_warning(ctx, code, NULL, W_PDF_BAD_MEDIABOX, "pdfi_get_media_size", NULL);
            code = gs_erasepage(ctx->pgs);
            return 0;
        }
        d[i] *= userunit;
    }
    pdfi_countdown(a);

    normalize_rectangle(d);
    memcpy(ctx->page.Size, d, 4 * sizeof(double));

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

    if (do_crop) {
        bbox.p.x = d_crop[0] - d[0];
        bbox.p.y = d_crop[1] - d[1];
        bbox.q.x = d_crop[2] - d[0];
        bbox.q.y = d_crop[3] - d[1];

        code = gs_rectclip(ctx->pgs, &bbox, 1);
        if (code < 0)
            return code;
    }

    gs_translate(ctx->pgs, d[0] * -1, d[1] * -1);

    code = gs_erasepage(ctx->pgs);
    return 0;
}

/* Setup default transfer functions */
static void pdfi_setup_transfers(pdf_context *ctx)
{
    if (ctx->pgs->set_transfer.red == 0x00) {
        ctx->page.DefaultTransfers[0].proc = gs_identity_transfer;
        memset(ctx->page.DefaultTransfers[0].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->page.DefaultTransfers[0].proc = ctx->pgs->set_transfer.red->proc;
        memcpy(ctx->page.DefaultTransfers[0].values, ctx->pgs->set_transfer.red->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->set_transfer.green == 0x00) {
        ctx->page.DefaultTransfers[1].proc = gs_identity_transfer;
        memset(ctx->page.DefaultTransfers[1].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->page.DefaultTransfers[1].proc = ctx->pgs->set_transfer.green->proc;
        memcpy(ctx->page.DefaultTransfers[1].values, ctx->pgs->set_transfer.green->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->set_transfer.blue == 0x00) {
        ctx->page.DefaultTransfers[2].proc = gs_identity_transfer;
        memset(ctx->page.DefaultTransfers[2].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->page.DefaultTransfers[2].proc = ctx->pgs->set_transfer.blue->proc;
        memcpy(ctx->page.DefaultTransfers[2].values, ctx->pgs->set_transfer.blue->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->set_transfer.gray == 0x00) {
        ctx->page.DefaultTransfers[3].proc = gs_identity_transfer;
        memset(ctx->page.DefaultTransfers[3].values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->page.DefaultTransfers[3].proc = ctx->pgs->set_transfer.gray->proc;
        memcpy(ctx->page.DefaultTransfers[3].values, ctx->pgs->set_transfer.gray->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->black_generation == 0x00) {
        ctx->page.DefaultBG.proc = gs_identity_transfer;
        memset(ctx->page.DefaultBG.values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->page.DefaultBG.proc = ctx->pgs->black_generation->proc;
        memcpy(ctx->page.DefaultBG.values, ctx->pgs->black_generation->values, transfer_map_size * sizeof(frac));
    }
    if (ctx->pgs->undercolor_removal == 0x00) {
        ctx->page.DefaultUCR.proc = gs_identity_transfer;
        memset(ctx->page.DefaultUCR.values, 0x00, transfer_map_size * sizeof(frac));
    } else {
        ctx->page.DefaultUCR.proc = ctx->pgs->undercolor_removal->proc;
        memcpy(ctx->page.DefaultUCR.values, ctx->pgs->undercolor_removal->values, transfer_map_size * sizeof(frac));
    }
}

/* Return a dictionary containing information about the page. Basic information is that
 * required to render the page; if extended is true then additionally contains an
 * array of spot ink names and an array of dictionaries each of which contains
 * information about a font used on the page. THis is normally only used for tools
 * like pdf_info.ps
 */
int pdfi_page_info(pdf_context *ctx, uint64_t page_num, pdf_dict **info, bool extended)
{
    int code = 0, i=0;
    pdf_dict *page_dict = NULL, *info_dict = NULL;
    pdf_array *fonts_array = NULL, *spots_array = NULL;
    pdf_array *a = NULL;
    pdf_obj *o = NULL;
    bool known = false;
    double dummy;

    code = pdfi_page_get_dict(ctx, page_num, &page_dict);
    if (code < 0)
        return code;

    if (code > 0) {
        code = gs_note_error(gs_error_unknownerror);
        goto done;
    }

    code = pdfi_dict_alloc(ctx, 6, &info_dict);
    if (code < 0)
        goto done;

    pdfi_countup(info_dict);

    if (extended)
        code = pdfi_check_page(ctx, page_dict, &fonts_array, &spots_array, false);
    else
        code = pdfi_check_page(ctx, page_dict, NULL, NULL, false);
    if (code < 0)
        goto done;

    if (spots_array != NULL) {
        code = pdfi_dict_put(ctx, info_dict, "Spots", (pdf_obj *)spots_array);
        if (code < 0)
            goto done;
        pdfi_countdown(spots_array);
    }

    if (fonts_array != NULL) {
        code = pdfi_dict_put(ctx, info_dict, "Fonts", (pdf_obj *)fonts_array);
        if (code < 0)
            goto done;
        pdfi_countdown(fonts_array);
    }

    code = pdfi_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0)
        pdfi_set_warning(ctx, code, NULL, W_PDF_BAD_MEDIABOX, "pdfi_page_info", NULL);

    if (code >= 0) {
        pdf_obj *box_obj = NULL;

        for (i = 0;i < pdfi_array_size(a); i++) {
            code = pdfi_array_get_no_store_R(ctx, a, i, &box_obj);
            if (code >= 0) {
                code = pdfi_obj_to_real(ctx, box_obj, &dummy);
                pdfi_countdown(box_obj);
            }
            if (code < 0) {
                pdfi_set_warning(ctx, code, NULL, W_PDF_BAD_MEDIABOX, "pdfi_page_info", NULL);
                goto done;
            }
        }

        code = pdfi_dict_put(ctx, info_dict, "MediaBox", (pdf_obj *)a);
        if (code < 0)
            goto done;
        pdfi_countdown(a);
        a = NULL;
    }

    code = pdfi_dict_get_type(ctx, page_dict, "ArtBox", PDF_ARRAY, (pdf_obj **)&a);
    if (code >= 0) {
        pdf_obj *box_obj = NULL;

        for (i = 0;i < pdfi_array_size(a); i++) {
            code = pdfi_array_get_no_store_R(ctx, a, i, &box_obj);
            if (code >= 0) {
                code = pdfi_obj_to_real(ctx, box_obj, &dummy);
                pdfi_countdown(box_obj);
            }
            if (code < 0)
                break;
        }
        if (code >= 0) {
            code = pdfi_dict_put(ctx, info_dict, "ArtBox", (pdf_obj *)a);
            if (code < 0)
                goto done;
        }
        pdfi_countdown(a);
        a = NULL;
    }

    code = pdfi_dict_get_type(ctx, page_dict, "CropBox", PDF_ARRAY, (pdf_obj **)&a);
    if (code >= 0) {
        pdf_obj *box_obj = NULL;

        for (i = 0;i < pdfi_array_size(a); i++) {
            code = pdfi_array_get_no_store_R(ctx, a, i, &box_obj);
            if (code >= 0) {
                code = pdfi_obj_to_real(ctx, box_obj, &dummy);
                pdfi_countdown(box_obj);
            }
            if (code < 0)
                break;
        }
        if (code >= 0) {
            code = pdfi_dict_put(ctx, info_dict, "CropBox", (pdf_obj *)a);
            if (code < 0)
                goto done;
        }
        pdfi_countdown(a);
        a = NULL;
    }

    code = pdfi_dict_get_type(ctx, page_dict, "TrimBox", PDF_ARRAY, (pdf_obj **)&a);
    if (code >= 0) {
        pdf_obj *box_obj = NULL;

        for (i = 0;i < pdfi_array_size(a); i++) {
            code = pdfi_array_get_no_store_R(ctx, a, i, &box_obj);
            if (code >= 0) {
                code = pdfi_obj_to_real(ctx, box_obj, &dummy);
                pdfi_countdown(box_obj);
            }
            if (code < 0)
                break;
        }
        if (code >= 0) {
            code = pdfi_dict_put(ctx, info_dict, "TrimBox", (pdf_obj *)a);
            if (code < 0)
                goto done;
        }
        pdfi_countdown(a);
        a = NULL;
    }

    code = pdfi_dict_get_type(ctx, page_dict, "BleedBox", PDF_ARRAY, (pdf_obj **)&a);
    if (code >= 0) {
        pdf_obj *box_obj = NULL;

        for (i = 0;i < pdfi_array_size(a); i++) {
            code = pdfi_array_get_no_store_R(ctx, a, i, &box_obj);
            if (code >= 0) {
                code = pdfi_obj_to_real(ctx, box_obj, &dummy);
                pdfi_countdown(box_obj);
            }
            if (code < 0)
                break;
        }
        if (code >= 0) {
            code = pdfi_dict_put(ctx, info_dict, "BleedBox", (pdf_obj *)a);
            if (code < 0)
                goto done;
        }
        pdfi_countdown(a);
        a = NULL;
    }
    code = 0;

    code = pdfi_dict_get(ctx, page_dict, "Rotate", &o);
    if (code >= 0) {
        if (pdfi_type_of(o) == PDF_INT || pdfi_type_of(o) == PDF_REAL) {
            code = pdfi_dict_put(ctx, info_dict, "Rotate", o);
            if (code < 0)
                goto done;
        }
        pdfi_countdown(o);
    }

    code = pdfi_dict_get(ctx, page_dict, "UserUnit", &o);
    if (code >= 0) {
        if (pdfi_type_of(o) == PDF_INT || pdfi_type_of(o) == PDF_REAL) {
            code = pdfi_dict_put(ctx, info_dict, "UserUnit", o);
            if (code < 0)
                goto done;
        }
        pdfi_countdown(o);
    }

    if (ctx->page.has_transparency)
        code = pdfi_dict_put(ctx, info_dict, "UsesTransparency", PDF_TRUE_OBJ);
    else
        code = pdfi_dict_put(ctx, info_dict, "UsesTransparency", PDF_FALSE_OBJ);
    if (code < 0)
        goto done;

    code = pdfi_dict_known(ctx, page_dict, "Annots", &known);
    if (code >= 0 && known)
        code = pdfi_dict_put(ctx, info_dict, "Annots", PDF_TRUE_OBJ);
    else
        code = pdfi_dict_put(ctx, info_dict, "Annots", PDF_FALSE_OBJ);
    if (code < 0)
        goto done;

    code = pdfi_object_alloc(ctx, PDF_INT, 0, &o);
    if (code >= 0) {
        pdfi_countup(o);
        ((pdf_num *)o)->value.i = ctx->page.num_spots;
        code = pdfi_dict_put(ctx, info_dict, "NumSpots", o);
        pdfi_countdown(o);
        o = NULL;
        if (code < 0)
            goto done;
    }

done:
    if (code < 0) {
        pdfi_countdown(info_dict);
        info_dict = NULL;
        *info = NULL;
    } else
        *info = info_dict;

    pdfi_countdown(a);
    pdfi_countdown(page_dict);
    return code;
}

int pdfi_page_get_dict(pdf_context *ctx, uint64_t page_num, pdf_dict **dict)
{
    int code = 0;
    uint64_t page_offset = 0;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    if (ctx->PagesTree == NULL) {
        pdf_obj *o = NULL;
        pdf_name *n = NULL;
        /* The only way this should be true is if the Pages entry in the Root dictionary
         * points to a single instance of a Page dictionary, instead of to a Pages dictionary.
         * in which case, simply retrieve that dictionary and return.
         */
        code = pdfi_dict_get(ctx, ctx->Root, "Pages", &o);
        if (code < 0)
            goto page_error;
        if (pdfi_type_of(o) != PDF_DICT) {
            code = gs_note_error(gs_error_typecheck);
            goto page_error;
        }
        code = pdfi_dict_get_type(ctx, (pdf_dict *)o, "Type", PDF_NAME, (pdf_obj **)&n);
        if (code == 0) {
            if(pdfi_name_is(n, "Page")) {
                *dict = (pdf_dict *)o;
                pdfi_countup(*dict);
            } else
                code = gs_note_error(gs_error_undefined);
        }
page_error:
        pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        pdfi_countdown(n);
        return code;
    }

    code = pdfi_loop_detector_add_object(ctx, ctx->PagesTree->object_num);
    if (code < 0)
        goto exit;

    code = pdfi_get_page_dict(ctx, ctx->PagesTree, page_num, &page_offset, dict, NULL);
    if (code > 0)
        code = gs_error_unknownerror;

    /* Cache the page_dict number in page_array */
    if (*dict)
        ctx->page_array[page_num] = (*dict)->object_num;

 exit:
    pdfi_loop_detector_cleartomark(ctx);
    return code;
}

/* Find the page number that corresponds to a page dictionary
 * Uses page_array cache to minimize the number of times a page_dict needs to
 * be fetched, because this is expensive.
 */
int pdfi_page_get_number(pdf_context *ctx, pdf_dict *target_dict, uint64_t *page_num)
{
    uint64_t i;
    int code = 0;
    pdf_dict *page_dict = NULL;
    uint32_t object_num;

    for (i=0; i<ctx->num_pages; i++) {
        /* If the page has been processed before, then its object_num should already
         * be cached in the page_array, so check that first
         */
        object_num = ctx->page_array[i];
        if (object_num == 0) {
            /* It wasn't cached, so this will cache it */
            code = pdfi_page_get_dict(ctx, i, &page_dict);
            if (code < 0)
                continue;
            object_num = ctx->page_array[i];
        }
        if (target_dict->object_num == object_num) {
            *page_num = i;
            goto exit;
        }

        pdfi_countdown(page_dict);
        page_dict = NULL;
    }

    code = gs_note_error(gs_error_undefined);

 exit:
    pdfi_countdown(page_dict);
    return code;
}

static void release_page_DefaultSpaces(pdf_context *ctx)
{
    if (ctx->page.DefaultGray_cs != NULL) {
        if (ctx->page.DefaultGray_cs->interpreter_data != NULL) {
            pdf_obj *o = (pdf_obj *)(ctx->page.DefaultGray_cs->interpreter_data);
            if (o != NULL && pdfi_type_of(o) == PDF_NAME) {
                pdfi_countdown(o);
                ctx->page.DefaultGray_cs->interpreter_data = NULL;
            }
        }
        rc_decrement(ctx->page.DefaultGray_cs, "pdfi_page_render");
        ctx->page.DefaultGray_cs = NULL;
    }
    if (ctx->page.DefaultRGB_cs != NULL) {
        if (ctx->page.DefaultRGB_cs->interpreter_data != NULL) {
            pdf_obj *o = (pdf_obj *)(ctx->page.DefaultRGB_cs->interpreter_data);
            if (o != NULL && pdfi_type_of(o) == PDF_NAME) {
                pdfi_countdown(o);
                ctx->page.DefaultRGB_cs->interpreter_data = NULL;
            }
        }
        rc_decrement(ctx->page.DefaultRGB_cs, "pdfi_page_render");
        ctx->page.DefaultRGB_cs = NULL;
    }
    if (ctx->page.DefaultCMYK_cs != NULL) {
        if (ctx->page.DefaultCMYK_cs->interpreter_data != NULL) {
            pdf_obj *o = (pdf_obj *)(ctx->page.DefaultCMYK_cs->interpreter_data);
            if (o != NULL && pdfi_type_of(o) == PDF_NAME) {
                pdfi_countdown(o);
                ctx->page.DefaultCMYK_cs->interpreter_data = NULL;
            }
        }
        rc_decrement(ctx->page.DefaultCMYK_cs, "pdfi_page_render");
        ctx->page.DefaultCMYK_cs = NULL;
    }
}

static int setup_page_DefaultSpaces(pdf_context *ctx, pdf_dict *page_dict)
{
    /* First off, discard any dangling Default* colour spaces, just in case. */
    release_page_DefaultSpaces(ctx);

    return(pdfi_setup_DefaultSpaces(ctx, page_dict));
}

static bool
pdfi_pattern_purge_all_proc(gx_color_tile * ctile, void *proc_data)
{
    return true;
}

int pdfi_page_render(pdf_context *ctx, uint64_t page_num, bool init_graphics)
{
    int code, code1=0;
    pdf_dict *page_dict = NULL;
    bool page_group_known = false;
    pdf_dict *group_dict = NULL;
    bool page_dict_error = false;
    bool need_pdf14 = false; /* true if the device is needed and was successfully pushed */
    int trans_depth = 0; /* -1 means special mode for transparency simulation */

    if (page_num > ctx->num_pages)
        return_error(gs_error_rangecheck);

    if (ctx->args.pdfdebug)
        dmprintf1(ctx->memory, "%% Processing Page %"PRIi64" content stream\n", page_num + 1);

    code = pdfi_page_get_dict(ctx, page_num, &page_dict);
    if (code < 0) {
        char extra_info[256];

        page_dict_error = true;
        gs_snprintf(extra_info, sizeof(extra_info), "*** ERROR: Page %ld has invalid Page dict, skipping\n", page_num+1);
        pdfi_set_error(ctx, 0, NULL, E_PDF_PAGEDICTERROR, "pdfi_page_render", extra_info);
        if (code != gs_error_VMerror && !ctx->args.pdfstoponerror)
            code = 0;
        goto exit3;
    }

    code = pdfi_check_page(ctx, page_dict, NULL, NULL, init_graphics);
    if (code < 0)
        goto exit3;

    if (ctx->args.pdfdebug) {
        dbgmprintf2(ctx->memory, "Current page %ld transparency setting is %d", page_num+1,
                ctx->page.has_transparency);

        if (ctx->device_state.spot_capable)
            dbgmprintf1(ctx->memory, ", spots=%d\n", ctx->page.num_spots);
        else
            dbgmprintf(ctx->memory, "\n");
    }

    code = pdfi_dict_knownget_type(ctx, page_dict, "Group", PDF_DICT, (pdf_obj **)&group_dict);
    /* Ignore errors retrieving the Group dictionary, we will just ignore it. This allows us
     * to handle files such as Bug #705206 where the Group dictionary is a free object in a
     * compressed object stream.
     */
    if (code < 0)
        pdfi_set_error(ctx, 0, NULL, E_BAD_GROUP_DICT, "pdfi_page_render", NULL);
    if (group_dict != NULL)
        page_group_known = true;

    pdfi_countdown(ctx->page.CurrentPageDict);
    ctx->page.CurrentPageDict = page_dict;
    pdfi_countup(ctx->page.CurrentPageDict);

    /* In case we don't call pdfi_set_media_size, which sets this up.
     * We shouldn't ever use it in that case, but best to be safe.
     */
    ctx->page.UserUnit = 1.0f;
    /* If we are being called from the PDF interpreter then
     * we need to set up the page  and the default graphics state
     * but if we are being called from PostScript we do *not*
     * want to alter any of the graphics state or the media size.
     */
    /* TODO: I think this is a mix of things we might need to
     * still be setting up.
     * (for example, I noticed the blendmode and moved it outside the if)
     */
    if (init_graphics) {
        code = pdfi_set_media_size(ctx, page_dict);
        if (code < 0)
            goto exit2;

        pdfi_set_ctm(ctx);

    } else {
        /* Gets ctx->page.Size setup correctly
         * TODO: Probably not right if the page is rotated?
         * page.Size is needed by the transparency code,
         * not sure where else it might be used, if anywhere.
         */
        pdfi_get_media_size(ctx, page_dict);
    }

    /* Write the various CropBox, TrimBox etc to the device */
    pdfi_pdfmark_write_boxes(ctx, page_dict);

    code = setup_page_DefaultSpaces(ctx, page_dict);
    if (code < 0)
        goto exit2;

    pdfi_setup_transfers(ctx);

    /* Set whether device needs OP support
     * This needs to be before transparency device is pushed, if applicable
     */
    pdfi_trans_set_needs_OP(ctx);
    pdfi_oc_init(ctx);

    code = pdfi_gsave(ctx);
    if (code < 0)
        goto exit2;

    /* Figure out if pdf14 device is needed.
     * This can be either for normal transparency deviceN, or because we are using
     * Overprint=/simulate for other devices
     */
    if (ctx->page.has_transparency) {
        need_pdf14 = true;
        if (ctx->page.simulate_op)
            trans_depth = -1;
    } else {
        /* This is the case where we are simulating overprint without transparency */
        if (ctx->page.simulate_op) {
            need_pdf14 = true;
            trans_depth = -1;
        }
    }
    if (need_pdf14) {
        /* We don't retain the PDF14 device */
        code = gs_push_pdf14trans_device(ctx->pgs, false, false, trans_depth, ctx->page.num_spots);
        if (code >= 0) {
            if (ctx->page.has_transparency && page_group_known) {
                code = pdfi_trans_begin_page_group(ctx, page_dict, group_dict);
                /* If setting the page group failed for some reason, abandon the page group,
                 *  but continue with the page
                 */
                if (code < 0)
                    page_group_known = false;
            }
        } else {
            /* Couldn't push the transparency compositor.
             * This is probably fatal, but attempt to recover by abandoning transparency
             */
            ctx->page.has_transparency = false;
            need_pdf14 = false;
        }
    }

    /* Init a base_pgs graphics state for Patterns
     * (this has to be after transparency device pushed, if applicable)
     */
    pdfi_set_DefaultQState(ctx, ctx->pgs);

    /* Render one page (including annotations) */
    if (!ctx->args.QUIET)
        outprintf(ctx->memory, "Page %"PRId64"\n", page_num + 1);

    code = pdfi_process_one_page(ctx, page_dict);

    if (need_pdf14 && ctx->page.has_transparency && page_group_known) {
        code1 = pdfi_trans_end_group(ctx);
    }

    if (need_pdf14) {
        if (code1 < 0) {
            (void)gs_abort_pdf14trans_device(ctx->pgs);
            code = code1;
            goto exit1;
        }

        code = gs_pop_pdf14trans_device(ctx->pgs, false);
        if (code < 0) {
            goto exit1;
        }
    }

exit1:
    pdfi_free_DefaultQState(ctx);
    pdfi_grestore(ctx);

exit2:
    pdfi_countdown(ctx->page.CurrentPageDict);
    ctx->page.CurrentPageDict = NULL;

exit3:
    pdfi_countdown(page_dict);
    pdfi_countdown(group_dict);

    release_page_DefaultSpaces(ctx);

    /* Flush any pattern tiles. We don't want to (potentially) return to PostScript
     * with any pattern tiles referencing our objects, in case the garbager runs.
     */
    gx_pattern_cache_winnow(gstate_pattern_cache(ctx->pgs), pdfi_pattern_purge_all_proc, NULL);
    /* We could be smarter, but for now.. purge for each page */
    pdfi_purge_cache_resource_font(ctx);

    if (code == 0 || (!ctx->args.pdfstoponerror && code != gs_error_pdf_stackoverflow))
        if (!page_dict_error && ctx->finish_page != NULL)
            code = ctx->finish_page(ctx);
    return code;
}
