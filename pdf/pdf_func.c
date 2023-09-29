/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

static int pdfi_build_sub_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_obj *stream_obj, pdf_dict *page_dict);

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
static void psc_fixup_ifelse(byte *p)
{
    int iflen = (p[0] << 8) + p[1];

    iflen += 3;         /* To skip past the 'if' body and the 'else' header */
    p[0] = (byte)(iflen >> 8);
    p[1] = (byte)iflen;
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
pdfi_parse_type4_func_stream(pdf_context *ctx, pdf_c_stream *function_stream, int depth, byte *ops, unsigned int *size)
{
    int code;
    int c;
    char TokenBuffer[17];
    unsigned int Size, IsReal;
    byte *clause = NULL;
    byte *p = (ops ? ops + *size : NULL);

    while (1) {
        c = pdfi_read_byte(ctx, function_stream);
        if (c < 0)
            break;
        switch(c) {
            case '%':
                do {
                    c = pdfi_read_byte(ctx, function_stream);
                    if (c < 0)
                        break;
                    if (c == 0x0a || c == 0x0d)
                        break;
                }while (1);
                break;
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
                    depth --;
                    if (code < 0)
                        return code;
                    if (p) {
                        if (clause == NULL) {
                            clause = p;
                            *p = (byte)PtCr_if;
                            psc_fixup(p, ops + *size);
                        } else {
                            *p = (byte)PtCr_else;
                            psc_fixup(p, ops + *size);
                            psc_fixup_ifelse(clause + 1);
                            clause = NULL;
                        }
                        p = ops + *size;
                    }
                }
                break;
            case '}':
                return *size;
                break;
            default:
                if (clause != NULL)
                    clause = NULL;
                if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
                    /* parse a number */
                    Size = 1;
                    if (c == '.')
                        IsReal = 1;
                    else
                        IsReal = 0;
                    TokenBuffer[0] = (byte)c;
                    while (1) {
                        c = pdfi_read_byte(ctx, function_stream);
                        if (c < 0)
                            return_error(gs_error_syntaxerror);

                        if (c == '.'){
                            if (IsReal == 1)
                                return_error(gs_error_syntaxerror);
                            else {
                                TokenBuffer[Size++] = (byte)c;
                                IsReal = 1;
                            }
                        } else {
                            if (c >= '0' && c <= '9') {
                                TokenBuffer[Size++] = (byte)c;
                            } else
                                break;
                        }
                        if (Size > NUMBERTOKENSIZE)
                            return_error(gs_error_syntaxerror);
                    }
                    TokenBuffer[Size] = 0x00;
                    pdfi_unread_byte(ctx, function_stream, (byte)c);
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
                    TokenBuffer[0] = (byte)c;
                    while (1) {
                        c = pdfi_read_byte(ctx, function_stream);
                        if (c < 0)
                            return_error(gs_error_syntaxerror);
                        if (c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d || c == '{' || c == '}')
                            break;
                        TokenBuffer[Size++] = (byte)c;
                        if (Size > OPTOKENSIZE)
                            return_error(gs_error_syntaxerror);
                    }
                    TokenBuffer[Size] = 0x00;
                    pdfi_unread_byte(ctx, function_stream, (byte)c);
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
    }

    return 0;
}

static int
pdfi_build_function_4(pdf_context *ctx, gs_function_params_t * mnDR,
                    pdf_stream *function_obj, int depth, gs_function_t ** ppfn)
{
    gs_function_PtCr_params_t params;
    pdf_c_stream *function_stream = NULL;
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

    if (pdfi_type_of(function_obj) != PDF_STREAM)
        return_error(gs_error_undefined);
    Length = pdfi_stream_length(ctx, (pdf_stream *)function_obj);

    savedoffset = pdfi_tell(ctx->main_stream);
    code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, function_obj), SEEK_SET);
    if (code < 0)
        return code;

    code = pdfi_open_memory_stream_from_filtered_stream(ctx, function_obj, (unsigned int)Length, &data_source_buffer, ctx->main_stream, &function_stream, false);
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
    function_stream = NULL;
    if (code < 0)
        goto function_4_error;

    params.ops.data = (const byte *)ops;
    /* ops will now be freed with the function params, NULL ops now to avoid double free on error */
    ops = NULL;
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
                    pdf_stream *function_obj, int depth, gs_function_t ** ppfn)
{
    gs_function_Sd_params_t params;
    pdf_c_stream *function_stream = NULL;
    int code = 0;
    int64_t Length, temp;
    byte *data_source_buffer;
    gs_offset_t savedoffset;
    pdf_dict *function_dict = NULL;

    memset(&params, 0x00, sizeof(gs_function_params_t));
    *(gs_function_params_t *) & params = *mnDR;
    params.Encode = params.Decode = NULL;
    params.pole = NULL;
    params.Size = params.array_step = params.stream_step = NULL;
    params.Order = 0;

    if (pdfi_type_of(function_obj) != PDF_STREAM)
        return_error(gs_error_undefined);

    code = pdfi_dict_from_obj(ctx, (pdf_obj *)function_obj, &function_dict);
    if (code < 0)
        return code;

    Length = pdfi_stream_length(ctx, (pdf_stream *)function_obj);

    savedoffset = pdfi_tell(ctx->main_stream);
    pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, function_obj), SEEK_SET);

    Length = pdfi_open_memory_stream_from_filtered_stream(ctx, function_obj, (unsigned int)Length, &data_source_buffer, ctx->main_stream, &function_stream, false);
    if (Length < 0) {
        pdfi_seek(ctx, ctx->main_stream, savedoffset, SEEK_SET);
        return Length;
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

    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.Encode, function_dict, "Encode");
    if (code < 0) {
        if (code == gs_error_undefined)
            code = 2 * params.m;
        else
            goto function_0_error;
    }
    if (code != 2 * params.m) {
        code = gs_error_rangecheck;
        goto function_0_error;
    }

    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.Decode, function_dict, "Decode");
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

    code = pdfi_make_int_array_from_dict(ctx, (int **)&params.Size, function_dict, "Size");
    if (code != params.m) {
        if (code >= 0)
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
        samples = params.n * (uint64_t)params.BitsPerSample;
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
    s_close_filters(&params.DataSource.data.strm, params.DataSource.data.strm->strm);
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

    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.C0, function_dict, "C0");
    if (code < 0 && code != gs_error_undefined)
        return code;
    n0 = code;

    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.C1, function_dict, "C1");
    if (code < 0 && code != gs_error_undefined)
        goto function_2_error;

    n1 = code;
    if (params.C0 == NULL)
        n0 = 1;
    if (params.C1 == NULL)
        n1 = 1;
    if (params.Range == 0)
        params.n = n0;		/* either one will do */
    if (n0 != n1 || n0 != params.n) {
        code = gs_note_error(gs_error_rangecheck);
        goto function_2_error;
    }

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

    code = pdfi_dict_get_type(ctx, function_dict, "Functions", PDF_ARRAY, (pdf_obj **)&Functions);
    if (code < 0)
        return code;

    params.k = pdfi_array_size(Functions);
    code = alloc_function_array(params.k, &ptr, ctx->memory);
    if (code < 0)
        goto function_3_error;

    params.Functions = (const gs_function_t * const *)ptr;

    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.Bounds, function_dict, "Bounds");
    if (code < 0)
        goto function_3_error;

    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.Encode, function_dict, "Encode");
    if (code < 0)
        goto function_3_error;

    if (code != 2 * params.k) {
        code = gs_note_error(gs_error_rangecheck);
        goto function_3_error;
    }
    code = 0;

    for (i = 0; i < params.k; ++i) {
        pdf_obj * rsubfn = NULL;

        /* This is basically hacky. The test file /tests_private/pdf/pdf_1.7_ATS/WWTW61EC_file.pdf
         * has a number of shadings on page 2. Although there are numerous shadings, they each use one
         * of four functions. However, these functions are themselves type 3 functions with 255
         * sub-functions. Because our cache only has 200 entries (at this moment), this overfills
         * the cache, ejecting all the cached objects (and then some). Which means that we throw
         * out any previous shadings or functions, meaning that on every use we have to reread them. This is,
         * obviously, slow. So in the hope that reuse of *sub_functions* is unlikely, we choose to
         * read the subfunction without caching. This means the main shadings, and the functions,
         * remain cached so we can reuse them saving an enormous amount of time. If we ever find a file
         * which significantly reuses sub-functions we may need to revisit this.
         */
        code = pdfi_array_get_nocache(ctx, (pdf_array *)Functions, (int64_t)i, &rsubfn);
        if (code < 0)
            goto function_3_error;

        code = pdfi_build_sub_function(ctx, &ptr[i], &params.Encode[i * 2], num_inputs, rsubfn, page_dict);
        pdfi_countdown(rsubfn);
        if (code < 0)
            goto function_3_error;
    }

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

