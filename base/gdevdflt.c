/* Copyright (C) 2001-2024 Artifex Software, Inc.
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

/* Default device implementation */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gsstruct.h"
#include "gxobj.h"
#include "gserrors.h"
#include "gsropt.h"
#include "gxcomp.h"
#include "gxdevice.h"
#include "gxdevsop.h"
#include "gdevp14.h"        /* Needed to patch up the procs after compositor creation */
#include "gstrans.h"        /* For gs_pdf14trans_t */
#include "gxgstate.h"       /* for gs_image_state_s */


/* defined in gsdpram.c */
int gx_default_get_param(gx_device *dev, char *Param, void *list);

/* ---------------- Default device procedures ---------------- */

/*
 * Set a color model polarity to be additive or subtractive. In either
 * case, indicate an error (and don't modify the polarity) if the current
 * setting differs from the desired and is not GX_CINFO_POLARITY_UNKNOWN.
 */
static void
set_cinfo_polarity(gx_device * dev, gx_color_polarity_t new_polarity)
{
#ifdef DEBUG
    /* sanity check */
    if (new_polarity == GX_CINFO_POLARITY_UNKNOWN) {
        dmprintf(dev->memory, "set_cinfo_polarity: illegal operand\n");
        return;
    }
#endif
    /*
     * The meory devices assume that single color devices are gray.
     * This may not be true if SeparationOrder is specified.  Thus only
     * change the value if the current value is unknown.
     */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_UNKNOWN)
        dev->color_info.polarity = new_polarity;
}

static gx_color_index
(*get_encode_color(gx_device *dev))(gx_device *, const gx_color_value *)
{
    dev_proc_encode_color(*encode_proc);

    /* use encode_color if it has been provided */
    if ((encode_proc = dev_proc(dev, encode_color)) == 0) {
        if (dev->color_info.num_components == 1                          &&
            dev_proc(dev, map_rgb_color) != 0) {
            set_cinfo_polarity(dev, GX_CINFO_POLARITY_ADDITIVE);
            encode_proc = gx_backwards_compatible_gray_encode;
        } else  if ( (dev->color_info.num_components == 3    )           &&
             (encode_proc = dev_proc(dev, map_rgb_color)) != 0  )
            set_cinfo_polarity(dev, GX_CINFO_POLARITY_ADDITIVE);
        else if ( dev->color_info.num_components == 4                    &&
                 (encode_proc = dev_proc(dev, map_cmyk_color)) != 0   )
            set_cinfo_polarity(dev, GX_CINFO_POLARITY_SUBTRACTIVE);
    }

    /*
     * If no encode_color procedure at this point, the color model had
     * better be monochrome (though not necessarily bi-level). In this
     * case, it is assumed to be additive, as that is consistent with
     * the pre-DeviceN code.
     *
     * If this is not the case, then the color model had better be known
     * to be separable and linear, for there is no other way to derive
     * an encoding. This is the case even for weakly linear and separable
     * color models with a known polarity.
     */
    if (encode_proc == 0) {
        if (dev->color_info.num_components == 1 && dev->color_info.depth != 0) {
            set_cinfo_polarity(dev, GX_CINFO_POLARITY_ADDITIVE);
            if (dev->color_info.max_gray == (1 << dev->color_info.depth) - 1)
                encode_proc = gx_default_gray_fast_encode;
            else
                encode_proc = gx_default_gray_encode;
            dev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
        } else if (colors_are_separable_and_linear(&dev->color_info)) {
            gx_color_value  max_gray = dev->color_info.max_gray;
            gx_color_value  max_color = dev->color_info.max_color;

            if ( (max_gray & (max_gray + 1)) == 0  &&
                 (max_color & (max_color + 1)) == 0  )
                /* NB should be gx_default_fast_encode_color */
                encode_proc = gx_default_encode_color;
            else
                encode_proc = gx_default_encode_color;
        }
    }

    return encode_proc;
}

/*
 * Determine if a color model has the properties of a DeviceRGB
 * color model. This procedure is, in all likelihood, high-grade
 * overkill, but since this is not a performance sensitive area
 * no harm is done.
 *
 * Since there is little benefit to checking the values 0, 1, or
 * 1/2, we use the values 1/4, 1/3, and 3/4 in their place. We
 * compare the results to see if the intensities match to within
 * a tolerance of .01, which is arbitrarily selected.
 */

static bool
is_like_DeviceRGB(gx_device * dev)
{
    frac                            cm_comp_fracs[3];
    int                             i;
    const gx_device                *cmdev;
    const gx_cm_color_map_procs    *cmprocs;

    if ( dev->color_info.num_components != 3                   ||
         dev->color_info.polarity != GX_CINFO_POLARITY_ADDITIVE  )
        return false;

    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);

    /* check the values 1/4, 1/3, and 3/4 */
    cmprocs->map_rgb(cmdev, 0, frac_1 / 4, frac_1 / 3, 3 * frac_1 / 4, cm_comp_fracs);

    /* verify results to .01 */
    cm_comp_fracs[0] -= frac_1 / 4;
    cm_comp_fracs[1] -= frac_1 / 3;
    cm_comp_fracs[2] -= 3 * frac_1 / 4;
    for ( i = 0;
           i < 3                            &&
           -frac_1 / 100 < cm_comp_fracs[i] &&
           cm_comp_fracs[i] < frac_1 / 100;
          i++ )
        ;
    return i == 3;
}

/*
 * Similar to is_like_DeviceRGB, but for DeviceCMYK.
 */
static bool
is_like_DeviceCMYK(gx_device * dev)
{
    frac                            cm_comp_fracs[4];
    int                             i;
    const gx_device                *cmdev;
    const gx_cm_color_map_procs    *cmprocs;

    if ( dev->color_info.num_components != 4                      ||
         dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE  )
        return false;

    cmprocs = dev_proc(dev, get_color_mapping_procs)(dev, &cmdev);
    /* check the values 1/4, 1/3, 3/4, and 1/8 */

    cmprocs->map_cmyk(cmdev,
                      frac_1 / 4,
                      frac_1 / 3,
                      3 * frac_1 / 4,
                      frac_1 / 8,
                      cm_comp_fracs);

    /* verify results to .01 */
    cm_comp_fracs[0] -= frac_1 / 4;
    cm_comp_fracs[1] -= frac_1 / 3;
    cm_comp_fracs[2] -= 3 * frac_1 / 4;
    cm_comp_fracs[3] -= frac_1 / 8;
    for ( i = 0;
           i < 4                            &&
           -frac_1 / 100 < cm_comp_fracs[i] &&
           cm_comp_fracs[i] < frac_1 / 100;
          i++ )
        ;
    return i == 4;
}

/*
 * Two default decode_color procedures to use for monochrome devices.
 * These will make use of the map_color_rgb routine, and use the first
 * component of the returned value or its inverse.
 */
static int
gx_default_1_add_decode_color(
    gx_device *     dev,
    gx_color_index  color,
    gx_color_value  cv[1] )
{
    gx_color_value  rgb[3];
    int             code = dev_proc(dev, map_color_rgb)(dev, color, rgb);

    cv[0] = rgb[0];
    return code;
}

static int
gx_default_1_sub_decode_color(
    gx_device *     dev,
    gx_color_index  color,
    gx_color_value  cv[1] )
{
    gx_color_value  rgb[3];
    int             code = dev_proc(dev, map_color_rgb)(dev, color, rgb);

    cv[0] = gx_max_color_value - rgb[0];
    return code;
}

/*
 * A default decode_color procedure for DeviceCMYK color models.
 *
 * There is no generally accurate way of decode a DeviceCMYK color using
 * the map_color_rgb method. Unfortunately, there are many older devices
 * employ the DeviceCMYK color model but don't provide a decode_color
 * method. The code below works on the assumption of full undercolor
 * removal and black generation. This may not be accurate, but is the
 * best that can be done in the general case without other information.
 */
static int
gx_default_cmyk_decode_color(
    gx_device *     dev,
    gx_color_index  color,
    gx_color_value  cv[4] )
{
    /* The device may have been determined to be 'separable'. */
    if (colors_are_separable_and_linear(&dev->color_info))
        return gx_default_decode_color(dev, color, cv);
    else {
        int i, code = dev_proc(dev, map_color_rgb)(dev, color, cv);
        gx_color_value min_val = gx_max_color_value;

        for (i = 0; i < 3; i++) {
            if ((cv[i] = gx_max_color_value - cv[i]) < min_val)
                min_val = cv[i];
        }
        for (i = 0; i < 3; i++)
            cv[i] -= min_val;
        cv[3] = min_val;

        return code;
    }
}

/*
 * Special case default color decode routine for a canonical 1-bit per
 * component DeviceCMYK color model.
 */
static int
gx_1bit_cmyk_decode_color(
    gx_device *     dev,
    gx_color_index  color,
    gx_color_value  cv[4] )
{
    cv[0] = ((color & 0x8) != 0 ? gx_max_color_value : 0);
    cv[1] = ((color & 0x4) != 0 ? gx_max_color_value : 0);
    cv[2] = ((color & 0x2) != 0 ? gx_max_color_value : 0);
    cv[3] = ((color & 0x1) != 0 ? gx_max_color_value : 0);
    return 0;
}

static int
(*get_decode_color(gx_device * dev))(gx_device *, gx_color_index, gx_color_value *)
{
    /* if a method has already been provided, use it */
    if (dev_proc(dev, decode_color) != 0)
        return dev_proc(dev, decode_color);

    /*
     * If a map_color_rgb method has been provided, we may be able to use it.
     * Currently this will always be the case, as a default value will be
     * provided this method. While this default may not be correct, we are not
     * introducing any new errors by using it.
     */
    if (dev_proc(dev, map_color_rgb) != 0) {

        /* if the device has a DeviceRGB color model, use map_color_rgb */
        if (is_like_DeviceRGB(dev))
            return dev_proc(dev, map_color_rgb);

        /* If separable ande linear then use default */
        if (colors_are_separable_and_linear(&dev->color_info))
            return &gx_default_decode_color;

        /* gray devices can be handled based on their polarity */
        if ( dev->color_info.num_components == 1 &&
             dev->color_info.gray_index == 0       )
            return dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE
                       ? &gx_default_1_add_decode_color
                       : &gx_default_1_sub_decode_color;

        /*
         * There is no accurate way to decode colors for cmyk devices
         * using the map_color_rgb procedure. Unfortunately, this cases
         * arises with some frequency, so it is useful not to generate an
         * error in this case. The mechanism below assumes full undercolor
         * removal and black generation, which may not be accurate but are
         * the  best that can be done in the general case in the absence of
         * other information.
         *
         * As a hack to handle certain common devices, if the map_rgb_color
         * routine is cmyk_1bit_map_color_rgb, we provide a direct one-bit
         * decoder.
         */
        if (is_like_DeviceCMYK(dev)) {
            if (dev_proc(dev, map_color_rgb) == cmyk_1bit_map_color_rgb)
                return &gx_1bit_cmyk_decode_color;
            else
                return &gx_default_cmyk_decode_color;
        }
    }

    /*
     * The separable and linear case will already have been handled by
     * code in gx_device_fill_in_procs, so at this point we can only hope
     * the device doesn't use the decode_color method.
     */
    if (colors_are_separable_and_linear(&dev->color_info))
        return &gx_default_decode_color;
    else
        return &gx_error_decode_color;
}

