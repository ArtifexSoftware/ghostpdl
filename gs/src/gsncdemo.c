/*
  Copyright (C) 2005 artofcode LLC, Artofex Software Inc.
  
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
/* $Id$ */
/* Sample implementation for handling PANTONE Colors */

/*
 * PANTONE is a registered trademark and PANTONE colors are a
 * licensed product of Pantone Inc. See http://www.pantone.com
 * for more information.
 */

/*
 * This module has been created to demonstrate how to support the use of
 * PANTONE colors to the Ghostscript graphics library.  PANTONE colors
 * are specified in both PostScript and PDF files via the use of DeviceN
 * or Separation color spaces.
 *
 * This implementation consists of four routines.  There are a pair of
 * routines for both Separation and DeviceN color spaces.  Each pair consists
 * of a routine that is called when the color space is installed and a
 * second routine that is called to transform colors in that color space
 * into device colorant values.  The routines client_pantone_install_Separation
 * and client_pantone_install_DeviceN are called when Separation or DeviceN
 * color space is installed.  These routines determine if Pantone colors are
 * used inside the color space.  These routines return true if colors in the
 * color space should be processed via the second pair of routines.
 * The routines client_pantone_remap_Separation and client_pantone_remap_DeviceN
 * are then called whenever a color is specified (and the 'install' routines
 * returned true).  This second set of routines act as a replacement for the
 * alternate tine transform functions.
 *
 * To use this code:
 * 1.  Set ENABLE_NAMED_COLOR_CALLBACK in gsnamecl.h to 1
 * 2.  Provide your implementation of the Pantone callback routines.  See
 *     the client_pantone_install_Separation, client_pantone_remap_Separation,
 *     client_pantone_install_DeviceN, and client_pantone_remap_DeviceN for
 *     an example of these routines.    
 * 3.  Define a Pantone callback parameter structure (see the 'demo_procs'
 *     example structure below) and pass it as a parameter to Ghostscript.
 *     This can be done on the command line by use of -sPantoneCallBack=16#xxxx
 *     where xxxx is the address of the call parameter structure specified as
 *     a hex ascii string.
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
 */

#include "stdpre.h"
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscdefs.h"
#include "gscspace.h"
#include "gxdevice.h"
#include "gzstate.h"

#if ENABLE_NAMED_COLOR_CALLBACK		/* Defined in src/gsnamecl.h */

/*
 * This s a list of PANTONE color names and a set of equivalent CMYK values,
 */
typedef struct pantone_list_s {
    const char *name;		/* Name of the PANTONE color */
    double c, m, y, k;		/* Equivalent CMYK values */
} pantone_list_t;

/*
 * Since this is only a 'demo' list, the list does have not entries for all
 * of the different PANTONE colors.  Creation of a real list is left as an
 * exercise for the user.
 */
private const pantone_list_t pantone_list[] = { 
    { "PantoneCyan",	1, 0, 0, 0 },
    { "PantoneMagenta",	0, 1, 0, 0 },
    { "PantoneYellow",	0, 0, 1, 0 },
    { "PantoneBlack",	0, 0, 0, 1 },
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
 * Check if we want to use the PANTONE color processing logic for the given
 * Separation color space.
 */
private bool
client_pantone_install_Separation(client_named_color_params_t * pparam,
			gs_color_space * pcs, gs_state * pgs)
{
    const gs_separation_name name = pcs->params.separation.sep_name;
    int pan_index;
    byte * pname;
    uint name_size;
    gx_device * dev = pgs->device;
    int num_pantone_colors = count_of(pantone_list);
    bool use_named_color_callback = false;

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
	    pcs->params.separation.named_color_params.color_index[0] = pan_index;
	    use_named_color_callback = true;
	    break;
	}
    }
    pcs->params.device_n.named_color_params.use_named_color_callback =
	    					use_named_color_callback;
    return (use_named_color_callback);
}

/*
 * Check if we want to use the PANTONE color processing logic for the given
 * DeviceN color space.
 */
