/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Implementation of transparency, other than rendering */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gstrans.h"
#include "gsutil.h"
#include "gzstate.h"
#include "gxdevcli.h"
#include "gdevdevn.h"
#include "gxblend.h"
#include "gdevp14.h"
#include "gscspace.h"
#include "gxarith.h"
#include "gxclist.h"

#define PUSH_TS 0

/* ------ Transparency-related graphics state elements ------ */

int
gs_setblendmode(gs_state *pgs, gs_blend_mode_t mode)
{
#ifdef DEBUG
    if (gs_debug_c('v')) {
	static const char *const bm_names[] = { GS_BLEND_MODE_NAMES };

	dlprintf1("[v](0x%lx)blend_mode = ", (ulong)pgs);
	if (mode >= 0 && mode < countof(bm_names))
	    dprintf1("%s\n", bm_names[mode]);
	else
	    dprintf1("%d??\n", (int)mode);
    }
#endif
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
    if_debug2('v', "[v](0x%lx)opacity.alpha = %g\n", (ulong)pgs, alpha);
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
    if_debug2('v', "[v](0x%lx)shape.alpha = %g\n", (ulong)pgs, alpha);
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
    if_debug2('v', "[v](0x%lx)text_knockout = %s\n", (ulong)pgs,
	      (knockout ? "true" : "false"));
    pgs->text_knockout = knockout;
    return 0;
}

bool
gs_currenttextknockout(const gs_state *pgs)
{
    return pgs->text_knockout;
}

/* ------ Transparency rendering stack ------ */

gs_transparency_state_type_t
gs_current_transparency_type(const gs_state *pgs)
{
    return (pgs->transparency_stack == 0 ? 0 :
	    pgs->transparency_stack->type);
}

/* Support for dummy implementation */
gs_private_st_ptrs1(st_transparency_state, gs_transparency_state_t,
		    "gs_transparency_state_t", transparency_state_enum_ptrs,
		    transparency_state_reloc_ptrs, saved);
#if PUSH_TS
static int
push_transparency_stack(gs_state *pgs, gs_transparency_state_type_t type,
			client_name_t cname)
{
    gs_transparency_state_t *pts =
	gs_alloc_struct(pgs->memory, gs_transparency_state_t,
			&st_transparency_state, cname);

    if (pts == 0)
	return_error(gs_error_VMerror);
    pts->saved = pgs->transparency_stack;
    pts->type = type;
    pgs->transparency_stack = pts;
    return 0;
}
#endif
static void
pop_transparency_stack(gs_state *pgs, client_name_t cname)
{
    gs_transparency_state_t *pts = pgs->transparency_stack; /* known non-0 */
    gs_transparency_state_t *saved = pts->saved;

    gs_free_object(pgs->memory, pts, cname);
    pgs->transparency_stack = saved;

}

/* This is used to keep pdf14 compositor actions from the interpreter from
   corrupting pattern renderings.  For example, if the file has a softmask,
   the intrepter will send push and pop transparency state commands when
   q and Q operations are encountered.  If we are writing out to a pattern
   clist that has no trasparency we do not want these state changes to 
   be entered as compositor actions in the pattern clist */

static int
check_for_nontrans_pattern(gs_state *pgs, unsigned char *comp_name)
{
    gx_device * dev = pgs->device;
    bool is_patt_clist = (strcmp("pattern-clist",dev->dname) == 0);
    bool is_patt_acum = (strcmp("pattern accumulator",dev->dname) == 0);

    /* Check if we are collecting data for a pattern that has no
       transparency.  In that case, we need to ignore the state changes */
    if (is_patt_clist || is_patt_acum) {
        if (is_patt_clist) {
            gx_device_clist_writer *clwdev = (gx_device_clist_writer*) dev;
            const gs_pattern1_instance_t *pinst = clwdev->pinst;

            if (!(pinst->template.uses_transparency)) {
                if_debug1('v', 
                    "[v]%s NOT sending in pattern\n",comp_name);
                return(1);
            }
        }
        if (is_patt_acum) {
            gx_device_pattern_accum *padev = (gx_device_pattern_accum*) dev;
            const gs_pattern1_instance_t *pinst = padev->instance;

            if (!(pinst->template.uses_transparency)) {
                if_debug1('v', 
                    "[v]%s NOT sending in pattern\n",comp_name);
                return(1);
            }
        }
    }    
    return(0);
}
   
