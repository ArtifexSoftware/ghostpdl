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


/* Halftone and bitmap writing for command lists */
#include "memory_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gdevprn.h"            /* for BLS_force_memory */
#include "gxcldev.h"
#include "gxfmap.h"
#include "gxpcolor.h"		/* for gx_device_is_pattern_clist */

/*
 * Define when, if ever, to write character bitmaps in all bands.
 * Set this to:
 *      0 to always write in all bands;
 *      N to write in all bands when the character has been seen in N+1
 *         bands on a page;
 *      max_ushort to never write in all bands.
 */
#define CHAR_ALL_BANDS_COUNT max_ushort

/* ------ Writing ------ */

/*
 * Determine the (possibly unpadded) width in bytes for writing a bitmap,
 * per the algorithm in gxcldev.h.  If compression_mask has any of the
 * cmd_mask_compress_any bits set, we assume the bitmap will be compressed.
 * Return the total size of the bitmap.
 */
uint
clist_bitmap_bytes(uint width_bits, uint height, int compression_mask,
                   uint * width_bytes, uint * raster)
{
    uint full_raster = *raster = bitmap_raster(width_bits);
    uint short_raster = (width_bits + 7) >> 3;
    uint width_bytes_last;

    if (compression_mask & cmd_mask_compress_any)
        *width_bytes = width_bytes_last = full_raster;
    else if (short_raster <= cmd_max_short_width_bytes ||
             height <= 1 ||
             (compression_mask & decompress_spread) != 0
        )
        *width_bytes = width_bytes_last = short_raster;
    else
        *width_bytes = full_raster, width_bytes_last = short_raster;
    return
        (height == 0 ? 0 : *width_bytes * (height - 1) + width_bytes_last);
}

/*
 * Compress a bitmap, skipping extra padding bytes at the end of each row if
 * necessary.  We require height >= 1, raster >= bitmap_raster(width_bits).
 */
static int
go_process(stream_state * st, stream_cursor_read *pr, stream_cursor_write *pw, bool end)
{
    int status = (*st->templat->process) (st, pr, pw, end);
    if (status)
        return status;
    /* We don't attempt to handle compressors that */
    /* require >1 input byte to make progress. */
    if (pr->ptr != pr->limit)
        return -1;
    return 0;
}
static byte zeros[1<<align_bitmap_mod] = { 0, };
static int
cmd_compress_bitmap(stream_state * st, const byte * data, uint width_bits,
                    uint raster, uint height, stream_cursor_write * pw)
{
    uint width_bytes = bitmap_raster(width_bits);
    int status = 0;
    stream_cursor_read r;
    stream_cursor_read r2;
    uint whole_bytes = width_bits>>3;
    uint mask = (0xff00>>(width_bits & 7)) & 0xff;
    uint padding = width_bytes - ((width_bits+7)>>3);

    if (raster == whole_bytes) {
        stream_cursor_read_init(&r, data, raster * (size_t)height);
        status = (*st->templat->process) (st, &r, pw, true);
    } else {			/* Compress row-by-row. */
        uint y;

        stream_cursor_read_init(&r, data, whole_bytes);

        for (y = height-1; (r.limit = r.ptr + whole_bytes), y > 0; y--) {
            status = go_process(st, &r, pw, false);
            if (status)
                break;
            if (mask) {
                byte b = r.ptr[1] & mask;

                stream_cursor_read_init(&r2, &b, 1);
                status = go_process(st, &r2, pw, false);
                if (status)
                    break;
            }
            if (padding) {
                stream_cursor_read_init(&r2, zeros, padding);
                status = go_process(st, &r2, pw, false);
                if (status)
                    break;
            }
            r.ptr += (int)(raster - whole_bytes);
        }
        if (status == 0) {
            status = go_process(st, &r, pw, padding == 0 && mask == 0);
            if (status == 0 && mask) {
                byte b = r.ptr[1] & mask;

                stream_cursor_read_init(&r2, &b, 1);
                status = go_process(st, &r2, pw, padding == 0);
            }
            if (status == 0 && padding) {
                stream_cursor_read_init(&r2, zeros, padding);
                status = go_process(st, &r2, pw, true);
            }
        }
    }
    if (st->templat->release)
        (*st->templat->release) (st);
    return status;
}

