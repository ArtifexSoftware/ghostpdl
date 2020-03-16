/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* Common code for subclassing devices */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gxdcolor.h"		/* for gx_device_black/white */
#include "gxiparam.h"		/* for image source size */
#include "gxgstate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"
#include "gxcmap.h"         /* color mapping procs */
#include "gsstype.h"
#include "gdevprn.h"
#include "gdevp14.h"        /* Needed to patch up the procs after compositor creation */
#include "gdevsclass.h"

/*
 * It would be nice if we could rewrite the clist handling to use this kind of device class chain
 * instead of the nasty hackery it currently utilises (stores the device procs for the existing
 * device in 'orig_procs' which is in the device structure) and overwrites the procs with its
 * own ones. The bbox forwarding device could also be rewritten this way and would probably then
 * be usable as a real forwarding device (last time I tried to do this for eps2write I was unable
 * to solve the problems with text enumerators).
 */

/* At first sight we should never have a method in a device structure which is NULL
 * because gx_device_fill_in_procs() should replace all the NULLs with default routines.
 * However, obselete routines, and a number of newer routines (especially those involving
 * transparency) don't get filled in. Its not obvious to me if this is deliberate or not,
 * but we'll be careful and check the subclassed device's method before trying to execute
 * it. Same for all the methods. NB the fill_rectangle method is deliberately not filled in
 * because that gets set up by gdev_prn_allocate_memory(). Isn't it great the way we do our
 * initialisation in lots of places?
 */

/* TODO make gx_device_fill_in_procs fill in *all* the procs, currently it doesn't.
 * this will mean declaring gx_default_ methods for the transparency methods, possibly
 * some others. Like a number of other default methods, these can simply return an error
 * which hopefuly will avoid us having to check for NULL device methods.
 * We also agreed to set the fill_rectangle method to a default as well (currently it explicitly
 * does not do this) and have gdev_prn_alloc_buffer check to see if the method is the default
 * before overwriting it, rather than the current check for NULL.
 */

/* More observations; method naems, we have text_begin, but begin_image.
 * The enumerator initialiser for images gx_image_enum_common_init doesn't initialise
 * the 'memory' member variable. The text enumerator initialiser gs_text_enum_init does.
 * The default text enum init routine increments the reference count of the device, but the image enumerator
 * doesn't.
 */

/* We have a device method for 'get_profile' but we don't have one for 'set_profile' which causes some
 * problems, the 'set' simply sets the profile in the top device. This is modified in gsicc_set_device_profile
 * for now but really should have a method to itself.
 *
 * And in a delightful asymmetry, we have a set_graphics_type_tag, but no get_graphics_type_tag. Instead
 * (shudder) the code pulls the currently encoded colour and tag *directly* from the current device.
 * This means we have to copy the ENCODE_TAGS from the device we are subclassing, into the device which
 * is newly created at installation time. We also have to have our default set_graphics_type_tag method
 * update its graphics_type_tag, even though this device has no interest in it, just in case we happen
 * to be the top device in the chain......
 */

/*
 * gsdparam.c line 272 checks for method being NULL, this is bad, we should check for a return error
 * or default method and do initialisation based on that.
 */


/* For printing devices the 'open' routine in gdevprn calls gdevprn_allocate_memory
 * which is responsible for creating the page buffer. This *also* fills in some of
 * the device procs, in particular fill_rectangle() so its vitally important that
 * we pass this on.
 */
int default_subclass_open_device(gx_device *dev)
{
    int code = 0;

    /* observed with Bug 699794, don't set is_open = true if the open_device failed */
    /* and make sure to propagate the return code from the child device to caller.  */
    /* Only open the child if it was closed  and if child open is OK, return 1.     */
    /* (see gs_opendevice) */
    if (dev->child && dev->child->is_open == 0) {
        code = dev_proc(dev->child, open_device)(dev->child);
        if (code >= 0) {
            dev->child->is_open = true;
            code = 1;	/* device had been closed, but now is open */
        }
        gx_update_from_subclass(dev);	/* this is probably safe to do even if the open failed */
    }
    return code;
}

