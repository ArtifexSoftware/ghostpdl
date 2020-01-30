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


/* Command list document- and page-level code. */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"           /* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gsparams.h"
#include "gxdcolor.h"
#include "gscms.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "gxdevsop.h"

#include "valgrind.h"

extern dev_proc_open_device(pattern_clist_open_device);

/* GC information */
/*  Where is the GC information for the common objects that are
    shared between the reader and writer.  I see pointers in
    there, but they don't seem to be GC.  This is why I have
    put the icc_table and the link cache in the reader and the
    writer rather than the common.   fixme: Also, if icc_cache_cl is not
    included in the writer, 64bit builds will seg fault */

extern_st(st_gs_gstate);
static
ENUM_PTRS_WITH(device_clist_enum_ptrs, gx_device_clist *cdev)
    if (index < st_device_forward_max_ptrs) {
        gs_ptr_type_t ret = ENUM_USING_PREFIX(st_device_forward, st_device_max_ptrs);

        return (ret ? ret : ENUM_OBJ(0));
    }
    index -= st_device_forward_max_ptrs;
    /* RJW: We do not enumerate icc_cache_cl or icc_cache_list as they
     * are allocated in non gc space */
    if (CLIST_IS_WRITER(cdev)) {
        switch (index) {
        case 0: return ENUM_OBJ((cdev->writer.image_enum_id != gs_no_id ?
                     cdev->writer.clip_path : 0));
        case 1: return ENUM_OBJ((cdev->writer.image_enum_id != gs_no_id ?
                     cdev->writer.color_space.space : 0));
        case 2: return ENUM_OBJ(cdev->writer.pinst);
        case 3: return ENUM_OBJ(cdev->writer.cropping_stack);
        case 4: return ENUM_OBJ(cdev->writer.icc_table);
        default:
        return ENUM_USING(st_gs_gstate, &cdev->writer.gs_gstate,
                  sizeof(gs_gstate), index - 5);
        }
    }
    else {
        /* 041207
         * clist is reader.
         * We don't expect this code to be exercised at this time as the reader
         * runs under gdev_prn_output_page which is an atomic function of the
         * interpreter. We do this as this situation may change in the future.
         */

        if (index == 0)
            return ENUM_OBJ(cdev->reader.offset_map);
        else if (index == 1)
            return ENUM_OBJ(cdev->reader.icc_table);
        else if (index == 2)
            return ENUM_OBJ(cdev->reader.color_usage_array);
        else
            return 0;
    }
ENUM_PTRS_END
static
RELOC_PTRS_WITH(device_clist_reloc_ptrs, gx_device_clist *cdev)
{
    RELOC_PREFIX(st_device_forward);
    if (CLIST_IS_WRITER(cdev)) {
        if (cdev->writer.image_enum_id != gs_no_id) {
            RELOC_VAR(cdev->writer.clip_path);
            RELOC_VAR(cdev->writer.color_space.space);
        }
        RELOC_VAR(cdev->writer.pinst);
        RELOC_VAR(cdev->writer.cropping_stack);
        RELOC_VAR(cdev->writer.icc_table);
        RELOC_USING(st_gs_gstate, &cdev->writer.gs_gstate,
            sizeof(gs_gstate));
    } else {
        /* 041207
         * clist is reader.
         * See note above in ENUM_PTRS_WITH section.
         */
        RELOC_VAR(cdev->reader.offset_map);
        RELOC_VAR(cdev->reader.icc_table);
        RELOC_VAR(cdev->reader.color_usage_array);
    }
} RELOC_PTRS_END
public_st_device_clist();
private_st_clist_writer_cropping_buffer();
private_st_clist_icctable_entry();
private_st_clist_icctable();

/* Forward declarations of driver procedures */
dev_proc_open_device(clist_open);
dev_proc_output_page(clist_output_page);
static dev_proc_close_device(clist_close);
static dev_proc_get_band(clist_get_band);
/* Driver procedures defined in other files are declared in gxcldev.h. */

/* Other forward declarations */
static int clist_put_current_params(gx_device_clist_writer *cldev);

/* The device procedures */
const gx_device_procs gs_clist_device_procs = {
    clist_open,
    gx_forward_get_initial_matrix,
    gx_default_sync_output,
    clist_output_page,
    clist_close,
    gx_forward_map_rgb_color,
    gx_forward_map_color_rgb,
    clist_fill_rectangle,
    gx_default_tile_rectangle,
    clist_copy_mono,
    clist_copy_color,
    gx_default_draw_line,
    gx_default_get_bits,
    gx_forward_get_params,
    gx_forward_put_params,
    gx_forward_map_cmyk_color,
    gx_forward_get_xfont_procs,
    gx_forward_get_xfont_device,
    gx_forward_map_rgb_alpha_color,
    gx_forward_get_page_device,
    gx_forward_get_alpha_bits,
    clist_copy_alpha,
    clist_get_band,
    gx_default_copy_rop,
    clist_fill_path,
    clist_stroke_path,
    clist_fill_mask,
    clist_fill_trapezoid,
    clist_fill_parallelogram,
    clist_fill_triangle,
    gx_default_draw_thin_line,
    gx_default_begin_image,
    gx_default_image_data,
    gx_default_end_image,
    clist_strip_tile_rectangle,
    clist_strip_copy_rop,
    gx_forward_get_clipping_box,
    clist_begin_typed_image,
    clist_get_bits_rectangle,
    gx_forward_map_color_rgb_alpha,
    clist_create_compositor,
    gx_forward_get_hardware_params,
    gx_default_text_begin,
    gx_default_finish_copydevice,
    gx_default_begin_transparency_group,                       /* begin_transparency_group */
    gx_default_end_transparency_group,                       /* end_transparency_group */
    gx_default_begin_transparency_mask,                       /* begin_transparency_mask */
    gx_default_end_transparency_mask,                       /* end_transparency_mask */
    gx_default_discard_transparency_layer,                       /* discard_transparency_layer */
    gx_forward_get_color_mapping_procs,
    gx_forward_get_color_comp_index,
    gx_forward_encode_color,
    gx_forward_decode_color,
    gx_default_pattern_manage,                       /* pattern_manage */
    clist_fill_rectangle_hl_color,
    gx_default_include_color_space,
    gx_default_fill_linear_color_scanline,
    clist_fill_linear_color_trapezoid,
    clist_fill_linear_color_triangle,
    gx_forward_update_spot_equivalent_colors,
    gx_forward_ret_devn_params,
    clist_fillpage,
    gx_default_push_transparency_state,                      /* push_transparency_state */
    gx_default_pop_transparency_state,                      /* pop_transparency_state */
    gx_default_put_image,                      /* put_image */
    clist_dev_spec_op,
    clist_copy_planes,         /* copy planes */
    gx_default_get_profile,
    gx_default_set_graphics_type_tag,
    clist_strip_copy_rop2,
    clist_strip_tile_rect_devn,
    clist_copy_alpha_hl_color,
    clist_process_page,
    gx_default_transform_pixel_region,
    clist_fill_stroke_path,
};