/*
 * Put a bitmap in the buffer, compressing if appropriate.
 * pcls == 0 means put the bitmap in all bands.
 * Return <0 if error, otherwise the compression method.
 * A return value of gs_error_limitcheck means that the bitmap was too big
 * to fit in the command reading buffer.
 * This won't happen if the compression_mask has allow_large_bitmap set.
 * Note that this leaves room for the command and initial arguments,
 * but doesn't fill them in.
 */
int
cmd_put_bits(gx_device_clist_writer * cldev, gx_clist_state * pcls,
  const byte * data, uint width_bits, uint height, uint raster, int op_size,
             int compression_mask, byte ** pdp, uint * psize)
{
    uint short_raster, full_raster;
    uint short_size = clist_bitmap_bytes(width_bits, height,
                          compression_mask & ~cmd_mask_compress_any,
                          &short_raster, &full_raster);
    uint uncompressed_raster;
    uint uncompressed_size = clist_bitmap_bytes(width_bits, height, compression_mask,
                       &uncompressed_raster, &full_raster);
    uint max_size = (compression_mask & allow_large_bitmap) ? 0x7fffffff :
                        data_bits_size - op_size;
    gs_memory_t *mem = cldev->memory;
    byte *dp;
    int compress = 0;
    int code;

    /*
     * See if compressing the bits is possible and worthwhile.
     * Currently we can't compress if the compressed data won't fit in
     * the command reading buffer, or if the decompressed data won't fit
     * in the buffer and decompress_elsewhere isn't set.
     */
    if (short_size >= 50 &&
        (compression_mask & ((1<<cmd_compress_rle) | (1<<cmd_compress_cfe))) != 0 &&
        (uncompressed_size <= max_size ||
         (compression_mask & decompress_elsewhere) != 0)
        ) {
        union ss_ {
            stream_state ss;
            stream_CFE_state cf;
            stream_RLE_state rl;
        } sstate;
        int try_size = op_size + min(uncompressed_size, max_size);

        *psize = try_size;
        code = (pcls != 0 ?
                set_cmd_put_op(&dp, cldev, pcls, 0, try_size) :
                set_cmd_put_all_op(&dp, cldev, 0, try_size));
        if (code < 0)
            return code;
        cmd_uncount_op(0, try_size);
        /*
         * Note that we currently keep all the padding if we are
         * compressing.  This is ridiculous, but it's too hard to
         * change right now.
         */
        if (compression_mask & (1 << cmd_compress_cfe)) {
            /* Try CCITTFax compression. */
            clist_cfe_init(&sstate.cf,
                           uncompressed_raster << 3 /*width_bits*/,
                           mem);
            compress = cmd_compress_cfe;
        } else if (compression_mask & (1 << cmd_compress_rle)) {
            /* Try RLE compression. */
            clist_rle_init(&sstate.rl);
            compress = cmd_compress_rle;
        }
        if (compress) {
            byte *wbase = dp + (op_size - 1);
            stream_cursor_write w;

            /*
             * We can give up on compressing if we generate too much
             * output to fit in the command reading buffer, or too
             * much to make compression worthwhile.
             */
            uint wmax = min(uncompressed_size, max_size);
            int status;

            w.ptr = wbase;
            w.limit = w.ptr + min(wmax, short_size >> 1);
            status = cmd_compress_bitmap((stream_state *) & sstate, data,
                                  width_bits, /* was uncompressed_raster << 3, but this overruns. */
                                         raster, height, &w);
            if (status == 0) {	/* Use compressed representation. */
                uint wcount = w.ptr - wbase;

                cmd_shorten_list_op(cldev,
                             (pcls ? &pcls->list : cldev->band_range_list),
                                    try_size - (op_size + wcount));
                *psize = op_size + wcount;
                goto out;
            }
        }
        if (uncompressed_size > max_size) {
            /* Shorten to zero, erasing the operation altogether */
            if_debug1m('L', cldev->memory,
                       "[L]Uncompressed bits %u too large for buffer\n",
                       uncompressed_size);
            cmd_shorten_list_op(cldev,
                             (pcls ? &pcls->list : cldev->band_range_list),
                                try_size);
            return_error(gs_error_limitcheck);
        }
        if (uncompressed_size != short_size) {
            if_debug2m('L',cldev->memory,"[L]Shortening bits from %u to %u\n",
                       try_size, op_size + short_size);
            cmd_shorten_list_op(cldev,
                             (pcls ? &pcls->list : cldev->band_range_list),
                                try_size - (op_size + short_size));
            *psize = op_size + short_size;
        }
        compress = 0;
    } else if (uncompressed_size > max_size)
        return_error(gs_error_limitcheck);
    else {
        *psize = op_size + short_size;
        code = (pcls != 0 ?
                set_cmd_put_op(&dp, cldev, pcls, 0, *psize) :
                set_cmd_put_all_op(&dp, cldev, 0, *psize));
        if (code < 0)
            return code;
        cmd_uncount_op(0, *psize);
    }
    if ((compression_mask & (1 << cmd_compress_const)) &&
        (code = bytes_rectangle_is_const(data, raster, uncompressed_raster << 3, height)) >= 0) {
        cmd_shorten_list_op(cldev,
                            (pcls ? &pcls->list : cldev->band_range_list),
                            *psize - (op_size + 1));
        *psize = op_size + 1;
        dp[op_size] = code;
        compress = cmd_compress_const;
    } else {
        uint copy_bytes = (width_bits + 7) >> 3;
        bytes_copy_rectangle_zero_padding_last_short(
                             dp + op_size, short_raster, data, raster,
                             copy_bytes, height);
    }
out:
    *pdp = dp;
    return compress;
}

