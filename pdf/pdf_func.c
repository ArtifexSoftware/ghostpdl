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

/* function creation for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_func.h"

#include "gsdsrc.h"
#include "gsfunc0.h"

/* Returns < 0 for error or the number of entries allocated */
static int func_float_array_from_dict_key(pdf_context *ctx, float **parray, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a;
    float *arr;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj *)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries & 1) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }
    arr = (float *)gs_alloc_byte_array(ctx->memory, a->entries, sizeof(float), "array_from_dict_key");
    *parray = arr;

    for (i=0;i< a->entries;i+=2) {
        if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        if (a->values[i]->type == PDF_INT)
            (*parray)[i] = (float)((pdf_num *)a->values[i])->value.i;
        else
            (*parray)[i] = (float)((pdf_num *)a->values[i])->value.d;
        if (a->values[i+1]->type == PDF_INT)
            (*parray)[i+1] = (float)((pdf_num *)a->values[i+1])->value.i;
        else
            (*parray)[i+1] = (float)((pdf_num *)a->values[i+1])->value.d;

        if ((*parray)[i] > (*parray)[i+1]) {
            pdfi_countdown(a);
            gs_free_const_object(ctx->memory, *parray, "array_from_dict_key");
            return_error(gs_error_rangecheck);
        }
    }
    pdfi_countdown(a);
    return a->entries;
}

static int func_int_array_from_dict_key(pdf_context *ctx, int **parray, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a;
    int *arr;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj *)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries & 1) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }
    arr = (int *)gs_alloc_byte_array(ctx->memory, a->entries, sizeof(int), "array_from_dict_key");
    *parray = arr;

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_INT) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        (*parray)[i] = (int)((pdf_num *)a->values[i])->value.i;
    }
    pdfi_countdown(a);
    return a->entries;
}

static int
pdfi_build_function_0(pdf_context *ctx, const gs_function_params_t * mnDR,
                    pdf_dict *function_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_Sd_params_t params;
    pdf_stream *profile_stream = NULL;
    int code;
    int64_t Length;
    byte *data_source_buffer;

    *(gs_function_params_t *) & params = *mnDR;
    params.Encode = params.Decode = NULL;
    params.pole = NULL;
    params.Size = params.array_step = params.stream_step = NULL;
    params.Order = 0;

    code = pdfi_dict_get_int(ctx, function_dict, "Length", &Length);
    if (code < 0)
        return code;

    pdfi_seek(ctx, ctx->main_stream, function_dict->stream_offset, SEEK_SET);
    code = pdfi_open_memory_stream_from_stream(ctx, (unsigned int)Length, &data_source_buffer, ctx->main_stream, &profile_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, data_source_buffer, profile_stream);
        return code;
    }
    data_source_init_stream(&params.DataSource, profile_stream->s);

    code = pdfi_dict_get_int(ctx, function_dict, "Order", &params.Order);
    if (code < 0 &&  code != gs_error_undefined)
        return code;

    code = pdfi_dict_get_int(ctx, function_dict, "BitsPerSample", &params.BitsPerSample);
    if (code < 0)
        return code;

    code = func_float_array_from_dict_key(ctx, (float **)&params.Encode, function_dict, "Encode");
    if (code < 0) {
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        if (params.Range != NULL)
            gs_free_const_object(ctx->memory, params.Domain, "Range");
        return code;
    }
    if (code != 2 * params.m) {
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        if (params.Range != NULL)
            gs_free_const_object(ctx->memory, params.Domain, "Range");
        return code;
    }

    code = func_float_array_from_dict_key(ctx, (float **)&params.Decode, function_dict, "Decode");
    if (code < 0) {
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        if (params.Range != NULL)
            gs_free_const_object(ctx->memory, params.Domain, "Range");
        return code;
    }
    if (code != 2 * params.n) {
        gs_free_const_object(ctx->memory, params.Decode, "Decode");
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        if (params.Range != NULL)
            gs_free_const_object(ctx->memory, params.Domain, "Range");
        return code;
    }

    code = func_int_array_from_dict_key(ctx, (int **)&params.Size, function_dict, "Size");
    if (code != params.m) {
        gs_free_const_object(ctx->memory, params.Decode, "Decode");
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        if (params.Range != NULL)
            gs_free_const_object(ctx->memory, params.Domain, "Range");
        if (code > 0)
            return_error(gs_error_rangecheck);
        return code;
    }
    code = gs_function_Sd_init(ppfn, &params, ctx->memory);
    if (code < 0) {
        gs_free_const_object(ctx->memory, params.Size, "Size");
        gs_free_const_object(ctx->memory, params.Decode, "Decode");
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        if (params.Range != NULL)
            gs_free_const_object(ctx->memory, params.Domain, "Range");
    }
    return 0;
}

static int pdfi_build_sub_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int Type, code, i;
    pdf_array *Domain = NULL, *Range = NULL, *value = NULL;
    gs_function_params_t params;

    code = pdfi_dict_get_int(ctx, stream_dict, "FunctionType", &Type);
    if (code < 0)
        return code;
    if (Type < 0 || Type > 4 || Type == 1)
        return_error(gs_error_rangecheck);

    params.Domain = 0;
    params.Range = 0;

    /* First gather all the entries common to all functions */
    code = func_float_array_from_dict_key(ctx, (float **)&params.Domain, stream_dict, "Domain");
    if (code < 0)
        return code;

    params.m = code >> 1;

    code = func_float_array_from_dict_key(ctx, (float **)&params.Range, stream_dict, "Range");
    if (code < 0 && code != gs_error_undefined) {
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        return code;
    } else {
        if (code > 0)
            params.n = code >> 1;
    }
    switch(Type) {
        case 0:
            code = pdfi_build_function_0(ctx, &params, stream_dict, 0, ppfn);
            if (code < 0)
                gs_free_const_object(ctx->memory, params.Domain, "Domain");
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        default:
            break;
    }
/*    gs_free_const_object(ctx->memory, params.Domain, "Domain");
    gs_free_const_object(ctx->memory, params.Domain, "Range");*/
    return code;
}

#if 0
if ((code = dict_int_param(op, "Order", 1, 3, 1, &params.Order)) < 0 ||
        (code = dict_int_param(op, "BitsPerSample", 1, 32, 0,
                               &params.BitsPerSample)) < 0 ||
        ((code = fn_build_float_array(op, "Encode", false, true, &params.Encode, mem)) != 2 * params.m && (code != 0 || params.Encode != 0)) ||
        ((code = fn_build_float_array(op, "Decode", false, true, &params.Decode, mem)) != 2 * params.n && (code != 0 || params.Decode != 0))
        ) {
#endif

int pdfi_build_function(pdf_context *ctx, gs_function_t ** ppfn, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return pdfi_build_sub_function(ctx, ppfn, NULL, 0, stream_dict, page_dict);
}
