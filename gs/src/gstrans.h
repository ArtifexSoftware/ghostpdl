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
/* Transparency definitions and interface */

#ifndef gstrans_INCLUDED
#  define gstrans_INCLUDED

#include "gstparam.h"

/* Access transparency-related graphics state elements. */
int gs_setblendmode(gs_state *, gs_blend_mode_t);
gs_blend_mode_t gs_currentblendmode(const gs_state *);
int gs_setopacityalpha(gs_state *, floatp);
float gs_currentopacityalpha(const gs_state *);
int gs_setshapealpha(gs_state *, floatp);
float gs_currentshapealpha(const gs_state *);
int gs_settextknockout(gs_state *, bool);
bool gs_currenttextknockout(const gs_state *);

/*
 * Manage transparency group and mask rendering.  Eventually these will be
 * driver procedures, taking dev + pis instead of pgs.
 */

gs_transparency_state_type_t
    gs_current_transparency_type(const gs_state *pgs);

/*
 * We have to abbreviate the procedure name because procedure names are
 * only unique to 23 characters on VMS.
 */
void gs_trans_group_params_init(gs_transparency_group_params_t *ptgp);

int gs_begin_transparency_group(gs_state *pgs,
				const gs_transparency_group_params_t *ptgp,
				const gs_rect *pbbox);

int gs_end_transparency_group(gs_state *pgs);

void gs_trans_mask_params_init(gs_transparency_mask_params_t *ptmp,
			       gs_transparency_mask_subtype_t subtype);

int gs_begin_transparency_mask(gs_state *pgs,
			       const gs_transparency_mask_params_t *ptmp,
			       const gs_rect *pbbox);

int gs_end_transparency_mask(gs_state *pgs,
			     gs_transparency_channel_selector_t csel);

int gs_init_transparency_mask(gs_state *pgs,
			      gs_transparency_channel_selector_t csel);

int gs_discard_transparency_layer(gs_state *pgs);

#endif /* gstrans_INCLUDED */