/* Add a command to set the tile size and depth. */
static uint
cmd_size_tile_params(const gx_strip_bitmap * tile, bool for_pattern)
{
    return 2 + (for_pattern ? cmd_size_w(tile->id) : 0) +
        cmd_size_w(tile->rep_width) + cmd_size_w(tile->rep_height) +
        (tile->rep_width == tile->size.x ? 0 :
         cmd_size_w(tile->size.x / tile->rep_width)) +
        (tile->rep_height == tile->size.y ? 0 :
         cmd_size_w(tile->size.y / tile->rep_height)) +
        (tile->rep_shift == 0 ? 0 : cmd_size_w(tile->rep_shift)) +
        (tile->num_planes == 1 ? 0 : 1);
}
static void
cmd_store_tile_params(byte * dp, const gx_strip_bitmap * tile, int depth,
                      uint csize, bool for_pattern, const gs_memory_t *mem)
{
    byte *p = dp + 2;
    byte bd = cmd_depth_to_code(depth);

    *dp = cmd_count_op(cmd_opv_set_tile_size, csize, mem);
    if (for_pattern)
        p = cmd_put_w(tile->id, p);
    p = cmd_put_w(tile->rep_width, p);
    p = cmd_put_w(tile->rep_height, p);
    if (tile->rep_width != tile->size.x) {
        p = cmd_put_w(tile->size.x / tile->rep_width, p);
        bd |= 0x20;
    }
    if (tile->rep_height != tile->size.y) {
        p = cmd_put_w(tile->size.y / tile->rep_height, p);
        bd |= 0x40;
    }
    if (tile->rep_shift != 0) {
        p = cmd_put_w(tile->rep_shift, p);
        bd |= 0x80;
    }
    if (tile->num_planes != 1) {
        *p++ = (byte)tile->num_planes;
        bd |= 0x10;
    }
    dp[1] = bd;
}

