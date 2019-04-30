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

/* Pattern operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_colour.h"
#include "pdf_pattern.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "gsicc_manage.h"
#include "gsicc_profilecache.h"
#include "gsicc_create.h"
#include "gsptype2.h"

#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_func.h"
#include "pdf_shading.h"
#include "gscsepr.h"
#include "stream.h"
#include "strmio.h"
#include "gscdevn.h"

/* Turn a /Matrix Array into a gs_matrix.  If Array is NULL, makes an identity matrix */
static int
pdfi_array_to_gs_matrix(pdf_context *ctx, pdf_array *array, gs_matrix *mat)
{
    double number;
    int code = 0;

    /* Init to identity matrix to allow sane continuation on errors */
    mat->xx = 1.0;
    mat->xy = 0.0;
    mat->yx = 0.0;
    mat->yy = 1.0;
    mat->tx = 0.0;
    mat->ty = 0.0;

    /* Identity matrix if no array */
    if (array == NULL) {
        return 0;
    }
    if (pdfi_array_size(array) != 6) {
        return_error(gs_error_rangecheck);
    }
    code = pdfi_array_get_number(ctx, array, 0, &number);
    if (code < 0) goto errorExit;
    mat->xx = (float)number;
    code = pdfi_array_get_number(ctx, array, 1, &number);
    if (code < 0) goto errorExit;
    mat->xy = (float)number;
    code = pdfi_array_get_number(ctx, array, 2, &number);
    if (code < 0) goto errorExit;
    mat->yx = (float)number;
    code = pdfi_array_get_number(ctx, array, 3, &number);
    if (code < 0) goto errorExit;
    mat->yy = (float)number;
    code = pdfi_array_get_number(ctx, array, 4, &number);
    if (code < 0) goto errorExit;
    mat->tx = (float)number;
    code = pdfi_array_get_number(ctx, array, 5, &number);
    if (code < 0) goto errorExit;
    mat->ty = (float)number;
    return 0;

 errorExit:
    return code;
}

/* Turn an Array into a gs_rect.  If Array is NULL, makes a tiny rect */
static int
pdfi_array_to_gs_rect(pdf_context *ctx, pdf_array *array, gs_rect *rect)
{
    double number;
    int code = 0;

    /* Init to tiny rect to allow sane continuation on errors */
    rect->p.x = 0.0;
    rect->p.y = 0.0;
    rect->q.x = 1.0;
    rect->q.y = 1.0;

    /* Identity matrix if no array */
    if (array == NULL) {
        return 0;
    }
    if (pdfi_array_size(array) != 4) {
        return_error(gs_error_rangecheck);
    }
    code = pdfi_array_get_number(ctx, array, 0, &number);
    if (code < 0) goto errorExit;
    rect->p.x = (float)number;
    code = pdfi_array_get_number(ctx, array, 1, &number);
    if (code < 0) goto errorExit;
    rect->p.y = (float)number;
    code = pdfi_array_get_number(ctx, array, 2, &number);
    if (code < 0) goto errorExit;
    rect->q.x = (float)number;
    code = pdfi_array_get_number(ctx, array, 3, &number);
    if (code < 0) goto errorExit;
    rect->q.y = (float)number;
    return 0;

 errorExit:
    return code;
}

/* NULL Pattern */
static int
pdfi_setpattern_null(pdf_context *ctx, gs_client_color *cc)
{
    int code = 0;
    gs_client_pattern templat;
    gs_matrix mat;
    gs_rect rect;

    gs_pattern1_init(&templat);

    /* Init identity matrix */
    pdfi_array_to_gs_matrix(ctx, NULL, &mat);
    pdfi_array_to_gs_rect(ctx, NULL, &rect);
    templat.BBox = rect;
    templat.PaintProc = NULL;
    templat.PaintType = 1;
    templat.TilingType = 3;
    templat.XStep = 1;
    templat.YStep = 1;

    code = gs_makepattern(cc, &templat, &mat, ctx->pgs, ctx->memory);

    return code;
}

