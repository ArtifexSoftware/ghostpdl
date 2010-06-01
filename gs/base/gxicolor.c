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
/* Color image rendering */

#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxfrac.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcconv.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gxdevmem.h"
#include "gxcpath.h"
#include "gximage.h"
#include "gsicc.h"
#include "gsicc_cache.h"
#include "gsicc_littlecms.h"
#include "gxcie.h"
#include "gscie.h"

typedef union {
    byte v[GS_IMAGE_MAX_COLOR_COMPONENTS];
#define BYTES_PER_BITS32 4
#define BITS32_PER_COLOR_SAMPLES\
  ((GS_IMAGE_MAX_COLOR_COMPONENTS + BYTES_PER_BITS32 - 1) / BYTES_PER_BITS32)
    bits32 all[BITS32_PER_COLOR_SAMPLES];	/* for fast comparison */
} color_samples;

extern void cmap_transfer_halftone(gx_color_value *pconc, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, bool has_transfer,
     bool has_halftone, gs_color_select_t select);

/* ------ Strategy procedure ------ */

/* Check the prototype. */
iclass_proc(gs_image_class_4_color);

static irender_proc(image_render_color_DeviceN);
static irender_proc(image_render_color_icc);

/* Returns false if range is not 0 1 */
static bool
check_cie_range( const gs_color_space * pcs )
{
    switch(gs_color_space_get_index(pcs)){
        case gs_color_space_index_CIEDEFG:
            return(check_range(&(pcs->params.defg->RangeDEFG.ranges[0]), 4));
        case gs_color_space_index_CIEDEF:
            return(check_range(&(pcs->params.def->RangeDEF.ranges[0]), 3));
        case gs_color_space_index_CIEABC:
            return(check_range(&(pcs->params.abc->RangeABC.ranges[0]), 3));
        case gs_color_space_index_CIEA:
            return(check_range(&(pcs->params.a->RangeA), 1));
        default:            
            return true;
    }
}

static gs_range*
get_cie_range( const gs_color_space * pcs )
{
    switch(gs_color_space_get_index(pcs)){
        case gs_color_space_index_CIEDEFG:
            return(&(pcs->params.defg->RangeDEFG.ranges[0]));
        case gs_color_space_index_CIEDEF:
            return(&(pcs->params.def->RangeDEF.ranges[0]));
        case gs_color_space_index_CIEABC:
            return(&(pcs->params.abc->RangeABC.ranges[0]));
        case gs_color_space_index_CIEA:
            return(&(pcs->params.a->RangeA));
        default:            
            return NULL;
    }
}

irender_proc_t
gs_image_class_4_color(gx_image_enum * penum)
{
    if (penum->use_mask_color) {
	/*
	 * Scale the mask colors to match the scaling of each sample to
	 * a full byte, and set up the quick-filter parameters.
	 */
	int i;
	color_samples mask, test;
	bool exact = penum->spp <= BYTES_PER_BITS32;

	memset(&mask, 0, sizeof(mask));
	memset(&test, 0, sizeof(test));
	for (i = 0; i < penum->spp; ++i) {
	    byte v0, v1;
	    byte match = 0xff;

	    gx_image_scale_mask_colors(penum, i);
	    v0 = (byte)penum->mask_color.values[2 * i];
	    v1 = (byte)penum->mask_color.values[2 * i + 1];
	    while ((v0 & match) != (v1 & match))
		match <<= 1;
	    mask.v[i] = match;
	    test.v[i] = v0 & match;
	    exact &= (v0 == match && (v1 | match) == 0xff);
	}
	penum->mask_color.mask = mask.all[0];
	penum->mask_color.test = test.all[0];
	penum->mask_color.exact = exact;
    } else {
	penum->mask_color.mask = 0;
	penum->mask_color.test = ~0;
    }
    if ( (gs_color_space_get_index(penum->pcs) == gs_color_space_index_DeviceN &&
        penum->pcs->cmm_icc_profile_data == NULL) || penum->use_mask_color ) {
        return &image_render_color_DeviceN;
    } else {
        /* Set up the link now */
        const gs_color_space *pcs;
        gsicc_rendering_param_t rendering_params;
        int k;
        int src_num_comp = cs_num_components(penum->pcs);

         penum->icc_setup.need_decode = false;
        /* Check if we need to do any decoding.  If yes, then that will slow us down */
        for (k = 0; k < src_num_comp; k++) {
            if ( penum->map[k].decoding != sd_none ) {
                penum->icc_setup.need_decode = true;
                break;
}
        }
        /* Define the rendering intents */
        rendering_params.black_point_comp = BP_ON;
        rendering_params.object_type = GS_IMAGE_TAG;
        rendering_params.rendering_intent = penum->pis->renderingintent;
        if (gs_color_space_is_PSCIE(penum->pcs) && penum->pcs->icc_equivalent != NULL) {
            pcs = penum->pcs->icc_equivalent;
        } else {
            pcs = penum->pcs;
        }
        penum->icc_setup.is_lab = pcs->cmm_icc_profile_data->islab;
        penum->icc_setup.must_halftone = gx_device_must_halftone(penum->dev);
        penum->icc_setup.has_transfer = gx_has_transfer(penum->pis,
                                penum->pis->icc_manager->device_profile->num_comps);
        if (penum->icc_setup.is_lab) penum->icc_setup.need_decode = false;
        if (penum->icc_link == NULL) {
            penum->icc_link = gsicc_get_link(penum->pis, pcs, NULL, 
                &rendering_params, penum->memory, false);
        }
        /* PS CIE color spaces may have addition decoding that needs to
           be performed to ensure that the range of 0 to 1 is provided
           to the CMM since ICC profiles are restricted to that range
           but the PS color spaces are not. */
        if (gs_color_space_is_PSCIE(penum->pcs) && 
            penum->pcs->icc_equivalent != NULL) {
            /* We have a PS CIE space.  Check the range */
            if ( !check_cie_range(penum->pcs) ) {
                /* It is not 0 to 1.  We will be doing decode
                   plus an additional linear adjustment */
                penum->cie_range = get_cie_range(penum->pcs);
            }
        }
        return &image_render_color_icc;
    }
}