/*
 * If a device has a linear and separable encode color function then
 * set up the comp_bits, comp_mask, and comp_shift fields.  Note:  This
 * routine assumes that the colorant shift factor decreases with the
 * component number.  See check_device_separable() for a general routine.
 */
void
set_linear_color_bits_mask_shift(gx_device * dev)
{
    int i;
    byte gray_index = dev->color_info.gray_index;
    gx_color_value max_gray = dev->color_info.max_gray;
    gx_color_value max_color = dev->color_info.max_color;
    int num_components = dev->color_info.num_components;

#define comp_bits (dev->color_info.comp_bits)
#define comp_mask (dev->color_info.comp_mask)
#define comp_shift (dev->color_info.comp_shift)
    comp_shift[num_components - 1] = 0;
    for ( i = num_components - 1 - 1; i >= 0; i-- ) {
        comp_shift[i] = comp_shift[i + 1] +
            ( i == gray_index ? ilog2(max_gray + 1) : ilog2(max_color + 1) );
    }
    for ( i = 0; i < num_components; i++ ) {
        comp_bits[i] = ( i == gray_index ?
                         ilog2(max_gray + 1) :
                         ilog2(max_color + 1) );
        comp_mask[i] = (((gx_color_index)1 << comp_bits[i]) - 1)
                                               << comp_shift[i];
    }
#undef comp_bits
#undef comp_mask
#undef comp_shift
}

/* Determine if a number is a power of two.  Works only for integers. */
#define is_power_of_two(x) ((((x) - 1) & (x)) == 0)

/* A brutish way to check if we are a HT device */
bool
device_is_contone(gx_device* pdev)
{
    if ((float)pdev->color_info.depth / (float)pdev->color_info.num_components >= 8)
        return true;
    return false;
}

/*
 * This routine attempts to determine if a device's encode_color procedure
 * produces gx_color_index values which are 'separable'.  A 'separable' value
 * means two things.  Each colorant has a group of bits in the gx_color_index
 * value which is associated with the colorant.  These bits are separate.
 * I.e. no bit is associated with more than one colorant.  If a colorant has
 * a value of zero then the bits associated with that colorant are zero.
 * These criteria allows the graphics library to build gx_color_index values
 * from the colorant values and not using the encode_color routine. This is
 * useful and necessary for overprinting, halftoning more
 * than four colorants, and the fast shading logic.  However this information
 * is not setup by the default device macros.  Thus we attempt to derive this
 * information.
 *
 * This routine can be fooled.  However it usually errors on the side of
 * assuing that a device is not separable.  In this case it does not create
 * any new problems.  In theory it can be fooled into believing that a device
 * is separable when it is not.  However we do not know of any real cases that
 * will fool it.
 */