private bool
client_pantone_install_DeviceN(client_named_color_params_t * pparam,
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
	    pcs->params.device_n.named_color_params.color_index[i] =
		   					 PANTONE_NONE;
	    continue;
	}
	/*
	 * Check if our color space includes the CMYK process colors.
	 */
	if (compare_names(cyan_str, cyan_size, pname, name_size)) {
	    pcs->params.device_n.named_color_params.color_index[i] =
		   					 PANTONE_CYAN;
	    continue;
	}
	if (compare_names(magenta_str, magenta_size, pname, name_size)) {
	    pcs->params.device_n.named_color_params.color_index[i] =
		   					 PANTONE_MAGENTA;
	    continue;
	}
	if (compare_names(yellow_str, yellow_size, pname, name_size)) {
	    pcs->params.device_n.named_color_params.color_index[i] =
		   					 PANTONE_YELLOW;
	    continue;
	}
	if (compare_names(black_str, black_size, pname, name_size)) {
	    pcs->params.device_n.named_color_params.color_index[i] =
		   					 PANTONE_BLACK;
	    continue;
	}
	/*
	 * Compare the colorant name to those in our Pantone color list.
	 */
	for (pan_index = 0; pan_index < num_pantone_colors ; pan_index++) {
	    const char * pan_name = pantone_list[pan_index].name;

	    if (compare_names(pname, name_size, pan_name, strlen(pan_name))) {
		pcs->params.device_n.named_color_params.color_index[i] =
		       			pan_index;
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
     * use the alternate tint transform for the DeviceN color space.
     */
    use_pantone = pantone_found && !other_separation_found;
    pcs->params.device_n.named_color_params.use_named_color_callback =
	   						 use_pantone;
    return (use_pantone);
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
client_pantone_remap_color(client_named_color_params_t * pparam,
	const frac * pconc, const named_color_params_t * pparams,
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
client_pantone_remap_Separation(client_named_color_params_t * pparam,
	const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    return client_pantone_remap_color(pparam, pconc,
	&(pcs->params.separation.named_color_params), pdc, pis, dev, select, 1);
}

/*
 * Convert a DeviceN color (with PANTONE colorants) into device color.
 */
private int
client_pantone_remap_DeviceN(client_named_color_params_t * pparam,
	const frac * pconc, const gs_color_space * pcs, gx_device_color * pdc,
       	const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    return client_pantone_remap_color(pparam, pconc,
	&(pcs->params.device_n.named_color_params), pdc, pis, dev, select,
	gs_color_space_num_components(pcs));
}

/*
 * Client call back procedures for our PANTONE demo.
 */
client_named_color_procs_t demo_procs = {
    client_pantone_install_Separation,
    client_pantone_remap_Separation,
    client_pantone_install_DeviceN,
    client_pantone_remap_DeviceN,
};

/*
 * Demo version of the PANTONE call back parameter structure.
 */
client_named_color_params_t demo_callback = {
    &demo_procs,
    /*
     * Use our 'list' of Pantone colors as an example data.
     */
    (void *)(&pantone_list)
};
#endif 			/* ENABLE_NAMED_COLOR_CALLBACK */

/*
 * This procedure is here to simplify debugging.  Normally one would expect the
 * PANTONE callback structure to be set up by a calling application.  Since I
 * do not have a calling application, I need a simple way to setup the callback
 * parameter.  The callback parameter is passed as a string value.  This
 * routine puts the address of our demo callback structure into the provided
 * string.
 *
 * This routine allows the PANTONE logic to be enabled by adding the following
 * to the command line:
 *   -c "<< /NamedColorCallBack 32 string .pantonecallback >> setpagedevice" -f
 */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "iddict.h"
#include "store.h"

/* <string> .pantonecallback <string> */
private int
zpantonecallback(i_ctx_t *i_ctx_p)
{
#if ENABLE_NAMED_COLOR_CALLBACK
    os_ptr op = osp;
    int val, idx, buf_pos = 3;
    size_t iptr;
#define PTR_STRING_SIZE (2 * size_of(void *) + 3)

    /* Verify that the string size is big enough for our output */
    check_type(*op, t_string);
    check_write(*op);
    if (r_size(op) < PTR_STRING_SIZE)
	return_error(e_rangecheck);

    /* Convert our call back parameter structure pointer into a string */
    op->value.bytes[0] = '1';
    op->value.bytes[1] = '6';
    op->value.bytes[2] = '#';
    iptr = (size_t)(&demo_callback);
    for (idx = ((int)size_of(size_t)) * 8 - 4; idx >= 0; idx -= 4) {
	val = (int)(iptr >> idx) & 0xf;
	op->value.bytes[buf_pos++] = (byte)((val <= 9) ? '0' + val
						       : 'a' - 10 + val);
    }
    r_size(op) = PTR_STRING_SIZE;
#endif 			/* ENABLE_NAMED_COLOR_CALLBACK */
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def pantone_op_defs[] =
{
    {"1.pantonecallback", zpantonecallback},
    op_def_end(0)
};

