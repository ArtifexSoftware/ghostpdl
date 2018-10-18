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
#include "pdf_dict.h"
#include "pdf_file.h"

#include "gsdsrc.h"
#include "gsfunc0.h"
#include "gsfunc4.h"

static int pdfi_build_sub_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_dict *stream_dict, pdf_dict *page_dict);

static int
pdfi_parse_type4_func_stream(pdf_context *ctx, pdf_stream *function_stream, char **ops, unsigned int *size)
{
    int code;
    byte c;
    char OpTokenBuffer[9], NumTokenBuffer[16];
    unsigned int OpSize, NumSize, IsReal;

    do {
        code = pdfi_read_bytes(ctx, &c, 1, 1, function_stream);
        if (code < 0)
            break;
        switch(c) {
            case 0x20:
            case 0x0a:
            case 0x0d:
            case 0x09:
                continue;
            case '{':
                break;
            case '}':
                break;
            default:
                if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
                    /* parse a number */
                    NumSize = 1;
                    if (c == '.')
                        IsReal = 1;
                    else
                        IsReal = 0;
                    NumTokenBuffer[0] = c;
                    do {
                        code = pdfi_read_bytes(ctx, &c, 1, 1, function_stream);
                        if (c == '.'){
                            if (IsReal == 1)
                                code = gs_error_syntaxerror;
                            else
                                NumTokenBuffer[NumSize++] = c;
                        } else {
                            if (c >= '0' && 'c' <= '9') {
                                NumTokenBuffer[NumSize++] = c;
                            } else
                                code = gs_error_syntaxerror;
                        }
                    } while (code >= 0);
                } else {
                    /* parse an operator */
                }
                break;
        }
    } while (code >= 0);

    return code;
}

static int
pdfi_build_function_4(pdf_context *ctx, const gs_function_params_t * mnDR,
                    pdf_dict *function_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_PtCr_params_t params;
    pdf_stream *function_stream = NULL, *filtered_function_stream = NULL;
    int code;
    int64_t Length, temp;
    byte *data_source_buffer;
    char *ops;
    unsigned int size;
    bool known = false;
    gs_offset_t savedoffset;

    *(gs_function_params_t *)&params = *mnDR;
    params.ops.data = 0;	/* in case of failure */
    params.ops.size = 0;	/* ditto */

    code = pdfi_dict_get_int(ctx, function_dict, "Length", &Length);
    if (code < 0)
        return code;

    savedoffset = pdfi_tell(ctx->main_stream);
    pdfi_seek(ctx, ctx->main_stream, function_dict->stream_offset, SEEK_SET);
    code = pdfi_open_memory_stream_from_stream(ctx, (unsigned int)Length, &data_source_buffer, ctx->main_stream, &function_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
        return code;
    }

    pdfi_dict_known(function_dict, "F", &known);
    if (!known)
        pdfi_dict_known(function_dict, "Filter", &known);

    /* If this is a compressed stream, we need to decompress it */
    if(known) {
        int decompressed_length = 0;
        byte *Buffer;

        /* This is again complicated by requiring a seekable stream, and the fact that,
         * unlike fonts, there is no Length2 key to tell us how large the uncompressed
         * stream is.
         */
        code = pdfi_filter(ctx, function_dict, function_stream, &filtered_function_stream, false);
        if (code < 0) {
            pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return code;
        }
        do {
            byte b;
            code = pdfi_read_bytes(ctx, &b, 1, 1, filtered_function_stream);
            if (code <= 0)
                break;
            decompressed_length++;
        } while (true);
        pdfi_close_file(ctx, filtered_function_stream);

        Buffer = gs_alloc_bytes(ctx->memory, decompressed_length, "pdfi_build_function_4 (decompression buffer)");
        if (Buffer != NULL) {
            code = srewind(function_stream->s);
            if (code >= 0) {
                code = pdfi_filter(ctx, function_dict, function_stream, &filtered_function_stream, false);
                if (code >= 0) {
                    code = pdfi_read_bytes(ctx, Buffer, 1, decompressed_length, filtered_function_stream);
                    pdfi_close_file(ctx, filtered_function_stream);
                    code = pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
                    if (code >= 0) {
                        data_source_buffer = Buffer;
                        code = pdfi_open_memory_stream_from_memory(ctx, (unsigned int)decompressed_length,
                                                                   data_source_buffer, &function_stream);
                    }
                }
            } else {
                pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
                gs_free_object(ctx->memory, Buffer, "pdfi_build_function_4");
                pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                return code;
            }
        } else {
            pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return_error(gs_error_VMerror);
        }
        if (code < 0) {
            gs_free_object(ctx->memory, Buffer, "pdfi_build_function_4");
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return code;
        }
    }

    code = pdfi_parse_type4_func_stream(ctx, function_stream, &ops, &size);
    pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
    if (code < 0)
        return code;

    params.ops.data = (const byte *)ops;
    params.ops.size = size + 1;
    code = gs_function_PtCr_init(ppfn, &params, ctx->memory);
    if (code < 0)
        gs_function_PtCr_free_params(&params, ctx->memory);

    return code;
}

