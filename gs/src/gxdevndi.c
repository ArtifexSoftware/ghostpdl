/* Copyright (C) 1989, 1995, 1996, 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id$ */
#include "gx.h"
#include "gsstruct.h"
#include "gsdcolor.h"
#include "gxdevice.h"
#include "gxlum.h"
#include "gxcmap.h"
#include "gxdither.h"
#include "gzht.h"
#include "gxfrac.h"
#include "gxwts.h"

/*
 * Binary halftoning algorithms.
 *
 * The procedures in this file use halftoning (if necessary)
 * to implement a given device color that has already gone through
 * the transfer function.  There are two major cases: gray and color.
 * Gray halftoning always uses a binary screen.  Color halftoning
 * uses either a fast algorithm with a binary screen that produces
 * relatively poor approximations, or a very slow algorithm with a
 * general colored screen (or screens) that faithfully implements
 * the Adobe specifications.
 */

/* Tables for fast computation of fractional color levels. */
/* We have to put the table before any uses of it because of a bug */
/* in the VAX C compiler. */
/* We have to split up the definition of the table itself because of a bug */
/*  in the IBM AIX 3.2 C compiler. */
private const gx_color_value q0[] = {
    0
};
private const gx_color_value q1[] = {
    0, frac_color_(1, 1)
};
private const gx_color_value q2[] = {
    0, frac_color_(1, 2), frac_color_(2, 2)
};
private const gx_color_value q3[] = {
    0, frac_color_(1, 3), frac_color_(2, 3), frac_color_(3, 3)
};
private const gx_color_value q4[] = {
    0, frac_color_(1, 4), frac_color_(2, 4), frac_color_(3, 4),
    frac_color_(4, 4)
};
private const gx_color_value q5[] = {
    0, frac_color_(1, 5), frac_color_(2, 5), frac_color_(3, 5),
    frac_color_(4, 5), frac_color_(5, 5)
};
private const gx_color_value q6[] = {
    0, frac_color_(1, 6), frac_color_(2, 6), frac_color_(3, 6),
    frac_color_(4, 6), frac_color_(5, 6), frac_color_(6, 6)
};
private const gx_color_value q7[] = {
    0, frac_color_(1, 7), frac_color_(2, 7), frac_color_(3, 7),
    frac_color_(4, 7), frac_color_(5, 7), frac_color_(6, 7), frac_color_(7, 7)
};

/* We export fc_color_quo for the fractional_color macro in gzht.h. */
const gx_color_value *const fc_color_quo[8] = {
    q0, q1, q2, q3, q4, q5, q6, q7
};

/* Begin code for setting up WTS device color. This should probably
   move into its own module. */

/**
 * gx_render_device_DevN_wts: Render DeviceN color by halftoning with WTS.
 *
 * This routine is the primary constructor for WTS device colors.
 * Note that, in the WTS code path, we sample the plane_vector array
 * during device color construction, while in the legacy code path,
 * it is sampled in the set_ht_colors procedure, invoked from
 * fill_rectangle. Does it affect correctness? I don't think so, but
 * it needs to be tested.
 **/
private int
gx_render_device_DeviceN_wts(frac * pcolor,
			     gx_device_color * pdevc, gx_device * dev,
			     gx_device_halftone * pdht,
			     const gs_int_point * ht_phase)
{
    int i;
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int num_comp = pdht->num_comp;

    for (i = 0; i < num_comp; i++) {
	cv[i] = 0;
    }

    pdevc->type = gx_dc_type_wts;
    pdevc->colors.wts.w_ht = pdht;

    if (dev->color_info.separable_and_linear != GX_CINFO_SEP_LIN) {
	/* Monochrome case may be inverted. */
	pdevc->colors.wts.plane_vector[1] =
	    dev_proc(dev, encode_color)(dev, cv);
    }
    for (i = 0; i < num_comp; i++) {
	pdevc->colors.wts.levels[i] = pcolor[i];
	cv[i] = gx_max_color_value;
	pdevc->colors.wts.plane_vector[i] =
	    dev_proc(dev, encode_color)(dev, cv);
	cv[i] = 0;
    }
    pdevc->colors.wts.num_components = num_comp;
    return 0;
}

/*
 * Render DeviceN possibly by halftoning.
 *  pcolors = pointer to an array color values (as fracs)
 *  pdevc - pointer to device color structure
 *  dev = pointer to device data structure
 *  pht = pointer to halftone data structure
 *  ht_phase  = halftone phase
 *  gray_colorspace = true -> current color space is DeviceGray.
 *  This is part of a kludge to minimize differences in the
 *  regression testing.
 */
