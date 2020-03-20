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

/* Graphics state operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_gstate.h"
#include "pdf_stack.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_func.h"
#include "pdf_file.h"
#include "pdf_misc.h"
#include "pdf_loop_detect.h"
#include "pdf_image.h"
#include "pdf_pattern.h"
#include "pdf_font.h"
#include "pdf_pattern.h"
#include "pdf_trans.h"

#include "gsmatrix.h"
#include "gslparam.h"
#include "gstparam.h"

#include "gxdht.h"
#include "gxht.h"
#include "gzht.h"
#include "gsht.h"

static const char *blend_mode_names[] = {
    GS_BLEND_MODE_NAMES, 0
};

int pdfi_get_blend_mode(pdf_context *ctx, pdf_name *name, gs_blend_mode_t *mode)
{
    const char **p;

    for (p = blend_mode_names; *p; ++p) {
        if (pdfi_name_is(name, *p)) {
            *mode = p - blend_mode_names;
            return 0;
        }
    }
    return -1;
}

void pdfi_gstate_smask_install(pdfi_int_gstate *igs, gs_memory_t *memory, pdf_dict *SMask, gs_gstate *gstate)
{
    void *client_data_save;

    if (!SMask)
        return;
    igs->memory = memory;
    igs->SMask = SMask;
    pdfi_countup(SMask);
    client_data_save = gstate->client_data;
    gstate->client_data = NULL;
    igs->GroupGState = gs_gstate_copy(gstate, memory);
    gstate->client_data = client_data_save;
}

void pdfi_gstate_smask_free(pdfi_int_gstate *igs)
{
    if (!igs->SMask)
        return;
    pdfi_countdown(igs->SMask);
    igs->SMask = NULL;
    if (igs->GroupGState)
        gs_gstate_free(igs->GroupGState);
    igs->GroupGState = NULL;
}


/* Allocate the interpreter's part of a graphics state. */
static void *
pdfi_gstate_alloc_cb(gs_memory_t * mem)
{
    pdfi_int_gstate *igs;

    igs = (pdfi_int_gstate *)gs_alloc_bytes(mem, sizeof(pdfi_int_gstate), "pdfi_gstate_alloc");
    if (igs == NULL)
        return NULL;
    memset(igs, 0, sizeof(pdfi_int_gstate));
    return igs;
}

/* Copy the interpreter's part of a graphics state. */
static int
pdfi_gstate_copy_cb(void *to, const void *from)
{
    const pdfi_int_gstate *igs_from = (const pdfi_int_gstate *)from;
    pdfi_int_gstate *igs_to = (pdfi_int_gstate *)to;

    /* Need to free destination contents before overwriting.
     *  On grestore, they might be non-empty.
     */
    pdfi_gstate_smask_free(igs_to);
    *(pdfi_int_gstate *) igs_to = *igs_from;
    pdfi_gstate_smask_install(igs_to, igs_from->memory, igs_from->SMask, igs_from->GroupGState);
    return 0;
}

/* Free the interpreter's part of a graphics state. */
static void
pdfi_gstate_free_cb(void *old, gs_memory_t * mem)
{
    pdfi_int_gstate *igs = (pdfi_int_gstate *)old;
    if (old == NULL)
        return;
    pdfi_gstate_smask_free(igs);
    gs_free_object(mem, igs, "pdfi_gstate_free");
}

static const gs_gstate_client_procs pdfi_gstate_procs = {
    pdfi_gstate_alloc_cb,
    pdfi_gstate_copy_cb,
    pdfi_gstate_free_cb,
    NULL, /* copy_for */
};

int
pdfi_gstate_set_client(pdf_context *ctx)
{
    pdfi_int_gstate *igs;

    igs = pdfi_gstate_alloc_cb(ctx->memory);
    gs_gstate_set_client(ctx->pgs, igs, &pdfi_gstate_procs, true /* TODO: client_has_pattern_streams ? */);
    return 0;
}

int pdfi_concat(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[6];
    gs_matrix m;

    if (pdfi_count_stack(ctx) < 6) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    if (ctx->TextBlockDepth != 0)
        ctx->pdf_warnings |= W_PDF_OPINVALIDINTEXT;

    for (i=0;i < 6;i++){
        num = (pdf_num *)ctx->stack_top[i - 6];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                pdfi_pop(ctx, 6);
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else
                    return 0;
            }
            else
                Values[i] = num->value.d;
        } else {
            Values[i] = (double)num->value.i;
        }
    }
    m.xx = (float)Values[0];
    m.xy = (float)Values[1];
    m.yx = (float)Values[2];
    m.yy = (float)Values[3];
    m.tx = (float)Values[4];
    m.ty = (float)Values[5];
    code = gs_concat(ctx->pgs, (const gs_matrix *)&m);
    pdfi_pop(ctx, 6);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_op_q(pdf_context *ctx)
{
    int code;

    dbgmprintf(ctx->memory, "(doing q)\n"); /* TODO: Spammy, delete me at some point */
    code = pdfi_gsave(ctx);

    if (code < 0 && ctx->pdfstoponerror)
        return code;
    else {
        if (ctx->page_has_transparency)
            return gs_push_transparency_state(ctx->pgs);
    }
    return 0;
}

