/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

/* Include file for common DeviceN process color model devices. */

#ifndef gdevdevn_INCLUDED
# define gdevdevn_INCLUDED

#include "gxblend.h"
#include "gsequivc.h"

/*
 * Type definitions associated with the fixed color model names.
 */
typedef const char * fixed_colorant_name;
typedef fixed_colorant_name * fixed_colorant_names_list;

/*
 * Structure for holding SeparationNames elements.
 */
typedef struct devn_separation_name_s {
    int size;
    byte * data;
} devn_separation_name;

/*
 * Structure for holding SeparationNames elements.
 */
struct gs_separations_s {
    int num_separations;
    devn_separation_name names[GX_DEVICE_MAX_SEPARATIONS];
};

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
     * names are those in this list plus those in the separation[i].name
     * list (below).
     */
    fixed_colorant_names_list std_colorant_names;
    int num_std_colorant_names;	/* Number of names in list */
    int max_separations;	/* From MaxSeparation parameter */
    /*
     * This value comes from scanning color space resources in PDF files.
     * Thus this value is only valid for PDF files.  The value may also
     * be high if there are color space resources that are defined for
     * a page but which are not actually used.  This value does give us
     * a maximum value for the number of spot colors.
     * From the PageSpotColors parameter.
     */
    int page_spot_colors;

    /*
    * Separation info (if any).
    */
    gs_separations separations;

    /*
     * Separation Order (if specified).
     */
    int num_separation_order_names;
    /*
     * The SeparationOrder parameter may change the logical order of
     * components.
     */
    gs_separation_map separation_order_map;
    /*
     * Number of reserved components (such as for tags). This is used
     * to prevent our auto_spot_colors code to expand into components
     * that it should not.
     */
    int num_reserved_components;
    /*
     * Pointer to our list of which colorant combinations are being used.
     */
    gs_separations pdf14_separations;
} gs_devn_params_t;

extern fixed_colorant_name DeviceGrayComponents[];
extern fixed_colorant_name DeviceRGBComponents[];
extern fixed_colorant_name DeviceCMYKComponents[];

/*
 * Utility routines for common DeviceN related parameters:
 *   SeparationColorNames, SeparationOrder, and MaxSeparations
 */

/*
 * Convert standard color spaces into DeviceN colorants.
 * Note;  This routine require SeparationOrder map.
 */
void gray_cs_to_devn_cm(const gx_device * dev, int * map, frac gray, frac out[]);

void rgb_cs_to_devn_cm(const gx_device * dev, int * map,
                const gs_gstate *pgs, frac r, frac g, frac b, frac out[]);

void cmyk_cs_to_devn_cm(const gx_device * dev, const int * map,
                frac c, frac m, frac y, frac k, frac out[]);

/*
 * Possible values for the 'auto_spot_colors' parameter.
 */
/*
 * Do not automatically include spot colors
 */
#define NO_AUTO_SPOT_COLORS 0
/*
 * Automatically add spot colors up to the number that the device can image.
 * Spot colors over that limit will be handled by the alternate color space
 * for the Separation or DeviceN color space.
 */
#define ENABLE_AUTO_SPOT_COLORS	1
/*
 * Automatically add spot colors up to the GX_DEVICE_MAX_SEPARATIONS value.
 * Note;  Spot colors beyond the number that the device can image will be
 * ignored (i.e. treated like a colorant that is not specified by the
 * SeparationOrder device parameter.
 */
#define ALLOW_EXTRA_SPOT_COLORS 2

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color colorants.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *   component_type - separation name or not
 *   pdevn_params - pointer to device's DeviceN paramters
 *   pequiv_colors - pointer to equivalent color structure (may be NULL)
 *   auto_spot_colors - See comments above.
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the color component is found but is not being used due to the
 * SeparationOrder parameter.  It returns a negative value if not found.
 *
 * This routine will also add separations to the device if space is
 * available.
 */
int devn_get_color_comp_index(gx_device * dev,
    gs_devn_params * pdevn_params, equivalent_cmyk_color_params * pequiv_colors,
    const char * pname, int name_size, int component_type,
    int auto_spot_colors);

/* Utility routine for getting DeviceN parameters */
int devn_get_params(gx_device * pdev, gs_param_list * plist,
                    gs_devn_params * pdevn_params,
                    equivalent_cmyk_color_params * pequiv_colors);

