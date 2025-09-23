/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Halftone rendering for imaging library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsbitops.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxdcolor.h"
#include "gxfixed.h"
#include "gxdevice.h"		/* for gzht.h */
#include "gxgstate.h"
#include "gzht.h"
#include "gsserial.h"

/* Define the binary halftone device color type. */
/* The type descriptor must be public for Pattern types. */
gs_public_st_composite(st_dc_ht_binary, gx_device_color, "dc_ht_binary",
                       dc_ht_binary_enum_ptrs, dc_ht_binary_reloc_ptrs);
static dev_color_proc_save_dc(gx_dc_ht_binary_save_dc);
static dev_color_proc_get_dev_halftone(gx_dc_ht_binary_get_dev_halftone);
static dev_color_proc_load(gx_dc_ht_binary_load);
static dev_color_proc_fill_rectangle(gx_dc_ht_binary_fill_rectangle);
static dev_color_proc_fill_masked(gx_dc_ht_binary_fill_masked);
static dev_color_proc_equal(gx_dc_ht_binary_equal);
static dev_color_proc_write(gx_dc_ht_binary_write);
static dev_color_proc_read(gx_dc_ht_binary_read);
const gx_device_color_type_t
      gx_dc_type_data_ht_binary =
{&st_dc_ht_binary,
 gx_dc_ht_binary_save_dc, gx_dc_ht_binary_get_dev_halftone,
 gx_dc_ht_get_phase,
 gx_dc_ht_binary_load, gx_dc_ht_binary_fill_rectangle,
 gx_dc_ht_binary_fill_masked, gx_dc_ht_binary_equal,
 gx_dc_ht_binary_write, gx_dc_ht_binary_read,
 gx_dc_ht_binary_get_nonzero_comps
};

#undef gx_dc_type_ht_binary
const gx_device_color_type_t *const gx_dc_type_ht_binary =
&gx_dc_type_data_ht_binary;

#define gx_dc_type_ht_binary (&gx_dc_type_data_ht_binary)
/* GC procedures */
static
ENUM_PTRS_WITH(dc_ht_binary_enum_ptrs, gx_device_color *cptr) return 0;
ENUM_PTR(0, gx_device_color, colors.binary.b_ht);
case 1:
{
    gx_ht_tile *tile = cptr->colors.binary.b_tile;

    ENUM_RETURN(tile ? tile - tile->index : 0);
}
ENUM_PTRS_END
static RELOC_PTRS_WITH(dc_ht_binary_reloc_ptrs, gx_device_color *cptr)
{
    gx_ht_tile *tile = cptr->colors.binary.b_tile;
    uint index = tile ? tile->index : 0;

    RELOC_PTR(gx_device_color, colors.binary.b_ht);
    RELOC_TYPED_OFFSET_PTR(gx_device_color, colors.binary.b_tile, index);
}
RELOC_PTRS_END
#undef cptr

/* Other GC procedures */
private_st_ht_tiles();
static
ENUM_PTRS_BEGIN_PROC(ht_tiles_enum_ptrs)
{
    return 0;
}
ENUM_PTRS_END_PROC
static RELOC_PTRS_BEGIN(ht_tiles_reloc_ptrs)
{
    /* Reset the bitmap pointers in the tiles. */
    /* We know the first tile points to the base of the bits. */
    gx_ht_tile *ht_tiles = vptr;
    byte *bits = ht_tiles->tiles.data;
    uint diff;

    if (bits == 0)
        return;
    RELOC_VAR(bits);
    if (size == size_of(gx_ht_tile)) {	/* only 1 tile */
        ht_tiles->tiles.data = bits;
        return;
    }
    diff = ht_tiles[1].tiles.data - ht_tiles[0].tiles.data;
    for (; size; ht_tiles++, size -= size_of(gx_ht_tile), bits += diff) {
        ht_tiles->tiles.data = bits;
    }
}
RELOC_PTRS_END
private_st_ht_cache();

