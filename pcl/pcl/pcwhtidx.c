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


/* pcwhtidx.c - code to find index of white in a palette */

#include "pcstate.h"
#include "pcpalet.h"
#include "pcindxed.h"
#include "pcwhtidx.h"

/*
 * Find the first white point in a palette. If none are found, the palette
 * size is returned.
 */
static int
find_first_white(const byte * ptbl, int num_entries)
{
    int i;

    for (i = 0; i < 3 * num_entries; i += 3) {
        if ((ptbl[i] == 255) && (ptbl[i + 1] == 255) && (ptbl[i + 2] == 255))
            break;
    }
    return i / 3;
}

/*
 * Create a palette index re-map array, assuming that the number of bits per
 * pixel divides 8. This allows parallel mapping of multiple pixels.
 *
 * It is also assumed that ptbl points to a table of at least 256 entries.
 */
static void
build_remap_array8(byte * pmap, /* existing (general) re-map array */
                   int num_entries,     /* number of entries in re-map array */
                   int b_per_p  /* bits per pixel */
    )
{
    byte tmp_map[256];
    int i;
    int pix_per_byte = 8 / b_per_p;
    uint mask = (1 << b_per_p) - 1;

    /* create the "parallel" mapping table */
    for (i = 0; i < 256; i++) {
        int j;

        tmp_map[i] = 0;
        for (j = 0; j < pix_per_byte; j++)
            tmp_map[i] |=
                pmap[((i >> (j * b_per_p)) & mask)] << (j * b_per_p);
    }

    /* copy into the orignal table */
    memcpy(pmap, tmp_map, 256);
}

/*
 * Create a palette index re-map array, which maps all non-white entries to
 * themselves and all white entries to the first white entry.
 *
 * If for_pattern is true, this code will always create a re-map array if
 * (1 << b_per_p) > num_entries, in which case pixel index values larger than
 * the palette are possible. A map is necessary in this case so that these
 * "out of range" pixel values are interpreted modulo the palette size. The
 * map is not necessary for the case of rasters, as for these the actual
 * pixel values will never be larger than the palette, even though b_per_p is
 * set to 8 in some cases to facilitate subsequent processing.
 *
 * Returns true if a re-map table formed and remapping is required, false
 * otherwise.
 */
static bool
build_remap_array(const byte * ptbl,
                  int num_entries,
                  byte * pmap,
                  int *pfirst_white, int b_per_p, bool for_pattern)
{
    int first_white;
    int second_white;
    int map_size = (1 << b_per_p);
    bool must_map = (map_size > num_entries);
    int i;

    /* limit consideration to those indices that can be achieved */
    if (num_entries > map_size)
        num_entries = map_size;

    /* if there is no white and we don't absolutely need a map, quit now */
    first_white = find_first_white(ptbl, num_entries);
    *pfirst_white = first_white;
    if ((first_white == num_entries) && !must_map)
        return false;

    /* get the next white; if no other white, quit if possible */
    second_white = find_first_white(ptbl + 3 * (first_white + 1),
                                    num_entries - first_white - 1) +
        first_white + 1;
    if ((second_white == num_entries) && !must_map)
        return false;

    /* build the re-map table */
    for (i = 0; i < second_white; i++)
        pmap[i] = i;

    if (second_white != num_entries)
        pmap[i++] = first_white;

    for (; i < map_size; i++) {
        int j = i & (num_entries - 1);  /* in case map_size > num_entries */

        if ((ptbl[3 * j] == 255) &&
            (ptbl[3 * j + 1] == 255) && (ptbl[3 * j + 2] == 255))
            pmap[i] = first_white;
        else
            pmap[i] = j;
    }
    /* check if a special "parallel map" can be used */
    if ((b_per_p < 8) &&
        ((8 % b_per_p) == 0) && (pcl_cs_indexed_palette_size >= 8))
        build_remap_array8(pmap, num_entries, b_per_p);

    return true;
}

/*
 * Special case for mapping rasters when the number of bits per pixel divides
 * 8 (i.e.: there is an integral number of pixels in a byte). In this case
 * all pixels within a byte can be mapped at once.
 */
static void remap_raster_ary8(const byte * inp, /* array to read from */
                              byte * outp,      /* array to write to; may be same as inp */
                              int npixels,      /* number of pixels in raster */
                              int b_per_p,      /* bits per pixel */
                              const byte * pmap /* re-map table */
    )
{
    int nbytes = (npixels * b_per_p) / 8;

    while (nbytes-- > 0)
        *outp++ = pmap[*inp++];
}

/*
 * Remap one raster array into another, using the re-map table provided. The
 * two arrays may be the same.
 */

static void remap_raster_ary(const byte * inp,  /* array to read from */
                             byte * outp,       /* array to write to; may be same as inp */
                             int npixels,       /* number of pixels in raster */
                             int b_per_p,       /* bits per pixel */
                             const byte * pmap  /* re-map table */
    )
{
    /* check if the the simpler case can be used */
    if (8 % b_per_p == 0) {
        remap_raster_ary8(inp, outp, npixels, b_per_p, pmap);
        return;
    }

    /* should not happen */
    gs_warn("Raster bits per pixel do not divide 8");
}

