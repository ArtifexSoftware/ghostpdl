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
/* Implementation of the ICCBased color space family */

#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "stream.h"
#include "gxcspace.h"		/* for gxcie.c */
#include "gxarith.h"
#include "gxcie.h"
#include "gzstate.h"
#include "gsicc.h"
#include "gsicccache.h"
#include "gsicc_littlecms.h"

#define SAVEICCPROFILE 0

/*
 * Discard a gs_cie_icc_s structure. This requires that we call the
 * destructor for ICC profile, lookup, and file objects (which are
 * stored in "foreign" memory).
 *
 * No special action is taken with respect to the stream pointer; that is
 * the responsibility of the client.  */
static void
cie_icc_finalize(void * pvicc_info)
{

}


/*
 * Color space methods for ICCBased color spaces.
 *
 * As documented, ICCBased color spaces may be used as both base and
 * alternative color spaces. Futhermore, they can themselves contain paint
 * color spaces as alternative color space. In this implementation we allow
 * them to be used as base and alternative color spaces, but only to contain
 * "small" base color spaces (CIEBased or smaller). This arrangement avoids
 * breaking the color space heirarchy. Providing a more correct arrangement
 * requires a major change in the color space mechanism.
 *
 * Several of the methods used by ICCBased color space apply as well to
 * DeviceN color spaces, in that they are generic to color spaces having
 * a variable number of components. We have elected not to attempt to
 * extract and combine these operations, because this would save only a
 * small amount of code, and much more could be saved by introducing certain
 * common elements (ranges, number of components, etc.) into the color space
 * root class.
 */
static cs_proc_num_components(gx_num_components_ICC);
static cs_proc_init_color(gx_init_ICC);
static cs_proc_restrict_color(gx_restrict_ICC);
static cs_proc_concrete_space(gx_concrete_space_ICC);
static cs_proc_concretize_color(gx_concretize_ICC);
static cs_proc_remap_color(gx_remap_ICC);
static cs_proc_install_cspace(gx_install_ICC);

#if ENABLE_CUSTOM_COLOR_CALLBACK
static cs_proc_remap_color(gx_remap_ICCBased);
#endif
static cs_proc_final(gx_final_ICC);
static cs_proc_serialize(gx_serialize_ICC);

static const gs_color_space_type gs_color_space_type_ICC = {
    gs_color_space_index_ICC,    /* index */
    true,                           /* can_be_base_space */
    true,                           /* can_be_alt_space */
    &st_base_color_space,         /* stype - structure descriptor */
    gx_num_components_ICC,       /* num_components */
    gx_init_ICC,                 /* init_color */
    gx_restrict_ICC,             /* restrict_color */
    gx_concrete_space_ICC,       /* concrete_space */
    gx_concretize_ICC,           /* concreteize_color */
    NULL,                           /* remap_concrete_color */
#if ENABLE_CUSTOM_COLOR_CALLBACK
    gx_remap_ICCBased,		    /* remap_color */
#else
    gx_remap_ICC,                   /* remap_color */
#endif
    gx_install_ICC,                 /* install_cpsace */
    gx_spot_colors_set_overprint,   /* set_overprint */
    gx_final_ICC,                /* final */
    gx_no_adjust_color_count,       /* adjust_color_count */
    gx_serialize_ICC,            /* serialize */
    gx_cspace_is_linear_default
};

/*
 * Return the number of components used by a ICCBased color space - 1, 3, or 4
 */
static int
gx_num_components_ICC(const gs_color_space * pcs)
{
    return pcs->cmm_icc_profile_data->num_comps;
}

/*
 * Set the initial client color for an ICCBased color space. The convention
 * suggested by the ICC specification is to set all components to 0.
 */
static void
gx_init_ICC(gs_client_color * pcc, const gs_color_space * pcs)
{
    int     i, ncomps = pcs->cmm_icc_profile_data->num_comps;

    for (i = 0; i < ncomps; ++i)
	pcc->paint.values[i] = 0.0;

    /* make sure that [ 0, ... 0] is in range */
    gx_restrict_ICC(pcc, pcs);
}

/*
 * Restrict an color to the range specified for an ICCBased color space.
 */
static void
gx_restrict_ICC(gs_client_color * pcc, const gs_color_space * pcs)
{
    int                 i, ncomps = pcs->cmm_icc_profile_data->num_comps;
    const gs_range *    ranges = pcs->cmm_icc_profile_data->Range.ranges;

    for (i = 0; i < ncomps; ++i) {
        floatp  v = pcc->paint.values[i];
        floatp  rmin = ranges[i].rmin, rmax = ranges[i].rmax;

        if (v < rmin)
            pcc->paint.values[i] = rmin;
        else if (v > rmax)
            pcc->paint.values[i] = rmax;
    }
}

/*
 * Return the concrete space to which this color space will map. If the
 * ICCBased color space is being used in native mode, the concrete space
 * will be dependent on the current color rendering dictionary, as it is
 * for all CIE bases. If the alternate color space is being used, then
 * this question is passed on to the appropriate method of that space.
 */
static const gs_color_space *
gx_concrete_space_ICC(const gs_color_space * pcs, const gs_imager_state * pis)
{
  /* MJV to FIX */

    const gs_color_space *  pacs = pcs->base_space;

    return cs_concrete_space(pacs, pis);
}


/*
 * To device space
 */
