/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* Device for first/last page handling device */
/* This device is the first 'subclassing' device; the intention of subclassing
 * is to allow us to develop a 'chain' or 'pipeline' of devices, each of which
 * can process some aspect of the graphics methods before passing them on to the
 * next device in the chain.
 *
 * This device's purpose is to implement the 'FirstPage' and 'LastPage' parameters
 * in Ghostscript. Initially only implemented in the PDF interpreter this functionality
 * has been shifted internally so that it can be implemented in all the interpreters.
 * The approach is pretty simple, we modify gdevprn.c and gdevvec.c so that if -dFirstPage
 * or -dLastPage is defined the device in question is subclassed and this device inserted.
 * This device then 'black hole's any graphics operations until we reach 'FirstPage'. We then
 * allow graphics to pass to the device until we reach the end of 'LastPage' at which time we
 * discard operations again until we reach the end, and close the device.
 */

/* This seems to be broadly similar to the 'device filter stack' which is defined in gsdfilt.c
 * and the stack for which is defined in the graphics state (dfilter_stack) but which seems to be
 * completely unused. We should probably remove dfilter_stack from the graphics state and remove
 * gsdfilt.c from the build.
 *
 * It would be nice if we could rewrite the clist handling to use this kind of device class chain
 * instead of the nasty hackery it currently utilises (stores the device procs for the existing
 * device in 'orig_procs' which is in the device structure) and overwrites the procs with its
 * own ones. The bbox forwarding device could also be rewritten this way and would probably then
 * be usable as a real forwarding device (last time I tried to do this for eps2write I was unable
 * to solve the problems with text enumerators).
 */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gdevflp.h"
#include "gxdcolor.h"		/* for gx_device_black/white */
#include "gxiparam.h"		/* for image source size */
#include "gxistate.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxcpath.h"
#include "gxcmap.h"         /* color mapping procs */
#include "gsstype.h"

#include "gdevflp.h"

/* GC descriptor */
public_st_device_flp();
/* we need text and image enumerators, because of the crazy way that text and images work */
private_st_flp_text_enum();

/* Device procedures, we need to implement all of them */
static dev_proc_open_device(flp_open_device);
static dev_proc_get_initial_matrix(flp_get_initial_matrix);
static dev_proc_sync_output(flp_sync_output);
static dev_proc_output_page(flp_output_page);
static dev_proc_close_device(flp_close_device);
static dev_proc_map_rgb_color(flp_map_rgb_color);
static dev_proc_map_color_rgb(flp_map_color_rgb);
static dev_proc_fill_rectangle(flp_fill_rectangle);
static dev_proc_tile_rectangle(flp_tile_rectangle);
static dev_proc_copy_mono(flp_copy_mono);
static dev_proc_copy_color(flp_copy_color);
static dev_proc_draw_line(flp_draw_line);
static dev_proc_get_bits(flp_get_bits);
static dev_proc_get_params(flp_get_params);
static dev_proc_put_params(flp_put_params);
static dev_proc_map_cmyk_color(flp_map_cmyk_color);
static dev_proc_get_xfont_procs(flp_get_xfont_procs);
static dev_proc_get_xfont_device(flp_get_xfont_device);
static dev_proc_map_rgb_alpha_color(flp_map_rgb_alpha_color);
static dev_proc_get_page_device(flp_get_page_device);
static dev_proc_get_alpha_bits(flp_get_alpha_bits);
static dev_proc_copy_alpha(flp_copy_alpha);
static dev_proc_get_band(flp_get_band);
static dev_proc_copy_rop(flp_copy_rop);
static dev_proc_fill_path(flp_fill_path);
static dev_proc_stroke_path(flp_stroke_path);
static dev_proc_fill_mask(flp_fill_mask);
static dev_proc_fill_trapezoid(flp_fill_trapezoid);
static dev_proc_fill_parallelogram(flp_fill_parallelogram);
static dev_proc_fill_triangle(flp_fill_triangle);
static dev_proc_draw_thin_line(flp_draw_thin_line);
static dev_proc_begin_image(flp_begin_image);
static dev_proc_image_data(flp_image_data);
static dev_proc_end_image(flp_end_image);
static dev_proc_strip_tile_rectangle(flp_strip_tile_rectangle);
static dev_proc_strip_copy_rop(flp_strip_copy_rop);
static dev_proc_get_clipping_box(flp_get_clipping_box);
static dev_proc_begin_typed_image(flp_begin_typed_image);
static dev_proc_get_bits_rectangle(flp_get_bits_rectangle);
static dev_proc_map_color_rgb_alpha(flp_map_color_rgb_alpha);
static dev_proc_create_compositor(flp_create_compositor);
static dev_proc_get_hardware_params(flp_get_hardware_params);
static dev_proc_text_begin(flp_text_begin);
static dev_proc_finish_copydevice(flp_finish_copydevice);
static dev_proc_begin_transparency_group(flp_begin_transparency_group);
static dev_proc_end_transparency_group(flp_end_transparency_group);
static dev_proc_begin_transparency_mask(flp_begin_transparency_mask);
static dev_proc_end_transparency_mask(flp_end_transparency_mask);
static dev_proc_discard_transparency_layer(flp_discard_transparency_layer);
static dev_proc_get_color_mapping_procs(flp_get_color_mapping_procs);
static dev_proc_get_color_comp_index(flp_get_color_comp_index);
static dev_proc_encode_color(flp_encode_color);
static dev_proc_decode_color(flp_decode_color);
static dev_proc_pattern_manage(flp_pattern_manage);
static dev_proc_fill_rectangle_hl_color(flp_fill_rectangle_hl_color);
static dev_proc_include_color_space(flp_include_color_space);
static dev_proc_fill_linear_color_scanline(flp_fill_linear_color_scanline);
static dev_proc_fill_linear_color_trapezoid(flp_fill_linear_color_trapezoid);
static dev_proc_fill_linear_color_triangle(flp_fill_linear_color_triangle);
static dev_proc_update_spot_equivalent_colors(flp_update_spot_equivalent_colors);
static dev_proc_ret_devn_params(flp_ret_devn_params);
static dev_proc_fillpage(flp_fillpage);
static dev_proc_push_transparency_state(flp_push_transparency_state);
static dev_proc_pop_transparency_state(flp_pop_transparency_state);
static dev_proc_put_image(flp_put_image);
static dev_proc_dev_spec_op(flp_dev_spec_op);
static dev_proc_copy_planes(flp_copy_planes);
static dev_proc_get_profile(flp_get_profile);
static dev_proc_set_graphics_type_tag(flp_set_graphics_type_tag);
static dev_proc_strip_copy_rop2(flp_strip_copy_rop2);
static dev_proc_strip_tile_rect_devn(flp_strip_tile_rect_devn);
static dev_proc_copy_alpha_hl_color(flp_copy_alpha_hl_color);
static dev_proc_process_page(flp_process_page);

