/* Copyright (C) 1994, 1997, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* HSB color operators */
#include "ghost.h"
#include "oper.h"
#include "igstate.h"
#include "store.h"
#include "gshsb.h"

/* - currenthsbcolor <hue> <saturation> <brightness> */
private int
zcurrenthsbcolor(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    float par[3];

    gs_currenthsbcolor(igs, par);
    push(3);
    make_floats(op - 2, par, 3);
    return 0;
}

/* <hue> <saturation> <brightness> sethsbcolor - */
private int
zsethsbcolor(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    double par[3];
    int code;

    if ((code = num_params(op, 3, par)) < 0 ||
	(code = gs_sethsbcolor(igs, par[0], par[1], par[2])) < 0
	)
	return code;
    make_null(&istate->colorspace.array);
    pop(3);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zhsb_op_defs[] =
{
    {"0currenthsbcolor", zcurrenthsbcolor},
    {"3sethsbcolor", zsethsbcolor},
    op_def_end(0)
};
