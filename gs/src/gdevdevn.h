/* Copyright (C) 2003 Artifex Software Inc.  All rights reserved.
  
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
/* Include file for common DeviceN process color model devices. */

#ifndef gdevdevn_INCLUDED
# define gdevdevn_INCLUDED

/* The device procedures descriptors */
dev_proc_get_params(devicen_get_params);

/*
 * Type definitions associated with the fixed color model names.
 */
typedef const char * fixed_colorant_name;
typedef fixed_colorant_name fixed_colorant_names_list[];

/*
 * Define the maximum number of separations supported by this device.
 * This value is arbitrary.  It is set simply to define a limit on
 * on the separation_name_array and map.
 */
#define GX_DEVICE_MAX_SEPARATIONS 16

/*
 * Structure for holding Separation information.
 */
typedef struct gs_separation_info_s {
    bool has_cmyk_color;
    const gs_param_string * name;
} gs_separation_info;

/*
 * Structure for holding SeparationNames elements.
 */
typedef struct gs_separations_s {
    int num_separations;
    bool have_all_cmyk_colors;
    gs_separation_info info[GX_DEVICE_MAX_SEPARATIONS];
} gs_separations;

/*
 * Structure for holding SeparationOrder elements.
 */
typedef struct gs_separation_order_s {
    int num_names;
    const gs_param_string * names[GX_DEVICE_COLOR_MAX_COMPONENTS];
} gs_separation_order;

/*
 * Type for holding a separation order map
 */
typedef int gs_separation_map[GX_DEVICE_MAX_SEPARATIONS];

typedef struct gs_devn_params_s {
    /*
     * Bits per component (device colorant).  Currently only 1 and 8 are
     * supported.
     */
    int bitspercomponent;

    /*
     * Pointer to the colorant names for the color model.  This will be
     * null if we have DeviceN type device.  The actual possible colorant
     * names are those in this list plus those in the separation[i].info.name
     * list (below).
     */
    fixed_colorant_names_list * std_colorant_names;
    int num_std_colorant_names;	/* Number of names in list */

    /*
    * Separation info (if any).
    */
    gs_separations separations;

    /*
     * Separation Order (if specified).
     */
    gs_separation_order separation_order;
    /*
     * The SeparationOrder parameter may change the logical order of
     * components.
     */
    gs_separation_map separation_order_map;
} gs_devn_params_t;

typedef gs_devn_params_t gs_devn_params;

extern const fixed_colorant_names_list DeviceCMYKComponents;

/*
 * Put the DeviceN related parameters.
 *
 * Note that this routine requires a pointer to the DeviceN parametes within
 * the device structure.
 *
 * Note:  See the devicen_put_params_no_sep_order routine (next) for comments
 * about the SeparationOrder parameter.
 */
int devicen_put_params(gx_device * pdev, gs_devn_params * pparams,
		gs_param_list * plist);

/*
 * Put the DeviceN related parameters.  This routine does not handle the
 * SeparationOrder parameter.  Some high level devices do not want the
 * SeparationOrder parameter to processed.  (The use of the SeparationOrder
 * parameter could result in only some of the separations being present in
 * the device's output.  This is not desired for a device like pdfwrite
 * which uses the process color model colorants for a backup case to
 * handle colors.  Missing separations would result in missing color values.)
 *
 * Note that this routine requires a pointer to the DeviceN parametes within
 * the device structure.
 */
int devicen_put_params_no_sep_order(gx_device * pdev, gs_devn_params * pparams,
		gs_param_list * plist);

/*
 * This routine will check to see if the color component name  match those
 * of either the process color model colorants or the names on the
 * SeparationColorNames list.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns a negative value if not found.
 */
int check_pcm_and_separation_names(const gx_device * dev,
		const gs_devn_params * pparams, const char * pname,
		int name_size, int component_type);

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color components.  
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns a negative value if not found.
 */
int devicen_get_color_comp_index(const gx_device * dev, gs_devn_params * pparams,
		const char * pname, int name_size, int component_type);

/*
 * This routine will extract a specified set of bits from a buffer and pack
 * them into a given buffer.
 *
 * Parameters:
 *   source - The source of the data
 *   dest - The destination for the data
 *   depth - The size of the bits per pixel - must be a multiple of 8
 *   first_bit - The location of the first data bit (LSB).
 *   bit_width - The number of bits to be extracted.
 *   npixel - The number of pixels.
 *
 * Returns:
 *   Length of the output line (in bytes)
 *   Data in dest.
 */
int repack_data(byte * source, byte * dest, int depth, int first_bit,
		int bit_width, int npixel);

/*
 * This utility routine calculates the number of bits required to store
 * color information.  In general the values are rounded up to an even
 * byte boundary except those cases in which mulitple pixels can evenly
 * into a single byte.
 *
 * The parameter are:
 *   ncomp - The number of components (colorants) for the device.  Valid
 * 	values are 1 to GX_DEVICE_COLOR_MAX_COMPONENTS
 *   bpc - The number of bits per component.  Valid values are 1, 2, 4, 5,
 *	and 8.
 * Input values are not tested for validity.
 */
int bpc_to_depth(int ncomp, int bpc);

#endif		/* ifndef gdevdevn_INCLUDED */