/* ------ Rendering procedures ------ */

/* Test whether a color is transparent. */
static bool
mask_color_matches(const byte *v, const gx_image_enum *penum,
		   int num_components)
{
    int i;

    for (i = num_components * 2, v += num_components - 1; (i -= 2) >= 0; --v)
	if (*v < penum->mask_color.values[i] ||
	    *v > penum->mask_color.values[i + 1]
	    )
	    return false;
    return true;
}

static inline float
rescale_input_color(gs_range range, float input)
{
    return((input-range.rmin)/(range.rmax-range.rmin));
}

/* This one includes an extra adjustment for the CIE PS color space 
   non standard range */
static void 
decode_row_cie(const gx_image_enum *penum, byte *psrc, int spp, byte *pdes, 
                byte *bufend, gs_range range_array[])
{
    byte *curr_pos = pdes;
    int k;
    float temp;

    while ( curr_pos < bufend ) {
        for ( k = 0; k < spp; k ++ ) {
            switch ( penum->map[k].decoding ) {
                case sd_none:
                    *curr_pos = *psrc;
                    break;
                case sd_lookup:	
                    temp = penum->map[k].decode_lookup[(*psrc) >> 4]*255.0;
                    temp = rescale_input_color(range_array[k], temp);
                    temp = temp*255;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                    break;
                case sd_compute:
                    temp = penum->map[k].decode_base + 
                        (*psrc) * penum->map[k].decode_factor;
                    temp = rescale_input_color(range_array[k], temp);
                    temp = temp*255;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                default:
                    break;
            }
            curr_pos++;
            psrc++;
        }
    }
}

static void 
decode_row(const gx_image_enum *penum, byte *psrc, int spp, byte *pdes, 
                byte *bufend)
{
    byte *curr_pos = pdes;
    int k;
    float temp;

    while ( curr_pos < bufend ) {
        for ( k = 0; k < spp; k ++ ) {
            switch ( penum->map[k].decoding ) {
                case sd_none:
                    *curr_pos = *psrc;
                    break;
                case sd_lookup:	
                    temp = penum->map[k].decode_lookup[(*psrc) >> 4]*255.0;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                    break;
                case sd_compute:
                    temp = penum->map[k].decode_base + 
                        (*psrc) * penum->map[k].decode_factor;
                    temp *= 255;
                    if (temp > 255) temp = 255;
                    if (temp < 0 ) temp = 0;
                    *curr_pos = (unsigned char) temp;
                default:
                    break;
            }
            curr_pos++;
            psrc++;
        }
    }
}

