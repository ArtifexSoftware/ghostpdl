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
#include "pdf_array.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"

#include "gsdsrc.h"
#include "gsfunc0.h"
#include "gsfunc3.h"
#include "gsfunc4.h"
#include "stream.h"

static int pdfi_build_sub_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_dict *stream_dict, pdf_dict *page_dict);

#define NUMBERTOKENSIZE 16
#define OPTOKENSIZE 9
#define TOKENBUFFERSIZE NUMBERTOKENSIZE + 1
#define NUMOPS 42

typedef struct op_struct {
    unsigned char length;
    gs_PtCr_opcode_t code;
    unsigned char op[8];
}op_struct_t;

static const op_struct_t ops_table[] = {
    {(unsigned char)2, PtCr_eq, "eq"},
    {(unsigned char)2, PtCr_ge, "ge"},
    {(unsigned char)2, PtCr_gt, "gt"},
    {(unsigned char)2, PtCr_if, "if"},
    {(unsigned char)2, PtCr_le, "le"},
    {(unsigned char)2, PtCr_ln, "ln"},
    {(unsigned char)2, PtCr_lt, "lt"},
    {(unsigned char)2, PtCr_ne, "ne"},
    {(unsigned char)2, PtCr_or, "or"},

    {(unsigned char)3, PtCr_abs, "abs"},
    {(unsigned char)3, PtCr_add, "add"},
    {(unsigned char)3, PtCr_and, "and"},
    {(unsigned char)3, PtCr_cos, "cos"},
    {(unsigned char)3, PtCr_cvi, "cvi"},
    {(unsigned char)3, PtCr_cvr, "cvr"},
    {(unsigned char)3, PtCr_div, "div"},
    {(unsigned char)3, PtCr_dup, "dup"},
    {(unsigned char)3, PtCr_exp, "exp"},
    {(unsigned char)3, PtCr_log, "log"},
    {(unsigned char)3, PtCr_mul, "mul"},
    {(unsigned char)3, PtCr_mod, "mod"},
    {(unsigned char)3, PtCr_neg, "neg"},
    {(unsigned char)3, PtCr_not, "not"},
    {(unsigned char)3, PtCr_pop, "pop"},
    {(unsigned char)3, PtCr_sin, "sin"},
    {(unsigned char)3, PtCr_sub, "sub"},
    {(unsigned char)3, PtCr_xor, "xor"},

    {(unsigned char)4, PtCr_atan, "atan"},
    {(unsigned char)4, PtCr_copy, "copy"},
    {(unsigned char)4, PtCr_exch, "exch"},
    {(unsigned char)4, PtCr_idiv, "idiv"},
    {(unsigned char)4, PtCr_roll, "roll"},
    {(unsigned char)4, PtCr_sqrt, "sqrt"},
    {(unsigned char)4, PtCr_true, "true"},

    {(unsigned char)5, PtCr_false, "false"},
    {(unsigned char)5, PtCr_floor, "floor"},
    {(unsigned char)5, PtCr_index, "index"},
    {(unsigned char)5, PtCr_round, "round"},

    {(unsigned char)6, PtCr_else, "ifelse"},

    {(unsigned char)7, PtCr_ceiling, "ceiling"},

    {(unsigned char)8, PtCr_bitshift, "bitshift"},
    {(unsigned char)8, PtCr_truncate, "truncate"},
};

/* Fix up an if or ifelse forward reference. */
static void
psc_fixup(byte *p, byte *to)
{
    int skip = to - (p + 3);

    p[1] = (byte)(skip >> 8);
    p[2] = (byte)skip;
}

/* Store an int in the  buffer */
static int
put_int(byte **p, int n) {
   if (n == (byte)n) {
       if (*p) {
          (*p)[0] = PtCr_byte;
          (*p)[1] = (byte)n;
          *p += 2;
       }
       return 2;
   } else {
       if (*p) {
          **p = PtCr_int;
          memcpy(*p + 1, &n, sizeof(int));
          *p += sizeof(int) + 1;
       }
       return (sizeof(int) + 1);
   }
}

