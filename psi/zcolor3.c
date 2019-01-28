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


/* Level 3 color operators */
#include "ghost.h"
#include "oper.h"
#include "igstate.h"
#include "store.h"

/*
 *  <bool>   .setuseciecolor  -
 *
 * Set the use_cie_color parameter for the interpreter state, which
 * corresponds to the UseCIEColor page device parameter. This parameter
 * may be read at all language levels, but it may be set only for
 * language level 3. The parameter is handled separately from the page
 * device dictionary primarily for performance reasons (it may need to
 * be checked frequently), but also to ensure proper language level
 * specific behavior.
 *
 * This operator is accessible only during initialization and is called
 * only under controlled conditions. Hence, it does not do any operand
 * checking.
 */
static int
zsetuseciecolor(i_ctx_t * i_ctx_p)
{
    os_ptr  op = osp;
    check_type(*op, t_boolean);

    istate->use_cie_color = *op;
    pop(1);
    return 0;
}

/* - .currentrenderingintent <int> */
static int
zcurrentrenderingintent(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, gs_currentrenderingintent(igs));
    return 0;
}

/* <int> .setrenderingintent -
 * See the comment in gsstate.c about the argumet interepretation.
 */
static int
zsetrenderingintent(i_ctx_t * i_ctx_p)
{
    os_ptr op = osp;
    int param;
    int code = int_param(op, max_int, &param);

    if (code < 0 || (code = gs_setrenderingintent(igs, param)) < 0)
        return code;
    pop(1);
    return 0;
}

/* - .currentblackptcomp <int> */
static int
zcurrentblackptcomp(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, gs_currentblackptcomp(igs));
    return 0;
}

/* <int> .setblackptcomp */
static int
zsetblackptcomp(i_ctx_t * i_ctx_p)
{
    os_ptr op = osp;
    int param;
    int code = int_param(op, max_int, &param);

    if (code < 0 || (code = gs_setblackptcomp(igs, param)) < 0)
        return code;
    pop(1);
    return 0;
}

/*
 * Initialization procedure
 */

const op_def    zcolor3_l3_op_defs[] = {
    op_def_begin_ll3(),
    { "1.setuseciecolor", zsetuseciecolor },
    { "0.currentrenderintent", zcurrentrenderingintent },
    { "1.setrenderingintent", zsetrenderingintent },
    { "0.currentblackptcomp", zcurrentblackptcomp },
    { "1.setblackptcomp", zsetblackptcomp },
    op_def_end(0)
};
