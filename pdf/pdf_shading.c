/* Copyright (C) 2018-2021 Artifex Software, Inc.
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

/* Shading operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_gstate.h"
#include "pdf_shading.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_func.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"
#include "pdf_trans.h"
#include "pdf_optcontent.h"
#include "pdf_doc.h"
#include "pdf_misc.h"

#include "gsfunc3.h"    /* for gs_function_Ad0t_params_t */
#include "gxshade.h"
#include "gsptype2.h"
#include "gsfunc0.h"    /* For gs_function */
#include "gscolor3.h"   /* For gs_shfill() */

static int pdfi_build_shading_function(pdf_context *ctx, gs_function_t **ppfn, const float *shading_domain, int num_inputs, pdf_dict *shading_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *o = NULL;
    pdf_obj * rsubfn = NULL;
    gs_function_AdOt_params_t params;

    memset(&params, 0x00, sizeof(params));

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_dict_get(ctx, shading_dict, "Function", &o);
    if (code < 0)
        goto build_shading_function_error;

    if (o->type != PDF_DICT && o->type != PDF_STREAM) {
        uint size;
        pdf_obj *rsubfn;
        gs_function_t **Functions;
        int64_t i;

        if (o->type != PDF_ARRAY) {
            code = gs_error_typecheck;
            goto build_shading_function_error;
        }
        size = pdfi_array_size(((pdf_array *)o));

        if (size == 0) {
            code = gs_error_rangecheck;
            goto build_shading_function_error;
        }
        code = alloc_function_array(size, &Functions, ctx->memory);
        if (code < 0)
            goto build_shading_function_error;

        for (i = 0; i < size; ++i) {
            code = pdfi_array_get(ctx, (pdf_array *)o, i, &rsubfn);
            if (code == 0) {
                if (rsubfn->type != PDF_DICT && rsubfn->type != PDF_STREAM)
                    code = gs_note_error(gs_error_typecheck);
            }
            if (code < 0) {
                int j;

                for (j = 0;j < i; j++) {
                    pdfi_free_function(ctx, Functions[j]);
                    Functions[j] = NULL;
                }
                gs_free_object(ctx->memory, Functions, "function array error, freeing functions");
                goto build_shading_function_error;
            }
            code = pdfi_build_function(ctx, &Functions[i], shading_domain, num_inputs, rsubfn, page_dict);
            if (code < 0)
                goto build_shading_function_error;
            pdfi_countdown(rsubfn);
            rsubfn = NULL;
        }
        params.m = num_inputs;
        params.Domain = 0;
        params.n = size;
        params.Range = 0;
        params.Functions = (const gs_function_t * const *)Functions;
        code = gs_function_AdOt_init(ppfn, &params, ctx->memory);
        if (code < 0)
            goto build_shading_function_error;
    } else {
        code = pdfi_build_function(ctx, ppfn, shading_domain, num_inputs, o, page_dict);
        if (code < 0)
            goto build_shading_function_error;
    }

    (void)pdfi_loop_detector_cleartomark(ctx);
    pdfi_countdown(o);
    return code;

build_shading_function_error:
    gs_function_AdOt_free_params(&params, ctx->memory);
    pdfi_countdown(rsubfn);
    pdfi_countdown(o);
    (void)pdfi_loop_detector_cleartomark(ctx);
    return code;
}