/* Return the default sizes of the halftone cache. */
uint
gx_ht_cache_default_tiles(void)
{
#ifdef DEBUG
    return (gs_debug_c('.') ? max_ht_cached_tiles_SMALL :
            max_ht_cached_tiles);
#else
    return max_ht_cached_tiles;
#endif
}
uint
gx_ht_cache_default_bits_size(void)
{
#ifdef DEBUG
    return (gs_debug_c('.') ? max_ht_cache_bits_size_SMALL :
            max_ht_cache_bits_size);
#else
    return max_ht_cache_bits_size;
#endif
}

/* Allocate a halftone cache. max_bits_size is number of bytes */
gx_ht_cache *
gx_ht_alloc_cache(gs_memory_t * mem, uint max_tiles, uint max_bits_size)
{
    gx_ht_cache *pcache =
    gs_alloc_struct(mem, gx_ht_cache, &st_ht_cache,
                    "alloc_ht_cache(struct)");
    byte *tbits =
        gs_alloc_bytes(mem, max_bits_size, "alloc_ht_cache(bits)");
    gx_ht_tile *ht_tiles =
        gs_alloc_struct_array(mem, max_tiles, gx_ht_tile, &st_ht_tiles,
                              "alloc_ht_cache(ht_tiles)");

    if (pcache == 0 || tbits == 0 || ht_tiles == 0) {
        gs_free_object(mem, ht_tiles, "alloc_ht_cache(ht_tiles)");
        gs_free_object(mem, tbits, "alloc_ht_cache(bits)");
        gs_free_object(mem, pcache, "alloc_ht_cache(struct)");
        return 0;
    }
    pcache->bits = tbits;
    pcache->bits_size = max_bits_size;
    pcache->ht_tiles = ht_tiles;
    pcache->num_tiles = max_tiles;
    pcache->order.cache = pcache;
    pcache->order.transfer = 0;
    gx_ht_clear_cache(pcache);
    return pcache;
}

/* Free a halftone cache. */
void
gx_ht_free_cache(gs_memory_t * mem, gx_ht_cache * pcache)
{
    gs_free_object(mem, pcache->ht_tiles, "free_ht_cache(ht_tiles)");
    gs_free_object(mem, pcache->bits, "free_ht_cache(bits)");
    gs_free_object(mem, pcache, "free_ht_cache(struct)");
}

/* Render a given level into a halftone cache. */
static int render_ht(gx_ht_tile *, int, const gx_ht_order *,
                      gx_bitmap_id);
static gx_ht_tile *
gx_render_ht_default(gx_ht_cache * pcache, int b_level)
{
    const gx_ht_order *porder = &pcache->order;
    int level = porder->levels[b_level];
    gx_ht_tile *bt;

    if (pcache->num_cached < porder->num_levels )
        bt = &pcache->ht_tiles[level / pcache->levels_per_tile];
    else
        bt =  &pcache->ht_tiles[b_level];	/* one tile per b_level */

    if (bt->level != level) {
        int code = render_ht(bt, level, porder, pcache->base_id + b_level);

        if (code < 0)
            return 0;
    }
    return bt;
}

/* save information about the operand binary halftone color */
static void
gx_dc_ht_binary_save_dc(const gx_device_color * pdevc,
                        gx_device_color_saved * psdc)
{
    psdc->type = pdevc->type;
    psdc->colors.binary.b_color[0] = pdevc->colors.binary.color[0];
    psdc->colors.binary.b_color[1] = pdevc->colors.binary.color[1];
    psdc->colors.binary.b_level = pdevc->colors.binary.b_level;
    psdc->colors.binary.b_index = pdevc->colors.binary.b_index;
    psdc->phase = pdevc->phase;
}

/* get the halftone used for a binary halftone color */
static const gx_device_halftone *
gx_dc_ht_binary_get_dev_halftone(const gx_device_color * pdevc)
{
    return pdevc->colors.binary.b_ht;
}