/*------------------- Choose the implementation -----------------------

   For chossing the clist i/o implementation by makefile options
   we define global variables, which are initialized with
   file/memory io procs when they are included into the build.
 */
const clist_io_procs_t *clist_io_procs_file_global = NULL;
const clist_io_procs_t *clist_io_procs_memory_global = NULL;

void
clist_init_io_procs(gx_device_clist *pclist_dev, bool in_memory)
{
#ifdef PACIFY_VALGRIND
    VALGRIND_HG_DISABLE_CHECKING(&clist_io_procs_file_global, sizeof(clist_io_procs_file_global));
    VALGRIND_HG_DISABLE_CHECKING(&clist_io_procs_memory_global, sizeof(clist_io_procs_memory_global));
#endif
    /* if clist_io_procs_file_global is NULL, then BAND_LIST_STORAGE=memory */
    /* was specified in the build, and "file" is not available */
    if (in_memory || clist_io_procs_file_global == NULL)
        pclist_dev->common.page_info.io_procs = clist_io_procs_memory_global;
    else
        pclist_dev->common.page_info.io_procs = clist_io_procs_file_global;
}

/* ------ Define the command set and syntax ------ */

/*
 * The buffer area (data, data_size) holds a bitmap cache when both writing
 * and reading.  The rest of the space is used for the command buffer and
 * band state bookkeeping when writing, and for the rendering buffer (image
 * device) when reading.  For the moment, we divide the space up
 * arbitrarily, except that we allocate less space for the bitmap cache if
 * the device doesn't need halftoning.
 *
 * All the routines for allocating tables in the buffer are idempotent, so
 * they can be used to check whether a given-size buffer is large enough.
 */

/*
 * Calculate the desired size for the tile cache.
 */
static uint
clist_tile_cache_size(const gx_device * target, uint data_size)
{
    uint bits_size =
    (data_size / 5) & -align_cached_bits_mod;   /* arbitrary */

    if (!gx_device_must_halftone(target)) {     /* No halftones -- cache holds only Patterns & characters. */
        bits_size -= bits_size >> 2;
    }
#define min_bits_size 1024
    if (bits_size < min_bits_size)
        bits_size = min_bits_size;
#undef min_bits_size
    return bits_size;
}

/*
 * Initialize the allocation for the tile cache.  Sets: tile_hash_mask,
 * tile_max_count, tile_table, chunk (structure), bits (structure).
 */
static int
clist_init_tile_cache(gx_device * dev, byte * init_data, ulong data_size)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    byte *data = init_data;
    uint bits_size = data_size;
    /*
     * Partition the bits area between the hash table and the actual
     * bitmaps.  The per-bitmap overhead is about 24 bytes; if the
     * average character size is 10 points, its bitmap takes about 24 +
     * 0.5 * 10/72 * xdpi * 10/72 * ydpi / 8 bytes (the 0.5 being a
     * fudge factor to account for characters being narrower than they
     * are tall), which gives us a guideline for the size of the hash
     * table.
     */
    uint avg_char_size =
        (uint)(dev->HWResolution[0] * dev->HWResolution[1] *
               (0.5 * 10 / 72 * 10 / 72 / 8)) + 24;
    uint hc = bits_size / avg_char_size;
    uint hsize;

    while ((hc + 1) & hc)
        hc |= hc >> 1;          /* make mask (power of 2 - 1) */
    if (hc < 0xff)
        hc = 0xff;              /* make allowance for halftone tiles */
    else if (hc > 0xfff)
        hc = 0xfff;             /* cmd_op_set_tile_index has 12-bit operand */
    /* Make sure the tables will fit. */
    while (hc >= 3 && (hsize = (hc + 1) * sizeof(tile_hash)) >= bits_size)
        hc >>= 1;
    if (hc < 3)
        return_error(gs_error_rangecheck);
    cdev->tile_hash_mask = hc;
    cdev->tile_max_count = hc - (hc >> 2);
    cdev->tile_table = (tile_hash *) data;
    data += hsize;
    bits_size -= hsize;
    gx_bits_cache_chunk_init(cdev->cache_chunk, data, bits_size);
    gx_bits_cache_init(&cdev->bits, cdev->cache_chunk);
    return 0;
}

/*
 * Initialize the allocation for the bands.  Requires: target.  Sets:
 * page_band_height (=page_info.band_params.BandHeight), nbands.
 */
static int
clist_init_bands(gx_device * dev, gx_device_memory *bdev, uint data_size,
                 int band_width, int band_height)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    int nbands;
    ulong space;

    if (dev_proc(dev, open_device) == pattern_clist_open_device) {
        /* We don't need bands really. */
        cdev->page_band_height = dev->height;
        cdev->nbands = 1;
        return 0;
    }
    if (gdev_mem_data_size(bdev, band_width, band_height, &space) < 0 ||
        space > data_size)
        return_error(gs_error_rangecheck);
    cdev->page_band_height = band_height;
    nbands = (cdev->target->height + band_height - 1) / band_height;
    cdev->nbands = nbands;
#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':'))
        dmlprintf4(dev->memory, "[:]width=%d, band_width=%d, band_height=%d, nbands=%d\n",
                   bdev->width, band_width, band_height, nbands);
#endif
    return 0;
}

/*
 * Initialize the allocation for the band states, which are used only
 * when writing.  Requires: nbands.  Sets: states, cbuf, cend.
 */
static int
clist_init_states(gx_device * dev, byte * init_data, uint data_size)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    ulong state_size = cdev->nbands * (ulong) sizeof(gx_clist_state);
    /* Align to the natural boundary for ARM processors, bug 689600 */
    long alignment = (-(long)init_data) & (sizeof(init_data) - 1);

    /*
     * The +100 in the next line is bogus, but we don't know what the
     * real check should be. We're effectively assuring that at least 100
     * bytes will be available to buffer command operands.
     */
    if (state_size + sizeof(cmd_prefix) + cmd_largest_size + 100 + alignment > data_size)
        return_error(gs_error_rangecheck);
    /* The end buffer position is not affected by alignment */
    cdev->cend = init_data + data_size;
    init_data +=  alignment;
    cdev->states = (gx_clist_state *) init_data;
    cdev->cbuf = init_data + state_size;
    return 0;
}