static int pdfi_shading1(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    int code, i;
    gs_shading_Fb_params_t params;
    static const float default_Domain[4] = {0, 1, 0, 1};
    pdf_dict *shading_dict;

    if (Shading->type != PDF_DICT)
        return_error(gs_error_typecheck);
    shading_dict = (pdf_dict *)Shading;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;
    gs_make_identity(&params.Matrix);
    params.Function = 0;

    code = fill_domain_from_dict(ctx, (float *)&params.Domain, 4, shading_dict);
    if (code < 0) {
        if (code == gs_error_undefined) {
            for (i = 0; i < 4; i++) {
                params.Domain[i] = default_Domain[i];
            }
        } else
            return code;
    }

    code = fill_matrix_from_dict(ctx, (float *)&params.Matrix, shading_dict);
    if (code < 0)
        return code;

    code = pdfi_build_shading_function(ctx, &params.Function, (const float *)&params.Domain, 2, (pdf_dict *)shading_dict, page_dict);
    if (code < 0){
        pdfi_countdown(o);
        return code;
    }
    code = gs_shading_Fb_init(ppsh, &params, ctx->memory);
    if (code < 0) {
        gs_free_object(ctx->memory, params.Function, "Function");
        pdfi_countdown(o);
    }
    return code;
}

static int pdfi_shading2(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    gs_shading_A_params_t params;
    static const float default_Domain[2] = {0, 1};
    int code, i;
    pdf_dict *shading_dict;

    if (Shading->type != PDF_DICT)
        return_error(gs_error_typecheck);
    shading_dict = (pdf_dict *)Shading;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = fill_float_array_from_dict(ctx, (float *)&params.Coords, 4, shading_dict, "Coords");
    if (code < 0)
        return code;
    code = fill_domain_from_dict(ctx, (float *)&params.Domain, 2, shading_dict);
    if (code < 0) {
        if (code == gs_error_undefined) {
            for (i = 0; i < 2; i++) {
                params.Domain[i] = default_Domain[i];
            }
        } else
            return code;
    }

    code = fill_bool_array_from_dict(ctx, (bool *)&params.Extend, 2, shading_dict, "Extend");
    if (code < 0) {
        if (code == gs_error_undefined) {
            params.Extend[0] = params.Extend[1] = false;
        } else
            return code;
    }

    code = pdfi_build_shading_function(ctx, &params.Function, (const float *)&params.Domain, 1, (pdf_dict *)shading_dict, page_dict);
    if (code < 0){
        pdfi_countdown(o);
        return code;
    }
    code = gs_shading_A_init(ppsh, &params, ctx->memory);
    if (code < 0){
        pdfi_countdown(o);
        return code;
    }

    return 0;
}

static int pdfi_shading3(pdf_context *ctx,  gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    gs_shading_R_params_t params;
    static const float default_Domain[2] = {0, 1};
    int code, i;
    pdf_dict *shading_dict;

    if (Shading->type != PDF_DICT)
        return_error(gs_error_typecheck);
    shading_dict = (pdf_dict *)Shading;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = fill_float_array_from_dict(ctx, (float *)&params.Coords, 6, shading_dict, "Coords");
    if (code < 0)
        return code;
    code = fill_domain_from_dict(ctx, (float *)&params.Domain, 4, shading_dict);
    if (code < 0) {
        if (code == gs_error_undefined) {
            for (i = 0; i < 2; i++) {
                params.Domain[i] = default_Domain[i];
            }
        } else
            return code;
    }

    code = fill_bool_array_from_dict(ctx, (bool *)&params.Extend, 2, shading_dict, "Extend");
    if (code < 0) {
        if (code == gs_error_undefined) {
            params.Extend[0] = params.Extend[1] = false;
        } else
            return code;
    }

    code = pdfi_build_shading_function(ctx, &params.Function, (const float *)&params.Domain, 1, (pdf_dict *)shading_dict, page_dict);
    if (code < 0){
        pdfi_countdown(o);
        return code;
    }
    code = gs_shading_R_init(ppsh, &params, ctx->memory);
    if (code < 0){
        pdfi_countdown(o);
        return code;
    }

    return 0;
}

