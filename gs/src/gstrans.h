/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/*$RCSfile$ $Revision$ */
/* Transparency definitions and interface */

#ifndef gstrans_INCLUDED
#  define gstrans_INCLUDED

#include "gstparam.h"

/* Access transparency-related graphics state elements. */
int gs_setblendmode(P2(gs_state *, gs_blend_mode_t));
gs_blend_mode_t gs_currentblendmode(P1(const gs_state *));
int gs_setopacityalpha(P2(gs_state *, floatp));
float gs_currentopacityalpha(P1(const gs_state *));
int gs_setshapealpha(P2(gs_state *, floatp));
float gs_currentshapealpha(P1(const gs_state *));
int gs_settextknockout(P2(gs_state *, bool));
bool gs_currenttextknockout(P1(const gs_state *));

/*
 * Manage transparency group and mask rendering.  Eventually these will be
 * driver procedures, taking dev + pis instead of pgs.
 */

gs_transparency_state_type_t
    gs_current_transparency_type(P1(const gs_state *pgs));

/*
 * We have to abbreviate the procedure name because procedure names are
 * only unique to 23 characters on VMS.
 */
void gs_trans_group_params_init(P1(gs_transparency_group_params_t *ptgp));

int gs_begin_transparency_group(P3(gs_state *pgs,
				   const gs_transparency_group_params_t *ptgp,
				   const gs_rect *pbbox));

int gs_end_transparency_group(P1(gs_state *pgs));

void gs_trans_mask_params_init(P2(gs_transparency_mask_params_t *ptmp,
				  gs_transparency_mask_subtype_t subtype));

int gs_begin_transparency_mask(P3(gs_state *pgs,
				  const gs_transparency_mask_params_t *ptmp,
				  const gs_rect *pbbox));

int gs_end_transparency_mask(P2(gs_state *pgs,
				gs_transparency_channel_selector_t csel));

int gs_init_transparency_mask(P2(gs_state *pgs,
				 gs_transparency_channel_selector_t csel));

int gs_discard_transparency_layer(P1(gs_state *pgs));

#endif /* gstrans_INCLUDED */