/* Render a color image with 8 or fewer bits per sample using ICC profile. */
static int
image_render_color_icc(gx_image_enum *penum_orig, const byte *buffer, int data_x,
		   uint w, int h, gx_device * dev)
{
    const gx_image_enum *const penum = penum_orig; /* const within proc */
    const gs_imager_state *pis = penum->pis;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed xprev, yprev;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int vci, vdi;
    const gs_color_space *pcs;
    gx_device_color devc1;
    gx_device_color devc2;
    gx_device_color *pdevc;
    gx_device_color *pdevc_next;
    gx_device_color *ptemp;
    int spp = penum->spp;
    const byte *psrc_initial = buffer + data_x * spp;
    const byte *psrc = psrc_initial;
    const byte *rsrc = psrc + spp; /* psrc + spp at start of run */
    fixed xrun;			/* x ditto */
    fixed yrun;			/* y ditto */
    int irun;			/* int x/rrun */
    color_samples run;		/* run value */
    color_samples next;		/* next sample value */
    byte *bufend;
    int code = 0, mcode = 0;
    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;
    unsigned char *psrc_cm, *psrc_cm_start, *psrc_decode;
    int k;
    gx_color_value conc[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int spp_cm, num_pixels;
    gx_color_index color;
    int src_num_comp = cs_num_components(penum->pcs);
    bool need_decode = penum->icc_setup.need_decode;
    bool must_halftone = penum->icc_setup.must_halftone;
    bool has_transfer = penum->icc_setup.has_transfer;

    if (penum->icc_link == NULL) {
        return gs_rethrow(-1, "ICC Link not created during image render color");
    }
    /* Needed for device N */
    memset(&(conc[0]), 0, sizeof(gx_color_value[GX_DEVICE_COLOR_MAX_COMPONENTS]));
    if (gs_color_space_is_PSCIE(penum->pcs) && penum->pcs->icc_equivalent != NULL) {
        pcs = penum->pcs->icc_equivalent;
    } else {
        pcs = penum->pcs;
    }
    pdevc = &devc1;
    pdevc_next = &devc2;
    /* These used to be set by init clues */
    pdevc->type = gx_dc_type_none;
    pdevc_next->type = gx_dc_type_none;
    if (h == 0)
	return 0;
    /* If the link is the identity, then we don't need to do any color 
       conversions except for potentially a decode. */
    if (penum->icc_link->is_identity && !need_decode) {
        /* Fastest case.  No decode or CM needed */
        psrc_cm = (unsigned char *) psrc;
        spp_cm = spp;
        bufend = psrc_cm +  w;
        psrc_cm_start = NULL;
    } else {
        spp_cm = pis->icc_manager->device_profile->num_comps;
        psrc_cm = gs_alloc_bytes(pis->memory,  w * spp_cm/spp, "image_render_color_icc");
        psrc_cm_start = psrc_cm;
        bufend = psrc_cm +  w * spp_cm/spp;
        if (penum->icc_link->is_identity) {
            /* decode only. no CM.  This is slow but does not happen that often */
            decode_row(penum, psrc, spp, psrc_cm, bufend);    
        } else {
            /* Set up the buffer descriptors. */
            num_pixels = w/spp;
            gsicc_init_buffer(&input_buff_desc, spp, 1,
                          false, false, false, 0, w,
                          1, num_pixels);
            gsicc_init_buffer(&output_buff_desc, spp_cm, 1,
                          false, false, false, 0, num_pixels * spp_cm,
                          1, num_pixels);
            /* For now, just blast it all through the link. If we had a significant reduction 
               we will want to repack the data first and then do this.  That will be 
               an optimization shortly.  For now just allocate a new output
               buffer.  We can reuse the old one if the number of channels in the output is
               less than or equal to the new one.  */
            if (need_decode) {
                /* Need decode and CM.  This is slow but does not happen that often */
                psrc_decode = gs_alloc_bytes(pis->memory,  w, "image_render_color_icc");
                if (penum->cie_range == NULL) {
                    decode_row(penum, psrc, spp, psrc_decode, psrc_decode+w);
                } else {
                    /* Decode needs to include adjustment for CIE range */
                    decode_row_cie(penum, psrc, spp, psrc_decode, 
                                    psrc_decode+w, penum->cie_range);
                }
                gscms_transform_color_buffer(penum->icc_link, &input_buff_desc, 
                                        &output_buff_desc, (void*) psrc_decode, 
                                        (void*) psrc_cm);
                gs_free_object(pis->memory, (byte *)psrc_decode, "image_render_color_icc");
            } else {
                /* CM only. No decode */
                gscms_transform_color_buffer(penum->icc_link, &input_buff_desc, 
                                            &output_buff_desc, (void*) psrc, 
                                            (void*) psrc_cm);
            }
        }
    }
    pnext = penum->dda.pixel0;
    xrun = xprev = dda_current(pnext.x);
    yrun = yprev = dda_current(pnext.y);
    pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
    pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
    switch (posture) {
	case image_portrait:
	    vci = penum->yci, vdi = penum->hci;
	    irun = fixed2int_var_rounded(xrun);
	    break;
	case image_landscape:
	default:    /* we don't handle skew -- treat as landscape */
	    vci = penum->xci, vdi = penum->wci;
	    irun = fixed2int_var_rounded(yrun);
	    break;
    }
    if_debug5('b', "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
	      penum->y, data_x, w, fixed2float(xprev), fixed2float(yprev));
    memset(&run, 0, sizeof(run));
    memset(&next, 0, sizeof(next));
    run.v[0] = ~psrc_cm[0];	/* Force intial setting */
    while (psrc_cm < bufend) {
	dda_next(pnext.x);
	dda_next(pnext.y);
        if ( penum->alpha ) {
            /* If the pixels are different, then take care of the alpha now */
            /* will need to adjust spp below.... */
        } else {
            memcpy(&(next.v[0]),psrc_cm, spp_cm);
            psrc_cm += spp_cm;
        }
        /* Compare to previous.  If same then move on */
        if (posture != image_skewed && next.all[0] == run.all[0])
		goto inc;
        /* This needs to be sped up */
         for ( k = 0; k < spp_cm; k++ ) {
            conc[k] = gx_color_value_from_byte(next.v[k]);
        }
        /* Now we can do an encoding directly or we have to apply transfer
           and or halftoning */
        if (must_halftone || has_transfer) {
            /* We need to do the tranfer function and/or the halftoning */
            cmap_transfer_halftone(&(conc[0]), pdevc_next, pis, dev, 
                has_transfer, must_halftone, gs_color_select_source);
        } else {
            /* encode as a color index. avoid all the cv to frac to cv
               conversions */
            color = dev_proc(dev, encode_color)(dev, &(conc[0]));
            /* check if the encoding was successful; we presume failure is rare */
            if (color != gx_no_color_index)
                color_set_pure(pdevc_next, color);
        }
        /* Fill the region between */
	/* xrun/irun and xprev */
		/*
	 * Note;  This section is nearly a copy of a simlar section below
         * for processing the last image pixel in the loop.  This would have been
         * made into a subroutine except for complications about the number of
         * variables that would have been needed to be passed to the routine.
		 */
	switch (posture) {
	case image_portrait:
	    {		/* Rectangle */
		int xi = irun;
		int wi = (irun = fixed2int_var_rounded(xprev)) - xi;

		if (wi < 0)
		    xi += wi, wi = -wi;
		if (wi > 0)
		    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
							pdevc, dev, lop);
		}
	    break;
	case image_landscape:
	    {		/* 90 degree rotated rectangle */
		int yi = irun;
		int hi = (irun = fixed2int_var_rounded(yprev)) - yi;

		if (hi < 0)
		    yi += hi, hi = -hi;
		if (hi > 0)
		    code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
							pdevc, dev, lop);
	    }
	    break;
	default:
	    {		/* Parallelogram */
		code = (*dev_proc(dev, fill_parallelogram))
		    (dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy,
		     pdevc, lop);
		xrun = xprev;
		yrun = yprev;
	    }
		}
	if (code < 0)
	    goto err;
	rsrc = psrc;
	if ((code = mcode) < 0) goto err;
        /* Swap around the colors due to a change */
        ptemp = pdevc;
        pdevc = pdevc_next;
        pdevc_next = ptemp;
 	run = next;
inc:	xprev = dda_current(pnext.x);
	yprev = dda_current(pnext.y);	/* harmless if no skew */
    }
    /* Fill the last run. */
		/*
     * Note;  This section is nearly a copy of a simlar section above
     * for processing an image pixel in the loop.  This would have been
     * made into a subroutine except for complications about the number
     * variables that would have been needed to be passed to the routine.
		 */
    switch (posture) {
    	case image_portrait:
	    {		/* Rectangle */
		int xi = irun;
		int wi = (irun = fixed2int_var_rounded(xprev)) - xi;

		if (wi < 0)
		    xi += wi, wi = -wi;
		if (wi > 0)
		    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
							pdevc, dev, lop);
			}
	    break;
	case image_landscape:
	    {		/* 90 degree rotated rectangle */
		int yi = irun;
		int hi = (irun = fixed2int_var_rounded(yprev)) - yi;

		if (hi < 0)
		    yi += hi, hi = -hi;
		if (hi > 0)
		    code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
							pdevc, dev, lop);
		}
	    break;
	default:
	    {		/* Parallelogram */
		code = (*dev_proc(dev, fill_parallelogram))
		    (dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy,
		     pdevc, lop);
		}
	    }
    /* Free cm buffer, if it was used */
    if (psrc_cm_start != NULL) {
        gs_free_object(pis->memory, (byte *)psrc_cm_start, "image_render_color_icc");
	    }
    return (code < 0 ? code : 1);
    /* Save position if error, in case we resume. */
