/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* zpaint.c */
/* Painting operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gspaint.h"
#include "igstate.h"

/* - fill - */
private int
zfill(register os_ptr op)
{	return gs_fill(igs);
}

/* - eofill - */
private int
zeofill(register os_ptr op)
{	return gs_eofill(igs);
}

/* - stroke - */
private int
zstroke(register os_ptr op)
{	return gs_stroke(igs);
}

/* ------ Non-standard operators ------ */

/* - .fillpage - */
private int
zfillpage(register os_ptr op)
{	return gs_fillpage(igs);
}

/* <width> <height> <data> .imagepath - */
private int
zimagepath(register os_ptr op)
{	int code;

	check_type(op[-2], t_integer);
	check_type(op[-1], t_integer);
	check_read_type(*op, t_string);
	if ( r_size(op) < ((op[-2].value.intval + 7) >> 3) * op[-1].value.intval )
	  return_error(e_rangecheck);
	code = gs_imagepath(igs,
		(int)op[-2].value.intval, (int)op[-1].value.intval,
		op->value.const_bytes);
	if ( code >= 0 )
	  pop(3);
	return code;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zpaint_op_defs) {
	{"0eofill", zeofill},
	{"0fill", zfill},
	{"0stroke", zstroke},
		/* Non-standard operators */
	{"0.fillpage", zfillpage},
	{"3.imagepath", zimagepath},
END_OP_DEFS(0) }