static int
pdfi_build_function_0(pdf_context *ctx, const gs_function_params_t * mnDR,
                    pdf_dict *function_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_Sd_params_t params;
    pdf_stream *function_stream = NULL, *filtered_function_stream = NULL;
    int code;
    int64_t Length, temp;
    byte *data_source_buffer;
    bool known = false;
    gs_offset_t savedoffset;

    *(gs_function_params_t *) & params = *mnDR;
    params.Encode = params.Decode = NULL;
    params.pole = NULL;
    params.Size = params.array_step = params.stream_step = NULL;
    params.Order = 0;

    code = pdfi_dict_get_int(ctx, function_dict, "Length", &Length);
    if (code < 0)
        return code;

    savedoffset = pdfi_tell(ctx->main_stream);
    pdfi_seek(ctx, ctx->main_stream, function_dict->stream_offset, SEEK_SET);
    code = pdfi_open_memory_stream_from_stream(ctx, (unsigned int)Length, &data_source_buffer, ctx->main_stream, &function_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
        return code;
    }

    pdfi_dict_known(function_dict, "F", &known);
    if (!known)
        pdfi_dict_known(function_dict, "Filter", &known);

    /* If this is a compressed stream, we need to decompress it */
    if(known) {
        int decompressed_length = 0;
        byte *Buffer;

        /* This is again complicated by requiring a seekable stream, and the fact that,
         * unlike fonts, there is no Length2 key to tell us how large the uncompressed
         * stream is.
         */
        code = pdfi_filter(ctx, function_dict, function_stream, &filtered_function_stream, false);
        if (code < 0) {
            pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return code;
        }
        do {
            byte b;
            code = pdfi_read_bytes(ctx, &b, 1, 1, filtered_function_stream);
            if (code <= 0)
                break;
            decompressed_length++;
        } while (true);
        pdfi_close_file(ctx, filtered_function_stream);

        Buffer = gs_alloc_bytes(ctx->memory, decompressed_length, "pdfi_build_function_4 (decompression buffer)");
        if (Buffer != NULL) {
            code = srewind(function_stream->s);
            if (code >= 0) {
                code = pdfi_filter(ctx, function_dict, function_stream, &filtered_function_stream, false);
                if (code >= 0) {
                    code = pdfi_read_bytes(ctx, Buffer, 1, decompressed_length, filtered_function_stream);
                    pdfi_close_file(ctx, filtered_function_stream);
                    code = pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
                    if (code >= 0) {
                        data_source_buffer = Buffer;
                        code = pdfi_open_memory_stream_from_memory(ctx, (unsigned int)decompressed_length,
                                                                   data_source_buffer, &function_stream);
                    }
                }
            } else {
                pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
                gs_free_object(ctx->memory, Buffer, "pdfi_build_function_4");
                pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
                return code;
            }
        } else {
            pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return_error(gs_error_VMerror);
        }
        if (code < 0) {
            gs_free_object(ctx->memory, Buffer, "pdfi_build_function_4");
            pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
            return code;
        }
    }

    data_source_init_stream(&params.DataSource, function_stream->s);

    code = pdfi_dict_get_int(ctx, function_dict, "Order", &temp);
    if (code < 0 &&  code != gs_error_undefined)
        return code;
    if (code == gs_error_undefined)
        params.Order = 1;
    else
        params.Order = (int)temp;

    code = pdfi_dict_get_int(ctx, function_dict, "BitsPerSample", &temp);
    if (code < 0)
        return code;
    params.BitsPerSample = temp;

    code = make_float_array_from_dict(ctx, (float **)&params.Encode, function_dict, "Encode");
    if (code < 0) {
        if (code == gs_error_undefined)
            code = 2 * params.m;
        else
            return code;
    }
    if (code != 2 * params.m) {
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        return_error(gs_error_rangecheck);
    }

    code = make_float_array_from_dict(ctx, (float **)&params.Decode, function_dict, "Decode");
    if (code < 0) {
        if (code == gs_error_undefined)
            code = 2 * params.n;
        else {
            gs_free_const_object(ctx->memory, params.Encode, "Encode");
            return code;
        }
    }
    if (code != 2 * params.n) {
        gs_free_const_object(ctx->memory, params.Decode, "Decode");
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        return_error(gs_error_rangecheck);
    }

    code = make_int_array_from_dict(ctx, (int **)&params.Size, function_dict, "Size");
    if (code != params.m) {
        gs_free_const_object(ctx->memory, params.Decode, "Decode");
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
        if (code > 0)
            return_error(gs_error_rangecheck);
        return code;
    }
    code = gs_function_Sd_init(ppfn, &params, ctx->memory);
    if (code < 0) {
        gs_free_const_object(ctx->memory, params.Size, "Size");
        gs_free_const_object(ctx->memory, params.Decode, "Decode");
        gs_free_const_object(ctx->memory, params.Encode, "Encode");
    }
    return code;
}

