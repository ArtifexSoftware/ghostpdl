/* Copyright (C) 2001-2007 artofcode LLC.
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
/* Sample implementation for client custom processing of color spaces. */

/*
 * If this flag is 1 then we also do custom processing for the DeviceGray,
 * DeviceRGB, and DeviceCMYK color spaces.  For these color spaces, we
 * convert all text to shades of red, all images to shades of green and
 * all lines and fills to shades of blue.  if the flag is 0 then our example
 * only handles PANTONE colors (see comment below).
 */
#define OBJECT_TYPE_EXAMPLE 1		/* 0 --> disabled, 1 --> enabled */

/*
 * This module has been created to demonstrate how to support the use of
 * PANTONE colors to the Ghostscript graphics library.  PANTONE colors
 * are specified in both PostScript and PDF files via the use of DeviceN
 * or Separation color spaces.
 *
 * PANTONE is a registered trademark and PANTONE colors are a
 * licensed product of Pantone Inc. See http://www.pantone.com
 * for more information.
 *
 * See the comments at the start of src/gsnamecl.c for description of the
 * client color processing routines.
 *
 * Since this is only a 'demo' implementation, the example implementation does
 * not have some features which might be expected in a 'real' implementation.
 *
 * 1.  The Pantone color data table does not have actual entries for all
 *     of the different Pantone colors.  This data is not included since
 *     the values are dependent upon specific details of the output device,
 *     inks, etc.
 * 2.  Colors in PostScript and PDF are specified with by values between
 *     0 and 1.  The output colorant values are scaled linearly.
 * 3.  DeviceN color spaces can specify multiple colors.  However this
 *     implementation assumes that if a PANTONE color is specified in a
 *     DeviceN color space, then only PANTONE colors or CMYK are present.
 *     This was done to keep the code simple.  If other colors are present,
 *     then this implementation falls back to using the alternate color space
 *     specified with the DeviceN color space.  (This is the normal PS
 *     and PDF operation.)
 *
 * See also src/zsncdummy.c for an example custom color callback. 
 */

#include "stdpre.h"
#include "math_.h"
#include "memory_.h"
#include "ierrors.h"
#include "gx.h"
#include "gserrors.h"
#include "gscdefs.h"
#include "gscspace.h"
#include "gscie.h"
#include "gsicc.h"
#include "gxdevice.h"
#include "gzstate.h"
#include "malloc_.h"
#include "gsutil.h"
#include "gsncdummy.h"

#if ENABLE_CUSTOM_COLOR_CALLBACK		/* Defined in src/gsnamecl.h */

/*
 * Since this is only a 'demo' list, the list does have not entries for all
 * of the different PANTONE colors.  Creation of a real list is left as an
 * exercise for the user.
 */
const pantone_list_t pantone_list[] = { 
    { "PantoneCyan",	1, 0, 0, 0 },
    { "PantoneMagenta",	0, 1, 0, 0 },
    { "PantoneYellow",	0, 0, 1, 0 },
    { "PantoneBlack",	0, 0, 0, 1 },
    { "Orange",	0, 1, .5, 0 }
};

/*
 * We will handle color spaces that include both PANTONE colors, CMYK, and
 * 'None'.  'None' is a special case in DeviceN color spaces.  It has no
 * effects upon the output color but it can be present in DeviceN color
 * spaces in a PDF file.  To simplify the code, we need pantone index values
 * for these five 'colors'.
 */
#define PANTONE_NONE    count_of(pantone_list)
#define PANTONE_CYAN    (PANTONE_NONE + 1)
#define PANTONE_MAGENTA (PANTONE_NONE + 2)
#define PANTONE_YELLOW  (PANTONE_NONE + 3)
#define PANTONE_BLACK   (PANTONE_NONE + 4)

/* Compare two names */
#define compare_names(name1, name_size1, name2, name_size2) \
    (name_size1 == name_size2 && \
	(memcmp((const char *)name1, (const char *)name2, name_size1) == 0))

/*
 * Define a structure for holding our client specific data.  In our demo,
 * we are only supporting Separation and DeviceN color spaces.  To make
 * life simpler, we are using the same data structure for both types
 * of color spaces.
 */
