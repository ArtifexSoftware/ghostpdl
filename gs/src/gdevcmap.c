/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Special color mapping device */
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxlum.h"
#include "gdevcmap.h"

/*
 * The devices in this file exist only to implement the PCL5 special
 * color mapping algorithms.  They are not useful for PostScript.
 */

/* GC descriptor */
public_st_device_cmap();

/* Device procedures */
private dev_proc_map_rgb_color(cmap_map_rgb_color);
private dev_proc_map_rgb_alpha_color(cmap_map_rgb_alpha_color);
private dev_proc_begin_typed_image(cmap_begin_typed_image);

private const gx_device_cmap gs_cmap_device =
{
    std_device_dci_body(gx_device_cmap, 0, "special color mapper",
			0, 0, 1, 1,
			3, 24, 255, 255, 256, 256),
    {
	0, 0, 0, 0, 0,
	cmap_map_rgb_color,
	0,			/* map_color_rgb */
	gx_forward_fill_rectangle,
	gx_forward_tile_rectangle,
	gx_forward_copy_mono,
	gx_forward_copy_color,
	0, 0, 0, 0,
	gx_default_map_cmyk_color,
	0, 0,
	cmap_map_rgb_alpha_color,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
	gx_default_begin_image,
	0, 0, 0, 0, 0,
	cmap_begin_typed_image,
	0,
	0			/* map_color_rgb_alpha */
    },
    0,				/* target */
    device_cmap_identity
};

/* Initialize the device. */
int
gdev_cmap_init(gx_device_cmap * dev, gx_device * target,
	       gx_device_color_mapping_method_t method)
{
    int code;

    gx_device_init((gx_device *) dev, (const gx_device *)&gs_cmap_device,
		   target->memory, true);
    dev->target = target;
    gx_device_copy_params((gx_device *)dev, target);
    gx_device_forward_fill_in_procs((gx_device_forward *) dev);
    code = gdev_cmap_set_method(dev, method);
    if (code < 0)
	return code;
    return 0;
}

/* Set the color mapping method. */
int
gdev_cmap_set_method(gx_device_cmap * cmdev,
		     gx_device_color_mapping_method_t method)
{
    gx_device *target = cmdev->target;

    /*
     * If we're transforming the color, we may need to fool the graphics
     * core into not halftoning.
     */
    switch (method) {

	case device_cmap_identity:
	    cmdev->color_info.max_gray = target->color_info.max_gray;
	    cmdev->color_info.max_color = target->color_info.max_color;
            cmdev->color_info.num_components =
		target->color_info.num_components;
	    break;

	case device_cmap_monochrome:
	    cmdev->color_info.max_gray = target->color_info.max_gray;
	    cmdev->color_info.max_color = target->color_info.max_color;
            cmdev->color_info.num_components = 1;
	    break;

	case device_cmap_snap_to_primaries:
	case device_cmap_color_to_black_over_white:
	    cmdev->color_info.max_gray = cmdev->color_info.max_color = 4095;
            cmdev->color_info.num_components =
		target->color_info.num_components;
	    break;

	default:
	    return_error(gs_error_rangecheck);
    }
    cmdev->mapping_method = method;
    return 0;
}

/* Map RGB colors. */
typedef struct cmap_rgb_s {
    gx_color_value red, green, blue;
} cmap_rgb_t;
private void
cmap_convert_rgb_color(const gx_device_cmap * cmdev, cmap_rgb_t * rgb)
{
    switch (cmdev->mapping_method) {

	case device_cmap_snap_to_primaries:
	    /* Snap each RGB primary component to 0 or 1 individually. */
	    rgb->red =
		(rgb->red <= gx_max_color_value / 2 ? 0 : gx_max_color_value);
	    rgb->green =
		(rgb->green <= gx_max_color_value / 2 ? 0 : gx_max_color_value);
	    rgb->blue =
		(rgb->blue <= gx_max_color_value / 2 ? 0 : gx_max_color_value);
	    break;

	case device_cmap_color_to_black_over_white:
	    /* Snap black to white, other colors to black. */
	    rgb->red = rgb->green = rgb->blue =
		((rgb->red | rgb->green | rgb->blue) == 0 ?
		 gx_max_color_value : 0);
	    break;

	case device_cmap_identity:
	case device_cmap_monochrome:
	default:
	    break;

    }
}
private gx_color_index
cmap_map_rgb_color(gx_device * dev, gx_color_value red,
		   gx_color_value green, gx_color_value blue)
{
    const gx_device_cmap *const cmdev = (const gx_device_cmap *)dev;
    gx_device *target = cmdev->target;
    cmap_rgb_t rgb;

    rgb.red = red;
    rgb.green = green;
    rgb.blue = blue;
    cmap_convert_rgb_color(cmdev, &rgb);
    return target->procs.map_rgb_color(target, rgb.red, rgb.green, rgb.blue);
}
private gx_color_index
cmap_map_rgb_alpha_color(gx_device * dev, gx_color_value red,
			 gx_color_value green, gx_color_value blue,
			 gx_color_value alpha)
{
    const gx_device_cmap *const cmdev = (const gx_device_cmap *)dev;
    gx_device *target = cmdev->target;
    cmap_rgb_t rgb;

    rgb.red = red;
    rgb.green = green;
    rgb.blue = blue;
    cmap_convert_rgb_color(cmdev, &rgb);
    return target->procs.map_rgb_alpha_color(target, rgb.red, rgb.green,
					     rgb.blue, alpha);
}

/*
 * Handle high-level images.  The only reason we do this, rather than simply
 * pass the operation to the target, is that the image still has to call the
 * cmap device to do its color mapping.  As presently implemented, this
 * disables any high-level implementation that the target may provide.
 */
private int
cmap_begin_typed_image(gx_device * dev,
		       const gs_imager_state * pis, const gs_matrix * pmat,
		   const gs_image_common_t * pic, const gs_int_rect * prect,
		       const gx_drawing_color * pdcolor,
		       const gx_clip_path * pcpath,
		       gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    const gx_device_cmap *const cmdev = (const gx_device_cmap *)dev;
    gx_device *target = cmdev->target;

    if (cmdev->mapping_method == device_cmap_identity)
	return (*dev_proc(target, begin_typed_image))
	    (target, pis, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
    return gx_default_begin_typed_image(dev, pis, pmat, pic, prect,
					pdcolor, pcpath, memory, pinfo);
}
