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

/* gsrop.c */
/* RasterOp / transparency / render algorithm accessing for library */
#include "gx.h"
#include "gserrors.h"
#include "gzstate.h"
#include "gsrop.h"

#define set_log_op(pgs, lopv)\
  (pgs)->log_op = (lopv)

/* setrasterop */
void
gs_setrasterop(gs_state *pgs, gs_rop3_t rop)
{	set_log_op(pgs, (rop & rop3_1) | (pgs->log_op & ~rop3_1));
}

/* currentrasterop */
gs_rop3_t
gs_currentrasterop(const gs_state *pgs)
{	return lop_rop(pgs->log_op);
}

/* setsourcetransparent */
void
gs_setsourcetransparent(gs_state *pgs, bool transparent)
{	set_log_op(pgs,
		   (transparent ? pgs->log_op | lop_S_transparent:
		    pgs->log_op & ~lop_S_transparent));
}

/* currentsourcetransparent */
bool
gs_currentsourcetransparent(const gs_state *pgs)
{	return (pgs->log_op & lop_S_transparent) != 0;
}

/* settexturetransparent */
void
gs_settexturetransparent(gs_state *pgs, bool transparent)
{	set_log_op(pgs,
		   (transparent ? pgs->log_op | lop_T_transparent:
		    pgs->log_op & ~lop_T_transparent));
}

/* currenttexturetransparent */
bool
gs_currenttexturetransparent(const gs_state *pgs)
{	return (pgs->log_op & lop_T_transparent) != 0;
}

/* setrenderalgorithm */
int
gs_setrenderalgorithm(gs_state *pgs, int render_algorithm)
{	if ( render_algorithm < render_algorithm_min ||
	     render_algorithm > render_algorithm_max
	   )
	  return_error(gs_error_rangecheck);
	set_log_op(pgs,
		   (render_algorithm << lop_ral_shift) |
		   (pgs->log_op & ~(lop_ral_mask << lop_ral_shift)));
	return 0;
}

/* currentrenderalgorithm */
int
gs_currentrenderalgorithm(const gs_state *pgs)
{	return (pgs->log_op >> lop_ral_shift) & lop_ral_mask;
}

/* Save/restore logical operation. */
void
gs_set_logical_op(gs_state *pgs, gs_logical_operation_t lop)
{	pgs->log_op = lop;
}
gs_logical_operation_t
gs_current_logical_op(const gs_state *pgs)
{	return pgs->log_op;
}
