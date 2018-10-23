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

#include "gsmatrix.h"
#include "gslparam.h"

int pdfi_concat(pdf_context *ctx)
{
    int i, code;
    pdf_num *num;
    double Values[6];
    gs_matrix m;

    if (ctx->stack_top - ctx->stack_bot < 6) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    for (i=0;i < 6;i++){
        num = (pdf_num *)ctx->stack_top[i - 6];
        if (num->type != PDF_INT) {
            if(num->type != PDF_REAL) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_typecheck);
                else {
                    pdfi_pop(ctx, 6);
                    return 0;
                }
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
    if (code == 0)
        pdfi_pop(ctx, 6);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_gsave(pdf_context *ctx)
{
    int code = gs_gsave(ctx->pgs);

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_grestore(pdf_context *ctx)
{
    int code = gs_grestore(ctx->pgs);

    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

int pdfi_setlinewidth(pdf_context *ctx)
{
    int code;
    pdf_num *n1;
    double d1;

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdfi_pop(ctx, 1);
                return 0;
            }
        }
    }
    code = gs_setlinewidth(ctx->pgs, d1);
    if (code == 0)
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

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinejoin(ctx->pgs, (gs_line_join)n1->value.d);
    } else {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else {
            pdfi_pop(ctx, 1);
            return 0;
        }
    }
    if (code == 0)
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

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        code = gs_setlinecap(ctx->pgs, (gs_line_cap)n1->value.d);
    } else {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else {
            pdfi_pop(ctx, 1);
            return 0;
        }
    }
    if (code == 0)
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

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdfi_pop(ctx, 1);
                return 0;
            }
        }
    }
    code = gs_setflat(ctx->pgs, d1);
    if (code == 0)
        pdfi_pop(ctx, 1);
    if(code < 0 && ctx->pdfstoponerror)
        return code;
    else
        return 0;
}

static int setdash_impl(pdf_context *ctx, pdf_array *a, double phase_d)
{
    float *dash_array;
    double temp;
    int i, code;

    dash_array = (float *)gs_alloc_bytes(ctx->memory, a->entries * sizeof (float), "temporary float array for setdash");
    if (dash_array == NULL)
        return_error(gs_error_VMerror);

    for (i=0;i < a->entries;i++){
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
    code = gs_setdash(ctx->pgs, dash_array, a->entries, phase_d);
    gs_free_object(ctx->memory, dash_array, "error in setdash");
    return code;
}
int pdfi_setdash(pdf_context *ctx)
{
    pdf_num *phase;
    pdf_array *a;
    double phase_d, temp;
    int i, code;

    if (ctx->stack_top - ctx->stack_bot < 2) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    phase = (pdf_num *)ctx->stack_top[-1];
    if (phase->type == PDF_INT){
        phase_d = (double)phase->value.i;
    } else{
        if (phase->type == PDF_REAL) {
            phase_d = phase->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdfi_pop(ctx, 1);
                return 0;
            }
        }
    }

    a = (pdf_array *)ctx->stack_top[-2];
    if (a->type != PDF_ARRAY) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_typecheck);
        else {
            pdfi_pop(ctx, 1);
            return 0;
        }
    }

    code = setdash_impl(ctx, a, phase_d);
    if (code == 0)
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

    if (ctx->stack_top - ctx->stack_bot < 1) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_stackunderflow);
        pdfi_clearstack(ctx);
        return 0;
    }

    n1 = (pdf_num *)ctx->stack_top[-1];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else {
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            else {
                pdfi_pop(ctx, 1);
                return 0;
            }
        }
    }
    code = gs_setmiterlimit(ctx->pgs, d1);
    if (code == 0)
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
    pdf_obj *o;
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

    code = setdash_impl(ctx, a1, d);
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

    if (pdfi_name_strcmp(n, "Perceptual") == 0) {
            code = gs_setrenderingintent(ctx->pgs, 0);
    } else if (pdfi_name_strcmp(n, "Saturation") == 0) {
        code = gs_setrenderingintent(ctx->pgs, 2);
    } else if (pdfi_name_strcmp(n, "RelativeColorimetric") == 0) {
        code = gs_setrenderingintent(ctx->pgs, 1);
    } else if (pdfi_name_strcmp(n, "AbsoluteColoimetric") == 0) {
        code = gs_setrenderingintent(ctx->pgs, 3);
    } else {
        code = gs_error_undefined;
    }
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
    dmprintf(ctx->memory, "ExtGState Font not yet implemented\n");
    return 0;
}

static int GS_BG(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dmprintf(ctx->memory, "ExtGState BG not yet implemented\n");
    return 0;
}