/*
 * Utility routine for handling DeviceN related parameters.  This routine
 * assumes that the device is based upon a standard printer type device.
 * (See the next routine if not.)
 *
 * Note that this routine requires a pointer to the DeviceN parameters within
 * the device structure.  The pointer to the equivalent_cmyk_color_params is
 * optional (it should be NULL if this feature is not used by the device).
 */
int devn_printer_put_params(gx_device * pdev, gs_param_list * plist,
                        gs_devn_params * pdevn_params,
                        equivalent_cmyk_color_params * pequiv_colors);

int
devn_generic_put_params(gx_device *pdev, gs_param_list *plist,
                        gs_devn_params *pdevn_params, equivalent_cmyk_color_params *pequiv_colors,
                        int is_printer);


/*
 * Utility routine for handling DeviceN related parameters.  This routine
 * may modify the color_info, devn_params, and the * equiv_colors fields.
 * The pointer to the equivalent_cmyk_color_params is optional (it should be
 * NULL if this feature is not used by the device).
 *
 * Note:  This routine does not restore values in case of a problem.  This
 * is left to the caller.
 */
int devn_put_params(gx_device * pdev, gs_param_list * plist,
                        gs_devn_params * pdevn_params,
                        equivalent_cmyk_color_params * pequiv_colors);

/*
* This routine will check to see if the color component name match those
* of the SeparationColorNames list.  Needed for case where we have RGB
* blending color spaces in transparency and need to maintain the spot colorants
*
* Parameters:
*   dev - pointer to device data structure.
*   pname - pointer to name (zero termination not required)
*   nlength - length of the name
*   index into colorants offset of device
*
* This routine returns a positive value (0 to n) which is the device colorant
* number if the name is found.  It returns a negative value if not found.
*/
int check_separation_names(const gx_device * dev, const gs_devn_params * pparams,
    const char * pname, int name_size, int component_type, int number);


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
 * This routine copies over the gs_devn_params from one device to another.
   This is needed when we launch multi-threaded rendering for separation
   devices
 *
 * Parameters :
 *      psrcdev - pointer to source device.
 *      pdesdev - pointer to destination device.
 *
 * Returns 0 if all allocations were fine.
 */
int devn_copy_params(gx_device * psrcdev, gx_device * pdesdev);

/*
 * This routine frees the gs_devn_params objects
 *
 * Parameters :
 *      dev - pointer to device.
 *
 */
void devn_free_params(gx_device *dev);

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
int bpc_to_depth(uchar ncomp, int bpc);

/*
 * We are using bytes to pack both which colorants are non zero and the values
 * of the colorants.  Thus do not change this value without recoding the
 * methods used for encoding our colorants.  (This definition is really here
 * to mark locations in the code that are extremely specific to using a byte
 * oriented approach to the encoding.)
 */
#define NUM_ENCODE_LIST_ITEMS 256
#define STD_ENCODED_VALUE 256

/*
 * Since we are byte packing things in a gx_color_index.  We need the number
 * of bytes in a gx_color_index.
 */
#define NUM_GX_COLOR_INDEX_BYTES ARCH_SIZEOF_GX_COLOR_INDEX
#define NUM_GX_COLOR_INDEX_BITS (8 * NUM_GX_COLOR_INDEX_BYTES)

/*
 * Define the highest level in our encoded color colorant list.
 * Since we need at least one byte to store our encoded list of colorants
 * and we are packing colorant values in bytes, the top level of our encoded
 * color colorants that is the size of gx_clor_index - 1.
 */
#define TOP_ENCODED_LEVEL (NUM_GX_COLOR_INDEX_BYTES - 1)

/*
 * The maximum number of colorants that we can encode.
 */
#define MAX_ENCODED_COMPONENTS 14

/*
 * To prevent having a bunch of one or two colorant elements, we set a
 * cutoff for the minimum number of colorants.  If we have less than the
 * cutoff then we add in our process colors on the assumption that it is
 * likely that sometime we will want a combination of the process and spot
 * colors.
 */
#define MIN_ENCODED_COMPONENTS 5

/*
 * Define a value to represent a color value that we cannot encode.  This can
 * occur if either we exceed MAX_ENCODED_COMPONENTS or all of the possible
 * levels of the encoded colorant list are full.
 */
#define NON_ENCODEABLE_COLOR (gx_no_color_index - 1)

/*
 * We keep a bit map of the colorants.  If a bit map will fit into a
 * gx_color_index sized intger then we use one, otherwize we use an array
 * of ints.
 */