/*
 * Initialize all the data allocations.  Requires: target.  Sets:
 * page_tile_cache_size, page_info.band_params.BandWidth,
 * page_info.band_params.BandBufferSpace, + see above.
 */
static int
clist_init_data(gx_device * dev, byte * init_data, uint data_size)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    gx_device *target = cdev->target;
    /* BandWidth can't be smaller than target device width */
    const int band_width =
        cdev->page_info.band_params.BandWidth = max(target->width, cdev->band_params.BandWidth);
    int band_height = cdev->band_params.BandHeight;
    bool page_uses_transparency = cdev->page_uses_transparency;
    const uint band_space =
    cdev->page_info.band_params.BandBufferSpace =
        (cdev->band_params.BandBufferSpace ?
         cdev->band_params.BandBufferSpace : data_size);
    byte *data = init_data;
    uint size = band_space;
    uint bits_size;
    gx_device_memory bdev;
    gx_device *pbdev = (gx_device *)&bdev;
    int code;
    int align = 1 << (target->log2_align_mod > log2_align_bitmap_mod ? target->log2_align_mod : log2_align_bitmap_mod);

    /* the clist writer has its own color info that depends upon the
       transparency group color space (if transparency exists).  The data that is
       used in the clist writing. Here it is initialized with
       the target device color info.  The values will be pushed and popped
       in a stack if we have changing color spaces in the transparency groups. */

    cdev->clist_color_info.depth = dev->color_info.depth;
    cdev->clist_color_info.polarity = dev->color_info.polarity;
    cdev->clist_color_info.num_components = dev->color_info.num_components;
    cdev->graphics_type_tag = target->graphics_type_tag;	/* initialize to same as target */

    /* Call create_buf_device to get the memory planarity set up. */
    code = cdev->buf_procs.create_buf_device(&pbdev, target, 0, NULL, NULL, NULL);
    if (code < 0)
        return code;
    /* HACK - if the buffer device can't do copy_alpha, disallow */
    /* copy_alpha in the commmand list device as well. */
    if (dev_proc(pbdev, copy_alpha) == gx_no_copy_alpha)
        cdev->disable_mask |= clist_disable_copy_alpha;
    if (dev_proc(cdev, open_device) == pattern_clist_open_device) {
        bits_size = data_size / 2;
        cdev->page_line_ptrs_offset = 0;
    } else {
        if (band_height) {
            /*
             * The band height is fixed, so the band buffer requirement
             * is completely determined.
             */
            ulong band_data_size;
            int adjusted;

            adjusted = (dev_proc(dev, dev_spec_op)(dev, gxdso_adjust_bandheight, NULL, band_height));
            if (adjusted > 0)
                band_height = adjusted;

            if (gdev_mem_data_size(&bdev, band_width, band_height, &band_data_size) < 0 ||
                band_data_size >= band_space) {
                if (pbdev->finalize)
                    pbdev->finalize(pbdev);
                return_error(gs_error_rangecheck);
            }
            /* If the tile_cache_size is specified, use it */
            if (cdev->space_params.band.tile_cache_size == 0) {
                bits_size = min(band_space - band_data_size, data_size >> 1);
            } else {
                bits_size = cdev->space_params.band.tile_cache_size;
            }
            /* The top of the tile_cache is the bottom of the imagable band buffer,
             * which needs to be appropriately aligned. Because the band height is
             * fixed, we must round *down* the size of the cache to a appropriate
             * value. See clist_render_thread() and clist_rasterize_lines()
             * for where the value is used.
             */
            bits_size = ROUND_DOWN(bits_size, align);
        } else {
            int adjusted;
            /*
             * Choose the largest band height that will fit in the
             * rendering-time buffer.
             */
            bits_size = clist_tile_cache_size(target, band_space);
            bits_size = min(bits_size, data_size >> 1);
            /* The top of the tile_cache is the bottom of the imagable band buffer,
             * which needs to be appropriately aligned. Because the band height is
             * fixed, here we round up the size of the cache, since the band height
             * is variable, and it should only be a few bytes. See clist_render_thread()
             * and clist_rasterize_lines() for where the value is used.
             */
            bits_size = ROUND_UP(bits_size, align);
            band_height = gdev_mem_max_height(&bdev, band_width,
                              band_space - bits_size, page_uses_transparency);
            if (band_height == 0) {
                if (pbdev->finalize)
                    pbdev->finalize(pbdev);
                return_error(gs_error_rangecheck);
            }
            adjusted = (dev_proc(dev, dev_spec_op)(dev, gxdso_adjust_bandheight, NULL, band_height));
            if (adjusted > 0)
                band_height = adjusted;
        }
        /* The above calculated bits_size's include space for line ptrs. What is
         * the offset for the line_ptrs within the buffer? */
        if (gdev_mem_bits_size(&bdev, band_width, band_height, &cdev->page_line_ptrs_offset) < 0)
            return_error(gs_error_VMerror);
    }
    cdev->ins_count = 0;
    code = clist_init_tile_cache(dev, data, bits_size);
    if (code < 0) {
        if (pbdev->finalize)
            pbdev->finalize(pbdev);
        return code;
    }
    cdev->page_tile_cache_size = bits_size;
    data += bits_size;
    size -= bits_size;
    code = clist_init_bands(dev, &bdev, size, band_width, band_height);
    if (code < 0) {
        if (pbdev->finalize)
            pbdev->finalize(pbdev);
        return code;
    }

    if (pbdev->finalize)
        pbdev->finalize(pbdev);

    return clist_init_states(dev, data, data_size - bits_size);
}
/*
 * Reset the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
static int
clist_reset(gx_device * dev)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    int code = clist_init_data(dev, cdev->data, cdev->data_size);
    int nbands;

    if (code < 0)
        return (cdev->permanent_error = code);
    /* Now initialize the rest of the state. */
    cdev->permanent_error = 0;
    nbands = cdev->nbands;
    cdev->ymin = cdev->ymax = -1;       /* render_init not done yet */
    memset(cdev->tile_table, 0, (cdev->tile_hash_mask + 1) *
       sizeof(*cdev->tile_table));
    cdev->cnext = cdev->cbuf;
    cdev->ccl = 0;
    cdev->band_range_list.head = cdev->band_range_list.tail = 0;
    cdev->band_range_min = 0;
    cdev->band_range_max = nbands - 1;
    {
        int band;
        gx_clist_state *states = cdev->states;

        for (band = 0; band < nbands; band++, states++) {
            static const gx_clist_state cls_initial = { cls_initial_values };

            *states = cls_initial;
        }
    }
    /*
     * Round up the size of the per-tile band mask so that the bits,
     * which follow it, stay aligned.
     */
    cdev->tile_band_mask_size =
        ((nbands + (align_bitmap_mod * 8 - 1)) >> 3) &
        ~(align_bitmap_mod - 1);
    /*
     * Initialize the all-band parameters to impossible values,
     * to force them to be written the first time they are used.
     */
    memset(&cdev->tile_params, 0, sizeof(cdev->tile_params));
    cdev->tile_depth = 0;
    cdev->tile_known_min = nbands;
    cdev->tile_known_max = -1;
    GS_STATE_INIT_VALUES_CLIST((&cdev->gs_gstate));
    cdev->clip_path = NULL;
    cdev->clip_path_id = gs_no_id;
    cdev->color_space.byte1 = 0;
    cdev->color_space.id = gs_no_id;
    cdev->color_space.space = 0;
    {
        int i;

        for (i = 0; i < countof(cdev->transfer_ids); ++i)
            cdev->transfer_ids[i] = gs_no_id;
    }
    cdev->black_generation_id = gs_no_id;
    cdev->undercolor_removal_id = gs_no_id;
    cdev->device_halftone_id = gs_no_id;
    cdev->image_enum_id = gs_no_id;
    cdev->cropping_min = cdev->save_cropping_min = 0;
    cdev->cropping_max = cdev->save_cropping_max = cdev->height;
    cdev->cropping_saved = false;
    cdev->cropping_stack = NULL;
    cdev->cropping_level = 0;
    cdev->mask_id_count = cdev->mask_id = cdev->temp_mask_id = 0;
    cdev->icc_table = NULL;
    cdev->op_fill_active = false;
    cdev->op_stroke_active = false;
    return 0;
}
/*
 * Initialize the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
static int
clist_init(gx_device * dev)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    int code = clist_reset(dev);

    if (code >= 0) {
        cdev->image_enum_id = gs_no_id;
        cdev->ignore_lo_mem_warnings = 0;
    }
    return code;
}

/* Write out the current parameters that must be at the head of each page */
/* if async rendering is in effect */
static int
clist_emit_page_header(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    int code = 0;

    if ((cdev->disable_mask & clist_disable_pass_thru_params)) {
        code = clist_put_current_params(cdev);
        cdev->permanent_error = (code < 0 ? code : 0);
    }
    return code;
}

