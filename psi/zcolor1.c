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


/* Level 1 extended color operators */
#include "ghost.h"
#include "oper.h"
#include "estack.h"
#include "ialloc.h"
#include "igstate.h"
#include "iutil.h"
#include "store.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gscolor1.h"
#include "gxcspace.h"
#include "icolor.h"
#include "iimage.h"
#include "gxhttype.h"

/* imported from gsht.c */
extern  void    gx_set_effective_transfer(gs_gstate *);

/* - currentblackgeneration <proc> */
static int
zcurrentblackgeneration(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    *op = istate->black_generation;
    return 0;
}

/* - currentcolortransfer <redproc> <greenproc> <blueproc> <grayproc> */
static int
zcurrentcolortransfer(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(4);
    op[-3] = istate->transfer_procs.red;
    op[-2] = istate->transfer_procs.green;
    op[-1] = istate->transfer_procs.blue;
    *op = istate->transfer_procs.gray;
    return 0;
}

/* - currentundercolorremoval <proc> */
static int
zcurrentundercolorremoval(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    *op = istate->undercolor_removal;
    return 0;
}

/* <proc> setblackgeneration - */
static int
zsetblackgeneration(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code, identity = r_size(op) == 0;
    gx_transfer_map *map = NULL, *map1;

    check_op(1);
    check_proc(*op);
    check_ostack(zcolor_remap_one_ostack - 1);
    check_estack(2 + zcolor_remap_one_estack);

    if (!identity) {
        map = igs->black_generation;
        if (map != NULL)
            rc_increment(map);
    }

    code = gs_setblackgeneration_remap(igs, gs_mapped_transfer, false);
    if (code < 0) {
        if (map != NULL)
            rc_decrement(map, "setblackgeneration");
        return code;
    }
    istate->black_generation = *op;
    ref_stack_pop(&o_stack, 1);
    push_op_estack(zcolor_remap_color);

    if (!identity) {
        map1 = igs->black_generation;
        igs->black_generation = map;
        map = map1;
        return zcolor_remap_one(i_ctx_p, &istate->black_generation,
                            map, igs,
                            setblackgeneration_remap_one_finish);
    } else
        return zcolor_remap_one(i_ctx_p, &istate->black_generation,
                            igs->black_generation, igs,
                            setblackgeneration_remap_one_finish);
}