static int
pdfi_build_function_2(pdf_context *ctx, const gs_function_params_t * mnDR,
                    pdf_dict *function_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_ElIn_params_t params;
    int code, n0, n1;
    double temp = 0.0;

    *(gs_function_params_t *)&params = *mnDR;
    params.C0 = 0;
    params.C1 = 0;

    code = pdfi_dict_get_number(ctx, function_dict, "N", &temp);
    if (code < 0 &&  code != gs_error_undefined)
        return code;
    params.N = (float)temp;

    code = make_float_array_from_dict(ctx, (float **)&params.C0, function_dict, "C0");
    if (code < 0 && code != gs_error_undefined)
        return code;
    n0 = code;

    code = make_float_array_from_dict(ctx, (float **)&params.C1, function_dict, "C1");
    if (code < 0 && code != gs_error_undefined) {
        gs_function_ElIn_free_params(&params, ctx->memory);
        return code;
    }
    n1 = code;

    if (params.C0 == NULL)
        n0 = 1;
    if (params.C1 == NULL)
        n1 = 1;
    if (params.Range == 0)
        params.n = n0;		/* either one will do */
    if (n0 != n1 || n0 != params.n) {
        gs_function_ElIn_free_params(&params, ctx->memory);
        return_error(gs_error_rangecheck);
    }
    code = gs_function_ElIn_init(ppfn, &params, ctx->memory);
    if (code < 0) {
        gs_function_ElIn_free_params(&params, ctx->memory);
        return code;
    }
    return 0;
}

static int
pdfi_build_function_3(pdf_context *ctx, const gs_function_params_t * mnDR,
                    pdf_dict *function_dict, const float *shading_domain, int num_inputs, pdf_dict *page_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_1ItSg_params_t params;
    int code, i;
    pdf_array *Functions;
    gs_function_t **ptr;

    *(gs_function_params_t *) & params = *mnDR;
    params.Functions = 0;
    params.Bounds = 0;
    params.Encode = 0;

    code = pdfi_dict_get(ctx, function_dict, "Functions", (pdf_obj **)&Functions);
    if (code < 0)
        return code;
    if (Functions->type != PDF_ARRAY) {
        pdf_indirect_ref *r = (pdf_indirect_ref *)Functions;

        if (Functions->type != PDF_INDIRECT)
            return_error(gs_error_typecheck);

        if (ctx->loop_detection == NULL) {
            pdfi_init_loop_detector(ctx);
            pdfi_loop_detector_mark(ctx);
        } else {
            pdfi_loop_detector_mark(ctx);
        }
        code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&Functions);
        pdfi_countdown(r);
        pdfi_loop_detector_cleartomark(ctx);
        if (code < 0)
            return code;

        if (Functions->type != PDF_ARRAY) {
            pdfi_countdown(Functions);
            return_error(gs_error_typecheck);
        }
    }

    params.k = Functions->entries;
    code = alloc_function_array(params.k, &ptr, ctx->memory);
    if (code < 0) {
        pdfi_countdown(Functions);
        return code;
    }
    params.Functions = (const gs_function_t * const *)ptr;

    for (i = 0; i < params.k; ++i) {
        pdf_obj * rsubfn = NULL;

        code = pdfi_array_get((pdf_array *)Functions, (int64_t)i, &rsubfn);
        if (code < 0)
            return  code;

        if (rsubfn->type == PDF_INDIRECT) {
            pdf_indirect_ref *r = (pdf_indirect_ref *)rsubfn;

            pdfi_loop_detector_mark(ctx);
            code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&rsubfn);
            pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(r);
            if (code < 0) {
                pdfi_countdown(Functions);
                return code;
            }
        }
        if (rsubfn->type != PDF_DICT) {
            pdfi_countdown(rsubfn);
            pdfi_countdown(Functions);
            return_error(gs_error_typecheck);
        }
        code = pdfi_build_sub_function(ctx, &ptr[i], shading_domain, num_inputs, (pdf_dict *)rsubfn, page_dict);
        pdfi_countdown(rsubfn);
        if (code < 0) {
            pdfi_countdown(Functions);
            return code;
        }
    }
    pdfi_countdown(Functions);

    code = make_float_array_from_dict(ctx, (float **)&params.Bounds, function_dict, "Bounds");
    if (code < 0){
        gs_function_1ItSg_free_params(&params, ctx->memory);
        return code;
    }

    code = make_float_array_from_dict(ctx, (float **)&params.Encode, function_dict, "Encode");
    if (code < 0) {
        gs_function_1ItSg_free_params(&params, ctx->memory);
        return code;
    }
    if (code != 2 * params.k) {
        gs_function_1ItSg_free_params(&params, ctx->memory);
        return_error(gs_error_rangecheck);
    }

    if (params.Range == 0)
        params.n = params.Functions[0]->params.n;
    code = gs_function_1ItSg_init(ppfn, &params, ctx->memory);
    if (code < 0)
        gs_function_1ItSg_free_params(&params, ctx->memory);

    return code;
}