/*
 * Push a PDF 1.4 transparency compositor onto the current device. Note that
 * if the current device already is a PDF 1.4 transparency compositor, the
 * create_compositor will update its parameters but not create a new
 * compositor device.
 */
static int
gs_state_update_pdf14trans(gs_state * pgs, gs_pdf14trans_params_t * pparams)
{
    gs_imager_state * pis = (gs_imager_state *)pgs;
    gx_device * dev = pgs->device;
    gx_device *pdf14dev = NULL;
    int code;

    /*
     * Send the PDF 1.4 create compositor action specified by the parameters.
     */
    code = send_pdf14trans(pis, dev, &pdf14dev, pparams, pgs->memory);
    /*
     * If we created a new PDF 1.4 compositor device then we need to install it
     * into the graphics state.
     */
    if (code >= 0 && pdf14dev != dev)
        gx_set_device_only(pgs, pdf14dev);

    return code;
}

void
gs_trans_group_params_init(gs_transparency_group_params_t *ptgp)
{
    ptgp->ColorSpace = 0;	/* bogus, but can't do better */
    ptgp->Isolated = false;
    ptgp->Knockout = false;
    ptgp->image_with_SMask = false;
    ptgp->mask_id = 0;
}

int
gs_begin_transparency_group(gs_state *pgs,
			    const gs_transparency_group_params_t *ptgp,
			    const gs_rect *pbbox)
{
    gs_pdf14trans_params_t params = { 0 };
    const gs_color_space *blend_color_space;
    gs_imager_state * pis = (gs_imager_state *)pgs;

    if (check_for_nontrans_pattern(pgs, "gs_begin_transparency_group")) {
        return(0);
    }
    /*
     * Put parameters into a compositor parameter and then call the
     * create_compositor.  This will pass the data to the PDF 1.4
     * transparency device.
     */
    params.pdf14_op = PDF14_BEGIN_TRANS_GROUP;
    params.Isolated = ptgp->Isolated;
    params.Knockout = ptgp->Knockout;
    params.image_with_SMask = ptgp->image_with_SMask;
    params.opacity = pgs->opacity;
    params.shape = pgs->shape;
    params.blend_mode = pgs->blend_mode;

    /* The blending procs must be based upon the current color space */
    /* Note:  This function is called during the c-list writer side. 
       Store some information so that we know what the color space is
       so that we can adjust according later during the clist reader */ 

    /* Note that we currently will use the concrete space for any space other than a 
        device space.  However, if the device is a sep device it will blend
        in DeviceN color space as required.  */

    blend_color_space = gs_currentcolorspace_inline(pgs);
    if (gs_color_space_get_index(blend_color_space) > gs_color_space_index_DeviceCMYK) {

       /* ICC and CIE based color space.  Problem right now is that the 
       current code does a concretization to the color space
       defined by the CRD.  This is not the space that we want
       to blend in.  Instead we want all colors to be mapped TO
       the ICC color space.  Then when the group is popped they
       should be converted to the parent space. 
       That I will need to fix another day with the color changes.  
       For now we will punt and set our blending space as the 
       concrete space for the ICC space, which is defined by
       the output (or default) CRD. */

        blend_color_space = cs_concrete_space(blend_color_space, pis);

    }

    /* Note that if the /CS parameter was not present in the push 
       of the transparency group, then we must actually inherent 
       the previous group color space, or the color space of the
       target device (process color model).  Note here we just want
       to set it as a unknown type for clist writing, as we .  We will later 
       during clist reading 
       */

    if (ptgp->ColorSpace == NULL) {

        params.group_color = UNKNOWN;
        params.group_color_numcomps = 0;
    
    } else {

        switch (cs_num_components(blend_color_space)) {
            case 1:				
                params.group_color = GRAY_SCALE;       
                params.group_color_numcomps = 1;  /* Need to check */
                break;
            case 3:				
                params.group_color = DEVICE_RGB;       
                params.group_color_numcomps = 3; 
                break;
            case 4:				
                params.group_color = DEVICE_CMYK;       
                params.group_color_numcomps = 4; 
            break;
            default:
                
                /* We can end up here if we are in
                   a deviceN color space and 
                   we have a sep output device */

                params.group_color = DEVICEN;
                params.group_color_numcomps = cs_num_components(blend_color_space);

            break;

         }  

    }

#ifdef DEBUG
    if (gs_debug_c('v')) {
	static const char *const cs_names[] = {
	    GS_COLOR_SPACE_TYPE_NAMES
	};

	dlprintf6("[v](0x%lx)begin_transparency_group [%g %g %g %g] Num_grp_clr_comp = %d\n",
		  (ulong)pgs, pbbox->p.x, pbbox->p.y, pbbox->q.x, pbbox->q.y,params.group_color_numcomps);
	if (ptgp->ColorSpace)
	    dprintf1("     CS = %s",
		cs_names[(int)gs_color_space_get_index(ptgp->ColorSpace)]);
	else
	    dputs("     (no CS)");
	dprintf2("  Isolated = %d  Knockout = %d\n",
		 ptgp->Isolated, ptgp->Knockout);
    }
#endif

    params.bbox = *pbbox;
    return gs_state_update_pdf14trans(pgs, &params);
}