/* Add a command to set the tile index. */
/* This is a relatively high-frequency operation, so we declare it `inline'. */
static inline int
cmd_put_tile_index(gx_device_clist_writer *cldev, gx_clist_state *pcls,
                   uint indx)
{
    int idelta = indx - pcls->tile_index + 8;
    byte *dp;
    int code;

    if (!(idelta & ~15)) {
        code = set_cmd_put_op(&dp, cldev, pcls,
                              cmd_op_delta_tile_index + idelta, 1);
        if (code < 0)
            return code;
    } else {
        code = set_cmd_put_op(&dp, cldev, pcls,
                              cmd_op_set_tile_index + (indx >> 8), 2);
        if (code < 0)
            return code;
        dp[1] = indx & 0xff;
    }
    if_debug2m('L', cldev->memory, "[L]writing index=%u, offset=%lu\n",
               indx, cldev->tile_table[indx].offset);
    return 0;
}

/* If necessary, write out data for a single color map. */
int
cmd_put_color_map(gx_device_clist_writer * cldev, cmd_map_index map_index,
        int comp_num, const gx_transfer_map * map, gs_id * pid)
{
    byte *dp;
    int code;

    if (map == 0) {
        if (pid && *pid == gs_no_id)
            return 0;	/* no need to write */
        code = set_cmd_put_all_op(&dp, cldev, cmd_opv_set_misc, 3);
        if (code < 0)
            return code;
        dp[1] = cmd_set_misc_map + (cmd_map_none << 4) + map_index;
        dp[2] = comp_num;
        if (pid)
            *pid = gs_no_id;
    } else {
        if (pid && map->id == *pid)
            return 0;	/* no need to write */
        if (map->proc == gs_identity_transfer) {
            code = set_cmd_put_all_op(&dp, cldev, cmd_opv_set_misc, 3);
            if (code < 0)
                return code;
            dp[1] = cmd_set_misc_map + (cmd_map_identity << 4) + map_index;
            dp[2] = comp_num;
        } else {
            code = set_cmd_put_all_op(&dp, cldev, cmd_opv_set_misc,
                                      3 + sizeof(map->values));
            if (code < 0)
                return code;
            dp[1] = cmd_set_misc_map + (cmd_map_other << 4) + map_index;
            dp[2] = comp_num;
            memcpy(dp + 3, map->values, sizeof(map->values));
        }
        if (pid)
            *pid = map->id;
    }
    return 0;
}

/* ------ Tile cache management ------ */

/* We want consecutive ids to map to consecutive hash slots if possible, */
/* so we can use a delta representation when setting the index. */
/* NB that we cannot emit 'delta' style tile indices if VM error recovery */
/* is in effect, since reader & writer's tile indices may get out of phase */
/* as a consequence of error recovery occurring. */
#define tile_id_hash(id) (id)
#define tile_hash_next(index) ((index) + 413)	/* arbitrary large odd # */
typedef struct tile_loc_s {
    uint index;
    tile_slot *tile;
} tile_loc;

/* Look up a tile or character in the cache.  If found, set the index and */
/* pointer; if not, set the index to the insertion point. */
static bool
clist_find_bits(gx_device_clist_writer * cldev, gx_bitmap_id id, tile_loc * ploc)
{
    uint index = tile_id_hash(id);
    const tile_hash *table = cldev->tile_table;
    uint mask = cldev->tile_hash_mask;
    ulong offset;

    for (; (offset = table[index &= mask].offset) != 0;
         index = tile_hash_next(index)
        ) {
        tile_slot *tile = (tile_slot *) (cldev->data + offset);

        if (tile->id == id) {
            ploc->index = index;
            ploc->tile = tile;
            return true;
        }
    }
    ploc->index = index;
    return false;
}

