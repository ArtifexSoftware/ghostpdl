/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* PostScript language support for FunctionType 4 (PS Calculator) Functions */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "opextern.h"
#include "gsfunc.h"
#include "gsfunc4.h"
#include "gsutil.h"
#include "idict.h"
#include "ifunc.h"
#include "iname.h"
#include "dstack.h"
#include "ialloc.h"
#include "gzstate.h"	    /* these are needed to check device parameters */
#include "gsparam.h"	    /* these are needed to check device parameters */
#include "zfunc.h"
#include "zcolor.h"
#include "gxdevsop.h"

/*
 * FunctionType 4 functions are not defined in the PostScript language.  We
 * provide support for them because they are needed for PDF 1.3.  In
 * addition to the standard FunctionType, Domain, and Range keys, they have
 * a Function key whose value is a procedure in a restricted subset of the
 * PostScript language.  Specifically, the procedure must (recursively)
 * contain only integer, real, Boolean, and procedure constants (only as
 * literal operands of if and and ifelse), and operators chosen from the set
 * given below.  Note that names other than true and false are not allowed:
 * the procedure must be 'bound'.
 *
 * The following list is taken directly from the PDF 1.3 documentation.
 */
#define XOP(zfn) int zfn(i_ctx_t *)
XOP(zabs); XOP(zand); XOP(zatan); XOP(zbitshift);
XOP(zceiling); XOP(zcos); XOP(zcvi); XOP(zcvr);
XOP(zdiv); XOP(zexp); XOP(zfloor); XOP(zidiv);
XOP(zln); XOP(zlog); XOP(zmod); XOP(zmul);
XOP(zneg); XOP(znot); XOP(zor); XOP(zround);
XOP(zsin); XOP(zsqrt); XOP(ztruncate); XOP(zxor);
XOP(zeq); XOP(zge); XOP(zgt); XOP(zle); XOP(zlt); XOP(zne);
XOP(z2copy);
#undef XOP
typedef struct calc_op_s {
    op_proc_t proc;
    gs_PtCr_opcode_t opcode;
} calc_op_t;
static const calc_op_t calc_ops[] = {

    /* Arithmetic operators */

    {zabs, PtCr_abs},
    {zadd, PtCr_add},
    {zand, PtCr_and},
    {zatan, PtCr_atan},
    {zbitshift, PtCr_bitshift},
    {zceiling, PtCr_ceiling},
    {zcos, PtCr_cos},
    {zcvi, PtCr_cvi},
    {zcvr, PtCr_cvr},
    {zdiv, PtCr_div},
    {zexp, PtCr_exp},
    {zfloor, PtCr_floor},
    {zidiv, PtCr_idiv},
    {zln, PtCr_ln},
    {zlog, PtCr_log},
    {zmod, PtCr_mod},
    {zmul, PtCr_mul},
    {zneg, PtCr_neg},
    {znot, PtCr_not},
    {zor, PtCr_or},
    {zround, PtCr_round},
    {zsin, PtCr_sin},
    {zsqrt, PtCr_sqrt},
    {zsub, PtCr_sub},
    {ztruncate, PtCr_truncate},
    {zxor, PtCr_xor},

    /* Comparison operators */

    {zeq, PtCr_eq},
    {zge, PtCr_ge},
    {zgt, PtCr_gt},
    {zle, PtCr_le},
    {zlt, PtCr_lt},
    {zne, PtCr_ne},

    /* Stack operators */

    {zcopy, PtCr_copy},
    {z2copy, PtCr_copy},
    {zdup, PtCr_dup},
    {zexch, PtCr_exch},
    {zindex, PtCr_index},
    {zpop, PtCr_pop},
    {zroll, PtCr_roll}

    /* Special operators */

    /*{zif, PtCr_if},*/
    /*{zifelse, PtCr_ifelse},*/
    /*{ztrue, PtCr_true},*/
    /*{zfalse, PtCr_false}*/
};

