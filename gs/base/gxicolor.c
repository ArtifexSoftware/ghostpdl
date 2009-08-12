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
#include "gsicccache.h"
#include "gsicc_littlecms.h"

typedef union {
    byte v[GS_IMAGE_MAX_COLOR_COMPONENTS];
#define BYTES_PER_BITS32 4
#define BITS32_PER_COLOR_SAMPLES\
  ((GS_IMAGE_MAX_COLOR_COMPONENTS + BYTES_PER_BITS32 - 1) / BYTES_PER_BITS32)
    bits32 all[BITS32_PER_COLOR_SAMPLES];	/* for fast comparison */
} color_samples;

/* ------ Strategy procedure ------ */

/* Check the prototype. */
iclass_proc(gs_image_class_4_color);

static irender_proc(image_render_color);
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
    return &image_render_color;
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

/* Render a color image with 8 or fewer bits per sample. */
static int
image_render_color(gx_image_enum *penum_orig, const byte *buffer, int data_x,
		   uint w, int h, gx_device * dev)
{
    const gx_image_enum *const penum = penum_orig; /* const within proc */
    gx_image_clue *const clues = penum_orig->clues; /* not const */
    const gs_imager_state *pis = penum->pis;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed xprev, yprev;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int vci, vdi;
    const gs_color_space *pcs = penum->pcs;
    cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
    cs_proc_remap_concrete_color((*remap_concrete_color)) =
	    pcs->type->remap_concrete_color;
    gs_client_color cc;
    bool device_color = penum->device_color;
    const gx_color_map_procs *cmap_procs = gx_get_cmap_procs(pis, dev);
    bits32 mask = penum->mask_color.mask;
    bits32 test = penum->mask_color.test;
    gx_device_color devc;
    gx_device_color *pdevc;
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
    bool use_cache = spp * penum->bps <= 12;
    int code = 0, mcode = 0;
    gsicc_rendering_param_t rendering_params;
    gsicc_link_t *icc_link;
    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;
    unsigned char *psrc_cm, *psrc_cm_start;
    int k;
    gx_color_index color;
    gx_color_value conc[GX_DEVICE_COLOR_MAX_COMPONENTS];

    pdevc = &devc;

    /* Define the rendering intents */
    rendering_params.black_point_comp = BP_ON;
    rendering_params.object_type = GS_IMAGE_TAG;
    rendering_params.rendering_intent = gsPERCEPTUAL;

    if (h == 0)
	return 0;

    /* Request the ICC link for the transform that we will need to use */
    
    icc_link = gsicc_get_link(pis, pcs, NULL, &rendering_params, pis->memory, false);

    /* Set up the buffer descriptors.  Hard coded for my testing of this */
    /* Need to clean up how to indicate the number of channels in the transform
       as well as the profile type */

    gsicc_init_buffer(&input_buff_desc, spp, 1,
                  false, false, false, 0, w,
                  1, w/spp, gsRGB);

    gsicc_init_buffer(&output_buff_desc, dev->color_info.num_components, 1,
                  false, false, false, 0, w,
                  1, w * dev->color_info.num_components/spp, gsRGB);

    /* For now, just blast it all through the link. If we had a significant reduction 
       we will want to repack the data first and then do this.  That will be 
       an optimization shortly.  Also we are going to have to do something here
       with respect to the decode.  If sdnone is the case, then we are fine but the
       others will need to be considered.  Also, for now just allocate a new output
       buffer.  We can reuse the old one if the number of channels in the output is
       less than or equal to the new one.  We will do that soon. */

    psrc_cm = gs_alloc_bytes(pis->memory, w * dev->color_info.num_components/spp, "image_render_color");
    psrc_cm_start = psrc_cm;
    bufend = psrc_cm + w * dev->color_info.num_components/spp;

    gscms_transform_color_buffer(icc_link, &input_buff_desc, &output_buff_desc, 
                             psrc,psrc_cm);

    /* Release the link */

    gsicc_release_link(icc_link);

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

    run.v[0] = ~psrc_cm[0];	/* Force intial setting */

    while (psrc_cm < bufend) {

        dda_next(pnext.x);
	dda_next(pnext.y);

        if ( penum->alpha ) {
        
            /* If the pixels are different, then take care of the alpha now */
            /* will need to adjust spp below.... */
            

             

        } else {

            /* No alpha. */

      /*      for ( k = 0; k < spp; k++ ) {
	        next.v[k] = psrc_cm[k];
            } */

            memcpy(&(next.v[0]),psrc_cm, spp);
            psrc_cm += spp;

        }

        /* Compare to previous.  If same then move on */

        if (posture != image_skewed && next.all[0] == run.all[0])
            goto inc;
            
         for ( k = 0; k < spp; k++ ) {
            decode_sample(next.v[k], cc, k);
            devc.ccolor.paint.values[k] = cc.paint.values[k];
            conc[k] = gx_unit_frac(cc.paint.values[k]);
        }

        /* encode as a color index */
        color = dev_proc(dev, encode_color)(dev, &(conc[0]));

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
        color_set_pure(pdevc, color);

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

    /* Free cm buffer */
    gs_free_object(pis->memory, (byte *)psrc_cm_start, "image_render_color");
    return (code < 0 ? code : 1);
    /* Save position if error, in case we resume. */
err:

    gs_free_object(pis->memory, (byte *)psrc_cm_start, "image_render_color");
    penum_orig->used.x = (rsrc - spp - psrc_initial) / spp;
    penum_orig->used.y = 0;
    return code;
}