/* Load the device color into the halftone cache if needed. */
static int
gx_dc_ht_binary_load(gx_device_color * pdevc, const gs_gstate * pgs,
                     gx_device * dev, gs_color_select_t select)
{
    int component_index = pdevc->colors.binary.b_index;
    const gx_ht_order *porder;
    gx_ht_cache *pcache;

    if (component_index < 0) {
        porder = &pdevc->colors.binary.b_ht->order;
    } else {
        int i = 0;

        /* We can get here with pgs being NULL from the clist. In that case we can't check the saved halftone
         * against the graphics state, because there is no graphics state. But I believe it should not be
         * possible for this to cause a problem with the clist.
         */
        if (pgs != NULL) {
            /* Ensure the halftone saved in the device colour matches one of the
             * object-type device halftones. It should not be possible for this not
             * to be the case, but an image with a procedural data source which executes
             * more grestores than gsaves can restore away the halftone that was in
             * force at the start of the image, while we're trying to still use it.
             * If that happens we cna't do anything but throw an error.
             */
            for (i=0;i < HT_OBJTYPE_COUNT;i++) {
                if (pdevc->colors.binary.b_ht == pgs->dev_ht[i])
                    break;
            }
            if (i == HT_OBJTYPE_COUNT)
                return_error(gs_error_unknownerror);
        }
        porder = &pdevc->colors.binary.b_ht->components[component_index].corder;

    }
    pcache = porder->cache;
    if (pcache->order.bit_data != porder->bit_data) {
        /* I don't think this should be possible, but just in case */
        if (pgs == NULL)
            return_error(gs_error_unknownerror);
        gx_ht_init_cache(pgs->memory, pcache, porder);
    }
    /*
     * We do not load the cache now.  Instead we wait until we are ready
     * to actually render the color.  This allows multiple colors to be
     * loaded without cache conflicts.  (Cache conflicts can occur when
     * if two device colors use the same cache elements.  This can occur
     * when the tile size is large enough that we do not have a separate
     * tile for each half tone level.)  See gx_dc_ht_binary_load_cache.
     */
    pdevc->colors.binary.b_tile = NULL;
    return 0;
}

/*
 * Load the half tone tile in the halftone cache.
 */
static int
gx_dc_ht_binary_load_cache(const gx_device_color * pdevc)
{
    int component_index = pdevc->colors.binary.b_index;
    const gx_ht_order *porder =
         &pdevc->colors.binary.b_ht->components[component_index].corder;
    gx_ht_cache *pcache = porder->cache;
    int b_level = pdevc->colors.binary.b_level;
    int level = porder->levels[b_level];
    gx_ht_tile *bt;

    if (pcache->num_cached < porder->num_levels )
        bt = &pcache->ht_tiles[level / pcache->levels_per_tile];
    else
        bt =  &pcache->ht_tiles[b_level];	/* one tile per b_level */

    if (bt->level != level) {
        int code = render_ht(bt, level, porder, pcache->base_id + b_level);

        if (code < 0)
            return_error(gs_error_Fatal);
    }
    ((gx_device_color *)pdevc)->colors.binary.b_tile = bt;
    return 0;
}

