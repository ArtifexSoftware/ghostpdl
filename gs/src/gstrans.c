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

/* ------ Transparency rendering stack ------ */

/*

This area of the transparency facilities is in flux.  Here is a proposal for
extending the driver interface.

int (*begin_transparency_group)(gx_device *dev,
    const gs_transparency_group_params_t *ptgp,
    const gs_rect *pbbox,
    gs_imager_state *pis,
    gs_transparency_state_t **ppts,
    gs_memory_t *mem)

Pushes the current transparency state (*ppts) onto the associated stack, and
sets *ppts to a new transparency state of the given dimension.  The
transparency state may copy some or all of the imager state, such as the
current alpha and/or transparency mask values, and definitely copies the
parameters.

int (*end_transparency_group)(gx_device *dev,
    gs_imager_state *pis,
    gs_transparency_state_t **ppts)

Blends the top element of the transparency stack, which must be a group,
into the next-to-top element, popping the stack.  If the stack only had a
single element, blends into the device output.  Sets *ppts to 0 iff the
stack is now empty.  If end_group fails, the stack is *not* popped.

int (*begin_transparency_mask)(gx_device *dev,
    const gs_transparency_mask_params_t *ptmp,
    const gs_rect *pbbox,
    gs_imager_state *pis,
    gs_transparency_state_t **ppts,
    gs_memory_t *mem)

See begin_transparency_group.

int (*end_transparency_mask)(gx_device *dev,
    gs_transparency_mask_t **pptm)

Stores a pointer to the rendered transparency mask into *pptm, popping the
stack like end_group.  Normally, the client will follow this by using
rc_assign to store the rendered mask into pis->{opacity,shape}.mask.  If
end_mask fails, the stack is *not* popped.

int (*discard_transparency_level)(gx_device *dev,
    gs_transparency_state_t **ppts)

Pops the transparency stack, discarding the top element, which may be either
a group or a mask.  Sets *ppts to 0 iff the stack is now empty.

 */

gs_transparency_state_type_t
gs_current_transparency_type(const gs_state *pgs)
{
    return (pgs->transparency_stack == 0 ? 0 :
	    pgs->transparency_stack->type);
}

void
gs_trans_group_params_init(gs_transparency_group_params_t *ptgp)
{
    ptgp->ColorSpace = 0;	/* bogus, but can't do better */
    ptgp->Isolated = false;
    ptgp->Knockout = false;
}

int
gs_begin_transparency_group(gs_state *pgs,
			    const gs_transparency_group_params_t *ptgp,
			    const gs_rect *pbbox)
{
    /****** NYI ******/
    return 0;
}

int
gs_end_transparency_group(gs_state *pgs)
{
    /****** NYI ******/
    return 0;
}

private float
transfer_identity(floatp x)
{
    return x;
}
void
gs_trans_mask_params_init(gs_transparency_mask_params_t *ptmp,
			  gs_transparency_mask_subtype_t subtype)
{
    ptmp->subtype = subtype;
    ptmp->has_Background = false;
    ptmp->TransferFunction = transfer_identity;
}

int
gs_begin_transparency_mask(gs_state *pgs,
			   const gs_transparency_mask_params_t *ptmp,
			   const gs_rect *pbbox)
{
    /****** NYI ******/
    return 0;
}

int
gs_end_transparency_mask(gs_state *pgs,
			 gs_transparency_channel_selector_t csel)
{
    /****** NYI ******/
    return 0;
}

int
gs_discard_transparency_level(gs_state *pgs)
{
    /****** NYI ******/
    return 0;
}

int
gs_init_transparency_mask(gs_state *pgs,
			  gs_transparency_channel_selector_t csel)
{
    /****** NYI ******/
    return 0;
}