/* <redproc> <greenproc> <blueproc> <grayproc> setcolortransfer - */
static int
zsetcolortransfer(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr ep = esp;
    int code;
    gx_transfer txfer, txfer1;

    check_op(4);
    check_proc(op[-3]);
    check_proc(op[-2]);
    check_proc(op[-1]);
    check_proc(*op);
    check_ostack(zcolor_remap_one_ostack * 4 - 4);
    check_estack(1 + zcolor_remap_one_estack * 4);

    txfer = igs->set_transfer;
    if (txfer.red != NULL)
        rc_increment(txfer.red);
    if (txfer.green != NULL)
        rc_increment(txfer.green);
    if (txfer.blue != NULL)
        rc_increment(txfer.blue);
    if (txfer.gray != NULL)
        rc_increment(txfer.gray);

    if ((code = gs_setcolortransfer_remap(igs,
                                     gs_mapped_transfer, gs_mapped_transfer,
                                     gs_mapped_transfer, gs_mapped_transfer,
                                          false)) < 0
                                          ) {
        rc_decrement(txfer.red, "setcolortransfer");
        rc_decrement(txfer.green, "setcolortransfer");
        rc_decrement(txfer.blue, "setcolortransfer");
        rc_decrement(txfer.gray, "setcolortransfer");
        return code;
    }
    istate->transfer_procs.red = op[-3];
    istate->transfer_procs.green = op[-2];
    istate->transfer_procs.blue = op[-1];
    istate->transfer_procs.gray = *op;

    /* Use osp rather than op here, because zcolor_remap_one pushes. */
    ref_stack_pop(&o_stack, 4);
    push_op_estack(zcolor_reset_transfer);

    txfer1 = igs->set_transfer;
    igs->set_transfer = txfer;
    txfer = txfer1;
    gx_set_effective_transfer(igs);

    if (r_size(&istate->transfer_procs.red) == 0) {
        rc_decrement(igs->set_transfer.red, "");
        igs->set_transfer.red = txfer.red;
        gx_set_effective_transfer(igs);
        igs->set_transfer.red_component_num = txfer.red_component_num;
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.red,
                                 igs->set_transfer.red, igs,
                                 zcolor_remap_one_finish);
    }
    else
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.red,
                                 txfer.red, igs,
                                 transfer_remap_red_finish);
    if (code < 0) {
        esp = ep;
        return code;
    }
    if (r_size(&istate->transfer_procs.green) == 0) {
        rc_decrement(igs->set_transfer.green, "");
        igs->set_transfer.green = txfer.green;
        gx_set_effective_transfer(igs);
        igs->set_transfer.green_component_num = txfer.green_component_num;
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.green,
                                 igs->set_transfer.green, igs,
                                 zcolor_remap_one_finish);
    }
    else
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.green,
                                 txfer.green, igs,
                                 transfer_remap_green_finish);
    if (code < 0) {
        esp = ep;
        return code;
    }
    if (r_size(&istate->transfer_procs.blue) == 0) {
        rc_decrement(igs->set_transfer.blue, "");
        igs->set_transfer.blue = txfer.blue;
        gx_set_effective_transfer(igs);
        igs->set_transfer.blue_component_num = txfer.blue_component_num;
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.blue,
                                 igs->set_transfer.blue, igs,
                                 zcolor_remap_one_finish);
    }
    else
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.blue,
                                 txfer.blue, igs,
                                 transfer_remap_blue_finish);
    if (code < 0) {
        esp = ep;
        return code;
    }
    if (r_size(&istate->transfer_procs.gray) == 0) {
        rc_decrement(igs->set_transfer.gray, "");
        igs->set_transfer.gray = txfer.gray;
        gx_set_effective_transfer(igs);
        igs->set_transfer.gray_component_num = txfer.gray_component_num;
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.gray,
                                 igs->set_transfer.gray, igs,
                                 zcolor_remap_one_finish);
    }
    else
        code = zcolor_remap_one(i_ctx_p,
                                 &istate->transfer_procs.gray,
                                 txfer.gray, igs,
                                 transfer_remap_gray_finish);
    if (code < 0) {
        esp = ep;
        return code;
    }

    return o_push_estack;
}

/* <proc> setundercolorremoval - */
static int
zsetundercolorremoval(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code, identity = r_size(op) == 0;
    gx_transfer_map *map = NULL, *map1;

    check_op(1);
    check_proc(*op);
    check_ostack(zcolor_remap_one_ostack - 1);
    check_estack(2 + zcolor_remap_one_estack);

    if (!identity) {
        map = igs->undercolor_removal;
        if (map != NULL)
            rc_increment(map);
    }

    code = gs_setundercolorremoval_remap(igs, gs_mapped_transfer, false);
    if (code < 0) {
        if (map != NULL)
            rc_decrement(map, "setundercolorremoval");
        return code;
    }
    istate->undercolor_removal = *op;
    ref_stack_pop(&o_stack, 1);
    push_op_estack(zcolor_remap_color);

    if (!identity) {
        map1 = igs->undercolor_removal;
        igs->undercolor_removal = map;
        map = map1;
        return zcolor_remap_one(i_ctx_p, &istate->undercolor_removal,
                            map, igs,
                            setundercolor_remap_one_signed_finish);
    }
    return zcolor_remap_one(i_ctx_p, &istate->undercolor_removal,
                            igs->undercolor_removal, igs,
                            setundercolor_remap_one_signed_finish);
}

/* ------ Initialization procedure ------ */

const op_def zcolor1_op_defs[] =
{
    {"0currentblackgeneration", zcurrentblackgeneration},
    {"0currentcolortransfer", zcurrentcolortransfer},
    {"0currentundercolorremoval", zcurrentundercolorremoval},
    {"1setblackgeneration", zsetblackgeneration},
    {"4setcolortransfer", zsetcolortransfer},
    {"1setundercolorremoval", zsetundercolorremoval},
    op_def_end(0)
};