typedef struct demo_color_space_data_s {
    /*
     * All client color space data blocks must begin with a routine for
     * handling the reference counts for the data block.
     */
    cs_proc_adjust_client_cspace_count((*client_adjust_cspace_count));

    /* Use a reference count for knowing when to release the data block. */
    int ref_count;

    /* A flag which indicates the client wants to process the color space. */
    bool client_is_going_to_handle_color_space;

    /*
     * We store an index into our Pantone color translation table for each
     * colorant in the color space.
     */
    int color_index[GS_CLIENT_COLOR_MAX_COMPONENTS];
} demo_color_space_data_t;

/*
 * Dummy install routine for color spaces which are not handled by the client.
 */
private bool
client_install_no_op(client_custom_color_params_t * pparams,
	    gs_color_space * pcs, gs_state * pgs)
{
    return false;	/* Do nothing */
}

/*
 * Dummy convert routine for simple color spaces (gray, RGB, CMYK, DeviceN,
 * and Separation) which are not handled by the client.
 */
private int
client_remap_simple_no_op(client_custom_color_params_t * pparams,
    const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
    const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    /*
     * Returning an error value will cause GS to use its normal processes
     * for handling the color space.
     */
    return_error(gs_error_rangecheck);
}

/*
 * Dummy convert routine for complex color spaces (CIEBasedA, CIEBasedABC,
 * CIEBasedDEF, CIEBasedDEF, CIEBasedDEFG, ICCBased) which are not handled
 * by the client.
 */
private int
client_remap_complex_no_op(client_custom_color_params_t * pparams,
    const gs_client_color * pc, const gs_color_space * pcs,
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    /*
     * Returning an error value will cause GS to use its normal processes
     * for handling the color space.
     */
    return_error(gs_error_rangecheck);
}

/*
 * Since this is an example for a client, we are using the system
 * malloc and free routines instead of the normal GS memory management
 * routines.
 */
private void
client_adjust_cspace_count(const gs_color_space * pcs, int delta)
{
    demo_color_space_data_t * pdata = 
	(demo_color_space_data_t *)(pcs->pclient_color_space_data);

    pdata->ref_count += delta;
    if (pdata->ref_count <= 0)
		free(pdata);
}

/*
 * Allocate a data block for holding our data for the client specific
 * data for a color space.  In our demo, we are only supporting the
 * Separation and DeviceN color spaces.  We use a single data structure
 * to make the code simpler.
 */
private demo_color_space_data_t *
allocate_client_data_block(int initial_ref_count)
{
    /*
     * Since this is an example for a client, we are using the system
     * malloc routine instead of the GS memory management routines.
     * As a result, the client is responsible for freeing the data
     * block.  For this purpose, we are using a simple reference count.
     * See client_adjust_cspace_count.
     */
    demo_color_space_data_t * pdata =
	(demo_color_space_data_t *)malloc(size_of(demo_color_space_data_t));

    if (pdata != NULL) {
		pdata->ref_count = 1;
	
		/*
		 * All client color space data blocks must have a pointer to a
		 * reference count adjust routine as their first field.
		 */
		pdata->client_adjust_cspace_count = client_adjust_cspace_count;

    }
    
    return pdata;
}

private bool
client_install_generic(client_custom_color_params_t * pparams,
	    gs_color_space * pcs, gs_state * pgs)
{
	demo_color_space_data_t * pclient_data;

	/* Exit if we have already installed this color space. */
	if (pcs->pclient_color_space_data != NULL)
		return true;

	pclient_data = allocate_client_data_block(1);
	pcs->pclient_color_space_data = (client_color_space_data_t *) pclient_data;
	if (pclient_data)
	{
		pclient_data->client_is_going_to_handle_color_space = 1;
		return true;
	}
	return false;
}

/*
 * Check if we want to use the PANTONE color processing logic for the given
 * Separation color space.
 */
private bool
client_pantone_install_Separation(client_custom_color_params_t * pparam,
			gs_color_space * pcs, gs_state * pgs)
{
    const gs_separation_name name = pcs->params.separation.sep_name;
    int pan_index;
    byte * pname;
    uint name_size;
    gx_device * dev = pgs->device;
    int num_pantone_colors = count_of(pantone_list);
    bool use_custom_color_callback = false;

    /* Exit if we have already installed this color space. */
    if (pcs->pclient_color_space_data != NULL)
		return true;

    /*
     * Get the character string and length for the component name.
     */
    pcs->params.separation.get_colorname_string(dev->memory, name,
						&pname, &name_size);
    /*
    * Compare the colorant name to those in our PANTONE color list.
    */
    for (pan_index = 0; pan_index < num_pantone_colors ; pan_index++) {
	const char * pan_name = pantone_list[pan_index].name;

	if (compare_names(pname, name_size, pan_name, strlen(pan_name))) {
	    use_custom_color_callback = true;
	    break;
	}
    }

    if (use_custom_color_callback) {
        demo_color_space_data_t * pclient_data = allocate_client_data_block(1);

	if (pclient_data == NULL)
		return false;
	pclient_data->color_index[0] = pan_index;
        pcs->pclient_color_space_data =
	       (client_color_space_data_t *) pclient_data;
    }
    return use_custom_color_callback;
}