void
check_device_separable(gx_device * dev)
{
    int i, j;
    gx_device_color_info * pinfo = &(dev->color_info);
    int num_components = pinfo->num_components;
    byte comp_shift[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte comp_bits[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index comp_mask[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color_index;
    gx_color_index current_bits = 0;
    gx_color_value colorants[GX_DEVICE_COLOR_MAX_COMPONENTS] = { 0 };

    /* If this is already known then we do not need to do anything. */
    if (pinfo->separable_and_linear != GX_CINFO_UNKNOWN_SEP_LIN)
        return;
    /* If there is not an encode_color_routine then we cannot proceed. */
    if (dev_proc(dev, encode_color) == NULL)
        return;
    /*
     * If these values do not check then we should have an error.  However
     * we do not know what to do so we are simply exitting and hoping that
     * the device will clean up its values.
     */
    if (pinfo->gray_index < num_components &&
        (!pinfo->dither_grays || pinfo->dither_grays != (pinfo->max_gray + 1)))
            return;
    if ((num_components > 1 || pinfo->gray_index != 0) &&
        (!pinfo->dither_colors || pinfo->dither_colors != (pinfo->max_color + 1)))
        return;
    /*
     * If dither_grays or dither_colors is not a power of two then we assume
     * that the device is not separable.  In theory this not a requirement
     * but it has been true for all of the devices that we have seen so far.
     * This assumption also makes the logic in the next section easier.
     */
    if (!is_power_of_two(pinfo->dither_grays)
                    || !is_power_of_two(pinfo->dither_colors))
        return;
    /*
     * Use the encode_color routine to try to verify that the device is
     * separable and to determine the shift count, etc. for each colorant.
     */
    color_index = dev_proc(dev, encode_color)(dev, colorants);
    if (color_index != 0)
        return;		/* Exit if zero colorants produce a non zero index */
    for (i = 0; i < num_components; i++) {
        /* Check this colorant = max with all others = 0 */
        for (j = 0; j < num_components; j++)
            colorants[j] = 0;
        colorants[i] = gx_max_color_value;
        color_index = dev_proc(dev, encode_color)(dev, colorants);
        if (color_index == 0)	/* If no bits then we have a problem */
            return;
        if (color_index & current_bits)	/* Check for overlapping bits */
            return;
        current_bits |= color_index;
        comp_mask[i] = color_index;
        /* Determine the shift count for the colorant */
        for (j = 0; (color_index & 1) == 0 && color_index != 0; j++)
            color_index >>= 1;
        comp_shift[i] = j;
        /* Determine the bit count for the colorant */
        for (j = 0; color_index != 0; j++) {
            if ((color_index & 1) == 0) /* check for non-consecutive bits */
                return;
            color_index >>= 1;
        }
        comp_bits[i] = j;
        /*
         * We could verify that the bit count matches the dither_grays or
         * dither_colors values, but this is not really required unless we
         * are halftoning.  Thus we are allowing for non equal colorant sizes.
         */
        /* Check for overlap with other colorant if they are all maxed */
        for (j = 0; j < num_components; j++)
            colorants[j] = gx_max_color_value;
        colorants[i] = 0;
        color_index = dev_proc(dev, encode_color)(dev, colorants);
        if (color_index & comp_mask[i])	/* Check for overlapping bits */
            return;
    }
    /* If we get to here then the device is very likely to be separable. */
    pinfo->separable_and_linear = GX_CINFO_SEP_LIN;
    for (i = 0; i < num_components; i++) {
        pinfo->comp_shift[i] = comp_shift[i];
        pinfo->comp_bits[i] = comp_bits[i];
        pinfo->comp_mask[i] = comp_mask[i];
    }
    /*
     * The 'gray_index' value allows one colorant to have a different number
     * of shades from the remainder.  Since the default macros only guess at
     * an appropriate value, we are setting its value based upon the data that
     * we just determined.  Note:  In some cases the macros set max_gray to 0
     * and dither_grays to 1.  This is not valid so ignore this case.
     */
    for (i = 0; i < num_components; i++) {
        int dither = 1 << comp_bits[i];

        if (pinfo->dither_grays != 1 && dither == pinfo->dither_grays) {
            pinfo->gray_index = i;
            break;
        }
    }
}
#undef is_power_of_two

/*
 * This routine attempts to determine if a device's encode_color procedure
 * produces values that are in keeping with "the standard encoding".
 * i.e. that given by pdf14_encode_color.
 *
 * It works by first checking to see if we are separable_and_linear. If not
 * we cannot hope to be the standard encoding.
 *
 * Then, we check to see if we are a dev device - if so, we must be
 * compatible.
 *
 * Failing that it checks to see if the encoding uses the appropriate
 * bit ranges for each individual color.
 *
 * If those (quick) tests pass, then we try the slower test of checking
 * the encodings. We can do this far faster than an exhaustive check, by
 * relying on the separability and linearity - we only need to check 256
 * possible values.
 *
 * The one tricky section there is to avoid the special case for
 * gx_no_color_index_value (which can occur when we have a 32bit
 * gx_color_index type, and a 4 component device, such as cmyk).
 * We allow the encoding to be off in the lower bits for that case.
 */
void check_device_compatible_encoding(gx_device *dev)
{
    gx_device_color_info * pinfo = &(dev->color_info);
    int num_components = pinfo->num_components;
    gx_color_index mul, color_index;
    int i, j;
    gx_color_value colorants[GX_DEVICE_COLOR_MAX_COMPONENTS];
    bool deep = device_is_deep(dev);

    if (pinfo->separable_and_linear == GX_CINFO_UNKNOWN_SEP_LIN)
        check_device_separable(dev);
    if (pinfo->separable_and_linear != GX_CINFO_SEP_LIN)
        return;

    if (dev_proc(dev, ret_devn_params)(dev) != NULL) {
        /* We know all devn devices are compatible. */
        pinfo->separable_and_linear = GX_CINFO_SEP_LIN_STANDARD;
        return;
    }

    /* Do the superficial quick checks */
    for (i = 0; i < num_components; i++) {
        int shift = (num_components-1-i)*(8<<deep);
        if (pinfo->comp_shift[i] != shift)
            goto bad;
        if (pinfo->comp_bits[i] != 8<<deep)
            goto bad;
        if (pinfo->comp_mask[i] != ((gx_color_index)(deep ? 65535 : 255))<<shift)
            goto bad;
    }

    /* OK, now we are going to be slower. */
    mul = 0;
    for (i = 0; i < num_components; i++) {
        mul = (mul<<(8<<deep)) | 1;
    }
    /* In the deep case, we don't exhaustively test */
    for (i = 0; i < 255; i++) {
        for (j = 0; j < num_components; j++)
            colorants[j] = i*257;
        color_index = dev_proc(dev, encode_color)(dev, colorants);
        if (color_index != i*mul*(deep ? 257 : 1) && (i*mul*(deep ? 257 : 1) != gx_no_color_index_value))
            goto bad;
    }
    /* If we reach here, then every value matched, except possibly the last one.
     * We'll allow that to differ just in the lowest bits. */
    if ((color_index | mul) != 255*mul*(deep ? 257 : 1))
        goto bad;

    pinfo->separable_and_linear = GX_CINFO_SEP_LIN_STANDARD;
    return;
bad:
    pinfo->separable_and_linear = GX_CINFO_SEP_LIN_NON_STANDARD;
}

int gx_default_no_copy_alpha_hl_color(gx_device * dev, const byte * data, int data_x, int raster, gx_bitmap_id id, int x, int y, int width, int height, const gx_drawing_color *pdcolor, int depth);

/* Fill in NULL procedures in a device procedure record. */
void
gx_device_fill_in_procs(register gx_device * dev)
{
    fill_dev_proc(dev, open_device, gx_default_open_device);
    fill_dev_proc(dev, get_initial_matrix, gx_default_get_initial_matrix);
    fill_dev_proc(dev, sync_output, gx_default_sync_output);
    fill_dev_proc(dev, output_page, gx_default_output_page);
    fill_dev_proc(dev, close_device, gx_default_close_device);
    /* see below for map_rgb_color */
    fill_dev_proc(dev, map_color_rgb, gx_default_map_color_rgb);
    /* NOT fill_rectangle */
    fill_dev_proc(dev, copy_mono, gx_default_copy_mono);
    fill_dev_proc(dev, copy_color, gx_default_copy_color);
    fill_dev_proc(dev, get_params, gx_default_get_params);
    fill_dev_proc(dev, put_params, gx_default_put_params);
    /* see below for map_cmyk_color */
    fill_dev_proc(dev, get_page_device, gx_default_get_page_device);
    fill_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
    fill_dev_proc(dev, copy_alpha, gx_default_copy_alpha);
    fill_dev_proc(dev, fill_path, gx_default_fill_path);
    fill_dev_proc(dev, stroke_path, gx_default_stroke_path);
    fill_dev_proc(dev, fill_mask, gx_default_fill_mask);
    fill_dev_proc(dev, fill_trapezoid, gx_default_fill_trapezoid);
    fill_dev_proc(dev, fill_parallelogram, gx_default_fill_parallelogram);
    fill_dev_proc(dev, fill_triangle, gx_default_fill_triangle);
    fill_dev_proc(dev, draw_thin_line, gx_default_draw_thin_line);
    fill_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
    fill_dev_proc(dev, strip_tile_rectangle, gx_default_strip_tile_rectangle);
    fill_dev_proc(dev, strip_copy_rop2, gx_default_strip_copy_rop2);
    fill_dev_proc(dev, strip_tile_rect_devn, gx_default_strip_tile_rect_devn);
    fill_dev_proc(dev, get_clipping_box, gx_default_get_clipping_box);
    fill_dev_proc(dev, begin_typed_image, gx_default_begin_typed_image);
    fill_dev_proc(dev, get_bits_rectangle, gx_default_get_bits_rectangle);
    fill_dev_proc(dev, composite, gx_default_composite);
    fill_dev_proc(dev, get_hardware_params, gx_default_get_hardware_params);
    fill_dev_proc(dev, text_begin, gx_default_text_begin);

    set_dev_proc(dev, encode_color, get_encode_color(dev));
    if (dev->color_info.num_components == 3)
        set_dev_proc(dev, map_rgb_color, dev_proc(dev, encode_color));
    if (dev->color_info.num_components == 4)
        set_dev_proc(dev, map_cmyk_color, dev_proc(dev, encode_color));

    if (colors_are_separable_and_linear(&dev->color_info)) {
        fill_dev_proc(dev, encode_color, gx_default_encode_color);
        fill_dev_proc(dev, map_cmyk_color, gx_default_encode_color);
        fill_dev_proc(dev, map_rgb_color, gx_default_encode_color);
    } else {
        /* if it isn't set now punt */
        fill_dev_proc(dev, encode_color, gx_error_encode_color);
        fill_dev_proc(dev, map_cmyk_color, gx_error_encode_color);
        fill_dev_proc(dev, map_rgb_color, gx_error_encode_color);
    }

    /*
     * Fill in the color mapping procedures and the component index
     * assignment procedure if they have not been provided by the client.
     *
     * Because it is difficult to provide default encoding procedures
     * that handle level inversion, this code needs to check both
     * the number of components and the polarity of color model.
     */
    switch (dev->color_info.num_components) {
    case 1:     /* DeviceGray or DeviceInvertGray */
        /*
         * If not gray then the device must provide the color
         * mapping procs.
         */
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
            fill_dev_proc( dev,
                       get_color_mapping_procs,
                       gx_default_DevGray_get_color_mapping_procs );
        } else
            fill_dev_proc(dev, get_color_mapping_procs, gx_error_get_color_mapping_procs);
        fill_dev_proc( dev,
                       get_color_comp_index,
                       gx_default_DevGray_get_color_comp_index );
        break;

    case 3:
        if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE) {
            fill_dev_proc( dev,
                       get_color_mapping_procs,
                       gx_default_DevRGB_get_color_mapping_procs );
            fill_dev_proc( dev,
                       get_color_comp_index,
                       gx_default_DevRGB_get_color_comp_index );
        } else {
            fill_dev_proc(dev, get_color_mapping_procs, gx_error_get_color_mapping_procs);
            fill_dev_proc(dev, get_color_comp_index, gx_error_get_color_comp_index);
        }
        break;

    case 4:
        fill_dev_proc(dev, get_color_mapping_procs, gx_default_DevCMYK_get_color_mapping_procs);
        fill_dev_proc(dev, get_color_comp_index, gx_default_DevCMYK_get_color_comp_index);
        break;
    default:		/* Unknown color model - set error handlers */
        if (dev_proc(dev, get_color_mapping_procs) == NULL) {
            fill_dev_proc(dev, get_color_mapping_procs, gx_error_get_color_mapping_procs);
            fill_dev_proc(dev, get_color_comp_index, gx_error_get_color_comp_index);
        }
    }

    set_dev_proc(dev, decode_color, get_decode_color(dev));
    fill_dev_proc(dev, get_profile, gx_default_get_profile);
    fill_dev_proc(dev, set_graphics_type_tag, gx_default_set_graphics_type_tag);

    fill_dev_proc(dev, fill_rectangle_hl_color, gx_default_fill_rectangle_hl_color);
    fill_dev_proc(dev, include_color_space, gx_default_include_color_space);
    fill_dev_proc(dev, fill_linear_color_scanline, gx_default_fill_linear_color_scanline);
    fill_dev_proc(dev, fill_linear_color_trapezoid, gx_default_fill_linear_color_trapezoid);
    fill_dev_proc(dev, fill_linear_color_triangle, gx_default_fill_linear_color_triangle);
    fill_dev_proc(dev, update_spot_equivalent_colors, gx_default_update_spot_equivalent_colors);
    fill_dev_proc(dev, ret_devn_params, gx_default_ret_devn_params);
    fill_dev_proc(dev, fillpage, gx_default_fillpage);
    fill_dev_proc(dev, copy_alpha_hl_color, gx_default_no_copy_alpha_hl_color);

    fill_dev_proc(dev, begin_transparency_group, gx_default_begin_transparency_group);
    fill_dev_proc(dev, end_transparency_group, gx_default_end_transparency_group);

    fill_dev_proc(dev, begin_transparency_mask, gx_default_begin_transparency_mask);
    fill_dev_proc(dev, end_transparency_mask, gx_default_end_transparency_mask);
    fill_dev_proc(dev, discard_transparency_layer, gx_default_discard_transparency_layer);

    fill_dev_proc(dev, push_transparency_state, gx_default_push_transparency_state);
    fill_dev_proc(dev, pop_transparency_state, gx_default_pop_transparency_state);

    fill_dev_proc(dev, put_image, gx_default_put_image);

    fill_dev_proc(dev, dev_spec_op, gx_default_dev_spec_op);
    fill_dev_proc(dev, copy_planes, gx_default_copy_planes);
    fill_dev_proc(dev, process_page, gx_default_process_page);
    fill_dev_proc(dev, transform_pixel_region, gx_default_transform_pixel_region);
    fill_dev_proc(dev, fill_stroke_path, gx_default_fill_stroke_path);
    fill_dev_proc(dev, lock_pattern, gx_default_lock_pattern);
}


int
gx_default_open_device(gx_device * dev)
{
    /* Initialize the separable status if not known. */
    check_device_separable(dev);
    return 0;
}

/* Get the initial matrix for a device with inverted Y. */
/* This includes essentially all printers and displays. */
/* Supports LeadingEdge, but no margins or viewports */
void
gx_default_get_initial_matrix(gx_device * dev, register gs_matrix * pmat)
{
    /* NB this device has no paper margins */
    double fs_res = dev->HWResolution[0] / 72.0;
    double ss_res = dev->HWResolution[1] / 72.0;

    switch(dev->LeadingEdge & LEADINGEDGE_MASK) {
    case 1: /* 90 degrees */
        pmat->xx = 0;
        pmat->xy = -ss_res;
        pmat->yx = -fs_res;
        pmat->yy = 0;
        pmat->tx = (float)dev->width;
        pmat->ty = (float)dev->height;
        break;
    case 2: /* 180 degrees */
        pmat->xx = -fs_res;
        pmat->xy = 0;
        pmat->yx = 0;
        pmat->yy = ss_res;
        pmat->tx = (float)dev->width;
        pmat->ty = 0;
        break;
    case 3: /* 270 degrees */
        pmat->xx = 0;
        pmat->xy = ss_res;
        pmat->yx = fs_res;
        pmat->yy = 0;
        pmat->tx = 0;
        pmat->ty = 0;
        break;
    default:
    case 0:
        pmat->xx = fs_res;
        pmat->xy = 0;
        pmat->yx = 0;
        pmat->yy = -ss_res;
        pmat->tx = 0;
        pmat->ty = (float)dev->height;
        /****** tx/y is WRONG for devices with ******/
        /****** arbitrary initial matrix ******/
        break;
    }
}
/* Get the initial matrix for a device with upright Y. */
/* This includes just a few printers and window systems. */
void
gx_upright_get_initial_matrix(gx_device * dev, register gs_matrix * pmat)
{
    pmat->xx = dev->HWResolution[0] / 72.0;	/* x_pixels_per_inch */
    pmat->xy = 0;
    pmat->yx = 0;
    pmat->yy = dev->HWResolution[1] / 72.0;	/* y_pixels_per_inch */
    /****** tx/y is WRONG for devices with ******/
    /****** arbitrary initial matrix ******/
    pmat->tx = 0;
    pmat->ty = 0;
}

int
gx_default_sync_output(gx_device * dev) /* lgtm [cpp/useless-expression] */
{
    return 0;
}

int
gx_default_output_page(gx_device * dev, int num_copies, int flush)
{
    int code = dev_proc(dev, sync_output)(dev);

    if (code >= 0)
        code = gx_finish_output_page(dev, num_copies, flush);
    return code;
}

int
gx_default_close_device(gx_device * dev)
{
    return 0;
}

gx_device *
gx_default_get_page_device(gx_device * dev)
{
    return NULL;
}
gx_device *
gx_page_device_get_page_device(gx_device * dev)
{
    return dev;
}

int
gx_default_get_alpha_bits(gx_device * dev, graphics_object_type type)
{
    return (type == go_text ? dev->color_info.anti_alias.text_bits :
            dev->color_info.anti_alias.graphics_bits);
}

void
gx_default_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    pbox->p.x = 0;
    pbox->p.y = 0;
    pbox->q.x = int2fixed(dev->width);
    pbox->q.y = int2fixed(dev->height);
}
void
gx_get_largest_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    pbox->p.x = min_fixed;
    pbox->p.y = min_fixed;
    pbox->q.x = max_fixed;
    pbox->q.y = max_fixed;
}