static int pdfi_build_sub_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code, i;
    int64_t Type;
    gs_function_params_t params;

    code = pdfi_dict_get_int(ctx, stream_dict, "FunctionType", &Type);
    if (code < 0)
        return code;
    if (Type < 0 || Type > 4 || Type == 1)
        return_error(gs_error_rangecheck);

    params.Domain = 0;
    params.Range = 0;

    /* First gather all the entries common to all functions */
    code = make_float_array_from_dict(ctx, (float **)&params.Domain, stream_dict, "Domain");
    if (code < 0)
        return code;

    if (code & 1) {
        gs_free_const_object(ctx->memory, params.Domain, "pdfi_build_sub_function");
        return_error(gs_error_rangecheck);
    }
    for (i=0;i<code;i+=2) {
        if (params.Domain[i] > params.Domain[i+1]) {
            gs_free_const_object(ctx->memory, params.Domain, "pdfi_build_sub_function");
            return_error(gs_error_rangecheck);
        }
    }
    if (shading_domain) {
        if (num_inputs != code >> 1) {
            gs_free_const_object(ctx->memory, params.Domain, "pdfi_build_sub_function");
            return_error(gs_error_rangecheck);
        }
        for (i=0;i<2*num_inputs;i+=2) {
            if (params.Domain[i] > shading_domain[i] || params.Domain[i+1] < shading_domain[i+1]) {
                gs_free_const_object(ctx->memory, params.Domain, "pdfi_build_sub_function");
                return_error(gs_error_rangecheck);
            }
        }
    }

    params.m = code >> 1;

    code = make_float_array_from_dict(ctx, (float **)&params.Range, stream_dict, "Range");
    if (code < 0 && code != gs_error_undefined) {
        gs_free_const_object(ctx->memory, params.Domain, "Domain");
        return code;
    } else {
        if (code > 0)
            params.n = code >> 1;
        else
            params.n = 0;
    }
    switch(Type) {
        case 0:
            code = pdfi_build_function_0(ctx, &params, stream_dict, 0, ppfn);
            if (code < 0) {
                gs_free_const_object(ctx->memory, params.Domain, "Domain");
                gs_free_const_object(ctx->memory, params.Range, "Range");
            }
            break;
        case 2:
            code = pdfi_build_function_2(ctx, &params, stream_dict, 0, ppfn);
            if (code < 0) {
                gs_free_const_object(ctx->memory, params.Domain, "Domain");
                gs_free_const_object(ctx->memory, params.Range, "Range");
            }
            break;
        case 3:
            code = pdfi_build_function_3(ctx, &params, stream_dict, shading_domain,  num_inputs, page_dict, 0, ppfn);
            if (code < 0) {
                gs_free_const_object(ctx->memory, params.Domain, "Domain");
                gs_free_const_object(ctx->memory, params.Range, "Range");
            }
            break;
        case 4:
            break;
        default:
            break;
    }
    return code;
}

int pdfi_build_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return pdfi_build_sub_function(ctx, ppfn, shading_domain, num_inputs, stream_dict, page_dict);
}