/* Store a float in the  buffer */
static int
put_float(byte **p, float n) {
   if (*p) {
      **p = PtCr_float;
      memcpy(*p + 1, &n, sizeof(float));
      *p += sizeof(float) + 1;
   }
   return (sizeof(float) + 1);
}

static int
pdfi_parse_type4_func_stream(pdf_context *ctx, pdf_stream *function_stream, int depth, byte *ops, unsigned int *size)
{
    int code;
    byte c;
    char TokenBuffer[16];
    unsigned int Size, IsReal;
    bool clause = false;
    byte *p = (ops ? ops + *size : NULL);

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
                if (depth == 0) {
                    depth++;
                } else {
                    /* recursion, move on 3 bytes, and parse the sub level */
                    if (depth++ == MAX_PSC_FUNCTION_NESTING)
                        return_error (gs_error_syntaxerror);
                    *size += 3;
                    code = pdfi_parse_type4_func_stream(ctx, function_stream, depth + 1, ops, size);
                    if (code < 0)
                        return code;
                    if (p) {
                        if (clause == false) {
                            *p = (byte)PtCr_if;
                            psc_fixup(p, ops + *size);
                            clause = true;
                        } else {
                            *p = (byte)PtCr_else;
                            psc_fixup(p, ops + *size);
                            clause = false;
                        }
                        p = ops + *size;
                    }
                }
                break;
            case '}':
                return *size;
                break;
            default:
                if (clause)
                    clause = false;
                if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
                    /* parse a number */
                    Size = 1;
                    if (c == '.')
                        IsReal = 1;
                    else
                        IsReal = 0;
                    TokenBuffer[0] = c;
                    do {
                        code = pdfi_read_bytes(ctx, &c, 1, 1, function_stream);
                        if (code < 0)
                            return code;
                        if (code == 0)
                            return_error(gs_error_syntaxerror);

                        if (c == '.'){
                            if (IsReal == 1)
                                code = gs_error_syntaxerror;
                            else {
                                TokenBuffer[Size++] = c;
                                IsReal = 1;
                            }
                        } else {
                            if (c >= '0' && c <= '9') {
                                TokenBuffer[Size++] = c;
                            } else
                                break;
                        }
                        if (Size > NUMBERTOKENSIZE)
                            return_error(gs_error_syntaxerror);
                    } while (code >= 0);
                    TokenBuffer[Size] = 0x00;
                    pdfi_unread(ctx, function_stream, &c, 1);
                    if (IsReal == 1) {
                        *size += put_float(&p, atof(TokenBuffer));
                    } else {
                        *size += put_int(&p, atoi(TokenBuffer));
                    }
                } else {
                    int i, NumOps = sizeof(ops_table) / sizeof(op_struct_t);
                    op_struct_t *Op;

                    /* parse an operator */
                    Size = 1;
                    TokenBuffer[0] = c;
                    do {
                        code = pdfi_read_bytes(ctx, &c, 1, 1, function_stream);
                        if (code < 0)
                            return code;
                        if (code == 0)
                            return_error(gs_error_syntaxerror);
                        if (c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d || c == '{' || c == '}')
                            break;
                        TokenBuffer[Size++] = c;
                        if (Size > OPTOKENSIZE)
                            return_error(gs_error_syntaxerror);
                    } while(code >= 0);
                    TokenBuffer[Size] = 0x00;
                    pdfi_unread(ctx, function_stream, &c, 1);
                    for (i=0;i < NumOps;i++) {
                        Op = (op_struct_t *)&ops_table[i];
                        if (Op->length < Size)
                            continue;

                        if (Op->length > Size)
                            return_error(gs_error_undefined);

                        if (memcmp(Op->op, TokenBuffer, Size) == 0)
                            break;
                    }
                    if (i > NumOps)
                        return_error(gs_error_syntaxerror);
                    if (p == NULL)
                        (*size)++;
                    else {
                        if (Op->code != PtCr_else && Op->code != PtCr_if) {
                            (*size)++;
                            *p++ = Op->code;
                        }
                    }
                }
                break;
        }
    } while (code >= 0);

    return code;
}