static void
flp_finalize(gx_device *dev);

/* The device prototype */
#define MAX_COORD (max_int_in_fixed - 1000)
#define MAX_RESOLUTION 4000

const
gx_device_flp gs_flp_device =
{
    sizeof(gx_device_flp),  /* params _size */\
    0,                      /* obselete static_procs */\
    "first_lastpage",       /* dname */\
    0,                      /* memory */\
    0,              /* stype */\
    0,              /* is_dynamic (always false for a prototype, will be true for an allocated device instance) */\
    flp_finalize,   /* The reason why we can't use the std_device_dci_body macro, we want a finalize */\
    {0},              /* rc */\
    0,              /* retained */\
    0,              /* subclass_data */\
    0,              /* is_open */\
    0,              /* max_fill_band */\
    dci_alpha_values(1, 8, 255, 0, 256, 1, 1, 1),\
    std_device_part2_(MAX_COORD, MAX_COORD, MAX_RESOLUTION, MAX_RESOLUTION),\
    offset_margin_values(0, 0, 0, 0, 0, 0),\
    std_device_part3_(),
#if 0
    /*
     * Define the device as 8-bit gray scale to avoid computing halftones.
     */
    std_device_dci_body(gx_device_flp, 0, "first_lastpage",
                        MAX_COORD, MAX_COORD,
                        MAX_RESOLUTION, MAX_RESOLUTION,
                        1, 8, 255, 0, 256, 1),
#endif
    {flp_open_device,
     flp_get_initial_matrix,
     flp_sync_output,			/* sync_output */
     flp_output_page,
     flp_close_device,
     flp_map_rgb_color,
     flp_map_color_rgb,
     flp_fill_rectangle,
     flp_tile_rectangle,			/* tile_rectangle */
     flp_copy_mono,
     flp_copy_color,
     flp_draw_line,			/* draw_line */
     flp_get_bits,			/* get_bits */
     flp_get_params,
     flp_put_params,
     flp_map_cmyk_color,
     flp_get_xfont_procs,			/* get_xfont_procs */
     flp_get_xfont_device,			/* get_xfont_device */
     flp_map_rgb_alpha_color,
     flp_get_page_device,
     flp_get_alpha_bits,			/* get_alpha_bits */
     flp_copy_alpha,
     flp_get_band,			/* get_band */
     flp_copy_rop,			/* copy_rop */
     flp_fill_path,
     flp_stroke_path,
     flp_fill_mask,
     flp_fill_trapezoid,
     flp_fill_parallelogram,
     flp_fill_triangle,
     flp_draw_thin_line,
     flp_begin_image,
     flp_image_data,			/* image_data */
     flp_end_image,			/* end_image */
     flp_strip_tile_rectangle,
     flp_strip_copy_rop,
     flp_get_clipping_box,			/* get_clipping_box */
     flp_begin_typed_image,
     flp_get_bits_rectangle,			/* get_bits_rectangle */
     flp_map_color_rgb_alpha,
     flp_create_compositor,
     flp_get_hardware_params,			/* get_hardware_params */
     flp_text_begin,
     flp_finish_copydevice,			/* finish_copydevice */
     flp_begin_transparency_group,			/* begin_transparency_group */
     flp_end_transparency_group,			/* end_transparency_group */
     flp_begin_transparency_mask,			/* begin_transparency_mask */
     flp_end_transparency_mask,			/* end_transparency_mask */
     flp_discard_transparency_layer,			/* discard_transparency_layer */
     flp_get_color_mapping_procs,			/* get_color_mapping_procs */
     flp_get_color_comp_index,			/* get_color_comp_index */
     flp_encode_color,			/* encode_color */
     flp_decode_color,			/* decode_color */
     flp_pattern_manage,			/* pattern_manage */
     flp_fill_rectangle_hl_color,			/* fill_rectangle_hl_color */
     flp_include_color_space,			/* include_color_space */
     flp_fill_linear_color_scanline,			/* fill_linear_color_scanline */
     flp_fill_linear_color_trapezoid,			/* fill_linear_color_trapezoid */
     flp_fill_linear_color_triangle,			/* fill_linear_color_triangle */
     flp_update_spot_equivalent_colors,			/* update_spot_equivalent_colors */
     flp_ret_devn_params,			/* ret_devn_params */
     flp_fillpage,		/* fillpage */
     flp_push_transparency_state,                      /* push_transparency_state */
     flp_pop_transparency_state,                      /* pop_transparency_state */
     flp_put_image,                      /* put_image */
     flp_dev_spec_op,                      /* dev_spec_op */
     flp_copy_planes,                      /* copy_planes */
     flp_get_profile,                      /* get_profile */
     flp_set_graphics_type_tag,                      /* set_graphics_type_tag */
     flp_strip_copy_rop2,
     flp_strip_tile_rect_devn,
     flp_copy_alpha_hl_color,
     flp_process_page
    }
};