void default_subclass_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{
    if (dev->child)
        dev_proc(dev->child, get_initial_matrix)(dev->child, pmat);
    else
        gx_default_get_initial_matrix(dev, pmat);
    return;
}

int default_subclass_sync_output(gx_device *dev)
{
    if (dev->child)
        return dev_proc(dev->child, sync_output)(dev->child);
    else
        gx_default_sync_output(dev);

    return 0;
}

int default_subclass_output_page(gx_device *dev, int num_copies, int flush)
{
    if (dev->child)
        return dev_proc(dev->child, output_page)(dev->child, num_copies, flush);
    return 0;
}

int default_subclass_close_device(gx_device *dev)
{
    int code;

    if (dev->child) {
        code = dev_proc(dev->child, close_device)(dev->child);
        dev->is_open = dev->child->is_open = false;
        return code;
    }
    dev->is_open = false;
    return 0;
}

gx_color_index default_subclass_map_rgb_color(gx_device *dev, const gx_color_value cv[])
{
    if (dev->child)
        return dev_proc(dev->child, map_rgb_color)(dev->child, cv);
    else
        gx_error_encode_color(dev, cv);
    return 0;
}

int default_subclass_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{
    if (dev->child)
        return dev_proc(dev->child, map_color_rgb)(dev->child, color, rgb);
    else
        gx_default_map_color_rgb(dev, color, rgb);

    return 0;
}

int default_subclass_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    if (dev->child)
        return dev_proc(dev->child, fill_rectangle)(dev->child, x, y, width, height, color);
    return 0;
}

int default_subclass_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if (dev->child)
        return dev_proc(dev->child, tile_rectangle)(dev->child, tile, x, y, width, height, color0, color1, phase_x, phase_y);
    return 0;
}

int default_subclass_copy_mono(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1)
{
    if (dev->child)
        return dev_proc(dev->child, copy_mono)(dev->child, data, data_x, raster, id, x, y, width, height, color0, color1);
    return 0;
}

int default_subclass_copy_color(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height)
{
    if (dev->child)
        return dev_proc(dev->child, copy_color)(dev->child, data, data_x, raster, id, x, y, width, height);
    return 0;
}

int default_subclass_draw_line(gx_device *dev, int x0, int y0, int x1, int y1, gx_color_index color)
{
    if (dev->child)
        return dev_proc(dev->child, obsolete_draw_line)(dev->child, x0, y0, x1, y1, color);
    return 0;
}

int default_subclass_get_bits(gx_device *dev, int y, byte *data, byte **actual_data)
{
    if (dev->child)
        return dev_proc(dev->child, get_bits)(dev->child, y, data, actual_data);
    else
        return gx_default_get_bits(dev, y, data, actual_data);
    return 0;
}

int default_subclass_get_params(gx_device *dev, gs_param_list *plist)
{
    if (dev->child)
        return dev_proc(dev->child, get_params)(dev->child, plist);
    else
        return gx_default_get_params(dev, plist);

    return 0;
}

int default_subclass_put_params(gx_device *dev, gs_param_list *plist)
{
    int code;

    if (dev->child) {
        code = dev_proc(dev->child, put_params)(dev->child, plist);
        /* The child device might have closed itself (yes seriously, this can happen!) */
        dev->is_open = dev->child->is_open;
        gx_update_from_subclass(dev);
        return code;
    }
    else
        return gx_default_put_params(dev, plist);

    return 0;
}

gx_color_index default_subclass_map_cmyk_color(gx_device *dev, const gx_color_value cv[])
{
    if (dev->child)
        return dev_proc(dev->child, map_cmyk_color)(dev->child, cv);
    else
        return gx_default_map_cmyk_color(dev, cv);

    return 0;
}

const gx_xfont_procs *default_subclass_get_xfont_procs(gx_device *dev)
{
    if (dev->child)
        return dev_proc(dev->child, get_xfont_procs)(dev->child);
    else
        return gx_default_get_xfont_procs(dev);

    return 0;
}

gx_device *default_subclass_get_xfont_device(gx_device *dev)
{
    if (dev->child)
        return dev_proc(dev->child, get_xfont_device)(dev->child);
    else
        return gx_default_get_xfont_device(dev);

    return 0;
}