/* Reset parameters for the beginning of a page. */
static void
clist_reset_page(gx_device_clist_writer *cwdev)
{
    cwdev->page_bfile_end_pos = 0;
}

/* Open the device's bandfiles */
static int
clist_open_output_file(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    char fmode[4];
    int code;

    if (cdev->do_not_open_or_close_bandfiles)
        return 0; /* external bandfile open/close managed externally */
    cdev->page_cfile = 0;       /* in case of failure */
    cdev->page_bfile = 0;       /* ditto */
    code = clist_init(dev);
    if (code < 0)
        return code;
    snprintf(fmode, sizeof(fmode), "w+%s", gp_fmode_binary_suffix);
    cdev->page_cfname[0] = 0;   /* create a new file */
    cdev->page_bfname[0] = 0;   /* ditto */
    clist_reset_page(cdev);
    if ((code = cdev->page_info.io_procs->fopen(cdev->page_cfname, fmode, &cdev->page_cfile,
                            cdev->bandlist_memory, cdev->bandlist_memory,
                            true)) < 0 ||
        (code = cdev->page_info.io_procs->fopen(cdev->page_bfname, fmode, &cdev->page_bfile,
                            cdev->bandlist_memory, cdev->bandlist_memory,
                            false)) < 0
        ) {
        clist_close_output_file(dev);
        cdev->permanent_error = code;
    }
    return code;
}

/* Close, and free the contents of, the temporary files of a page. */
/* Note that this does not deallocate the buffer. */
int
clist_close_page_info(gx_band_page_info_t *ppi)
{
    if (ppi->cfile != NULL) {
        ppi->io_procs->fclose(ppi->cfile, ppi->cfname, true);
        ppi->cfile = NULL;
        ppi->cfname[0] = 0;     /* prevent re-use in case this is a fake path */
    }
    if (ppi->bfile != NULL) {
        ppi->io_procs->fclose(ppi->bfile, ppi->bfname, true);
        ppi->bfile = NULL;
        ppi->bfname[0] = 0;     /* prevent re-use in case this is a fake path */
    }
    return 0;
}

/* Close the device by freeing the temporary files. */
/* Note that this does not deallocate the buffer. */
int
clist_close_output_file(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;

    return clist_close_page_info(&cdev->page_info);
}

/* Open the device by initializing the device state and opening the */
/* scratch files. */
int
clist_open(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    bool save_is_open = dev->is_open;
    int code;

    cdev->permanent_error = 0;
    cdev->is_open = false;

    cdev->cache_chunk = (gx_bits_cache_chunk *)gs_alloc_bytes(cdev->memory->non_gc_memory, sizeof(gx_bits_cache_chunk), "alloc tile cache for clist");
    if (!cdev->cache_chunk)
        return_error(gs_error_VMerror);
    memset(cdev->cache_chunk, 0x00, sizeof(gx_bits_cache_chunk));

    code = clist_init(dev);
    if (code < 0)
        goto errxit;

    cdev->icc_cache_list_len = 0;
    cdev->icc_cache_list = NULL;
    code = clist_open_output_file(dev);
    if ( code >= 0)
        code = clist_emit_page_header(dev);
    if (code >= 0) {
        dev->is_open = save_is_open;
        return code;		/* success */
    }
    /* fall through to clean up and return error code */
errxit:
    /* prevent leak */
    gs_free_object(cdev->memory->non_gc_memory, cdev->cache_chunk, "free tile cache for clist");
    cdev->cache_chunk = NULL;
    return code;
}