#undef MAX_COORD
#undef MAX_RESOLUTION

int gx_device_subclass(gx_device *dev_to_subclass, gx_device *new_prototype, unsigned int private_data_size)
{
    gx_device *child_dev;
    subclass_data_common_t *psubclass_data;

    if (!dev_to_subclass->stype)
        return_error(gs_error_VMerror);

    if (private_data_size < sizeof(subclass_data_common_t))
        return_error(gs_error_typecheck);

    child_dev = (gx_device *)gs_alloc_bytes_immovable(dev_to_subclass->memory->non_gc_memory, dev_to_subclass->stype->ssize, "gx_device_subclass(device)");
    if (child_dev == 0)
        return_error(gs_error_VMerror);

    /* Make sure all methods are filled in, note this won't work for a forwarding device
     * so forwarding devices will have to be filled in before being subclassed. This doesn't fill
     * in the fill_rectangle proc, that gets done in the ultimate device's open proc.
     */
    gx_device_fill_in_procs(dev_to_subclass);
    memcpy(child_dev, dev_to_subclass, dev_to_subclass->stype->ssize);

    psubclass_data = (void *)gs_alloc_bytes(dev_to_subclass->memory->non_gc_memory, private_data_size, "subclass memory for first-last page");
    if (psubclass_data == 0){
        gs_free(dev_to_subclass->memory, child_dev, 1, dev_to_subclass->stype->ssize, "free subclass memory for first-last page");
        return_error(gs_error_VMerror);
    }
    memset(psubclass_data, 0x00, private_data_size);
    dev_to_subclass->subclass_data = psubclass_data;
    psubclass_data->child = child_dev;
    psubclass_data->private_data_size = private_data_size;

    memcpy(&dev_to_subclass->procs, &new_prototype->procs, sizeof(gx_device_procs));
    dev_to_subclass->finalize = new_prototype->finalize;
    dev_to_subclass->dname = new_prototype->dname;

    if (child_dev->subclass_data) {
        subclass_data_common_t *pchild_subclass_data = child_dev->subclass_data;
        gx_device *parent_dev = pchild_subclass_data->parent;

        psubclass_data->parent = parent_dev;
        pchild_subclass_data->parent = dev_to_subclass;
    }
    return 0;
}

static
void flp_finalize(gx_device *dev) {
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;
    gx_device *child_dev = subclass_common->child;

    memcpy(dev, child_dev, child_dev->stype->ssize);
    gs_free(dev->memory, psubclass_data, 1, psubclass_data->data_size, "subclass memory for first-last page");
    gs_free(dev->memory, child_dev, 1, child_dev->stype->ssize, "gx_device_subclass(device)");
    if (dev->finalize)
        dev->finalize(dev);
    return;
}

/* For printing devices the 'open' routine in gdevprn calls gdevprn_allocate_memory
 * which is responsible for creating the page buffer. This *also* fills in some of
 * the device procs, in particular fill_rectangle() so its vitally important that
 * we pass this on.
 */
int flp_open_device(gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.open_device)
        subclass_common->child->procs.open_device(subclass_common->child);

    return 0;
}

void flp_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    /* At first sight we should never have a method in a device structure which is NULL
     * because gx_device_fill_in_procs() should replace all the NULLs with default routines.
     * However, obselete routines, and a number of newer routines (especially those involving
     * transparency) don't get filled in. Its not obvious to me if this is deliberate or not,
     * but we'll be careful and check the subclassed device's method before trying to execute
     * it. Same for all the methods. NB the fill_rectangle method is deliberately not filled in
     * because that gets set up by gdev_prn_allocate_memory(). Isn't it great the way we do our
     * initialisation in lots of places?
     */
    if (subclass_common->child->procs.get_initial_matrix)
        subclass_common->child->procs.get_initial_matrix(subclass_common->child, pmat);
    else
        gx_default_get_initial_matrix(dev, pmat);
    return;
}