/*
 * Check if we want to use the PANTONE color processing logic for the given
 * DeviceN color space.
 */
private bool
client_pantone_install_DeviceN(client_custom_color_params_t * pparam,
			gs_color_space * pcs, gs_state * pgs)
{
    const gs_separation_name *names = pcs->params.device_n.names;
    int num_comp = pcs->params.device_n.num_components;
    int i;
    int pan_index;
    byte * pname;
    uint name_size;
    gx_device * dev = pgs->device;
    int num_pantone_colors = count_of(pantone_list);
    bool pantone_found = false;
    bool other_separation_found = false;
    bool use_pantone;
    const char none_str[] = "None";
    const uint none_size = strlen(none_str);
    const char cyan_str[] = "Cyan";
    const uint cyan_size = strlen(cyan_str);
    const char magenta_str[] = "Magenta";
    const uint magenta_size = strlen(magenta_str);
    const char yellow_str[] = "Yellow";
    const uint yellow_size = strlen(yellow_str);
    const char black_str[] = "Black";
    const uint black_size = strlen(black_str);
    int pantone_color_index[GS_CLIENT_COLOR_MAX_COMPONENTS];

    /* Exit if we have already installed this color space. */
    if (pcs->pclient_color_space_data != NULL)
		return true;

    /*
     * Now check the names of the color components.
     */
    for(i = 0; i < num_comp; i++ ) {
	bool match = false;

	/*
	 * Get the character string and length for the component name.
	 */
	pcs->params.device_n.get_colorname_string(dev->memory, names[i],
							&pname, &name_size);
	/*
         * Postscript does not include /None as a color component but it is
         * allowed in PDF so we accept it.  We simply skip components named
	 * 'None'.
         */
	if (compare_names(none_str, none_size, pname, name_size)) {
	    pantone_color_index[i] = PANTONE_NONE;
	    continue;
	}
	/*
	 * Check if our color space includes the CMYK process colors.
	 */
	if (compare_names(cyan_str, cyan_size, pname, name_size)) {
	    pantone_color_index[i] = PANTONE_CYAN;
	    continue;
	}
	if (compare_names(magenta_str, magenta_size, pname, name_size)) {
	    pantone_color_index[i] = PANTONE_MAGENTA;
	    continue;
	}
	if (compare_names(yellow_str, yellow_size, pname, name_size)) {
	    pantone_color_index[i] = PANTONE_YELLOW;
	    continue;
	}
	if (compare_names(black_str, black_size, pname, name_size)) {
	    pantone_color_index[i] = PANTONE_BLACK;
	    continue;
	}
	/*
	 * Compare the colorant name to those in our Pantone color list.
	 */
	for (pan_index = 0; pan_index < num_pantone_colors ; pan_index++) {
	    const char * pan_name = pantone_list[pan_index].name;

	    if (compare_names(pname, name_size, pan_name, strlen(pan_name))) {
	        pantone_color_index[i] = pan_index;
		match = pantone_found = true;
		break;
	    }
	}
	if (!match) {		/* Exit if we find a non Pantone color */
	    other_separation_found = true;
	    break;
	}
    }
    /*
     * Handle this color space as a 'pantone color space' if we have only
     * PANTONE colors and CMYK.  Any other separations will force us to
     * use the normal Ghostscript processing for a DeviceN color space.
     */
    use_pantone = pantone_found && !other_separation_found;
    if (use_pantone) {
        demo_color_space_data_t * pclient_data = allocate_client_data_block(1);

        if (pclient_data == NULL)
	    return false;
        for(i = 0; i < num_comp; i++ )
	    pclient_data->color_index[i] = pantone_color_index[i];
        pcs->pclient_color_space_data =
	       (client_color_space_data_t *) pclient_data;
    }
    return use_pantone;
}