/* Delete a tile from the cache. */
static void
clist_delete_tile(gx_device_clist_writer * cldev, tile_slot * slot)
{
    tile_hash *table = cldev->tile_table;
    uint mask = cldev->tile_hash_mask;
    uint index = slot->index;
    ulong offset;

    if_debug2m('L', cldev->memory, "[L]deleting index=%u, offset=%lu\n",
               index, (ulong) ((byte *) slot - cldev->data));
    gx_bits_cache_free(&cldev->bits, (gx_cached_bits_head *) slot,
                       cldev->cache_chunk);
    table[index].offset = 0;
    /* Delete the entry from the hash table. */
    /* We'd like to move up any later entries, so that we don't need */
    /* a deleted mark, but it's too difficult to note this in the */
    /* band list, so instead, we just delete any entries that */
    /* would need to be moved. */
    while ((offset = table[index = tile_hash_next(index) & mask].offset) != 0) {
        tile_slot *tile = (tile_slot *) (cldev->data + offset);
        tile_loc loc;

        if (!clist_find_bits(cldev, tile->id, &loc)) {	/* We didn't find it, so it should be moved into a slot */
            /* that we just vacated; instead, delete it. */
            if_debug2m('L', cldev->memory,
                       "[L]move-deleting index=%u, offset=%lu\n",
                       index, offset);
            gx_bits_cache_free(&cldev->bits,
                             (gx_cached_bits_head *) (cldev->data + offset),
                               cldev->cache_chunk);
            table[index].offset = 0;
        }
    }
}

/* Add a tile to the cache. */
/* tile->raster holds the raster for the replicated tile; */
/* we pass the raster of the actual data separately. */
static int
clist_add_tile(gx_device_clist_writer * cldev, const gx_strip_bitmap * tiles,
               uint sraster, int depth)
{
    uint raster = tiles->raster;
    uint size_bytes = raster * tiles->size.y * tiles->num_planes;
    uint tsize =
    sizeof(tile_slot) + cldev->tile_band_mask_size + size_bytes;
    tile_slot *slot;

    if (cldev->bits.csize == cldev->tile_max_count) {	/* Don't let the hash table get too full: delete an entry. */
        /* Since gx_bits_cache_alloc returns an entry to delete when */
        /* it fails, just force it to fail. */
        gx_bits_cache_alloc(&cldev->bits, (ulong) cldev->cache_chunk->size,
                            (gx_cached_bits_head **)&slot);
        if (slot == NULL) {	/* Wrap around and retry. */
            cldev->bits.cnext = 0;
            gx_bits_cache_alloc(&cldev->bits, (ulong) cldev->cache_chunk->size,
                                (gx_cached_bits_head **)&slot);
#ifdef DEBUG
            if (slot == NULL) {
                lprintf("No entry to delete!\n");
                return_error(gs_error_Fatal);
            }
#endif
        }
        clist_delete_tile(cldev, slot);
    }
    /* Allocate the space for the new entry, deleting entries as needed. */
    while (gx_bits_cache_alloc(&cldev->bits, (ulong) tsize, (gx_cached_bits_head **)&slot) < 0) {
        if (slot == NULL) {	/* Wrap around. */
            if (cldev->bits.cnext == 0) {	/* Too big to fit.  We should probably detect this */
                /* sooner, since if we get here, we've cleared the */
                /* cache. */
                return_error(gs_error_limitcheck);
            }
            cldev->bits.cnext = 0;
        } else
            clist_delete_tile(cldev, slot);
    }
    /* Fill in the entry. */
    slot->head.depth = depth;
    slot->raster = raster;
    slot->width = tiles->rep_width;
    slot->height = tiles->rep_height;
    slot->shift = slot->rep_shift = tiles->rep_shift;
    slot->x_reps = slot->y_reps = 1;
    slot->id = tiles->id;
    slot->num_planes = (byte)tiles->num_planes;
    if (slot->num_planes != 1)
        depth /= slot->num_planes;
    memset(ts_mask(slot), 0, cldev->tile_band_mask_size);
    bytes_copy_rectangle_zero_padding(ts_bits(cldev, slot), raster,
                                      tiles->data, sraster,
                                      (tiles->rep_width * depth + 7) >> 3,
                                      tiles->rep_height * slot->num_planes);
    /* Make the hash table entry. */
    {
        tile_loc loc;

#ifdef DEBUG
        if (clist_find_bits(cldev, tiles->id, &loc))
            lprintf1("clist_find_bits(0x%lx) should have failed!\n",
                     (ulong) tiles->id);
#else
        clist_find_bits(cldev, tiles->id, &loc);	/* always fails */
#endif
        slot->index = loc.index;
        cldev->tile_table[loc.index].offset =
            (byte *) slot - cldev->data;
        if_debug2m('L', cldev->memory, "[L]adding index=%u, offset=%lu\n",
                   loc.index, cldev->tile_table[loc.index].offset);
    }
    slot->num_bands = 0;
    return 0;
}