int flp_sync_output(gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.sync_output)
        return subclass_common->child->procs.sync_output(subclass_common->child);
    else
        gx_default_sync_output(dev);

    return 0;
}

int flp_output_page(gx_device *dev, int num_copies, int flush)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.output_page)
                return subclass_common->child->procs.output_page(subclass_common->child, num_copies, flush);
        }
    }
    psubclass_data->PageCount++;

    return 0;
}

int flp_close_device(gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.close_device)
        return subclass_common->child->procs.close_device(subclass_common->child);
    return 0;
}

gx_color_index flp_map_rgb_color(gx_device *dev, const gx_color_value cv[])
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.map_rgb_color)
        return subclass_common->child->procs.map_rgb_color(subclass_common->child, cv);
    else
        gx_error_encode_color(dev, cv);
    return 0;
}

int flp_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.map_color_rgb)
        return subclass_common->child->procs.map_color_rgb(subclass_common->child, color, rgb);
    else
        gx_default_map_color_rgb(dev, color, rgb);

    return 0;
}

int flp_fill_rectangle(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_rectangle)
                return subclass_common->child->procs.fill_rectangle(subclass_common->child, x, y, width, height, color);
        }
    }

    return 0;
}

int flp_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.tile_rectangle)
                return subclass_common->child->procs.tile_rectangle(subclass_common->child, tile, x, y, width, height, color0, color1, phase_x, phase_y);
        }
    }

    return 0;
}

int flp_copy_mono(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.copy_mono)
                return subclass_common->child->procs.copy_mono(subclass_common->child, data, data_x, raster, id, x, y, width, height, color0, color1);
        }
    }

    return 0;
}

int flp_copy_color(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.copy_color)
                return subclass_common->child->procs.copy_color(subclass_common->child, data, data_x, raster, id, x, y, width, height);
        }
    }

    return 0;
}

int flp_draw_line(gx_device *dev, int x0, int y0, int x1, int y1, gx_color_index color)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.obsolete_draw_line)
                return subclass_common->child->procs.obsolete_draw_line(subclass_common->child, x0, y0, x1, y1, color);
        }
    }
    return 0;
}

int flp_get_bits(gx_device *dev, int y, byte *data, byte **actual_data)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.get_bits)
                return subclass_common->child->procs.get_bits(subclass_common->child, y, data, actual_data);
            else
                return gx_default_get_bits(dev, y, data, actual_data);
        }
        else
            return gx_default_get_bits(dev, y, data, actual_data);
    }
    else
        return gx_default_get_bits(dev, y, data, actual_data);

    return 0;
}

int flp_get_params(gx_device *dev, gs_param_list *plist)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_params)
        return subclass_common->child->procs.get_params(subclass_common->child, plist);
    else
        return gx_default_get_params(dev, plist);

    return 0;
}

int flp_put_params(gx_device *dev, gs_param_list *plist)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.put_params)
        return subclass_common->child->procs.put_params(subclass_common->child, plist);
    else
        return gx_default_put_params(dev, plist);

    return 0;
}

gx_color_index flp_map_cmyk_color(gx_device *dev, const gx_color_value cv[])
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.map_cmyk_color)
        return subclass_common->child->procs.map_cmyk_color(subclass_common->child, cv);
    else
        return gx_default_map_cmyk_color(dev, cv);

    return 0;
}

const gx_xfont_procs *flp_get_xfont_procs(gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_xfont_procs)
        return subclass_common->child->procs.get_xfont_procs(subclass_common->child);
    else
        return gx_default_get_xfont_procs(dev);

    return 0;
}

gx_device *flp_get_xfont_device(gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_xfont_device)
        return subclass_common->child->procs.get_xfont_device(subclass_common->child);
    else
        return gx_default_get_xfont_device(dev);

    return 0;
}

gx_color_index flp_map_rgb_alpha_color(gx_device *dev, gx_color_value red, gx_color_value green, gx_color_value blue,
    gx_color_value alpha)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.map_rgb_alpha_color)
        return subclass_common->child->procs.map_rgb_alpha_color(subclass_common->child, red, green, blue, alpha);
    else
        return gx_default_map_rgb_alpha_color(subclass_common->child, red, green, blue, alpha);

    return 0;
}

gx_device *flp_get_page_device(gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_page_device)
        return subclass_common->child->procs.get_page_device(subclass_common->child);
    else
        return gx_default_get_page_device(dev);

    return 0;
}

int flp_get_alpha_bits(gx_device *dev, graphics_object_type type)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.get_alpha_bits)
                return subclass_common->child->procs.get_alpha_bits(subclass_common->child, type);
        }
    }

    return 0;
}

int flp_copy_alpha(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    gx_color_index color, int depth)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.copy_alpha)
                return subclass_common->child->procs.copy_alpha(subclass_common->child, data, data_x, raster, id, x, y, width, height, color, depth);
        }
    }

    return 0;
}

