/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

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

/*$Id$ */
/* Transparency definitions and interface */

#ifndef gstrans_INCLUDED
#  define gstrans_INCLUDED

#include "gstparam.h"

/* Access transparency-related graphics state elements. */
/* Note that setting or reading a mask copies the structure. */
int gs_setblendmode(P2(gs_state *, gs_blend_mode_t));
gs_blend_mode_t gs_currentblendmode(P1(const gs_state *));
int gs_setopacityalpha(P2(gs_state *, floatp));
float gs_currentopacityalpha(P1(const gs_state *));
int gs_setopacitymask(P2(gs_state *, const gs_soft_mask_t *));
const gs_soft_mask_t * gs_currentopacitymask(P1(const gs_state *));
int gs_setshapealpha(P2(gs_state *, floatp));
float gs_currentshapealpha(P1(const gs_state *));
int gs_setshapemask(P2(gs_state *, const gs_soft_mask_t *));
const gs_soft_mask_t * gs_currentshapemask(P1(const gs_state *));
int gs_settextknockout(P2(gs_state *, bool));
bool gs_currenttextknockout(P1(const gs_state *));

/* Allocate/initialize a soft mask. */
int gs_soft_mask_alloc(P3(gs_memory_t *, gs_soft_mask_t **, client_name_t));
void gs_soft_mask_init(P1(gs_soft_mask_t *));

/* Begin / end / discard a transparency group. */
int gs_begintransparencygroup(P4(gs_state *pgs, bool isolated, bool knockout,
				 const gs_rect *pbbox));
int gs_discardtransparencygroup(P1(gs_state *));
int gs_endtransparencygroup(P1(gs_state *));

#endif /* gstrans_INCLUDED */