/* ------ Driver procedure support ------ */

/* Change the tile parameters (size and depth). */
/* Currently we do this for all bands at once. */
static void
clist_new_tile_params(gx_strip_bitmap * new_tile, const gx_strip_bitmap * tiles,
                      int depth, const gx_device_clist_writer * cldev)
{				/*
                                 * Adjust the replication factors.  If we can, we replicate
                                 * the tile in X up to 32 bytes, and then in Y up to 4 copies,
                                 * as long as we don't exceed a total tile size of 256 bytes,
                                 * or more than 255 repetitions in X or Y, or make the tile so
                                 * large that not all possible tiles will fit in the cache.
                                 * Also, don't attempt Y replication if shifting is required,
                                 * or if num_planes != 1.
                                 */
#define max_tile_reps_x 255
#define max_tile_bytes_x 32
#define max_tile_reps_y 4
#define max_tile_bytes 256
    uint rep_width = tiles->rep_width;
    uint rep_height = tiles->rep_height;
    uint rep_width_bits;
    uint tile_overhead =
    sizeof(tile_slot) + cldev->tile_band_mask_size;
    uint max_bytes;

    if (tiles->num_planes != 1)
        depth /= tiles->num_planes;
    rep_width_bits = rep_width * depth;
    max_bytes = cldev->cache_chunk->size / (rep_width_bits * rep_height);

    max_bytes -= min(max_bytes, tile_overhead);
    if (max_bytes > max_tile_bytes)
        max_bytes = max_tile_bytes;
    *new_tile = *tiles;
    {
        uint max_bits_x = max_bytes * 8 / rep_height;
        uint reps_x =
        min(max_bits_x, max_tile_bytes_x * 8) / rep_width_bits;
        uint reps_y;

        while (reps_x > max_tile_reps_x)
            reps_x >>= 1;
        new_tile->size.x = max(reps_x, 1) * rep_width;
        new_tile->raster = bitmap_raster(new_tile->size.x * depth);
        if (tiles->shift != 0 || tiles->num_planes != 1)
            reps_y = 1;
        else {
            reps_y = max_bytes / (new_tile->raster * rep_height);
            if (reps_y > max_tile_reps_y)
                reps_y = max_tile_reps_y;
            else if (reps_y < 1)
                reps_y = 1;
        }
        new_tile->size.y = reps_y * rep_height;
    }
#undef max_tile_reps_x
#undef max_tile_bytes_x
#undef max_tile_reps_y
#undef max_tile_bytes
}

extern dev_proc_open_device(pattern_clist_open_device);