gx_color_index default_subclass_map_rgb_alpha_color(gx_device *dev, gx_color_value red, gx_color_value green, gx_color_value blue,
    gx_color_value alpha)
{
    if (dev->child) {
        return dev_proc(dev->child, map_rgb_alpha_color)(dev->child, red, green, blue, alpha);
    } else
        return gx_default_map_rgb_alpha_color(dev, red, green, blue, alpha);

    return 0;
}

gx_device *default_subclass_get_page_device(gx_device *dev)
{
    if (dev->child)
        return dev_proc(dev->child, get_page_device)(dev->child);
    else
        return gx_default_get_page_device(dev);

    return 0;
}

int default_subclass_get_alpha_bits(gx_device *dev, graphics_object_type type)
{
    if (dev->child)
        return dev_proc(dev->child, get_alpha_bits)(dev->child, type);
    return 0;
}

int default_subclass_copy_alpha(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    gx_color_index color, int depth)
{
    if (dev->child)
        return dev_proc(dev->child, copy_alpha)(dev->child, data, data_x, raster, id, x, y, width, height, color, depth);
    return 0;
}

int default_subclass_get_band(gx_device *dev, int y, int *band_start)
{
    if (dev->child)
        return dev_proc(dev->child, get_band)(dev->child, y, band_start);
    else
        return gx_default_get_band(dev, y, band_start);
    return 0;
}

int default_subclass_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_tile_bitmap *texture, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    if (dev->child) {
        return dev_proc(dev->child, copy_rop)(dev->child, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
    } else
        return gx_default_copy_rop(dev, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
    return 0;
}

int default_subclass_fill_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_path)(dev->child, pgs, ppath, params, pdcolor, pcpath);
    } else
        return gx_default_fill_path(dev, pgs, ppath, params, pdcolor, pcpath);

    return 0;
}

int default_subclass_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child) {
        return dev_proc(dev->child, stroke_path)(dev->child, pgs, ppath, params, pdcolor, pcpath);
    } else
        return gx_default_stroke_path(dev, pgs, ppath, params, pdcolor, pcpath);
    return 0;
}

int default_subclass_fill_mask(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_mask)(dev->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
    } else
        return gx_default_fill_mask(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
    return 0;
}

int default_subclass_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left, const gs_fixed_edge *right,
    fixed ybot, fixed ytop, bool swap_axes,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_trapezoid)(dev->child, left, right, ybot, ytop, swap_axes, pdcolor, lop);
    } else
        return gx_default_fill_trapezoid(dev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
    return 0;
}

int default_subclass_fill_parallelogram(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_parallelogram)(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
    } else
        return gx_default_fill_parallelogram(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    return 0;
}

int default_subclass_fill_triangle(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_triangle)(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
    } else
        return gx_default_fill_triangle(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    return 0;
}

