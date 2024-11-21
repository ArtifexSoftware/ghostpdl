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
#include "gxdevsop.h"

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

/* More observations; method naems, we have text_begin, but begin_typed_image.
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
    /* else */
    return gx_default_sync_output(dev);
}

int default_subclass_output_page(gx_device *dev, int num_copies, int flush)
{
    int code = 0;

    if (dev->child) {
        code = dev_proc(dev->child, output_page)(dev->child, num_copies, flush);
        dev->PageCount = dev->child->PageCount;
        return code;
    }
    dev->PageCount += num_copies;	/* a minor lie */
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
    /* else */
    return gx_default_map_color_rgb(dev, color, rgb);
}

int default_subclass_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    if (dev->child)
        return dev_proc(dev->child, fill_rectangle)(dev->child, x, y, width, height, color);
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

int default_subclass_get_params(gx_device *dev, gs_param_list *plist)
{
    if (dev->child)
        return dev_proc(dev->child, get_params)(dev->child, plist);
    /* else */
    return gx_default_get_params(dev, plist);
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
    /* else */
    return gx_default_put_params(dev, plist);
}

gx_color_index default_subclass_map_cmyk_color(gx_device *dev, const gx_color_value cv[])
{
    if (dev->child)
        return dev_proc(dev->child, map_cmyk_color)(dev->child, cv);
    /* else */
    return gx_default_map_cmyk_color(dev, cv);
}

gx_device *default_subclass_get_page_device(gx_device *dev)
{
    if (dev->child)
        return dev_proc(dev->child, get_page_device)(dev->child);
    /* else */
    return gx_default_get_page_device(dev);
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

int default_subclass_fill_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child)
        return dev_proc(dev->child, fill_path)(dev->child, pgs, ppath, params, pdcolor, pcpath);
    /* else */
    return gx_default_fill_path(dev, pgs, ppath, params, pdcolor, pcpath);
}

int default_subclass_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child)
        return dev_proc(dev->child, stroke_path)(dev->child, pgs, ppath, params, pdcolor, pcpath);
    /* else */
    return gx_default_stroke_path(dev, pgs, ppath, params, pdcolor, pcpath);
}

int default_subclass_fill_mask(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    if (dev->child)
        return dev_proc(dev->child, fill_mask)(dev->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
    /* else */
    return gx_default_fill_mask(dev, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
}

int default_subclass_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left, const gs_fixed_edge *right,
    fixed ybot, fixed ytop, bool swap_axes,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child)
        return dev_proc(dev->child, fill_trapezoid)(dev->child, left, right, ybot, ytop, swap_axes, pdcolor, lop);
    /* else */
    return gx_default_fill_trapezoid(dev, left, right, ybot, ytop, swap_axes, pdcolor, lop);
}