int flp_get_band(gx_device *dev, int y, int *band_start)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.get_band)
                return subclass_common->child->procs.get_band(subclass_common->child, y, band_start);
            else
                return gx_default_get_band(dev, y, band_start);
        }
        else
            return gx_default_get_band(dev, y, band_start);
    }
    else
        return gx_default_get_band(dev, y, band_start);

    return 0;
}

int flp_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_tile_bitmap *texture, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.copy_rop)
                return subclass_common->child->procs.copy_rop(subclass_common->child, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
            else
                return gx_default_copy_rop(subclass_common->child, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
        } else
            return gx_default_copy_rop(subclass_common->child, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);
    }
    else
        return gx_default_copy_rop(dev, sdata, sourcex, sraster, id, scolors, texture, tcolors, x, y, width, height, phase_x, phase_y, lop);

    return 0;
}

int flp_fill_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_path)
                return subclass_common->child->procs.fill_path(subclass_common->child, pis, ppath, params, pdcolor, pcpath);
            else
                return gx_default_fill_path(subclass_common->child, pis, ppath, params, pdcolor, pcpath);
        }
    }

    return 0;
}

int flp_stroke_path(gx_device *dev, const gs_imager_state *pis, gx_path *ppath,
    const gx_stroke_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.stroke_path)
                return subclass_common->child->procs.stroke_path(subclass_common->child, pis, ppath, params, pdcolor, pcpath);
            else
                return gx_default_stroke_path(subclass_common->child, pis, ppath, params, pdcolor, pcpath);
        }
    }

    return 0;
}

int flp_fill_mask(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth,
    gs_logical_operation_t lop, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_mask)
                return subclass_common->child->procs.fill_mask(subclass_common->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
            else
                return gx_default_fill_mask(subclass_common->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth, lop, pcpath);
        }
    }

    return 0;
}

int flp_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left, const gs_fixed_edge *right,
    fixed ybot, fixed ytop, bool swap_axes,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_trapezoid)
                return subclass_common->child->procs.fill_trapezoid(subclass_common->child, left, right, ybot, ytop, swap_axes, pdcolor, lop);
            else
                return gx_default_fill_trapezoid(subclass_common->child, left, right, ybot, ytop, swap_axes, pdcolor, lop);
        }
    }

    return 0;
}

int flp_fill_parallelogram(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_parallelogram)
                return subclass_common->child->procs.fill_parallelogram(subclass_common->child, px, py, ax, ay, bx, by, pdcolor, lop);
            else
                return gx_default_fill_parallelogram(subclass_common->child, px, py, ax, ay, bx, by, pdcolor, lop);
        }
    }

    return 0;
}

int flp_fill_triangle(gx_device *dev, fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_triangle)
                return subclass_common->child->procs.fill_triangle(subclass_common->child, px, py, ax, ay, bx, by, pdcolor, lop);
            else
                return gx_default_fill_triangle(subclass_common->child, px, py, ax, ay, bx, by, pdcolor, lop);
        }
    }

    return 0;
}

int flp_draw_thin_line(gx_device *dev, fixed fx0, fixed fy0, fixed fx1, fixed fy1,
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop,
    fixed adjustx, fixed adjusty)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.draw_thin_line)
                return subclass_common->child->procs.draw_thin_line(subclass_common->child, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
            else
                return gx_default_draw_thin_line(subclass_common->child, fx0, fy0, fx1, fy1, pdcolor, lop, adjustx, adjusty);
        }
    }

    return 0;
}

int flp_begin_image(gx_device *dev, const gs_imager_state *pis, const gs_image_t *pim,
    gs_image_format_t format, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.begin_image)
                return subclass_common->child->procs.begin_image(subclass_common->child, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo);
            else
                return gx_default_begin_image(subclass_common->child, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo);
        }
    }

    return 0;
}

int flp_image_data(gx_device *dev, gx_image_enum_common_t *info, const byte **planes, int data_x,
    uint raster, int height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.image_data)
                return subclass_common->child->procs.image_data(subclass_common->child, info, planes, data_x, raster, height);
        }
    }

    return 0;
}

int flp_end_image(gx_device *dev, gx_image_enum_common_t *info, bool draw_last)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.end_image)
                return subclass_common->child->procs.end_image(subclass_common->child, info, draw_last);
        }
    }

    return 0;
}

int flp_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    gx_color_index color0, gx_color_index color1,
    int phase_x, int phase_y)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.strip_tile_rectangle)
                return subclass_common->child->procs.strip_tile_rectangle(subclass_common->child, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
            else
                return gx_default_strip_tile_rectangle(subclass_common->child, tiles, x, y, width, height, color0, color1, phase_x, phase_y);
        }
    }

    return 0;
}

int flp_strip_copy_rop(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors,
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height,
    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.strip_copy_rop)
                return subclass_common->child->procs.strip_copy_rop(subclass_common->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
            else
                return gx_default_strip_copy_rop(subclass_common->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop);
        }
    }

    return 0;
}

void flp_get_clipping_box(gx_device *dev, gs_fixed_rect *pbox)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_clipping_box)
        subclass_common->child->procs.get_clipping_box(subclass_common->child, pbox);
    else
        gx_default_get_clipping_box(subclass_common->child, pbox);

    return;
}