static int pdfi_build_sub_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_obj *stream_obj, pdf_dict *page_dict)
{
    int code, i;
    int64_t Type;
    gs_function_params_t params;
    pdf_dict *stream_dict;
    int obj_num;

    params.Range = params.Domain = NULL;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    obj_num = pdf_object_num(stream_obj);
    if (obj_num != 0) {
        if (pdfi_loop_detector_check_object(ctx, obj_num))
            return gs_note_error(gs_error_circular_reference);
        code = pdfi_loop_detector_add_object(ctx, obj_num);
        if (code < 0)
            goto sub_function_error;
    }

    code = pdfi_dict_from_obj(ctx, stream_obj, &stream_dict);
    if (code < 0)
        goto sub_function_error;

    code = pdfi_dict_get_int(ctx, stream_dict, "FunctionType", &Type);
    if (code < 0)
        goto sub_function_error;

    if (Type < 0 || Type > 4 || Type == 1) {
        code = gs_note_error(gs_error_rangecheck);
        goto sub_function_error;
    }

    memset(&params, 0x00, sizeof(gs_function_params_t));

    /* First gather all the entries common to all functions */
    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.Domain, stream_dict, "Domain");
    if (code < 0)
        goto sub_function_error;

    if (code & 1) {
        code = gs_error_rangecheck;
        goto sub_function_error;
    }

    for (i=0;i<code;i+=2) {
        if (params.Domain[i] > params.Domain[i+1]) {
            code = gs_error_rangecheck;
            goto sub_function_error;
        }
    }
    if (shading_domain) {
        if (num_inputs != code >> 1) {
            code = gs_error_rangecheck;
            goto sub_function_error;
        }

        for (i=0;i<2*num_inputs;i+=2) {
            if (params.Domain[i] > shading_domain[i] || params.Domain[i+1] < shading_domain[i+1]) {
                code = gs_error_rangecheck;
                goto sub_function_error;
            }
        }
    }

    params.m = code >> 1;

    code = pdfi_make_float_array_from_dict(ctx, (float **)&params.Range, stream_dict, "Range");
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
            code = pdfi_build_function_0(ctx, &params, (pdf_stream *)stream_obj, 0, ppfn);
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
            code = pdfi_build_function_4(ctx, &params, (pdf_stream *)stream_obj, 0, ppfn);
            if (code < 0)
                goto sub_function_error;
            break;
        default:
            break;
    }
    pdfi_loop_detector_cleartomark(ctx);
    return 0;