int default_subclass_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,
    fixed adjustx, fixed adjusty)
{
    if (dev->child) {
        return dev_proc(dev->child, draw_thin_line)(dev->child, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
    } else
        return gx_default_draw_thin_line(dev, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
    return 0;
}

int default_subclass_begin_image(gx_device *dev, const gs_gstate *pgs, const gs_image_t *pim,
    gs_image_format_t format, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    if (dev->child) {
        return dev_proc(dev->child, begin_image)(dev->child, pgs, pim, format, prect, pdcolor, pcpath, memory, pinfo);
    } else
        return gx_default_begin_image(dev, pgs, pim, format, prect, pdcolor, pcpath, memory, pinfo);

    return 0;
}

int default_subclass_image_data(gx_device *dev, gx_image_enum_common_t *info, const byte **planes, int data_x,
    uint raster, int height)
{
    if (dev->child)
        return dev_proc(dev->child, image_data)(dev->child, info, planes, data_x, raster, height);
    return 0;
}

int default_subclass_end_image(gx_device *dev, gx_image_enum_common_t *info, bool draw_last)
{
    if (dev->child)
        return dev_proc(dev->child, end_image)(dev->child, info, draw_last);
    return 0;
}

int default_subclass_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if (dev->child) {
        return dev_proc(dev->child, strip_tile_rectangle)(dev->child, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    } else
        return gx_default_strip_tile_rectangle(dev, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    return 0;
}

int default_subclass_strip_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    if (dev->child) {
        return dev_proc(dev->child, strip_copy_rop)(dev->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
    } else
        return gx_default_strip_copy_rop(dev, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
    return 0;
}

void default_subclass_get_clipping_box(gx_device *dev, gs_fixed_rect *pbox)
{
    if (dev->child) {
        dev_proc(dev->child, get_clipping_box)(dev->child, pbox);
    } else
        gx_default_get_clipping_box(dev, pbox);

    return;
}

int default_subclass_begin_typed_image(gx_device *dev, const gs_gstate *pgs, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    if (dev->child) {
        return dev_proc(dev->child, begin_typed_image)(dev->child, pgs, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
    } else
        return gx_default_begin_typed_image(dev, pgs, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
    return 0;
}

int default_subclass_get_bits_rectangle(gx_device *dev, const gs_int_rect *prect,
    gs_get_bits_params_t *params, gs_int_rect **unread)
{
    if (dev->child) {
        return dev_proc(dev->child, get_bits_rectangle)(dev->child, prect, params, unread);
    } else
        return gx_default_get_bits_rectangle(dev, prect, params, unread);

    return 0;
}

int default_subclass_map_color_rgb_alpha(gx_device *dev, gx_color_index color, gx_color_value rgba[4])
{
    if (dev->child) {
        return dev_proc(dev->child, map_color_rgb_alpha)(dev->child, color, rgba);
    } else
        return gx_default_map_color_rgb_alpha(dev, color, rgba);

    return 0;
}

int default_subclass_create_compositor(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_gstate *pgs, gs_memory_t *memory, gx_device *cdev)
{
    default_subclass_subclass_data *psubclass_data = dev->subclass_data;
    int code;

    if (dev->child) {
        /* Some more unpleasantness here. If the child device is a clist, then it will use the first argument
         * that we pass to access its own data (not unreasonably), so we need to make sure we pass in the
         * child device. This has some follow on implications detailed below.
         */
        code = dev_proc(dev->child, create_compositor)(dev->child, pcdev, pcte, pgs, memory, cdev);
        if (code < 0)
            return code;

        if (*pcdev != 0L && *pcdev != dev->child){
            /* If the child created a new compositor, which it wants to be the new 'device' in the
             * graphics state, it sets it in the returned pcdev variable. When we return from this
             * method, if pcdev is not the same as the device in the graphics state then the interpreter
             * sets pcdev as the new device in the graphics state. But because we passed in the child device
             * to the child method, if it did create a compositor it will be a forwarding device, and it will
             * be forwarding to our child, we need it to point to us instead. So if pcdev is not the same as the
             * child device, we fixup the target in the child device to point to us.
             */
            gx_device_forward *fdev = (gx_device_forward *)*pcdev;

            if (fdev->target == dev->child) {
                if (gs_is_pdf14trans_compositor(pcte) != 0 && strncmp(fdev->dname, "pdf14clist", 10) == 0) {
                    pdf14_clist_device *p14dev;

                    p14dev = (pdf14_clist_device *)*pcdev;

                    dev->color_info = dev->child->color_info;

                    psubclass_data->saved_compositor_method = p14dev->procs.create_compositor;
                    psubclass_data->forwarding_dev = fdev;
                    p14dev->procs.create_compositor = gx_subclass_create_compositor;
                }

                fdev->target = dev;
                rc_decrement_only(dev->child, "first-last page compositor code");
                rc_increment(dev);
            }
            return_error(gs_error_handled);
        }
        else {
            /* See the 2 comments above. Now, if the child did not create a new compositor (eg its a clist)
             * then it returns pcdev pointing to the passed in device (the child in our case). Now this is a
             * problem, if we return with pcdev == child->dev, and teh current device is 'dev' then the
             * compositor code will think we wanted to push a new device and will select the child device.
             * so here if pcdev == dev->child we change it to be our own device, so that the calling code
             * won't redirect the device in the graphics state.
             */
            *pcdev = dev;
            return code;
        }
    }
    return 0;
}

int default_subclass_get_hardware_params(gx_device *dev, gs_param_list *plist)
{
    if (dev->child) {
        return dev_proc(dev->child, get_hardware_params)(dev->child, plist);
    } else
        return gx_default_get_hardware_params(dev, plist);

    return 0;
}

int default_subclass_text_begin(gx_device *dev, gs_gstate *pgs, const gs_text_params_t *text,
    gs_font *font, gx_path *path, const gx_device_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gs_text_enum_t **ppte)
{
    if (dev->child) {
        return dev_proc(dev->child, text_begin)(dev->child, pgs, text, font, path, pdcolor, pcpath, memory, ppte);
    } else
        return gx_default_text_begin(dev, pgs, text, font, path, pdcolor, pcpath, memory, ppte);

    return 0;
}

/* This method seems (despite the name) to be intended to allow for
 * devices to initialise data before being invoked. For our subclassed
 * device this should already have been done.
 */
int default_subclass_finish_copydevice(gx_device *dev, const gx_device *from_dev)
{
    return 0;
}

int default_subclass_begin_transparency_group(gx_device *dev, const gs_transparency_group_params_t *ptgp,
    const gs_rect *pbbox, gs_gstate *pgs, gs_memory_t *mem)
{
    if (dev->child)
        return dev_proc(dev->child, begin_transparency_group)(dev->child, ptgp, pbbox, pgs, mem);

    return 0;
}

int default_subclass_end_transparency_group(gx_device *dev, gs_gstate *pgs)
{
    if (dev->child)
        return dev_proc(dev->child, end_transparency_group)(dev->child, pgs);

    return 0;
}

int default_subclass_begin_transparency_mask(gx_device *dev, const gx_transparency_mask_params_t *ptmp,
    const gs_rect *pbbox, gs_gstate *pgs, gs_memory_t *mem)
{
    if (dev->child)
        return dev_proc(dev->child, begin_transparency_mask)(dev->child, ptmp, pbbox, pgs, mem);

    return 0;
}

int default_subclass_end_transparency_mask(gx_device *dev, gs_gstate *pgs)
{
    if (dev->child)
        return dev_proc(dev->child, end_transparency_mask)(dev->child, pgs);
    return 0;
}

int default_subclass_discard_transparency_layer(gx_device *dev, gs_gstate *pgs)
{
    if (dev->child)
        return dev_proc(dev->child, discard_transparency_layer)(dev->child, pgs);

    return 0;
}

const gx_cm_color_map_procs *default_subclass_get_color_mapping_procs(const gx_device *dev)
{
    if (dev->child) {
        return dev_proc(dev->child, get_color_mapping_procs)(dev->child);
    } else
        return gx_default_DevGray_get_color_mapping_procs(dev);

    return 0;
}

int  default_subclass_get_color_comp_index(gx_device *dev, const char * pname, int name_size, int component_type)
{
    if (dev->child) {
        return dev_proc(dev->child, get_color_comp_index)(dev->child, pname, name_size, component_type);
    } else
        return gx_error_get_color_comp_index(dev, pname, name_size, component_type);

    return 0;
}

gx_color_index default_subclass_encode_color(gx_device *dev, const gx_color_value colors[])
{
    if (dev->child) {
        return dev_proc(dev->child, encode_color)(dev->child, colors);
    } else
        return gx_error_encode_color(dev, colors);

    return 0;
}

int default_subclass_decode_color(gx_device *dev, gx_color_index cindex, gx_color_value colors[])
{
    if (dev->child)
        return dev_proc(dev->child, decode_color)(dev->child, cindex, colors);
    else {
        memset(colors, 0, sizeof(gx_color_value[GX_DEVICE_COLOR_MAX_COMPONENTS]));
    }

    return 0;
}

int default_subclass_pattern_manage(gx_device *dev, gx_bitmap_id id,
                gs_pattern1_instance_t *pinst, pattern_manage_t function)
{
    if (dev->child)
        return dev_proc(dev->child, pattern_manage)(dev->child, id, pinst, function);

    return 0;
}

int default_subclass_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
        const gs_gstate *pgs, const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child)
        return dev_proc(dev->child, fill_rectangle_hl_color)(dev->child, rect, pgs, pdcolor, pcpath);
    else
        return_error(gs_error_rangecheck);

    return 0;
}

int default_subclass_include_color_space(gx_device *dev, gs_color_space *cspace, const byte *res_name, int name_length)
{
    if (dev->child)
        return dev_proc(dev->child, include_color_space)(dev->child, cspace, res_name, name_length);

    return 0;
}

int default_subclass_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i, int j, int w, const frac31 *c0, const int32_t *c0_f, const int32_t *cg_num,
        int32_t cg_den)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_linear_color_scanline)(dev->child, fa, i, j, w, c0, c0_f, cg_num, cg_den);
    } else
        return gx_default_fill_linear_color_scanline(dev, fa, i, j, w, c0, c0_f, cg_num, cg_den);

    return 0;
}