int
gx_remap_ICC(const gs_client_color * pcc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		       gs_color_select_t select)
{

    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k,i;
    gx_color_index color;

    /* Define the rendering intents.  MJV to fix */
    rendering_params.black_point_comp = BP_ON;
    rendering_params.object_type = GS_PATH_TAG;
    rendering_params.rendering_intent = gsSATURATION;

     /* This needs to be optimized. And range corrected */
   for (k = 0; k < pcs->cmm_icc_profile_data->num_comps; k++){
    
        psrc[k] = pcc->paint.values[k]*65535;
           
    }

    /* Get a link from the cache, or create if it is not there. Need to get 16 bit profile */
    icc_link = gsicc_get_link(pis, pcs, NULL, &rendering_params, pis->memory, false);

    /* Transform the color */
    gscms_transform_color(icc_link, psrc, psrc_cm, 2, NULL);

    /* Release the link */
    gsicc_release_link(icc_link);

    /* We are already in short form */
    color = dev_proc(dev, encode_color)(dev, psrc_cm);
    if (color != gx_no_color_index) 
	color_set_pure(pdc, color);

    /* Save original color space and color info into dev color */
    i = pcs->cmm_icc_profile_data->num_comps;
    for (i--; i >= 0; i--)
	pdc->ccolor.paint.values[i] = pcc->paint.values[i];
    pdc->ccolor_valid = true;
    return 0;


}

/*
 * Convert an ICCBased color space to a concrete color space.
 */
static int
gx_concretize_ICC(
    const gs_client_color * pcc,
    const gs_color_space *  pcs,
    frac *                  pconc,
    const gs_imager_state * pis )
{

    gsicc_link_t *icc_link;
    gsicc_rendering_param_t rendering_params;
    unsigned short psrc[GS_CLIENT_COLOR_MAX_COMPONENTS], psrc_cm[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int k;

    /* Define the rendering intents.  MJV to fix */
    rendering_params.black_point_comp = BP_ON;
    rendering_params.object_type = GS_PATH_TAG;
    rendering_params.rendering_intent = gsSATURATION;

     /* This need to be optimized */
   for (k = 0; k < pcs->cmm_icc_profile_data->num_comps; k++){
    
        psrc[k] = pcc->paint.values[k]*65535;
           
    }

    /* Get a link from the cache, or create if it is not there. Get 16 bit profile */
    icc_link = gsicc_get_link(pis, pcs, NULL, &rendering_params, pis->memory, false);

    /* Transform the color */
    gscms_transform_color(icc_link, psrc, psrc_cm, 2, NULL);

    /* This needs to be optimized */
    for (k = 0; k < pis->icc_manager->device_profile->num_comps; k++){
    
        pconc[k] = float2frac(((float) psrc_cm[k])/65535.0);
           
    }


    /* Release the link */
    gsicc_release_link(icc_link);

    return 0;
}

#if ENABLE_CUSTOM_COLOR_CALLBACK
/*
 * This routine is only used if ENABLE_CUSTOM_COLOR_CALLBACK is true.
 * Otherwise we use gx_default_remap_color directly for CIEBasedDEFG color
 * spaces.
 *
 * Render a CIEBasedDEFG color.
 */
int
gx_remap_ICCBased(const gs_client_color * pc, const gs_color_space * pcs,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
		gs_color_select_t select)
{
    client_custom_color_params_t * pcb =
	    (client_custom_color_params_t *) (pis->memory->gs_lib_ctx->custom_color_callback);

    if (pcb != NULL) {
	if (pcb->client_procs->remap_ICCBased(pcb, pc, pcs,
			   			pdc, pis, dev, select) == 0)
	    return 0;
    }
    /* Use default routine for non custom color processing. */
    return gx_default_remap_color(pc, pcs, pdc, pis, dev, select);
}

#endif

/*
 * Finalize the contents of an ICC color space. Now that color space
 * objects have straightforward reference counting discipline, there's
 * nothing special about it. In the previous state of affairs, the
 * argument in favor of correct reference counting spoke of "an
 * unintuitive but otherwise legitimate state of affairs".
 */
static void
gx_final_ICC(const gs_color_space * pcs)
{

    if (pcs->cmm_icc_profile_data != NULL) {

        rc_decrement_only(pcs->cmm_icc_profile_data, "gx_final_ICC");

    }

    /* rc_decrement_only(pcs->params.icc.picc_info, "gx_final_ICC"); */
}

/*
 * Install an ICCBased color space.
 *
 * Note that an ICCBased color space must be installed before it is known if
 * the ICC profile or the alternate color space is to be used.
 */
static int
gx_install_ICC(gs_color_space * pcs, gs_state * pgs)
{

#if ENABLE_CUSTOM_COLOR_CALLBACK
    {
        /*
         * Check if we want to use the callback color processing for this
         * color space.
         */
        client_custom_color_params_t * pcb =
	    (client_custom_color_params_t *) pgs->memory->gs_lib_ctx->custom_color_callback;

        if (pcb != NULL) {
	    if (pcb->client_procs->install_ICCBased(pcb, pcs, pgs))
    	        /* Exit if the client will handle the colorspace completely */
		return 0;
        }
    }
#endif
    /* update the stub information used by the joint caches */
    return 0;
}


/*
 * Constructor for ICCBased color space. As with the other color space
 * constructors, this provides only minimal initialization.
 */
int
gs_cspace_build_ICC(
    gs_color_space **   ppcspace,
    void *              client_data,
    gs_memory_t *       pmem )
{

    gs_color_space *pcspace = gs_cspace_alloc(pmem, &gs_color_space_type_ICC);
    *ppcspace = pcspace;

    return 0;
}

/* ---------------- Serialization. -------------------------------- */

static int
gx_serialize_ICC(const gs_color_space * pcs, stream * s)
{

    return(0);
}
