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
/* Dummy implementation of transparency. */
#include "gx.h"
#include "gserrors.h"
#include "gstrans.h"
#include "gzstate.h"

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
gs_setopacitymask(gs_state *pgs, const gs_soft_mask_t *psmask)
{
    /****** NYI ******/
    return 0;
}

gs_soft_mask_t *
gs_currentopacitymask(const gs_state *pgs)
{
    /****** NYI ******/
    return 0;
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
gs_setshapemask(gs_state *pgs, const gs_soft_mask_t *psmask)
{
    /****** NYI ******/
    return 0;
}

gs_soft_mask_t *
gs_currentshapemask(const gs_state *pgs)
{
    /****** NYI ******/
    return 0;
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