static int GS_BG2(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dmprintf(ctx->memory, "ExtGState BG2 not yet implemented\n");
    return 0;
}

static int GS_UCR(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dmprintf(ctx->memory, "ExtGState UCR not yet implemented\n");
    return 0;
}

static int GS_UCR2(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dmprintf(ctx->memory, "ExtGState UCR2 not yet implemented\n");
    return 0;
}

static int GS_TR(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dmprintf(ctx->memory, "ExtGState TR not yet implemented\n");
    return 0;
}

static int GS_TR2(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_obj *o = NULL, *n = NULL;
    gs_offset_t saved_stream_offset;

    code = pdfi_dict_get(ctx, GS, "TR2", &n);
    if (code < 0)
        return code;

    if (n->type == PDF_NAME) {
        pdfi_countdown(o);
        return 0;
    }

    saved_stream_offset = pdfi_unread_tell(ctx);
    if (n->type == PDF_INDIRECT) {
        if (ctx->loop_detection == NULL) {
            pdfi_init_loop_detector(ctx);
            pdfi_loop_detector_mark(ctx);
        } else {
            pdfi_loop_detector_mark(ctx);
        }
        code = pdfi_dereference(ctx, n->object_num, n->generation_num, &o);
        pdfi_loop_detector_cleartomark(ctx);
        if (code < 0) {
            (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
            pdfi_countdown(o);
            return code;
        }
    } else
        o = n;

    if (o->type == PDF_ARRAY) {
    } else {
        if (o->type == PDF_DICT) {
            gs_function_t *pfn;

            code = pdfi_build_function(ctx, &pfn, NULL, 1, (pdf_dict *)o, page_dict);
            if (code < 0) {
                (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                pdfi_countdown(o);
                return code;
            }
        }
    }

    (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    dmprintf(ctx->memory, "ExtGState TR2 not yet implemented\n");

    return 0;
}

static int GS_HT(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dmprintf(ctx->memory, "ExtGState HT not yet implemented\n");
    return 0;
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
    dmprintf(ctx->memory, "ExtGState BM not yet implemented\n");
    return 0;
}

static int GS_SMask(pdf_context *ctx, pdf_dict *GS, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    dmprintf(ctx->memory, "ExtGState SMask not yet implemented\n");
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

    code = pdfi_dict_get_number(ctx, GS, "CA", &d1);
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
    char *Name;
    GS_proc proc;
} GS_Func_t;

GS_Func_t ExtGStateTable[] = {
    {"LW", GS_LW},
    {"LC", GS_LC},
    {"LJ", GS_LJ},
    {"ML", GS_ML},
    {"D", GS_LC},
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
    pdf_obj *o, *o1, *cspace;
    int code, i, limit = sizeof(ExtGStateTable) / sizeof (GS_Func_t);
    bool known;

    if (ctx->loop_detection == NULL) {
        pdfi_init_loop_detector(ctx);
        pdfi_loop_detector_mark(ctx);
    } else {
        pdfi_loop_detector_mark(ctx);
    }

    if (ctx->stack_top - ctx->stack_bot < 1) {
        pdfi_loop_detector_cleartomark(ctx);
        return_error(gs_error_stackunderflow);
    }
    n = (pdf_name *)ctx->stack_top[-1];
    if (n->type != PDF_NAME) {
        pdfi_pop(ctx, 1);
        pdfi_loop_detector_cleartomark(ctx);
        return_error(gs_error_typecheck);
    }

    code = pdfi_find_resource(ctx, (unsigned char *)"ExtGState", n, stream_dict, page_dict, &o);
    pdfi_pop(ctx, 1);
    if (code < 0) {
        pdfi_loop_detector_cleartomark(ctx);
            return code;
    }
    if (o->type != PDF_DICT) {
        pdfi_loop_detector_cleartomark(ctx);
        pdfi_countdown(o);
        return_error(gs_error_typecheck);
    }

    for (i=0;i < limit; i++) {
        code = pdfi_dict_known((pdf_dict *)o, ExtGStateTable[i].Name, &known);
        if (code < 0 && ctx->pdfstoponerror) {
            pdfi_loop_detector_cleartomark(ctx);
            pdfi_countdown(o);
            return code;
        }
        if (known) {
            code = ExtGStateTable[i].proc(ctx, (pdf_dict *)o, stream_dict, page_dict);
            if (code < 0 && ctx->pdfstoponerror) {
                pdfi_loop_detector_cleartomark(ctx);
                pdfi_countdown(o);
                return code;
            }
        }
    }

    pdfi_loop_detector_cleartomark(ctx);
    pdfi_countdown(o);
    return 0;
}
