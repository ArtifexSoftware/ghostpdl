/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Arithmetic operators */
#include "math_.h"
#include "ghost.h"
#include "oper.h"
#include "store.h"
#include "gsstate.h"

/*
 * Many of the procedures in this file are public only so they can be
 * called from the FunctionType 4 interpreter (zfunc4.c).
 */

static int mul_64_64_overflowcheck(int64_t abc, int64_t def, int64_t *res);

/* <num1> <num2> add <sum> */
/* We make this into a separate procedure because */
/* the interpreter will almost always call it directly. */
int
zop_add(i_ctx_t *i_ctx_p)
{
    register os_ptr op = osp;
    float result;

    switch (r_type(op)) {
    default:
        return_op_typecheck(op);
    case t_real:
        switch (r_type(op - 1)) {
        default:
            return_op_typecheck(op - 1);
        case t_real:
            result = op[-1].value.realval + op->value.realval;
#ifdef HAVE_ISINF
            if (isinf(result))
                return_error(gs_error_undefinedresult);
#endif
#ifdef HAVE_ISNAN
            if (isnan(result))
                return_error(gs_error_undefinedresult);
#endif
            op[-1].value.realval = result;
            break;
        case t_integer:
            make_real(op - 1, (double)op[-1].value.intval + op->value.realval);
        }
        break;
    case t_integer:
        switch (r_type(op - 1)) {
        default:
            return_op_typecheck(op - 1);
        case t_real:
            result = op[-1].value.realval + (double)op->value.intval;
#ifdef HAVE_ISINF
            if (isinf(result))
                return_error(gs_error_undefinedresult);
#endif
#ifdef HAVE_ISNAN
            if (isnan(result))
                return_error(gs_error_undefinedresult);
#endif
            op[-1].value.realval = result;
            break;
        case t_integer: {
            if (sizeof(ps_int) != 4 && gs_currentcpsimode(imemory)) {
                ps_int32 int1 = (ps_int32)op[-1].value.intval;
                ps_int32 int2 = (ps_int32)op->value.intval;

                if (((int1 += int2) ^ int2) < 0 &&
                    ((int1 - int2) ^ int2) >= 0
                    ) {                     /* Overflow, convert to real */
                    make_real(op - 1, (double)(int1 - int2) + int2);
                }
                else {
                    op[-1].value.intval = (ps_int)int1;
                }
            }
            else {
                ps_int int2 = op->value.intval;

                if (((op[-1].value.intval += int2) ^ int2) < 0 &&
                    ((op[-1].value.intval - int2) ^ int2) >= 0
                    ) {                     /* Overflow, convert to real */
                    make_real(op - 1, (double)(op[-1].value.intval - int2) + int2);
                }
            }
        }
        }
    }
    return 0;
}
int
zadd(i_ctx_t *i_ctx_p)
{
    int code = zop_add(i_ctx_p);

    if (code == 0) {
        pop(1);
    }
    return code;
}

/* <num1> <num2> div <real_quotient> */
int
zdiv(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr op1 = op - 1;
    float result;

    /* We can't use the non_int_cases macro, */
    /* because we have to check explicitly for op == 0. */
    switch (r_type(op)) {
        default:
            return_op_typecheck(op);
        case t_real:
            if (op->value.realval == 0)
                return_error(gs_error_undefinedresult);
            switch (r_type(op1)) {
                default:
                    return_op_typecheck(op1);
                case t_real:
                    result = op1->value.realval / op->value.realval;
#ifdef HAVE_ISINF
                    if (isinf(result))
                        return_error(gs_error_undefinedresult);
#endif
#ifdef HAVE_ISNAN
                    if (isnan(result))
                        return_error(gs_error_undefinedresult);
#endif
                    op1->value.realval = result;
                    break;
                case t_integer:
                    result = (double)op1->value.intval / op->value.realval;
                    make_real(op1, result);
            }
            break;
        case t_integer:
            if (op->value.intval == 0)
                return_error(gs_error_undefinedresult);
            switch (r_type(op1)) {
                default:
                    return_op_typecheck(op1);
                case t_real:
                    result = op1->value.realval / (double)op->value.intval;
#ifdef HAVE_ISINF
                    if (isinf(result))
                        return_error(gs_error_undefinedresult);
#endif
#ifdef HAVE_ISNAN
                    if (isnan(result))
                        return_error(gs_error_undefinedresult);
#endif
                    op1->value.realval = result;
                    break;
                case t_integer:
                    result = (double)op1->value.intval / (double)op->value.intval;
#ifdef HAVE_ISINF
                    if (isinf(result))
                        return_error(gs_error_undefinedresult);
#endif
#ifdef HAVE_ISNAN
                    if (isnan(result))
                        return_error(gs_error_undefinedresult);
#endif
                    make_real(op1, result);
            }
    }
    pop(1);
    return 0;
}