static int
pdfi_pattern_paint(const gs_client_color *pcc, gs_gstate *pgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    pdf_context *ctx = (pdf_context *)ppat->client_data;
    dbgmprintf(ctx->memory, "WARNING: pdfi_pattern_paint not implemented\n");
    return 0;
}

static int
pdfi_pattern_remap(const gs_client_color *pcc, gs_gstate *pgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    pdf_context *ctx = (pdf_context *)ppat->client_data;
    int code = 0;


    /* pgs->device is the newly created pattern accumulator, but we want to test the device
     * that is 'behind' that, the actual output device, so we use the one from
     * the saved graphics state.
     */
    if (pgs->have_pattern_streams) {
        dbgmprintf(ctx->memory, "WARNING: have_pattern_streams, but not attempting to use device accum\n");
#if 0
        code = dev_proc(pcc->pattern->saved->device, dev_spec_op)(pcc->pattern->saved->device,
                                gxdso_pattern_can_accum, (void *)ppat, ppat->uid.id);
#endif
    }

    if (code == 1) {
        /* Device handles high-level patterns, so return 'remap'.
         * This closes the internal accumulator device, as we no longer need
         * it, and the error trickles back up to the PDL client. The client
         * must then take action to start the device's accumulator, draw the
         * pattern, close the device's accumulator and generate a cache entry.
         * TODO: See px_high_level_pattern above.
         */
        return_error(gs_error_Remap_Color);
    } else {
        return pdfi_pattern_paint(pcc, pgs);
    }
}



/* Setup the correct gstate for a pattern (this does a gsave) */
static int
pdfi_pattern_gset(pdf_context *ctx)
{
    int code;
    float strokealpha, fillalpha;

    code = gs_gsave(ctx->pgs);
    if (code < 0)
        goto exit;
    strokealpha = gs_getstrokeconstantalpha(ctx->pgs);
    fillalpha = gs_getfillconstantalpha(ctx->pgs);
    code = gs_copygstate(ctx->pgs, ctx->base_pgs);
    if (code < 0)
        goto exit;
    code = gs_setstrokeconstantalpha(ctx->pgs, strokealpha);
    code = gs_setfillconstantalpha(ctx->pgs, fillalpha);

 exit:
    return code;
}

/* Type 1 (tiled) Pattern */
static int
pdfi_setpattern_type1(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict,
                      pdf_dict *pdict, gs_client_color *cc)
{
    int code = 0;
    gs_client_pattern templat;
    gs_matrix mat;
    gs_rect rect;
    int64_t PaintType, TilingType;
    pdf_array *BBox = NULL;
    double XStep, YStep;
    pdf_dict *Resources = NULL;
    pdf_array *Matrix = NULL;

    dbgmprintf(ctx->memory, "PATTERN: Type 1 pattern\n");

    gs_pattern1_init(&templat);

    /* Required */
    code = pdfi_dict_get_int(ctx, pdict, "PaintType", &PaintType);
    if (code < 0)
        goto exit;
    code = pdfi_dict_get_int(ctx, pdict, "TilingType", &TilingType);
    if (code < 0)
        goto exit;
    code = pdfi_dict_get_type(ctx, pdict, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
    if (code < 0)
        goto exit;
    code = pdfi_array_to_gs_rect(ctx, BBox, &rect);
    if (code < 0)
        goto exit;
    code = pdfi_dict_get_number(ctx, pdict, "XStep", &XStep);
    if (code < 0)
        goto exit;
    code = pdfi_dict_get_number(ctx, pdict, "YStep", &YStep);
    if (code < 0)
        goto exit;
    code = pdfi_dict_get_type(ctx, pdict, "Resources", PDF_DICT, (pdf_obj **)&Resources);
    if (code < 0)
        goto exit;

    /* (optional Matrix) */
    code = pdfi_dict_knownget_type(ctx, pdict, "Matrix", PDF_ARRAY, (pdf_obj **)&Matrix);
    if (code < 0)
        goto exit;
    code = pdfi_array_to_gs_matrix(ctx, Matrix, &mat);
    if (code < 0)
        goto exit;

    /* Check some bounds */
    if (pdfi_array_size(BBox) != 4) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }
    if (PaintType != 1 && PaintType != 2) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }
    if (TilingType != 1 && TilingType != 2 && TilingType != 3) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    templat.BBox = rect;
    /* TODO! This can't be null and I have no clue... (see zPaintProc or px_remap_pattern) */
    templat.PaintProc = pdfi_pattern_remap;
    templat.PaintType = PaintType;
    templat.TilingType = TilingType;
    templat.XStep = XStep;
    templat.YStep = YStep;
    templat.client_data = ctx; /* TODO: Might need to pass other things like Resource? */

    code = pdfi_pattern_gset(ctx);
    if (code < 0)
        goto exit;

    code = gs_make_pattern(cc, (const gs_pattern_template_t *)&templat, &mat, ctx->pgs, ctx->memory);
    if (code < 0)
        goto exit;

    code = gs_grestore(ctx->pgs);
    if (code < 0)
        goto exit;
 exit:
    pdfi_countdown(Resources);
    pdfi_countdown(Matrix);
    pdfi_countdown(BBox);
    return code;
}