sub_function_error:
    gs_free_const_object(ctx->memory, params.Domain, "pdfi_build_sub_function (Domain) error exit\n");
    gs_free_const_object(ctx->memory, params.Range, "pdfi_build_sub_function(Range) error exit\n");
    pdfi_loop_detector_cleartomark(ctx);
    return code;
}


static int pdfi_free_function_special(pdf_context *ctx, gs_function_t *pfn);

#if 0
/* For type 0 functions, need to free up the data associated with the stream
 * that it was using.  This doesn't get freed in the gs_function_free() code.
 */
static int pdfi_free_function_0(pdf_context *ctx, gs_function_t *pfn)
{
    gs_function_Sd_params_t *params = (gs_function_Sd_params_t *)&pfn->params;

    s_close_filters(&params->DataSource.data.strm, params->DataSource.data.strm->strm);
    gs_free_object(ctx->memory, params->DataSource.data.strm, "pdfi_free_function");
    return 0;
}
#endif

/* For type 3 functions, it has an array of functions that might need special handling.
 */
static int pdfi_free_function_3(pdf_context *ctx, gs_function_t *pfn)
{
    gs_function_1ItSg_params_t *params = (gs_function_1ItSg_params_t *)&pfn->params;
    int i;

    for (i=0; i<params->k; i++) {
        pdfi_free_function_special(ctx, (gs_function_t *)params->Functions[i]);
    }
    return 0;
}