static int
clist_close(gx_device *dev)
{
    int i;
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;

    /* I'd like to free the cache chunk in here, but we can't, because the pattern clist
     * device gets closed, but not discarded, later it gets run. So we have to free the memory
     * in *2* places, once in gdev_prn_tear_down() for regular clists, and once in
     * gx_pattern_cache_free_entry() for pattern clists....
     */
    for(i = 0; i < cdev->icc_cache_list_len; i++) {
        rc_decrement(cdev->icc_cache_list[i], "clist_close");
    }
    cdev->icc_cache_list_len = 0;
    gs_free_object(cdev->memory->thread_safe_memory, cdev->icc_cache_list, "clist_close");
    cdev->icc_cache_list = NULL;

    if (cdev->do_not_open_or_close_bandfiles)
        return 0;
    if (dev_proc(cdev, open_device) == pattern_clist_open_device) {
        gs_free_object(cdev->bandlist_memory, cdev->data, "clist_close");
        cdev->data = NULL;
    }
    return clist_close_output_file(dev);
}

/* The output_page procedure should never be called! */
int
clist_output_page(gx_device * dev, int num_copies, int flush)
{
    return_error(gs_error_Fatal);
}

/* Reset (or prepare to append to) the command list after printing a page. */
int
clist_finish_page(gx_device *dev, bool flush)
{
    gx_device_clist_writer *const cdev = &((gx_device_clist *)dev)->writer;
    int code;

    /* If this is a reader clist, which is about to be reset to a writer,
     * free any color_usage array used by same.
     * since we have been rendering, shut down threads
     * Also free the icc_table at this time and the icc_cache
     */
    if (!CLIST_IS_WRITER((gx_device_clist *)dev)) {
        gx_device_clist_reader * const crdev =  &((gx_device_clist *)dev)->reader;

        clist_teardown_render_threads(dev);
        gs_free_object(cdev->memory, crdev->color_usage_array, "clist_color_usage_array");
        crdev->color_usage_array = NULL;

       /* Free the icc table associated with this device.
           The threads that may have pointed to this were destroyed in
           the above call to clist_teardown_render_threads.  Since they
           all maintained a copy of the cache and the table there should not
           be any issues. */
        clist_free_icc_table(crdev->icc_table, crdev->memory);
        crdev->icc_table = NULL;
    }
    if (flush) {
        if (cdev->page_cfile != 0) {
            code = cdev->page_info.io_procs->rewind(cdev->page_cfile, true, cdev->page_cfname);
            if (code < 0) return code;
        }
        if (cdev->page_bfile != 0) {
            code = cdev->page_info.io_procs->rewind(cdev->page_bfile, true, cdev->page_bfname);
            if (code < 0) return code;
        }
        cdev->page_info.bfile_end_pos = 0;
        clist_reset_page(cdev);
    } else {
        if (cdev->page_cfile != 0)
            cdev->page_info.io_procs->fseek(cdev->page_cfile, 0L, SEEK_END, cdev->page_cfname);
        if (cdev->page_bfile != 0)
            cdev->page_info.io_procs->fseek(cdev->page_bfile, 0L, SEEK_END, cdev->page_bfname);
    }
    code = clist_init(dev);             /* reinitialize */
    if (code >= 0)
        code = clist_emit_page_header(dev);

    return code;
}

/* ------ Writing ------ */

/* End a page by flushing the buffer and terminating the command list. */
int     /* ret 0 all-ok, -ve error code, or +1 ok w/low-mem warning */
clist_end_page(gx_device_clist_writer * cldev)
{
    int code;
    cmd_block cb;
    int ecode = 0;

    code = cmd_write_buffer(cldev, cmd_opv_end_page);
    if (code >= 0)
        ecode |= code;
    else
        ecode = code;

    /* If we have ICC profiles present in the cfile save the table now,
       along with the ICC profiles. Table is stored in band maxband + 1. */
    if ( cldev->icc_table != NULL ) {
        /* Save the table */
        code = clist_icc_writetable(cldev);
        /* Free the table */
        clist_free_icc_table(cldev->icc_table, cldev->memory);
        cldev->icc_table = NULL;
    }
    if (code >= 0) {
        code = clist_write_color_usage_array(cldev);
        if (code >= 0) {
            ecode |= code;
            /*
             * Write the terminating entry in the block file.
             * Note that because of copypage, there may be many such entries.
             */
            memset(&cb, 0, sizeof(cb)); /* Zero the block, including any padding */
            cb.band_min = cb.band_max = cmd_band_end;
            cb.pos = (cldev->page_cfile == 0 ? 0 : cldev->page_info.io_procs->ftell(cldev->page_cfile));
            code = cldev->page_info.io_procs->fwrite_chars(&cb, sizeof(cb), cldev->page_bfile);
            if (code > 0)
                code = 0;
        }
    }
    if (code >= 0) {
        ecode |= code;
        cldev->page_bfile_end_pos = cldev->page_info.io_procs->ftell(cldev->page_bfile);
    } else
        ecode = code;

    /* Reset warning margin to 0 to release reserve memory if mem files */
    if (cldev->page_bfile != 0)
        cldev->page_info.io_procs->set_memory_warning(cldev->page_bfile, 0);
    if (cldev->page_cfile != 0)
        cldev->page_info.io_procs->set_memory_warning(cldev->page_cfile, 0);

#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':')) {
        if (cb.pos <= 0xFFFFFFFF)
            dmlprintf2(cldev->memory, "[:]clist_end_page at cfile=%lu, bfile=%lu\n",
                  (unsigned long)cb.pos, (unsigned long)cldev->page_bfile_end_pos);
        else
            dmlprintf3(cldev->memory, "[:]clist_end_page at cfile=%lu%0lu, bfile=%lu\n",
                (unsigned long) (cb.pos >> 32), (unsigned long) (cb.pos & 0xFFFFFFFF),
                (unsigned long)cldev->page_bfile_end_pos);
    }
#endif
    if (cldev->page_uses_transparency && gs_debug[':']) {
        /* count how many bands were skipped */
        int skip_count = 0;
        int band;

        for (band=0; band < cldev->nbands - 1; band++) {
            if (cldev->states[band].color_usage.trans_bbox.p.y >
                cldev->states[band].color_usage.trans_bbox.q.y)
                skip_count++;
        }
        dprintf2("%d bands skipped out of %d\n", skip_count, cldev->nbands);
    }

    return ecode;
}

gx_color_usage_bits
gx_color_index2usage(gx_device *dev, gx_color_index color)
{
    gx_color_usage_bits bits = 0;
    uchar i;

    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
        color = color ^ ~0;		/* white is 0 */

    for (i = 0; i < dev->color_info.num_components; i++) {
        if (color & dev->color_info.comp_mask[i])
            bits |= (1<<i);
    }
    return bits;
}