static int
pdfi_build_function_4(pdf_context *ctx, gs_function_params_t * mnDR,
                    pdf_dict *function_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_PtCr_params_t params;
    pdf_stream *function_stream = NULL;
    int code;
    int64_t Length;
    byte *data_source_buffer;
    byte *ops = NULL;
    unsigned int size;
    gs_offset_t savedoffset;

    memset(&params, 0x00, sizeof(gs_function_PtCr_params_t));
    *(gs_function_params_t *)&params = *mnDR;
    params.ops.data = 0;	/* in case of failure */
    params.ops.size = 0;	/* ditto */

    code = pdfi_dict_get_int(ctx, function_dict, "Length", &Length);
    if (code < 0)
        return code;

    savedoffset = pdfi_tell(ctx->main_stream);
    code = pdfi_seek(ctx, ctx->main_stream, function_dict->stream_offset, SEEK_SET);
    if (code < 0)
        return code;

    code = pdfi_open_memory_stream_from_filtered_stream(ctx, function_dict, (unsigned int)Length, &data_source_buffer, ctx->main_stream, &function_stream);
    if (code < 0)
        goto function_4_error;

    size = 0;
    code = pdfi_parse_type4_func_stream(ctx, function_stream, 0, NULL, &size);
    if (code < 0)
        goto function_4_error;

    ops = gs_alloc_string(ctx->memory, size + 1, "pdfi_build_function_4(ops)");
    if (ops == NULL) {
        code = gs_error_VMerror;
        goto function_4_error;
    }

    code = pdfi_seek(ctx, function_stream, 0, SEEK_SET);
    if (code < 0)
        goto function_4_error;

    size = 0;
    code = pdfi_parse_type4_func_stream(ctx, function_stream, 0, ops, &size);
    if (code < 0)
        goto function_4_error;
    ops[size] = PtCr_return;

    code = pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
    if (code < 0) {
        function_stream = NULL;
        goto function_4_error;
    }

    params.ops.data = (const byte *)ops;
    params.ops.size = size + 1;
    code = gs_function_PtCr_init(ppfn, &params, ctx->memory);
    if (code < 0)
        goto function_4_error;

    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
    return 0;

function_4_error:
    if (function_stream)
        (void)pdfi_close_memory_stream(ctx, data_source_buffer, function_stream);
    (void)pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);

    gs_function_PtCr_free_params(&params, ctx->memory);
    if (ops)
        gs_free_const_string(ctx->memory, ops, size, "pdfi_build_function_4(ops)");
    mnDR->Range = NULL;
    mnDR->Domain = NULL;
    return code;
}

static int
pdfi_build_function_0(pdf_context *ctx, gs_function_params_t * mnDR,
                    pdf_dict *function_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_Sd_params_t params;
    pdf_stream *function_stream = NULL;
    int code;
    int64_t Length, temp;
    byte *data_source_buffer;
    gs_offset_t savedoffset;

    memset(&params, 0x00, sizeof(gs_function_params_t));
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

    Length = pdfi_open_memory_stream_from_filtered_stream(ctx, function_dict, (unsigned int)Length, &data_source_buffer, ctx->main_stream, &function_stream);
    if (code < 0) {
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return code;
    }

    data_source_init_stream(&params.DataSource, function_stream->s);

    pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);

    /* We need to clear up the PDF stream, but leave the underlying stream alone, that's now
     * pointed to by the params.DataSource member.
     */
    gs_free_object(ctx->memory, function_stream, "discard memory stream(pdf_stream)");

    code = pdfi_dict_get_int(ctx, function_dict, "Order", &temp);
    if (code < 0 &&  code != gs_error_undefined)
        goto function_0_error;
    if (code == gs_error_undefined)
        params.Order = 1;
    else
        params.Order = (int)temp;

    code = pdfi_dict_get_int(ctx, function_dict, "BitsPerSample", &temp);
    if (code < 0)
        goto function_0_error;
    params.BitsPerSample = temp;

    code = make_float_array_from_dict(ctx, (float **)&params.Encode, function_dict, "Encode");
    if (code < 0) {
        if (code == gs_error_undefined)
            code = 2 * params.m;
        else
            goto function_0_error;
    }
    if (code != 2 * params.m)
        goto function_0_error;

    code = make_float_array_from_dict(ctx, (float **)&params.Decode, function_dict, "Decode");
    if (code < 0) {
        if (code == gs_error_undefined)
            code = 2 * params.n;
        else
            goto function_0_error;
    }
    if (code != 2 * params.n) {
        code = gs_error_rangecheck;
        goto function_0_error;
    }

    code = make_int_array_from_dict(ctx, (int **)&params.Size, function_dict, "Size");
    if (code != params.m) {
        if (code > 0)
            code = gs_error_rangecheck;
        goto function_0_error;
    }
    /* check the stream has enough data */
    {
        unsigned int i;
        uint64_t inputs = 1, samples = 0;

        for (i=0;i<params.m;i++) {
            inputs *= params.Size[i];
        }
        samples = params.n * params.BitsPerSample;
        samples *= inputs;
        samples = samples >> 3;
        if (samples > Length) {
            code = gs_error_rangecheck;
            goto function_0_error;
        }
    }

    code = gs_function_Sd_init(ppfn, &params, ctx->memory);
    if (code < 0)
        goto function_0_error;
    return 0;

