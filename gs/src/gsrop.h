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

/* gsrop.h */
/* RasterOp / transparency / render algorithm procedure interface */

#ifndef gsrop_INCLUDED
#  define gsrop_INCLUDED

#include "gsropt.h"

/* Procedural interface */

void	gs_setrasterop(P2(gs_state *, gs_rop3_t));
gs_rop3_t
	gs_currentrasterop(P1(const gs_state *));
void	gs_setsourcetransparent(P2(gs_state *, bool));
bool	gs_currentsourcetransparent(P1(const gs_state *));
void	gs_settexturetransparent(P2(gs_state *, bool));
bool	gs_currenttexturetransparent(P1(const gs_state *));
int	gs_setrenderalgorithm(P2(gs_state *, int));
int	gs_currentrenderalgorithm(P1(const gs_state *));
/* Save/restore the combined logical operation. */
gs_logical_operation_t
	gs_current_logical_op(P1(const gs_state *));
void	gs_set_logical_op(P2(gs_state *, gs_logical_operation_t));

#endif					/* gsrop_INCLUDED */