/* Fill a rectangle with a binary halftone. */
/* Note that we treat this as "texture" for RasterOp. */
static int
gx_dc_ht_binary_fill_rectangle(const gx_device_color * pdevc, int x, int y,
                  int w, int h, gx_device * dev, gs_logical_operation_t lop,
                               const gx_rop_source_t * source)
{
    gx_rop_source_t no_source;

    fit_fill(dev, x, y, w, h);
    /* Load the halftone cache for the color */
    gx_dc_ht_binary_load_cache(pdevc);
    /*
     * Observation of H-P devices and documentation yields confusing
     * evidence about whether white pixels in halftones are always
     * opaque.  It appears that for black-and-white devices, these
     * pixels are *not* opaque.
     */
    if (dev->color_info.depth > 1)
        lop &= ~lop_T_transparent;
    if (source == NULL && lop_no_S_is_T(lop))
        return (*dev_proc(dev, strip_tile_rectangle)) (dev,
                                        &pdevc->colors.binary.b_tile->tiles,
                                  x, y, w, h, pdevc->colors.binary.color[0],
                                              pdevc->colors.binary.color[1],
                                            pdevc->phase.x, pdevc->phase.y);
    /* Adjust the logical operation per transparent colors. */
    if (pdevc->colors.binary.color[0] == gx_no_color_index)
        lop = rop3_use_D_when_T_0(lop);
    if (pdevc->colors.binary.color[1] == gx_no_color_index)
        lop = rop3_use_D_when_T_1(lop);
    if (source == NULL)
        set_rop_no_source(source, no_source, dev);
    return (*dev_proc(dev, strip_copy_rop2))
                             (dev, source->sdata,
                              source->sourcex, source->sraster, source->id,
                              (source->use_scolors ? source->scolors : NULL),
                              &pdevc->colors.binary.b_tile->tiles,
                              pdevc->colors.binary.color,
                              x, y, w, h, pdevc->phase.x, pdevc->phase.y,
                              lop, source->planar_height);
}

static int
gx_dc_ht_binary_fill_masked(const gx_device_color * pdevc, const byte * data,
        int data_x, int raster, gx_bitmap_id id, int x, int y, int w, int h,
                   gx_device * dev, gs_logical_operation_t lop, bool invert)
{
    /*
     * Load the halftone cache for the color.  We do not do it earlier
     * because for small halftone caches, each cache tile may be used for
     * for more than one halftone level.  This can cause conflicts if more
     * than one device color has been set and they use the same cache
     * entry.
     */
    int code = gx_dc_ht_binary_load_cache(pdevc);

    if (code < 0)
        return code;
    return gx_dc_default_fill_masked(pdevc, data, data_x, raster, id,
                                        x, y, w, h, dev, lop, invert);
}

/* Compare two binary halftones for equality. */
static bool
gx_dc_ht_binary_equal(const gx_device_color * pdevc1,
                      const gx_device_color * pdevc2)
{
    return pdevc2->type == pdevc1->type &&
        pdevc1->phase.x == pdevc2->phase.x &&
        pdevc1->phase.y == pdevc2->phase.y &&
        gx_dc_binary_color0(pdevc1) == gx_dc_binary_color0(pdevc2) &&
        gx_dc_binary_color1(pdevc1) == gx_dc_binary_color1(pdevc2) &&
        pdevc1->colors.binary.b_level == pdevc2->colors.binary.b_level;
}

/*
 * Flags to indicate the pieces of a binary halftone that are included
 * in its string representation. The first byte of the string holds this
 * set of flags.
 *
 * The binary halftone tile is never transmitted as part of the string
 * representation, so there is also no flag bit for it.
 */
enum {
    dc_ht_binary_has_color0 = 0x01,
    dc_ht_binary_has_color1 = 0x02,
    dc_ht_binary_has_level = 0x04,
    dc_ht_binary_has_index = 0x08,
    dc_ht_binary_has_phase_x = 0x10,
    dc_ht_binary_has_phase_y = 0x20,
};

/*
 * Serialize a binany halftone device color.
 *
 * Operands:
 *
 *  pdevc       pointer to device color to be serialized
 *
 *  psdc        pointer ot saved version of last serialized color (for
 *              this band)
 *
 *  dev         pointer to the current device, used to retrieve process
 *              color model information
 *
 *  pdata       pointer to buffer in which to write the data
 *
 *  psize       pointer to a location that, on entry, contains the size of
 *              the buffer pointed to by pdata; on return, the size of
 *              the data required or actually used will be written here.
 *
 * Returns:
 *  1, with *psize set to 0, if *psdc and *pdevc represent the same color
 *
 *  0, with *psize set to the amount of data written, if everything OK
 *
 *  gs_error_rangecheck, with *psize set to the size of buffer required,
 *  if *psize was not large enough
 *
 *  < 0, != gs_error_rangecheck, in the event of some other error; in this
 *  case *psize is not changed.
 */
