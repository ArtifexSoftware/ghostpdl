/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* RasterOp control operators */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsrop.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "store.h"

/* <int8> .setrasterop - */
private int
zsetrasterop(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int param;
    int code = int_param(op, 0xff, &param);

    if (code < 0)
	return code;
    gs_setrasterop(igs, (gs_rop3_t)param);
    pop(1);
    return 0;
}

/* - .currentrasterop <int8> */
private int
zcurrentrasterop(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, (int)gs_currentrasterop(igs));
    return 0;
}

/* <bool> .setsourcetransparent - */
private int
zsetsourcetransparent(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_boolean);
    gs_setsourcetransparent(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - .currentsourcetransparent <bool> */
private int
zcurrentsourcetransparent(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currentsourcetransparent(igs));
    return 0;
}

/* <bool> .settexturetransparent - */
private int
zsettexturetransparent(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_boolean);
    gs_settexturetransparent(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - .currenttexturetransparent <bool> */
private int
zcurrenttexturetransparent(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currenttexturetransparent(igs));
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zrop_op_defs[] =
{
    {"0.currentrasterop", zcurrentrasterop},
    {"0.currentsourcetransparent", zcurrentsourcetransparent},
    {"0.currenttexturetransparent", zcurrenttexturetransparent},
    {"1.setrasterop", zsetrasterop},
    {"1.setsourcetransparent", zsetsourcetransparent},
    {"1.settexturetransparent", zsettexturetransparent},
    op_def_end(0)
};