int
gx_no_composite(gx_device * dev, gx_device ** pcdev,
                        const gs_composite_t * pcte,
                        gs_gstate * pgs, gs_memory_t * memory,
                        gx_device *cdev)
{
    return_error(gs_error_unknownerror);	/* not implemented */
}
int
gx_default_composite(gx_device * dev, gx_device ** pcdev,
                             const gs_composite_t * pcte,
                             gs_gstate * pgs, gs_memory_t * memory,
                             gx_device *cdev)
{
    return pcte->type->procs.create_default_compositor
        (pcte, pcdev, dev, pgs, memory);
}
int
gx_null_composite(gx_device * dev, gx_device ** pcdev,
                          const gs_composite_t * pcte,
                          gs_gstate * pgs, gs_memory_t * memory,
                          gx_device *cdev)
{
    *pcdev = dev;
    return 0;
}

/*
 * Default handler for creating a compositor device when writing the clist. */
int
gx_default_composite_clist_write_update(const gs_composite_t *pcte, gx_device * dev,
                gx_device ** pcdev, gs_gstate * pgs, gs_memory_t * mem)
{
    *pcdev = dev;		/* Do nothing -> return the same device */
    return 0;
}

/* Default handler for adjusting a compositor's CTM. */
int
gx_default_composite_adjust_ctm(gs_composite_t *pcte, int x0, int y0, gs_gstate *pgs)
{
    return 0;
}

/*
 * Default check for closing compositor.
 */
gs_compositor_closing_state
gx_default_composite_is_closing(const gs_composite_t *this, gs_composite_t **pcte, gx_device *dev)
{
    return COMP_ENQUEUE;
}

/*
 * Default check whether a next operation is friendly to the compositor.
 */
bool
gx_default_composite_is_friendly(const gs_composite_t *this, byte cmd0, byte cmd1)
{
    return false;
}

/*
 * Default handler for updating the clist device when reading a compositing
 * device.
 */
int
gx_default_composite_clist_read_update(gs_composite_t *pxcte, gx_device * cdev,
                gx_device * tdev, gs_gstate * pgs, gs_memory_t * mem)
{
    return 0;			/* Do nothing */
}

/*
 * Default handler for get_cropping returns no cropping.
 */
int
gx_default_composite_get_cropping(const gs_composite_t *pxcte, int *ry, int *rheight,
                                  int cropping_min, int cropping_max)
{
    return 0;			/* No cropping. */
}

int
gx_default_initialize_device(gx_device *dev)
{
    return 0;
}

int
gx_default_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    switch(dev_spec_op) {
        case gxdso_form_begin:
        case gxdso_form_end:
        case gxdso_pattern_can_accum:
        case gxdso_pattern_start_accum:
        case gxdso_pattern_finish_accum:
        case gxdso_pattern_load:
        case gxdso_pattern_shading_area:
        case gxdso_pattern_is_cpath_accum:
        case gxdso_pattern_handles_clip_path:
        case gxdso_is_pdf14_device:
        case gxdso_supports_devn:
        case gxdso_supports_hlcolor:
        case gxdso_supports_saved_pages:
        case gxdso_needs_invariant_palette:
        case gxdso_supports_iccpostrender:
        case gxdso_supports_alpha:
        case gxdso_pdf14_sep_device:
        case gxdso_supports_pattern_transparency:
        case gxdso_overprintsim_state:
        case gxdso_skip_icc_component_validation:
            return 0;
        case gxdso_pattern_shfill_doesnt_need_path:
            return (dev_proc(pdev, fill_path) == gx_default_fill_path);
        case gxdso_is_std_cmyk_1bit:
            return (dev_proc(pdev, map_cmyk_color) == cmyk_1bit_map_cmyk_color);
        case gxdso_interpolate_antidropout:
            return pdev->color_info.use_antidropout_downscaler;
        case gxdso_interpolate_threshold:
            if ((pdev->color_info.num_components == 1 &&
                 pdev->color_info.max_gray < 15) ||
                (pdev->color_info.num_components > 1 &&
                 pdev->color_info.max_color < 15)) {
                /* If we are a limited color device (i.e. we are halftoning)
                 * then only interpolate if we are upscaling by at least 4 */
                return 4;
            }
            return 0; /* Otherwise no change */
        case gxdso_get_dev_param:
            {
                dev_param_req_t *request = (dev_param_req_t *)data;
                return gx_default_get_param(pdev, request->Param, request->list);
            }
        case gxdso_current_output_device:
            {
                *(gx_device **)data = pdev;
                return 0;
            }
        case gxdso_copy_color_is_fast:
            return (dev_proc(pdev, copy_color) != gx_default_copy_color);
        case gxdso_is_encoding_direct:
            if (pdev->color_info.depth != 8 * pdev->color_info.num_components)
                return 0;
            return (dev_proc(pdev, encode_color) == gx_default_encode_color ||
                    dev_proc(pdev, encode_color) == gx_default_rgb_map_rgb_color);
        /* Just ignore information about events */
        case gxdso_event_info:
            return 0;
        case gxdso_overprint_active:
            return 0;
    }
    return_error(gs_error_undefined);
}

int
gx_default_fill_rectangle_hl_color(gx_device *pdev,
    const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    return_error(gs_error_rangecheck);
}

int
gx_default_include_color_space(gx_device *pdev, gs_color_space *cspace,
        const byte *res_name, int name_length)
{
    return 0;
}

/*
 * If a device wants to determine an equivalent color for its spot colors then
 * it needs to implement this method.  See comments at the start of
 * src/gsequivc.c.
 */
int
gx_default_update_spot_equivalent_colors(gx_device *pdev, const gs_gstate * pgs, const gs_color_space *pcs)
{
    return 0;
}

/*
 * If a device wants to determine implement support for spot colors then
 * it needs to implement this method.
 */
gs_devn_params *
gx_default_ret_devn_params(gx_device *pdev)
{
    return NULL;
}

int
gx_default_process_page(gx_device *dev, gx_process_page_options_t *options)
{
    gs_int_rect rect;
    int code = 0;
    void *buffer = NULL;

    /* Possible future improvements in here could be given by us dividing the
     * page up into n chunks, and spawning a thread per chunk to do the
     * process_fn call on. n could be given by NumRenderingThreads. This
     * would give us multi-core advantages even without clist. */
    if (options->init_buffer_fn) {
        code = options->init_buffer_fn(options->arg, dev, dev->memory, dev->width, dev->height, &buffer);
        if (code < 0)
            return code;
    }

    rect.p.x = 0;
    rect.p.y = 0;
    rect.q.x = dev->width;
    rect.q.y = dev->height;
    if (options->process_fn)
        code = options->process_fn(options->arg, dev, dev, &rect, buffer);
    if (code >= 0 && options->output_fn)
        code = options->output_fn(options->arg, dev, buffer);

    if (options->free_buffer_fn)
        options->free_buffer_fn(options->arg, dev, dev->memory, buffer);

    return code;
}

int
gx_default_begin_transparency_group(gx_device *dev, const gs_transparency_group_params_t *ptgp, const gs_rect *pbbox, gs_gstate *pgs, gs_memory_t *mem)
{
    return 0;
}

int
gx_default_end_transparency_group(gx_device *dev, gs_gstate *pgs)
{
    return 0;
}

int
gx_default_begin_transparency_mask(gx_device *dev, const gx_transparency_mask_params_t *ptgp, const gs_rect *pbbox, gs_gstate *pgs, gs_memory_t *mem)
{
    return 0;
}

int
gx_default_end_transparency_mask(gx_device *dev, gs_gstate *pgs)
{
    return 0;
}

int
gx_default_discard_transparency_layer(gx_device *dev, gs_gstate *pgs)
{
    return 0;
}

int
gx_default_push_transparency_state(gx_device *dev, gs_gstate *pgs)
{
    return 0;
}

int
gx_default_pop_transparency_state(gx_device *dev, gs_gstate *pgs)
{
    return 0;
}

int
gx_default_put_image(gx_device *dev, gx_device *mdev, const byte **buffers, int num_chan, int x, int y, int width, int height, int row_stride, int alpha_plane_index, int tag_plane_index)
{
    return_error(gs_error_undefined);
}

int
gx_default_no_copy_alpha_hl_color(gx_device * dev, const byte * data, int data_x, int raster, gx_bitmap_id id, int x, int y, int width, int height, const gx_drawing_color *pdcolor, int depth)
{
    return_error(gs_error_undefined);
}

int
gx_default_copy_planes(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id, int x, int y, int width, int height, int plane_height)
{
    return_error(gs_error_undefined);
}

/* ---------------- Default per-instance procedures ---------------- */

int
gx_default_install(gx_device * dev, gs_gstate * pgs)
{
    return 0;
}

int
gx_default_begin_page(gx_device * dev, gs_gstate * pgs)
{
    return 0;
}

int
gx_default_end_page(gx_device * dev, int reason, gs_gstate * pgs)
{
    return (reason != 2 ? 1 : 0);
}

void
gx_default_set_graphics_type_tag(gx_device *dev, gs_graphics_type_tag_t graphics_type_tag)
{
    /* set the tag but carefully preserve GS_DEVICE_ENCODES_TAGS */
    dev->graphics_type_tag = (dev->graphics_type_tag & GS_DEVICE_ENCODES_TAGS) | graphics_type_tag;
}

/* ---------------- Device subclassing procedures ---------------- */

