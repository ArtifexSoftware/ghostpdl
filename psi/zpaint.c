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


/* Painting operators */
#include "ghost.h"
#include "oper.h"
#include "gspaint.h"
#include "igstate.h"
#include "store.h"
#include "estack.h"

/* - fill - */
static int
zfill(i_ctx_t *i_ctx_p)
{
    return gs_fill(igs);
}

/* - eofill - */
static int
zeofill(i_ctx_t *i_ctx_p)
{
    return gs_eofill(igs);
}

/* - stroke - */
static int
zstroke(i_ctx_t *i_ctx_p)
{
    return gs_stroke(igs);
}

static int
fillstroke_cont(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int restart, code;

    check_op(1);
    check_type(*op, t_integer);
    restart = (int)op->value.intval;
    code = gs_fillstroke(igs, &restart);
    if (code == gs_error_Remap_Color) {
        op->value.intval = restart;
        return code;
    }
    pop(1);
    return code;
}

static int
zfillstroke(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    push(1);
    check_estack(1);
    make_int(op, 0);		/* 0 implies we are at fill color, need to swap first */
    push_op_estack(fillstroke_cont);
    return o_push_estack;
}

static int
eofillstroke_cont(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int restart, code;

    check_op(1);
    check_type(*op, t_integer);
    restart = (int)op->value.intval;
    code = gs_eofillstroke(igs, &restart);
    if (code == gs_error_Remap_Color) {
        op->value.intval = restart;
        return code;
    }
    pop(1);
    return code;
}

static int
zeofillstroke(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    push(1);
    make_int(op, 0);
    push_op_estack(eofillstroke_cont);
    return o_push_estack;
}

/* ------ Non-standard operators ------ */

/* - .fillpage - */
static int
zfillpage(i_ctx_t *i_ctx_p)
{
    return gs_fillpage(igs);
}

/* <width> <height> <data> .imagepath - */
static int
zimagepath(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;

    check_op(3);
    check_type(op[-2], t_integer);
    check_type(op[-1], t_integer);
    check_read_type(*op, t_string);
    if (r_size(op) < ((op[-2].value.intval + 7) >> 3) * op[-1].value.intval)
        return_error(gs_error_rangecheck);
    code = gs_imagepath(igs,
                        (int)op[-2].value.intval, (int)op[-1].value.intval,
                        op->value.const_bytes);
    if (code >= 0)
        pop(3);
    return code;
}

/* ------ Initialization procedure ------ */

const op_def zpaint_op_defs[] =
{
    {"0eofill", zeofill},
    {"0fill", zfill},
    {"0stroke", zstroke},
                /* Non-standard operators */
    {"0.fillpage", zfillpage},
    {"3.imagepath", zimagepath},
    {"0.eofillstroke", zeofillstroke},
    {"0.fillstroke", zfillstroke},
    {"0%eofillstroke_cont", eofillstroke_cont },
    {"0%fillstroke_cont", fillstroke_cont },
    op_def_end(0)
};