int
gx_begin_transparency_group(gs_imager_state * pis, gx_device * pdev,
				const gs_pdf14trans_params_t * pparams)
{
    gs_transparency_group_params_t tgp = {0};
    gs_rect bbox;

    if (pparams->Background_components != 0 && 
	pparams->Background_components != pdev->color_info.num_components)
	return_error(gs_error_rangecheck);
    tgp.Isolated = pparams->Isolated;
    tgp.Knockout = pparams->Knockout;
    tgp.idle = pparams->idle;
    tgp.mask_id = pparams->mask_id;

    /* Needed so that we do proper blending */
    tgp.group_color = pparams->group_color;
    tgp.group_color_numcomps = pparams->group_color_numcomps;

    pis->opacity.alpha = pparams->opacity.alpha;
    pis->shape.alpha = pparams->shape.alpha;
    pis->blend_mode = pparams->blend_mode;
    bbox = pparams->bbox;
#ifdef DEBUG
    if (gs_debug_c('v')) {
	static const char *const cs_names[] = {
	    GS_COLOR_SPACE_TYPE_NAMES
	};

	dlprintf6("[v](0x%lx)gx_begin_transparency_group [%g %g %g %g] Num_grp_clr_comp = %d\n",
		  (ulong)pis, bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y,pparams->group_color_numcomps);
	if (tgp.ColorSpace)
	    dprintf1("     CS = %s",
		cs_names[(int)gs_color_space_get_index(tgp.ColorSpace)]);
	else
	    dputs("     (no CS)");
	dprintf2("  Isolated = %d  Knockout = %d\n",
		 tgp.Isolated, tgp.Knockout);
    }
#endif
    if (dev_proc(pdev, begin_transparency_group) != 0)
	return (*dev_proc(pdev, begin_transparency_group)) (pdev, &tgp,
							&bbox, pis, NULL, NULL);
    else
	return 0;
}