int default_subclass_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_linear_color_trapezoid)(dev->child, fa, p0, p1, p2, p3, c0, c1, c2, c3);
    } else
        return gx_default_fill_linear_color_trapezoid(dev, fa, p0, p1, p2, p3, c0, c1, c2, c3);

    return 0;
}

int default_subclass_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    if (dev->child) {
        return dev_proc(dev->child, fill_linear_color_triangle)(dev->child, fa, p0, p1, p2, c0, c1, c2);
    } else
        return gx_default_fill_linear_color_triangle(dev, fa, p0, p1, p2, c0, c1, c2);

    return 0;
}

int default_subclass_update_spot_equivalent_colors(gx_device *dev, const gs_gstate * pgs)
{
    if (dev->child)
        return dev_proc(dev->child, update_spot_equivalent_colors)(dev->child, pgs);

    return 0;
}

gs_devn_params *default_subclass_ret_devn_params(gx_device *dev)
{
    if (dev->child)
        return dev_proc(dev->child, ret_devn_params)(dev->child);

    return 0;
}

int default_subclass_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
    if (dev->child) {
        return dev_proc(dev->child, fillpage)(dev->child, pgs, pdevc);
    } else
        return gx_default_fillpage(dev, pgs, pdevc);

    return 0;
}

int default_subclass_push_transparency_state(gx_device *dev, gs_gstate *pgs)
{
    if (dev->child)
        return dev_proc(dev->child, push_transparency_state)(dev->child, pgs);

    return 0;
}