/* Type 2 (shading) Pattern */
static int
pdfi_setpattern_type2(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict,
                      pdf_dict *pdict, gs_client_color *cc)
{
    int code = 0;
    pdf_dict *Shading = NULL;
    pdf_dict *ExtGState = NULL;
    pdf_array *Matrix = NULL;
    gs_matrix mat;
    gs_shading_t *shading;
    int64_t shading_type;
    gs_pattern2_template_t templat;

    /* See zbuildshadingpattern() */

    dbgmprintf(ctx->memory, "PATTERN: Type 2 pattern\n");

    /* (optional Matrix) */
    code = pdfi_dict_knownget_type(ctx, pdict, "Matrix", PDF_ARRAY, (pdf_obj **)&Matrix);
    if (code < 0)
        goto exit;
    code = pdfi_array_to_gs_matrix(ctx, Matrix, &mat);
    if (code < 0)
        goto exit;

    /* Required Shading, can be stream or dict (but a stream is also a dict..) */
    code = pdfi_dict_knownget_type(ctx, pdict, "Shading", PDF_DICT, (pdf_obj **)&Shading);
    if (code < 0)
        goto exit;
    if (code == 0) {
        dbgmprintf(ctx->memory, "ERROR: Shading not found in Pattern Type 2\n");
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

    /* Optional ExtGState */
    code = pdfi_dict_knownget_type(ctx, pdict, "ExtGState", PDF_DICT, (pdf_obj **)&ExtGState);
    if (code < 0)
        goto exit;
    if (code != 0) {
        /* Not implemented, ignore for now */
        dbgmprintf(ctx->memory, "WARNING: Pattern ExtGState not supported, skipping\n");
    }

    code = pdfi_pattern_gset(ctx);
    if (code < 0)
        goto exit;
    code = pdfi_shading_build(ctx, stream_dict, page_dict, Shading, &shading, &shading_type);
    if (code != 0) {
        dbgmprintf(ctx->memory, "ERROR: can't build shading structure\n");
        goto exit;
    }
    gs_pattern2_init(&templat);
    templat.Shading = shading;
    code = gs_make_pattern(cc, (const gs_pattern_template_t *)&templat, &mat, ctx->pgs, ctx->memory);
    if (code < 0)
        goto exit;

    code = gs_grestore(ctx->pgs);
    if (code < 0)
        goto exit;

    /* NOTE: I am pretty sure this needs to be freed much later, but haven't figured it out :( */
    //pdfi_shading_free(ctx, shading, shading_type);

 exit:
    pdfi_countdown(Shading);
    pdfi_countdown(Matrix);
    pdfi_countdown(ExtGState);
    return code;
}

int
pdfi_pattern_set(pdf_context *ctx, pdf_dict *stream_dict,
                pdf_dict *page_dict, pdf_name *pname,
                gs_client_color *cc)
{
    pdf_dict *pdict = NULL;
    int code;
    int64_t patternType;

    memset(cc, 0, sizeof(*cc));
    code = pdfi_find_resource(ctx, (unsigned char *)"Pattern", pname, stream_dict,
                              page_dict, (pdf_obj **)&pdict);
    if (code < 0) {
        dbgmprintf(ctx->memory, "WARNING: Pattern object not found in resources\n");
        goto exit;
    }
    dbgmprintf1(ctx->memory, "PATTERN: pdfi_setpattern: found pattern object %lu\n", pdict->object_num);

    code = pdfi_dict_get_int(ctx, pdict, "PatternType", &patternType);
    if (code < 0)
        goto exit;
    if (patternType == 1) {
        code = pdfi_setpattern_type1(ctx, stream_dict, page_dict, pdict, cc);
        if (code < 0)
            goto exit;
    } else if (patternType == 2) {
        code = pdfi_setpattern_type2(ctx, stream_dict, page_dict, pdict, cc);
        if (code < 0)
            goto exit;
    } else {
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

 exit:
    pdfi_countdown(pdict);
    return code;
}

/* Create a Pattern colorspace.
 * If ppcs is NULL, then we will set the colorspace
 * If ppcs not NULL, point the new colorspace to it
 *
 * If color_array is NULL, then this is a simple "Pattern" colorspace, e.g. "/Pattern cs".
 * If it is an array, then first element is "Pattern" and second element should be the base colorspace.
 * e.g. "/CS1 cs" where /CS1 is a ColorSpace Resource containing "[/Pattern /DeviceRGB]"
 *
 */
int
pdfi_pattern_create(pdf_context *ctx, pdf_array *color_array,
                    pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs)
{
    gs_color_space *pcs = NULL;
    gs_color_space *base_space;
    pdf_name *base_name = NULL;
    int code;

    /* TODO: should set to "the initial color is a pattern object that causes nothing to be painted."
     * (see page 288 of PDF 1.7)
     * Need to make a "nullpattern" (see pdf_ops.c, /nullpattern)
     */
    /* NOTE: See zcolor.c/setpatternspace */
    dbgmprintf(ctx->memory, "PATTERN: pdfi_create_Pattern\n");
    //    return 0;

    pcs = gs_cspace_alloc(ctx->memory, &gs_color_space_type_Pattern);
    if (pcs == NULL) {
        return_error(gs_error_VMerror);
    }
    if (color_array == NULL) {
        pcs->base_space = NULL;
        pcs->params.pattern.has_base_space = false;
    } else {
        dbgmprintf(ctx->memory, "PATTERN: with base space! pdfi_create_Pattern\n");
        if (pdfi_array_size(color_array) < 2) {
            return_error(gs_error_syntaxerror);
        }
        code = pdfi_array_get_type(ctx, color_array, 1, PDF_NAME, (pdf_obj **)&base_name);
        if (code < 0)
            goto exit;
        code = pdfi_create_colorspace_by_name(ctx, base_name, stream_dict, page_dict, &base_space);
        if (code < 0)
            goto exit;
        pcs->base_space = base_space;
        pcs->params.pattern.has_base_space = true;
    }
    if (ppcs != NULL) {
        *ppcs = pcs;
    } else {
        code = gs_setcolorspace(ctx->pgs, pcs);
        /* release reference from construction */
        rc_decrement_only_cs(pcs, "create_Pattern");

#if 0
        /* An attempt to init a "Null" pattern, causes crashes on cluster */
        {
        gs_client_color cc;
        memset(&cc, 0, sizeof(cc));
        code = pdfi_setpattern_null(ctx, &cc);
        code = gs_setcolor(ctx->pgs, &cc);
        }
#endif
    }


 exit:
    pdfi_countdown(base_name);
    return code;
}