/*
To detect 64bit x 64bit multiplication overflowing, consider
breaking the numbers down into 32bit chunks.

  abc = (a<<64) + (b<<32) + c
      (where a is 0 or -1, and b and c are 32bit unsigned.

Similarly:

  def = (d<<64) + (b<<32) + f

Then:

  abc.def = ((a<<64) + (b<<32) + c) * ((d<<64) + (e<<32) + f)
          = (a<<64).def + (d<<64).abc + (b<<32).(e<<32) +
            (b<<32).f + (e<<32).c + cf
          = (a.def + d.abc + b.e)<<64 + (b.f + e.c)<<32 + cf

*/

static int mul_64_64_overflowcheck(int64_t abc, int64_t def, int64_t *res)
{
  uint32_t b = (abc>>32);
  uint32_t c = (uint32_t)abc;
  uint32_t e = (def>>32);
  uint32_t f = (uint32_t)def;
  uint64_t low, mid, high, bf, ec;

  /* Low contribution */
  low = (uint64_t)c * (uint64_t)f;
  /* Mid contributions */
  bf = (uint64_t)b * (uint64_t)f;
  ec = (uint64_t)e * (uint64_t)c;
  /* Top contribution */
  high = (uint64_t)b * (uint64_t)e;
  if (abc < 0)
      high -= def;
  if (def < 0)
      high -= abc;
  /* How do we check for carries from 64bit unsigned adds?
   *  x + y >= (1<<64) == x >= (1<<64) - y
   *                   == x >  (1<<64) - y - 1
   * if we consider just 64bits, this is:
   * x > NOT y
   */
  if (bf > ~ec)
      high += ((uint64_t)1)<<32;
  mid = bf + ec;
  if (low > ~(mid<<32))
      high += 1;
  high += (mid>>32);
  low += (mid<<32);

  *res = low;

  return (int64_t)low < 0 ? high != -1 : high != 0;
}

/* <num1> <num2> mul <product> */
int
zmul(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    float result;

    switch (r_type(op)) {
    default:
        return_op_typecheck(op);
    case t_real:
        switch (r_type(op - 1)) {
        default:
            return_op_typecheck(op - 1);
        case t_real:
            result = op[-1].value.realval * op->value.realval;
#ifdef HAVE_ISINF
            if (isinf(result))
                return_error(gs_error_undefinedresult);
#endif
#ifdef HAVE_ISNAN
            if (isnan(result))
                return_error(gs_error_undefinedresult);
#endif
            op[-1].value.realval = result;
            break;
        case t_integer:
            result = (double)op[-1].value.intval * op->value.realval;
            make_real(op - 1, result);
        }
        break;
    case t_integer:
        switch (r_type(op - 1)) {
        default:
            return_op_typecheck(op - 1);
        case t_real:
            result = op[-1].value.realval * (double)op->value.intval;
#ifdef HAVE_ISINF
            if (isinf(result))
                return_error(gs_error_undefinedresult);
#endif
#ifdef HAVE_ISNAN
            if (isnan(result))
                return_error(gs_error_undefinedresult);
#endif
            op[-1].value.realval = result;
            break;
        case t_integer: {
            if (sizeof(ps_int) != 4 && gs_currentcpsimode(imemory)) {
                double ab = (double)op[-1].value.intval * op->value.intval;
                if (ab > (double)MAX_PS_INT32)       /* (double)0x7fffffff */
                    make_real(op - 1, ab);
                else if (ab < (double)MIN_PS_INT32) /* (double)(int)0x80000000 */
                    make_real(op - 1, ab);
                else
                    op[-1].value.intval = (ps_int)ab;
            }
            else {
                int64_t result;
                if (mul_64_64_overflowcheck(op[-1].value.intval, op->value.intval, &result)) {
                    double ab = (double)op[-1].value.intval * op->value.intval;
                    make_real(op - 1, ab);
                } else {
                    op[-1].value.intval = result;
                }
            }
        }
        }
    }
    pop(1);
    return 0;
}

/* <num1> <num2> sub <difference> */
/* We make this into a separate procedure because */
/* the interpreter will almost always call it directly. */
int
zop_sub(i_ctx_t *i_ctx_p)
{
    register os_ptr op = osp;

    switch (r_type(op)) {
    default:
        return_op_typecheck(op);
    case t_real:
        switch (r_type(op - 1)) {
        default:
            return_op_typecheck(op - 1);
        case t_real:
            op[-1].value.realval -= op->value.realval;
            break;
        case t_integer:
            make_real(op - 1, (double)op[-1].value.intval - op->value.realval);
        }
        break;
    case t_integer:
        switch (r_type(op - 1)) {
        default:
            return_op_typecheck(op - 1);
        case t_real:
            op[-1].value.realval -= (double)op->value.intval;
            break;
        case t_integer: {
            if (sizeof(ps_int) != 4 && gs_currentcpsimode(imemory)) {
                ps_int32 int1 = (ps_int)op[-1].value.intval;
                ps_int32 int2 = (ps_int)op->value.intval;
                ps_int32 int3;

                if ((int1 ^ (int3 = int1 - int2)) < 0 &&
                    (int1 ^ int2) < 0
                    ) {                     /* Overflow, convert to real */
                    make_real(op - 1, (float)int1 - op->value.intval);
                }
                else {
                    op[-1].value.intval = (ps_int)int3;
                }
            }
            else {
                ps_int int1 = op[-1].value.intval;

                if ((int1 ^ (op[-1].value.intval = int1 - op->value.intval)) < 0 &&
                    (int1 ^ op->value.intval) < 0
                    ) {                     /* Overflow, convert to real */
                    make_real(op - 1, (float)int1 - op->value.intval);
                }
            }
        }
        }
    }
    return 0;
}
int
zsub(i_ctx_t *i_ctx_p)
{
    int code = zop_sub(i_ctx_p);

    if (code == 0) {
        pop(1);
    }
    return code;
}