static int
gx_dc_ht_binary_write(
    const gx_device_color *         pdevc,
    const gx_device_color_saved *   psdc0,
    const gx_device *               dev,
    int64_t			    offset,
    byte *                          pdata,
    uint *                          psize )
{
    int                             req_size = 1;   /* flag bits */
    int                             flag_bits = 0;
    uint                            tmp_size;
    byte *                          pdata0 = pdata;
    const gx_device_color_saved *   psdc = psdc0;
    int                             code;

    if (offset != 0)
        return_error(gs_error_unregistered); /* Not implemented yet. */

    /* check if operand and saved colors are the same type */
    if (psdc != 0 && psdc->type != pdevc->type)
        psdc = 0;

    /* check for the information that must be transmitted */
    if ( psdc == 0                                                      ||
         pdevc->colors.binary.color[0] != psdc->colors.binary.b_color[0]  ) {
        flag_bits |= dc_ht_binary_has_color0;
        tmp_size = 0;
        (void)gx_dc_write_color( pdevc->colors.binary.color[0],
                                 dev,
                                 pdata,
                                 &tmp_size );
        req_size += tmp_size;
    }
    if ( psdc == NULL ||
         pdevc->colors.binary.color[1] != psdc->colors.binary.b_color[1]  ) {
        flag_bits |= dc_ht_binary_has_color1;
        tmp_size = 0;
        (void)gx_dc_write_color( pdevc->colors.binary.color[1],
                                 dev,
                                 pdata,
                                 &tmp_size );
        req_size += tmp_size;
    }

    if ( psdc == NULL ||
         pdevc->colors.binary.b_level != psdc->colors.binary.b_level  ) {
        flag_bits |= dc_ht_binary_has_level;
        req_size += enc_u_sizew(pdevc->colors.binary.b_level);
    }

    if ( psdc == NULL ||
         pdevc->colors.binary.b_index != psdc->colors.binary.b_index  ) {
        flag_bits |= dc_ht_binary_has_index;
        req_size += 1;
    }

    if ( psdc == NULL ||
         pdevc->phase.x != psdc->phase.x ) {
        flag_bits |= dc_ht_binary_has_phase_x;
        req_size += enc_u_sizew(pdevc->phase.x);
    }

    if ( psdc == NULL ||
         pdevc->phase.y != psdc->phase.y ) {
        flag_bits |= dc_ht_binary_has_phase_y;
        req_size += enc_u_sizew(pdevc->phase.y);
    }

    /* check if there is anything to be done */
    if (flag_bits == 0) {
        *psize = 0;
        return 1;
    }

    /* check if sufficient space has been provided */
    if (req_size > *psize) {
        *psize = req_size;
        return_error(gs_error_rangecheck);
    }

    /* write out the flag byte */
    *pdata++ = (byte)flag_bits;

    /* write out such other parts of the device color as are required */
    if ((flag_bits & dc_ht_binary_has_color0) != 0) {
        tmp_size = req_size - (pdata - pdata0);
        code = gx_dc_write_color( pdevc->colors.binary.color[0],
                                  dev,
                                  pdata,
                                  &tmp_size );
        if (code < 0)
            return code;
        pdata += tmp_size;
    }
    if ((flag_bits & dc_ht_binary_has_color1) != 0) {
        tmp_size = req_size - (pdata - pdata0);
        code = gx_dc_write_color( pdevc->colors.binary.color[1],
                                  dev,
                                  pdata,
                                  &tmp_size );
        if (code < 0)
            return code;
        pdata += tmp_size;
    }
    if ((flag_bits & dc_ht_binary_has_level) != 0)
        enc_u_putw(pdevc->colors.binary.b_level, pdata);
    if ((flag_bits & dc_ht_binary_has_index) != 0)
        *pdata++ = pdevc->colors.binary.b_index;
    if ((flag_bits & dc_ht_binary_has_phase_x) != 0)
        enc_u_putw(pdevc->phase.x, pdata);
    if ((flag_bits & dc_ht_binary_has_phase_y) != 0)
        enc_u_putw(pdevc->phase.y, pdata);

    *psize = pdata - pdata0;
    return 0;
}