/* Non-obvious code. The 'dest_procs' is the 'procs' memory occupied by the original device that we decided to subclass,
 * 'src_procs' is the newly allocated piece of memory, to which we have already copied the content of the
 * original device (including the procs), prototype is the device structure prototype for the subclassing device.
 * Here we copy the methods from the prototype to the original device procs memory *but* if the original (src_procs)
 * device had a NULL method, we make the new device procs have a NULL method too.
 * The reason for ths is ugly, there are some places in the graphics library which explicitly check for
 * a device having a NULL method and take different code paths depending on the result.
 * Now in general we expect subclassing devices to implement *every* method, so if we didn't copy
 * over NULL methods present in the original source device then the code path could be inappropriate for
 * that underlying (now subclassed) device.
 */
/* November 10th 2017 Restored the original behaviour of the device methods, they should now never be NULL.
 * Howwever, there are still places in the code which take different code paths if the device method is (now)
 * the default device method, rather than a device-specific method.
 * So instead of checking for NULL, we now need to check against the default implementation, and *NOT* copy the
 * prototype (subclass device) method if the original device had the default implementation.
 * I suspect a combination of forwarding and subclassing devices will not work properly for this reason.
 */
int gx_copy_device_procs(gx_device *dest, const gx_device *src, const gx_device *pprototype)
{
    gx_device prototype = *pprototype;

    /* In the new (as of 2021) world, the prototype does not contain
     * device procs. We need to call the 'initialize_device_procs'
     * function to properly populate the procs array. We can't write to
     * the const prototype pointer we are passed in, so copy it to a
     * local block, and initialize that instead, */
    prototype.initialize_device_procs(&prototype);
    /* Fill in missing entries with the global defaults */
    gx_device_fill_in_procs(&prototype);

    if (dest->initialize_device_procs == NULL)
       dest->initialize_device_procs = prototype.initialize_device_procs;

    set_dev_proc(dest, initialize_device, dev_proc(&prototype, initialize_device));
    set_dev_proc(dest, open_device, dev_proc(&prototype, open_device));
    set_dev_proc(dest, get_initial_matrix, dev_proc(&prototype, get_initial_matrix));
    set_dev_proc(dest, sync_output, dev_proc(&prototype, sync_output));
    set_dev_proc(dest, output_page, dev_proc(&prototype, output_page));
    set_dev_proc(dest, close_device, dev_proc(&prototype, close_device));
    set_dev_proc(dest, map_rgb_color, dev_proc(&prototype, map_rgb_color));
    set_dev_proc(dest, map_color_rgb, dev_proc(&prototype, map_color_rgb));
    set_dev_proc(dest, fill_rectangle, dev_proc(&prototype, fill_rectangle));
    set_dev_proc(dest, copy_mono, dev_proc(&prototype, copy_mono));
    set_dev_proc(dest, copy_color, dev_proc(&prototype, copy_color));
    set_dev_proc(dest, get_params, dev_proc(&prototype, get_params));
    set_dev_proc(dest, put_params, dev_proc(&prototype, put_params));
    set_dev_proc(dest, map_cmyk_color, dev_proc(&prototype, map_cmyk_color));
    set_dev_proc(dest, get_page_device, dev_proc(&prototype, get_page_device));
    set_dev_proc(dest, get_alpha_bits, dev_proc(&prototype, get_alpha_bits));
    set_dev_proc(dest, copy_alpha, dev_proc(&prototype, copy_alpha));
    set_dev_proc(dest, fill_path, dev_proc(&prototype, fill_path));
    set_dev_proc(dest, stroke_path, dev_proc(&prototype, stroke_path));
    set_dev_proc(dest, fill_trapezoid, dev_proc(&prototype, fill_trapezoid));
    set_dev_proc(dest, fill_parallelogram, dev_proc(&prototype, fill_parallelogram));
    set_dev_proc(dest, fill_triangle, dev_proc(&prototype, fill_triangle));
    set_dev_proc(dest, draw_thin_line, dev_proc(&prototype, draw_thin_line));
    set_dev_proc(dest, strip_tile_rectangle, dev_proc(&prototype, strip_tile_rectangle));
    set_dev_proc(dest, get_clipping_box, dev_proc(&prototype, get_clipping_box));
    set_dev_proc(dest, begin_typed_image, dev_proc(&prototype, begin_typed_image));
    set_dev_proc(dest, get_bits_rectangle, dev_proc(&prototype, get_bits_rectangle));
    set_dev_proc(dest, composite, dev_proc(&prototype, composite));
    set_dev_proc(dest, get_hardware_params, dev_proc(&prototype, get_hardware_params));
    set_dev_proc(dest, text_begin, dev_proc(&prototype, text_begin));
    set_dev_proc(dest, discard_transparency_layer, dev_proc(&prototype, discard_transparency_layer));
    set_dev_proc(dest, get_color_mapping_procs, dev_proc(&prototype, get_color_mapping_procs));
    set_dev_proc(dest, get_color_comp_index, dev_proc(&prototype, get_color_comp_index));
    set_dev_proc(dest, encode_color, dev_proc(&prototype, encode_color));
    set_dev_proc(dest, decode_color, dev_proc(&prototype, decode_color));
    set_dev_proc(dest, fill_rectangle_hl_color, dev_proc(&prototype, fill_rectangle_hl_color));
    set_dev_proc(dest, include_color_space, dev_proc(&prototype, include_color_space));
    set_dev_proc(dest, fill_linear_color_scanline, dev_proc(&prototype, fill_linear_color_scanline));
    set_dev_proc(dest, fill_linear_color_trapezoid, dev_proc(&prototype, fill_linear_color_trapezoid));
    set_dev_proc(dest, fill_linear_color_triangle, dev_proc(&prototype, fill_linear_color_triangle));
    set_dev_proc(dest, update_spot_equivalent_colors, dev_proc(&prototype, update_spot_equivalent_colors));
    set_dev_proc(dest, ret_devn_params, dev_proc(&prototype, ret_devn_params));
    set_dev_proc(dest, fillpage, dev_proc(&prototype, fillpage));
    set_dev_proc(dest, push_transparency_state, dev_proc(&prototype, push_transparency_state));
    set_dev_proc(dest, pop_transparency_state, dev_proc(&prototype, pop_transparency_state));
    set_dev_proc(dest, dev_spec_op, dev_proc(&prototype, dev_spec_op));
    set_dev_proc(dest, get_profile, dev_proc(&prototype, get_profile));
    set_dev_proc(dest, strip_copy_rop2, dev_proc(&prototype, strip_copy_rop2));
    set_dev_proc(dest, strip_tile_rect_devn, dev_proc(&prototype, strip_tile_rect_devn));
    set_dev_proc(dest, process_page, dev_proc(&prototype, process_page));
    set_dev_proc(dest, transform_pixel_region, dev_proc(&prototype, transform_pixel_region));
    set_dev_proc(dest, fill_stroke_path, dev_proc(&prototype, fill_stroke_path));
    set_dev_proc(dest, lock_pattern, dev_proc(&prototype, lock_pattern));

    /*
     * We absolutely must set the 'set_graphics_type_tag' to the default subclass one
     * even if the subclassed device is using the default. This is because the
     * default implementation sets a flag in the device structure, and if we
     * copy the default method, we'll end up setting the flag in the subclassing device
     * instead of the subclassed device!
     */
    set_dev_proc(dest, set_graphics_type_tag, dev_proc(&prototype, set_graphics_type_tag));

    /* These are the routines whose existence is checked against the default at
     * some point in the code. The code path differs when the device implements a
     * method other than the default, so the subclassing device needs to ensure that
     * if the subclassed device has one of these methods set to the default, we
     * do not overwrite the default method.
     */
    if (dev_proc(src, fill_mask) != gx_default_fill_mask)
        set_dev_proc(dest, fill_mask, dev_proc(&prototype, fill_mask));
    if (dev_proc(src, begin_transparency_group) != gx_default_begin_transparency_group)
        set_dev_proc(dest, begin_transparency_group, dev_proc(&prototype, begin_transparency_group));
    if (dev_proc(src, end_transparency_group) != gx_default_end_transparency_group)
        set_dev_proc(dest, end_transparency_group, dev_proc(&prototype, end_transparency_group));
    if (dev_proc(src, put_image) != gx_default_put_image)
        set_dev_proc(dest, put_image, dev_proc(&prototype, put_image));
    if (dev_proc(src, copy_planes) != gx_default_copy_planes)
        set_dev_proc(dest, copy_planes, dev_proc(&prototype, copy_planes));
    if (dev_proc(src, copy_alpha_hl_color) != gx_default_no_copy_alpha_hl_color)
        set_dev_proc(dest, copy_alpha_hl_color, dev_proc(&prototype, copy_alpha_hl_color));

    return 0;
}