int default_subclass_fill_parallelogram(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child)
        return dev_proc(dev->child, fill_parallelogram)(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
    /* else */
    return gx_default_fill_parallelogram(dev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int default_subclass_fill_triangle(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    if (dev->child)
        return dev_proc(dev->child, fill_triangle)(dev->child, px, py, ax, ay, bx, by, pdcolor, lop);
    /* else */
    return gx_default_fill_triangle(dev, px, py, ax, ay, bx, by, pdcolor, lop);
}

int default_subclass_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,
    fixed adjustx, fixed adjusty)
{
    if (dev->child)
        return dev_proc(dev->child, draw_thin_line)(dev->child, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
    /* else */
    return gx_default_draw_thin_line(dev, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
}

int default_subclass_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    if (dev->child)
        return dev_proc(dev->child, strip_tile_rectangle)(dev->child, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
    /* else */
    return gx_default_strip_tile_rectangle(dev, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
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
    if (dev->child)
        return dev_proc(dev->child, begin_typed_image)(dev->child, pgs, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
    /* else */
    return gx_default_begin_typed_image(dev, pgs, pmat, pic, prect, pdcolor, pcpath, memory, pinfo);
}

int default_subclass_get_bits_rectangle(gx_device *dev, const gs_int_rect *prect,
    gs_get_bits_params_t *params)
{
    if (dev->child)
        return dev_proc(dev->child, get_bits_rectangle)(dev->child, prect, params);
    /* else */
    return gx_default_get_bits_rectangle(dev, prect, params);
}

static void subclass_composite_front_finalize(gx_device *dev)
{
    generic_subclass_data *psubclass_data = (generic_subclass_data *)dev->parent->subclass_data;

    dev->parent->child = psubclass_data->pre_composite_device;
    psubclass_data->saved_finalize_method(dev);
}

int default_subclass_composite_front(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_gstate *pgs, gs_memory_t *memory, gx_device *cdev)
{
    int code = 0;
    gs_pdf14trans_t *pct = (gs_pdf14trans_t *)pcte;
    generic_subclass_data *psubclass_data = (generic_subclass_data *)dev->subclass_data;
    gx_device *thisdev = dev;

    if (dev->child) {
        code = dev_proc(dev->child, composite)(dev->child, pcdev, pcte, pgs, memory, cdev);
        if (code < 0)
            return code;

        if (gs_is_pdf14trans_compositor(pcte)) {
            switch(pct->params.pdf14_op)
            {
                case PDF14_POP_DEVICE:
                    if (psubclass_data->pre_composite_device != NULL) {
                        if (dev->child) {
                            dev->child->parent = NULL;
                            dev->child->child = NULL;
                            dev->child->finalize = psubclass_data->saved_finalize_method;
                            rc_decrement(dev->child, "default_subclass_composite_front");
                        }
                        dev->child = psubclass_data->pre_composite_device;
                        psubclass_data->pre_composite_device = NULL;
                        psubclass_data->saved_finalize_method = NULL;
                        while (dev) {
                            memcpy(&(dev->color_info), &(dev->child->color_info), sizeof(gx_device_color_info));
                            dev->num_planar_planes = dev->child->num_planar_planes;
                            dev = dev->parent;
                        }
                    }
                    break;
                case PDF14_PUSH_DEVICE:
                    /* *pcdev is always returned containing a device capable of doing
                     * compositing. This may mean it is a new device. If this wants
                     * to be the new 'device' in the graphics state, then code will
                     * return as 1. */
                    if (code == 1) {
                        /* We want this device to stay ahead of the compositor; the newly created compositor has
                         * inserted itself in front of our child device, so basically we want to replace
                         * our current child with the newly created compositor. I hope !
                         */
                        psubclass_data = (generic_subclass_data *)dev->subclass_data;
                        if (psubclass_data == NULL)
                            return_error(gs_error_undefined);
                        psubclass_data->pre_composite_device = dev->child;
                        psubclass_data->saved_finalize_method = (*pcdev)->finalize;
                        (*pcdev)->finalize = subclass_composite_front_finalize;

                        (*pcdev)->child = dev->child;
                        dev->child = *pcdev;
                        (*pcdev)->parent = dev;
                        while (dev) {
                            memcpy(&dev->color_info, &(*pcdev)->color_info, sizeof(gx_device_color_info));
                            dev->num_planar_planes = dev->child->num_planar_planes;
                            dev = dev->parent;
                        }
                    }
                    break;
                default:
                    /* It seems like many operations can result in the pdf14 device altering its color
                     * info, presumably as we push different blending spaces. Ick. In order to stay in sync
                     * any time we have inserted a compositor after this class, we must update the color info
                     * of this device after every operation, in case it changes....
                     */
                    if (psubclass_data->pre_composite_device != NULL) {
                        while (dev) {
                            memcpy(&(dev->color_info), &(dev->child->color_info), sizeof(gx_device_color_info));
                            dev->num_planar_planes = dev->child->num_planar_planes;
                            dev = dev->parent;
                        }
                    }
                    break;
            }

        }
        /* We are inserting the compositor code after this device, or the compositor
         * did not create a new compositor. Either way we don't want the compositor code
         * to think we want to push a new device, so just return this device to the caller.
         */
        *pcdev = thisdev;
        return 0;
    }
    return 0;
}

int default_subclass_composite(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_gstate *pgs, gs_memory_t *memory, gx_device *cdev)
{
    int code;

    if (dev->child) {
        /* Some more unpleasantness here. If the child device is a clist, then it will use the first argument
         * that we pass to access its own data (not unreasonably), so we need to make sure we pass in the
         * child device. This has some follow on implications detailed below.
         */
        code = dev_proc(dev->child, composite)(dev->child, pcdev, pcte, pgs, memory, cdev);
        if (code < 0)
            return code;

        /* *pcdev is always returned containing a device capable of doing
         * compositing. This may mean it is a new device. If this wants
         * to be the new 'device' in the graphics state, then code will
         * return as 1. */
        if (code == 1) {
            /* The device chain on entry to this function was:
             *   dev(the subclassing device) -> child.
             * But now we also have:
             *   *pcdev -> child.
             * Or in some cases:
             *   *pcdev (-> other device)* -> child
             * Most callers would be happy to make dev->child = *pcdev,
             * thus giving us:
             *   dev -> *pcdev (-> other device)* ->child
             * Unfortunately, we are not happy with that. We need to
             * remain tightly bound to the child. i.e. we are aiming for:
             *   *pcdev (-> other device)* -> dev -> child
             * Accordingly, we need to move ourselves within the device
             * chain.
             */
            gx_device *penult = *pcdev;

            if (penult == NULL) {
                /* This should never happen. */
                return gs_error_unknownerror;
            }

            /* Find the penultimate device. */
            while (1) {
                gxdso_device_child_request req;
                req.target = penult;
                req.n = 0;
                code = dev_proc(penult, dev_spec_op)(penult, gxdso_device_child, &req, sizeof(req));
                if (code < 0)
                    return code;
                if (req.target == NULL) {
                    /* Wooah! Where was dev->child? */
                    return gs_error_unknownerror;
                }
                if (req.target == dev->child)
                    break; /* penult is the parent. */
                penult = req.target;
            }

            if (penult == NULL) {
                /* This should never happen. We know that we've just
                 * had a compositor inserted before dev->child, so there
                 * really ought to be one! */
                return gs_error_unknownerror;
            }

            /* We already point to dev->child, and hence own a reference
             * to it. */

            /* Now insert ourselves as the child of the penultimate one. */
            code = dev_proc(penult, dev_spec_op)(penult, gxdso_device_insert_child, dev, 0);
            if (code < 0)
                return code;

            /* Now we want our caller to update itself to recognise that
             * *pcdev should be its child, not dev. So we return 1. */
            return 1;
        }
        else {
            /* See the 2 comments above. Now, if the child did not create a new compositor (eg its a clist)
             * then it returns pcdev pointing to the passed in device (the child in our case). Now this is a
             * problem, if we return with pcdev == child->dev, and the current device is 'dev' then the
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
    if (dev->child)
        return dev_proc(dev->child, get_hardware_params)(dev->child, plist);
    /* else */
    return gx_default_get_hardware_params(dev, plist);
}

int default_subclass_text_begin(gx_device *dev, gs_gstate *pgs, const gs_text_params_t *text,
    gs_font *font, const gx_clip_path *pcpath,
    gs_text_enum_t **ppte)
{
    if (dev->child)
        return dev_proc(dev->child, text_begin)(dev->child, pgs, text, font, pcpath, ppte);
    /* else */
    return gx_default_text_begin(dev, pgs, text, font, pcpath, ppte);
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

const gx_cm_color_map_procs *default_subclass_get_color_mapping_procs(const gx_device *dev,
                                                                      const gx_device **tdev)
{
    if (dev->child)
        return dev_proc(dev->child, get_color_mapping_procs)(dev->child, tdev);
    /* else */
    return gx_default_DevGray_get_color_mapping_procs(dev, tdev);
}

int  default_subclass_get_color_comp_index(gx_device *dev, const char * pname, int name_size, int component_type)
{
    if (dev->child)
        return dev_proc(dev->child, get_color_comp_index)(dev->child, pname, name_size, component_type);
    /* else */
    return gx_error_get_color_comp_index(dev, pname, name_size, component_type);
}

gx_color_index default_subclass_encode_color(gx_device *dev, const gx_color_value colors[])
{
    if (dev->child)
        return dev_proc(dev->child, encode_color)(dev->child, colors);
    /* else */
    return gx_error_encode_color(dev, colors);
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

int default_subclass_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
        const gs_gstate *pgs, const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    if (dev->child)
        return dev_proc(dev->child, fill_rectangle_hl_color)(dev->child, rect, pgs, pdcolor, pcpath);
    /* else */
    return_error(gs_error_rangecheck);
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
    if (dev->child)
        return dev_proc(dev->child, fill_linear_color_scanline)(dev->child, fa, i, j, w, c0, c0_f, cg_num, cg_den);
    /* else */
    return gx_default_fill_linear_color_scanline(dev, fa, i, j, w, c0, c0_f, cg_num, cg_den);
}

int default_subclass_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    if (dev->child)
        return dev_proc(dev->child, fill_linear_color_trapezoid)(dev->child, fa, p0, p1, p2, p3, c0, c1, c2, c3);
    /* else */
    return gx_default_fill_linear_color_trapezoid(dev, fa, p0, p1, p2, p3, c0, c1, c2, c3);
}

int default_subclass_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    if (dev->child)
        return dev_proc(dev->child, fill_linear_color_triangle)(dev->child, fa, p0, p1, p2, c0, c1, c2);
    /* else */
    return gx_default_fill_linear_color_triangle(dev, fa, p0, p1, p2, c0, c1, c2);
}

int default_subclass_update_spot_equivalent_colors(gx_device *dev, const gs_gstate * pgs, const gs_color_space *pcs)
{
    if (dev->child)
        return dev_proc(dev->child, update_spot_equivalent_colors)(dev->child, pgs, pcs);

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
    if (dev->child)
        return dev_proc(dev->child, fillpage)(dev->child, pgs, pdevc);
    /* else */
    return gx_default_fillpage(dev, pgs, pdevc);
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
    if (op == gxdso_is_clist_device)
        return 0;
    if (op == gxdso_device_child) {
        gxdso_device_child_request *d = (gxdso_device_child_request *)data;
        if (d->target == dev) {
            d->target = dev->child;
            return 1;
        }
    }
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

int default_subclass_get_profile(const gx_device *dev, cmm_dev_profile_t **dev_profile)
{
    if (dev->child) {
        return dev_proc(dev->child, get_profile)(dev->child, dev_profile);
    }
    /* else */
    return gx_default_get_profile(dev, dev_profile);
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
    /* else */
    return gx_default_strip_tile_rect_devn(dev, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);
}

int default_subclass_copy_alpha_hl_color(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth)
{
    if (dev->child)
        return dev_proc(dev->child, copy_alpha_hl_color)(dev->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth);
    /* else */
    return_error(gs_error_rangecheck);
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

int default_subclass_lock_pattern(gx_device *dev, gs_gstate *pgs, gs_id pattern_id, int lock)
{
    if (dev->child)
        return dev_proc(dev->child, lock_pattern)(dev->child, pgs, pattern_id, lock);
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

    /* The only way we should get here is when the original device
     * should be freed (because the subclassing device is pretending
     * to be the original device). That being the case, all the child
     * devices should have a reference count of 1 (referenced only by
     * their parent). Anything else is an error.
     */
    if (dev->child != NULL) {
        if (dev->child->rc.ref_count != 1) {
            dmprintf(dev->memory, "Error: finalizing subclassing device while child refcount > 1\n");
            while (dev->child->rc.ref_count != 1)
                rc_decrement_only(dev->child, "de-reference child device");
        }
        rc_decrement(dev->child, "de-reference child device");
    }

    if (psubclass_data) {
        gs_free_object(dev->memory->non_gc_memory, psubclass_data, "gx_epo_finalize(suclass data)");
        dev->subclass_data = NULL;
    }
    if (dev->stype_is_dynamic)
        gs_free_const_object(dev->memory->non_gc_memory, dev->stype,
                             "default_subclass_finalize");
    if (dev->icc_struct)
        rc_decrement(dev->icc_struct, "finalize subclass device");
    if (dev->PageList)
        rc_decrement(dev->PageList, "finalize subclass device");
    if (dev->NupControl)
        rc_decrement(dev->NupControl, "finalize subclass device");
}

void default_subclass_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, default_subclass_open_device);
    set_dev_proc(dev, get_initial_matrix, default_subclass_get_initial_matrix);
    set_dev_proc(dev, sync_output, default_subclass_sync_output);
    set_dev_proc(dev, output_page, default_subclass_output_page);
    set_dev_proc(dev, close_device, default_subclass_close_device);
    set_dev_proc(dev, map_rgb_color, default_subclass_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, default_subclass_map_color_rgb);
    set_dev_proc(dev, fill_rectangle, default_subclass_fill_rectangle);
    set_dev_proc(dev, copy_mono, default_subclass_copy_mono);
    set_dev_proc(dev, copy_color, default_subclass_copy_color);
    set_dev_proc(dev, get_params, default_subclass_get_params);
    set_dev_proc(dev, put_params, default_subclass_put_params);
    set_dev_proc(dev, map_cmyk_color, default_subclass_map_cmyk_color);
    set_dev_proc(dev, get_page_device, default_subclass_get_page_device);
    set_dev_proc(dev, get_alpha_bits, default_subclass_get_alpha_bits);
    set_dev_proc(dev, copy_alpha, default_subclass_copy_alpha);
    set_dev_proc(dev, fill_path, default_subclass_fill_path);
    set_dev_proc(dev, stroke_path, default_subclass_stroke_path);
    set_dev_proc(dev, fill_mask, default_subclass_fill_mask);
    set_dev_proc(dev, fill_trapezoid, default_subclass_fill_trapezoid);
    set_dev_proc(dev, fill_parallelogram, default_subclass_fill_parallelogram);
    set_dev_proc(dev, fill_triangle, default_subclass_fill_triangle);
    set_dev_proc(dev, draw_thin_line, default_subclass_draw_thin_line);
    set_dev_proc(dev, strip_tile_rectangle, default_subclass_strip_tile_rectangle);
    set_dev_proc(dev, get_clipping_box, default_subclass_get_clipping_box);
    set_dev_proc(dev, begin_typed_image, default_subclass_begin_typed_image);
    set_dev_proc(dev, get_bits_rectangle, default_subclass_get_bits_rectangle);
    set_dev_proc(dev, composite, default_subclass_composite);
    set_dev_proc(dev, get_hardware_params, default_subclass_get_hardware_params);
    set_dev_proc(dev, text_begin, default_subclass_text_begin);
    set_dev_proc(dev, begin_transparency_group, default_subclass_begin_transparency_group);
    set_dev_proc(dev, end_transparency_group, default_subclass_end_transparency_group);
    set_dev_proc(dev, begin_transparency_mask, default_subclass_begin_transparency_mask);
    set_dev_proc(dev, end_transparency_mask, default_subclass_end_transparency_mask);
    set_dev_proc(dev, discard_transparency_layer, default_subclass_discard_transparency_layer);
    set_dev_proc(dev, get_color_mapping_procs, default_subclass_get_color_mapping_procs);
    set_dev_proc(dev, get_color_comp_index, default_subclass_get_color_comp_index);
    set_dev_proc(dev, encode_color, default_subclass_encode_color);
    set_dev_proc(dev, decode_color, default_subclass_decode_color);
    set_dev_proc(dev, fill_rectangle_hl_color, default_subclass_fill_rectangle_hl_color);
    set_dev_proc(dev, include_color_space, default_subclass_include_color_space);
    set_dev_proc(dev, fill_linear_color_scanline, default_subclass_fill_linear_color_scanline);
    set_dev_proc(dev, fill_linear_color_trapezoid, default_subclass_fill_linear_color_trapezoid);
    set_dev_proc(dev, fill_linear_color_triangle, default_subclass_fill_linear_color_triangle);
    set_dev_proc(dev, update_spot_equivalent_colors, default_subclass_update_spot_equivalent_colors);
    set_dev_proc(dev, ret_devn_params, default_subclass_ret_devn_params);
    set_dev_proc(dev, fillpage, default_subclass_fillpage);
    set_dev_proc(dev, push_transparency_state, default_subclass_push_transparency_state);
    set_dev_proc(dev, pop_transparency_state, default_subclass_pop_transparency_state);
    set_dev_proc(dev, put_image, default_subclass_put_image);
    set_dev_proc(dev, dev_spec_op, default_subclass_dev_spec_op);
    set_dev_proc(dev, copy_planes, default_subclass_copy_planes);
    set_dev_proc(dev, get_profile, default_subclass_get_profile);
    set_dev_proc(dev, set_graphics_type_tag, default_subclass_set_graphics_type_tag);
    set_dev_proc(dev, strip_copy_rop2, default_subclass_strip_copy_rop2);
    set_dev_proc(dev, strip_tile_rect_devn, default_subclass_strip_tile_rect_devn);
    set_dev_proc(dev, copy_alpha_hl_color, default_subclass_copy_alpha_hl_color);
    set_dev_proc(dev, process_page, default_subclass_process_page);
    set_dev_proc(dev, transform_pixel_region, default_subclass_transform_pixel_region);
    set_dev_proc(dev, fill_stroke_path, default_subclass_fill_stroke_path);
    set_dev_proc(dev, lock_pattern, default_subclass_lock_pattern);
}

int
default_subclass_install(gx_device *dev, gs_gstate *pgs)
{
    dev = dev->child;
    return dev->page_procs.install(dev, pgs);
}

int
default_subclass_begin_page(gx_device *dev, gs_gstate *pgs)
{
    dev = dev->child;
    return dev->page_procs.begin_page(dev, pgs);
}

int
default_subclass_end_page(gx_device *dev, int reason, gs_gstate *pgs)
{
    dev = dev->child;
    return dev->page_procs.end_page(dev, reason, pgs);
}

void gx_subclass_fill_in_page_procs(gx_device *dev)
{
    if (dev->page_procs.install == NULL)
        dev->page_procs.install = default_subclass_install;
    if (dev->page_procs.begin_page == NULL)
        dev->page_procs.begin_page = default_subclass_begin_page;
    if (dev->page_procs.end_page == NULL)
        dev->page_procs.end_page = default_subclass_end_page;
}