/*
 * Reconstruct a binary halftone device color from its serial representation.
 *
 * Operands:
 *
 *  pdevc       pointer to the location in which to write the
 *              reconstructed device color
 *
 *  pgs         pointer to the current gs_gstate (to access the
 *              current halftone)
 *
 *  prior_devc  pointer to the current device color (this is provided
 *              separately because the device color is not part of the
 *              gs_gstate)
 *
 *  dev         pointer to the current device, used to retrieve process
 *              color model information
 *
 *  pdata       pointer to the buffer to be read
 *
 *  size        size of the buffer to be read; this should be large
 *              enough to hold the entire color description
 *
 *  mem         pointer to the memory to be used for allocations
 *              (ignored here)
 *
 * Returns:
 *
 *  # of bytes read if everthing OK, < 0 in the event of an error
 */
static int
gx_dc_ht_binary_read(
    gx_device_color *       pdevc,
    const gs_gstate        * pgs,
    const gx_device_color * prior_devc,
    const gx_device *       dev,        /* ignored */
    int64_t		    offset,
    const byte *            pdata,
    uint                    size,
    gs_memory_t *           mem,        /* ignored */
    int                     x0,
    int                     y0)
{
    gx_device_color         devc;
    const byte *            pdata0 = pdata;
    int                     code, flag_bits;

    if (offset != 0)
        return_error(gs_error_unregistered); /* Not implemented yet. */

    /* if prior information is available, use it */
    if (prior_devc != 0 && prior_devc->type == gx_dc_type_ht_binary)
        devc = *prior_devc;
    else
        memset(&devc, 0, sizeof(devc));   /* clear pointers */
    devc.type = gx_dc_type_ht_binary;

    /* the halftone is always taken from the gs_gstate */
    devc.colors.binary.b_ht = pgs->dev_ht[HT_OBJTYPE_DEFAULT];

    /* cache is not provided until the device color is used */
    devc.colors.binary.b_tile = 0;

    /* verify the minimum amount of information */
    if (size == 0)
        return_error(gs_error_rangecheck);
    size --;
    flag_bits = *pdata++;

    /* read the other information provided */
    if ((flag_bits & dc_ht_binary_has_color0) != 0) {
        code = gx_dc_read_color( &devc.colors.binary.color[0],
                                 dev,
                                 pdata,
                                 size );
        if (code < 0)
            return code;
        size -= code;
        pdata += code;
    }
    if ((flag_bits & dc_ht_binary_has_color1) != 0) {
        code = gx_dc_read_color( &devc.colors.binary.color[1],
                                 dev,
                                 pdata,
                                 size );
        if (code < 0)
            return code;
        size -= code;
        pdata += code;
    }
    if ((flag_bits & dc_ht_binary_has_level) != 0) {
        const byte *pdata_start = pdata;

        if (size < 1)
            return_error(gs_error_rangecheck);
        enc_u_getw(devc.colors.binary.b_level, pdata);
        size -= pdata - pdata_start;
    }
    if ((flag_bits & dc_ht_binary_has_index) != 0) {
        if (size == 0)
            return_error(gs_error_rangecheck);
        --size;
        devc.colors.binary.b_index = *pdata++;
    }
    if ((flag_bits & dc_ht_binary_has_phase_x) != 0) {
        const byte *pdata_start = pdata;

        if (size < 1)
            return_error(gs_error_rangecheck);
        enc_u_getw(devc.phase.x, pdata);
        devc.phase.x += x0;
        size -= pdata - pdata_start;
    }
    if ((flag_bits & dc_ht_binary_has_phase_y) != 0) {
        const byte *pdata_start = pdata;

        if (size < 1)
            return_error(gs_error_rangecheck);
        enc_u_getw(devc.phase.y, pdata);
        devc.phase.y += y0;
        size -= pdata - pdata_start;
    }

    /* everything looks good */
    *pdevc = devc;
    return pdata - pdata0;
}