/* Change tile for clist_tile_rectangle. */
int
clist_change_tile(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                  const gx_strip_bitmap * tiles, int depth)
{
    tile_loc loc;
    int code;

#define tile_params_differ(cldev, tiles, depth)\
  ((tiles)->rep_width != (cldev)->tile_params.rep_width ||\
   (tiles)->rep_height != (cldev)->tile_params.rep_height ||\
   (tiles)->rep_shift != (cldev)->tile_params.rep_shift ||\
   (depth) != (cldev)->tile_depth)

  top:if (clist_find_bits(cldev, tiles->id, &loc)) {	/* The bitmap is in the cache.  Check whether this band */
        /* knows about it. */
        int band_index = pcls - cldev->states;
        byte *bptr = ts_mask(loc.tile) + (band_index >> 3);
        byte bmask = 1 << (band_index & 7);
        bool for_pattern = gx_device_is_pattern_clist((gx_device *)cldev);

        if (*bptr & bmask) {	/* Already known.  Just set the index. */
            if (pcls->tile_index == loc.index)
                return 0;
            if ((code = cmd_put_tile_index(cldev, pcls, loc.index)) < 0)
                return code;
        } else {
            uint extra = 0;

            if (tile_params_differ(cldev, tiles, depth) ||
                for_pattern) {			/*
                                                 * We have a cached tile whose parameters differ from
                                                 * the current ones.  Because of the way tile IDs are
                                                 * managed, this is currently only possible when mixing
                                                 * Patterns and halftones, but if we didn't generate new
                                                 * IDs each time the main halftone cache needed to be
                                                 * refreshed, this could also happen simply from
                                                 * switching screens.
                                                 */
                int band;

                clist_new_tile_params(&cldev->tile_params, tiles, depth,
                                      cldev);
                cldev->tile_depth = depth;
                /* No band knows about the new parameters. */
                for (band = cldev->tile_known_min;
                     band <= cldev->tile_known_max;
                     ++band
                    )
                    cldev->states[band].known &= ~tile_params_known;
                cldev->tile_known_min = cldev->nbands;
                cldev->tile_known_max = -1;
                }
            if (!(pcls->known & tile_params_known)) {	/* We're going to have to write the tile parameters. */
                extra = cmd_size_tile_params(&cldev->tile_params, for_pattern);
            } {			/*
                                 * This band doesn't know this tile yet, so output the
                                 * bits.  Note that the offset we write is the one used by
                                 * the reading phase, not the writing phase.  Note also
                                 * that the size of the cached and written tile may differ
                                 * from that of the client's tile.  Finally, note that
                                 * this tile's size parameters are guaranteed to be
                                 * compatible with those stored in the device
                                 * (cldev->tile_params).
                                 */
                ulong offset = (byte *) loc.tile - cldev->cache_chunk->data;
                uint rsize =
                    extra + 1 + cmd_size_w(loc.index) + cmd_size_w(offset);
                byte *dp;
                uint csize;
                int code;
                int pdepth = depth;
                if (tiles->num_planes != 1)
                    pdepth /= tiles->num_planes;

                /* put the bits, but don't restrict to a single buffer */
                code = cmd_put_bits(cldev, pcls, ts_bits(cldev, loc.tile),
                                    tiles->rep_width * pdepth,
                                    tiles->rep_height * tiles->num_planes,
                                    loc.tile->raster, rsize,
                                    allow_large_bitmap |
                                        (cldev->tile_params.size.x > tiles->rep_width ?
                                             decompress_elsewhere | decompress_spread :
                                             decompress_elsewhere),
                                    &dp, &csize);

                if (code < 0)
                    return code;
                if (extra) {	/* Write the tile parameters before writing the bits. */
                    if_debug1m('L', cldev->memory,
                               "[L] fake end_run: really set_tile_size[%d]\n", extra);
                    cmd_store_tile_params(dp, &cldev->tile_params, depth,
                                          extra, for_pattern, cldev->memory);
                    dp += extra;
                    /* This band now knows the parameters. */
                    pcls->known |= tile_params_known;
                    if (band_index < cldev->tile_known_min)
                        cldev->tile_known_min = band_index;
                    if (band_index > cldev->tile_known_max)
                        cldev->tile_known_max = band_index;
                }
                if_debug1m('L', cldev->memory,
                           "[L] fake end_run: really set_tile_bits[%d]\n", csize-extra);
                *dp = cmd_count_op(cmd_opv_set_tile_bits, csize - extra, cldev->memory);
                dp++;
                dp = cmd_put_w(loc.index, dp);
                cmd_put_w(offset, dp);
                *bptr |= bmask;
                loc.tile->num_bands++;
            }
        }
        pcls->tile_index = loc.index;
        pcls->tile_id = loc.tile->id;
        return 0;
    }
    /* The tile is not in the cache, add it. */
    {
        gx_strip_bitmap new_tile;
        gx_strip_bitmap *ptile;

        /* Ensure that the tile size is compatible. */
        if (tile_params_differ(cldev, tiles, depth)) {	/* We'll reset cldev->tile_params when we write the bits. */
            clist_new_tile_params(&new_tile, tiles, depth, cldev);
            ptile = &new_tile;
        } else {
            cldev->tile_params.id = tiles->id;
            cldev->tile_params.data = tiles->data;
            ptile = &cldev->tile_params;
        }
        code = clist_add_tile(cldev, ptile, tiles->raster, depth);
        if (code < 0)
            return code;
    }
    goto top;
#undef tile_params_differ
}