int default_subclass_pop_transparency_state(gx_device *dev, gs_gstate *pgs)
{
    if (dev->child)
        return dev_proc(dev->child, pop_transparency_state)(dev->child, pgs);

    return 0;
}

int default_subclass_put_image(gx_device *dev, gx_device *mdev, const byte **buffers, int num_chan, int x, int y,
            int width, int height, int row_stride,
            int alpha_plane_index, int tag_plane_index)
{
    if (dev->child) {
        if (dev == mdev) {
            return dev_proc(dev->child, put_image)(dev->child, dev->child, buffers, num_chan, x, y, width, height, row_stride, alpha_plane_index, tag_plane_index);
        }
        else {
            return dev_proc(dev->child, put_image)(dev->child, mdev, buffers, num_chan, x, y, width, height, row_stride, alpha_plane_index, tag_plane_index);
        }
    }
    return 0;
}

int default_subclass_dev_spec_op(gx_device *dev, int op, void *data, int datasize)
{
    if (dev->child)
        return dev_proc(dev->child, dev_spec_op)(dev->child, op, data, datasize);

    return 0;
}

int default_subclass_copy_planes(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height, int plane_height)
{
    if (dev->child)
        return dev_proc(dev->child, copy_planes)(dev->child, data, data_x, raster, id, x, y, width, height, plane_height);

    return 0;
}

int default_subclass_get_profile(gx_device *dev, cmm_dev_profile_t **dev_profile)
{
    if (dev->child) {
        return dev_proc(dev->child, get_profile)(dev->child, dev_profile);
    }
    else {
        return gx_default_get_profile(dev, dev_profile);
    }

    return 0;
}