function_0_error:
    sclose(params.DataSource.data.strm);
    params.DataSource.data.strm = NULL;
    gs_function_Sd_free_params(&params, ctx->memory);
    /* These are freed by gs_function_Sd_free_params, since we copied
     * the poitners, we must NULL the originals, so that we don't double free.
     */
    mnDR->Range = NULL;
    mnDR->Domain = NULL;
    return code;
}

static int
pdfi_build_function_2(pdf_context *ctx, gs_function_params_t * mnDR,
                    pdf_dict *function_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_ElIn_params_t params;
    int code, n0, n1;
    double temp = 0.0;

    memset(&params, 0x00, sizeof(gs_function_params_t));
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
    if (code < 0 && code != gs_error_undefined)
        goto function_2_error;

    n1 = code;
    if (params.C0 == NULL)
        n0 = 1;
    if (params.C1 == NULL)
        n1 = 1;
    if (params.Range == 0)
        params.n = n0;		/* either one will do */
    if (n0 != n1 || n0 != params.n)
        goto function_2_error;

    code = gs_function_ElIn_init(ppfn, &params, ctx->memory);
    if (code < 0)
        goto function_2_error;

    return 0;

function_2_error:
    gs_function_ElIn_free_params(&params, ctx->memory);
    mnDR->Range = NULL;
    mnDR->Domain = NULL;
    return code;
}