/* Change "tile" for clist_copy_*.  tiles->[rep_]shift must be zero. */
int
clist_change_bits(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                  const gx_strip_bitmap * tiles, int depth)
{
    tile_loc loc;
    int code;
    uint band_index = pcls - cldev->states;
    byte bmask = 1 << (band_index & 7);
    byte *bptr;

    while (!clist_find_bits(cldev, tiles->id, &loc)) {
        /* The tile is not in the cache. */
        code = clist_add_tile(cldev, tiles, tiles->raster, depth);
        if (code < 0)
            return code;
    }

    /* The bitmap is in the cache.  Check whether this band */
    /* knows about it. */
    bptr = ts_mask(loc.tile) + (band_index >> 3);

    if (*bptr & bmask) {	/* Already known.  Just set the index. */
        if (pcls->tile_index == loc.index)
            return 0;
        cmd_put_tile_index(cldev, pcls, loc.index);
    } else {		/* Not known yet.  Output the bits. */
        /* Note that the offset we write is the one used by */
        /* the reading phase, not the writing phase. */
        ulong offset = (byte *) loc.tile - cldev->cache_chunk->data;
        uint rsize = 2 + cmd_size_w(loc.tile->width) +
                     cmd_size_w(loc.tile->height) +
                     (loc.tile->num_planes > 1 ? 1 : 0) +
                     cmd_size_w(loc.index) +
                     cmd_size_w(offset);
        byte *dp;
        uint csize;
        gx_clist_state *bit_pcls = pcls;
        int pdepth = depth;

        if (tiles->num_planes != 1)
            pdepth /= loc.tile->num_planes;
        if (loc.tile->num_bands == CHAR_ALL_BANDS_COUNT)
            bit_pcls = NULL;
        /* put the bits, but don't restrict to a single buffer */
        code = cmd_put_bits(cldev, bit_pcls, ts_bits(cldev, loc.tile),
                            loc.tile->width * pdepth,
                            loc.tile->height * loc.tile->num_planes, loc.tile->raster,
                            rsize,
                            decompress_elsewhere |
                                (cldev->target->BLS_force_memory ? (1 << cmd_compress_cfe) : 0),
                            &dp, &csize);

        if (code < 0)
            return code;
        if_debug1m('L', cldev->memory,
                   "[L] fake end_run: really set_bits[%d]\n", csize);
        *dp = cmd_count_op(loc.tile->num_planes > 1 ? cmd_opv_set_bits_planar : cmd_opv_set_bits,
                           csize, cldev->memory);
        dp[1] = (depth << 2) + code;
        dp += 2;
        dp = cmd_put_w(loc.tile->width, dp);
        dp = cmd_put_w(loc.tile->height, dp);
        if (loc.tile->num_planes > 1)
            *dp++ = loc.tile->num_planes;
        dp = cmd_put_w(loc.index, dp);
        cmd_put_w(offset, dp);
        if_debug7m('L', cldev->memory, " compress=%d depth=%d size=(%d,%d) planes=%d index=%d offset=%ld\n",
                   code, depth, loc.tile->width, loc.tile->height, loc.tile->num_planes, loc.index, offset);
        if (bit_pcls == NULL) {
            memset(ts_mask(loc.tile), 0xff,
                   cldev->tile_band_mask_size);
            loc.tile->num_bands = cldev->nbands;
        } else {
            *bptr |= bmask;
            loc.tile->num_bands++;
        }
    }
    pcls->tile_index = loc.index;
    pcls->tile_id = loc.tile->id;
    return 0;
}