/* Fix up an if or ifelse forward reference. */
static void
psc_fixup(byte *p, byte *to)
{
    int skip = to - (p + 3);

    p[1] = (byte)(skip >> 8);
    p[2] = (byte)skip;
}

/* Check whether the ref is a given operator or resolves to it */
static bool
resolves_to_oper(i_ctx_t *i_ctx_p, const ref *pref, const op_proc_t proc)
{
    if (!r_has_attr(pref, a_executable))
        return false;
    if (r_btype(pref) == t_operator) {
        return pref->value.opproc == proc;
    } else if (r_btype(pref) == t_name) {
        ref * val;
        if (dict_find(systemdict, pref, &val) <= 0)
            return false;
        if (r_btype(val) != t_operator)
            return false;
        if (!r_has_attr(val, a_executable))
            return false;
        return val->value.opproc == proc;
    }
   else
      return false;
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

/* Store an op code in the  buffer */
static int
put_op(byte **p, byte op) {
   if (*p)
      *(*p)++ = op;
   return 1;
}

/*
 * Check a calculator function for validity, optionally storing its encoded
 * representation and add the size of the encoded representation to *psize.
 * Note that we arbitrarily limit the depth of procedure nesting.  pref is
 * known to be a procedure.
 */
static int
check_psc_function(i_ctx_t *i_ctx_p, const ref *pref, int depth, byte *ops, int *psize, bool AllowRepeat)
{
    int code;
    uint i, j;
    uint size = r_size(pref);
    byte *p;

    if (size == 2 && depth == 0) {
        /* Bug 690986. Recognize and replace Corel tint transform function.
           { tint_params CorelTintTransformFunction }

           Where tint_params resolves to an arrey like:
             [ 1.0 0.0 0.0 0.0
               0.0 1.0 0.0 0.0
               0.0 0.0 1.0 0.0
               0.0 0.0 0.0 1.0
               0.2 0.81 0.76 0.61
            ]
           And CorelTintTransformFunction is:
            { /colorantSpecArray exch def
              /nColorants colorantSpecArray length 4 idiv def
              /inColor nColorants 1 add 1 roll nColorants array astore def
              /outColor 4 array def
              0 1 3 {
                /nOutInk exch def
                1
                0 1 nColorants 1 sub {
                  dup inColor exch get
                  exch 4 mul nOutInk add colorantSpecArray exch get mul
                  1 exch sub mul
                } for
                1 exch sub
                outColor nOutInk 3 -1 roll put
              } for
              outColor aload pop
            }
        */
        ref r_tp, r_cttf; /* original references */
        ref n_tp, n_cttf; /* names */
        ref *v_tp, *v_cttf; /* values */
        int sz;

        p = ops;
        sz = *psize;
        if(array_get(imemory, pref, 0, &r_tp) < 0)
            goto idiom_failed;
        if (array_get(imemory, pref, 1, &r_cttf) < 0)
            goto idiom_failed;
        if (r_has_type(&r_tp, t_name) && r_has_type(&r_cttf, t_name)) {
            if ((code = name_enter_string(imemory, "tint_params", &n_tp)) < 0)
                return code;
            if (r_tp.value.pname == n_tp.value.pname) {
                if ((code = name_enter_string(imemory, "CorelTintTransformFunction", &n_cttf)) < 0)
                    return code;
                if (r_cttf.value.pname == n_cttf.value.pname) {
                    v_tp   = dict_find_name(&n_tp);
                    v_cttf = dict_find_name(&n_cttf);
                    if (v_tp && v_cttf && r_is_array(v_tp) && r_is_array(v_cttf)) {
                        uint n_elem = r_size(v_tp);

                        if ((n_elem & 3) == 0 && r_size(v_cttf) == 31) {
                            /* Enough testing, idiom recognition tests less. */
                            uint n_col = n_elem/4;

                            for (i = 0; i < 4; i ++) {
                                ref v;
                                float fv;
                                bool first = true;

                                for (j = 0; j < n_col; j++) {
                                    if (array_get(imemory, v_tp, j*4 + i, &v) < 0)
                                        goto idiom_failed;
                                    if (r_type(&v) == t_integer)
                                        fv = (float)v.value.intval;
                                    else if (r_type(&v) == t_real)
                                        fv = v.value.realval;
                                    else
                                        goto idiom_failed;

                                    if (fv != 0.) {
                                        if (first)
                                            sz += put_int(&p, 1);
                                        sz += put_int(&p, 1);
                                        sz += put_int(&p, n_col + 1 - j + i + !first);
                                        sz += put_op(&p, PtCr_index);
                                        if (fv != 1.) {
                                            sz += put_float(&p, fv);
                                            sz += put_op(&p, PtCr_mul);
                                        }
                                        sz += put_op(&p, PtCr_sub);
                                        if (first)
                                            first = false;
                                        else
                                            sz += put_op(&p, PtCr_mul);
                                    }
                                }
                                if (first)
                                    sz += put_int(&p, 0);
                                else
                                    sz += put_op(&p, PtCr_sub);
                            }
                            /* n_col+4 4 roll pop ... pop  */
                            sz += put_int(&p, n_col + 4);
                            sz += put_int(&p, 4);
                            sz += put_op(&p, PtCr_roll);
                            for (j = 0; j < n_col; j++)
                                sz += put_op(&p, PtCr_pop);
                            *psize = sz;
                            return 0;
                        }
                    }
                }
            }
        }
    }
  idiom_failed:;
    for (i = 0; i < size; ++i) {
        byte no_ops[1 + max(sizeof(int), sizeof(float))];
        ref elt, elt2, elt3;
        ref * delp;

        p = (ops ? ops + *psize : no_ops);
        array_get(imemory, pref, i, &elt);
        switch (r_btype(&elt)) {
        case t_integer:
            *psize += put_int(&p, elt.value.intval);
            break;
        case t_real:
            *psize += put_float(&p, elt.value.realval);
            break;
        case t_boolean:
            *p = (elt.value.boolval ? PtCr_true : PtCr_false);
            ++*psize;
            break;
        case t_name:
            if (!r_has_attr(&elt, a_executable))
                return_error(gs_error_rangecheck);
            name_string_ref(imemory, &elt, &elt);
            if (!bytes_compare(elt.value.bytes, r_size(&elt),
                               (const byte *)"true", 4)) {
                *p = PtCr_true;
                ++*psize;
                break;
            }
            if (!bytes_compare(elt.value.bytes, r_size(&elt),
                                      (const byte *)"false", 5)) {
                *p = PtCr_false;
                ++*psize;
                break;
            }
            /* Check if the name is a valid operator in systemdict */
            if (dict_find(systemdict, &elt, &delp) <= 0)
                return_error(gs_error_undefined);
            if (r_btype(delp) != t_operator)
                return_error(gs_error_typecheck);
            if (!r_has_attr(delp, a_executable))
                return_error(gs_error_rangecheck);
            elt = *delp;
            /* Fall into the operator case */
        case t_operator: {
            int j;

            for (j = 0; j < countof(calc_ops); ++j)
                if (elt.value.opproc == calc_ops[j].proc) {
                    *p = calc_ops[j].opcode;
                    ++*psize;
                    goto next;
                }
            return_error(gs_error_rangecheck);
        }
        default: {
            if (!r_is_proc(&elt))
                return_error(gs_error_typecheck);
            if (depth == MAX_PSC_FUNCTION_NESTING)
                return_error(gs_error_limitcheck);
            if ((code = array_get(imemory, pref, ++i, &elt2)) < 0)
                return code;
            *psize += 3;
            code = check_psc_function(i_ctx_p, &elt, depth + 1, ops, psize, AllowRepeat);
            if (code < 0)
                return code;
            /* Check for { proc } repeat | {proc} if | {proc1} {proc2} ifelse */
            if (resolves_to_oper(i_ctx_p, &elt2, zrepeat)) {
                if (!AllowRepeat)
                    return_error(gs_error_rangecheck);
                if (ops) {
                    *p = PtCr_repeat;
                    psc_fixup(p, ops + *psize);
                    p = ops + *psize;
                    *p++ = PtCr_repeat_end;
                }
                *psize += 1;	/* extra room for repeat_end */
            } else if (resolves_to_oper(i_ctx_p, &elt2, zif)) {
                if (ops) {
                    *p = PtCr_if;
                    psc_fixup(p, ops + *psize);
                }
            } else if (!r_is_proc(&elt2))
                return_error(gs_error_rangecheck);
            else if ((code = array_get(imemory, pref, ++i, &elt3)) < 0)
                return code;
            else if (resolves_to_oper(i_ctx_p, &elt3, zifelse)) {
                if (ops) {
                    *p = PtCr_if;
                    psc_fixup(p, ops + *psize + 3);
                    p = ops + *psize;
                    *p = PtCr_else;
                }
                *psize += 3;
                code = check_psc_function(i_ctx_p, &elt2, depth + 1, ops, psize, AllowRepeat);
                if (code < 0)
                    return code;
                if (ops)
                    psc_fixup(p, ops + *psize);
            } else
                return_error(gs_error_rangecheck);
            }	 /* end 'default' */
        }
    next:
        DO_NOTHING;
    }
    return 0;
}
#undef MAX_PSC_FUNCTION_NESTING

/* Check prototype */
build_function_proc(gs_build_function_4);

/* Finish building a FunctionType 4 (PostScript Calculator) function. */
int
gs_build_function_4(i_ctx_t *i_ctx_p, const ref *op, const gs_function_params_t * mnDR,
                    int depth, gs_function_t ** ppfn, gs_memory_t *mem)
{
    gs_function_PtCr_params_t params;
    ref *proc;
    int code;
    byte *ops;
    int size;
    int AllowRepeat = 1; /* Default to permitting Repeat, devices which can't handle it implement the spec_op */

    *(gs_function_params_t *)&params = *mnDR;
    params.ops.data = 0;	/* in case of failure */
    params.ops.size = 0;	/* ditto */
    if (dict_find_string(op, "Function", &proc) <= 0) {
        code = gs_note_error(gs_error_rangecheck);
        goto fail;
    }
    if (!r_is_proc(proc)) {
        code = gs_note_error(gs_error_typecheck);
        goto fail;
    }
    size = 0;

    /* Check if the device allows the use of repeat in functions */
    /* We can't handle 'repeat' with pdfwrite since it emits FunctionType 4 */
    {
        char data[] = {"AllowPSRepeatFunctions"};
        dev_param_req_t request;
        gs_c_param_list list;

        gs_c_param_list_write(&list, i_ctx_p->pgs->device->memory);
        /* Stuff the data into a structure for passing to the spec_op */
        request.Param = data;
        request.list = &list;
        code = dev_proc(i_ctx_p->pgs->device, dev_spec_op)(i_ctx_p->pgs->device, gxdso_get_dev_param, &request, sizeof(dev_param_req_t));
        if (code < 0 && code != gs_error_undefined) {
            gs_c_param_list_release(&list);
            return code;
        }
        gs_c_param_list_read(&list);
        code = param_read_bool((gs_param_list *)&list,
            "AllowPSRepeatFunctions",
            &AllowRepeat);
        gs_c_param_list_release(&list);
        if (code < 0)
            return code;
    }

    code = check_psc_function(i_ctx_p, proc, 0, NULL, &size, AllowRepeat);
    if (code < 0)
        goto fail;
    ops = gs_alloc_string(mem, size + 1, "gs_build_function_4(ops)");
    if (ops == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto fail;
    }
    size = 0;
    check_psc_function(i_ctx_p, proc, 0, ops, &size, AllowRepeat); /* can't fail */
    ops[size] = PtCr_return;
    params.ops.data = ops;
    params.ops.size = size + 1;
    code = gs_function_PtCr_init(ppfn, &params, mem);
    if (code >= 0)
        return 0;
    /* free_params will free the ops string */
fail:
    gs_function_PtCr_free_params(&params, mem);
    return code;
}

int make_type4_function(i_ctx_t * i_ctx_p, ref *arr, ref *pproc, gs_function_t **func)
{
    int code, size, num_components, CIESubst;
    byte *ops;
    gs_function_PtCr_params_t params;
    float *ptr;
    ref alternatespace, *palternatespace = &alternatespace;
    PS_colour_space_t *space, *altspace;
    int AllowRepeat = 1; /* Default to permitting Repeat, devices which can't handle it implement the spec_op */

    code = get_space_object(i_ctx_p, arr, &space);
    if (code < 0)
        return code;
    if (!space->alternateproc)
        return gs_error_typecheck;
    code = space->alternateproc(i_ctx_p, arr, &palternatespace, &CIESubst);
    if (code < 0)
        return code;
    code = get_space_object(i_ctx_p, palternatespace, &altspace);
    if (code < 0)
        return code;

    code = space->numcomponents(i_ctx_p, arr, &num_components);
    if (code < 0)
        return code;
    ptr = (float *)gs_alloc_byte_array(imemory, num_components * 2, sizeof(float), "make_type4_function(Domain)");
    if (!ptr)
        return gs_error_VMerror;
    code = space->domain(i_ctx_p, arr, ptr);
    if (code < 0) {
        gs_free_const_object(imemory, ptr, "make_type4_function(Domain)");
        return code;
    }
    params.Domain = ptr;
    params.m = num_components;

    code = altspace->numcomponents(i_ctx_p, &alternatespace, &num_components);
    if (code < 0) {
        gs_free_const_object(imemory, params.Domain, "make_type4_function(Domain)");
        return code;
    }
    ptr = (float *)gs_alloc_byte_array(imemory, num_components * 2, sizeof(float), "make_type4_function(Range)");
    if (!ptr) {
        gs_free_const_object(imemory, params.Domain, "make_type4_function(Domain)");
        return gs_error_VMerror;
    }
    code = altspace->range(i_ctx_p, &alternatespace, ptr);
    if (code < 0) {
        gs_free_const_object(imemory, ptr, "make_type4_function(Domain)");
        gs_free_const_object(imemory, params.Domain, "make_type4_function(Range)");
        return code;
    }
    params.Range = ptr;
    params.n = num_components;

    params.ops.data = 0;	/* in case of failure, see gs_function_PtCr_free_params */
    params.ops.size = 0;	/* ditto */
    size = 0;

    /* Check if the device allows the use of repeat in functions */
    /* We can't handle 'repeat' with pdfwrite since it emits FunctionType 4 */
    {
        char data[] = {"AllowPSRepeatFunctions"};
        dev_param_req_t request;
        gs_c_param_list list;

        gs_c_param_list_write(&list, i_ctx_p->pgs->device->memory);
        /* Stuff the data into a structure for passing to the spec_op */
        request.Param = data;
        request.list = &list;
        code = dev_proc(i_ctx_p->pgs->device, dev_spec_op)(i_ctx_p->pgs->device, gxdso_get_dev_param, &request, sizeof(dev_param_req_t));
        if (code < 0 && code != gs_error_undefined) {
            gs_c_param_list_release(&list);
            return code;
        }
        gs_c_param_list_read(&list);
        code = param_read_bool((gs_param_list *)&list,
            "AllowPSRepeatFunctions",
            &AllowRepeat);
        gs_c_param_list_release(&list);
        if (code < 0)
            return code;
    }

    code = check_psc_function(i_ctx_p, (const ref *)pproc, 0, NULL, &size, AllowRepeat);
    if (code < 0) {
        gs_function_PtCr_free_params(&params, imemory);
        return code;
    }
    ops = gs_alloc_string(imemory, size + 1, "make_type4_function(ops)");
    size = 0;
    check_psc_function(i_ctx_p, (const ref *)pproc, 0, ops, &size, AllowRepeat); /* can't fail */
    ops[size] = PtCr_return;
    params.ops.data = ops;
    params.ops.size = size + 1;
    code = gs_function_PtCr_init(func, &params, imemory);
    if (code < 0)
        gs_function_PtCr_free_params(&params, imemory);

    return code;
}