/* Free any special stuff associated with a function */
static int pdfi_free_function_special(pdf_context *ctx, gs_function_t *pfn)
{
    switch(pfn->head.type) {
#if 0
    /* Before commit 3f2408d5ac786ac1c0a837b600f4ef3be9be0332
     * https://git.ghostscript.com/?p=ghostpdl.git;a=commit;h=3f2408d5ac786ac1c0a837b600f4ef3be9be0332
     * we needed to close the data stream and free the memory. That is now
     * performed by the graphics library so we don't need to do this any more.
     */
    case 0:
        pdfi_free_function_0(ctx, pfn);
        break;
#endif
    case 3:
        pdfi_free_function_3(ctx, pfn);
        break;
    default:
        break;
    }
    return 0;
}

int pdfi_free_function(pdf_context *ctx, gs_function_t *pfn)
{
    if (pfn == NULL)
        return 0;

    /* Free any special stuff for the function */
    pdfi_free_function_special(ctx, pfn);

    /* Free the standard stuff handled by the gs library */
    gs_function_free(pfn, true, ctx->memory);
    return 0;
}

int pdfi_build_function(pdf_context *ctx, gs_function_t ** ppfn, const float *shading_domain, int num_inputs, pdf_obj *stream_obj, pdf_dict *page_dict)
{
    return pdfi_build_sub_function(ctx, ppfn, shading_domain, num_inputs, stream_obj, page_dict);
}

int pdfi_build_halftone_function(pdf_context *ctx, gs_function_t ** ppfn, byte *Buffer, int64_t Length)
{
    gs_function_PtCr_params_t params;
    pdf_c_stream *function_stream = NULL;
    int code=0;
    byte *ops = NULL;
    unsigned int size;
    float *pfloat;
    byte *stream_buffer = NULL;

    memset(&params, 0x00, sizeof(gs_function_PtCr_params_t));
    params.ops.data = 0;	/* in case of failure */
    params.ops.size = 0;	/* ditto */

    stream_buffer = gs_alloc_bytes(ctx->memory, Length, "pdfi_build_halftone_function(stream_buffer))");
    if (stream_buffer == NULL)
        goto halftone_function_error;

    memcpy(stream_buffer, Buffer, Length);

    code = pdfi_open_memory_stream_from_memory(ctx, Length, stream_buffer, &function_stream, true);
    if (code < 0)
        goto halftone_function_error;

    size = 0;
    code = pdfi_parse_type4_func_stream(ctx, function_stream, 0, NULL, &size);
    if (code < 0)
        goto halftone_function_error;

    ops = gs_alloc_string(ctx->memory, size + 1, "pdfi_build_halftone_function(ops)");
    if (ops == NULL) {
        code = gs_error_VMerror;
        goto halftone_function_error;
    }

    code = pdfi_seek(ctx, function_stream, 0, SEEK_SET);
    if (code < 0)
        goto halftone_function_error;

    size = 0;
    code = pdfi_parse_type4_func_stream(ctx, function_stream, 0, ops, &size);
    if (code < 0)
        goto halftone_function_error;
    ops[size] = PtCr_return;

    code = pdfi_close_memory_stream(ctx, stream_buffer, function_stream);
    if (code < 0) {
        function_stream = NULL;
        goto halftone_function_error;
    }

    params.ops.data = (const byte *)ops;
    params.ops.size = size + 1;
    params.m = 2;
    params.n = 1;
    pfloat = (float *)gs_alloc_byte_array(ctx->memory, 4, sizeof(float), "pdfi_build_halftone_function(Domain)");
    if (pfloat == NULL) {
        code = gs_error_VMerror;
        goto halftone_function_error;
    }
    pfloat[0] = -1;
    pfloat[1] = 1;
    pfloat[2] = -1;
    pfloat[3] = 1;
    params.Domain = (const float *)pfloat;
    pfloat = (float *)gs_alloc_byte_array(ctx->memory, 2, sizeof(float), "pdfi_build_halftone_function(Domain)");
    if (pfloat == NULL) {
        code = gs_error_VMerror;
        goto halftone_function_error;
    }
    pfloat[0] = -1;
    pfloat[1] = 1;
    params.Range = (const float *)pfloat;

    code = gs_function_PtCr_init(ppfn, &params, ctx->memory);
    if (code < 0)
        goto halftone_function_error;

    return 0;

halftone_function_error:
    if (function_stream)
        (void)pdfi_close_memory_stream(ctx, stream_buffer, function_stream);

    gs_function_PtCr_free_params(&params, ctx->memory);
    if (ops)
        gs_free_const_string(ctx->memory, ops, size, "pdfi_build_function_4(ops)");
    return code;
}