/* Write the target device's current parameter list */
static int      /* ret 0 all ok, -ve error */
clist_put_current_params(gx_device_clist_writer *cldev)
{
    gx_device *target = cldev->target;
    gs_c_param_list param_list;
    int code;

    /*
     * If a put_params call fails, the device will be left in a closed
     * state, but higher-level code won't notice this fact.  We flag this by
     * setting permanent_error, which prevents writing to the command list.
     */

    if (cldev->permanent_error)
        return cldev->permanent_error;
    gs_c_param_list_write(&param_list, cldev->memory);
    code = (*dev_proc(target, get_params))
        (target, (gs_param_list *)&param_list);
    if (code >= 0) {
        gs_c_param_list_read(&param_list);
        code = cmd_put_params( cldev, (gs_param_list *)&param_list );
    }
    gs_c_param_list_release(&param_list);

    return code;
}

/* ---------------- Driver interface ---------------- */

static int
clist_get_band(gx_device * dev, int y, int *band_start)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    int band_height = cdev->page_band_height;
    int start;

    if (y < 0)
        y = 0;
    else if (y >= dev->height)
        y = dev->height;
    *band_start = start = y - y % band_height;
    return min(dev->height - start, band_height);
}

/* ICC table operations.  See gxclist.h for details */
/* This checks the table for a hash code entry */
bool
clist_icc_searchtable(gx_device_clist_writer *cdev, int64_t hashcode)
{
    clist_icctable_t *icc_table = cdev->icc_table;
    clist_icctable_entry_t *curr_entry;

    if (icc_table == NULL)
        return(false);  /* No entry */
    curr_entry = icc_table->head;
    while(curr_entry != NULL) {
        if (curr_entry->serial_data.hashcode == hashcode){
            return(true);
        }
        curr_entry = curr_entry->next;
    }
     return(false);  /* No entry */
}

static void
clist_free_icc_table_contents(clist_icctable_t *icc_table)
{
    int number_entries;
    clist_icctable_entry_t *curr_entry, *next_entry;
    int k;

    number_entries = icc_table->tablesize;
    curr_entry = icc_table->head;
    for (k = 0; k < number_entries; k++) {
        next_entry = curr_entry->next;
        gsicc_adjust_profile_rc(curr_entry->icc_profile, -1, "clist_free_icc_table");
        gs_free_object(icc_table->memory, curr_entry, "clist_free_icc_table");
        curr_entry = next_entry;
    }
}

void
clist_icc_table_finalize(const gs_memory_t *memory, void * vptr)
{
    clist_icctable_t *icc_table = (clist_icctable_t *)vptr;

    clist_free_icc_table_contents(icc_table);
}

/* Free the table */
int
clist_free_icc_table(clist_icctable_t *icc_table, gs_memory_t *memory)
{
    if (icc_table == NULL)
        return(0);

    gs_free_object(icc_table->memory, icc_table, "clist_free_icc_table");
    return(0);
}

/* This serializes the ICC table and writes it out for maxband+1 */
int
clist_icc_writetable(gx_device_clist_writer *cldev)
{
    unsigned char *pbuf, *buf;
    clist_icctable_t *icc_table = cldev->icc_table;
    int number_entries = icc_table->tablesize;
    clist_icctable_entry_t *curr_entry;
    int size_data;
    int k;
    bool rend_is_valid;

    /* First we need to write out the ICC profiles themselves and update
       in the table where they will be stored and their size.  Set the
       rend cond valid flag prior to writing */
    curr_entry = icc_table->head;
    for ( k = 0; k < number_entries; k++ ){
        rend_is_valid = curr_entry->icc_profile->rend_is_valid;
        curr_entry->icc_profile->rend_is_valid = curr_entry->render_is_valid;
        curr_entry->serial_data.file_position = clist_icc_addprofile(cldev, curr_entry->icc_profile, &size_data);
        curr_entry->icc_profile->rend_is_valid = rend_is_valid;
        curr_entry->serial_data.size = size_data;
        gsicc_adjust_profile_rc(curr_entry->icc_profile, -1, "clist_icc_writetable");
        curr_entry->icc_profile = NULL;
        curr_entry = curr_entry->next;
    }

    /* Now serialize the table data */
    size_data = number_entries*sizeof(clist_icc_serial_entry_t) + sizeof(number_entries);
    buf = gs_alloc_bytes(cldev->memory, size_data, "clist_icc_writetable");
    if (buf == NULL)
        return gs_rethrow(-1, "insufficient memory for icc table buffer");
    pbuf = buf;
    memcpy(pbuf, &number_entries, sizeof(number_entries));
    pbuf += sizeof(number_entries);
    curr_entry = icc_table->head;
    for (k = 0; k < number_entries; k++) {
        memcpy(pbuf, &(curr_entry->serial_data), sizeof(clist_icc_serial_entry_t));
        pbuf += sizeof(clist_icc_serial_entry_t);
        curr_entry = curr_entry->next;
    }
    /* Now go ahead and save the table data */
    cmd_write_pseudo_band(cldev, buf, size_data, ICC_TABLE_OFFSET);
    gs_free_object(cldev->memory, buf, "clist_icc_writetable");
    return(0);
}

/* This write the actual data out to the cfile */

int64_t
clist_icc_addprofile(gx_device_clist_writer *cldev, cmm_profile_t *iccprofile, int *size)
{

    clist_file_ptr cfile = cldev->page_cfile;
    int64_t fileposit;
#if defined(DEBUG) || defined(PACIFY_VALGRIND)
    gsicc_serialized_profile_t profile_data = { 0 };
#else
    gsicc_serialized_profile_t profile_data;
#endif
    int count1, count2;

    /* Get the current position */
    fileposit = cldev->page_info.io_procs->ftell(cfile);
    /* Get the serialized header */
    gsicc_profile_serialize(&profile_data, iccprofile);
    /* Write the header */
    if_debug1m('l', cldev->memory, "[l]writing icc profile in cfile at pos %"PRId64"\n",fileposit);
    count1 = cldev->page_info.io_procs->fwrite_chars(&profile_data, GSICC_SERIALIZED_SIZE, cfile);
    /* Now write the profile */
    count2 = cldev->page_info.io_procs->fwrite_chars(iccprofile->buffer, iccprofile->buffer_size, cfile);
    /* Return where we wrote this in the cfile */
    *size = count1 + count2;
    return(fileposit);
}

/* This add a new entry into the table */