int gx_device_subclass(gx_device *dev_to_subclass, gx_device *new_prototype, unsigned int private_data_size)
{
    gx_device *child_dev;
    void *psubclass_data;
    gs_memory_struct_type_t *a_std, *b_std = NULL;
    int dynamic = dev_to_subclass->stype_is_dynamic;
    char *ptr, *ptr1;

    /* If this happens we are stuffed, as there is no way to get hold
     * of the original device's stype structure, which means we cannot
     * allocate a replacement structure. Abort if so.
     * Also abort if the new_prototype device struct is too large.
     */
    if (!dev_to_subclass->stype ||
        dev_to_subclass->stype->ssize < new_prototype->params_size)
        return_error(gs_error_VMerror);

    /* We make a 'stype' structure for our new device, and copy the old stype into it
     * This means our new device will always have the 'stype_is_dynamic' flag set
     */
    a_std = (gs_memory_struct_type_t *)
        gs_alloc_bytes_immovable(dev_to_subclass->memory->non_gc_memory, sizeof(*a_std),
                                 "gs_device_subclass(stype)");
    if (!a_std)
        return_error(gs_error_VMerror);
    *a_std = *dev_to_subclass->stype;
    a_std->ssize = dev_to_subclass->params_size;

    if (!dynamic) {
        b_std = (gs_memory_struct_type_t *)
            gs_alloc_bytes_immovable(dev_to_subclass->memory->non_gc_memory, sizeof(*b_std),
                                     "gs_device_subclass(stype)");
        if (!b_std)
            return_error(gs_error_VMerror);
    }

    /* Allocate a device structure for the new child device */
    child_dev = gs_alloc_struct_immovable(dev_to_subclass->memory->stable_memory, gx_device, a_std,
                                        "gs_device_subclass(device)");
    if (child_dev == 0) {
        gs_free_const_object(dev_to_subclass->memory->non_gc_memory, a_std, "gs_device_subclass(stype)");
        gs_free_const_object(dev_to_subclass->memory->non_gc_memory, b_std, "gs_device_subclass(stype)");
        return_error(gs_error_VMerror);
    }

    /* Make sure all methods are filled in, note this won't work for a forwarding device
     * so forwarding devices will have to be filled in before being subclassed. This doesn't fill
     * in the fill_rectangle proc, that gets done in the ultimate device's open proc.
     */
    gx_device_fill_in_procs(dev_to_subclass);
    memcpy(child_dev, dev_to_subclass, dev_to_subclass->stype->ssize);
    child_dev->stype = a_std;
    child_dev->stype_is_dynamic = 1;

    /* At this point, the only counted reference to the child is from its parent, and we need it to use the right allocator */
    rc_init(child_dev, dev_to_subclass->memory->stable_memory, 1);

    psubclass_data = (void *)gs_alloc_bytes(dev_to_subclass->memory->non_gc_memory, private_data_size, "subclass memory for subclassing device");
    if (psubclass_data == 0){
        gs_free_const_object(dev_to_subclass->memory->non_gc_memory, b_std, "gs_device_subclass(stype)");
        /* We *don't* want to run the finalize routine. This would free the stype and
         * properly handle the icc_struct and PageList, but for devices with a custom
         * finalize (eg psdcmyk) it might also free memory it had allocated, and we're
         * still pointing at that memory in the parent.
         */
        a_std->finalize = NULL;
        gs_set_object_type(dev_to_subclass->memory->stable_memory, child_dev, a_std);
        gs_free_object(dev_to_subclass->memory->stable_memory, child_dev, "free subclass memory for subclassing device");
        gs_free_const_object(dev_to_subclass->memory->non_gc_memory, a_std, "gs_device_subclass(stype)");
        return_error(gs_error_VMerror);
    }
    memset(psubclass_data, 0x00, private_data_size);

    gx_copy_device_procs(dev_to_subclass, child_dev, new_prototype);
    dev_to_subclass->finalize = new_prototype->finalize;
    dev_to_subclass->dname = new_prototype->dname;
    if (dev_to_subclass->icc_struct)
        rc_increment(dev_to_subclass->icc_struct);
    if (dev_to_subclass->PageList)
        rc_increment(dev_to_subclass->PageList);
    if (dev_to_subclass->NupControl)
        rc_increment(dev_to_subclass->NupControl);

    dev_to_subclass->page_procs = new_prototype->page_procs;
    gx_subclass_fill_in_page_procs(dev_to_subclass);

    /* In case the new device we're creating has already been initialised, copy
     * its additional data.
     */
    ptr = ((char *)dev_to_subclass) + sizeof(gx_device);
    ptr1 = ((char *)new_prototype) + sizeof(gx_device);
    memcpy(ptr, ptr1, new_prototype->params_size - sizeof(gx_device));

    /* If the original device's stype structure was dynamically allocated, we need
     * to 'fixup' the contents, it's procs need to point to the new device's procs
     * for instance.
     */
    if (dynamic) {
        if (new_prototype->stype) {
            b_std = (gs_memory_struct_type_t *)dev_to_subclass->stype;
            *b_std = *new_prototype->stype;
            b_std->ssize = a_std->ssize;
            dev_to_subclass->stype_is_dynamic = 1;
        } else {
            gs_free_const_object(child_dev->memory->non_gc_memory, dev_to_subclass->stype,
                             "unsubclass");
            dev_to_subclass->stype = NULL;
            b_std = (gs_memory_struct_type_t *)new_prototype->stype;
            dev_to_subclass->stype_is_dynamic = 0;
        }
    }
    else {
        *b_std = *new_prototype->stype;
        b_std->ssize = a_std->ssize;
        dev_to_subclass->stype_is_dynamic = 1;
    }
    dev_to_subclass->stype = b_std;
    /* We have to patch up the "type" parameters that the memory manage/garbage
     * collector will use, as well.
     */
    gs_set_object_type(child_dev->memory, dev_to_subclass, b_std);

    dev_to_subclass->subclass_data = psubclass_data;
    dev_to_subclass->child = child_dev;
    if (child_dev->parent) {
        dev_to_subclass->parent = child_dev->parent;
        child_dev->parent->child = dev_to_subclass;
    }
    if (child_dev->child) {
        child_dev->child->parent = child_dev;
    }
    child_dev->parent = dev_to_subclass;

    return 0;
}

void gx_device_unsubclass(gx_device *dev)
{
    generic_subclass_data *psubclass_data;
    gx_device *parent, *child;
    gs_memory_struct_type_t *a_std = 0, *b_std = 0;
    int dynamic, ref_count;
    gs_memory_t *rcmem;

    /* This should not happen... */
    if (!dev)
        return;

    ref_count = dev->rc.ref_count;
    rcmem = dev->rc.memory;

    child = dev->child;
    psubclass_data = (generic_subclass_data *)dev->subclass_data;
    parent = dev->parent;
    dynamic = dev->stype_is_dynamic;

    /* We need to account for the fact that we are removing ourselves from
     * the device chain after a clist device has been pushed, due to a
     * compositor action. Since we patched the clist 'composite'
     * method (and target device) when it was pushed.
     * A point to note; we *don't* want to change the forwarding device's
     * 'target', because when we copy the child up to replace 'this' device
     * we do still want the forwarding device to point here. NB its the *child*
     * device that goes away.
     */
    if (psubclass_data != NULL && psubclass_data->forwarding_dev != NULL && psubclass_data->saved_compositor_method)
        psubclass_data->forwarding_dev->procs.composite = psubclass_data->saved_compositor_method;

    /* If ths device's stype is dynamically allocated, keep a copy of it
     * in case we might need it.
     */
    if (dynamic) {
        a_std = (gs_memory_struct_type_t *)dev->stype;
        if (child)
            *a_std = *child->stype;
    }

    /* If ths device has any private storage, free it now */
    if (psubclass_data)
        gs_free_object(dev->memory->non_gc_memory, psubclass_data, "gx_device_unsubclass");

    /* Copy the child device into ths device's memory */
    if (child) {
        b_std = (gs_memory_struct_type_t *)dev->stype;
        rc_decrement(dev->icc_struct, "unsubclass device");
        rc_increment(child->icc_struct);
        memcpy(dev, child, child->stype->ssize);
        /* Patch back the 'stype' in the memory manager */
        gs_set_object_type(child->memory, dev, b_std);

        dev->stype = b_std;
        /* The reference count of the subclassing device may have been
         * changed (eg graphics states pointing to it) after we subclassed
         * the device. We need to ensure that we do not overwrite this
         * when we copy back the subclassed device.
         */
        dev->rc.ref_count = ref_count;
        dev->rc.memory = rcmem;

        /* If we have a chain of devices, make sure the chain beyond the
         * device we're unsubclassing doesn't get broken, we need to
         * detach the lower chain and reattach it at the new highest level.
         */
        if (child->child)
            child->child->parent = dev;
        child->parent->child = child->child;
    }

    /* How can we have a subclass device with no child ? Simples; when we
     * hit the end of job restore, the devices are not freed in device
     * chain order. To make sure we don't end up following stale pointers,
     * when a device is freed we remove it from the chain and update
     * any dangling pointers to NULL. When we later free the remaining
     * devices it's possible that their child pointer can then be NULL.
     */
    if (child) {
        /* We cannot afford to free the child device if its stype is not
         * dynamic because we can't 'null' the finalise routine, and we
         * cannot permit the device to be finalised because we have copied
         * it up one level, not discarded it. (This shouldn't happen! Child
         * devices are always created with a dynamic stype.) If this ever
         * happens garbage collecton will eventually clean up the memory.
         */
        if (child->stype_is_dynamic) {
            /* Make sure that nothing will try to follow the device chain,
             * just security here. */
            child->parent = NULL;
            child->child = NULL;

            /* We *don't* want to run the finalize routine. This would free
             * the stype and properly handle the icc_struct and PageList,
             * but for devices with a custom finalize (eg psdcmyk) it might
             * also free memory it had allocated, and we're still pointing
             * at that memory in the parent. The indirection through a
             * variable is just to get rid of const warnings.
             */
            b_std = (gs_memory_struct_type_t *)child->stype;
            gs_free_const_object(dev->memory->non_gc_memory, b_std, "gs_device_unsubclass(stype)");
            /* Make this into a generic device */
            child->stype = &st_device;
            child->stype_is_dynamic = false;

            /* We can't simply discard the child device, because there may be references to it elsewhere,
               but equally, we really don't want it doing anything, so set the procs so actions are just discarded.
             */
            gx_copy_device_procs(child, (gx_device *)&gs_null_device, (gx_device *)&gs_null_device);

            /* Having changed the stype, we need to make sure the memory
             * manager uses it. It keeps a copy in its own data structure,
             * and would use that copy, which would mean it would call the
             * finalize routine that we just patched out.
             */
            gs_set_object_type(dev->memory->stable_memory, child, child->stype);
            child->finalize = NULL;
            /* Now (finally) free the child memory */
            rc_decrement(child, "gx_device_unsubclass(device)");
        }
    }
    dev->parent = parent;

    /* If this device has a dynamic stype, we wnt to keep using it, but we copied
     * the stype pointer from the child when we copied the rest of the device. So
     * we update the stype pointer with the saved pointer to this device's stype.
     */
    if (dynamic) {
        dev->stype = a_std;
        dev->stype_is_dynamic = 1;
    } else {
        dev->stype_is_dynamic = 0;
    }
}

int gx_update_from_subclass(gx_device *dev)
{
    if (!dev->child)
        return 0;

    memcpy(&dev->color_info, &dev->child->color_info, sizeof(gx_device_color_info));
    memcpy(&dev->cached_colors, &dev->child->cached_colors, sizeof(gx_device_cached_colors_t));
    dev->max_fill_band = dev->child->max_fill_band;
    dev->width = dev->child->width;
    dev->height = dev->child->height;
    dev->pad = dev->child->pad;
    dev->log2_align_mod = dev->child->log2_align_mod;
    dev->max_fill_band = dev->child->max_fill_band;
    dev->num_planar_planes = dev->child->num_planar_planes;
    dev->LeadingEdge = dev->child->LeadingEdge;
    memcpy(&dev->ImagingBBox, &dev->child->ImagingBBox, sizeof(dev->child->ImagingBBox));
    dev->ImagingBBox_set = dev->child->ImagingBBox_set;
    memcpy(&dev->MediaSize, &dev->child->MediaSize, sizeof(dev->child->MediaSize));
    memcpy(&dev->HWResolution, &dev->child->HWResolution, sizeof(dev->child->HWResolution));
    memcpy(&dev->Margins, &dev->child->Margins, sizeof(dev->child->Margins));
    memcpy(&dev->HWMargins, &dev->child->HWMargins, sizeof(dev->child->HWMargins));
    dev->FirstPage = dev->child->FirstPage;
    dev->LastPage = dev->child->LastPage;
    dev->PageCount = dev->child->PageCount;
    dev->ShowpageCount = dev->child->ShowpageCount;
    dev->NumCopies = dev->child->NumCopies;
    dev->NumCopies_set = dev->child->NumCopies_set;
    dev->IgnoreNumCopies = dev->child->IgnoreNumCopies;
    dev->UseCIEColor = dev->child->UseCIEColor;
    dev->LockSafetyParams= dev->child->LockSafetyParams;
    dev->band_offset_x = dev->child->band_offset_y;
    dev->sgr = dev->child->sgr;
    dev->MaxPatternBitmap = dev->child->MaxPatternBitmap;
    dev->page_uses_transparency = dev->child->page_uses_transparency;
    memcpy(&dev->space_params, &dev->child->space_params, sizeof(gdev_space_params));
    dev->graphics_type_tag = dev->child->graphics_type_tag;

    return 0;
}