int
gs_end_transparency_group(gs_state *pgs)
{
    gs_pdf14trans_params_t params = { 0 };

    if (check_for_nontrans_pattern(pgs, "gs_end_transparency_group")) {
        return(0);
    }
    if_debug0('v', "[v]gs_end_transparency_group\n");
    params.pdf14_op = PDF14_END_TRANS_GROUP;  /* Other parameters not used */
    return gs_state_update_pdf14trans(pgs, &params);
}

int
gx_end_transparency_group(gs_imager_state * pis, gx_device * pdev)
{
    if_debug0('v', "[v]gx_end_transparency_group\n");
    if (dev_proc(pdev, end_transparency_group) != 0)
	return (*dev_proc(pdev, end_transparency_group)) (pdev, pis, NULL);
    else
	return 0;
}


/* Commands for handling q softmask Q in graphic states */

int
gs_push_transparency_state(gs_state *pgs)
{
    gs_pdf14trans_params_t params = { 0 };
    gs_imager_state * pis = (gs_imager_state *)pgs;
    int code;

    if (check_for_nontrans_pattern(pgs, "gs_push_transparency_state")) {
        return(0);
    }
    /* Set the pending flag to true, which indicates
       that we need to watch for end transparency 
       soft masks when we are at this graphic state
       level */

    /* pis->trans_flags.xstate_pending = true; */

    /* Actually I believe the above flag is not 
       needed.  We really should be watching for
       the softmask even at the base level.  What
       we need to watch for are q operations after
       a soft mask end has occured. */

    /* Check if we have a change flag set to true.
       this indicates that a softmask is present.
       We will need to send a push state to save
       the current soft mask, so that we can
       restore it later */

    if (pis->trans_flags.xstate_change) {

        if_debug0('v', "[v]gs_push_transparency_state sending\n");
        params.pdf14_op = PDF14_PUSH_TRANS_STATE;  
        code = gs_state_update_pdf14trans(pgs, &params);
        if (code < 0) 
            return(code);

    } else {

        if_debug0('v', "[v]gs_push_transparency_state NOT sending\n");

    }


    return(0);
}


int
gs_pop_transparency_state(gs_state *pgs)
{
    gs_pdf14trans_params_t params = { 0 };
    gs_imager_state * pis = (gs_imager_state *)pgs;
    int code;

    if (check_for_nontrans_pattern(pgs, "gs_pop_transparency_state")) {
        return(0);
    }
    /* Check if flag is set, which indicates that we have 
       an active softmask for the graphic state.  We
       need to communicate to the compositor to pop
       the softmask */
    if ( pis->trans_flags.xstate_change ) {
    
        if_debug0('v', "[v]gs_pop_transparency_state sending\n");
        params.pdf14_op = PDF14_POP_TRANS_STATE;  
        code = gs_state_update_pdf14trans(pgs, &params);
        if ( code < 0 )
            return (code);

    } else {

        if_debug0('v', "[v]gs_pop_transparency_state NOT sending\n");

    }

    /* There is no reason to reset any of the flags since
       they will be reset by the graphic state restore */

    return(0);

}


int
gx_pop_transparency_state(gs_imager_state * pis, gx_device * pdev)
{
    if_debug0('v', "[v]gx_pop_transparency_state\n");
    if (dev_proc(pdev, pop_transparency_state) != 0)
	return (*dev_proc(pdev, pop_transparency_state)) (pdev, pis);
    else
	return 0;
}

int
gx_push_transparency_state(gs_imager_state * pis, gx_device * pdev)
{
    if_debug0('v', "[v]gx_push_transparency_state\n");
    if (dev_proc(pdev, push_transparency_state) != 0)
	return (*dev_proc(pdev, push_transparency_state)) (pdev, pis);
    else
	return 0;
}



/*
 * Handler for identity mask transfer functions.
 */
static int
mask_transfer_identity(floatp in, float *out, void *proc_data)
{
    *out = (float) in;
    return 0;
}