int
clist_icc_addentry(gx_device_clist_writer *cdev, int64_t hashcode_in, cmm_profile_t *icc_profile)
{

    clist_icctable_t *icc_table = cdev->icc_table;
    clist_icctable_entry_t *entry, *curr_entry;
    int k;
    int64_t hashcode;
    gs_memory_t *stable_mem = cdev->memory->stable_memory;

    /* If the hash code is not valid then compute it now */
    if (icc_profile->hash_is_valid == false) {
        gsicc_get_icc_buff_hash(icc_profile->buffer, &hashcode,
                                icc_profile->buffer_size);
        icc_profile->hashcode = hashcode;
        icc_profile->hash_is_valid = true;
    } else {
        hashcode = hashcode_in;
    }
    if ( icc_table == NULL ) {
        entry = (clist_icctable_entry_t *) gs_alloc_struct(stable_mem,
                               clist_icctable_entry_t, &st_clist_icctable_entry,
                               "clist_icc_addentry");
        if (entry == NULL)
            return gs_rethrow(-1, "insufficient memory to allocate entry in icc table");
#ifdef PACIFY_VALGRIND
        /* Avoid uninitialised padding upsetting valgrind when it's written
         * into the clist. */
        memset(entry, 0, sizeof(*entry));
#endif
        entry->next = NULL;
        entry->serial_data.hashcode = hashcode;
        entry->serial_data.size = -1;
        entry->serial_data.file_position = -1;
        entry->icc_profile = icc_profile;
        entry->render_is_valid = icc_profile->rend_is_valid;
        gsicc_adjust_profile_rc(icc_profile, 1, "clist_icc_addentry");
        icc_table = gs_alloc_struct(stable_mem, clist_icctable_t,
                                    &st_clist_icctable, "clist_icc_addentry");
        if (icc_table == NULL)
            return gs_rethrow(-1, "insufficient memory to allocate icc table");
        icc_table->tablesize = 1;
        icc_table->head = entry;
        icc_table->final = entry;
        icc_table->memory = stable_mem;
        /* For now, we are just going to put the icc_table itself
            at band_range_max + 1.  The ICC profiles are written
            in the cfile at the current stored file position*/
        cdev->icc_table = icc_table;
    } else {
        /* First check if we already have this entry */
        curr_entry = icc_table->head;
        for (k = 0; k < icc_table->tablesize; k++) {
            if (curr_entry->serial_data.hashcode == hashcode)
                return 0;  /* A hit */
            curr_entry = curr_entry->next;
        }
         /* Add a new ICC profile */
        entry =
            (clist_icctable_entry_t *) gs_alloc_struct(icc_table->memory,
                                                       clist_icctable_entry_t,
                                                       &st_clist_icctable_entry,
                                                       "clist_icc_addentry");
        if (entry == NULL)
            return gs_rethrow(-1, "insufficient memory to allocate entry in icc table");
#ifdef PACIFY_VALGRIND
        /* Avoid uninitialised padding upsetting valgrind when it's written
         * into the clist. */
        memset(entry, 0, sizeof(*entry));
#endif
        entry->next = NULL;
        entry->serial_data.hashcode = hashcode;
        entry->serial_data.size = -1;
        entry->serial_data.file_position = -1;
        entry->icc_profile = icc_profile;
        entry->render_is_valid = icc_profile->rend_is_valid;
        gsicc_adjust_profile_rc(icc_profile, 1, "clist_icc_addentry");
        icc_table->final->next = entry;
        icc_table->final = entry;
        icc_table->tablesize++;
    }
    return(0);
}

/* This writes out the color_usage_array for maxband+1 */
int
clist_write_color_usage_array(gx_device_clist_writer *cldev)
{
   gx_color_usage_t *color_usage_array;
   int i, size_data = cldev->nbands * sizeof(gx_color_usage_t);

    /* Now serialize the table data */
    color_usage_array = (gx_color_usage_t *)gs_alloc_bytes(cldev->memory, size_data,
                                       "clist_write_color_usage_array");
    if (color_usage_array == NULL)
        return gs_rethrow(-1, "insufficient memory for color_usage_array");
    for (i = 0; i < cldev->nbands; i++) {
        memcpy(&(color_usage_array[i]), &(cldev->states[i].color_usage), sizeof(gx_color_usage_t));
    }
    /* Now go ahead and save the table data */
    cmd_write_pseudo_band(cldev, (unsigned char *)color_usage_array,
                          size_data, COLOR_USAGE_OFFSET);
    gs_free_object(cldev->memory, color_usage_array, "clist_write_color_usage_array");
    return(0);
}

/* Compute color_usage over a Y range while writing clist */
/* Sets color_usage fields and range_start.               */
/* Returns range end (max dev->height)                    */
/* NOT expected to be used. */
int
clist_writer_color_usage(gx_device_clist_writer *cldev, int y, int height,
                     gx_color_usage_t *color_usage, int *range_start)
{
        gx_color_usage_bits or = 0;
        bool slow_rop = false;
        int i, band_height = cldev->page_band_height;
        int start = y / band_height, end = (y + height) / band_height;

        for (i = start; i < end; ++i) {
            or |= cldev->states[i].color_usage.or;
            slow_rop |= cldev->states[i].color_usage.slow_rop;
        }
        color_usage->or = or;
        color_usage->slow_rop = slow_rop;
        *range_start = start * band_height;
        return min(end * band_height, cldev->height) - *range_start;
}

int
clist_writer_push_no_cropping(gx_device_clist_writer *cdev)
{
    clist_writer_cropping_buffer_t *buf = gs_alloc_struct(cdev->memory,
                clist_writer_cropping_buffer_t,
                &st_clist_writer_cropping_buffer, "clist_writer_transparency_push");

    if (buf == NULL)
        return_error(gs_error_VMerror);
    if_debug4m('v', cdev->memory, "[v]push cropping[%d], min=%d, max=%d, buf=%p\n",
               cdev->cropping_level, cdev->cropping_min, cdev->cropping_max, buf);
    buf->next = cdev->cropping_stack;
    cdev->cropping_stack = buf;
    buf->cropping_min = cdev->cropping_min;
    buf->cropping_max = cdev->cropping_max;
    buf->mask_id = cdev->mask_id;
    buf->temp_mask_id = cdev->temp_mask_id;
    cdev->cropping_level++;
    return 0;
}

int
clist_writer_push_cropping(gx_device_clist_writer *cdev, int ry, int rheight)
{
    int code = clist_writer_push_no_cropping(cdev);

    if (code < 0)
        return 0;
    cdev->cropping_min = max(cdev->cropping_min, ry);
    cdev->cropping_max = min(cdev->cropping_max, ry + rheight);
    return 0;
}

