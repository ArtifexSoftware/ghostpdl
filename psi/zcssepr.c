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


/* Separation color space support */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gscolor.h"
#include "gsmatrix.h"		/* for gxcolor2.h */
#include "gxcspace.h"
#include "gxfixed.h"		/* ditto */
#include "gxcolor2.h"
#include "estack.h"
#include "ialloc.h"
#include "icsmap.h"
#include "ifunc.h"
#include "igstate.h"
#include "iname.h"
#include "ivmspace.h"
#include "store.h"
#include "gscsepr.h"
#include "gscdevn.h"
#include "gxcdevn.h"
#include "zht2.h"

/* Imported from gscsepr.c */
extern const gs_color_space_type gs_color_space_type_Separation;
/* Imported from gscdevn.c */
extern const gs_color_space_type gs_color_space_type_DeviceN;

/*
 * Adobe first created the separation colorspace type and then later created
 * the DeviceN colorspace.  Logically the separation colorspace is the same
 * as a DeviceN colorspace with a single component, except for the /None and
 * /All parameter values.  We treat the separation colorspace as a DeviceN
 * colorspace except for the /All case.
 */

/* - currentoverprint <bool> */
static int
zcurrentoverprint(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currentoverprint(igs));
    return 0;
}

/* <bool> setoverprint - */
static int
zsetoverprint(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_type(*op, t_boolean);
    gs_setoverprint(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - currentstrokeoverprint <bool> */
static int
zcurrentstrokeoverprint(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currentstrokeoverprint(igs));
    return 0;
}

/* <bool> setstrokeoverprint - */
static int
zsetstrokeoverprint(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_type(*op, t_boolean);
    gs_setstrokeoverprint(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - currentfilloverprint <bool> */
static int
zcurrentfilloverprint(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currentfilloverprint(igs));
    return 0;
}

/* <bool> setfilloverprint - */
static int
zsetfilloverprint(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_type(*op, t_boolean);
    gs_setfilloverprint(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - .currentoverprintmode <int> */
static int
zcurrentoverprintmode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_int(op, gs_currentoverprintmode(igs));
    return 0;
}

/* <int> .setoverprintmode - */
static int
zsetoverprintmode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int param;
    int code;

    check_op(1);
    code = int_param(op, max_int, &param);

    if (code < 0 || (code = gs_setoverprintmode(igs, param)) < 0)
        return code;
    pop(1);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zcssepr_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"0currentoverprint", zcurrentoverprint},
    {"0.currentoverprintmode", zcurrentoverprintmode},
    {"1setoverprint", zsetoverprint},
    {"1.setoverprintmode", zsetoverprintmode},
    {"0.currentstrokeoverprint", zcurrentstrokeoverprint},
    {"1.setstrokeoverprint", zsetstrokeoverprint},
    {"0.currentfilloverprint", zcurrentfilloverprint},
    {"1.setfilloverprint", zsetfilloverprint},
    op_def_end(0)
};