/* In a delightful asymmetry, we have a set_graphics_type_tag, but no get_graphics_type_tag. Instead
 * (shudder) the code pulls the currently encoded colour and tag *directly* from the current device.
 * This means we have to copy the ENCODE_TAGS from the device we are subclassing, into the device which
 * is newly created at installation time. We also have to have our default set_graphics_type_tag method
 * update its graphics_type_tag, even though this device has no interest in it, just in case we happen
 * to be the top device in the chain......
 */

void default_subclass_set_graphics_type_tag(gx_device *dev, gs_graphics_type_tag_t tag)
{
    /*
     * AIUI we should not be calling this method *unless* the ENCODE_TAGS bit is set, so we don't need
     * to do any checking. Just set the supplied tag in the current device, and pass it on to the underlying
     * device(s). This line is a direct copy from gx_default_set_graphics_type_tag.
     */
    dev->graphics_type_tag = (dev->graphics_type_tag & GS_DEVICE_ENCODES_TAGS) | tag;

    if (dev->child)
        dev_proc(dev->child, set_graphics_type_tag)(dev->child, tag);

    return;
}

int default_subclass_strip_copy_rop2(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors, const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height, int phase_x, int phase_y, gs_logical_operation_t lop, uint planar_height)
{
    if (!dev->child)
        return 0;

    return dev_proc(dev->child, strip_copy_rop2)(dev->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);
}

int default_subclass_strip_tile_rect_devn(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor0, const gx_drawing_color *pdcolor1, int phase_x, int phase_y)
{
    if (dev->child)
        return dev_proc(dev->child, strip_tile_rect_devn)(dev->child, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);
    else
        return gx_default_strip_tile_rect_devn(dev->child, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);

    return 0;
}

int default_subclass_copy_alpha_hl_color(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth)
{
    if (dev->child)
        return dev_proc(dev->child, copy_alpha_hl_color)(dev->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth);
    else
        return_error(gs_error_rangecheck);

    return 0;
}

int default_subclass_process_page(gx_device *dev, gx_process_page_options_t *options)
{
    if (dev->child)
        return dev_proc(dev->child, process_page)(dev->child, options);

    return 0;
}

int default_subclass_fill_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
        const gx_fill_params *fill_params, const gx_drawing_color *pdcolor_fill,
        const gx_stroke_params *stroke_params, const gx_drawing_color *pdcolor_stroke,
        const gx_clip_path *pcpath)
{
    if (dev->child)
        return dev_proc(dev->child, fill_stroke_path)(dev->child, pgs, ppath, fill_params, pdcolor_fill,
                                                      stroke_params, pdcolor_stroke, pcpath);
    return 0;
}

int default_subclass_transform_pixel_region(gx_device *dev, transform_pixel_region_reason reason, transform_pixel_region_data *data)
{
    if (dev->child)
        return dev_proc(dev->child, transform_pixel_region)(dev->child, reason, data);

    return gs_error_unknownerror;
}

void default_subclass_finalize(const gs_memory_t *cmem, void *vptr)
{
    gx_device * const dev = (gx_device *)vptr;
    generic_subclass_data *psubclass_data = (generic_subclass_data *)dev->subclass_data;
    (void)cmem; /* unused */

    discard(gs_closedevice(dev));

    if (dev->finalize)
        dev->finalize(dev);

    if (psubclass_data) {
        gs_free_object(dev->memory->non_gc_memory, psubclass_data, "gx_epo_finalize(suclass data)");
        dev->subclass_data = NULL;
    }
    if (dev->child) {
        gs_free_object(dev->memory->stable_memory, dev->child, "free child device memory for subclassing device");
    }
    if (dev->stype_is_dynamic)
        gs_free_const_object(dev->memory->non_gc_memory, dev->stype,
                             "default_subclass_finalize");
    if (dev->parent)
        dev->parent->child = dev->child;
    if (dev->child)
        dev->child->parent = dev->parent;
    if (dev->icc_struct)
        rc_decrement(dev->icc_struct, "finalize subclass device");
    if (dev->PageList)
        rc_decrement(dev->PageList, "finalize subclass device");
}