err:
    gs_free_object(pis->memory, (byte *)psrc_cm_start, "image_render_color_icc");
    penum_orig->used.x = (rsrc - spp - psrc_initial) / spp;
    penum_orig->used.y = 0;
    return code;
	    }

/* Render a color image for deviceN source color with no ICC profile.  This   
   is also used if the image has any masking (type4 image) since we will not 
   be blasting through quickly */
static int
image_render_color_DeviceN(gx_image_enum *penum_orig, const byte *buffer, int data_x,
		   uint w, int h, gx_device * dev)
{
    const gx_image_enum *const penum = penum_orig; /* const within proc */
    const gs_imager_state *pis = penum->pis;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed xprev, yprev;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int vci, vdi;
    const gs_color_space *pcs = penum->pcs;
    cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
    gs_client_color cc;
    gx_device_color devc1;
    gx_device_color devc2;
    gx_device_color *pdevc;
    gx_device_color *pdevc_next;
    gx_device_color *ptemp;
    int spp = penum->spp;
    const byte *psrc_initial = buffer + data_x * spp;
    const byte *psrc = psrc_initial;
    const byte *rsrc = psrc + spp; /* psrc + spp at start of run */
    fixed xrun;			/* x ditto */
    fixed yrun;			/* y ditto */
    int irun;			/* int x/rrun */
    color_samples run;		/* run value */
    color_samples next;		/* next sample value */
    const byte *bufend = psrc + w;
    int code = 0, mcode = 0;
	    int i;
    bits32 mask = penum->mask_color.mask;
    bits32 test = penum->mask_color.test;

    if (h == 0)
	return 0;
    pdevc = &devc1;
    pdevc_next = &devc2;
    /* These used to be set by init clues */
    pdevc->type = gx_dc_type_none;
    pdevc_next->type = gx_dc_type_none;
    pnext = penum->dda.pixel0;
    xrun = xprev = dda_current(pnext.x);
    yrun = yprev = dda_current(pnext.y);
    pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
    pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
    switch (posture) {
	case image_portrait:
	    vci = penum->yci, vdi = penum->hci;
	    irun = fixed2int_var_rounded(xrun);
	    break;
	case image_landscape:
	default:    /* we don't handle skew -- treat as landscape */
	    vci = penum->xci, vdi = penum->wci;
	    irun = fixed2int_var_rounded(yrun);
	    break;
    }
    if_debug5('b', "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
	      penum->y, data_x, w, fixed2float(xprev), fixed2float(yprev));
    memset(&run, 0, sizeof(run));
    memset(&next, 0, sizeof(next));
    cs_full_init_color(&cc, pcs);
    run.v[0] = ~psrc[0];	/* force remap */
    while (psrc < bufend) {
        dda_next(pnext.x);
        dda_next(pnext.y);
	    if (posture != image_skewed && !memcmp(psrc, run.v, spp)) {
		psrc += spp;
		goto inc;
	    }
	    memcpy(next.v, psrc, spp);
	    psrc += spp;
        /* Check for transparent color. */
	    if ((next.all[0] & mask) == test &&
            penum->mask_color.exact || 
            mask_color_matches(next.v, penum, spp)) {
		color_set_null(pdevc_next);
		goto mapped;
	    }
	    for (i = 0; i < spp; ++i)
		decode_sample(next.v[i], cc, i);
#ifdef DEBUG
	    if (gs_debug_c('B')) {
		dprintf2("[B]cc[0..%d]=%g", spp - 1,
			 cc.paint.values[0]);
		for (i = 1; i < spp; ++i)
		    dprintf1(",%g", cc.paint.values[i]);
		dputs("\n");
	    }
#endif
	mcode = remap_color(&cc, pcs, pdevc_next, pis, dev,
			   gs_color_select_source);
mapped:	if (mcode < 0)
	    goto fill;
f:	if (sizeof(pdevc_next->colors.binary.color[0]) <= sizeof(ulong))
	    if_debug7('B', "[B]0x%x,0x%x,0x%x,0x%x -> 0x%lx,0x%lx,0x%lx\n",
		  next.v[0], next.v[1], next.v[2], next.v[3],
		  (ulong)pdevc_next->colors.binary.color[0],
		  (ulong)pdevc_next->colors.binary.color[1],
		  (ulong) pdevc_next->type);
	else
	    if_debug9('B', "[B]0x%x,0x%x,0x%x,0x%x -> 0x%08lx%08lx,0x%08lx%08lx,0x%lx\n",
		  next.v[0], next.v[1], next.v[2], next.v[3],
		  (ulong)(pdevc_next->colors.binary.color[0] >> 
			8 * (sizeof(pdevc_next->colors.binary.color[0]) - sizeof(ulong))),
		  (ulong)pdevc_next->colors.binary.color[0],
		  (ulong)(pdevc_next->colors.binary.color[1] >> 
			8 * (sizeof(pdevc_next->colors.binary.color[1]) - sizeof(ulong))),
		  (ulong)pdevc_next->colors.binary.color[1],
		  (ulong) pdevc_next->type);
	/* NB: printf above fails to account for sizeof gx_color_index 4 or 8 bytes */
	if (posture != image_skewed && next.all[0] == run.all[0])
	    goto set;
fill:	/* Fill the region between */
	/* xrun/irun and xprev */
        /*
	 * Note;  This section is nearly a copy of a simlar section below
         * for processing the last image pixel in the loop.  This would have been
         * made into a subroutine except for complications about the number of
         * variables that would have been needed to be passed to the routine.
	 */
	switch (posture) {
	case image_portrait:
	    {		/* Rectangle */
		int xi = irun;
		int wi = (irun = fixed2int_var_rounded(xprev)) - xi;

		if (wi < 0)
		    xi += wi, wi = -wi;
		if (wi > 0)
		    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
							pdevc, dev, lop);
	    }
	    break;
	case image_landscape:
	    {		/* 90 degree rotated rectangle */
		int yi = irun;
		int hi = (irun = fixed2int_var_rounded(yprev)) - yi;

		if (hi < 0)
		    yi += hi, hi = -hi;
		if (hi > 0)
		    code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
							pdevc, dev, lop);
	    }
	    break;
	default:
	    {		/* Parallelogram */
		code = (*dev_proc(dev, fill_parallelogram))
		    (dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy,
		     pdevc, lop);
		xrun = xprev;
		yrun = yprev;
	    }
	}
	if (code < 0)
	    goto err;
	rsrc = psrc;

	if ((code = mcode) < 0) goto err;
        /* Swap around the colors due to a change */
        ptemp = pdevc;
        pdevc = pdevc_next;
        pdevc_next = ptemp;