int
clist_writer_pop_cropping(gx_device_clist_writer *cdev)
{
    clist_writer_cropping_buffer_t *buf = cdev->cropping_stack;

    if (buf == NULL)
        return_error(gs_error_unregistered); /*Must not happen. */
    cdev->cropping_min = buf->cropping_min;
    cdev->cropping_max = buf->cropping_max;
    cdev->mask_id = buf->mask_id;
    cdev->temp_mask_id = buf->temp_mask_id;
    cdev->cropping_stack = buf->next;
    cdev->cropping_level--;
    if_debug4m('v', cdev->memory, "[v]pop cropping[%d] min=%d, max=%d, buf=%p\n",
               cdev->cropping_level, cdev->cropping_min, cdev->cropping_max, buf);
    gs_free_object(cdev->memory, buf, "clist_writer_transparency_pop");
    return 0;
}

int
clist_writer_check_empty_cropping_stack(gx_device_clist_writer *cdev)
{
    if (cdev->cropping_stack != NULL) {
        if_debug1m('v', cdev->memory, "[v]Error: left %d cropping(s)\n", cdev->cropping_level);
        return_error(gs_error_unregistered); /* Must not happen */
    }
    return 0;
}

/* Retrieve total size for cfile and bfile. */
int clist_data_size(const gx_device_clist *cdev, int select)
{
    const gx_band_page_info_t *pinfo = &cdev->common.page_info;
    clist_file_ptr pfile = (!select ? pinfo->bfile : pinfo->cfile);
    const char *fname = (!select ? pinfo->bfname : pinfo->cfname);
    int code, size;

    code = pinfo->io_procs->fseek(pfile, 0, SEEK_END, fname);
    if (code < 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    code = pinfo->io_procs->ftell(pfile);
    if (code < 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    size = code;
    return size;
}

/* Get command list data. */
int
clist_get_data(const gx_device_clist *cdev, int select, int64_t offset, byte *buf, int length)
{
    const gx_band_page_info_t *pinfo = &cdev->common.page_info;
    clist_file_ptr pfile = (!select ? pinfo->bfile : pinfo->cfile);
    const char *fname = (!select ? pinfo->bfname : pinfo->cfname);
    int code;

    code = pinfo->io_procs->fseek(pfile, offset, SEEK_SET, fname);
    if (code < 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    /* This assumes that fread_chars doesn't return prematurely
       when the buffer is not fully filled and the end of stream is not reached. */
    return pinfo->io_procs->fread_chars(buf, length, pfile);
}

/* Put command list data. */
int
clist_put_data(const gx_device_clist *cdev, int select, int64_t offset, const byte *buf, int length)
{
    const gx_band_page_info_t *pinfo = &cdev->common.page_info;
    clist_file_ptr pfile = (!select ? pinfo->bfile : pinfo->cfile);
    int64_t code;

    code = pinfo->io_procs->ftell(pfile);
    if (code < 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (code != offset) {
        /* Assuming a consecutive writing only. */
        return_error(gs_error_unregistered); /* Must not happen. */
    }
    /* This assumes that fwrite_chars doesn't return prematurely
       when the buffer is not fully written, except with an error. */
    return pinfo->io_procs->fwrite_chars(buf, length, pfile);
}

gx_device_clist *
clist_make_accum_device(gx_device *target, const char *dname, void *base, int space,
                        gx_device_buf_procs_t *buf_procs, gx_band_params_t *band_params,
                        bool use_memory_clist, bool uses_transparency,
                        gs_pattern1_instance_t *pinst)
{
        gs_memory_t *mem = target->memory;
        gx_device_clist *cdev = gs_alloc_struct(mem, gx_device_clist,
                        &st_device_clist, "clist_make_accum_device");
        gx_device_clist_writer *cwdev = (gx_device_clist_writer *)cdev;

        if (cdev == 0)
            return 0;
        memset(cdev, 0, sizeof(*cdev));
        cwdev->params_size = sizeof(gx_device_clist);
        cwdev->static_procs = NULL;
        cwdev->dname = dname;
        cwdev->memory = mem;
        cwdev->stype = &st_device_clist;
        cwdev->stype_is_dynamic = false;
        rc_init(cwdev, mem, 1);
        cwdev->retained = true;
        cwdev->is_open = false;
        cwdev->color_info = target->color_info;
        cwdev->pinst = pinst;
        cwdev->cached_colors = target->cached_colors;
        if (pinst != NULL) {
            cwdev->width = pinst->size.x;
            cwdev->height = pinst->size.y;
            cwdev->band_params.BandHeight = pinst->size.y;
        } else {
            cwdev->width = target->width;
            cwdev->height = target->height;
        }
        cwdev->LeadingEdge = target->LeadingEdge;
        cwdev->is_planar = target->is_planar;
        cwdev->HWResolution[0] = target->HWResolution[0];
        cwdev->HWResolution[1] = target->HWResolution[1];
        cwdev->icc_cache_cl = NULL;
        cwdev->icc_table = NULL;
        cwdev->UseCIEColor = target->UseCIEColor;
        cwdev->LockSafetyParams = true;
        cwdev->procs = gs_clist_device_procs;
        gx_device_copy_color_params((gx_device *)cwdev, target);
        rc_assign(cwdev->target, target, "clist_make_accum_device");
        clist_init_io_procs(cdev, use_memory_clist);
        cwdev->data = base;
        cwdev->data_size = space;
        memcpy (&(cwdev->buf_procs), buf_procs, sizeof(gx_device_buf_procs_t));
        cwdev->page_uses_transparency = uses_transparency;
        cwdev->band_params.BandWidth = cwdev->width;
        cwdev->band_params.BandBufferSpace = 0;
        cwdev->do_not_open_or_close_bandfiles = false;
        cwdev->bandlist_memory = mem->non_gc_memory;
        set_dev_proc(cwdev, get_clipping_box, gx_default_get_clipping_box);
        set_dev_proc(cwdev, get_profile, gx_forward_get_profile);
        set_dev_proc(cwdev, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
        cwdev->graphics_type_tag = target->graphics_type_tag;		/* initialize to same as target */
        cwdev->interpolate_control = target->interpolate_control;	/* initialize to same as target */

        /* to be set by caller: cwdev->finalize = finalize; */

        /* Fields left zeroed :
            int   max_fill_band;
            int   is_printer;
            float MediaSize[2];
            float ImagingBBox[4];
            bool  ImagingBBox_set;
            float Margins[2];
            float HWMargins[4];
            long  PageCount;
            long  ShowpageCount;
            int   NumCopies;
            bool  NumCopies_set;
            bool  IgnoreNumCopies;
            int   disable_mask;
            gx_page_device_procs page_procs;

        */
        return cdev;
}
