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

/* Hit detection device */
#include "std.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxdevice.h"

/* Define the value returned for a detected hit. */
const int gs_hit_detected = gs_error_hit_detected;

/*
 * Define a minimal device for insideness testing.
 * It returns e_hit whenever it is asked to actually paint any pixels.
 */
static dev_proc_fill_rectangle(hit_fill_rectangle);
static void
hit_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, fill_rectangle, hit_fill_rectangle);
    set_dev_proc(dev, composite, gx_non_imaging_composite);

    set_dev_proc(dev, map_rgb_color, gx_default_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_map_color_rgb);
    set_dev_proc(dev, map_cmyk_color, gx_default_map_cmyk_color);
    set_dev_proc(dev, get_page_device, gx_default_get_page_device);
    set_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
    set_dev_proc(dev, fill_path, gx_default_fill_path);
    set_dev_proc(dev, fill_trapezoid, gx_default_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, gx_default_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, gx_default_fill_triangle);
    set_dev_proc(dev, draw_thin_line, gx_default_draw_thin_line);
    set_dev_proc(dev, strip_tile_rectangle, gx_default_strip_tile_rectangle);
    set_dev_proc(dev, strip_copy_rop2, gx_default_strip_copy_rop2);
    set_dev_proc(dev, get_clipping_box, gx_get_largest_clipping_box);
    set_dev_proc(dev, begin_typed_image, gx_default_begin_typed_image);
}
const gx_device gs_hit_device = {
 std_device_std_body(gx_device, hit_initialize_device_procs, "hit detector",
                     0, 0, 1, 1)
};

/* Test for a hit when filling a rectangle. */
static int
hit_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                   gx_color_index color)
{
    if (w > 0 && h > 0)
        return_error(gs_error_hit_detected);
    return 0;
}