int gx_subclass_composite(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_gstate *pgs, gs_memory_t *memory, gx_device *cdev)
{
    pdf14_clist_device *p14dev;
    generic_subclass_data *psubclass_data;
    int code = 0;

    p14dev = (pdf14_clist_device *)dev;
    psubclass_data = (generic_subclass_data *)p14dev->target->subclass_data;

    set_dev_proc(dev, composite, psubclass_data->saved_compositor_method);

    if (gs_is_pdf14trans_compositor(pcte) != 0 && strncmp(dev->dname, "pdf14clist", 10) == 0) {
        const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pcte;

        switch (pdf14pct->params.pdf14_op) {
            case PDF14_POP_DEVICE:
                {
                    pdf14_clist_device *p14dev = (pdf14_clist_device *)dev;
                    gx_device *subclass_device;

                    p14dev->target->color_info = p14dev->saved_target_color_info;
                    if (p14dev->target->child) {
                        p14dev->target->child->color_info = p14dev->saved_target_color_info;

                        set_dev_proc(p14dev->target->child, encode_color, p14dev->saved_target_encode_color);
                        set_dev_proc(p14dev->target->child, decode_color, p14dev->saved_target_decode_color);
                        set_dev_proc(p14dev->target->child, get_color_mapping_procs, p14dev->saved_target_get_color_mapping_procs);
                        set_dev_proc(p14dev->target->child, get_color_comp_index, p14dev->saved_target_get_color_comp_index);
                    }

                    pgs->get_cmap_procs = p14dev->save_get_cmap_procs;
                    gx_set_cmap_procs(pgs, p14dev->target);

                    subclass_device = p14dev->target;
                    p14dev->target = p14dev->target->child;

                    code = dev_proc(dev, composite)(dev, pcdev, pcte, pgs, memory, cdev);

                    p14dev->target = subclass_device;

                    /* We return 0, rather than 1, as we have not created
                     * a new compositor that wraps dev. */
                    if (code == 1)
                        code = 0;
                    return code;
                }
                break;
            default:
                code = dev_proc(dev, composite)(dev, pcdev, pcte, pgs, memory, cdev);
                break;
        }
    } else {
        code = dev_proc(dev, composite)(dev, pcdev, pcte, pgs, memory, cdev);
    }
    set_dev_proc(dev, composite, gx_subclass_composite);
    return code;
}

typedef enum
{
    transform_pixel_region_portrait,
    transform_pixel_region_landscape,
    transform_pixel_region_skew
} transform_pixel_region_posture;

typedef struct gx_default_transform_pixel_region_state_s gx_default_transform_pixel_region_state_t;

typedef int (gx_default_transform_pixel_region_render_fn)(gx_device *dev, gx_default_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs);

struct gx_default_transform_pixel_region_state_s
{
    gs_memory_t *mem;
    gx_dda_fixed_point pixels;
    gx_dda_fixed_point rows;
    gs_int_rect clip;
    int w;
    int h;
    int spp;
    transform_pixel_region_posture posture;
    gs_logical_operation_t lop;
    byte *line;
    gx_default_transform_pixel_region_render_fn *render;
};

static void
get_portrait_y_extent(gx_default_transform_pixel_region_state_t *state, int *iy, int *ih)
{
    fixed y0, y1;
    gx_dda_fixed row = state->rows.y;

    y0 = dda_current(row);
    dda_next(row);
    y1 = dda_current(row);

    if (y1 < y0) {
        fixed t = y1; y1 = y0; y0 = t;
    }

    *iy = fixed2int_pixround_perfect(y0);
    *ih = fixed2int_pixround_perfect(y1) - *iy;
}

static void
get_landscape_x_extent(gx_default_transform_pixel_region_state_t *state, int *ix, int *iw)
{
    fixed x0, x1;
    gx_dda_fixed row = state->rows.x;

    x0 = dda_current(row);
    dda_next(row);
    x1 = dda_current(row);

    if (x1 < x0) {
        fixed t = x1; x1 = x0; x0 = t;
    }

    *ix = fixed2int_pixround_perfect(x0);
    *iw = fixed2int_pixround_perfect(x1) - *ix;
}

static void
get_skew_extents(gx_default_transform_pixel_region_state_t *state, fixed *w, fixed *h)
{
    fixed x0, x1, y0, y1;
    gx_dda_fixed_point row = state->rows;

    x0 = dda_current(row.x);
    y0 = dda_current(row.y);
    dda_next(row.x);
    dda_next(row.y);
    x1 = dda_current(row.x);
    y1 = dda_current(row.y);

    *w = x1-x0;
    *h = y1-y0;
}

static int
transform_pixel_region_render_portrait(gx_device *dev, gx_default_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    gs_logical_operation_t lop = state->lop;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int irun;			/* int x/rrun */
    int w = state->w;
    int h = state->h;
    int spp = state->spp;
    const byte *data = buffer[0] + data_x * spp;
    const byte *bufend = NULL;
    int code = 0;
    const byte *run = NULL;
    int k;
    gx_color_value *conc = &cmapper->conc[0];
    int to_rects;
    gx_cmapper_fn *mapper = cmapper->set_color;
    int minx, maxx;

    if (h == 0)
        return 0;

    /* Clip on Y */
    get_portrait_y_extent(state, &vci, &vdi);
    if (vci < state->clip.p.y)
        vdi += vci - state->clip.p.y, vci = state->clip.p.y;
    if (vci+vdi > state->clip.q.y)
        vdi = state->clip.q.y - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    irun = fixed2int_var_rounded(dda_current(pnext.x));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));
    to_rects = (dev->color_info.depth != spp*8);
    if (to_rects == 0) {
        if (dev_proc(dev, dev_spec_op)(dev, gxdso_copy_color_is_fast, NULL, 0) <= 0)
            to_rects = 1;
    }

    minx = state->clip.p.x;
    maxx = state->clip.q.x;
    bufend = data + w * spp;
    if (to_rects) {
        while (data < bufend) {
            /* Find the length of the next run. It will either end when we hit
             * the end of the source data, or when the pixel data differs. */
            run = data + spp;
            while (1) {
                dda_next(pnext.x);
                if (run >= bufend)
                    break;
                if (memcmp(run, data, spp))
                    break;
                run += spp;
            }
            /* So we have a run of pixels from data to run that are all the same. */
            /* This needs to be sped up */
            for (k = 0; k < spp; k++) {
                conc[k] = gx_color_value_from_byte(data[k]);
            }
            mapper(cmapper);
            /* Fill the region between irun and fixed2int_var_rounded(pnext.x) */
            {
                int xi = irun;
                int wi = (irun = fixed2int_var_rounded(dda_current(pnext.x))) - xi;

                if (wi < 0)
                    xi += wi, wi = -wi;
                if (xi < minx)
                    wi += xi - minx, xi = minx;
                if (xi + wi > maxx)
                    wi = maxx - xi;
                if (wi > 0)
                    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
                                                        &cmapper->devc, dev, lop);
            }
            if (code < 0)
                goto err;
            data = run;
        }
    } else {
        int pending_left = irun;
        int pending_right;
        byte *out;
        int depth = spp;
        if (state->line == NULL) {
            state->line = gs_alloc_bytes(state->mem,
                                         (size_t)dev->width * depth,
                                         "image line");
            if (state->line == NULL)
                return gs_error_VMerror;
        }
        out = state->line;

        if (minx < 0)
            minx = 0;
        if (maxx > dev->width)
            maxx = dev->width;

        if (pending_left < minx)
            pending_left = minx;
        else if (pending_left > maxx)
            pending_left = maxx;
        pending_right = pending_left;

        while (data < bufend) {
            /* Find the length of the next run. It will either end when we hit
             * the end of the source data, or when the pixel data differs. */
            run = data + spp;
            while (1) {
                dda_next(pnext.x);
                if (run >= bufend)
                    break;
                if (memcmp(run, data, spp))
                    break;
                run += spp;
            }
            /* So we have a run of pixels from data to run that are all the same. */
            /* This needs to be sped up */
            for (k = 0; k < spp; k++) {
                conc[k] = gx_color_value_from_byte(data[k]);
            }
            mapper(cmapper);
            /* Fill the region between irun and fixed2int_var_rounded(pnext.x) */
            {
                int xi = irun;
                int wi = (irun = fixed2int_var_rounded(dda_current(pnext.x))) - xi;

                if (wi < 0)
                    xi += wi, wi = -wi;

                if (xi < minx)
                    wi += xi - minx, xi = minx;
                if (xi + wi > maxx)
                    wi = maxx - xi;

                if (wi > 0) {
                    if (color_is_pure(&cmapper->devc)) {
                        gx_color_index color = cmapper->devc.colors.pure;
                        int xii = xi * spp;

                        if (pending_left > xi)
                            pending_left = xi;
                        else
                            pending_right = xi + wi;
                        do {
                            /* Excuse the double shifts below, that's to stop the
                             * C compiler complaining if the color index type is
                             * 32 bits. */
                            switch(depth)
                            {
                            case 8: out[xii++] = ((color>>28)>>28) & 0xff;
                            case 7: out[xii++] = ((color>>24)>>24) & 0xff;
                            case 6: out[xii++] = ((color>>24)>>16) & 0xff;
                            case 5: out[xii++] = ((color>>24)>>8) & 0xff;
                            case 4: out[xii++] = (color>>24) & 0xff;
                            case 3: out[xii++] = (color>>16) & 0xff;
                            case 2: out[xii++] = (color>>8) & 0xff;
                            case 1: out[xii++] = color & 0xff;
                            }
                        } while (--wi != 0);
                    } else {
                        if (pending_left != pending_right) {
                            code = dev_proc(dev, copy_color)(dev, out, pending_left, 0, 0, pending_left, vci, pending_right - pending_left, vdi);
                            if (code < 0)
                                goto err;
                        }
                        pending_left = pending_right = xi + (pending_left > xi ? 0 : wi);
                        code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
                                                            &cmapper->devc, dev, lop);
                    }
                }
                if (code < 0)
                    goto err;
            }
            data = run;
        }
        if (pending_left != pending_right) {
            code = dev_proc(dev, copy_color)(dev, out, pending_left, 0, 0, pending_left, vci, pending_right - pending_left, vdi);
            if (code < 0)
                goto err;
        }
    }
    return 1;
    /* Save position if error, in case we resume. */