int pdfi_op_Q(pdf_context *ctx)
{
    int code = 0;

    dbgmprintf(ctx->memory, "(doing Q)\n"); /* TODO: Spammy, delete me at some point */
    if (ctx->pgs->level <= ctx->current_stream_save.gsave_level) {
        /* We don't throw an error here, we just ignore it and continue */
        ctx->pdf_warnings |= W_PDF_TOOMANYQ;
        dbgmprintf(ctx->memory, "WARNING: Too many q/Q (too many Q's) -- ignoring Q\n");
        return 0;
    }
    if (ctx->page_has_transparency)
        code = gs_pop_transparency_state(ctx->pgs, false);

    if (code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return pdfi_grestore(ctx);
    return 0;
}

int pdfi_gsave(pdf_context *ctx)
{
    int code;

    code = gs_gsave(ctx->pgs);

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else {
        pdfi_countup_current_font(ctx);
        return 0;
    }
}

int pdfi_grestore(pdf_context *ctx)
{
    int code;
    pdf_font *font = NULL, *font1 = NULL;

    /* Make sure we have encountered as many gsave operations in this
     * stream as grestores. If not, log an error
     */
    if (ctx->pgs->level > ctx->current_stream_save.gsave_level) {
        font = pdfi_get_current_pdf_font(ctx);

        if (ctx->pgs->color[0].color_space->type->index == gs_color_space_index_Pattern && ctx->pgs->color[0].color_space->rc.ref_count == 1)
            (void)pdfi_pattern_cleanup(ctx->pgs->color[0].ccolor);
        if (ctx->pgs->color[1].color_space->type->index == gs_color_space_index_Pattern && ctx->pgs->color[1].color_space->rc.ref_count == 1)
            (void)pdfi_pattern_cleanup(ctx->pgs->color[1].ccolor);

        code = gs_grestore(ctx->pgs);

        font1 = pdfi_get_current_pdf_font(ctx);
        if (font != NULL && (font != font1 || ((pdf_obj *)font)->refcnt > 1)) {
            /* TODO: This countdown might have been causing memory corruption (dangling pointer)
             * but seems to be okay now.  Maybe was fixed by other memory issue. 8-28-19
             * If you come upon this comment in the future and it all seems fine, feel free to
             * clean this up... (delete comment, remove the commented out warning message, etc)
             */
#if REFCNT_DEBUG
            dbgmprintf2(ctx->memory, "pdfi_grestore() counting down font UID %ld, refcnt %d\n",
                        font->UID, font->refcnt);
#endif
            //            dbgmprintf(ctx->memory, "WARNING pdfi_grestore() DISABLED pdfi_countdown (FIXME!)\n");
            pdfi_countdown(font);
        }

        if(code < 0 && ctx->pdfstoponerror)
            return code;
        else
            return 0;
    } else {
        /* We don't throw an error here, we just ignore it and continue */
        ctx->pdf_warnings |= W_PDF_TOOMANYQ;
    }
    return 0;
}

int pdfi_setlinewidth(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
    double d1;

    if (pdfi_count_stack(ctx) < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            pdfi_pop(ctx, 1);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else
                return 0;
        }
    }
    code = gs_setlinewidth(ctx->pgs, d1);
    pdfi_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setlinejoin(pdf_context *ctx)
{
    int code;
    pdf_num *n1;

    if (pdfi_count_stack(ctx) < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinejoin(ctx->pgs, n1->value.i);
    } else {
        pdfi_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else
            return 0;
    }
    pdfi_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setlinecap(pdf_context *ctx)
{
    int code;
    pdf_num *n1;

    if (pdfi_count_stack(ctx) < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinecap(ctx->pgs, n1->value.i);
    } else {
        pdfi_pop(ctx, 1);
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else
            return 0;
    }
    pdfi_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setflat(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
    double d1;

    if (pdfi_count_stack(ctx) < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            pdfi_pop(ctx, 1);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else
                return 0;
        }
    }
    /* PDF spec says the value is 1-100, with 0 meaning "use the default"
     * But gs code (and now our code) forces the value to be <= 1
     * This matches what Adobe and evince seem to do (see Bug 555657).
     * Apparently mupdf implements this as a no-op, which is essentially
     * what this does now.
     */
    if (d1 > 1.0)
        d1 = 1.0;
    code = gs_setflat(ctx->pgs, d1);
    pdfi_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setdash_impl(pdf_context *ctx, pdf_array *a, double phase_d)
{
    float *dash_array;
    double temp;
    int i, code;

    dash_array = (float *)gs_alloc_bytes(ctx->memory, pdfi_array_size(a) * sizeof (float),
                                         "temporary float array for setdash");
    if (dash_array == NULL)
        return_error(gs_error_VMerror);

    for (i=0;i < pdfi_array_size(a);i++){
        code = pdfi_array_get_number(ctx, a, (uint64_t)i, &temp);
        if (code < 0) {
            if (ctx->pdfstoponerror) {
                gs_free_object(ctx->memory, dash_array, "error in setdash");
                return code;
            }
            else {
                gs_free_object(ctx->memory, dash_array, "error in setdash");
                return 0;
            }
        }
        dash_array[i] = (float)temp;
    }
    code = gs_setdash(ctx->pgs, dash_array, pdfi_array_size(a), phase_d);
    gs_free_object(ctx->memory, dash_array, "error in setdash");
    return code;
}
int pdfi_setdash(pdf_context *ctx)
{
    pdf_num *phase;
    pdf_array *a;
    double phase_d;
    int code;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    phase = (pdf_num *)ctx->stack_top[-1];
    if (phase->type == PDF_INT){
        phase_d = (double)phase->value.i;
    } else{
        if (phase->type == PDF_REAL) {
            phase_d = phase->value.d;
        } else {
            pdfi_pop(ctx, 2);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else
                return 0;
        }
    }

    a = (pdf_array *)ctx->stack_top[-2];
    if (a->type != PDF_ARRAY) {
        pdfi_pop(ctx, 2);
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else
            return 0;
    }

    code = pdfi_setdash_impl(ctx, a, phase_d);
    pdfi_pop(ctx, 2);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setmiterlimit(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
    double d1;

    if (pdfi_count_stack(ctx) < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            pdfi_pop(ctx, 1);
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else
                return 0;
        }
    }
    code = gs_setmiterlimit(ctx->pgs, d1);
    pdfi_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

static int GS_LW(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    double d1;
    int code;

    code = pdfi_dict_get_number(ctx, GS, "LW", &d1);
    if (code < 0)
        return code;

    code = gs_setlinewidth(ctx->pgs, d1);
    return code;
}

static int GS_LC(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int64_t i;
    int code;

    code = pdfi_dict_get_int(ctx, GS, "LC", &i);
    if (code < 0)
        return code;

    code = gs_setlinecap(ctx->pgs, i);
    return code;
}

static int GS_LJ(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int64_t i;
    int code;

    code = pdfi_dict_get_int(ctx, GS, "LJ", &i);
    if (code < 0)
        return code;

    code = gs_setlinejoin(ctx->pgs, i);
    return code;
}

static int GS_ML(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    double d1;

    code = pdfi_dict_get_number(ctx, GS, "ML", &d1);
    if (code < 0)
        return code;

    code = gs_setmiterlimit(ctx->pgs, d1);
    return code;
}

static int GS_D(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_array *a, *a1;
    double d;
    int code;

    code = pdfi_dict_get_type(ctx, GS, "D", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0)
        return code;

    code = pdfi_array_get_type(ctx, a, (int64_t)0, PDF_ARRAY, (pdf_obj **)&a1);
    if (code < 0) {
        pdfi_countdown(a);
        return code;
    }

    code = pdfi_array_get_number(ctx, a, (int64_t)1, &d);
    if (code < 0) {
        pdfi_countdown(a1);
        pdfi_countdown(a);
        return code;
    }

    code = pdfi_setdash_impl(ctx, a1, d);
    pdfi_countdown(a1);
    pdfi_countdown(a);
    return code;
}

static int GS_RI(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_name *n;
    int code;

    code = pdfi_dict_get_type(ctx, GS, "RI", PDF_NAME, (pdf_obj **)&n);
    if (code < 0)
        return code;

    code = pdfi_setrenderingintent(ctx, n);
    pdfi_countdown(n);
    return code;
}

static int GS_OP(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_bool *b;
    int code;

    code = pdfi_dict_get_type(ctx, GS, "OP", PDF_BOOL, (pdf_obj **)&b);
    if (code < 0)
        return code;

    gs_setstrokeoverprint(ctx->pgs, b->value);
    pdfi_countdown(b);
    return 0;
}

static int GS_op(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_bool *b;
    int code;

    code = pdfi_dict_get_type(ctx, GS, "op", PDF_BOOL, (pdf_obj **)&b);
    if (code < 0)
        return code;

    gs_setfilloverprint(ctx->pgs, b->value);
    pdfi_countdown(b);
    return 0;
}

static int GS_OPM(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int64_t i;
    int code;

    code = pdfi_dict_get_int(ctx, GS, "OPM", &i);
    if (code < 0)
        return code;

    code = gs_setoverprintmode(ctx->pgs, i);
    return code;
}

static int GS_Font(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dbgmprintf(ctx->memory, "ExtGState Font not yet implemented\n");
    return 0;
}

static int pdfi_set_blackgeneration(pdf_context *ctx, pdf_obj *obj, pdf_dict *stream_dict, pdf_dict *page_dict, bool is_BG)
{
    int code = 0, i;
    gs_function_t *pfn;

    if (obj->type == PDF_NAME) {
        if (pdfi_name_is((const pdf_name *)obj, "Identity")) {
            code = gs_setblackgeneration_remap(ctx->pgs, gs_identity_transfer, false);
            goto exit;
        } else {
            if (!is_BG && pdfi_name_is((const pdf_name *)obj, "Default")) {
                code = gs_setblackgeneration_remap(ctx->pgs, ctx->DefaultBG.proc, false);
                memcpy(ctx->pgs->black_generation->values, ctx->DefaultBG.values, transfer_map_size * sizeof(frac));
                goto exit;
            } else {
                code = gs_note_error(gs_error_rangecheck);
                goto exit;
            }
        }
    } else {
        if (obj->type != PDF_DICT)
            return_error(gs_error_typecheck);

        code = pdfi_build_function(ctx, &pfn, NULL, 1, (pdf_dict *)obj, page_dict);
        if (code < 0)
            return code;

        gs_setblackgeneration_remap(ctx->pgs, gs_mapped_transfer, false);
        for (i = 0; i < transfer_map_size; i++) {
            float v, f;

            f = (1.0f / transfer_map_size) * i;

            code = gs_function_evaluate(pfn, (const float *)&f, &v);
            if (code < 0) {
                pdfi_free_function(ctx, pfn);
                return code;
            }

            ctx->pgs->black_generation->values[i] =
                (v < 0.0 ? float2frac(0.0) :
                 v >= 1.0 ? frac_1 :
                 float2frac(v));
        }
        code = pdfi_free_function(ctx, pfn);
    }
exit:
    return code;
}

static int GS_BG(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *obj = NULL;

    /* If the dictionary also has a BG2, then we must use that */
    code = pdfi_dict_get(ctx, GS, "BG2", &obj);
    if (code == 0) {
        pdfi_countdown(obj);
        return 0;
    }

    code = pdfi_dict_get(ctx, GS, "BG", &obj);
    if (code < 0)
        return code;

    code = pdfi_set_blackgeneration(ctx, obj, stream_dict, page_dict, true);

    pdfi_countdown(obj);

    return code;
}

static int GS_BG2(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *obj = NULL;

    code = pdfi_dict_get(ctx, GS, "BG2", &obj);
    if (code < 0)
        return code;

    code = pdfi_set_blackgeneration(ctx, obj, stream_dict, page_dict, false);

    pdfi_countdown(obj);

    return code;
}

static int pdfi_set_undercolorremoval(pdf_context *ctx, pdf_obj *obj, pdf_dict *stream_dict, pdf_dict *page_dict, bool is_BG)
{
    int code = 0, i;
    gs_function_t *pfn;

    if (obj->type == PDF_NAME) {
        if (pdfi_name_is((const pdf_name *)obj, "Identity")) {
            code = gs_setundercolorremoval_remap(ctx->pgs, gs_identity_transfer, false);
            goto exit;
        } else {
            if (!is_BG && pdfi_name_is((const pdf_name *)obj, "Default")) {
                code = gs_setundercolorremoval_remap(ctx->pgs, ctx->DefaultUCR.proc, false);
                memcpy(ctx->pgs->undercolor_removal->values, ctx->DefaultUCR.values, transfer_map_size * sizeof(frac));
                goto exit;
            } else {
                code = gs_note_error(gs_error_rangecheck);
                goto exit;
            }
        }
    } else {
        if (obj->type != PDF_DICT)
            return_error(gs_error_typecheck);

        code = pdfi_build_function(ctx, &pfn, NULL, 1, (pdf_dict *)obj, page_dict);
        if (code < 0)
            return code;

        gs_setundercolorremoval_remap(ctx->pgs, gs_mapped_transfer, false);
        for (i = 0; i < transfer_map_size; i++) {
            float v, f;

            f = (1.0f / transfer_map_size) * i;

            code = gs_function_evaluate(pfn, (const float *)&f, &v);
            if (code < 0) {
                pdfi_free_function(ctx, pfn);
                return code;
            }

            ctx->pgs->undercolor_removal->values[i] =
                (v < 0.0 ? float2frac(0.0) :
                 v >= 1.0 ? frac_1 :
                 float2frac(v));
        }
        code = pdfi_free_function(ctx, pfn);
    }
exit:
    return code;
}

static int GS_UCR(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *obj = NULL;

    /* If the dictionary also has a UCR2, then we must use that and ignore the UCR */
    code = pdfi_dict_get(ctx, GS, "UCR2", &obj);
    if (code == 0) {
        pdfi_countdown(obj);
        return 0;
    }

    code = pdfi_dict_get(ctx, GS, "UCR", &obj);
    if (code < 0)
        return code;

    code = pdfi_set_undercolorremoval(ctx, obj, stream_dict, page_dict, true);

    pdfi_countdown(obj);

    return code;
}

static int GS_UCR2(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *obj = NULL;

    code = pdfi_dict_get(ctx, GS, "UCR2", &obj);
    if (code < 0)
        return code;

    code = pdfi_set_undercolorremoval(ctx, obj, stream_dict, page_dict, false);

    pdfi_countdown(obj);

    return code;
}

typedef enum {
    E_IDENTITY,
    E_DEFAULT,
    E_FUNCTION
} pdf_transfer_function_type_e;

/* We use this for both TR and TR2, is_TR is true if this is a TR, in which case we don't want
 * to permit /Default names for fucntions.
 */
static int pdfi_set_all_transfers(pdf_context *ctx, pdf_array *a, pdf_dict *page_dict, bool is_TR)
{
    int code = 0, i, j;
    pdf_obj *o = NULL;
    int proc_types[4];
    gs_mapping_proc map_procs[4];
    gs_function_t *pfn[4];

    memset(pfn, 0x00, 4 * sizeof(gs_function_t *));
    memset(map_procs, 0x00, 4 * sizeof(gs_mapping_proc *));

    /* Two passes, the first one is to find the appropriate transfer procedures
     * and do the majorty of the error checking;
     */
    for (i = 0; i < 4; i++) {
        code = pdfi_array_get(ctx, a, (uint64_t)i, &o);
        if (code < 0)
            goto exit;
        if (o->type == PDF_NAME) {
            if (pdfi_name_is((const pdf_name *)o, "Identity")) {
                proc_types[i] = E_IDENTITY;
                map_procs[i] = gs_identity_transfer;
            } else {
                if (!is_TR && pdfi_name_is((const pdf_name *)o, "Default")) {
                    proc_types[i] = E_DEFAULT;
                    map_procs[i] = ctx->DefaultTransfers[i].proc;
                } else {
                    pdfi_countdown(o);
                    code = gs_note_error(gs_error_typecheck);
                    goto exit;
                }
            }
        } else {
            if (o->type == PDF_DICT) {
                proc_types[i] = E_FUNCTION;
                map_procs[i] = gs_mapped_transfer;
                code = pdfi_build_function(ctx, &pfn[i], NULL, 1, (pdf_dict *)o, page_dict);
                if (code < 0) {
                    pdfi_countdown(o);
                    goto exit;
                }
            } else {
                pdfi_countdown(o);
                code = gs_note_error(gs_error_typecheck);
                goto exit;
            }
        }
        pdfi_countdown(o);
    }
    code = gs_setcolortransfer_remap(ctx->pgs, map_procs[0], map_procs[1], map_procs[2], map_procs[3], false);
    if (code < 0)
        goto exit;

    /* Second pass is to evaluate and set the transfer maps */
    for (j = 0; j < 4; j++) {
        if (proc_types[j] == E_DEFAULT) {
            switch(j) {
                case 0:
                    memcpy(ctx->pgs->set_transfer.red->values, ctx->DefaultTransfers[j].values, transfer_map_size * sizeof(frac));
                    break;
                case 1:
                    memcpy(ctx->pgs->set_transfer.green->values, ctx->DefaultTransfers[j].values, transfer_map_size * sizeof(frac));
                    break;
                case 2:
                    memcpy(ctx->pgs->set_transfer.blue->values, ctx->DefaultTransfers[j].values, transfer_map_size * sizeof(frac));
                    break;
                case 3:
                    memcpy(ctx->pgs->set_transfer.gray->values, ctx->DefaultTransfers[j].values, transfer_map_size * sizeof(frac));
                    break;
            }
        }
        if (proc_types[j] == E_FUNCTION) {
            for (i = 0; i < transfer_map_size; i++) {
                float v, f;
                frac value;

                f = (1.0f / transfer_map_size) * i;

                code = gs_function_evaluate(pfn[j], (const float *)&f, &v);
                if (code < 0)
                    goto exit;

                value =
                    (v < 0.0 ? float2frac(0.0) :
                     v >= 1.0 ? frac_1 :
                     float2frac(v));
                switch(j) {
                    case 0:
                        ctx->pgs->set_transfer.red->values[i] = value;
                        break;
                    case 1:
                        ctx->pgs->set_transfer.green->values[i] = value;
                        break;
                    case 2:
                        ctx->pgs->set_transfer.blue->values[i] = value;
                        break;
                    case 3:
                        ctx->pgs->set_transfer.gray->values[i] = value;
                        break;
                }
            }
        }
    }
 exit:
//    (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    for (i = 0; i < 4; i++) {
        pdfi_free_function(ctx, pfn[i]);
    }
    return code;
}

static int pdfi_set_gray_transfer(pdf_context *ctx, pdf_dict *d, pdf_dict *page_dict)
{
    int code = 0, i;
    gs_function_t *pfn;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    code = pdfi_build_function(ctx, &pfn, NULL, 1, d, page_dict);
    if (code < 0)
        return code;

    gs_settransfer_remap(ctx->pgs, gs_mapped_transfer, false);
    for (i = 0; i < transfer_map_size; i++) {
        float v, f;

        f = (1.0f / transfer_map_size) * i;

        code = gs_function_evaluate(pfn, (const float *)&f, &v);
        if (code < 0) {
            pdfi_free_function(ctx, pfn);
            return code;
        }

        ctx->pgs->set_transfer.gray->values[i] =
            (v < 0.0 ? float2frac(0.0) :
             v >= 1.0 ? frac_1 :
             float2frac(v));
    }
    return pdfi_free_function(ctx, pfn);
}

static int pdfi_set_transfer(pdf_context *ctx, pdf_obj *obj, pdf_dict *stream_dict, pdf_dict *page_dict, bool is_TR)
{
    int code = 0;

    if (obj->type == PDF_NAME) {
        if (pdfi_name_is((const pdf_name *)obj, "Identity")) {
            code = gs_settransfer_remap(ctx->pgs, gs_identity_transfer, false);
            goto exit;
        } else {
            if (!is_TR && pdfi_name_is((const pdf_name *)obj, "Default")) {
                code = gs_settransfer_remap(ctx->pgs, ctx->DefaultTransfers[3].proc, false);
                memcpy(ctx->pgs->set_transfer.gray->values, ctx->DefaultTransfers[3].values, transfer_map_size * sizeof(frac));
                goto exit;
            } else {
                code = gs_note_error(gs_error_rangecheck);
                goto exit;
            }
        }
    }

    if (obj->type == PDF_ARRAY) {
        if (pdfi_array_size((pdf_array *)obj) != 4) {
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        } else {
            code = pdfi_set_all_transfers(ctx, (pdf_array *)obj, page_dict, false);
        }
    } else
        code = pdfi_set_gray_transfer(ctx, (pdf_dict *)obj, page_dict);

exit:
    return code;
}

static int GS_TR(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *obj = NULL;

    code = pdfi_dict_get(ctx, GS, "TR", &obj);
    if (code < 0)
        return code;

    code = pdfi_set_transfer(ctx, obj, stream_dict, page_dict, true);

    pdfi_countdown(obj);

    return code;
}

static int GS_TR2(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *obj = NULL;
    code = pdfi_dict_get(ctx, GS, "TR2", &obj);
    if (code < 0)
        return code;

    code = pdfi_set_transfer(ctx, obj, stream_dict, page_dict, false);

    pdfi_countdown(obj);

    return code;
}

static const char *spot_functions[] = {
    "{dup mul exch dup mul add 1 exch sub}",        /* SimpleDot */
    "{dup mul exch dup mul add 1 sub}",             /* InvertedSimpleDot */
    "{360 mul sin 2 div exch 360 mul sin 2 div add",/* DoubleDot */
    "360 mul sin 2 div exch 360 mul\
     sin 2 div add neg}",                           /* InvertedDoubleDot */
    "{180 mul cos exch 180 mul cos add 2 div}",     /* CosineDot */
    "{360 mul sin 2 div exch 2 div \
     360 mul sin 2 div add}",                       /* Double */
    "{360 mul sin 2 div exch 2 div \
     360 mul sin 2 div add neg}",                   /* InvertedDouble */
    "{exch pop abs neg}",                           /* Line */
    "{pop}",                                        /* LineX */
    "{exch pop}",                                   /* LineY */
    "{abs exch abs 2 copy add 1.0 le\
     {dup mul exch dup mul add 1 exch sub}\
     {1 sub dup mul exch 1 sub dup mul add 1 sub}\
     ifelse}",                                      /* Round */
    "{abs exch abs 2 copy 3 mul exch 4 mul\
     add 3 sub dup 0 lt\
     {pop dup mul exch 0.75 div dup mul add\
     4 div 1 exch sub}\
     {dup 1 gt\
     {pop 1 exch sub dup mul exch 1 exch sub\
     0.75 div dup mul add 4 div 1 sub}\
     {0.5 exch sub exch pop exch pop}}\
     ifelse}ifelse}}",                              /* Ellipse */
    "{dup mul 0.9 mul exch dup mul add 1 exch sub}",/* EllipseA */
    "{dup mul 0.9 mul exch dup mul add 1 sub}",     /* InvertedEllipseA */
    "{dup 5 mul 8 div mul exch dup mul exch add\
     sqrt 1 exch sub}",                             /* EllipseB */
    "{dup mul exch dup mul 0.9 mul add 1 exch sub}",/* EllipseC */
    "{dup mul exch dup mul 0.9 mul add 1 sub}",     /* InvertedEllipseC */
    "{abs exch abs 2 copy lt {exch} if pop neg}",   /* Square */
    "{abs exch abs 2 copy gt {exch} if pop neg}",   /* Cross */
    "{abs exch abs 0.9 mul add 2 div}",             /* Rhomboid */
    "{abs exch abs 2 copy add 0.75 le\
     {dup mul exch dup mul add 1 exch sub}\
     {1 copy add 1.23 le\
     {0.85 mul add 1 exch sub}\
     {1 sub dup mul exch 1 sub dup mul add 1 sub}\
     ifelse} ifelse}"                               /* Diamond */
};

static const char *spot_table[] = {
    "SimpleDot",
    "InvertedSimpleDot",
    "DoubleDot",
    "InvertedDoubleDot",
    "CosineDot",
    "Double",
    "InvertedDouble",
    "Line",
    "LineX",
    "LineY",
    "Round",
    "Ellipse",
    "EllipseA",
    "InvertedEllipseA",
    "EllipseB",
    "EllipseC",
    "InvertedEllipseC",
    "Square",
    "Cross",
    "Rhomboid",
    "Diamond"
};

#if 0
static int pdfi_set_halftone(pdf_context *ctx, pdf_obj *obj, pdf_dict *stream_dict, pdf_dict *page_dict, bool is_TR)
{
    int code = 0;

    if (obj->type == PDF_NAME) {
            if (pdfi_name_is((const pdf_name *)obj, "Default")) {
                goto exit;
            } else {
                code = gs_note_error(gs_error_rangecheck);
                goto exit;
            }
    }

    if (obj->type == PDF_ARRAY) {
        if (pdfi_array_size((pdf_array *)obj) != 4) {
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        } else {
            code = pdfi_set_all_transfers(ctx, (pdf_array *)obj, page_dict, false);
        }
    } else
        code = pdfi_set_gray_transfer(ctx, (pdf_dict *)obj, page_dict);

exit:
    return code;
}

int do_screen(pdf_context *ctx, pdf_dict *halftone_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_screen_enum *penum = NULL;
    gs_point pt;
    int code;
    gs_screen_halftone screen;
    gx_ht_order order;
    pdf_obj *obj = NULL;
    double f, a;
    float values[2] = {0, 0}, domain[2] = {-1, 1}, out;
    gs_function_t *pfn = NULL;

    memset(&screen, 0x00, sizeof(gs_screen_halftone));

    code = pdfi_dict_get_number(ctx, halftone_dict, "Frequency", &f);
    if (code < 0)
        return code;

    code = pdfi_dict_get_number(ctx, halftone_dict, "Angle", &a);
    if (code < 0)
        return code;

    code = pdfi_dict_get(ctx, halftone_dict, "SpotFunction", &obj);
    if (code < 0)
        return code;

    if (obj->type == PDF_NAME) {
        if (pdfi_name_is((pdf_name *)obj, "Default")) {
            return 0;
        } else {
            int i;

            for (i = 0; i < sizeof(spot_table); i++){
                if (pdfi_name_is((pdf_name *)obj, spot_table[i]))
                    break;
            }
            if (i > sizeof(spot_table))
                return gs_note_error(gs_error_rangecheck);
            code = pdfi_build_halftone_function(ctx, &pfn, (byte *)spot_functions[i], strlen(spot_functions[i]));
            if (code < 0)
                goto error;
        }
    } else {
        if (obj->type == PDF_DICT) {
            code = pdfi_build_function(ctx, &pfn, (const float *)domain, 2, (pdf_dict *)obj, page_dict);
            if (code < 0)
                goto error;
        } else
            return gs_note_error(gs_error_typecheck);
    }

    screen.frequency = f;
    screen.angle = a;

    code = gs_screen_order_init_memory(&order, ctx->pgs, &screen,
                                       gs_currentaccuratescreens(ctx->memory), ctx->memory);
    if (code < 0)
        goto error;

    penum = gs_screen_enum_alloc(ctx->memory, "HT");
    if (penum == 0) {
        code = gs_error_VMerror;
        goto error;
    }

    code = gs_screen_enum_init_memory(penum, &order, ctx->pgs, &screen, ctx->memory);
    if (code < 0)
        goto error;

    do {
        /* Generate x and y, the parameteric variables */
        code = gs_screen_currentpoint(penum, &pt);
        if (code < 0)
            goto error;

        if (code == 1)
            break;

        /* Process sample */
        values[0] = pt.x, values[1] = pt.y;
        code = gs_function_evaluate(pfn, (const float *)&values, &out);
        if (code < 0)
            goto error;

        /* Store the sample */
        code = gs_screen_next(penum, out);
        if (code < 0)
            goto error;

    } while (1);
    code = 0;

    gs_screen_install(penum);

error:
    pdfi_countdown(obj);
    pdfi_free_function(ctx, pfn);
    gs_free_object(ctx->memory, penum, "HT");
    return code;
}
#endif

/* Dummy spot function */
static float
pdfi_spot1_dummy(double x, double y)
{
    return (x + y) / 2;
}

static int build_type1_halftone(pdf_context *ctx, pdf_dict *halftone_dict, pdf_dict *page_dict, gx_device_halftone *pdht, gs_halftone_component *phtc, char *name, int len)
{
    int code;
    pdf_obj *obj = NULL;
    double f, a;
    float values[2] = {0, 0}, domain[4] = {-1, 1, -1, 1}, out;
    gs_function_t *pfn = NULL;
    gx_ht_order *order = NULL;
    gs_screen_enum *penum = NULL;
    gs_point pt;

    code = pdfi_dict_get_number(ctx, halftone_dict, "Frequency", &f);
    if (code < 0)
        return code;

    code = pdfi_dict_get_number(ctx, halftone_dict, "Angle", &a);
    if (code < 0)
        return code;

    code = pdfi_dict_get(ctx, halftone_dict, "SpotFunction", &obj);
    if (code < 0)
        return code;

    order = (gx_ht_order *)gs_alloc_bytes(ctx->memory, sizeof(gx_ht_order), "build_type1_halftone");
    if (order == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    if (obj->type == PDF_NAME) {
        if (pdfi_name_is((pdf_name *)obj, "Default")) {
            code = 0;
            goto error;
        } else {
            int i;

            for (i = 0; i < sizeof(spot_table); i++){
                if (pdfi_name_is((pdf_name *)obj, spot_table[i]))
                    break;
            }
            if (i > sizeof(spot_table))
                return gs_note_error(gs_error_rangecheck);
            code = pdfi_build_halftone_function(ctx, &pfn, (byte *)spot_functions[i], strlen(spot_functions[i]));
            if (code < 0)
                goto error;
        }
    } else {
        if (obj->type == PDF_DICT) {
            code = pdfi_build_function(ctx, &pfn, (const float *)domain, 2, (pdf_dict *)obj, page_dict);
            if (code < 0)
                goto error;
        } else {
            code = gs_note_error(gs_error_typecheck);
            goto error;
        }
    }

    phtc->params.spot.screen.frequency = f;
    phtc->params.spot.screen.angle = a;
    phtc->params.spot.screen.spot_function = pdfi_spot1_dummy;
    phtc->params.spot.transfer = (code > 0 ? (gs_mapping_proc) 0 : gs_mapped_transfer);
    phtc->params.spot.transfer_closure.proc = 0;
    phtc->params.spot.transfer_closure.data = 0;
    phtc->type = ht_type_spot;
    code = pdfi_get_name_index(ctx, name, len, (unsigned int *)&phtc->cname);
    if (code < 0)
        goto error;

    phtc->comp_number = gs_cname_to_colorant_number(ctx->pgs, (byte *)name, len, 1);

    code = gs_screen_order_init_memory(order, ctx->pgs, &phtc->params.spot.screen,
                                       gs_currentaccuratescreens(ctx->memory), ctx->memory);
    if (code < 0)
        goto error;

    penum = gs_screen_enum_alloc(ctx->memory, "build_type1_halftone");
    if (penum == 0) {
        code = gs_error_VMerror;
        goto error;
    }

    code = gs_screen_enum_init_memory(penum, order, ctx->pgs, &phtc->params.spot.screen, ctx->memory);
    if (code < 0)
        goto error;

    do {
        /* Generate x and y, the parameteric variables */
        code = gs_screen_currentpoint(penum, &pt);
        if (code < 0)
            goto error;

        if (code == 1)
            break;

        /* Process sample */
        values[0] = pt.x, values[1] = pt.y;
        code = gs_function_evaluate(pfn, (const float *)&values, &out);
        if (code < 0)
            goto error;

        /* Store the sample */
        code = gs_screen_next(penum, out);
        if (code < 0)
            goto error;

    } while (1);
    code = 0;
    pdht->order = penum->order;

error:
    pdfi_countdown(obj);
    pdfi_free_function(ctx, pfn);
    gs_free_object(ctx->memory, order, "build_type1_halftone");
    gs_free_object(ctx->memory, penum, "build_type1_halftone");
    return code;
}

static int build_type6_halftone(pdf_context *ctx, pdf_dict *halftone_dict, pdf_dict *page_dict, gx_device_halftone *pdht, gs_halftone_component *phtc, char *name, int len)
{
    int code;
    int64_t w, h;
    gs_threshold2_halftone *ptp = &phtc->params.threshold2;

    ptp->thresholds.data = NULL;
    ptp->thresholds.size = 0;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Width", &w);
    if (code < 0)
        return code;
    ptp->width = w;
    ptp->width2 = 0;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Height", &h);
    if (code < 0)
        return code;
    ptp->height = h;
    ptp->height2 = 0;

    ptp->bytes_per_sample = 1;
    ptp->transfer = 0;
    ptp->transfer_closure.proc = 0;
    ptp->transfer_closure.data = 0;

    code = pdfi_get_name_index(ctx, name, len, (unsigned int *)&phtc->cname);
    if (code < 0)
        goto error;

    phtc->comp_number = gs_cname_to_colorant_number(ctx->pgs, (byte *)name, len, 1);

    code = pdfi_stream_to_buffer(ctx, halftone_dict, (byte **)&ptp->thresholds.data, (int64_t *)&ptp->thresholds.size);
    if (code < 0)
        goto error;

    phtc->type = ht_type_threshold2;
    return code;

error:
    gs_free_object(ctx->memory, (byte *)ptp->thresholds.data, "build_type6_halftone");
    return code;
}

static int build_type10_halftone(pdf_context *ctx, pdf_dict *halftone_dict, pdf_dict *page_dict, gx_device_halftone *pdht, gs_halftone_component *phtc, char *name, int len)
{
    int code;
    int64_t w, h;
    gs_threshold2_halftone *ptp = &phtc->params.threshold2;

    ptp->thresholds.data = NULL;
    ptp->thresholds.size = 0;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Width", &w);
    if (code < 0)
        return code;
    ptp->width = w;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Height", &h);
    if (code < 0)
        return code;
    ptp->height = h;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Xsquare", &w);
    if (code < 0)
        return code;
    ptp->width2 = w;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Ysquare", &h);
    if (code < 0)
        return code;
    ptp->height2 = h;

    ptp->bytes_per_sample = 1;
    ptp->transfer = 0;
    ptp->transfer_closure.proc = 0;
    ptp->transfer_closure.data = 0;

    code = pdfi_get_name_index(ctx, name, len, (unsigned int *)&phtc->cname);
    if (code < 0)
        goto error;

    phtc->comp_number = gs_cname_to_colorant_number(ctx->pgs, (byte *)name, len, 1);

    code = pdfi_stream_to_buffer(ctx, halftone_dict, (byte **)&ptp->thresholds.data, (int64_t *)&ptp->thresholds.size);
    if (code < 0)
        goto error;

    phtc->type = ht_type_threshold2;
    return code;

error:
    gs_free_object(ctx->memory, (byte *)ptp->thresholds.data, "build_type6_halftone");
    return code;
}

static int build_type16_halftone(pdf_context *ctx, pdf_dict *halftone_dict, pdf_dict *page_dict, gx_device_halftone *pdht, gs_halftone_component *phtc, char *name, int len)
{
    int code;
    int64_t w, h;
    gs_threshold2_halftone *ptp = &phtc->params.threshold2;

    ptp->thresholds.data = NULL;
    ptp->thresholds.size = 0;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Width", &w);
    if (code < 0)
        return code;
    ptp->width = w;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Height", &h);
    if (code < 0)
        return code;
    ptp->height = h;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Width2", &w);
    if (code < 0)
        return code;
    ptp->width2 = w;

    code = pdfi_dict_get_int(ctx, halftone_dict, "Height2", &h);
    if (code < 0)
        return code;
    ptp->height2 = h;

    ptp->bytes_per_sample = 2;
    ptp->transfer = 0;
    ptp->transfer_closure.proc = 0;
    ptp->transfer_closure.data = 0;

    code = pdfi_get_name_index(ctx, name, len, (unsigned int *)&phtc->cname);
    if (code < 0)
        goto error;

    phtc->comp_number = gs_cname_to_colorant_number(ctx->pgs, (byte *)name, len, 1);

    code = pdfi_stream_to_buffer(ctx, halftone_dict, (byte **)&ptp->thresholds.data, (int64_t *)&ptp->thresholds.size);
    if (code < 0)
        goto error;

    phtc->type = ht_type_threshold2;
    return code;

error:
    gs_free_object(ctx->memory, (byte *)ptp->thresholds.data, "build_type6_halftone");
    return code;
}

static void pdfi_free_halftone(gs_memory_t *memory, void *data, client_name_t cname)
{
    int i=0;
    gs_halftone *pht = (gs_halftone *)data;
    gs_halftone_component comp;

    for (i=0;i< pht->params.multiple.num_comp;i++) {
        comp = pht->params.multiple.components[i];
        switch(comp.type) {
            case ht_type_threshold:
                if (comp.params.threshold.thresholds.data != NULL)
                    gs_free_object(memory, (byte *)comp.params.threshold.thresholds.data, "pdfi_free_halftone - thresholds");
                break;
            case ht_type_threshold2:
                if (comp.params.threshold2.thresholds.data != NULL)
                    gs_free_object(memory, (byte *)comp.params.threshold2.thresholds.data, "pdfi_free_halftone - thresholds");
                break;
            default:
                break;
        }
    }
    gs_free_object(memory, pht->params.multiple.components, "pdfi_free_halftone");
}

static int pdfi_do_halftone(pdf_context *ctx, pdf_dict *halftone_dict, pdf_dict *page_dict)
{
    int code, str_len;
    char *str = NULL;
    int64_t type;
    gs_halftone *pht = NULL;
    gx_device_halftone *pdht = NULL;
    gs_halftone_component *phtc = NULL;
    pdf_obj *Key = NULL, *Value = NULL;
    int index = 0, ix = 0, NumComponents = 0;
    bool known = false;

    code = pdfi_dict_get_int(ctx, halftone_dict, "HalftoneType", &type);
    if (code < 0)
        return code;

    pht = (gs_halftone *)gs_alloc_bytes(ctx->memory, sizeof(gs_halftone), "pdfi_do_halftone");
    if (pht == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    memset(pht, 0x00, sizeof(gs_halftone));
    pht->rc.memory = ctx->memory;
    pht->rc.free = pdfi_free_halftone;

    pdht = (gx_device_halftone *)gs_alloc_bytes(ctx->memory, sizeof(gx_device_halftone), "pdfi_do_halftone");
    if (pdht == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    memset(pdht, 0x00, sizeof(gx_device_halftone));
    pdht->num_dev_comp = ctx->pgs->device->color_info.num_components;

    switch(type) {
        case 1:
            phtc = (gs_halftone_component *)gs_alloc_bytes(ctx->memory, sizeof(gs_halftone_component), "pdfi_do_halftone");
            if (phtc == 0) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }

            code = build_type1_halftone(ctx, halftone_dict, page_dict, pdht, phtc, (char *)"Default", 7);
            if (code < 0)
                goto error;

            pht->type = ht_type_multiple;
            pht->params.multiple.components = phtc;
            pht->params.multiple.num_comp = 1;
            pht->params.multiple.get_colorname_string = pdfi_separation_name_from_index;
            code = gx_gstate_dev_ht_install(ctx->pgs, pdht, pht->type, gs_currentdevice_inline(ctx->pgs));
            if (code < 0)
                goto error;

            gx_device_halftone_release(pdht, pdht->rc.memory);
            ctx->pgs->halftone = pht;
            rc_increment(ctx->pgs->halftone);
            gx_unset_both_dev_colors(ctx->pgs);
            break;

        case 5:
            /* The only case involving multiple halftones, we need to enumerate each entry
             * in the dictionary
             */
            /* Type 5 halftone dictionaries are required to have a Default */
            code = pdfi_dict_known(halftone_dict, "Default", &known);
            if (code < 0)
                return code;
            if (!known) {
                code = gs_note_error(gs_error_undefined);
                return code;
            }

            code = pdfi_dict_first(ctx, halftone_dict, &Key, &Value, &index);
            if (code < 0)
                goto error;

            do {
                if (Key->type != PDF_NAME) {
                    code = gs_note_error(gs_error_typecheck);
                    goto error;
                }
                if (!pdfi_name_is((const pdf_name *)Key, "HalftoneName") && !pdfi_name_is((const pdf_name *)Key, "HalftoneType") && !pdfi_name_is((const pdf_name *)Key, "Type"))
                    NumComponents++;

                pdfi_countdown(Key);
                pdfi_countdown(Value);
                Key = Value = NULL;
                if (ctx->loop_detection)
                    pdfi_loop_detector_mark(ctx);

                code = pdfi_dict_next(ctx, halftone_dict, &Key, &Value, &index);
                if (code < 0 && code != gs_error_undefined)
                    goto error;
                if (ctx->loop_detection)
                    pdfi_loop_detector_cleartomark(ctx);

            } while (code >= 0);

            phtc = (gs_halftone_component *)gs_alloc_bytes(ctx->memory, sizeof(gs_halftone_component) * NumComponents, "pdfi_do_halftone");
            if (phtc == 0) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }

            /* extract each halftone from the type 5 dictionary, build a halftone for each by type */
            ix = index = 0;
            code = pdfi_dict_first(ctx, halftone_dict, &Key, &Value, &index);
            if (code < 0)
                goto error;
            do {
                if (Key->type != PDF_NAME || Value->type != PDF_DICT) {
                    code = gs_note_error(gs_error_typecheck);
                    goto error;
                }
                if (pdfi_name_is((const pdf_name *)Key, "HalftoneName") || pdfi_name_is((const pdf_name *)Key, "HalftoneType") || pdfi_name_is((const pdf_name *)Key, "Type")) {
                    pdfi_countdown(Key);
                    pdfi_countdown(Value);
                    continue;
                }

                code = pdfi_dict_get_int(ctx, (pdf_dict *)Value, "HalftoneType", &type);
                if (code < 0)
                    return code;

                code = pdfi_string_from_name(ctx, (pdf_name *)Key, &str, &str_len);
                if (code < 0)
                    goto error;

                switch(type) {
                    case 1:
                        code = build_type1_halftone(ctx, (pdf_dict *)Value, page_dict, pdht, &phtc[ix++], str, str_len);
                        if (code < 0)
                            goto error;
                        break;
                    case 6:
                        code = build_type6_halftone(ctx, (pdf_dict *)Value, page_dict, pdht, &phtc[ix++], str, str_len);
                        if (code < 0)
                            goto error;
                        break;
                    case 10:
                        code = build_type10_halftone(ctx, (pdf_dict *)Value, page_dict, pdht, &phtc[ix++], str, str_len);
                        if (code < 0)
                            goto error;
                        break;
                    case 16:
                        code = build_type16_halftone(ctx, (pdf_dict *)Value, page_dict, pdht, &phtc[ix++], str, str_len);
                        if (code < 0)
                            goto error;
                        break;
                    default:
                        code = gs_note_error(gs_error_rangecheck);
                        goto error;
                        break;

                }
                gs_free_object(ctx->memory, str, "pdfi_string_from_name");
                str = NULL;


                pdfi_countdown(Key);
                pdfi_countdown(Value);
                Key = Value = NULL;
                code = pdfi_dict_next(ctx, halftone_dict, &Key, &Value, &index);
                if (code < 0 && code != gs_error_undefined)
                    goto error;
            } while (code >= 0);

            pht->type = ht_type_multiple;
            pht->params.multiple.components = phtc;
            pht->params.multiple.num_comp = NumComponents;
            pht->params.multiple.get_colorname_string = pdfi_separation_name_from_index;
            code = gx_gstate_dev_ht_install(ctx->pgs, pdht, pht->type, gs_currentdevice_inline(ctx->pgs));
            if (code < 0)
                goto error;

            gx_device_halftone_release(pdht, pdht->rc.memory);
            ctx->pgs->halftone = pht;
            rc_increment(ctx->pgs->halftone);
            gx_unset_both_dev_colors(ctx->pgs);
            break;

        case 6:
            phtc = (gs_halftone_component *)gs_alloc_bytes(ctx->memory, sizeof(gs_halftone_component), "pdfi_do_halftone");
            if (phtc == 0) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }

            code = build_type6_halftone(ctx, halftone_dict, page_dict, pdht, phtc, (char *)"Default", 7);
            if (code < 0)
                goto error;

            pht->type = ht_type_multiple;
            pht->params.multiple.components = phtc;
            pht->params.multiple.num_comp = 1;
            pht->params.multiple.get_colorname_string = pdfi_separation_name_from_index;

            code = gs_sethalftone_prepare(ctx->pgs, pht, pdht);

            code = gx_gstate_dev_ht_install(ctx->pgs, pdht, pht->type, gs_currentdevice_inline(ctx->pgs));
            if (code < 0)
                goto error;

            gx_device_halftone_release(pdht, pdht->rc.memory);
            ctx->pgs->halftone = pht;
            rc_increment(ctx->pgs->halftone);
            gx_unset_both_dev_colors(ctx->pgs);
            break;
        case 10:
            phtc = (gs_halftone_component *)gs_alloc_bytes(ctx->memory, sizeof(gs_halftone_component), "pdfi_do_halftone");
            if (phtc == 0) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }

            code = build_type10_halftone(ctx, halftone_dict, page_dict, pdht, phtc, (char *)"Default", 7);
            if (code < 0)
                goto error;

            pht->type = ht_type_multiple;
            pht->params.multiple.components = phtc;
            pht->params.multiple.num_comp = 1;
            pht->params.multiple.get_colorname_string = pdfi_separation_name_from_index;

            code = gs_sethalftone_prepare(ctx->pgs, pht, pdht);

            code = gx_gstate_dev_ht_install(ctx->pgs, pdht, pht->type, gs_currentdevice_inline(ctx->pgs));
            if (code < 0)
                goto error;

            gx_device_halftone_release(pdht, pdht->rc.memory);
            ctx->pgs->halftone = pht;
            rc_increment(ctx->pgs->halftone);
            gx_unset_both_dev_colors(ctx->pgs);
            break;
        case 16:
            phtc = (gs_halftone_component *)gs_alloc_bytes(ctx->memory, sizeof(gs_halftone_component), "pdfi_do_halftone");
            if (phtc == 0) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }

            code = build_type16_halftone(ctx, halftone_dict, page_dict, pdht, phtc, (char *)"Default", 7);
            if (code < 0)
                goto error;

            pht->type = ht_type_multiple;
            pht->params.multiple.components = phtc;
            pht->params.multiple.num_comp = 1;
            pht->params.multiple.get_colorname_string = pdfi_separation_name_from_index;

            code = gs_sethalftone_prepare(ctx->pgs, pht, pdht);

            code = gx_gstate_dev_ht_install(ctx->pgs, pdht, pht->type, gs_currentdevice_inline(ctx->pgs));
            if (code < 0)
                goto error;

            gx_device_halftone_release(pdht, pdht->rc.memory);
            ctx->pgs->halftone = pht;
            rc_increment(ctx->pgs->halftone);
            gx_unset_both_dev_colors(ctx->pgs);
            break;
        default:
            return_error(gs_error_rangecheck);
            break;
    }
    gs_free_object(ctx->memory, pdht, "pdfi_do_halftone");
    return 0;

error:
    gs_free_object(ctx->memory, str, "pdfi_string_from_name");
    pdfi_countdown(Key);
    pdfi_countdown(Value);
    gs_free_object(ctx->memory, pht, "pdfi_do_halftone");
    gs_free_object(ctx->memory, phtc, "pdfi_do_halftone");
    gs_free_object(ctx->memory, pdht, "pdfi_do_halftone");
    return code;
}

static int GS_HT(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *obj = NULL;

    code = pdfi_dict_get(ctx, GS, "HT", &obj);
    if (code < 0)
        return code;


    if (obj->type == PDF_NAME) {
        if (pdfi_name_is((const pdf_name *)obj, "Default")) {
            goto exit;
        } else {
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
        }
    } else {
        if (obj->type != PDF_DICT) {
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }
        code = pdfi_do_halftone(ctx, (pdf_dict *)obj, page_dict);
    }

exit:
    pdfi_countdown(obj);
    return code;
}

static int GS_FL(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    double d1;

    code = pdfi_dict_get_number(ctx, GS, "FL", &d1);
    if (code < 0)
        return code;

    code = gs_setflat(ctx->pgs, d1);
    return code;
}

static int GS_SM(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    double d1;

    code = pdfi_dict_get_number(ctx, GS, "SM", &d1);
    if (code < 0)
        return code;

    code = gs_setsmoothness(ctx->pgs, d1);
    return code;
}

static int GS_SA(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_bool *b;
    int code;

    code = pdfi_dict_get_type(ctx, GS, "SA", PDF_BOOL, (pdf_obj **)&b);
    if (code < 0)
        return code;

    code = gs_setstrokeadjust(ctx->pgs, b->value);
    pdfi_countdown(b);
    return 0;
}

static int GS_BM(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_name *n;
    int code;
    gs_blend_mode_t mode;

    code = pdfi_dict_get_type(ctx, GS, "BM", PDF_NAME, (pdf_obj **)&n);
    if (code < 0)
        return code;

    code = pdfi_get_blend_mode(ctx, n, &mode);
    pdfi_countdown(n);
    if (code == 0)
        return gs_setblendmode(ctx->pgs, mode);
    return_error(gs_error_undefined);
}

static int GS_SMask(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;
    int code;

    if (ctx->page_has_transparency == false || ctx->notransparency == true)
        return 0;

    code = pdfi_dict_get(ctx, GS, "SMask", (pdf_obj **)&o);
    if (code < 0)
        return code;

    if (o->type == PDF_NAME) {
        pdf_name *n = (pdf_name *)o;

        if (pdfi_name_is(n, "None")) {
            if (igs->SMask) {
                pdfi_gstate_smask_free(igs);
                code = pdfi_trans_end_smask_notify(ctx);
            }
            goto exit;
        }
        code = pdfi_find_resource(ctx, (unsigned char *)"ExtGState", n, stream_dict, page_dict, &o);
        pdfi_countdown(n);
        if (code < 0)
            return code;
    }

    if (o->type == PDF_DICT) {
        if (igs->SMask)
            pdfi_gstate_smask_free(igs);
        pdfi_gstate_smask_install(igs, ctx->memory, (pdf_dict *)o, ctx->pgs);
    }

 exit:
    pdfi_countdown(o);
    return 0;
}

static int GS_CA(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    double d1;

    code = pdfi_dict_get_number(ctx, GS, "CA", &d1);
    if (code < 0)
        return code;

    code = gs_setstrokeconstantalpha(ctx->pgs, d1);
    return code;
}

static int GS_ca(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    double d1;

    code = pdfi_dict_get_number(ctx, GS, "ca", &d1);
    if (code < 0)
        return code;

    code = gs_setfillconstantalpha(ctx->pgs, d1);
    return code;
}

static int GS_AIS(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_bool *b;
    int code;

    code = pdfi_dict_get_type(ctx, GS, "AIS", PDF_BOOL, (pdf_obj **)&b);
    if (code < 0)
        return code;

    code = gs_setalphaisshape(ctx->pgs, b->value);
    pdfi_countdown(b);
    return 0;
}

static int GS_TK(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_bool *b;
    int code;

    code = pdfi_dict_get_type(ctx, GS, "TK", PDF_BOOL, (pdf_obj **)&b);
    if (code < 0)
        return code;

    code = gs_settextknockout(ctx->pgs, b->value);
    pdfi_countdown(b);
    return 0;
}

typedef int (*GS_proc)(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict);

typedef struct GS_Func {
    const char *Name;
    GS_proc proc;
} GS_Func_t;

GS_Func_t ExtGStateTable[] = {
    {"LW", GS_LW},
    {"LC", GS_LC},
    {"LJ", GS_LJ},
    {"ML", GS_ML},
    {"D", GS_D},
    {"RI", GS_RI},
    {"OP", GS_OP},
    {"op", GS_op},
    {"OPM", GS_OPM},
    {"Font", GS_Font},
    {"BG", GS_BG},
    {"BG2", GS_BG2},
    {"UCR", GS_UCR},
    {"UCR2", GS_UCR2},
    {"TR", GS_TR},
    {"TR2", GS_TR2},
    {"HT", GS_HT},
    {"FL", GS_FL},
    {"SM", GS_SM},
    {"SA", GS_SA},
    {"BM", GS_BM},
    {"SMask", GS_SMask},
    {"CA", GS_CA},
    {"ca", GS_ca},
    {"AIS", GS_AIS},
    {"TK", GS_TK},
};

int pdfi_setgstate(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_name *n;
    pdf_obj *o;
    int code, i, limit = sizeof(ExtGStateTable) / sizeof (GS_Func_t);
    bool known;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    if (pdfi_count_stack(ctx) < 1) {
        (void)pdfi_loop_detector_cleartomark(ctx);
        return_error(gs_error_stackunderflow);
    }
    n = (pdf_name *)ctx->stack_top[-1];
    if (n->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        return_error(gs_error_typecheck);
    }

    code = pdfi_find_resource(ctx, (unsigned char *)"ExtGState", n, stream_dict, page_dict, &o);
    pdfi_pop(ctx, 1);
    if (code < 0) {
        (void)pdfi_loop_detector_cleartomark(ctx);
        return code;
    }
    if (o->type != PDF_DICT) {
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        return_error(gs_error_typecheck);
    }

    for (i=0;i < limit; i++) {
        code = pdfi_dict_known((pdf_dict *)o, ExtGStateTable[i].Name, &known);
        if (code < 0 && ctx->pdfstoponerror) {
            (void)pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(o);
            return code;
        }
        if (known) {
            code = ExtGStateTable[i].proc(ctx, (pdf_dict *)o, stream_dict, page_dict);
            if (code < 0 && ctx->pdfstoponerror) {
                (void)pdfi_loop_detector_cleartomark(ctx);
                pdfi_countdown(o);
                return code;
            }
        }
    }

    code = pdfi_loop_detector_cleartomark(ctx);
    pdfi_countdown(o);
    return code;
}


int pdfi_free_DefaultQState(pdf_context *ctx)
{
    int code = 0;

    if (ctx->DefaultQState)
        code = gs_gstate_free(ctx->DefaultQState);
    ctx->DefaultQState = NULL;
    return code;
}

int pdfi_set_DefaultQState(pdf_context *ctx, gs_gstate *pgs)
{
    pdfi_free_DefaultQState(ctx);
    ctx->DefaultQState = gs_gstate_copy(pgs, ctx->memory);
    if (ctx->DefaultQState == NULL)
        return_error(gs_error_VMerror);
    return 0;
}

gs_gstate *pdfi_get_DefaultQState(pdf_context *ctx)
{
    return ctx->DefaultQState;
}

int pdfi_copy_DefaultQState(pdf_context *ctx, gs_gstate **pgs)
{
    *pgs = gs_gstate_copy(ctx->DefaultQState, ctx->memory);
    if (*pgs == NULL)
        return_error(gs_error_VMerror);
    return 0;
}

int pdfi_restore_DefaultQState(pdf_context *ctx, gs_gstate **pgs)
{
    int code;

    code = pdfi_set_DefaultQState(ctx, *pgs);
    if (code < 0)
        return code;
    code = gs_gstate_free(*pgs);
    *pgs = NULL;
    return code;
}