int flp_begin_typed_image(gx_device *dev, const gs_imager_state *pis, const gs_matrix *pmat,
    const gs_image_common_t *pim, const gs_int_rect *prect,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gx_image_enum_common_t **pinfo)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.begin_typed_image)
                return subclass_common->child->procs.begin_typed_image(subclass_common->child, pis, pmat, pim, prect, pdcolor, pcpath, memory, pinfo);
            else
                return gx_default_begin_typed_image(subclass_common->child, pis, pmat, pim, prect, pdcolor, pcpath, memory, pinfo);
        } else
            return gx_default_begin_typed_image(subclass_common->child, pis, pmat, pim, prect, pdcolor, pcpath, memory, pinfo);
    }
    else
        return gx_default_begin_typed_image(subclass_common->child, pis, pmat, pim, prect, pdcolor, pcpath, memory, pinfo);

    return 0;
}

int flp_get_bits_rectangle(gx_device *dev, const gs_int_rect *prect,
    gs_get_bits_params_t *params, gs_int_rect **unread)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.get_bits_rectangle)
                return subclass_common->child->procs.get_bits_rectangle(subclass_common->child, prect, params, unread);
            else
                return gx_default_get_bits_rectangle(subclass_common->child, prect, params, unread);
        } else
            return gx_default_get_bits_rectangle(subclass_common->child, prect, params, unread);
    }
    else
        return gx_default_get_bits_rectangle(subclass_common->child, prect, params, unread);

    return 0;
}

int flp_map_color_rgb_alpha(gx_device *dev, gx_color_index color, gx_color_value rgba[4])
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.map_color_rgb_alpha)
        return subclass_common->child->procs.map_color_rgb_alpha(subclass_common->child, color, rgba);
    else
        return gx_default_map_color_rgb_alpha(subclass_common->child, color, rgba);

    return 0;
}

int flp_create_compositor(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,\
    gs_imager_state *pis, gs_memory_t *memory, gx_device *cdev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.create_compositor)
                return subclass_common->child->procs.create_compositor(subclass_common->child, pcdev, pcte, pis, memory, cdev);
        }
    }

    return 0;
}

int flp_get_hardware_params(gx_device *dev, gs_param_list *plist)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_hardware_params)
        return subclass_common->child->procs.get_hardware_params(subclass_common->child, plist);
    else
        return gx_default_get_hardware_params(subclass_common->child, plist);

    return 0;
}

/* Text processing (like images) works differently to other device
 * methods. Instead of the interpreter callign a device method, only
 * the 'begin' method is called, this creates a text enumerator which
 * it fills in (in part with the routines for processing text) and returns
 * to the interpreter. The interpreter then calls the methods defined in
 * the text enumerator to process the text.
 * Mad as a fish.....
 */

/* For our purposes if we are handling the text its because we are not
 * printing the page, so we cna afford to ignore all the text processing.
 * A more complex device might need to define real handlers for these, and
 * pass them on to the subclassed device.
 */
static text_enum_proc_process(flp_text_process);
static int
flp_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    return 0;
}
int
flp_text_process(gs_text_enum_t *pte)
{
    return 0;
}
static bool
flp_text_is_width_only(const gs_text_enum_t *pte)
{
    return false;
}
static int
flp_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    return 0;
}
static int
flp_text_set_cache(gs_text_enum_t *pte, const double *pw,
                   gs_text_cache_control_t control)
{
    return 0;
}
flp_text_retry(gs_text_enum_t *pte)
{
    return 0;
}
static void
flp_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    flp_text_enum_t *const penum = (flp_text_enum_t *)pte;

    gx_default_text_release(pte, cname);
}

static const gs_text_enum_procs_t flp_text_procs = {
    flp_text_resync, flp_text_process,
    flp_text_is_width_only, flp_text_current_width,
    flp_text_set_cache, flp_text_retry,
    flp_text_release
};

/* The device method which we do actually need to define. Either we are skipping the page,
 * in which case we create a text enumerator with our dummy procedures, or we are leaving it
 * up to the device, in which case we simply pass on the 'begin' method to the device.
 */
int flp_text_begin(gx_device *dev, gs_imager_state *pis, const gs_text_params_t *text,
    gs_font *font, gx_path *path, const gx_device_color *pdcolor, const gx_clip_path *pcpath,
    gs_memory_t *memory, gs_text_enum_t **ppte)
{
    flp_text_enum_t *penum;
    int code;

    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage && (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage)) {
        if (subclass_common->child->procs.text_begin)
            return subclass_common->child->procs.text_begin(subclass_common->child, pis, text, font, path, pdcolor, pcpath, memory, ppte);
        else
            return gx_default_text_begin(subclass_common->child, pis, text, font, path, pdcolor, pcpath, memory, ppte);
    }
    else {
        rc_alloc_struct_1(penum, flp_text_enum_t, &st_flp_text_enum, memory,
                      return_error(gs_error_VMerror), "gdev_flp_text_begin");
        penum->rc.free = rc_free_text_enum;
        code = gs_text_enum_init((gs_text_enum_t *)penum, &flp_text_procs,
                             dev, pis, text, font, path, pdcolor, pcpath, memory);
        if (code < 0) {
            gs_free_object(memory, penum, "gdev_flp_text_begin");
            return code;
        }
        *ppte = (gs_text_enum_t *)penum;
    }

    return 0;
}

