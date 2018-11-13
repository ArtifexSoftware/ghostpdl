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

/* Shading operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_shading.h"
#include "pdf_dict.h"
#include "pdf_func.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
#include "pdf_colour.h"

#include "gxshade.h"
#include "gsptype2.h"
#include "gsfunc0.h"    /* For gs_function */

static int pdfi_build_shading_function(pdf_context *ctx, gs_function_t **ppfn, const float *shading_domain, int num_inputs, pdf_dict *shading_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *o;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_dict_get(ctx, shading_dict, "Function", &o);
    if (code < 0) {
        (void)pdfi_loop_detector_cleartomark(ctx);
        return code;
    }

    if (o->type != PDF_DICT) {
        uint size;
        pdf_obj *rsubfn;
        gs_function_t **Functions;
        int64_t i;
        gs_function_AdOt_params_t params;

        if (o->type != PDF_ARRAY) {
            (void)pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }
        size = ((pdf_array *)o)->entries;

        if (size == 0) {
            (void)pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(o);
            return_error(gs_error_rangecheck);
        }
        code = alloc_function_array(size, &Functions, ctx->memory);
        if (code < 0) {
            (void)pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(o);
            return code;
        }
        for (i = 0; i < size; ++i) {
            pdf_obj * rsubfn = NULL;

            code = pdfi_array_get((pdf_array *)o, i, &rsubfn);
            if (rsubfn->type == PDF_INDIRECT) {
                pdf_indirect_ref *r = (pdf_indirect_ref *)rsubfn;

                (void)pdfi_loop_detector_mark(ctx);
                code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&rsubfn);
                (void)pdfi_loop_detector_cleartomark(ctx);
                pdfi_countdown(r);
                if (code < 0) {
                    (void)pdfi_loop_detector_cleartomark(ctx);
                    pdfi_countdown(o);
                    return code;
                }
            }
            if (rsubfn->type != PDF_DICT) {
                (void)pdfi_loop_detector_cleartomark(ctx);
                pdfi_countdown(rsubfn);
                pdfi_countdown(o);
                return_error(gs_error_typecheck);
            }
            code = pdfi_build_function(ctx, &Functions[i], shading_domain, num_inputs, (pdf_dict *)rsubfn, page_dict);
            if (code < 0) {
                (void)pdfi_loop_detector_cleartomark(ctx);
                pdfi_countdown(rsubfn);
                pdfi_countdown(o);
                break;
            }
        }
        params.m = num_inputs;
        params.Domain = 0;
        params.n = size;
        params.Range = 0;
        params.Functions = (const gs_function_t * const *)Functions;
        if (code >= 0)
            code = gs_function_AdOt_init(ppfn, &params, ctx->memory);
        if (code < 0)
            gs_function_AdOt_free_params(&params, ctx->memory);
    } else
        code = pdfi_build_function(ctx, ppfn, shading_domain, num_inputs, (pdf_dict *)o, page_dict);

    (void)pdfi_loop_detector_cleartomark(ctx);
    pdfi_countdown(o);
    return code;
}