/*
 * Convert a set of color values in a 'PANTONE color space' into a device
 * color values.
 *
 * This routine creates an equivalent CMYK color and then uses
 * gx_remap_concrete_cmyk to convert this into device colorants.  Note:  It
 * is possible to go directy to the output device colorants.  However the
 * pantone_install_xxx routines should verify that the expected device
 * colorants match the actual device colorants.  (For instance, Ghostscript
 * can install temporary compositing devices for functions like handling
 * PDF 1.4 transparency.  The compositing devices may have a process color
 * models which differ from the final output device.)
 */
private int
client_pantone_remap_color(client_custom_color_params_t * pparam,
	const frac * pconc, const demo_color_space_data_t * pparams,
       	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
	gs_color_select_t select, int num_comp)
{
    int i, pantone_index, cvalue;
    int cyan = 0;
    int magenta = 0;
    int yellow = 0;
    int black = 0;
    frac cc, cm, cy, ck;
    const pantone_list_t * plist;

    /*
     * If the client color space data pointer is NULL then we are not processing
     * this color space.  The rangecheck error will indicate that GS should do
     * its normal color space processing.
     */
    if (pparams == NULL)
	return_error(gs_error_rangecheck);

    /*
     * Create a CMYK representation of the various colors in our color space.
     * Note:  If we have multiple components, then we do a simple sum of the
     * CMYK equivalent for each color.  If desired, a more complex handling is
     * left to the user.
     */
    for (i = 0; i < num_comp; i++) {
	cvalue = pconc[i];
	pantone_index = pparams->color_index[i];
	switch (pantone_index) {
	    case PANTONE_NONE:
		break;
	    case PANTONE_CYAN:
		cyan += cvalue;
		break;
	    case PANTONE_MAGENTA:
		magenta += cvalue;
		break;
	    case PANTONE_YELLOW:
		yellow += cvalue;
		break;
	    case PANTONE_BLACK:
		black += cvalue;
		break;
	    default:
		plist = &(pantone_list[pantone_index]);
		cyan += (int) floor(cvalue * plist->c);
		magenta += (int) floor(cvalue * plist->m);
		yellow += (int) floor(cvalue * plist->y);
		black += (int) floor(cvalue * plist->k);
		break;
	}
    }
    /* Clamp our color values */
    cc = (cyan > frac_1) ? frac_1 : (cyan < frac_0) ? frac_0 : cyan;
    cm = (magenta > frac_1) ? frac_1 : (magenta < frac_0) ? frac_0 : magenta;
    cy = (yellow > frac_1) ? frac_1 : (yellow < frac_0) ? frac_0 : yellow;
    ck = (black > frac_1) ? frac_1 : (black < frac_0) ? frac_0 : black;
    gx_remap_concrete_cmyk(cc, cm, cy, ck, pdc, pis, dev, select);
    return 0;
}

/*
 * Convert a Separation color (with PANTONE colorants) into device color.
 */
private int
client_pantone_remap_Separation(client_custom_color_params_t * pparam,
	const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    return client_pantone_remap_color(pparam, pconc,
	(demo_color_space_data_t *)(pcs->pclient_color_space_data),
       	pdc, pis, dev, select, 1);
}

/*
 * Convert a DeviceN color (with PANTONE colorants) into device color.
 */
private int
client_pantone_remap_DeviceN(client_custom_color_params_t * pparam,
	const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
   	return client_pantone_remap_color(pparam, pconc,
	(demo_color_space_data_t *)(pcs->pclient_color_space_data),
	pdc, pis, dev, select, gs_color_space_num_components(pcs));	
}

#if OBJECT_TYPE_EXAMPLE
/*
 * Install a DeviceGray color space.
 */
private bool
client_install_DeviceGray(client_custom_color_params_t * pparams,
	    gs_color_space * pcs, gs_state * pgs)
{
    /* Nothing to do in our demo */
    return true;
}

/*
 * For demo and debug purposes, make our colors a function of the
 * intensity of the given colors and the object type.
 */
private int
convert_intensity_into_device_color(const frac intensity,
	gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
	gs_color_select_t select)
{
    frac cc, cm, cy, ck;

    switch (pis->object_tag) {
	case GS_TEXT_TAG:		/* Make text red. */
		cc = ck = 0;
		cm = cy = frac_1 - intensity;
		break;
	case GS_IMAGE_TAG:		/* Make images green. */
		cm = ck = 0;
		cc = cy = frac_1 - intensity;
		break;
	case GS_PATH_TAG:		/* Make lines and fills blue. */
	default:
		cy = ck = 0;
		cc = cm = frac_1 - intensity;
		break;
    }

    /* Send CMYK colors to the device */
    gx_remap_concrete_cmyk(cc, cm, cy, ck, pdc, pis, dev, select);
    return 0;
}