void
gs_trans_mask_params_init(gs_transparency_mask_params_t *ptmp,
			  gs_transparency_mask_subtype_t subtype)
{
    ptmp->ColorSpace = 0;
    ptmp->subtype = subtype;
    ptmp->Background_components = 0;
    ptmp->TransferFunction = mask_transfer_identity;
    ptmp->TransferFunction_data = 0;
    ptmp->replacing = false;
}

int
gs_begin_transparency_mask(gs_state * pgs,
			   const gs_transparency_mask_params_t * ptmp,
			   const gs_rect * pbbox, bool mask_is_image)
{
    gs_pdf14trans_params_t params = { 0 };
    const int l = sizeof(params.Background[0]) * ptmp->Background_components;
    int i;
    const gs_color_space *blend_color_space;
    gs_imager_state * pis = (gs_imager_state *)pgs;
    int num_components;

    if (check_for_nontrans_pattern(pgs, "gs_pop_transparency_state")) {
        return(0);
    }
    params.pdf14_op = PDF14_BEGIN_TRANS_MASK;
    params.bbox = *pbbox;
    params.subtype = ptmp->subtype;
   /* params.SMask_is_CIE = gs_color_space_is_CIE(pgs->color_space); */  /* See comments in gs_begin_transparency_mask */
    params.SMask_is_CIE = false; 
    params.Background_components = ptmp->Background_components;
    memcpy(params.Background, ptmp->Background, l);
    params.GrayBackground = ptmp->GrayBackground;
    params.transfer_function = ptmp->TransferFunction_data;
    params.function_is_identity =
	    (ptmp->TransferFunction == mask_transfer_identity);
    params.mask_is_image = mask_is_image;
    params.replacing = ptmp->replacing;
    /* Note that the SMask buffer may have a different 
       numcomps than the device buffer */
    params.group_color_numcomps = cs_num_components(gs_currentcolorspace_inline(pgs));

    if_debug9('v', "[v](0x%lx)gs_begin_transparency_mask [%g %g %g %g]\n\
      subtype = %d  Background_components = %d Num_grp_clr_comp = %d  %s\n",
	      (ulong)pgs, pbbox->p.x, pbbox->p.y, pbbox->q.x, pbbox->q.y,
	      (int)ptmp->subtype, ptmp->Background_components,
              params.group_color_numcomps,
	      (ptmp->TransferFunction == mask_transfer_identity ? "no TR" :
	       "has TR"));

    /* Sample the transfer function */
    for (i = 0; i < MASK_TRANSFER_FUNCTION_SIZE; i++) {
	float in = (float)(i * (1.0 / (MASK_TRANSFER_FUNCTION_SIZE - 1)));
	float out;

	ptmp->TransferFunction(in, &out, ptmp->TransferFunction_data);
	params.transfer_fn[i] = (byte)floor((double)(out * 255 + 0.5));
    }

    /* If we have a CIE space & a luminosity subtype
       we will need to do our concretization
       to CIEXYZ so that we can obtain the proper 
       luminance value.  This is what SHOULD happen
       according to the spec.  However AR does not 
       follow this.  It always seems to do the soft mask
       creation in the device space.  For this reason
       we will do that too. SMask_is_CIE is always false for now */

    /* The blending procs are currently based upon the device type.
       We need to have them based upon the current color space */

    /* Note:  This function is called during the c-list writer side. */ 


    if ( params.SMask_is_CIE && params.subtype == TRANSPARENCY_MASK_Luminosity ){

        /* Install Color Space to go to CIEXYZ */
        
        /* int ok;
        ok = gx_cie_to_xyz_alloc2(pgs->color_space,pgs); */  /* quite compiler */
        params.group_color_numcomps = 3;  /* CIEXYZ */

        /* Mark the proper spaces so that we make
         * the appropriate changes in the device */

        params.group_color = CIE_XYZ;

    } else {

    /* Set the group color type, which may be 
     *  different than the device type.  Note
        we want to check the concrete space due
        to the fact that things are done
        in device space always. */

        blend_color_space = gs_currentcolorspace_inline(pgs);
        if(gs_color_space_is_CIE(blend_color_space)){

           /* ICC or CIE based color space.  Problem right now is that the 
           current code does a concretization to the color space
           defined by the CRD.  This is not the space that we want
           to blend in.  Instead we want all colors to be mapped TO
           the ICC color space.  Then when the group is popped they
           should be converted to the parent space. 
           That I will need to fix another day with the color changes.  
           For now we will punt and set our blending space as the 
           concrete space for the ICC space, which is defined by
           the output (or default) CRD. */

            blend_color_space = cs_concrete_space(blend_color_space, pis);

        }

        /* For the softmask blend color space, we will always use the above blend_color_space. 
           Problems can occur if we go all the way back to the device color space,
           which could be DeviceN for a sep device.  Blending to the luminosity
           channel for this case would not be defined. */

        num_components = cs_num_components(blend_color_space);
        switch (any_abs(num_components)) {

            case 1:				
                params.group_color = GRAY_SCALE;       
                params.group_color_numcomps = 1;  /* Need to check */
                break;
            case 3:				
                params.group_color = DEVICE_RGB;       
                params.group_color_numcomps = 3; 
                break;
            case 4:				
                params.group_color = DEVICE_CMYK;       
                params.group_color_numcomps = 4; 
            break;
            default:
                /* Transparency soft mask spot
                   colors are NEVER available. 
                   We must use the alternate tint
                   transform */
            return_error(gs_error_rangecheck);
            break;

         }    

    }

    return gs_state_update_pdf14trans(pgs, &params);
}