/*
 * Get the nonzero components of a binary halftone. This is used to
 * distinguish components that are given zero intensity due to halftoning
 * from those for which the original color intensity was in fact zero.
 *
 * Since this device color type involves only a single halftone component,
 * we can reasonably assume that b_level != 0. Hence, we need to check
 * for components with identical intensities in color[0] and color[1].
 */
int
gx_dc_ht_binary_get_nonzero_comps(
    const gx_device_color * pdevc,
    const gx_device *       dev,
    gx_color_index *        pcomp_bits )
{
    int                     code;
    gx_color_value          cvals_0[GX_DEVICE_COLOR_MAX_COMPONENTS],
                            cvals_1[GX_DEVICE_COLOR_MAX_COMPONENTS];

    if ( (code = dev_proc(dev, decode_color)( (gx_device *)dev,
                                              pdevc->colors.binary.color[0],
                                              cvals_0 )) >= 0 &&
         (code = dev_proc(dev, decode_color)( (gx_device *)dev,
                                              pdevc->colors.binary.color[1],
                                              cvals_1 )) >= 0   ) {
        int     i, ncomps = dev->color_info.num_components;
        int     mask = 0x1, comp_bits = 0;

        for (i = 0; i < ncomps; i++, mask <<= 1) {
            if (cvals_0[i] != 0 || cvals_1[i] != 0)
                comp_bits |= mask;
        }
        *pcomp_bits = comp_bits;
        code = 0;
    }

    return code;
}

/* Initialize the tile cache for a given screen. */
/* Cache as many different levels as will fit. */
void
gx_ht_init_cache(const gs_memory_t *mem, gx_ht_cache * pcache, const gx_ht_order * porder)
{
    uint width = porder->width;
    uint height = porder->height;
    uint size = width * height + 1;
    int width_unit =
    (width <= ht_mask_bits / 2 ? ht_mask_bits / width * width :
     width);
    int height_unit = height;
    uint raster = porder->raster;
    uint tile_bytes = raster * height;
    uint shift = porder->shift;
    int num_cached;
    int i;
    byte *tbits = pcache->bits;

    /* Non-monotonic halftones may have more bits than size. */
    if (porder->num_bits >= size)
        size = porder->num_bits + 1;
    /* Make sure num_cached is within bounds */
    num_cached = pcache->bits_size / tile_bytes;
    if (num_cached > size)
        num_cached = size;
    if (num_cached > pcache->num_tiles)
        num_cached = pcache->num_tiles;
    if (num_cached == size &&
        tile_bytes * num_cached <= pcache->bits_size / 2
        ) {
        /*
         * We can afford to replicate every tile in the cache,
         * which will reduce breakage when tiling.  Since
         * horizontal breakage is more expensive than vertical,
         * and since wide shallow fills are more common than
         * narrow deep fills, we replicate the tile horizontally.
         * We do have to be careful not to replicate the tile
         * to an absurdly large size, however.
         */
        uint rep_raster =
        ((pcache->bits_size / num_cached) / height) &
        ~(align_bitmap_mod - 1);
        uint rep_count = rep_raster * 8 / width;

        /*
         * There's no real value in replicating the tile
         * beyond the point where the byte width of the replicated
         * tile is a multiple of a long.
         */
        if (rep_count > sizeof(ulong) * 8)
            rep_count = sizeof(ulong) * 8;
        width_unit = width * rep_count;
        raster = bitmap_raster(width_unit);
        tile_bytes = raster * height;
    }
    pcache->base_id = gs_next_ids(mem, porder->num_levels + 1);
    pcache->order = *porder;
    /* The transfer function is irrelevant, and might become dangling. */
    pcache->order.transfer = 0;
    pcache->num_cached = num_cached;
    pcache->levels_per_tile = (size + num_cached - 1) / num_cached;
    pcache->tiles_fit = -1;
    memset(tbits, 0, pcache->bits_size);
    for (i = 0; i < num_cached; i++, tbits += tile_bytes) {
        register gx_ht_tile *bt = &pcache->ht_tiles[i];

        bt->level = 0;
        bt->index = i;
        bt->tiles.data = tbits;
        bt->tiles.raster = raster;
        bt->tiles.size.x = width_unit;
        bt->tiles.size.y = height_unit;
        bt->tiles.rep_width = width;
        bt->tiles.rep_height = height;
        bt->tiles.shift = bt->tiles.rep_shift = shift;
        bt->tiles.num_planes = 1;
    }
    pcache->render_ht = gx_render_ht_default;
}