static int pdfi_shading1(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    int code, i;
    gs_shading_Fb_params_t params;
    static const float default_Domain[4] = {0, 1, 0, 1};

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
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    gs_shading_A_params_t params;
    static const float default_Domain[2] = {0, 1};
    int code, i;

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
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *o = NULL;
    gs_shading_R_params_t params;
    static const float default_Domain[2] = {0, 1};
    int code, i;

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
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int num_decode = 4, code;
    gs_offset_t savedoffset;
    int64_t Length;
    byte *data_source_buffer;
    bool known = false;
    pdf_stream *function_stream = NULL, *filtered_function_stream = NULL;
    int64_t i;

    if (shading_dict->stream_offset == 0)
        return_error(gs_error_typecheck);

    code = pdfi_dict_get_int(ctx, shading_dict, "Length", &Length);
    if (code < 0)
        return code;

    savedoffset = pdfi_tell(ctx->main_stream);
    code = pdfi_seek(ctx, ctx->main_stream, shading_dict->stream_offset, SEEK_SET);
    if (code < 0)
        return code;

    code = pdfi_open_memory_stream_from_filtered_stream(ctx, stream_dict, Length, &data_source_buffer, ctx->main_stream, &function_stream);
    if (code < 0) {
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    data_source_init_stream(&params->DataSource, function_stream->s);

    code = pdfi_seek(ctx, function_stream, 0, SEEK_SET);
    if (code < 0)
        return code;
    code = pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    if (code < 0)
        return code;

    code = pdfi_build_shading_function(ctx, &params->Function, (const float *)NULL, 1, (pdf_dict *)shading_dict, page_dict);
    if (code < 0 && code != gs_error_undefined)
        return code;

    code = pdfi_dict_get_int(ctx, shading_dict, "BitsPerCoordinate", &i);
    if (code < 0)
        return code;

    if (i != 1 && i != 2 && i != 4 && i != 8 && i != 12 && i != 16 && i != 24 && i != 32)
        return_error(gs_error_rangecheck);

    params->BitsPerCoordinate = i;

    code = pdfi_dict_get_int(ctx, shading_dict, "BitsPerComponent", &i);
    if (code < 0)
        return code;

    if (i != 1 && i != 2 && i != 4 && i != 8 && i != 12 && i != 16)
        return_error(gs_error_rangecheck);

    params->BitsPerComponent = i;

    if (params->Function != NULL)
        num_decode += 1;
    else
        num_decode += gs_color_space_num_components(params->ColorSpace) * 2;

    params->Decode = (float *) gs_alloc_byte_array(ctx->memory, num_decode, sizeof(float),
                            "build_mesh_shading");

    code = fill_float_array_from_dict(ctx, (float *)params->Decode, num_decode, shading_dict, "Decode");
    if (code < 0) {
        gs_free_object(ctx->memory, params->Decode, "Decode");
        return code;
    }
    return 0;
}

static int pdfi_shading4(pdf_context *ctx, gs_shading_params_t *pcommon,
                  gs_shading_t **ppsh,
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_FfGt_params_t params;
    int code;
    int64_t i;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, shading_dict, stream_dict, page_dict);
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
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_LfGt_params_t params;
    int code;
    int64_t i;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, shading_dict, stream_dict, page_dict);
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
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_Cp_params_t params;
    int code;
    int64_t i;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, shading_dict, stream_dict, page_dict);
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
                  pdf_dict *shading_dict, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    gs_shading_Tpp_params_t params;
    int code;
    int64_t i;

    memset(&params, 0, sizeof(params));
    *(gs_shading_params_t *)&params = *pcommon;

    code = pdfi_build_mesh_shading(ctx, (gs_shading_mesh_params_t *)&params, shading_dict, stream_dict, page_dict);
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
    pdf_array *a;
    pdf_bool b;

    if (num_comp < 0)	/* Pattern color space */
        return_error(gs_error_typecheck);

    params->ColorSpace = pcs;
    rc_increment_cs(pcs);

    code = pdfi_dict_get_type(ctx, shading_dict, "Background", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0 && code != gs_error_undefined)
        return code;

    if (code >= 0) {
        gs_client_color *pcc;
        uint64_t i;

        if (a->size < num_comp) {
            pdfi_countdown((pdf_obj *)a);
            return_error(gs_error_rangecheck);
        }

        pcc = gs_alloc_struct(ctx->memory, gs_client_color, &st_client_color, "get_shading_common");
        if (pcc == 0) {
            pdfi_countdown((pdf_obj *)a);
            return_error(gs_error_VMerror);
        }

        pcc->pattern = 0;
        params->Background = pcc;

        for(i=0;i<num_comp;i++) {
            code = pdfi_array_get_number(ctx, a, i, &pcc->paint.values[i]);
            if (code < 0) {
                pdfi_countdown((pdf_obj *)a);
                gs_free_object(ctx->memory, params->Background, "Background");
                return code;
            }
        }
        pdfi_countdown((pdf_obj *)a);
    }

    code = pdfi_dict_get_type(ctx, shading_dict, "BBox", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0 && code != gs_error_undefined) {
        gs_free_object(ctx->memory, params->Background, "Background");
        return code;
    }

    if (code >= 0) {
        double box[4];
        uint64_t i;

        if (a->size < 4) {
            gs_free_object(ctx->memory, params->Background, "Background");
            return_error(gs_error_rangecheck);
        }

        for(i=0;i<4;i++) {
            code = pdfi_array_get_number(ctx, a, i, &box[i]);
            if (code < 0) {
                pdfi_countdown((pdf_obj *)a);
                gs_free_object(ctx->memory, params->Background, "Background");
                return code;
            }
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

    code = pdfi_dict_get_bool(ctx, shading_dict, "AntiAlias", &params->AntiAlias);
    if (code < 0 && code != gs_error_undefined) {
        gs_free_object(ctx->memory, params->Background, "Background");
        return code;
    }
    return 0;
}

int pdfi_shading(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code, code1;
    pdf_name *n = NULL;
    pdf_obj *o, *o1, *cspace;
    gs_shading_params_t params;
    gs_shading_t *psh;
    gs_offset_t savedoffset;

    memset(&params, 0, sizeof(params));

    savedoffset = pdfi_tell(ctx->main_stream);
    code = pdfi_loop_detector_mark(ctx);

    if (ctx->stack_top - ctx->stack_bot < 1) {
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return_error(gs_error_stackunderflow);
    }

    n = (pdf_name *)ctx->stack_top[-1];
    if (n->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return_error(gs_error_typecheck);
    }

    code = pdfi_find_resource(ctx, (unsigned char *)"Shading", n, stream_dict, page_dict, &o);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        if (ctx->pdfstoponerror)
            return code;
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return 0;
    }
    if (o->type != PDF_DICT) {
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return_error(gs_error_typecheck);
    }

    code = gs_gsave(ctx->pgs);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    params.ColorSpace = 0;
    params.cie_joint_caches = 0;
    params.Background = 0;
    params.have_BBox = 0;
    params.AntiAlias = 0;

    code = pdfi_dict_get(ctx, (pdf_dict *)o, "ColorSpace", &cspace);
    if (code < 0) {
        (void)gs_grestore(ctx->pgs);
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    code = pdfi_setcolorspace(ctx, cspace, stream_dict, page_dict);
    if (code < 0) {
        (void)gs_grestore(ctx->pgs);
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    code = get_shading_common(ctx, (pdf_dict *)o, &params);
    if (code < 0) {
        (void)gs_grestore(ctx->pgs);
        pdfi_pop(ctx, 1);
        (void)pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    /* Collect parameters common to all shading types. */
    {
    }

    code = pdfi_dict_get(ctx, (pdf_dict *)o, "ShadingType", &o1);
    if (code == 0) {
        if (o1->type == PDF_INT) {
            switch(((pdf_num *)o1)->value.i){
                case 1:
                    code = pdfi_shading1(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 2:
                    code = pdfi_shading2(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 3:
                    code = pdfi_shading3(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 4:
                    code = pdfi_shading4(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 5:
                    code = pdfi_shading5(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 6:
                    code = pdfi_shading6(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                case 7:
                    code = pdfi_shading7(ctx, &params, &psh, (pdf_dict *)o, stream_dict, page_dict);
                    break;
                default:
                    code = gs_error_rangecheck;
                    break;
            }
            pdfi_countdown(o1);
        } else {
            pdfi_countdown(o1);
            code = gs_error_typecheck;
        }
    }

    pdfi_countdown(o);
    pdfi_pop(ctx, 1);
    (void)pdfi_loop_detector_cleartomark(ctx);

    if (code >= 0) {
        code = gs_shfill(ctx->pgs, psh);
    }

    code1 = gs_grestore(ctx->pgs);
    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    if (code1 == 0)
        return code;
    return code1;
}