err:
    buffer[0] = run;
    return code;
}

static int
transform_pixel_region_render_landscape(gx_device *dev, gx_default_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    gs_logical_operation_t lop = state->lop;
    gx_dda_fixed_point pnext;
    int vci, vdi;
    int irun;			/* int x/rrun */
    int w = state->w;
    int h = state->h;
    int spp = state->spp;
    const byte *data = buffer[0] + data_x * spp;
    const byte *bufend = NULL;
    int code = 0;
    const byte *run;
    int k;
    gx_color_value *conc = &cmapper->conc[0];
    int to_rects;
    gx_cmapper_fn *mapper = cmapper->set_color;
    int miny, maxy;

    if (h == 0)
        return 0;

    /* Clip on X */
    get_landscape_x_extent(state, &vci, &vdi);
    if (vci < state->clip.p.x)
        vdi += vci - state->clip.p.x, vci = state->clip.p.x;
    if (vci+vdi > state->clip.q.x)
        vdi = state->clip.q.x - vci;
    if (vdi <= 0)
        return 0;

    pnext = state->pixels;
    dda_translate(pnext.x,  (-fixed_epsilon));
    irun = fixed2int_var_rounded(dda_current(pnext.y));
    if_debug5m('b', dev->memory, "[b]y=%d data_x=%d w=%d xt=%f yt=%f\n",
               vci, data_x, w, fixed2float(dda_current(pnext.x)), fixed2float(dda_current(pnext.y)));
    to_rects = (dev->color_info.depth != spp*8);
    if (to_rects == 0) {
        if (dev_proc(dev, dev_spec_op)(dev, gxdso_copy_color_is_fast, NULL, 0) <= 0)
            to_rects = 1;
    }

    miny = state->clip.p.y;
    maxy = state->clip.q.y;
    bufend = data + w * spp;
    while (data < bufend) {
        /* Find the length of the next run. It will either end when we hit
         * the end of the source data, or when the pixel data differs. */
        run = data + spp;
        while (1) {
            dda_next(pnext.y);
            if (run >= bufend)
                break;
            if (memcmp(run, data, spp))
                break;
            run += spp;
        }
        /* So we have a run of pixels from data to run that are all the same. */
        /* This needs to be sped up */
        for (k = 0; k < spp; k++) {
            conc[k] = gx_color_value_from_byte(data[k]);
        }
        mapper(cmapper);
        /* Fill the region between irun and fixed2int_var_rounded(pnext.y) */
        {              /* 90 degree rotated rectangle */
            int yi = irun;
            int hi = (irun = fixed2int_var_rounded(dda_current(pnext.y))) - yi;

            if (hi < 0)
                yi += hi, hi = -hi;
            if (yi < miny)
                hi += yi - miny, yi = miny;
            if (yi + hi > maxy)
                hi = maxy - yi;
            if (hi > 0)
                code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
                                                    &cmapper->devc, dev, lop);
        }
        if (code < 0)
            goto err;
        data = run;
    }
    return 1;
    /* Save position if error, in case we resume. */
err:
    buffer[0] = run;
    return code;
}

static int
transform_pixel_region_render_skew(gx_device *dev, gx_default_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    gs_logical_operation_t lop = state->lop;
    gx_dda_fixed_point pnext;
    fixed xprev, yprev;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int w = state->w;
    int h = state->h;
    int spp = state->spp;
    const byte *data = buffer[0] + data_x * spp;
    fixed xpos;			/* x ditto */
    fixed ypos;			/* y ditto */
    const byte *bufend = data + w * spp;
    int code = 0;
    int k;
    byte initial_run[GX_DEVICE_COLOR_MAX_COMPONENTS] = { 0 };
    const byte *prev = &initial_run[0];
    gx_cmapper_fn *mapper = cmapper->set_color;
    gx_color_value *conc = &cmapper->conc[0];

    if (h == 0)
        return 0;
    pnext = state->pixels;
    get_skew_extents(state, &pdyx, &pdyy);
    dda_translate(pnext.x,  (-fixed_epsilon));
    xprev = dda_current(pnext.x);
    yprev = dda_current(pnext.y);
    if_debug4m('b', dev->memory, "[b]y=? data_x=%d w=%d xt=%f yt=%f\n",
               data_x, w, fixed2float(xprev), fixed2float(yprev));
    initial_run[0] = ~data[0];	/* Force intial setting */
    while (data < bufend) {
        dda_next(pnext.x);
        dda_next(pnext.y);
        xpos = dda_current(pnext.x);
        ypos = dda_current(pnext.y);

        if (memcmp(prev, data, spp) != 0)
        {
            /* This needs to be sped up */
            for (k = 0; k < spp; k++) {
                conc[k] = gx_color_value_from_byte(data[k]);
            }
            mapper(cmapper);
        }
        /* Fill the region between */
        /* xprev/yprev and xpos/ypos */
        /* Parallelogram */
        code = (*dev_proc(dev, fill_parallelogram))
                    (dev, xprev, yprev, xpos - xprev, ypos - yprev, pdyx, pdyy,
                     &cmapper->devc, lop);
        xprev = xpos;
        yprev = ypos;
        if (code < 0)
            goto err;
        prev = data;
        data += spp;
    }
    return 1;
    /* Save position if error, in case we resume. */
err:
    /* Only set buffer[0] if we've managed to set prev to something valid. */
    if (prev != &initial_run[0]) buffer[0] = prev;
    return code;
}

static int
gx_default_transform_pixel_region_begin(gx_device *dev, int w, int h, int spp,
                             const gx_dda_fixed_point *pixels, const gx_dda_fixed_point *rows,
                             const gs_int_rect *clip, gs_logical_operation_t lop,
                             gx_default_transform_pixel_region_state_t **statep)
{
    gx_default_transform_pixel_region_state_t *state;
    gs_memory_t *mem = dev->memory->non_gc_memory;

    *statep = state = (gx_default_transform_pixel_region_state_t *)gs_alloc_bytes(mem, sizeof(gx_default_transform_pixel_region_state_t), "gx_default_transform_pixel_region_state_t");
    if (state == NULL)
        return gs_error_VMerror;
    state->mem = mem;
    state->rows = *rows;
    state->pixels = *pixels;
    state->clip = *clip;
    state->w = w;
    state->h = h;
    state->spp = spp;
    state->lop = lop;
    state->line = NULL;

    /* FIXME: Consider sheers here too. Probably happens rarely enough not to be worth it. */
    if (rows->x.step.dQ == 0 && rows->x.step.dR == 0 && pixels->y.step.dQ == 0 && pixels->y.step.dR == 0)
        state->posture = transform_pixel_region_portrait;
    else if (rows->y.step.dQ == 0 && rows->y.step.dR == 0 && pixels->x.step.dQ == 0 && pixels->x.step.dR == 0)
        state->posture = transform_pixel_region_landscape;
    else
        state->posture = transform_pixel_region_skew;

    if (state->posture == transform_pixel_region_portrait)
        state->render = transform_pixel_region_render_portrait;
    else if (state->posture == transform_pixel_region_landscape)
        state->render = transform_pixel_region_render_landscape;
    else
        state->render = transform_pixel_region_render_skew;

    return 0;
}

static void
step_to_next_line(gx_default_transform_pixel_region_state_t *state)
{
    fixed x = dda_current(state->rows.x);
    fixed y = dda_current(state->rows.y);

    dda_next(state->rows.x);
    dda_next(state->rows.y);
    x = dda_current(state->rows.x) - x;
    y = dda_current(state->rows.y) - y;
    dda_translate(state->pixels.x, x);
    dda_translate(state->pixels.y, y);
}

static int
gx_default_transform_pixel_region_data_needed(gx_device *dev, gx_default_transform_pixel_region_state_t *state)
{
    if (state->posture == transform_pixel_region_portrait) {
        int iy, ih;

        get_portrait_y_extent(state, &iy, &ih);

        if (iy + ih < state->clip.p.y || iy >= state->clip.q.y) {
            /* Skip this line. */
            step_to_next_line(state);
            return 0;
        }
    } else if (state->posture == transform_pixel_region_landscape) {
        int ix, iw;

        get_landscape_x_extent(state, &ix, &iw);

        if (ix + iw < state->clip.p.x || ix >= state->clip.q.x) {
            /* Skip this line. */
            step_to_next_line(state);
            return 0;
        }
    }

    return 1;
}

static int
gx_default_transform_pixel_region_process_data(gx_device *dev, gx_default_transform_pixel_region_state_t *state, const unsigned char **buffer, int data_x, gx_cmapper_t *cmapper, const gs_gstate *pgs)
{
    int ret = state->render(dev, state, buffer, data_x, cmapper, pgs);

    step_to_next_line(state);
    return ret;
}

static int
gx_default_transform_pixel_region_end(gx_device *dev, gx_default_transform_pixel_region_state_t *state)
{
    if (state) {
        gs_free_object(state->mem, state->line, "image line");
        gs_free_object(state->mem, state, "gx_default_transform_pixel_region_state_t");
    }
    return 0;
}

int
gx_default_transform_pixel_region(gx_device *dev,
                       transform_pixel_region_reason reason,
                       transform_pixel_region_data *data)
{
    gx_default_transform_pixel_region_state_t *state = (gx_default_transform_pixel_region_state_t *)data->state;

    switch (reason)
    {
    case transform_pixel_region_begin:
        return gx_default_transform_pixel_region_begin(dev, data->u.init.w, data->u.init.h, data->u.init.spp, data->u.init.pixels, data->u.init.rows, data->u.init.clip, data->u.init.lop, (gx_default_transform_pixel_region_state_t **)&data->state);
    case transform_pixel_region_data_needed:
        return gx_default_transform_pixel_region_data_needed(dev, state);
    case transform_pixel_region_process_data:
        return gx_default_transform_pixel_region_process_data(dev, state, data->u.process_data.buffer, data->u.process_data.data_x, data->u.process_data.cmapper, data->u.process_data.pgs);
    case transform_pixel_region_end:
        data->state = NULL;
        return gx_default_transform_pixel_region_end(dev, state);
    default:
        return gs_error_unknownerror;
    }
}
