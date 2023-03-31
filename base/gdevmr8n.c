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

/* RasterOp implementation for 8N-bit memory devices */
#include "memory_.h"
#include "gx.h"
#include "gsbittab.h"
#include "gserrors.h"
#include "gsropt.h"
#include "gxcindex.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gdevmem.h"
#include "gdevmrop.h"

/* Use the RUN_ROP code. */
#define USE_RUN_ROP
#undef COMPARE_AND_CONTRAST

/*
 * NOTE: The 16- and 32-bit cases aren't implemented: they just fall back to
 * the default implementation.  This is very slow and will be fixed someday.
 */

#define chunk byte

/* Calculate the X offset for a given Y value, */
/* taking shift into account if necessary. */
#define x_offset(px, ty, textures)\
  ((textures)->shift == 0 ? (px) :\
   (px) + (ty) / (textures)->rep_height * (textures)->rep_shift)

/* ---------------- RasterOp with 8-bit gray / 24-bit RGB ---------------- */

int
mem_gray8_rgb24_strip_copy_rop2(gx_device * dev,
             const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
                               const gx_color_index * scolors,
           const gx_strip_bitmap * textures, const gx_color_index * tcolors,
                               int x, int y, int width, int height,
                       int phase_x, int phase_y,
                       gs_logical_operation_t dirty_lop, uint planar_height)
{
    gx_device_memory *mdev = (gx_device_memory *) dev;
    gs_logical_operation_t lop = lop_sanitize(dirty_lop);
    gs_rop3_t rop;
    gx_color_index const_source = gx_no_color_index;
    gx_color_index const_texture = gx_no_color_index;
    uint draster = mdev->raster;
    int line_count;
    byte *drow, *base;
    int depth = dev->color_info.depth;
    int bpp = depth >> 3;       /* bytes per pixel, 1 or 3 */
    gx_color_index all_ones = ((gx_color_index) 1 << depth) - 1;
    gx_color_index local_scolors[2];
    gx_color_index local_tcolors[2];

#ifdef USE_RUN_ROP
    rop_run_op ropper;
#ifdef COMPARE_AND_CONTRAST
    static byte testbuffer[4096];
    static byte *start;
    static int bytelen;
#endif
#endif

    /* assert(planar_height == 0); */

    /* Check for constant source. */
    if (!rop3_uses_S(lop)) {
        const_source = 0;       /* arbitrary */
    } else if (scolors != 0 && scolors[0] == scolors[1]) {
        /* Constant source */
        const_source = scolors[0];
        if (const_source == gx_device_black(dev))
            lop = lop_know_S_0(lop);
        else if (const_source == gx_device_white(dev)) {
            lop = lop_know_S_1(lop);
        }
    }

    /* Check for constant texture. */
    if (!rop3_uses_T(lop))
        const_texture = 0;      /* arbitrary */
    else if (tcolors != 0 && tcolors[0] == tcolors[1]) {
        /* Constant texture */
        const_texture = tcolors[0];
        if (const_texture == gx_device_black(dev))
            lop = lop_know_T_0(lop);
        else if (const_texture == gx_device_white(dev)) {
            lop = lop_know_T_1(lop);
        }
    }

    rop = lop_rop(lop);

    if (bpp == 1 &&
        (gx_device_has_color(dev) ||
         (gx_device_black(dev) != 0 || gx_device_white(dev) != all_ones))
        ) {
        /*
         * This is an 8-bit device but not gray-scale.  Except in a few
         * simple cases, we have to use the slow algorithm that converts
         * values to and from RGB.
         */
        gx_color_index bw_pixel;

        switch (rop) {
        case rop3_0:
            bw_pixel = gx_device_black(dev);
            goto bw;
        case rop3_1:
            bw_pixel = gx_device_white(dev);
bw:         if (bw_pixel == 0x00)
                rop = rop3_0;
            else if (bw_pixel == 0xff)
                rop = rop3_1;
            else
                goto df;
            break;
        case rop3_D:
            break;
        case rop3_S:
            break;
        case rop3_T:
            break;
        default:
df:         return mem_default_strip_copy_rop2(dev,
                                               sdata, sourcex, sraster, id,
                                               scolors, textures, tcolors,
                                               x, y, width, height,
                                               phase_x, phase_y, lop, 0);
        }
        /* Put the updated rop back into the lop */
        lop = rop;
    }

    /* Adjust coordinates to be in bounds. */
    if (const_source == gx_no_color_index) {
        fit_copy(dev, sdata, sourcex, sraster, id,
                 x, y, width, height);
    } else {
        fit_fill(dev, x, y, width, height);
    }

    /* Set up transfer parameters. */
    line_count = height;
    base = scan_line_base(mdev, y);
    drow = base + x * bpp;

    /* Allow for colors being passed in with tags in the top bits.
     * This confuses transparency code.
     */
    {
        gx_color_index color_mask = (bpp == 1 ? 0xff : 0xffffff);
        if (scolors)
        {
            local_scolors[0] = scolors[0] & color_mask;
            local_scolors[1] = scolors[1] & color_mask;
            scolors = local_scolors;
        }
        if (tcolors)
        {
            local_tcolors[0] = tcolors[0] & color_mask;
            local_tcolors[1] = tcolors[1] & color_mask;
            tcolors = local_tcolors;
        }
    }

    /*
     * There are 18 cases depending on whether each of the source and
     * texture is constant, 1-bit, or multi-bit, and on whether the
     * depth is 8 or 24 bits.  We divide first according to constant
     * vs. non-constant, and then according to 1- vs. multi-bit, and
     * finally according to pixel depth.  This minimizes source code,
     * but not necessarily time, since we do some of the divisions
     * within 1 or 2 levels of loop.
     */

#ifdef USE_RUN_ROP
#define dbit(base, i) ((base)[(i) >> 3] & (0x80 >> ((i) & 7)))
/* 8-bit */
#define cbit8(base, i, colors)\
  (dbit(base, i) ? (byte)colors[1] : (byte)colors[0])
#ifdef COMPARE_AND_CONTRAST
#define rop_body_8(s_pixel, t_pixel)\
  *dptr = (*rop_proc_table[rop])(*dptr, s_pixel, t_pixel)
#endif
/* 24-bit */
#define get24(ptr)\
  (((gx_color_index)(ptr)[0] << 16) | ((gx_color_index)(ptr)[1] << 8) | (ptr)[2])
#define put24(ptr, pixel)\
  (ptr)[0] = (byte)((pixel) >> 16),\
  (ptr)[1] = (byte)((uint)(pixel) >> 8),\
  (ptr)[2] = (byte)(pixel)
#define cbit24(base, i, colors)\
  (dbit(base, i) ? colors[1] : colors[0])
#ifdef COMPARE_AND_CONTRAST
#define rop_body_24(s_pixel, t_pixel)\
  { gx_color_index d_pixel = get24(dptr);\
    d_pixel = (*rop_proc_table[rop])(d_pixel, s_pixel, t_pixel);\
    put24(dptr, d_pixel);\
  }
#endif
    if (const_texture != gx_no_color_index) {
/**** Constant texture ****/
        if (const_source != gx_no_color_index) {
/**** Constant source & texture ****/
            rop_set_s_constant(&ropper, const_source);
            rop_set_t_constant(&ropper, const_texture);
            if (rop_get_run_op(&ropper, lop, depth, rop_s_constant | rop_t_constant)) {
                for (; line_count-- > 0; drow += draster) {
#ifdef COMPARE_AND_CONTRAST
                    byte *dptr = drow;
                    int left = width;

                    bytelen = left*bpp; start = dptr;
                    memcpy(testbuffer, dptr, bytelen);
                    rop_run(&ropper, testbuffer, left);

                    if (bpp == 1)
/**** 8-bit destination ****/
                        for (; left > 0; ++dptr, --left) {
                            rop_body_8((byte)const_source, (byte)const_texture);
                        }
                    else
/**** 24-bit destination ****/
                        for (; left > 0; dptr += 3, --left) {
                            rop_body_24(const_source, const_texture);
                        }
                    if (memcmp(testbuffer, start, bytelen) != 0) {
                        emprintf(dev->memory, "Failed!\n");
                    }
#else
                    rop_run(&ropper, drow, width);
#endif
                }
                rop_release_run_op(&ropper);
            }
        } else {
/**** Data source, const texture ****/
            if (scolors) {
                const byte *srow = sdata;

                rop_set_t_constant(&ropper, const_texture);
                rop_set_s_colors(&ropper, scolors);
                if (rop_get_run_op(&ropper, lop, depth, rop_t_constant | rop_s_1bit)) {

                    for (; line_count-- > 0; drow += draster, srow += sraster) {
#ifdef COMPARE_AND_CONTRAST
                        byte *dptr = drow;
                        int left = width;
/**** 1-bit source ****/
                        int sx = sourcex;

                        rop_set_s_bitmap_subbyte(&ropper, srow, sourcex);
                        bytelen = left*bpp; start = dptr;
                        memcpy(testbuffer, dptr, bytelen);
                        rop_run(&ropper, testbuffer, width);
                        if (bpp == 1)
/**** 8-bit destination ****/
                            for (; left > 0; ++dptr, ++sx, --left) {
                                byte s_pixel = cbit8(srow, sx, scolors);

                                rop_body_8(s_pixel, (byte)const_texture);
                            }
                        else
/**** 24-bit destination ****/
                            for (; left > 0; dptr += 3, ++sx, --left) {
                                bits32 s_pixel = cbit24(srow, sx, scolors);

                                rop_body_24(s_pixel, const_texture);
                            }
                        if (memcmp(testbuffer, start, bytelen) != 0) {
                            emprintf(dev->memory, "Failed!\n");
                        }
#else
/**** 1-bit source ****/
/**** 8-bit destination ****/
/**** 24-bit destination ****/
                        rop_set_s_bitmap_subbyte(&ropper, srow, sourcex);
                        rop_run(&ropper, drow, width);
#endif
                    }
                    rop_release_run_op(&ropper);
                }
            } else {
                const byte *srow = sdata;
                rop_set_t_constant(&ropper, const_texture);
                if (rop_get_run_op(&ropper, lop, depth, rop_t_constant)) {
                    for (; line_count-- > 0; drow += draster, srow += sraster) {
#ifdef COMPARE_AND_CONTRAST
                        byte *dptr = drow;
                        int left = width;

                        bytelen = left*bpp; start = dptr;
                        memcpy(testbuffer, dptr, bytelen);

                        rop_set_s_bitmap(&ropper, srow + sourcex * bpp);
                        rop_run(&ropper, testbuffer, left);
/**** 8-bit source & dest ****/
                        if (bpp == 1) {
                            const byte *sptr = srow + sourcex;

                            for (; left > 0; ++dptr, ++sptr, --left) {
                                byte s_pixel = *sptr;

                                rop_body_8(s_pixel, (byte)const_texture);
                            }
                        } else {
/**** 24-bit source & dest ****/
                            const byte *sptr = srow + sourcex * 3;

                            bytelen = left*bpp; start = dptr;
                            memcpy(testbuffer, dptr, bytelen);

                            for (; left > 0; dptr += 3, sptr += 3, --left) {
                                bits32 s_pixel = get24(sptr);

                                rop_body_24(s_pixel, const_texture);
                            }
                        }
                        if (memcmp(testbuffer, start, bytelen) != 0) {
                            emprintf(dev->memory, "Failed!\n");
                        }
#else
/**** 8-bit source & dest ****/
/**** 24-bit source & dest ****/
                        rop_set_s_bitmap(&ropper, srow + sourcex * bpp);
                        rop_run(&ropper, drow, width);
#endif
                    }
                    rop_release_run_op(&ropper);
                }
            }
        }
    } else if (const_source != gx_no_color_index) {
/**** Const source, data texture ****/
        if (tcolors) {
            uint traster = textures->raster;
            int ty = y + phase_y;

            rop_set_s_constant(&ropper, const_source);
            rop_set_t_colors(&ropper, tcolors);
            if (rop_get_run_op(&ropper, lop, depth, rop_s_constant | rop_t_1bit)) {
                for (; line_count-- > 0; drow += draster, ++ty) {   /* Loop over copies of the tile. */
                    int dx = x, w = width, nw;
                    byte *dptr = drow;
                    const byte *trow =
                    textures->data + (ty % textures->size.y) * traster;
                    int xoff = x_offset(phase_x, ty, textures);

                    for (; w > 0; dx += nw, w -= nw) {
                        int tx = (dx + xoff) % textures->rep_width;
                        int left = nw = min(w, textures->size.x - tx);
#ifdef COMPARE_AND_CONTRAST
                        const byte *tptr = trow;
#endif

                        rop_set_t_bitmap_subbyte(&ropper, trow, tx);
#ifdef COMPARE_AND_CONTRAST
                        bytelen = left*bpp; start = dptr;
                        memcpy(testbuffer, dptr, bytelen);
                        rop_run(&ropper, testbuffer, left);
/**** 1-bit texture ****/
                        if (bpp == 1)
/**** 8-bit dest ****/
                            for (; left > 0; ++dptr, ++tx, --left) {
                                byte t_pixel = cbit8(tptr, tx, tcolors);

                                rop_body_8((byte)const_source, t_pixel);
                            }
                        else
/**** 24-bit dest ****/
                            for (; left > 0; dptr += 3, ++tx, --left) {
                                bits32 t_pixel = cbit24(tptr, tx, tcolors);

                                rop_body_24(const_source, t_pixel);
                            }
                        if (memcmp(testbuffer, start, bytelen) != 0) {
                            emprintf(dev->memory, "Failed!\n");
                        }
#else
                        rop_run(&ropper, dptr, left);
                        dptr += left;
#endif
                    }
                }
            }
        } else {
            uint traster = textures->raster;
            int ty = y + phase_y;

            rop_set_s_constant(&ropper, const_source);
            if (rop_get_run_op(&ropper, lop, depth, rop_s_constant)) {

                for (; line_count-- > 0; drow += draster, ++ty) {   /* Loop over copies of the tile. */
                    int dx = x, w = width, nw;
                    byte *dptr = drow;
                    const byte *trow =
                    textures->data + (ty % textures->size.y) * traster;
                    int xoff = x_offset(phase_x, ty, textures);

                    for (; w > 0; dx += nw, w -= nw) {
                        int tx = (dx + xoff) % textures->rep_width;
                        int left = nw = min(w, textures->size.x - tx);
                        const byte *tptr = trow + tx*bpp;
                        rop_set_t_bitmap(&ropper, tptr);
#ifdef COMPARE_AND_CONTRAST
                        bytelen = left*bpp; start = dptr;
                        memcpy(testbuffer, dptr, bytelen);
                        rop_run(&ropper, testbuffer, left);
/**** 8-bit T & D ****/
                        if (bpp == 1) {
                            for (; left > 0; ++dptr, ++tptr, --left) {
                                byte t_pixel = *tptr;

                                rop_body_8((byte)const_source, t_pixel);
                            }
                        } else {
/**** 24-bit T & D ****/
                            for (; left > 0; dptr += 3, tptr += 3, --left) {
                                bits32 t_pixel = get24(tptr);

                                rop_body_24(const_source, t_pixel);
                            }
                        }
                        if (memcmp(testbuffer, start, bytelen) != 0) {
                            emprintf(dev->memory, "Failed!\n");
                        }
#else
/**** 8-bit T & D ****/
/**** 24-bit T & D ****/
                        rop_run(&ropper, dptr, left);
                        dptr += left * bpp;
#endif
                    }
                }
                rop_release_run_op(&ropper);
            }
        }
    } else {
/**** Data source & texture ****/
        if (scolors != NULL || tcolors != NULL) {
            uint traster = textures->raster;
            int ty = y + phase_y;
            const byte *srow = sdata;

            if (scolors)
                rop_set_s_colors(&ropper, scolors);
            if (tcolors)
                rop_set_s_colors(&ropper, tcolors);
            if (rop_get_run_op(&ropper, lop, depth,
                               ((scolors == NULL ? 0 : rop_s_1bit) |
                                (tcolors == NULL ? 0 : rop_t_1bit)))) {
                /* Loop over scan lines. */
                for (; line_count-- > 0; drow += draster, srow += sraster, ++ty) {  /* Loop over copies of the tile. */
                    int sx = sourcex;
                    int dx = x;
                    int w = width;
                    int nw;
                    byte *dptr = drow;
                    const byte *trow =
                    textures->data + (ty % textures->size.y) * traster;
                    int xoff = x_offset(phase_x, ty, textures);

                    for (; w > 0; dx += nw, w -= nw) {      /* Loop over individual pixels. */
                        int tx = (dx + xoff) % textures->rep_width;
                        int left = nw = min(w, textures->size.x - tx);
                        const byte *sptr = srow + sx*bpp;
                        const byte *tptr = trow + tx*bpp;

                        /*
                         * For maximum speed, we should split this loop
                         * into 7 cases depending on source & texture
                         * depth: (1,1), (1,8), (1,24), (8,1), (8,8),
                         * (24,1), (24,24).  But since we expect these
                         * cases to be relatively uncommon, we just
                         * divide on the destination depth.
                         */
                        if (scolors)
                            rop_set_s_bitmap_subbyte(&ropper, srow, sx);
                        else
                            rop_set_s_bitmap(&ropper, sptr);
                        if (tcolors)
                            rop_set_t_bitmap_subbyte(&ropper, trow, tx);
                        else
                            rop_set_t_bitmap(&ropper, tptr);

#ifdef COMPARE_AND_CONTRAST
                        bytelen = left*bpp; start = dptr;
                        memcpy(testbuffer, dptr, bytelen);
                        rop_run(&ropper, testbuffer, left);
                        if (bpp == 1) {
/**** 8-bit destination ****/
                            for (; left > 0; ++dptr, ++sptr, ++tptr, ++sx, ++tx, --left) {
                                byte s_pixel =
                                    (scolors ? cbit8(srow, sx, scolors) : *sptr);
                                byte t_pixel =
                                    (tcolors ? cbit8(trow, tx, tcolors) : *tptr);

                                rop_body_8(s_pixel, t_pixel);
                            }
                        } else {
/**** 24-bit destination ****/
                            for (; left > 0; dptr += 3, sptr += 3, tptr += 3, ++sx, ++tx, --left) {
                                bits32 s_pixel =
                                    (scolors ? cbit24(srow, sx, scolors) :
                                     get24(sptr));
                                bits32 t_pixel =
                                    (tcolors ? cbit24(tptr, tx, tcolors) :
                                     get24(tptr));

                                rop_body_24(s_pixel, t_pixel);
                            }
                        }
                        if (memcmp(testbuffer, start, bytelen) != 0) {
                            emprintf(dev->memory, "Failed!\n");
                        }
#else
                        rop_run(&ropper, dptr, left);
                        dptr += left * bpp;
#endif
                    }
                }
            }
        } else {
            uint traster = textures->raster;
            int ty = y + phase_y;
            const byte *srow = sdata;

            /* Loop over scan lines. */
            if (rop_get_run_op(&ropper, lop, depth, 0)) {
                for (; line_count-- > 0; drow += draster, srow += sraster, ++ty) {  /* Loop over copies of the tile. */
                    int sx = sourcex;
                    int dx = x;
                    int w = width;
                    int nw;
                    byte *dptr = drow;
                    const byte *trow =
                    textures->data + (ty % textures->size.y) * traster;
                    int xoff = x_offset(phase_x, ty, textures);

                    for (; w > 0; sx += nw, dx += nw, w -= nw) {      /* Loop over individual pixels. */
                        int tx = (dx + xoff) % textures->rep_width;
                        int left = nw = min(w, textures->size.x - tx);
                        const byte *tptr = trow + tx * bpp;
                        const byte *sptr = srow + sx * bpp;

                        rop_set_s_bitmap(&ropper, sptr);
                        rop_set_t_bitmap(&ropper, tptr);
#ifdef COMPARE_AND_CONTRAST
                        if (bpp == 1) {
                            rop_run(&ropper, testbuffer, left);
/**** 8-bit destination ****/

                            for (; left > 0; ++dptr, ++sptr, ++tptr, ++sx, ++tx, --left) {
                                rop_body_8(*sptr, *tptr);
                            }
                        } else {
/**** 24-bit destination ****/
                            for (; left > 0; dptr += 3, sptr += 3, tptr += 3, ++sx, ++tx, --left) {
                                bits32 s_pixel = get24(sptr);
                                bits32 t_pixel = get24(tptr);

                                rop_body_24(s_pixel, t_pixel);
                            }
                        }
                        if (memcmp(testbuffer, start, bytelen) != 0) {
                            emprintf(dev->memory, "Failed!\n");
                        }
#else
/**** 8-bit destination ****/
/**** 24-bit destination ****/
                        rop_run(&ropper, dptr, left);
                        dptr += left * bpp;
#endif
                    }
                }
                rop_release_run_op(&ropper);
            }
        }
    }
#undef rop_body_8
#undef rop_body_24
#undef dbit
#undef cbit8
#undef cbit24
#else

#define dbit(base, i) ((base)[(i) >> 3] & (0x80 >> ((i) & 7)))
/* 8-bit */
#define cbit8(base, i, colors)\
  (dbit(base, i) ? (byte)colors[1] : (byte)colors[0])
#define rop_body_8(s_pixel, t_pixel)\
  *dptr = (*rop_proc_table[rop])(*dptr, s_pixel, t_pixel)
/* 24-bit */
#define get24(ptr)\
  (((gx_color_index)(ptr)[0] << 16) | ((gx_color_index)(ptr)[1] << 8) | (ptr)[2])
#define put24(ptr, pixel)\
  (ptr)[0] = (byte)((pixel) >> 16),\
  (ptr)[1] = (byte)((uint)(pixel) >> 8),\
  (ptr)[2] = (byte)(pixel)
#define cbit24(base, i, colors)\
  (dbit(base, i) ? colors[1] : colors[0])
#define rop_body_24(s_pixel, t_pixel)\
  { gx_color_index d_pixel = get24(dptr);\
    d_pixel = (*rop_proc_table[rop])(d_pixel, s_pixel, t_pixel);\
    put24(dptr, d_pixel);\
  }

    if (const_texture != gx_no_color_index) {
/**** Constant texture ****/
        if (const_source != gx_no_color_index) {
/**** Constant source & texture ****/
            for (; line_count-- > 0; drow += draster) {
                byte *dptr = drow;
                int left = width;

                if (bpp == 1)
/**** 8-bit destination ****/
                    for (; left > 0; ++dptr, --left) {
                        rop_body_8((byte)const_source, (byte)const_texture);
                    }
                else
/**** 24-bit destination ****/
                    for (; left > 0; dptr += 3, --left) {
                        rop_body_24(const_source, const_texture);
                    }
            }
        } else {
/**** Data source, const texture ****/
            const byte *srow = sdata;

            for (; line_count-- > 0; drow += draster, srow += sraster) {
                byte *dptr = drow;
                int left = width;

                if (scolors) {
/**** 1-bit source ****/
                    int sx = sourcex;

                    if (bpp == 1)
/**** 8-bit destination ****/
                        for (; left > 0; ++dptr, ++sx, --left) {
                            byte s_pixel = cbit8(srow, sx, scolors);

                            rop_body_8(s_pixel, (byte)const_texture);
                        }
                    else
/**** 24-bit destination ****/
                        for (; left > 0; dptr += 3, ++sx, --left) {
                            bits32 s_pixel = cbit24(srow, sx, scolors);

                            rop_body_24(s_pixel, const_texture);
                        }
                } else if (bpp == 1) {
/**** 8-bit source & dest ****/
                    const byte *sptr = srow + sourcex;

                    for (; left > 0; ++dptr, ++sptr, --left) {
                        byte s_pixel = *sptr;

                        rop_body_8(s_pixel, (byte)const_texture);
                    }
                } else {
/**** 24-bit source & dest ****/
                    const byte *sptr = srow + sourcex * 3;

                    for (; left > 0; dptr += 3, sptr += 3, --left) {
                        bits32 s_pixel = get24(sptr);

                        rop_body_24(s_pixel, const_texture);
                    }
                }
            }
        }
    } else if (const_source != gx_no_color_index) {
/**** Const source, data texture ****/
        uint traster = textures->raster;
        int ty = y + phase_y;

        for (; line_count-- > 0; drow += draster, ++ty) {       /* Loop over copies of the tile. */
            int dx = x, w = width, nw;
            byte *dptr = drow;
            const byte *trow =
            textures->data + (ty % textures->size.y) * traster;
            int xoff = x_offset(phase_x, ty, textures);

            for (; w > 0; dx += nw, w -= nw) {
                int tx = (dx + xoff) % textures->rep_width;
                int left = nw = min(w, textures->size.x - tx);
                const byte *tptr = trow;

                if (tcolors) {
/**** 1-bit texture ****/
                    if (bpp == 1)
/**** 8-bit dest ****/
                        for (; left > 0; ++dptr, ++tx, --left) {
                            byte t_pixel = cbit8(tptr, tx, tcolors);

                            rop_body_8((byte)const_source, t_pixel);
                        }
                    else
/**** 24-bit dest ****/
                        for (; left > 0; dptr += 3, ++tx, --left) {
                            bits32 t_pixel = cbit24(tptr, tx, tcolors);

                            rop_body_24(const_source, t_pixel);
                        }
                } else if (bpp == 1) {
/**** 8-bit T & D ****/
                    tptr += tx;
                    for (; left > 0; ++dptr, ++tptr, --left) {
                        byte t_pixel = *tptr;

                        rop_body_8((byte)const_source, t_pixel);
                    }
                } else {
/**** 24-bit T & D ****/
                    tptr += tx * 3;
                    for (; left > 0; dptr += 3, tptr += 3, --left) {
                        bits32 t_pixel = get24(tptr);

                        rop_body_24(const_source, t_pixel);
                    }
                }
            }
        }
    } else {
/**** Data source & texture ****/
        uint traster = textures->raster;
        int ty = y + phase_y;
        const byte *srow = sdata;

        /* Loop over scan lines. */
        for (; line_count-- > 0; drow += draster, srow += sraster, ++ty) {      /* Loop over copies of the tile. */
            int sx = sourcex;
            int dx = x;
            int w = width;
            int nw;
            byte *dptr = drow;
            const byte *trow =
            textures->data + (ty % textures->size.y) * traster;
            int xoff = x_offset(phase_x, ty, textures);

            for (; w > 0; dx += nw, w -= nw) {  /* Loop over individual pixels. */
                int tx = (dx + xoff) % textures->rep_width;
                int left = nw = min(w, textures->size.x - tx);
                const byte *tptr = trow;

                /*
                 * For maximum speed, we should split this loop
                 * into 7 cases depending on source & texture
                 * depth: (1,1), (1,8), (1,24), (8,1), (8,8),
                 * (24,1), (24,24).  But since we expect these
                 * cases to be relatively uncommon, we just
                 * divide on the destination depth.
                 */
                if (bpp == 1) {
/**** 8-bit destination ****/
                    const byte *sptr = srow + sx;

                    tptr += tx;
                    for (; left > 0; ++dptr, ++sptr, ++tptr, ++sx, ++tx, --left) {
                        byte s_pixel =
                            (scolors ? cbit8(srow, sx, scolors) : *sptr);
                        byte t_pixel =
                            (tcolors ? cbit8(tptr, tx, tcolors) : *tptr);

                        rop_body_8(s_pixel, t_pixel);
                    }
                } else {
/**** 24-bit destination ****/
                    const byte *sptr = srow + sx * 3;

                    tptr += tx * 3;
                    for (; left > 0; dptr += 3, sptr += 3, tptr += 3, ++sx, ++tx, --left) {
                        bits32 s_pixel =
                            (scolors ? cbit24(srow, sx, scolors) :
                             get24(sptr));
                        bits32 t_pixel =
                            (tcolors ? cbit24(tptr, tx, tcolors) :
                             get24(tptr));

                        rop_body_24(s_pixel, t_pixel);
                    }
                }
            }
        }
    }
#undef rop_body_8
#undef rop_body_24
#undef dbit
#undef cbit8
#undef cbit24
#endif
    return 0;
}