#if GX_DEVICE_COLOR_MAX_COMPONENTS <= (ARCH_SIZEOF_GX_COLOR_INDEX * 8)
typedef gx_color_index comp_bit_map_t;
#define set_colorant_present(pbit_map, comp_list, comp_num)\
   (pbit_map)->comp_list |= (((gx_color_index)1) << comp_num)
#define clear_colorant_present(pbit_map, comp_list, comp_num)\
   (pbit_map)->comp_list &= ~(((gx_color_index)1) << comp_num)
#define colorant_present(pbit_map, comp_list, comp_num)\
   ((int)(((pbit_map)->comp_list >> comp_num)) & 1)
/*
 * The compare bit map soutine (for the array case) is too complex for a simple
 * macro.  So it is comditionally compiled using this switch.
 */
#define DEVN_ENCODE_COLOR_USING_BIT_MAP_ARRAY 0

#else
/*
 * If we are trying to handle more colorants than will fit into a gx_color_index,
 * then we bit pack them into uints.  So we need to define some values for
 * defining the number of elements that we need, etc.
 */
#define comp_bit_map_elem_t uint
#define BITS_PER_COMP_BIT_MAP_ELEM (size_of(comp_bit_map_elem_t) * 8)
#define COMP_BIT_MAP_ELEM_MASK (BITS_PER_COMP_BIT_MAP_ELEM - 1)

#define COMP_BIT_MAP_SIZE \
    ((GX_DEVICE_COLOR_MAX_COMPONENTS + COMP_BIT_MAP_ELEM_MASK) / \
                                                BITS_PER_COMP_BIT_MAP_ELEM)

/* Bit map list of colorants in the gx_color_index value */
typedef comp_bit_map_elem_t comp_bit_map_t[COMP_BIT_MAP_SIZE];
#define set_colorant_present(pbit_map, comp_list, comp_num)\
   (pbit_map)->comp_list[comp_num / BITS_PER_COMP_BIT_MAP_ELEM] |=\
                                (1 << (comp_num & COMP_BIT_MAP_ELEM_MASK))
#define clear_colorant_present(pbit_map, comp_list, comp_num)\
   (pbit_map)->comp_list[comp_num / BITS_PER_COMP_BIT_MAP_ELEM] &=\
                                ~(1 << (comp_num & COMP_BIT_MAP_ELEM_MASK))
#define colorant_present(pbit_map, comp_list, comp_num)\
   ((pbit_map)->comp_list[comp_num / BITS_PER_COMP_BIT_MAP_ELEM] >>\
                                ((comp_num & COMP_BIT_MAP_ELEM_MASK)) & 1)
/*
 * The compare bit map soutine is too complex for s simple macro.
 * So it is comditionally compiled using this switch.
 */
#define DEVN_ENCODE_COLOR_USING_BIT_MAP_ARRAY 1
#endif

/*
* Element for a map to convert colorants to a CMYK color.
*/
typedef struct cmyk_composite_map_s {
    frac c, m, y, k;
} cmyk_composite_map;

/*
 * The colorant bit map list struct.
 */
typedef struct comp_bit_map_list_s {
    short num_comp;
    short num_non_solid_comp;
    bool solid_not_100;		/* 'solid' colorants are not 1005 solid */
    comp_bit_map_t colorants;
    comp_bit_map_t solid_colorants;
} comp_bit_map_list_t;

/*
 * Unpack a row of 'encoded color' values.  These values are encoded as
 * described for the devn_encode_color routine.
 *
 * The routine takes a raster line of data and expands each pixel into a buffer
 * of 8 bit values for each colorant.
 *
 * See comments preceding devn_encode_color in gdevdevn.c for more information
 * about how we encode the color information into a gx_color_index value.
 */
int devn_unpack_row(gx_device * dev, int num_comp, gs_devn_params * pdevn_params,
                                         int width, byte * in, byte * out);

/*
 * The elements of this array contain the number of bits used to encode a color
 * value in a 'compressed' gx_color_index value.  The index into the array is
 * the number of compressed components.
 */
extern int num_comp_bits[];

/*
 * The elements of this array contain factors used to convert compressed color
 * values to gx_color_values.  The index into the array is the number of
 * compressed components.
 */
extern int comp_bit_factor[];

/*
 * Free a set of separation names
 */
void free_separation_names(gs_memory_t *mem, gs_separations * pseparation);

/* Build a map for use by devices that are building composite views */
void build_cmyk_map(gx_device *pdev, int num_comp,
        equivalent_cmyk_color_params *equiv_cmyk_colors,
        cmyk_composite_map * cmyk_map);

#endif		/* ifndef gdevdevn_INCLUDED */