/* This occurs on the c-list reader side */

int
gx_begin_transparency_mask(gs_imager_state * pis, gx_device * pdev,
				const gs_pdf14trans_params_t * pparams)
{
    gx_transparency_mask_params_t tmp;
    const int l = sizeof(pparams->Background[0]) * pparams->Background_components;

    tmp.group_color = pparams->group_color;
    tmp.subtype = pparams->subtype;
    tmp.SMask_is_CIE = pparams->SMask_is_CIE;
    tmp.group_color_numcomps = pparams->group_color_numcomps;
    tmp.Background_components = pparams->Background_components;
    memcpy(tmp.Background, pparams->Background, l);
    tmp.GrayBackground = pparams->GrayBackground;
    tmp.function_is_identity = pparams->function_is_identity;
    tmp.idle = pparams->idle;
    tmp.replacing = pparams->replacing;
    tmp.mask_id = pparams->mask_id;
    memcpy(tmp.transfer_fn, pparams->transfer_fn, size_of(tmp.transfer_fn));
    if_debug9('v', "[v](0x%lx)gx_begin_transparency_mask [%g %g %g %g]\n\
      subtype = %d  Background_components = %d  Num_grp_clr_comp = %d %s\n",
	      (ulong)pis, pparams->bbox.p.x, pparams->bbox.p.y,
	      pparams->bbox.q.x, pparams->bbox.q.y,
	      (int)tmp.subtype, tmp.Background_components,
              tmp.group_color_numcomps,
	      (tmp.function_is_identity ? "no TR" :
	       "has TR"));
    if (dev_proc(pdev, begin_transparency_mask) != 0)
	return (*dev_proc(pdev, begin_transparency_mask))
	    		(pdev, &tmp, &(pparams->bbox), pis, NULL, NULL);
    else
	return 0;
}