int
gx_render_device_DeviceN(frac * pcolor,
	gx_device_color * pdevc, gx_device * dev,
	gx_device_halftone * pdht, const gs_int_point * ht_phase,
	bool gray_colorspace)
{
    uint max_value = (dev->color_info.max_components == 1) ?
	dev->color_info.dither_grays - 1 :
	dev->color_info.dither_colors - 1;
    frac rem_color[GS_CLIENT_COLOR_MAX_COMPONENTS];
    frac dither_check = 0;
    uint int_color[GS_CLIENT_COLOR_MAX_COMPONENTS];
    gx_color_value vcolor[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int i;
    int num_colors = dev->color_info.num_components;
    uint l_color[GS_CLIENT_COLOR_MAX_COMPONENTS];

    if (pdht && pdht->components && pdht->components[0].corder.wts)
	return gx_render_device_DeviceN_wts(pcolor, pdevc, dev, pdht,
					    ht_phase);

    /* TO_DO_DEVICEN - kludge to minimize DeviceN regressions */
    if (gray_colorspace || num_colors == 1) {
        bool invert = dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE;

	for (i = 0; i < num_colors; i++) {
	    unsigned long hsize = pdht ?
		    (unsigned) pdht->components[i].corder.num_levels
	    	    : 1;
	    unsigned long nshades = hsize * max_value + 1;
	    long shade = (invert ? frac_1 - pcolor[i] : pcolor[i]) *
				nshades / (frac_1_long + 1);
	    int_color[i] = (invert ? nshades - 1 - shade : shade) / hsize;
	    l_color[i] = (invert ? nshades - 1 - shade : shade) % hsize;
	    dither_check |= l_color[i];
	}
    } else {

        /* Compute the quotient and remainder of each color component */
        /* with the actual number of available colors. */
        switch (max_value) {
	    case 1:		/* 8 or 16 colors */
	        for (i = 0; i < num_colors; i++) {
	            if (pcolor[i] == frac_1) {
		        rem_color[i] = 0;
		        int_color[i] = 1;
		    }
		    else {
		        rem_color[i] = pcolor[i];
		        dither_check |= rem_color[i];
		        int_color[i] = 0;
		    }
	        }
	        break;
	    default:
	        {
		    ulong want_x;

	            for (i = 0; i < num_colors; i++) {
		        want_x = (ulong) max_value * pcolor[i];
		        int_color[i] = frac_1_quo(want_x);
		        rem_color[i] = frac_1_rem(want_x, int_color[i]);
		        dither_check |= rem_color[i];
	            }
	        }
        }

	for (i = 0; i < num_colors; i++)
	    l_color[i] = rem_color[i] * pdht->components[i].corder.num_levels
				/ frac_1;
    }

    /* Check for no dithering required */
    if (!dither_check) {
	for (i = 0; i < num_colors; i++)
	    vcolor[i] = fractional_color(int_color[i], max_value);
	color_set_pure(pdevc, dev_proc(dev, encode_color)(dev, vcolor));
	return 0;
    }


#ifdef DEBUG
    if (gs_debug_c('c')) {
	dlprintf1("[c]ncomp=%d ", num_colors);
	for (i = 0; i < num_colors; i++)
	    dlprintf1("0x%x, ", pcolor[i]);
	dlprintf("-->\n   ");
	for (i = 0; i < num_colors; i++)
	    dlprintf2("%x+0x%x, ", int_color[i], rem_color[i]);
	dlprintf("-->\n");
    }
#endif

    /* Use the slow, general colored halftone algorithm. */

    for (i = 0; i < num_colors; i++)
        _color_set_c(pdevc, i, int_color[i], l_color[i]);
    gx_complete_halftone(pdevc, num_colors, pdht);

    color_set_phase_mod(pdevc, ht_phase->x, ht_phase->y,
			    pdht->lcm_width, pdht->lcm_height);

    /* Determine if we are using only one component */
    if (!(pdevc->colors.colored.plane_mask &
	 (pdevc->colors.colored.plane_mask - 1))) {
	/* We can reduce this color to a binary halftone or pure color. */
	return gx_devn_reduce_colored_halftone(pdevc, dev);
    }

    return 1;
}

/* Reduce a colored halftone to a binary halftone or pure color. */
/* This routine is called when only one component is being halftoned. */
int
gx_devn_reduce_colored_halftone(gx_device_color *pdevc, gx_device *dev)
{
    int planes = pdevc->colors.colored.plane_mask;
    int gray_index = dev->color_info.gray_index;
    int num_colors = dev->color_info.num_components;
    gx_color_value max_color = (num_colors == 1 && gray_index == 0) ?
	dev->color_info.dither_grays - 1 :
	dev->color_info.dither_colors - 1;
    uint b[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value v[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index c0, c1;
    int i;

    for (i = 0; i < num_colors; i++) {
        b[i] = pdevc->colors.colored.c_base[i];
        v[i] = fractional_color(b[i], max_color);
    }
    c0 = dev_proc(dev, encode_color)(dev, v);

    if (planes == 0) {
	/*
	 * Use a pure color.  This case is unlikely, but it can occur if
	 * (and only if) the difference of each component from the nearest
	 * device color is less than one halftone level.
	 */
	color_set_pure(pdevc, c0);
	return 0;
    } else {
	/* Use a binary color. */
	int i = 0;
	uint bi;
	const gx_device_halftone *pdht = pdevc->colors.colored.c_ht;
	/*
	 * NB: the halftone orders are all set up for an additive color
	 *     space.  To use these work with a subtractive color space, it is
	 *     necessary to invert both the color level and the color
	 *     pair. Note that if the original color was provided an
	 *     additive space, this will reverse (in an approximate sense)
	 *     the color conversion performed to express the color in
	 *     subtractive space.
	 */
	bool invert = dev->color_info.polarity == GX_CINFO_POLARITY_SUBTRACTIVE;
	uint level;

	/* Convert plane mask bit position to component number */
	/* Determine i = log2(planes);  This works for powers of two */
	while (planes > 7) {
	    i += 3;
	    planes >>= 3;
	}
	i += planes >> 1;  /* log2 for 1,2,4 */

	bi = b[i] + 1;
	v[i] = fractional_color(bi, max_color);
	level = pdevc->colors.colored.c_level[i];
        c1 = dev_proc(dev, encode_color)(dev, v);
	if (invert) {
	    level = pdht->components[i].corder.num_levels - level;
	    color_set_binary_halftone_component(pdevc, pdht, i, c1, c0, level);
	} else
	    color_set_binary_halftone_component(pdevc, pdht, i, c0, c1, level);
					    
	return 1;
    }
}
