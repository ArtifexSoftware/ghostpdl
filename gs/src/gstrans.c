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
/* Implementation of transparency, other than rendering */
#include "gx.h"
#include "gserrors.h"
#include "gstrans.h"
#include "gsutil.h"
#include "gzstate.h"

private_st_gs_soft_mask();

/* ------ Internal procedures ------ */

private int
gs_set_transparency_mask(gs_state *pgs, gs_soft_mask_t **ppsm,
			 const gs_soft_mask_t *psm_new)
{
    rc_header rc;
    gs_soft_mask_t *psm = *ppsm;

    if (psm_new == 0) {
	rc_decrement_only(psm, "gs_set_transparency_mask");
	*ppsm = 0;
    } else {
	rc_unshare_struct(psm, gs_soft_mask_t, &st_gs_soft_mask, pgs->memory,
			  return_error(gs_error_VMerror),
			  "gs_set_transparency_mask");
	rc = psm->rc;
	*psm = *psm_new;
	psm->rc = rc;
	if (psm->id == gs_no_id)
	    psm->id = gs_next_ids(1);
	psm->save_ctm = ctm_only(pgs);
	*ppsm = psm;
    }
    return 0;
}

/* ------ Transparency-related graphics state elements ------ */

int
gs_setblendmode(gs_state *pgs, gs_blend_mode_t mode)
{
    if (mode < 0 || mode > MAX_BLEND_MODE)
	return_error(gs_error_rangecheck);
    pgs->blend_mode = mode;
    return 0;
}

gs_blend_mode_t
gs_currentblendmode(const gs_state *pgs)
{
    return pgs->blend_mode;
}

int
gs_setopacityalpha(gs_state *pgs, floatp alpha)
{
    pgs->opacity.alpha = (alpha < 0.0 ? 0.0 : alpha > 1.0 ? 1.0 : alpha);
    return 0;
}

float
gs_currentopacityalpha(const gs_state *pgs)
{
    return pgs->opacity.alpha;
}

int
gs_setopacitymask(gs_state *pgs, const gs_soft_mask_t *psm)
{
    return gs_set_transparency_mask(pgs, &pgs->opacity.mask, psm);
}

const gs_soft_mask_t *
gs_currentopacitymask(const gs_state *pgs)
{
    return pgs->opacity.mask;
}

int
gs_setshapealpha(gs_state *pgs, floatp alpha)
{
    pgs->shape.alpha = (alpha < 0.0 ? 0.0 : alpha > 1.0 ? 1.0 : alpha);
    return 0;
}

float
gs_currentshapealpha(const gs_state *pgs)
{
    return pgs->shape.alpha;
}

int
gs_setshapemask(gs_state *pgs, const gs_soft_mask_t *psm)
{
    return gs_set_transparency_mask(pgs, &pgs->shape.mask, psm);
}

const gs_soft_mask_t *
gs_currentshapemask(const gs_state *pgs)
{
    return pgs->shape.mask;
}

int
gs_settextknockout(gs_state *pgs, bool knockout)
{
    pgs->text_knockout = knockout;
    return 0;
}

bool
gs_currenttextknockout(const gs_state *pgs)
{
    return pgs->text_knockout;
}

/* ------ Soft masks ------ */

int
gs_soft_mask_alloc(gs_memory_t *mem, gs_soft_mask_t **ppsm,
		   client_name_t cname)
{
    gs_soft_mask_t *psm;

    rc_alloc_struct_0(psm, gs_soft_mask_t, &st_gs_soft_mask, mem,
		      return_error(gs_error_VMerror), cname);
    gs_soft_mask_init(psm);
    *ppsm = psm;
    return 0;
}

void
gs_soft_mask_init(gs_soft_mask_t *psm)
{
    psm->id = gs_no_id;
    /* Initialize as a mask (to set ColorSpace = NULL). */
    gs_image_t_init_mask_adjust(&psm->image, true, false);
    psm->image.ImageMask = false; /* not actually a mask */
    psm->DataSource = 0;	/* for GC */
}

/* ------ Transparency group ------ */

int
gs_begintransparencygroup(gs_state *pgs, bool isolated, bool knockout,
			  const gs_rect *pbbox)
{
    /****** NYI ******/
    return 0;
}

int
gs_discardtransparencygroup(gs_state *pgs)
{
    /****** NYI ******/
    return 0;
}

int
gs_endtransparencygroup(gs_state *pgs)
{
    /****** NYI ******/
    return 0;
}