/*
 * Determine the white entry in the color palette of an indexed color space,
 * and perform any remapping that is required.
 *
 * The must_copy operand indicates if the raster data can be overwritten in
 * place (false) or if a new array must be allocated.
 *
 * Colored patterns are always provided at either 1 or 8-bits per pixel. In
 * the latter case, the palette size may be smaller than the number of bits
 * per pixel. In this case, values should be interpreted modulo the palette
 * size. The graphic library does not support such an interpretation, so
 * remapping is required at this level. The code in build_remap_ary will detect
 * this situation and create a re=-map array even if one is not otherwise
 * required.
 *
 * Returns 0 if successful, < 0 in the event of an error.
 */
int
pcl_cmap_map_raster(const pcl_cs_indexed_t * pindexed,
                    int *pfirst_white,
                    const gs_depth_bitmap * pin_pixinfo,
                    gs_depth_bitmap * pout_pixinfo,
                    bool must_copy, gs_memory_t * pmem)
{
    byte remap[pcl_cs_indexed_palette_size];
    const byte *pin_rast = 0;
    byte *pout_rast = 0;
    int pix_depth = pin_pixinfo->pix_depth;
    bool fast_mode = ((pix_depth < 8) && (8 % pix_depth == 0));
    int npixels = pin_pixinfo->size.x;
    int i;

    /* see if any remapping is necessary */
    *pout_pixinfo = *pin_pixinfo;
    if (!build_remap_array(pindexed->palette.data,
                           pindexed->num_entries,
                           remap, pfirst_white, pix_depth, true))
        return 0;

    /* allocate a new raster if necessary (pack scanlines) */
    if (must_copy) {
        long nbytes = pin_pixinfo->size.x * pin_pixinfo->pix_depth;

        nbytes = ((nbytes + 7) / 8);
        pout_pixinfo->raster = nbytes;
        pout_rast = gs_alloc_bytes(pmem,
                                   nbytes * pin_pixinfo->size.y,
                                   "re-map colored pattern raster");
        if (pout_rast == 0)
            return e_Memory;
        pout_pixinfo->data = pout_rast;

    } else
        pout_rast = (byte *) pin_pixinfo->data;

    /* remap one scanline at a time */
    pin_rast = pin_pixinfo->data;
    for (i = 0; i < pin_pixinfo->size.y; i++) {
        if (fast_mode)
            remap_raster_ary8(pin_rast, pout_rast, npixels, pix_depth, remap);
        else
            remap_raster_ary(pin_rast, pout_rast, npixels, pix_depth, remap);
        pin_rast += pin_pixinfo->raster;
        pout_rast += pout_pixinfo->raster;
    }

    return 0;
}

/*
 * Create a re-map array to be used with raster, if inecessary. For such an
 * array to be necessary, all of the following must hold:
 *
 *      source transparency on
 *      pixel encoding is indexed by plane or indexed by pixel
 *      there is more than one "white" color in the palette
 *
 * If all of these conditions hold, a point to a remapping array is retuned;
 * otherwise a null pointer is returned. The routne pcl_free_remap_ary should
 * be used to free this memory.
 *
 * To simplify processing, data received in the indexed by plane pixel encoding
 * format are consolidated into an indexed-by-pixel form with 8 bits/pixel,
 * irrespective of the specified number of bits per index (so long as the latter
 * number if > 1). This convention can make it appear that (1 << b_per_p) is
 * larger than the number of entries in the palette, though no pixel index
 * values are ever larger than the palette size.
 */
const void *
pcl_cmap_create_remap_ary(pcl_state_t * pcs, int *pfirst_white)
{
    byte tmp_remap[pcl_cs_indexed_palette_size];
    byte *pmap = 0;
    pcl_cs_indexed_t *pindexed = pcs->ppalet->pindexed;
    int b_per_p;

    /* if a re-map array might be required, build it on the stack first */
    *pfirst_white = pindexed->num_entries;

    if ((!pcs->source_transparent && !pcs->pattern_transparent) ||
        (pcl_cs_indexed_get_encoding(pindexed) > pcl_penc_indexed_by_pixel))
        return 0;
    b_per_p = pcl_cs_indexed_get_bits_per_index(pindexed);
    if (!build_remap_array(pindexed->palette.data,
                           pindexed->num_entries,
                           tmp_remap, pfirst_white, b_per_p, false))
        return 0;

    /* a re-mapping array is necessary; copy the temprorary one to the heap */
    pmap = gs_alloc_bytes(pcs->memory,
                          pcl_cs_indexed_palette_size,
                          "create PCL raster remapping array");
    memcpy(pmap, tmp_remap, pcl_cs_indexed_palette_size);

    return (const void *)pmap;
}

/*
 * Apply the remapping array to one raster scanline.
 *
 * This routine is normally accessed via the pcl_cmap_apply_remap_ary macro,
 * which checks for a null remap array. However, and additional check is
 * performed here for safety purposes. Similarly, for rasters it is always
 * the case that 8 % bits_per_pixel == 0, but an explicit check is performed
 * in any case.
 */
void
pcl_cmap_apply_remap_ary(const void *vpmap, /* remap array pointer */
                         byte * prast,      /* array of bytes to be mapped */
                         int b_per_p,       /* bits per pixel */
                         int npixels)
{
    if (8 % b_per_p == 0)
        remap_raster_ary8(prast, prast, npixels, b_per_p,
                          (const byte *)vpmap);
    else
        remap_raster_ary(prast, prast, npixels, b_per_p, (const byte *)vpmap);

}