static int pdfi_build_mesh_shading(pdf_context *ctx, gs_shading_mesh_params_t *params,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int num_decode = 4, code;
    gs_offset_t savedoffset;
    gs_offset_t stream_offset;
    int64_t Length;
    byte *data_source_buffer = NULL;
    pdf_c_stream *shading_stream = NULL;
    int64_t i;
    pdf_dict *shading_dict;

    if (Shading->type != PDF_STREAM)
        return_error(gs_error_typecheck);

    code = pdfi_dict_from_obj(ctx, Shading, &shading_dict);
    if (code < 0)
        return code;

    params->Function = NULL;
    params->Decode = NULL;

    stream_offset = pdfi_stream_offset(ctx, (pdf_stream *)Shading);
    if (stream_offset == 0)
        return_error(gs_error_typecheck);

    Length = pdfi_stream_length(ctx, (pdf_stream *)Shading);

    savedoffset = pdfi_tell(ctx->main_stream);
    code = pdfi_seek(ctx, ctx->main_stream, stream_offset, SEEK_SET);
    if (code < 0)
        return code;

    code = pdfi_open_memory_stream_from_filtered_stream(ctx, (pdf_stream *)Shading, Length, &data_source_buffer, ctx->main_stream, &shading_stream, false);
    if (code < 0) {
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    data_source_init_stream(&params->DataSource, shading_stream->s);

    /* We need to clear up the PDF stream, but leave the underlying stream alone, that's now
     * pointed to by the params.DataSource member.
     */
    gs_free_object(ctx->memory, shading_stream, "discard memory stream(pdf_stream)");

    code = pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    if (code < 0)
        goto build_mesh_shading_error;

    code = pdfi_build_shading_function(ctx, &params->Function, (const float *)NULL, 1,
                                       shading_dict, page_dict);
    if (code < 0 && code != gs_error_undefined)
        goto build_mesh_shading_error;

    code = pdfi_dict_get_int(ctx, shading_dict, "BitsPerCoordinate", &i);
    if (code < 0)
        goto build_mesh_shading_error;

    if (i != 1 && i != 2 && i != 4 && i != 8 && i != 12 && i != 16 && i != 24 && i != 32) {
        code = gs_error_rangecheck;
        goto build_mesh_shading_error;
    }

    params->BitsPerCoordinate = i;

    code = pdfi_dict_get_int(ctx, shading_dict, "BitsPerComponent", &i);
    if (code < 0)
        goto build_mesh_shading_error;

    if (i != 1 && i != 2 && i != 4 && i != 8 && i != 12 && i != 16) {
        code = gs_error_rangecheck;
        goto build_mesh_shading_error;
    }

    params->BitsPerComponent = i;

    if (params->Function != NULL)
        num_decode += 2;
    else
        num_decode += gs_color_space_num_components(params->ColorSpace) * 2;

    params->Decode = (float *) gs_alloc_byte_array(ctx->memory, num_decode, sizeof(float),
                            "build_mesh_shading");
    if (params->Decode == NULL) {
        code = gs_error_VMerror;
        goto build_mesh_shading_error;
    }

    code = fill_float_array_from_dict(ctx, (float *)params->Decode, num_decode, shading_dict, "Decode");
    if (code < 0)
        goto build_mesh_shading_error;

    return 0;

build_mesh_shading_error:
    if (params->Function)
        pdfi_free_function(ctx, params->Function);
    if (params->DataSource.data.strm != NULL) {
        s_close_filters(&params->DataSource.data.strm, params->DataSource.data.strm->strm);
        gs_free_object(ctx->memory, params->DataSource.data.strm, "release mesh shading Data Source");
    }
    gs_free_object(ctx->memory, params->Decode, "Decode");
    return code;
}

static int pdfi_shading4(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_FfGt_params_t params;
    int code;
    int64_t i;
    pdf_dict *shading_dict;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, Shading, stream_dict, page_dict);
    if (code < 0)
        return code;

    /* pdfi_build_mesh_shading checks the type of the Shading object, so we don't need to here */
    code = pdfi_dict_from_obj(ctx, Shading, &shading_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_int(ctx, shading_dict, "BitsPerFlag", &i);
    if (code < 0)
        return code;

    if (i != 2 && i != 4 && i != 8)
        return_error(gs_error_rangecheck);

    params.BitsPerFlag = i;

    code = gs_shading_FfGt_init(ppsh, &params, ctx->memory);
    if (code < 0) {
        gs_free_object(ctx->memory, params.Function, "Function");
        gs_free_object(ctx->memory, params.Decode, "Decode");
        return code;
    }
    return 0;
}

static int pdfi_shading5(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_LfGt_params_t params;
    int code;
    int64_t i;
    pdf_dict *shading_dict;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, Shading, stream_dict, page_dict);
    if (code < 0)
        return code;

    /* pdfi_build_mesh_shading checks the type of the Shading object, so we don't need to here */
    code = pdfi_dict_from_obj(ctx, Shading, &shading_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_int(ctx, shading_dict, "VerticesPerRow", &i);
    if (code < 0)
        return code;

    if (i < 2)
        return_error(gs_error_rangecheck);

    params.VerticesPerRow = i;

    code = gs_shading_LfGt_init(ppsh, &params, ctx->memory);
    if (code < 0) {
        gs_free_object(ctx->memory, params.Function, "Function");
        gs_free_object(ctx->memory, params.Decode, "Decode");
        return code;
    }
    return 0;
}

static int pdfi_shading6(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_Cp_params_t params;
    int code;
    int64_t i;
    pdf_dict *shading_dict;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, Shading, stream_dict, page_dict);
    if (code < 0)
        return code;

    /* pdfi_build_mesh_shading checks the type of the Shading object, so we don't need to here */
    code = pdfi_dict_from_obj(ctx, Shading, &shading_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_int(ctx, shading_dict, "BitsPerFlag", &i);
    if (code < 0)
        return code;

    if (i != 2 && i != 4 && i != 8)
        return_error(gs_error_rangecheck);

    params.BitsPerFlag = i;

    code = gs_shading_Cp_init(ppsh, &params, ctx->memory);
    if (code < 0) {
        gs_free_object(ctx->memory, params.Function, "Function");
        gs_free_object(ctx->memory, params.Decode, "Decode");
        return code;
    }
    return 0;
}

static int pdfi_shading7(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_obj *Shading, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_Tpp_params_t params;
    int code;
    int64_t i;
    pdf_dict *shading_dict;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, Shading, stream_dict, page_dict);
    if (code < 0)
        return code;

    /* pdfi_build_mesh_shading checks the type of the Shading object, so we don't need to here */
    code = pdfi_dict_from_obj(ctx, Shading, &shading_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_get_int(ctx, shading_dict, "BitsPerFlag", &i);
    if (code < 0)
        return code;

    if (i != 2 && i != 4 && i != 8)
        return_error(gs_error_rangecheck);

    params.BitsPerFlag = i;

    code = gs_shading_Tpp_init(ppsh, &params, ctx->memory);
    if (code < 0) {
        gs_free_object(ctx->memory, params.Function, "Function");
        gs_free_object(ctx->memory, params.Decode, "Decode");
        return code;
    }
    return 0;
}

static int get_shading_common(pdf_context *ctx, pdf_dict *shading_dict, gs_shading_params_t *params)
{
    gs_color_space *pcs = gs_currentcolorspace(ctx->pgs);
    int code, num_comp = gs_color_space_num_components(pcs);
    pdf_array *a = NULL;
    double *temp;

    if (num_comp < 0)	/* Pattern color space */
        return_error(gs_error_typecheck);

    params->ColorSpace = pcs;
    params->Background = NULL;
    rc_increment_cs(pcs);

    code = pdfi_dict_get_type(ctx, shading_dict, "Background", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0 && code != gs_error_undefined)
        return code;

    if (code >= 0) {
        uint64_t i;
        gs_client_color *pcc = NULL;

        if (pdfi_array_size(a) < num_comp) {
            code = gs_error_rangecheck;
            goto get_shading_common_error;
        }

        pcc = gs_alloc_struct(ctx->memory, gs_client_color, &st_client_color, "get_shading_common");
        if (pcc == 0) {
            code = gs_error_VMerror;
            goto get_shading_common_error;
        }

        pcc->pattern = 0;
        params->Background = pcc;

        temp = (double *)gs_alloc_bytes(ctx->memory, num_comp * sizeof(double), "temporary array of doubles");
        for(i=0;i<num_comp;i++) {
            code = pdfi_array_get_number(ctx, a, i, &temp[i]);
            if (code < 0) {
                gs_free_object(ctx->memory, temp, "free workign array (error)");
                goto get_shading_common_error;
            }
            pcc->paint.values[i] = temp[i];
        }
        pdfi_countdown((pdf_obj *)a);
        a = NULL;
        gs_free_object(ctx->memory, temp, "free workign array (done)");
    }


    code = pdfi_dict_get_type(ctx, shading_dict, "BBox", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0 && code != gs_error_undefined)
        goto get_shading_common_error;

    if (code >= 0) {
        double box[4];
        uint64_t i;

        if (pdfi_array_size(a) < 4) {
            code = gs_error_rangecheck;
            goto get_shading_common_error;
        }

        for(i=0;i<4;i++) {
            code = pdfi_array_get_number(ctx, a, i, &box[i]);
            if (code < 0)
                goto get_shading_common_error;
        }
        /* Adobe Interpreters accept denormalised BBox - bug 688937 */
        if (box[0] <= box[2]) {
            params->BBox.p.x = box[0];
            params->BBox.q.x = box[2];
        } else {
            params->BBox.p.x = box[2];
            params->BBox.q.x = box[0];
        }
        if (box[1] <= box[3]) {
            params->BBox.p.y = box[1];
            params->BBox.q.y = box[3];
        } else {
            params->BBox.p.y = box[3];
            params->BBox.q.y = box[1];
        }
        params->have_BBox = true;
    } else {
        params->have_BBox = false;
    }
    pdfi_countdown(a);
    a = NULL;

    code = pdfi_dict_get_bool(ctx, shading_dict, "AntiAlias", &params->AntiAlias);
    if (code < 0 && code != gs_error_undefined)
        goto get_shading_common_error;

    return 0;
get_shading_common_error:
    pdfi_countdown((pdf_obj *)a);
    gs_free_object(ctx->memory, params->Background, "Background (common_shading_error)");
    return code;
}

/* Build gs_shading_t object from a Shading Dict */
int
pdfi_shading_build(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict,
                   pdf_obj *Shading, gs_shading_t **ppsh)
{
    gs_shading_params_t params;
    gs_shading_t *psh = NULL;
    pdf_obj *cspace = NULL;
    int64_t type = 0;
    int code = 0;
    pdf_dict *sdict = NULL;

    memset(&params, 0, sizeof(params));

    params.ColorSpace = 0;
    params.cie_joint_caches = 0;
    params.Background = 0;
    params.have_BBox = 0;
    params.AntiAlias = 0;

    code = pdfi_dict_from_obj(ctx, Shading, &sdict);
    if (code < 0)
        return code;

    code = pdfi_dict_get(ctx, sdict, "ColorSpace", &cspace);
    if (code < 0)
        goto shading_error;

    code = pdfi_setcolorspace(ctx, cspace, stream_dict, page_dict);
    if (code < 0)
        goto shading_error;

    code = get_shading_common(ctx, sdict, &params);
    if (code < 0)
        goto shading_error;

    code = pdfi_dict_get_int(ctx, sdict, "ShadingType", &type);
    if (code < 0)
        goto shading_error;

    switch(type){
    case 1:
        code = pdfi_shading1(ctx, &params, &psh, Shading, stream_dict, page_dict);
        break;
    case 2:
        code = pdfi_shading2(ctx, &params, &psh, Shading, stream_dict, page_dict);
        break;
    case 3:
        code = pdfi_shading3(ctx, &params, &psh, Shading, stream_dict, page_dict);
        break;
    case 4:
        code = pdfi_shading4(ctx, &params, &psh, Shading, stream_dict, page_dict);
        break;
    case 5:
        code = pdfi_shading5(ctx, &params, &psh, Shading, stream_dict, page_dict);
        break;
    case 6:
        code = pdfi_shading6(ctx, &params, &psh, Shading, stream_dict, page_dict);
        break;
    case 7:
        code = pdfi_shading7(ctx, &params, &psh, Shading, stream_dict, page_dict);
        break;
    default:
        code = gs_note_error(gs_error_rangecheck);
        break;
    }
    if (code < 0)
        goto shading_error;

    pdfi_countdown(cspace);
    if (code >= 0)
        *ppsh = psh;
    return code;

 shading_error:
    if (cspace != NULL)
        pdfi_countdown(cspace);
    if (params.ColorSpace != NULL) {
        rc_decrement_only(params.ColorSpace, "ColorSpace (shading_build_error)");
        params.ColorSpace = NULL;
    }
    if (params.Background != NULL) {
        gs_free_object(ctx->memory, params.Background, "Background (shading_build_error)");
        params.Background = NULL;
    }
    return code;
}

/* Free stuff associated with a gs_shading_t.
 */
void
pdfi_shading_free(pdf_context *ctx, gs_shading_t *psh)
{
    gs_shading_params_t *params = &psh->params;

    rc_decrement_cs(params->ColorSpace, "pdfi_shading_free(ColorSpace)");
    params->ColorSpace = NULL;

    if (params->Background != NULL) {
        gs_free_object(ctx->memory, params->Background, "pdfi_shading_free(Background)");
        params->Background = NULL;
    }

    if (psh->head.type > 3) {
        gs_shading_mesh_params_t *mesh_params = (gs_shading_mesh_params_t *)params;

        if (mesh_params->Decode != NULL)
            gs_free_object(ctx->memory, mesh_params->Decode, "release mesh shading Decode array");
        if (mesh_params->DataSource.data.strm != NULL) {
            s_close_filters(&mesh_params->DataSource.data.strm, mesh_params->DataSource.data.strm->strm);
            gs_free_object(ctx->memory, mesh_params->DataSource.data.strm, "release mesh shading Data Source");
        }
    }

    switch(psh->head.type) {
    case 1:
        if (((gs_shading_Fb_params_t *)&psh->params)->Function != NULL)
            pdfi_free_function(ctx, ((gs_shading_Fb_params_t *)&psh->params)->Function);
        break;
    case 2:
        if (((gs_shading_A_params_t *)&psh->params)->Function != NULL)
            pdfi_free_function(ctx, ((gs_shading_A_params_t *)&psh->params)->Function);
        break;
    case 3:
        if (((gs_shading_R_params_t *)&psh->params)->Function != NULL)
            pdfi_free_function(ctx, ((gs_shading_R_params_t *)&psh->params)->Function);
        break;
    case 4:
        if (((gs_shading_FfGt_params_t *)&psh->params)->Function != NULL)
            pdfi_free_function(ctx, ((gs_shading_FfGt_params_t *)&psh->params)->Function);
        break;
    case 5:
        if (((gs_shading_LfGt_params_t *)&psh->params)->Function != NULL)
            pdfi_free_function(ctx, ((gs_shading_LfGt_params_t *)&psh->params)->Function);
        break;
    case 6:
        if (((gs_shading_Cp_params_t *)&psh->params)->Function != NULL)
            pdfi_free_function(ctx, ((gs_shading_Cp_params_t *)&psh->params)->Function);
        break;
    case 7:
        if (((gs_shading_Tpp_params_t *)&psh->params)->Function != NULL)
            pdfi_free_function(ctx, ((gs_shading_Tpp_params_t *)&psh->params)->Function);
        break;
    default:
        break;
    }
    gs_free_object(ctx->memory, psh, "Free shading, finished");
}

/* Setup for transparency (see pdf_draw.ps/sh) */
static int
pdfi_shading_setup_trans(pdf_context *ctx, pdfi_trans_state_t *state, pdf_obj *Shading)
{
    int code;
    gs_rect bbox, *box = NULL;
    pdf_array *BBox = NULL;
    pdf_dict *shading_dict;

    code = pdfi_dict_from_obj(ctx, Shading, &shading_dict);
    if (code < 0)
        return code;

    code = pdfi_dict_knownget_type(ctx, shading_dict, "BBox", PDF_ARRAY, (pdf_obj **)&BBox);
    if (code < 0)
        goto exit;

    if (code > 0) {
        code = pdfi_array_to_gs_rect(ctx, BBox, &bbox);
        if (code >= 0)
            box = &bbox;
    }

    /* If we didn't get a BBox for the shading, then we need to create one, in order to
     * pass it to the transparency setup, which (potentially, at least, uses it to set
     * up a transparency group.
     * In the basence of anything better, we take the currnet clip, turn that into a path
     * and then get the bounding box of that path. Obviously we don't want to disturb the
     * current path in the graphics state, so we do a gsave/grestore round it.
     */
    if (box == NULL) {
        code = pdfi_gsave(ctx);
        if (code < 0)
            goto exit;

        code = gs_newpath(ctx->pgs);
        if (code < 0)
            goto bbox_error;

        code = gs_clippath(ctx->pgs);
        if (code < 0)
            goto bbox_error;

        code = pdfi_get_current_bbox(ctx, &bbox, false);

bbox_error:
        pdfi_grestore(ctx);

        if (code < 0)
            goto exit;

        box = &bbox;
    }
    code = pdfi_trans_setup(ctx, state, box, TRANSPARENCY_Caller_Other);

 exit:
    pdfi_countdown(BBox);
    return code;
}

int pdfi_shading(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code, code1;
    pdf_name *n = NULL;
    pdf_obj *Shading = NULL;
    gs_shading_t *psh = NULL;
    gs_offset_t savedoffset;
    pdfi_trans_state_t trans_state;

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    if (ctx->text.BlockDepth != 0)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_OPINVALIDINTEXT, "pdfi_shading", NULL);

    if (pdfi_oc_is_off(ctx))
        return 0;

    n = (pdf_name *)ctx->stack_top[-1];
    if (n->type != PDF_NAME)
        return_error(gs_error_typecheck);

    savedoffset = pdfi_tell(ctx->main_stream);
    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_op_q(ctx);
    if (code < 0)
        goto exit1;

    code = pdfi_find_resource(ctx, (unsigned char *)"Shading", n, (pdf_dict *)stream_dict, page_dict,
                              &Shading);
    if (code < 0)
        goto exit2;

    if (Shading->type != PDF_DICT && Shading->type != PDF_STREAM) {
        code = gs_note_error(gs_error_typecheck);
        goto exit2;
    }

    code = pdfi_trans_set_params(ctx);
    if (code < 0)
        goto exit2;

    code = pdfi_shading_build(ctx, stream_dict, page_dict, Shading, &psh);
    if (code < 0)
        goto exit2;

    if (ctx->page.has_transparency) {
        code = pdfi_shading_setup_trans(ctx, &trans_state, Shading);
        if (code < 0)
            goto exit2;
    }

    code = gs_shfill(ctx->pgs, psh);
    if (code < 0) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_BADSHADING, "pdfi_rectpath", (char *)"ERROR: ignoring invalid smooth shading object, output may be incorrect");
        code = 0;
    }

    if (ctx->page.has_transparency) {
        code1 = pdfi_trans_teardown(ctx, &trans_state);
        if (code == 0)
            code = code1;
    }

 exit2:
    if (psh)
        pdfi_shading_free(ctx, psh);

    pdfi_countdown(Shading);
    code1 = pdfi_op_Q(ctx);
    if (code == 0)
        code = code1;
 exit1:
    pdfi_pop(ctx, 1);
    (void)pdfi_loop_detector_cleartomark(ctx);
    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    return code;
}