/* <num1> <num2> idiv <int_quotient> */
int
zidiv(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_integer);
    check_type(op[-1], t_integer);
    if (sizeof(ps_int) && gs_currentcpsimode(imemory)) {
        int tmpval;
        if ((op->value.intval == 0) || (op[-1].value.intval == (ps_int)MIN_PS_INT32 && op->value.intval == -1)) {
            /* Anomalous boundary case: -MININT / -1, fail. */
            return_error(gs_error_undefinedresult);
        }
        tmpval = (int)op[-1].value.intval / op->value.intval;
        op[-1].value.intval = (int64_t)tmpval;
    }
    else {
        if ((op->value.intval == 0) || (op[-1].value.intval == MIN_PS_INT && op->value.intval == -1)) {
            /* Anomalous boundary case: -MININT / -1, fail. */
            return_error(gs_error_undefinedresult);
        }
        op[-1].value.intval /= op->value.intval;
    }
    pop(1);
    return 0;
}

/* <int1> <int2> mod <remainder> */
int
zmod(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_integer);
    check_type(op[-1], t_integer);
    if (op->value.intval == 0)
        return_error(gs_error_undefinedresult);
    op[-1].value.intval %= op->value.intval;
    pop(1);
    return 0;
}

/* <num1> neg <num2> */
int
zneg(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    switch (r_type(op)) {
        default:
            return_op_typecheck(op);
        case t_real:
            op->value.realval = -op->value.realval;
            break;
        case t_integer:
            if (sizeof(ps_int) != 32 && gs_currentcpsimode(imemory)) {
                if (((unsigned int)op->value.intval) == MIN_PS_INT32)
                    make_real(op, -(float)(ps_uint32)MIN_PS_INT32);
                else
                    op->value.intval = -op->value.intval;
            }
            else {
                if (op->value.intval == MIN_PS_INT)
                    make_real(op, -(float)MIN_PS_INT);
                else
                    op->value.intval = -op->value.intval;
            }
    }
    return 0;
}

/* <num1> abs <num2> */
int
zabs(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    switch (r_type(op)) {
        default:
            return_op_typecheck(op);
        case t_real:
            if (op->value.realval >= 0)
                return 0;
            break;
        case t_integer:
            if (op->value.intval >= 0)
                return 0;
            break;
    }
    return zneg(i_ctx_p);
}

/* <num1> ceiling <num2> */
int
zceiling(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    switch (r_type(op)) {
        default:
            return_op_typecheck(op);
        case t_real:
            op->value.realval = ceil(op->value.realval);
        case t_integer:;
    }
    return 0;
}

/* <num1> floor <num2> */
int
zfloor(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    switch (r_type(op)) {
        default:
            return_op_typecheck(op);
        case t_real:
            op->value.realval = floor(op->value.realval);
        case t_integer:;
    }
    return 0;
}

/* <num1> round <num2> */
int
zround(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    switch (r_type(op)) {
        default:
            return_op_typecheck(op);
        case t_real:
            op->value.realval = floor(op->value.realval + 0.5);
        case t_integer:;
    }
    return 0;
}

/* <num1> truncate <num2> */
int
ztruncate(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    switch (r_type(op)) {
        default:
            return_op_typecheck(op);
        case t_real:
            op->value.realval =
                (op->value.realval < 0.0 ?
                 ceil(op->value.realval) :
                 floor(op->value.realval));
        case t_integer:;
    }
    return 0;
}

/* Non-standard operators */

/* <int1> <int2> .bitadd <sum> */
static int
zbitadd(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_integer);
    check_type(op[-1], t_integer);
    op[-1].value.intval += op->value.intval;
    pop(1);
    return 0;
}

/* ------ Initialization table ------ */

const op_def zarith_op_defs[] =
{
    {"1abs", zabs},
    {"2add", zadd},
    {"2.bitadd", zbitadd},
    {"1ceiling", zceiling},
    {"2div", zdiv},
    {"2idiv", zidiv},
    {"1floor", zfloor},
    {"2mod", zmod},
    {"2mul", zmul},
    {"1neg", zneg},
    {"1round", zround},
    {"2sub", zsub},
    {"1truncate", ztruncate},
    op_def_end(0)
};
