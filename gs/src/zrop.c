/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zrop.c */
/* RasterOp control operators */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
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
zsetrasterop(register os_ptr op)
{	int param;
	int code = int_param(op, 0xff, &param);
	if ( code < 0 )
	  return code;
	gs_setrasterop(igs, (gs_rop3_t)param);
	pop(1);
	return 0;
}

/* - .currentrasterop <int8> */
private int
zcurrentrasterop(register os_ptr op)
{	push(1);
	make_int(op, (int)gs_currentrasterop(igs));
	return 0;
}

/* <int> .setrenderalgorithm - */
private int
zsetrenderalgorithm(register os_ptr op)
{	int param;
	int code = int_param(op, render_algorithm_max, &param);
	if ( code < 0 )
	  return code;
	gs_setrenderalgorithm(igs, param);
	pop(1);
	return 0;
}

/* - .currentrenderalgorithm <int> */
private int
zcurrentrenderalgorithm(register os_ptr op)
{	push(1);
	make_int(op, gs_currentrenderalgorithm(igs));
	return 0;
}
	
/* <bool> .setsourcetransparent - */
private int
zsetsourcetransparent(register os_ptr op)
{	check_type(*op, t_boolean);
	gs_setsourcetransparent(igs, op->value.boolval);
	pop(1);
	return 0;
}

/* - .currentsourcetransparent <bool> */
private int
zcurrentsourcetransparent(register os_ptr op)
{	push(1);
	make_bool(op, gs_currentsourcetransparent(igs));
	return 0;
}
	
/* <bool> .settexturetransparent - */
private int
zsettexturetransparent(register os_ptr op)
{	check_type(*op, t_boolean);
	gs_settexturetransparent(igs, op->value.boolval);
	pop(1);
	return 0;
}

/* - .currenttexturetransparent <bool> */
private int
zcurrenttexturetransparent(register os_ptr op)
{	push(1);
	make_bool(op, gs_currenttexturetransparent(igs));
	return 0;
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zrop_op_defs) {
	{"0.currentrasterop", zcurrentrasterop},
	{"0.currentrenderalgorithm", zcurrentrenderalgorithm},
	{"0.currentsourcetransparent", zcurrentsourcetransparent},
	{"0.currenttexturetransparent", zcurrenttexturetransparent},
	{"1.setrasterop", zsetrasterop},
	{"1.setrenderalgorithm", zsetrenderalgorithm},
	{"1.setsourcetransparent", zsetsourcetransparent},
	{"1.settexturetransparent", zsettexturetransparent},
END_OP_DEFS(0) }