/*
 * Convert a DeviceGray color into device color.
 */
private int
client_remap_DeviceGray(client_custom_color_params_t * pparams,
    const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
    const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given colors and the object type.
     */
    frac intensity = pconc[0];

    convert_intensity_into_device_color(intensity, pdc, pis, dev, select);
    return 0;
}

/*
 * Install a DeviceRGB color space.
 */
private bool
client_install_DeviceRGB(client_custom_color_params_t * pparams,
	    gs_color_space * pcs, gs_state * pgs)
{
    /* Nothing to do in our demo */
    dlprintf1("client_install_DeviceRGB ri = %d\n", pgs->renderingintent);
    return true;
}

/*
 * Convert a DeviceRGB color into device color.
 */
private int
client_remap_DeviceRGB(client_custom_color_params_t * pparams,
	const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given colors and the object type.
     */
    frac intensity = (frac)(pconc[0] * 0.30 + pconc[1] * 0.59 + pconc[2] * 0.11);

    convert_intensity_into_device_color(intensity, pdc, pis, dev, select);
    return 0;
}

/*
 * Install a DeviceCMYK color space.
 */
private bool
client_install_DeviceCMYK(client_custom_color_params_t * pparams,
	    gs_color_space * pcs, gs_state * pgs)
{
    /* Nothing to do in our demo */
    return true;
}

/*
 * Convert a DeviceGray color into device color.
 */
private int
client_remap_DeviceCMYK(client_custom_color_params_t * pparams,
	const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given colors and the object type.
     */
    frac intensity = frac_1 - (frac)(pconc[0] * 0.30 + pconc[1] * 0.59
		    + pconc[2] * 0.11 + pconc[3]);

    if (intensity < frac_0)
	intensity = frac_0;
    convert_intensity_into_device_color(intensity, pdc, pis, dev, select);
    return 0;
}

/*
 * Convert a floating point color value into a fixed (frac) color value
 * given a specified floating point range.
 */
#define convert2frac(color, range) \
	(color <= range.rmin) ? frac_0 \
	    : (color >= range.rmax)  ? frac_1 \
		: (frac) (frac_1 * \
			(color - range.rmin) / (range.rmax - range.rmin))

/*
 * Convert a CIEBasedA color into device color.
 */
private int
client_remap_CIEBasedA(client_custom_color_params_t * pparams,
    const gs_client_color * pc, const gs_color_space * pcs,
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    frac gray = convert2frac(pc->paint.values[0], pcs->params.a->RangeA);

    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given color value and the object type.
     */
    return client_remap_DeviceGray(pparams, &gray, pcs, pdc, pis, dev, select);
}

/*
 * Convert a CIEBasedABC color into device color.
 */
private int
client_remap_CIEBasedABC(client_custom_color_params_t * pparams,
    const gs_client_color * pc, const gs_color_space * pcs,
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    frac rgb[3];
    int i;

    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given color value and the object type.  The color
     * values could represent almost anything.  However we are assuming
     * that they are RGB values.
     */
    for (i = 0; i < 3; i++)
	rgb[i] = convert2frac(pc->paint.values[i],
		       	pcs->params.abc->RangeABC.ranges[i]);
    return client_remap_DeviceRGB(pparams, rgb, pcs, pdc, pis, dev, select);
}

/*
 * Convert a CIEBasedDEF color into device color.
 */
private int
client_remap_CIEBasedDEF(client_custom_color_params_t * pparams,
    const gs_client_color * pc, const gs_color_space * pcs,
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    frac rgb[3];
    int i;

    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given color value and the object type.  The color
     * values could represent almost anything.  However we are assuming
     * that they are RGB values.
     */
    for (i = 0; i < 3; i++)
	rgb[i] = convert2frac(pc->paint.values[i],
		       	pcs->params.def->RangeDEF.ranges[i]);
    return client_remap_DeviceRGB(pparams, rgb, pcs, pdc, pis, dev, select);
}

/*
 * Convert a CIEBasedDEFG color into device color.
 */