int
gs_end_transparency_mask(gs_state *pgs,
			 gs_transparency_channel_selector_t csel)
{
    gs_pdf14trans_params_t params = { 0 };
    gs_imager_state * pis = (gs_imager_state *)pgs;

    if (check_for_nontrans_pattern(pgs, "gs_end_transparency_mask")) {
        return(0);
    }
    /* If we have done a q then set a flag to watch for any Qs */

   /* if (pis->trans_flags.xstate_pending)
        pis->trans_flags.xstate_change = true; */

    /* This should not depend upon if we have encountered a q
       operation.  We could be setting a softmask, before 
       there is any q operation.  Unlikely but it could happen.
       Then if we encouter a q operation (and this flag
       is true) we will need to 
       push the mask graphic state (PDF14_PUSH_TRANS_STATE). */

    pis->trans_flags.xstate_change = true;

    if_debug1('v', "[v]xstate_changed set true, gstate level is %d\n", pgs->level);

    if_debug2('v', "[v](0x%lx)gs_end_transparency_mask(%d)\n", (ulong)pgs,
	      (int)csel);

    params.pdf14_op = PDF14_END_TRANS_MASK;  /* Other parameters not used */
    params.csel = csel;
    return gs_state_update_pdf14trans(pgs, &params);
}

int
gx_end_transparency_mask(gs_imager_state * pis, gx_device * pdev,
				const gs_pdf14trans_params_t * pparams)
{
    if_debug2('v', "[v](0x%lx)gx_end_transparency_mask(%d)\n", (ulong)pis,
	      (int)pparams->csel);

    if (dev_proc(pdev, end_transparency_mask) != 0)
	return (*dev_proc(pdev, end_transparency_mask)) (pdev, pis, NULL);
    else
	return 0;
}

int
gs_discard_transparency_layer(gs_state *pgs)
{
    /****** NYI, DUMMY ******/
    gs_transparency_state_t *pts = pgs->transparency_stack;

    if_debug1('v', "[v](0x%lx)gs_discard_transparency_layer\n", (ulong)pgs);
    if (!pts)
	return_error(gs_error_rangecheck);
    pop_transparency_stack(pgs, "gs_discard_transparency_layer");
    return 0;
}

/*
 * We really only care about the number of spot colors when we have
 * a device which supports spot colors.  With the other devices we use
 * the tint transform function for DeviceN and Separation color spaces
 * and convert spot colors into process colors.
 */
static int
get_num_pdf14_spot_colors(gs_state * pgs)
{
    gx_device * dev = pgs->device;
    gs_devn_params * pclist_devn_params = dev_proc(dev, ret_devn_params)(dev);

    /*
     * Devices which support spot colors store the PageSpotColors device
     * parameter inside their devn_params structure.  (This is done by the
     * devn_put_params routine.)  The PageSpotColors device parameter is
     * set by pdf_main whenever a PDF page is being processed.  See
     * countspotcolors in lib/pdf_main.ps.
     */
    if (pclist_devn_params != NULL) {

        /* If the sep order names were specified, then we should only allocate
           for those.  But only the nonstandard colorants that are stored
           in num_separations.  See devn_put_params for details on this. 
           Right now, the PDF14 device will always include CMYK.  A future
           optimization is to be able to NOT have those included in the buffer 
           allocations if we don't specify them.  It would then be possible to
           output 8 separations at a time without using compressed color. */

        if (pclist_devn_params->num_separation_order_names == 0)
	    return pclist_devn_params->page_spot_colors;

        return (pclist_devn_params->separations.num_separations);

    }
    return 0;
}

int
gs_push_pdf14trans_device(gs_state * pgs)
{
    gs_pdf14trans_params_t params = { 0 };

    params.pdf14_op = PDF14_PUSH_DEVICE;
    /*
     * We really only care about the number of spot colors when we have
     * a device which supports spot colors.  With the other devices we use
     * the tint transform function for DeviceN and Separation color spaces
     * and convert spot colors into process colors.
     */
    params.num_spot_colors = get_num_pdf14_spot_colors(pgs);
    /* Note: Other parameters not used */
    return gs_state_update_pdf14trans(pgs, &params);
}

int
gs_pop_pdf14trans_device(gs_state * pgs)
{
    gs_pdf14trans_params_t params = { 0 };

    params.pdf14_op = PDF14_POP_DEVICE;  /* Other parameters not used */
    return gs_state_update_pdf14trans(pgs, &params);
}