set:	run = next;
inc:	xprev = dda_current(pnext.x);
	yprev = dda_current(pnext.y);	/* harmless if no skew */
    }
    /* Fill the last run. */
    /*
     * Note;  This section is nearly a copy of a simlar section above
     * for processing an image pixel in the loop.  This would have been
     * made into a subroutine except for complications about the number
     * variables that would have been needed to be passed to the routine.
     */
    switch (posture) {
    	case image_portrait:
	    {		/* Rectangle */
		int xi = irun;
		int wi = (irun = fixed2int_var_rounded(xprev)) - xi;

		if (wi < 0)
		    xi += wi, wi = -wi;
		if (wi > 0)
		    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
							pdevc, dev, lop);
	    }
	    break;
	case image_landscape:
	    {		/* 90 degree rotated rectangle */
		int yi = irun;
		int hi = (irun = fixed2int_var_rounded(yprev)) - yi;

		if (hi < 0)
		    yi += hi, hi = -hi;
		if (hi > 0)
		    code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
							pdevc, dev, lop);
	    }
	    break;
	default:
	    {		/* Parallelogram */
		code = (*dev_proc(dev, fill_parallelogram))
		    (dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy,
		     pdevc, lop);
	    }
    }
    return (code < 0 ? code : 1);
    /* Save position if error, in case we resume. */
err:
    penum_orig->used.x = (rsrc - spp - psrc_initial) / spp;
    penum_orig->used.y = 0;
    return code;
}