/*
 * Compute and save the rendering of a given gray level
 * with the current halftone.  The cache holds multiple tiles,
 * where each tile covers a range of possible levels.
 * We adjust the tile whose range includes the desired level incrementally;
 * this saves a lot of time for the average image, where gray levels
 * don't change abruptly.  Note that the "level" is the number of bits,
 * not the index in the levels vector.
 */
static int
render_ht(gx_ht_tile * pbt, int level /* [1..num_bits-1] */ ,
          const gx_ht_order * porder, gx_bitmap_id new_id)
{
    byte *data = pbt->tiles.data;
    int code;

    if_debug7('H', "[H]Halftone cache slot "PRI_INTPTR": old=%d, new=%d, w=%d(%d), h=%d(%d):\n",
              (intptr_t)data, pbt->level, level,
              pbt->tiles.size.x, porder->width,
              pbt->tiles.size.y, porder->num_bits / porder->width);
#ifdef DEBUG
    if (level < 0 || level > porder->num_bits) {
        lprintf3("Error in render_ht: level=%d, old level=%d, num_bits=%d\n",
                 level, pbt->level, porder->num_bits);
        return_error(gs_error_Fatal);
    }
#endif
    code = porder->procs->render(pbt, level, porder);
    if (code < 0)
        return code;
    pbt->level = level;
    pbt->tiles.id = new_id;
    pbt->tiles.num_planes = 1;
    /*
     * Check whether we want to replicate the tile in the cache.
     * Since we only do this when all the renderings will fit
     * in the cache, we only do it once per level, and it doesn't
     * have to be very efficient.
     */
        /****** TEST IS WRONG if width > rep_width but tile.raster ==
         ****** order raster.
         ******/
    if (pbt->tiles.raster > porder->raster)
        bits_replicate_horizontally(data, pbt->tiles.rep_width,
                                    pbt->tiles.rep_height, porder->raster,
                                    pbt->tiles.size.x, pbt->tiles.raster);
    if (pbt->tiles.size.y > pbt->tiles.rep_height &&
        pbt->tiles.shift == 0
        )
        bits_replicate_vertically(data, pbt->tiles.rep_height,
                                  pbt->tiles.raster, pbt->tiles.size.y);
#ifdef DEBUG
    if (gs_debug_c('H')) {
        const byte *p = pbt->tiles.data;
        int wb = pbt->tiles.raster;
        const byte *ptr = p + wb * pbt->tiles.size.y;

        while (p < ptr) {
            dmprintf8(porder->data_memory, " %d%d%d%d%d%d%d%d",
                      *p >> 7, (*p >> 6) & 1, (*p >> 5) & 1,
                      (*p >> 4) & 1, (*p >> 3) & 1, (*p >> 2) & 1,
                      (*p >> 1) & 1, *p & 1);
            if ((++p - data) % wb == 0)
                dmputc(porder->data_memory, '\n');
        }
    }
#endif
    return 0;
}