private int
client_remap_CIEBasedDEFG(client_custom_color_params_t * pparams,
    const gs_client_color * pc, const gs_color_space * pcs,
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    frac cmyk[4];
    int i;

    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given color value and the object type.  The color
     * values could represent almost anything.  However we are assuming
     * that they are CMYK values.
     */
    for (i = 0; i < 4; i++)
	cmyk[i] = convert2frac(pc->paint.values[i],
		       	pcs->params.defg->RangeDEFG.ranges[i]);
    return client_remap_DeviceRGB(pparams, cmyk, pcs, pdc, pis, dev, select);
}

/*
 * Convert a ICCBased color into device color.
 */
private int
client_remap_ICCBased(client_custom_color_params_t * pparams,
    const gs_client_color * pc, const gs_color_space * pcs,
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    frac frac_color[GS_CLIENT_COLOR_MAX_COMPONENTS];
    int i, num_values = pcs->params.icc.picc_info->num_components;

    /*
     * For demo and debug purposes, make our colors a function of the
     * intensity of the given color value and the object type.  The color
     * values could represent almost anything.  However based upon the
     * number of color values, we are assuming that they are either
     * gray, RGB, or CMYK values.
     */
    for (i = 0; i < num_values; i++)
	frac_color[i] = convert2frac(pc->paint.values[i],
		       	pcs->params.icc.picc_info->Range.ranges[i]);
    switch (num_values) {
	case 0:
	case 2:
	    return_error(e_rangecheck);
	case 1:
	    return client_remap_DeviceGray(pparams, frac_color, pcs,
			   		 pdc, pis, dev, select);
	case 3:
	    return client_remap_DeviceRGB(pparams, frac_color, pcs,
			   		 pdc, pis, dev, select);
	case 4:
	default:
	    return client_remap_DeviceCMYK(pparams, frac_color, pcs,
			   		 pdc, pis, dev, select);
    }
}

#undef convert2frac

#endif 		/* OBJECT_TYPE_EXAMPLE */

#if OBJECT_TYPE_EXAMPLE
/*
 * Client call back procedures for our demo which illustrates color
 * processing based upon object type.
 */
client_custom_color_procs_t demo_procs = {
    client_install_DeviceGray,		/* DeviceGray */
    client_remap_DeviceGray,
    client_install_DeviceRGB,		/* DeviceRGB */
    client_remap_DeviceRGB,
    client_install_DeviceCMYK,		/* DeviceCMYK */
    client_remap_DeviceCMYK,
    client_pantone_install_Separation,	/* Separation */
    client_pantone_remap_Separation,
    client_pantone_install_DeviceN,	/* DeviceN */
    client_pantone_remap_DeviceN,
    client_install_generic,		/* CIEBasedA */
    client_remap_CIEBasedA,
    client_install_generic,		/* CIEBasedABC */
    client_remap_CIEBasedABC,
    client_install_generic,		/* CIEBasedDEF */
    client_remap_CIEBasedDEF,
    client_install_generic,		/* CIEBasedDEFG */
    client_remap_CIEBasedDEFG,
    client_install_generic,		/* ICCBased */
    client_remap_ICCBased
};
#else			/* Not OBJECT_TYPE_EXAMPLE special */
/*
 * For PANTONE colors, we only need to handle Separation and DeviceN
 * color spaces.  These are the only color spaces that can have PANTONE
 * colors.
 */
client_custom_color_procs_t demo_procs = {
    client_install_no_op,		/* DeviceGray */
    client_remap_simple_no_op,
    client_install_no_op,		/* DeviceRGB */
    client_remap_simple_no_op,
    client_install_no_op,		/* DeviceCMYK */
    client_remap_simple_no_op,
    client_pantone_install_Separation,	/* Separation */
    client_pantone_remap_Separation,
    client_pantone_install_DeviceN,	/* DeviceN */
    client_pantone_remap_DeviceN,
    client_install_no_op,		/* CIEBasedA */
    client_remap_complex_no_op,
    client_install_no_op,		/* CIEBasedABC */
    client_remap_complex_no_op,
    client_install_no_op,		/* CIEBasedDEF */
    client_remap_complex_no_op,
    client_install_no_op,		/* CIEBasedDEFG */
    client_remap_complex_no_op,
    client_install_no_op,		/* ICCBased */
    client_remap_complex_no_op
};
#endif		/* OBJECT_TYPE_EXAMPLE */

#endif		/* ENABLE_CUSTOM_COLOR_CALLBACK */
