/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Basic path operators */
#include "math_.h"
#include "ghost.h"
#include "oper.h"
#include "igstate.h"
#include "gsmatrix.h"
#include "gspath.h"
#include "store.h"

/* Forward references */
private int common_to(P2(os_ptr,
			 int (*)(P3(gs_state *, floatp, floatp))));
private int common_curve(P2(os_ptr,
  int (*)(P7(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp))));

/* - newpath - */
private int
znewpath(register os_ptr op)
{
    return gs_newpath(igs);
}

/* - currentpoint <x> <y> */
private int
zcurrentpoint(register os_ptr op)
{
    gs_point pt;
    int code = gs_currentpoint(igs, &pt);

    if (code < 0)
	return code;
    push(2);
    make_real(op - 1, pt.x);
    make_real(op, pt.y);
    return 0;
}

/* <x> <y> moveto - */
int
zmoveto(os_ptr op)
{
    return common_to(op, gs_moveto);
}

/* <dx> <dy> rmoveto - */
int
zrmoveto(os_ptr op)
{
    return common_to(op, gs_rmoveto);
}

/* <x> <y> lineto - */
int
zlineto(os_ptr op)
{
    return common_to(op, gs_lineto);
}

/* <dx> <dy> rlineto - */
int
zrlineto(os_ptr op)
{
    return common_to(op, gs_rlineto);
}

/* Common code for [r](move/line)to */
private int
common_to(os_ptr op, int (*add_proc)(P3(gs_state *, floatp, floatp)))
{
    double opxy[2];
    int code;

    if ((code = num_params(op, 2, opxy)) < 0 ||
	(code = (*add_proc)(igs, opxy[0], opxy[1])) < 0
	)
	return code;
    pop(2);
    return 0;
}

/* <x1> <y1> <x2> <y2> <x3> <y3> curveto - */
int
zcurveto(register os_ptr op)
{
    return common_curve(op, gs_curveto);
}

/* <dx1> <dy1> <dx2> <dy2> <dx3> <dy3> rcurveto - */
int
zrcurveto(register os_ptr op)
{
    return common_curve(op, gs_rcurveto);
}

/* Common code for [r]curveto */
private int
common_curve(os_ptr op,
	     int (*add_proc)(P7(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp)))
{
    double opxy[6];
    int code;

    if ((code = num_params(op, 6, opxy)) < 0)
	return code;
    code = (*add_proc)(igs, opxy[0], opxy[1], opxy[2], opxy[3], opxy[4], opxy[5]);
    if (code >= 0)
	pop(6);
    return code;
}

/* - closepath - */
int
zclosepath(register os_ptr op)
{
    return gs_closepath(igs);
}

/* - initclip - */
private int
zinitclip(register os_ptr op)
{
    return gs_initclip(igs);
}

/* - clip - */
private int
zclip(register os_ptr op)
{
    return gs_clip(igs);
}

/* - eoclip - */
private int
zeoclip(register os_ptr op)
{
    return gs_eoclip(igs);
}

/* <bool> .setclipoutside - */
private int
zsetclipoutside(register os_ptr op)
{
    int code;

    check_type(*op, t_boolean);
    code = gs_setclipoutside(igs, op->value.boolval);
    if (code >= 0)
	pop(1);
    return code;
}

/* - .currentclipoutside <bool> */
private int
zcurrentclipoutside(register os_ptr op)
{
    push(1);
    make_bool(op, gs_currentclipoutside(igs));
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zpath_op_defs[] =
{
    {"0clip", zclip},
    {"0closepath", zclosepath},
    {"0.currentclipoutside", zcurrentclipoutside},
    {"0currentpoint", zcurrentpoint},
    {"6curveto", zcurveto},
    {"0eoclip", zeoclip},
    {"0initclip", zinitclip},
    {"2lineto", zlineto},
    {"2moveto", zmoveto},
    {"0newpath", znewpath},
    {"6rcurveto", zrcurveto},
    {"2rlineto", zrlineto},
    {"2rmoveto", zrmoveto},
    {"1.setclipoutside", zsetclipoutside},
    op_def_end(0)
};