/* This method seems (despite the name) to be intended to allow for
 * devices to initialise data before being invoked. For our subclassed
 * device this should already have been done.
 */
int flp_finish_copydevice(gx_device *dev, const gx_device *from_dev)
{
    return 0;
}

int flp_begin_transparency_group(gx_device *dev, const gs_transparency_group_params_t *ptgp,
    const gs_rect *pbbox, gs_imager_state *pis, gs_memory_t *mem)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.begin_transparency_group)
                return subclass_common->child->procs.begin_transparency_group(subclass_common->child, ptgp, pbbox, pis, mem);
        }
    }

    return 0;
}

int flp_end_transparency_group(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.end_transparency_group)
                return subclass_common->child->procs.end_transparency_group(subclass_common->child, pis);
        }
    }

    return 0;
}

int flp_begin_transparency_mask(gx_device *dev, const gx_transparency_mask_params_t *ptmp,
    const gs_rect *pbbox, gs_imager_state *pis, gs_memory_t *mem)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.begin_transparency_mask)
                return subclass_common->child->procs.begin_transparency_mask(subclass_common->child, ptmp, pbbox, pis, mem);
        }
    }

    return 0;
}

int flp_end_transparency_mask(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.end_transparency_mask)
                return subclass_common->child->procs.end_transparency_mask(subclass_common->child, pis);
        }
    }

}

int flp_discard_transparency_layer(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.discard_transparency_layer)
                return subclass_common->child->procs.discard_transparency_layer(subclass_common->child, pis);
        }
    }

    return 0;
}

const gx_cm_color_map_procs *flp_get_color_mapping_procs(const gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_color_mapping_procs)
        return subclass_common->child->procs.get_color_mapping_procs(subclass_common->child);
    else
        return gx_default_DevGray_get_color_mapping_procs(subclass_common->child);

    return 0;
}

int  flp_get_color_comp_index(gx_device *dev, const char * pname, int name_size, int component_type)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_color_comp_index)
        return subclass_common->child->procs.get_color_comp_index(subclass_common->child, pname, name_size, component_type);
    else
        return gx_error_get_color_comp_index(subclass_common->child, pname, name_size, component_type);

    return 0;
}

gx_color_index flp_encode_color(gx_device *dev, const gx_color_value colors[])
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.encode_color)
        return subclass_common->child->procs.encode_color(subclass_common->child, colors);
    else
        return gx_error_encode_color(subclass_common->child, colors);

    return 0;
}

flp_decode_color(gx_device *dev, gx_color_index cindex, gx_color_value colors[])
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.decode_color)
        return subclass_common->child->procs.decode_color(subclass_common->child, cindex, colors);
    else {
        memset(colors, 0, sizeof(gx_color_value[GX_DEVICE_COLOR_MAX_COMPONENTS]));
    }

    return 0;
}

int flp_pattern_manage(gx_device *dev, gx_bitmap_id id,
                gs_pattern1_instance_t *pinst, pattern_manage_t function)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.pattern_manage)
                return subclass_common->child->procs.pattern_manage(subclass_common->child, id, pinst, function);
        }
    }

    return 0;
}

int flp_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
        const gs_imager_state *pis, const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_rectangle_hl_color)
                return subclass_common->child->procs.fill_rectangle_hl_color(subclass_common->child, rect, pis, pdcolor, pcpath);
            else
                return_error(gs_error_rangecheck);
        }
    }

    return 0;
}

int flp_include_color_space(gx_device *dev, gs_color_space *cspace, const byte *res_name, int name_length)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.include_color_space)
        return subclass_common->child->procs.include_color_space(subclass_common->child, cspace, res_name, name_length);

    return 0;
}

int flp_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i, int j, int w, const frac31 *c0, const int32_t *c0_f, const int32_t *cg_num,
        int32_t cg_den)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_linear_color_scanline)
                return subclass_common->child->procs.fill_linear_color_scanline(subclass_common->child, fa, i, j, w, c0, c0_f, cg_num, cg_den);
            else
                return gx_default_fill_linear_color_scanline(subclass_common->child, fa, i, j, w, c0, c0_f, cg_num, cg_den);
        }
    }

    return 0;
}

int flp_fill_linear_color_trapezoid(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const gs_fixed_point *p3,
        const frac31 *c0, const frac31 *c1,
        const frac31 *c2, const frac31 *c3)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_linear_color_trapezoid)
                return subclass_common->child->procs.fill_linear_color_trapezoid(subclass_common->child, fa, p0, p1, p2, p3, c0, c1, c2, c3);
            else
                return gx_default_fill_linear_color_trapezoid(subclass_common->child, fa, p0, p1, p2, p3, c0, c1, c2, c3);
        }
    }

    return 0;
}

int flp_fill_linear_color_triangle(gx_device *dev, const gs_fill_attributes *fa,
        const gs_fixed_point *p0, const gs_fixed_point *p1,
        const gs_fixed_point *p2, const frac31 *c0, const frac31 *c1, const frac31 *c2)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fill_linear_color_triangle)
                return subclass_common->child->procs.fill_linear_color_triangle(subclass_common->child, fa, p0, p1, p2, c0, c1, c2);
            else
                return gx_default_fill_linear_color_triangle(subclass_common->child, fa, p0, p1, p2, c0, c1, c2);
        }
    }

    return 0;
}