static int
pdfi_build_function_3(pdf_context *ctx, gs_function_params_t * mnDR,
                    pdf_dict *function_dict, const float *shading_domain, int num_inputs, pdf_dict *page_dict, int depth, gs_function_t ** ppfn)
{
    gs_function_1ItSg_params_t params;
    int code, i;
    pdf_array *Functions = NULL;
    gs_function_t **ptr = NULL;

    memset(&params, 0x00, sizeof(gs_function_params_t));
    *(gs_function_params_t *) &params = *mnDR;
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

        code = pdfi_loop_detector_mark(ctx);
        if (code < 0) {
            pdfi_countdown(r);
            return code;
        }

        code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&Functions);
        pdfi_countdown(r);
        (void)pdfi_loop_detector_cleartomark(ctx);
        if (code < 0)
            return code;

        if (Functions->type != PDF_ARRAY) {
            pdfi_countdown(Functions);
            return_error(gs_error_typecheck);
        }
    }

    params.k = Functions->entries;
    code = alloc_function_array(params.k, &ptr, ctx->memory);
    if (code < 0)
        goto function_3_error;

    params.Functions = (const gs_function_t * const *)ptr;

    for (i = 0; i < params.k; ++i) {
        pdf_obj * rsubfn = NULL;

        code = pdfi_array_get((pdf_array *)Functions, (int64_t)i, &rsubfn);
        if (code < 0)
            goto function_3_error;

        if (rsubfn->type == PDF_INDIRECT) {
            pdf_indirect_ref *r = (pdf_indirect_ref *)rsubfn;

            pdfi_loop_detector_mark(ctx);
            code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, (pdf_obj **)&rsubfn);
            pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(r);
            if (code < 0)
                goto function_3_error;
        }
        if (rsubfn->type != PDF_DICT) {
            pdfi_countdown(rsubfn);
            goto function_3_error;
        }
        code = pdfi_build_sub_function(ctx, &ptr[i], shading_domain, num_inputs, (pdf_dict *)rsubfn, page_dict);
        pdfi_countdown(rsubfn);
        if (code < 0)
            goto function_3_error;
    }

    code = make_float_array_from_dict(ctx, (float **)&params.Bounds, function_dict, "Bounds");
    if (code < 0)
        goto function_3_error;

    code = make_float_array_from_dict(ctx, (float **)&params.Encode, function_dict, "Encode");
    if (code < 0)
        goto function_3_error;

    if (code != 2 * params.k)
        goto function_3_error;

    if (params.Range == 0)
        params.n = params.Functions[0]->params.n;

    code = gs_function_1ItSg_init(ppfn, &params, ctx->memory);
    if (code < 0)
        goto function_3_error;

    pdfi_countdown(Functions);
    return 0;

function_3_error:
    pdfi_countdown(Functions);
    gs_function_1ItSg_free_params(&params, ctx->memory);
    mnDR->Range = NULL;
    mnDR->Domain = NULL;
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

    memset(&params, 0x00, sizeof(gs_function_params_t));

    /* First gather all the entries common to all functions */
    code = make_float_array_from_dict(ctx, (float **)&params.Domain, stream_dict, "Domain");
    if (code < 0)
        return code;

    if (code & 1)
        goto sub_function_error;

    for (i=0;i<code;i+=2) {
        if (params.Domain[i] > params.Domain[i+1])
            goto sub_function_error;
    }
    if (shading_domain) {
        if (num_inputs != code >> 1)
            goto sub_function_error;

        for (i=0;i<2*num_inputs;i+=2) {
            if (params.Domain[i] > shading_domain[i] || params.Domain[i+1] < shading_domain[i+1])
                goto sub_function_error;
        }
    }

    params.m = code >> 1;

    code = make_float_array_from_dict(ctx, (float **)&params.Range, stream_dict, "Range");
    if (code < 0 && code != gs_error_undefined)
        goto sub_function_error;
    else {
        if (code > 0)
            params.n = code >> 1;
        else
            params.n = 0;
    }
    switch(Type) {
        case 0:
            code = pdfi_build_function_0(ctx, &params, stream_dict, 0, ppfn);
            if (code < 0)
                goto sub_function_error;
            break;
        case 2:
            code = pdfi_build_function_2(ctx, &params, stream_dict, 0, ppfn);
            if (code < 0)
                goto sub_function_error;
            break;
        case 3:
            code = pdfi_build_function_3(ctx, &params, stream_dict, shading_domain,  num_inputs, page_dict, 0, ppfn);
            if (code < 0)
                goto sub_function_error;
            break;
        case 4:
            code = pdfi_build_function_4(ctx, &params, stream_dict, 0, ppfn);
            if (code < 0)
                goto sub_function_error;
            break;
        default:
            break;
    }
    return 0;

sub_function_error:
    gs_free_const_object(ctx->memory, params.Domain, "pdfi_build_sub_function (Domain) error exit\n");
    gs_free_const_object(ctx->memory, params.Range, "pdfi_build_sub_function(Range) error exit\n");
    return code;
}

int pdfi_free_function(pdf_context *ctx, gs_function_t *pfn)
{
    if (pfn == NULL)
        return 0;

    gs_function_free(pfn, true, ctx->memory);
    return 0;
}

int pdfi_build_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    return pdfi_build_sub_function(ctx, ppfn, shading_domain, num_inputs, stream_dict, page_dict);
}