int flp_update_spot_equivalent_colors(gx_device *dev, const gs_state * pgs)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.update_spot_equivalent_colors)
        return subclass_common->child->procs.update_spot_equivalent_colors(subclass_common->child, pgs);

    return 0;
}

gs_devn_params *flp_ret_devn_params(gx_device *dev)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.ret_devn_params)
        return subclass_common->child->procs.ret_devn_params(subclass_common->child);

    return 0;
}

int flp_fillpage(gx_device *dev, gs_imager_state * pis, gx_device_color *pdevc)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.fillpage)
                return subclass_common->child->procs.fillpage(subclass_common->child, pis, pdevc);
            else
                return gx_default_fillpage(subclass_common->child, pis, pdevc);
        }
    }

    return 0;
}

int flp_push_transparency_state(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.push_transparency_state)
                return subclass_common->child->procs.push_transparency_state(subclass_common->child, pis);
        }
    }

    return 0;
}

int flp_pop_transparency_state(gx_device *dev, gs_imager_state *pis)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.push_transparency_state)
                return subclass_common->child->procs.push_transparency_state(subclass_common->child, pis);
        }
    }

    return 0;
}

int flp_put_image(gx_device *dev, const byte *buffer, int num_chan, int x, int y,
            int width, int height, int row_stride, int plane_stride,
            int alpha_plane_index, int tag_plane_index)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.put_image)
                return subclass_common->child->procs.put_image(subclass_common->child, buffer, num_chan, x, y, width, height, row_stride, plane_stride, alpha_plane_index, tag_plane_index);
        }
    }

    return 0;
}

int flp_dev_spec_op(gx_device *dev, int op, void *data, int datasize)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.dev_spec_op)
        return subclass_common->child->procs.dev_spec_op(subclass_common->child, op, data, datasize);

    return 0;
}

int flp_copy_planes(gx_device *dev, const byte *data, int data_x, int raster, gx_bitmap_id id,
    int x, int y, int width, int height, int plane_height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.copy_planes)
                return subclass_common->child->procs.copy_planes(subclass_common->child, data, data_x, raster, id, x, y, width, height, plane_height);
        }
    }

    return 0;
}

int flp_get_profile(gx_device *dev, cmm_dev_profile_t **dev_profile)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (subclass_common->child->procs.get_profile)
        return subclass_common->child->procs.get_profile(subclass_common->child, dev_profile);
    else
        return gx_default_get_profile(subclass_common->child, dev_profile);

    return 0;
}

void flp_set_graphics_type_tag(gx_device *dev, gs_graphics_type_tag_t tag)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.set_graphics_type_tag)
                subclass_common->child->procs.set_graphics_type_tag(subclass_common->child, tag);
        }
    }

    return;
}

int flp_strip_copy_rop2(gx_device *dev, const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
    const gx_color_index *scolors, const gx_strip_bitmap *textures, const gx_color_index *tcolors,
    int x, int y, int width, int height, int phase_x, int phase_y, gs_logical_operation_t lop, uint planar_height)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.strip_copy_rop2)
                return subclass_common->child->procs.strip_copy_rop2(subclass_common->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);
            else
                return gx_default_strip_copy_rop2(subclass_common->child, sdata, sourcex, sraster, id, scolors, textures, tcolors, x, y, width, height, phase_x, phase_y, lop, planar_height);
        }
    }

    return 0;
}

int flp_strip_tile_rect_devn(gx_device *dev, const gx_strip_bitmap *tiles, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor0, const gx_drawing_color *pdcolor1, int phase_x, int phase_y)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.strip_tile_rect_devn)
                return subclass_common->child->procs.strip_tile_rect_devn(subclass_common->child, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);
            else
                return gx_default_strip_tile_rect_devn(subclass_common->child, tiles, x, y, width, height, pdcolor0, pdcolor1, phase_x, phase_y);
        }
    }

    return 0;
}

int flp_copy_alpha_hl_color(gx_device *dev, const byte *data, int data_x,
    int raster, gx_bitmap_id id, int x, int y, int width, int height,
    const gx_drawing_color *pdcolor, int depth)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.copy_alpha_hl_color)
                return subclass_common->child->procs.copy_alpha_hl_color(subclass_common->child, data, data_x, raster, id, x, y, width, height, pdcolor, depth);
            else
                return_error(gs_error_rangecheck);
        }
    }

    return 0;
}

int flp_process_page(gx_device *dev, gx_process_page_options_t *options)
{
    first_last_subclass_data *psubclass_data = dev->subclass_data;
    subclass_data_common_t *subclass_common = &psubclass_data->subclass_common;

    if (psubclass_data->PageCount >= dev->FirstPage) {
        if (!dev->LastPage || psubclass_data->PageCount <= dev->LastPage) {
            if (subclass_common->child->procs.process_page)
                return subclass_common->child->procs.process_page(subclass_common->child, options);
        }
    }

    return 0;
}
