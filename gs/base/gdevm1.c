/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id$ */
/* Monobit "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"           /* semi-public definitions */
#include "gdevmem.h"            /* private definitions */
#include "gsrop.h"

/* Either we can implement copy_mono directly, or we can call copy_rop to do
 * its work. It used to be faster to do it directly, but no more. */
#define DO_COPY_MONO_BY_COPY_ROP

/* Either we can implement tile_rect directly, or we can call copy_rop to do
 * its work. It used to be faster to do it directly, but no more. */
#undef DO_TILE_RECT_BY_COPY_ROP

/* Either we can implement fill_rect directly, or we can call copy_rop to do
 * its work. For now we still implement it directly. */
#undef DO_FILL_RECT_BY_COPY_ROP

/* When implementing copy_rop, we can either use braindead local functions,
 * or appeal to the fast rop_run operations. We leave this option in for
 * debugging. */
#define USE_ROP_RUN

/* Set COMPARE_AND_CONTRAST in conjunction with USE_ROP_RUN to perform rops
 * both using rop_run and using the braindead functions and compare the
 * results. This is a hack, but useful for debugging. Use with care. */
#undef COMPARE_AND_CONTRAST

/* Calculate the X offset for a given Y value, */
/* taking shift into account if necessary. */
#define x_offset(px, ty, textures)\
  ((textures)->shift == 0 ? (px) :\
   (px) + (ty) / (textures)->rep_height * (textures)->rep_shift)

/* ---------------- Monobit RasterOp ---------------- */

/* The guts of this function originally came from mem_mono_strip_copy_rop,
 * but have been split out here to allow other callers, such as the
 * functions below. In this function, rop works in terms of device pixel
 * values, not RGB-space values. */
int
mem_mono_strip_copy_rop_dev(gx_device * dev, const byte * sdata,
                            int sourcex,uint sraster, gx_bitmap_id id,
                            const gx_color_index * scolors,
                            const gx_strip_bitmap * textures,
                            const gx_color_index * tcolors,
                            int x, int y, int width, int height,
                            int phase_x, int phase_y,
                            gs_logical_operation_t lop)
{
    gx_device_memory *mdev = (gx_device_memory *) dev;
    gs_rop3_t rop = (gs_rop3_t)lop;
    gx_strip_bitmap no_texture;
    uint draster = mdev->raster;
    uint traster;
    int line_count;
    byte *drow;
    const byte *srow;
    int ty;
#ifdef USE_ROP_RUN
    rop_run_op ropper;
#endif

    /* Modify the raster operation according to the source palette. */
    if (scolors != 0) {         /* Source with palette. */
        switch ((int)((scolors[1] << 1) + scolors[0])) {
            case 0:
                rop = rop3_know_S_0(rop);
                break;
            case 1:
                rop = rop3_invert_S(rop);
                break;
            case 2:
                break;
            case 3:
                rop = rop3_know_S_1(rop);
                break;
        }
    }
    /* Modify the raster operation according to the texture palette. */
    if (tcolors != 0) {         /* Texture with palette. */
        switch ((int)((tcolors[1] << 1) + tcolors[0])) {
            case 0:
                rop = rop3_know_T_0(rop);
                break;
            case 1:
                rop = rop3_invert_T(rop);
                break;
            case 2:
                break;
            case 3:
                rop = rop3_know_T_1(rop);
                break;
        }
    }
    /* Handle constant source and/or texture, and other special cases. */
    {
#if !defined(DO_COPY_MONO_BY_COPY_ROP) || !defined(DO_TILE_RECT_BY_COPY_ROP)
        gx_color_index color0, color1;
#endif

        switch (rop_usage_table[rop]) {
            case rop_usage_none:
#ifndef DO_FILL_RECT_BY_COPY_ROP /* Fill rect calls us - don't call it */
                /* We're just filling with a constant. */
                return (*dev_proc(dev, fill_rectangle))
                    (dev, x, y, width, height, (gx_color_index) (rop & 1));
#else
                break;
#endif
            case rop_usage_D:
                /* This is either D (no-op) or ~D. */
                if (rop == rop3_D)
                    return 0;
                /* Code no_S inline, then finish with no_T. */
                fit_fill(dev, x, y, width, height);
                sdata = scan_line_base(mdev, 0);
                sourcex = x;
                sraster = 0;
                goto no_T;
            case rop_usage_S:
#ifndef DO_COPY_MONO_BY_COPY_ROP /* Copy mono is calling us, don't call it! */
                /* This is either S or ~S, which copy_mono can handle. */
                if (rop == rop3_S)
                    color0 = 0, color1 = 1;
                else
                    color0 = 1, color1 = 0;
              do_copy:return (*dev_proc(dev, copy_mono))
                    (dev, sdata, sourcex, sraster, id, x, y, width, height,
                     color0, color1);
#else
                fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
                break;
#endif
            case rop_usage_DS:
#ifndef DO_COPY_MONO_BY_COPY_ROP /* Copy mono is calling us, don't call it! */
                /* This might be a case that copy_mono can handle. */
#define copy_case(c0, c1) color0 = c0, color1 = c1; goto do_copy;
                switch ((uint) rop) {   /* cast shuts up picky compilers */
                    case rop3_D & rop3_not(rop3_S):
                        copy_case(gx_no_color_index, 0);
                    case rop3_D | rop3_S:
                        copy_case(gx_no_color_index, 1);
                    case rop3_D & rop3_S:
                        copy_case(0, gx_no_color_index);
                    case rop3_D | rop3_not(rop3_S):
                        copy_case(1, gx_no_color_index);
                    default:;
                }
#undef copy_case
#endif
                fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
              no_T:             /* Texture is not used; textures may be garbage. */
                no_texture.data = scan_line_base(mdev, 0);  /* arbitrary */
                no_texture.raster = 0;
                no_texture.size.x = width;
                no_texture.size.y = height;
                no_texture.rep_width = no_texture.rep_height = 1;
                no_texture.rep_shift = no_texture.shift = 0;
                textures = &no_texture;
                break;
            case rop_usage_T:
#ifndef DO_TILE_RECT_BY_COPY_ROP /* Tile rect calls us - don't call it! */
                /* This is either T or ~T, which tile_rectangle can handle. */
                if (rop == rop3_T)
                    color0 = 0, color1 = 1;
                else
                    color0 = 1, color1 = 0;
              do_tile:return (*dev_proc(dev, strip_tile_rectangle))
                    (dev, textures, x, y, width, height, color0, color1,
                     phase_x, phase_y);
#else
                fit_fill(dev, x, y, width, height);
                break;
#endif
            case rop_usage_DT:
#ifndef DO_TILE_RECT_BY_COPY_ROP /* Tile rect calls us - don't call it! */
                /* This might be a case that tile_rectangle can handle. */
#define tile_case(c0, c1) color0 = c0, color1 = c1; goto do_tile;
                switch ((uint) rop) {   /* cast shuts up picky compilers */
                    case rop3_D & rop3_not(rop3_T):
                        tile_case(gx_no_color_index, 0);
                    case rop3_D | rop3_T:
                        tile_case(gx_no_color_index, 1);
                    case rop3_D & rop3_T:
                        tile_case(0, gx_no_color_index);
                    case rop3_D | rop3_not(rop3_T):
                        tile_case(1, gx_no_color_index);
                    default:;
                }
#undef tile_case
#endif
                fit_fill(dev, x, y, width, height);
                /* Source is not used; sdata et al may be garbage. */
                sdata = mdev->base;     /* arbitrary, as long as all */
                                        /* accesses are valid */
                sourcex = x;    /* guarantee no source skew */
                sraster = 0;
                break;
            default:            /* rop_usage_[D]ST */
                fit_copy(dev, sdata, sourcex, sraster, id, x, y, width, height);
        }
    }

#ifdef DEBUG
    if_debug1('b', "final rop=0x%x\n", rop);
#endif

    /* Set up transfer parameters. */
    line_count = height;
    srow = sdata;
    drow = scan_line_base(mdev, y);
    traster = (textures ? textures->raster : 0);
    ty = y + phase_y;

#ifdef USE_ROP_RUN
    rop_get_run_op(&ropper, rop, 1, 0);
#endif

    /* Loop over scan lines. */
    for (; line_count-- > 0; drow += draster, srow += sraster, ++ty) {
        int sx = sourcex;
        int dx = x;
        int w = width;
        const byte *trow = (textures == NULL ? 0 :
                            textures->data + (ty % textures->rep_height) * traster);
        int xoff = (textures ? x_offset(phase_x, ty, textures) : 0);
        int nw;

        /* Loop over (horizontal) copies of the tile. */
        for (; w > 0; sx += nw, dx += nw, w -= nw) {
#ifndef USE_ROP_RUN
            int dbit = dx & 7;
            int sbit = sx & 7;
            int sskew = sbit - dbit;
            int tx = (dx + xoff) % textures->rep_width;
            int tbit = tx & 7;
            int tskew = tbit - dbit;
            int left = nw = min(w, textures->size.x - tx);
            byte lmask = 0xff >> dbit;
            byte rmask = 0xff << (~(dbit + nw - 1) & 7);
            byte mask = lmask;
            int nx = 8 - dbit;
            byte *dptr = drow + (dx >> 3);
            const byte *sptr = srow + (sx >> 3);
            const byte *tptr = trow + (tx >> 3);

            if (sskew < 0)
                --sptr, sskew += 8;
            if (tskew < 0)
                --tptr, tskew += 8;
            for (; left > 0;
                 left -= nx, mask = 0xff, nx = 8,
                 ++dptr, ++sptr, ++tptr
                ) {
                byte dbyte = *dptr;

#define fetch1(ptr, skew)\
  (skew ? (ptr[0] << skew) + (ptr[1] >> (8 - skew)) : *ptr)
                byte sbyte = fetch1(sptr, sskew);
                byte tbyte = fetch1(tptr, tskew);

#undef fetch1
                byte result =
                (*rop_proc_table[rop]) (dbyte, sbyte, tbyte);

                if (left <= nx)
                    mask &= rmask;
                *dptr = (mask == 0xff ? result :
                         (result & mask) | (dbyte & ~mask));
            }
#else
            int dbit = dx & 7;
            int sbit = sx & 7;
            int tx = (textures ? (dx + xoff) % textures->rep_width : 0);
            int tbit = tx & 7;
            int left = nw = (textures ? min(w, textures->size.x - tx) : w);
            byte *dptr = drow + (dx >> 3);
            const byte *sptr = srow + (sx >> 3);
            const byte *tptr = trow + (tx >> 3);
#ifdef COMPARE_AND_CONTRAST
            int sskew = sbit - dbit;
            int tskew = tbit - dbit;
            byte lmask = 0xff >> dbit;
            byte rmask = 0xff << (~(dbit + nw - 1) & 7);
            byte mask = lmask;
            int nx = 8 - dbit;

            static byte testbuffer[4096];
            byte *start = dptr-2;
            int bytelen = (left+32+7)>>3;
            memcpy(testbuffer, start, bytelen);
#endif
            rop_set_s_bitmap_subbyte(&ropper, sptr, sbit);
            rop_set_t_bitmap_subbyte(&ropper, tptr, tbit);
#ifndef COMPARE_AND_CONTRAST
            rop_run_subbyte(&ropper, dptr, dbit, left);
#else
            eprintf5("rop=%x dbit=%d sbit=%d tbit=%d left=%d\n", rop, dbit, sbit, tbit, left);
            rop_run_subbyte(&ropper, testbuffer+2, dbit, left);
            if (sskew < 0)
                --sptr, sskew += 8;
            if (tskew < 0)
                --tptr, tskew += 8;
            for (; left > 0;
                 left -= nx, mask = 0xff, nx = 8,
                 ++dptr, ++sptr, ++tptr
                ) {
                byte dbyte = *dptr;

#define fetch1(ptr, skew)\
  (skew ? (ptr[0] << skew) + (ptr[1] >> (8 - skew)) : *ptr)
                byte sbyte = fetch1(sptr, sskew);
                byte tbyte = fetch1(tptr, tskew);

#undef fetch1
                byte result =
                (*rop_proc_table[rop]) (dbyte, sbyte, tbyte);

                if (left <= nx)
                    mask &= rmask;
                *dptr = (mask == 0xff ? result :
                         (result & mask) | (dbyte & ~mask));
            }
            if (memcmp(testbuffer, start, bytelen) != 0)
                eprintf("Different! Shoulda put a breakpoint on it.\n");
#endif
#endif
        }
    }

#ifdef USE_ROP_RUN
    rop_release_run_op(&ropper);
#endif
#ifdef DEBUG
    if (gs_debug_c('B'))
        debug_dump_bitmap(scan_line_base(mdev, y), mdev->raster,
                          height, "final dest bits");
#endif
    return 0;
}

/* ================ Standard (byte-oriented) device ================ */

/* Procedures */
static dev_proc_map_rgb_color(mem_mono_map_rgb_color);
static dev_proc_map_color_rgb(mem_mono_map_color_rgb);
static dev_proc_copy_mono(mem_mono_copy_mono);
static dev_proc_fill_rectangle(mem_mono_fill_rectangle);
static dev_proc_strip_tile_rectangle(mem_mono_strip_tile_rectangle);

/* The device descriptor. */
/* The instance is public. */
const gx_device_memory mem_mono_device =
mem_full_alpha_device("image1", 0, 1, mem_open,
                      mem_mono_map_rgb_color, mem_mono_map_color_rgb,
         mem_mono_copy_mono, gx_default_copy_color, mem_mono_fill_rectangle,
                      gx_default_map_cmyk_color, gx_no_copy_alpha,
                      mem_mono_strip_tile_rectangle, mem_mono_strip_copy_rop,
                      mem_get_bits_rectangle);

/* Map color to/from RGB.  This may be inverted. */
static gx_color_index
mem_mono_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    return (gx_default_w_b_map_rgb_color(dev, cv) ^ mdev->palette.data[0]) & 1;
}

static int
mem_mono_map_color_rgb(gx_device * dev, gx_color_index color,
                       gx_color_value prgb[3])
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    /* NB code doesn't make sense... map_color_rgb procedures return an error code */
    return (gx_default_w_b_map_color_rgb(dev, color, prgb) ^ mdev->palette.data[0]) & 1;
}

/* Fill a rectangle with a color. */
static int
mem_mono_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                        gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

#ifdef DO_FILL_RECT_BY_COPY_ROP
    return mem_mono_strip_copy_rop(dev, NULL, 0, 0, gx_no_bitmap_id, NULL,
                             NULL, NULL,
                             x, y, w, h, 0, 0,
                             (color ? rop3_1 : rop3_0));
#else
    fit_fill(dev, x, y, w, h);
    bits_fill_rectangle(scan_line_base(mdev, y), x, mdev->raster,
                        -(int)(mono_fill_chunk) color, w, h);
    return 0;
#endif
}

/* Convert x coordinate to byte offset in scan line. */
#define x_to_byte(x) ((x) >> 3)

/* Copy a monochrome bitmap. */
#undef mono_masks
#define mono_masks mono_copy_masks

/*
 * Fetch a chunk from the source.
 *
 * Since source and destination are both always big-endian,
 * fetching an aligned chunk never requires byte swapping.
 */
#define CFETCH_ALIGNED(cptr)\
  (*(const chunk *)(cptr))

/*
 * Note that the macros always cast cptr,
 * so it doesn't matter what the type of cptr is.
 */
/* cshift = chunk_bits - shift. */
#undef chunk
#if arch_is_big_endian
#  define chunk uint
#  define CFETCH_RIGHT(cptr, shift, cshift)\
        (CFETCH_ALIGNED(cptr) >> shift)
#  define CFETCH_LEFT(cptr, shift, cshift)\
        (CFETCH_ALIGNED(cptr) << shift)
#  define CFETCH_USES_CSKEW 0
/* Fetch a chunk that straddles a chunk boundary. */
#  define CFETCH2(cptr, cskew, skew)\
    (CFETCH_LEFT(cptr, cskew, skew) +\
     CFETCH_RIGHT((const chunk *)(cptr) + 1, skew, cskew))
#else /* little-endian */
#  define chunk bits16
static const bits16 right_masks2[9] =
{
    0xffff, 0x7f7f, 0x3f3f, 0x1f1f, 0x0f0f, 0x0707, 0x0303, 0x0101, 0x0000
};
static const bits16 left_masks2[9] =
{
    0xffff, 0xfefe, 0xfcfc, 0xf8f8, 0xf0f0, 0xe0e0, 0xc0c0, 0x8080, 0x0000
};

#  define CCONT(cptr, off) (((const chunk *)(cptr))[off])
#  define CFETCH_RIGHT(cptr, shift, cshift)\
        ((shift) < 8 ?\
         ((CCONT(cptr, 0) >> (shift)) & right_masks2[shift]) +\
          (CCONT(cptr, 0) << (cshift)) :\
         ((chunk)*(const byte *)(cptr) << (cshift)) & 0xff00)
#  define CFETCH_LEFT(cptr, shift, cshift)\
        ((shift) < 8 ?\
         ((CCONT(cptr, 0) << (shift)) & left_masks2[shift]) +\
          (CCONT(cptr, 0) >> (cshift)) :\
         ((CCONT(cptr, 0) & 0xff00) >> (cshift)) & 0xff)
#  define CFETCH_USES_CSKEW 1
/* Fetch a chunk that straddles a chunk boundary. */
/* We can avoid testing the shift amount twice */
/* by expanding the CFETCH_LEFT/right macros in-line. */
#  define CFETCH2(cptr, cskew, skew)\
        ((cskew) < 8 ?\
         ((CCONT(cptr, 0) << (cskew)) & left_masks2[cskew]) +\
          (CCONT(cptr, 0) >> (skew)) +\
          (((chunk)(((const byte *)(cptr))[2]) << (cskew)) & 0xff00) :\
         (((CCONT(cptr, 0) & 0xff00) >> (skew)) & 0xff) +\
          ((CCONT(cptr, 1) >> (skew)) & right_masks2[skew]) +\
           (CCONT(cptr, 1) << (cskew)))
#endif

typedef enum {
    COPY_OR = 0, COPY_STORE, COPY_AND, COPY_FUNNY
} copy_function;
typedef struct {
    int invert;
    copy_function op;
} copy_mode;

/*
 * Map from <color0,color1> to copy_mode.
 * Logically, this is a 2-D array.
 * The indexing is (transparent, 0, 1, unused). */
static const copy_mode copy_modes[16] = {
    {~0, COPY_FUNNY},           /* NN */
    {~0, COPY_AND},             /* N0 */
    {0, COPY_OR},               /* N1 */
    {0, 0},                     /* unused */
    {0, COPY_AND},              /* 0N */
    {0, COPY_FUNNY},            /* 00 */
    {0, COPY_STORE},            /* 01 */
    {0, 0},                     /* unused */
    {~0, COPY_OR},              /* 1N */
    {~0, COPY_STORE},           /* 10 */
    {0, COPY_FUNNY},            /* 11 */
    {0, 0},                     /* unused */
    {0, 0},                     /* unused */
    {0, 0},                     /* unused */
    {0, 0},                     /* unused */
    {0, 0},                     /* unused */
};

/* Handle the funny cases that aren't supposed to happen. */
#define FUNNY_CASE()\
  (invert ? gs_note_error(-1) :\
   mem_mono_fill_rectangle(dev, x, y, w, h, color0))

static int
mem_mono_copy_mono(gx_device * dev,
 const byte * source_data, int source_x, int source_raster, gx_bitmap_id id,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1)
{
/* Macros for writing partial chunks. */
/* The destination pointer is always named optr, */
/* and must be declared as chunk *. */
/* CINVERT may be temporarily redefined. */
#define CINVERT(bits) ((bits) ^ invert)
#define WRITE_OR_MASKED(bits, mask, off)\
  optr[off] |= (CINVERT(bits) & mask)
#define WRITE_STORE_MASKED(bits, mask, off)\
  optr[off] = ((optr[off] & ~mask) | (CINVERT(bits) & mask))
#define WRITE_AND_MASKED(bits, mask, off)\
  optr[off] &= (CINVERT(bits) | ~mask)
/* Macros for writing full chunks. */
#define WRITE_OR(bits)  *optr |= CINVERT(bits)
#define WRITE_STORE(bits) *optr = CINVERT(bits)
#define WRITE_AND(bits) *optr &= CINVERT(bits)

#ifdef DO_COPY_MONO_BY_COPY_ROP
    return mem_mono_strip_copy_rop_dev(dev, source_data, source_x, source_raster,
                             id, NULL, NULL, NULL,
                             x, y, w, h, 0, 0,
                             ((color0 == gx_no_color_index ? rop3_D :
                               color0 == 0 ? rop3_0 : rop3_1) & ~rop3_S) |
                             ((color1 == gx_no_color_index ? rop3_D :
                               color1 == 0 ? rop3_0 : rop3_1) & rop3_S));
#else /* !DO_COPY_MONO_BY_COPY_ROP */
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    register const byte *bptr;  /* actually chunk * */
    int dbit, wleft;
    uint mask;
    copy_mode mode;

    DECLARE_SCAN_PTR_VARS(dbptr, byte *, dest_raster);
#define optr ((chunk *)dbptr)
    register int skew;
    register uint invert;

    fit_copy(dev, source_data, source_x, source_raster, id, x, y, w, h);
#if gx_no_color_index_value != -1       /* hokey! */
    if (color0 == gx_no_color_index)
        color0 = -1;
    if (color1 == gx_no_color_index)
        color1 = -1;
#endif
    mode = copy_modes[((int)color0 << 2) + (int)color1 + 5];
    invert = (uint)mode.invert; /* load register */
    SETUP_RECT_VARS(dbptr, byte *, dest_raster);
    bptr = source_data + ((source_x & ~chunk_align_bit_mask) >> 3);
    dbit = x & chunk_align_bit_mask;
    skew = dbit - (source_x & chunk_align_bit_mask);

/* Macro for incrementing to next chunk. */
#define NEXT_X_CHUNK()\
  bptr += chunk_bytes; dbptr += chunk_bytes
/* Common macro for the end of each scan line. */
#define END_Y_LOOP(sdelta, ddelta)\
  bptr += sdelta; dbptr += ddelta

    if ((wleft = w + dbit - chunk_bits) <= 0) {         /* The entire operation fits in one (destination) chunk. */
        set_mono_thin_mask(mask, w, dbit);

#define WRITE_SINGLE(wr_op, src)\
  for ( ; ; )\
   { wr_op(src, mask, 0);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }

#define WRITE1_LOOP(src)\
  switch ( mode.op ) {\
    case COPY_OR: WRITE_SINGLE(WRITE_OR_MASKED, src); break;\
    case COPY_STORE: WRITE_SINGLE(WRITE_STORE_MASKED, src); break;\
    case COPY_AND: WRITE_SINGLE(WRITE_AND_MASKED, src); break;\
    default: return FUNNY_CASE();\
  }

        if (skew >= 0) {        /* single -> single, right/no shift */
            if (skew == 0) {    /* no shift */
                WRITE1_LOOP(CFETCH_ALIGNED(bptr));
            } else {            /* right shift */
#if CFETCH_USES_CSKEW
                int cskew = chunk_bits - skew;
#endif

                WRITE1_LOOP(CFETCH_RIGHT(bptr, skew, cskew));
            }
        } else if (wleft <= skew) {     /* single -> single, left shift */
#if CFETCH_USES_CSKEW
            int cskew = chunk_bits + skew;
#endif

            skew = -skew;
            WRITE1_LOOP(CFETCH_LEFT(bptr, skew, cskew));
        } else {                /* double -> single */
            int cskew = -skew;

            skew += chunk_bits;
            WRITE1_LOOP(CFETCH2(bptr, cskew, skew));
        }
#undef WRITE1_LOOP
#undef WRITE_SINGLE
    } else if (wleft <= skew) { /* 1 source chunk -> 2 destination chunks. */
        /* This is an important special case for */
        /* both characters and halftone tiles. */
        uint rmask;
        int cskew = chunk_bits - skew;

        set_mono_left_mask(mask, dbit);
        set_mono_right_mask(rmask, wleft);
#undef CINVERT
#define CINVERT(bits) (bits)    /* pre-inverted here */

#if arch_is_big_endian          /* no byte swapping */
#  define WRITE_1TO2(wr_op)\
  for ( ; ; )\
   { register uint bits = CFETCH_ALIGNED(bptr) ^ invert;\
     wr_op(bits >> skew, mask, 0);\
     wr_op(bits << cskew, rmask, 1);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }
#else /* byte swapping */
#  define WRITE_1TO2(wr_op)\
  for ( ; ; )\
   { wr_op(CFETCH_RIGHT(bptr, skew, cskew) ^ invert, mask, 0);\
     wr_op(CFETCH_LEFT(bptr, cskew, skew) ^ invert, rmask, 1);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }
#endif

        switch (mode.op) {
            case COPY_OR:
                WRITE_1TO2(WRITE_OR_MASKED);
                break;
            case COPY_STORE:
                WRITE_1TO2(WRITE_STORE_MASKED);
                break;
            case COPY_AND:
                WRITE_1TO2(WRITE_AND_MASKED);
                break;
            default:
                return FUNNY_CASE();
        }
#undef CINVERT
#define CINVERT(bits) ((bits) ^ invert)
#undef WRITE_1TO2
    } else {                    /* More than one source chunk and more than one */
        /* destination chunk are involved. */
        uint rmask;
        int words = (wleft & ~chunk_bit_mask) >> 3;
        uint sskip = source_raster - words;
        uint dskip = dest_raster - words;
        register uint bits;

        set_mono_left_mask(mask, dbit);
        set_mono_right_mask(rmask, wleft & chunk_bit_mask);
        if (skew == 0) {        /* optimize the aligned case */

#define WRITE_ALIGNED(wr_op, wr_op_masked)\
  for ( ; ; )\
   { int count = wleft;\
     /* Do first partial chunk. */\
     wr_op_masked(CFETCH_ALIGNED(bptr), mask, 0);\
     /* Do full chunks. */\
     while ( (count -= chunk_bits) >= 0 )\
      { NEXT_X_CHUNK(); wr_op(CFETCH_ALIGNED(bptr)); }\
     /* Do last chunk */\
     if ( count > -chunk_bits )\
      { wr_op_masked(CFETCH_ALIGNED(bptr + chunk_bytes), rmask, 1); }\
     if ( --h == 0 ) break;\
     END_Y_LOOP(sskip, dskip);\
   }

            switch (mode.op) {
                case COPY_OR:
                    WRITE_ALIGNED(WRITE_OR, WRITE_OR_MASKED);
                    break;
                case COPY_STORE:
                    WRITE_ALIGNED(WRITE_STORE, WRITE_STORE_MASKED);
                    break;
                case COPY_AND:
                    WRITE_ALIGNED(WRITE_AND, WRITE_AND_MASKED);
                    break;
                default:
                    return FUNNY_CASE();
            }
#undef WRITE_ALIGNED
        } else {                /* not aligned */
            int cskew = -skew & chunk_bit_mask;
            bool case_right =
            (skew >= 0 ? true :
             ((bptr += chunk_bytes), false));

            skew &= chunk_bit_mask;

#define WRITE_UNALIGNED(wr_op, wr_op_masked)\
  /* Prefetch partial word. */\
  bits =\
    (case_right ? CFETCH_RIGHT(bptr, skew, cskew) :\
     CFETCH2(bptr - chunk_bytes, cskew, skew));\
  wr_op_masked(bits, mask, 0);\
  /* Do full chunks. */\
  while ( count >= chunk_bits )\
    { bits = CFETCH2(bptr, cskew, skew);\
      NEXT_X_CHUNK(); wr_op(bits); count -= chunk_bits;\
    }\
  /* Do last chunk */\
  if ( count > 0 )\
    { bits = CFETCH_LEFT(bptr, cskew, skew);\
      if ( count > skew ) bits += CFETCH_RIGHT(bptr + chunk_bytes, skew, cskew);\
      wr_op_masked(bits, rmask, 1);\
    }

            switch (mode.op) {
                case COPY_OR:
                    for (;;) {
                        int count = wleft;

                        WRITE_UNALIGNED(WRITE_OR, WRITE_OR_MASKED);
                        if (--h == 0)
                            break;
                        END_Y_LOOP(sskip, dskip);
                    }
                    break;
                case COPY_STORE:
                    for (;;) {
                        int count = wleft;

                        WRITE_UNALIGNED(WRITE_STORE, WRITE_STORE_MASKED);
                        if (--h == 0)
                            break;
                        END_Y_LOOP(sskip, dskip);
                    }
                    break;
                case COPY_AND:
                    for (;;) {
                        int count = wleft;

                        WRITE_UNALIGNED(WRITE_AND, WRITE_AND_MASKED);
                        if (--h == 0)
                            break;
                        END_Y_LOOP(sskip, dskip);
                    }
                    break;
                default /*case COPY_FUNNY */ :
                    return FUNNY_CASE();
            }
#undef WRITE_UNALIGNED
        }
    }
#undef END_Y_LOOP
#undef NEXT_X_CHUNK
    return 0;
#undef optr
#endif /* !USE_COPY_ROP */
}

/* Strip-tile with a monochrome halftone. */
/* This is a performance bottleneck for monochrome devices, */
/* so we re-implement it, even though it takes a lot of code. */
static int
mem_mono_strip_tile_rectangle(gx_device * dev,
                              register const gx_strip_bitmap * tiles,
int tx, int y, int tw, int th, gx_color_index color0, gx_color_index color1,
                              int px, int py)
{
#ifdef DO_TILE_RECT_BY_COPY_ROP
    return mem_mono_strip_copy_rop(dev, NULL, 0, 0, tiles->id, NULL,
                                   tiles, NULL,
                                   tx, y, tw, th, px, py,
                                   ((color0 == gx_no_color_index ? rop3_D :
                                 color0 == 0 ? rop3_0 : rop3_1) & ~rop3_T) |
                                   ((color1 == gx_no_color_index ? rop3_D :
                                  color1 == 0 ? rop3_0 : rop3_1) & rop3_T));
#else /* !USE_COPY_ROP */
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    register uint invert;
    int source_raster;
    uint tile_bits_size;
    const byte *source_data;
    const byte *end;
    int x, rw, w, h;
    register const byte *bptr;  /* actually chunk * */
    int dbit, wleft;
    uint mask;
    byte *dbase;

    DECLARE_SCAN_PTR_VARS(dbptr, byte *, dest_raster);
#define optr ((chunk *)dbptr)
    register int skew;

    /* This implementation doesn't handle strips yet. */
    if (color0 != (color1 ^ 1) || tiles->shift != 0)
        return gx_default_strip_tile_rectangle(dev, tiles, tx, y, tw, th,
                                               color0, color1, px, py);
    fit_fill(dev, tx, y, tw, th);
    invert = (uint)(-(int) color0);
    source_raster = tiles->raster;
    source_data = tiles->data + ((y + py) % tiles->rep_height) * source_raster;
    tile_bits_size = tiles->size.y * source_raster;
    end = tiles->data + tile_bits_size;
#undef END_Y_LOOP
#define END_Y_LOOP(sdelta, ddelta)\
  if ( end - bptr <= sdelta )   /* wrap around */\
    bptr -= tile_bits_size;\
  bptr += sdelta; dbptr += ddelta
    dest_raster = mdev->raster;
    dbase = scan_line_base(mdev, y);
    x = tx;
    rw = tw;
    /*
     * The outermost loop here works horizontally, one iteration per
     * copy of the tile.  Note that all iterations except the first
     * have source_x = 0.
     */
    {
        int source_x = (x + px) % tiles->rep_width;

        w = tiles->size.x - source_x;
        bptr = source_data + ((source_x & ~chunk_align_bit_mask) >> 3);
        dbit = x & chunk_align_bit_mask;
        skew = dbit - (source_x & chunk_align_bit_mask);
    }
  outer:if (w > rw)
        w = rw;
    h = th;
    dbptr = dbase + ((x >> 3) & -chunk_align_bytes);
    if ((wleft = w + dbit - chunk_bits) <= 0) {         /* The entire operation fits in one (destination) chunk. */
        set_mono_thin_mask(mask, w, dbit);
#define WRITE1_LOOP(src)\
  for ( ; ; )\
   { WRITE_STORE_MASKED(src, mask, 0);\
     if ( --h == 0 ) break;\
     END_Y_LOOP(source_raster, dest_raster);\
   }
        if (skew >= 0) {        /* single -> single, right/no shift */
            if (skew == 0) {    /* no shift */
                WRITE1_LOOP(CFETCH_ALIGNED(bptr));
            } else {            /* right shift */
#if CFETCH_USES_CSKEW
                int cskew = chunk_bits - skew;
#endif

                WRITE1_LOOP(CFETCH_RIGHT(bptr, skew, cskew));
            }
        } else if (wleft <= skew) {     /* single -> single, left shift */
#if CFETCH_USES_CSKEW
            int cskew = chunk_bits + skew;
#endif

            skew = -skew;
            WRITE1_LOOP(CFETCH_LEFT(bptr, skew, cskew));
        } else {                /* double -> single */
            int cskew = -skew;

            skew += chunk_bits;
            WRITE1_LOOP(CFETCH2(bptr, cskew, skew));
        }
#undef WRITE1_LOOP
    } else if (wleft <= skew) { /* 1 source chunk -> 2 destination chunks. */
        /* This is an important special case for */
        /* both characters and halftone tiles. */
        uint rmask;
        int cskew = chunk_bits - skew;

        set_mono_left_mask(mask, dbit);
        set_mono_right_mask(rmask, wleft);
#if arch_is_big_endian          /* no byte swapping */
#undef CINVERT
#define CINVERT(bits) (bits)    /* pre-inverted here */
        for (;;) {
            register uint bits = CFETCH_ALIGNED(bptr) ^ invert;

            WRITE_STORE_MASKED(bits >> skew, mask, 0);
            WRITE_STORE_MASKED(bits << cskew, rmask, 1);
            if (--h == 0)
                break;
            END_Y_LOOP(source_raster, dest_raster);
        }
#undef CINVERT
#define CINVERT(bits) ((bits) ^ invert)
#else /* byte swapping */
        for (;;) {
            WRITE_STORE_MASKED(CFETCH_RIGHT(bptr, skew, cskew), mask, 0);
            WRITE_STORE_MASKED(CFETCH_LEFT(bptr, cskew, skew), rmask, 1);
            if (--h == 0)
                break;
            END_Y_LOOP(source_raster, dest_raster);
        }
#endif
    } else {                    /* More than one source chunk and more than one */
        /* destination chunk are involved. */
        uint rmask;
        int words = (wleft & ~chunk_bit_mask) >> 3;
        uint sskip = source_raster - words;
        uint dskip = dest_raster - words;
        register uint bits;

#define NEXT_X_CHUNK()\
  bptr += chunk_bytes; dbptr += chunk_bytes

        set_mono_right_mask(rmask, wleft & chunk_bit_mask);
        if (skew == 0) {        /* optimize the aligned case */
            if (dbit == 0)
                mask = 0;
            else
                set_mono_left_mask(mask, dbit);
            for (;;) {
                int count = wleft;

                /* Do first partial chunk. */
                if (mask)
                    WRITE_STORE_MASKED(CFETCH_ALIGNED(bptr), mask, 0);
                else
                    WRITE_STORE(CFETCH_ALIGNED(bptr));
                /* Do full chunks. */
                while ((count -= chunk_bits) >= 0) {
                    NEXT_X_CHUNK();
                    WRITE_STORE(CFETCH_ALIGNED(bptr));
                }
                /* Do last chunk */
                if (count > -chunk_bits) {
                    WRITE_STORE_MASKED(CFETCH_ALIGNED(bptr + chunk_bytes), rmask, 1);
                }
                if (--h == 0)
                    break;
                END_Y_LOOP(sskip, dskip);
            }
        } else {                /* not aligned */
            bool case_right =
            (skew >= 0 ? true :
             ((bptr += chunk_bytes), false));
            int cskew = -skew & chunk_bit_mask;

            skew &= chunk_bit_mask;
            set_mono_left_mask(mask, dbit);
            for (;;) {
                int count = wleft;

                if (case_right)
                    bits = CFETCH_RIGHT(bptr, skew, cskew);
                else
                    bits = CFETCH2(bptr - chunk_bytes, cskew, skew);
                WRITE_STORE_MASKED(bits, mask, 0);
                /* Do full chunks. */
                while (count >= chunk_bits) {
                    bits = CFETCH2(bptr, cskew, skew);
                    NEXT_X_CHUNK();
                    WRITE_STORE(bits);
                    count -= chunk_bits;
                }
                /* Do last chunk */
                if (count > 0) {
                    bits = CFETCH_LEFT(bptr, cskew, skew);
                    if (count > skew)
                        bits += CFETCH_RIGHT(bptr + chunk_bytes, skew, cskew);
                    WRITE_STORE_MASKED(bits, rmask, 1);
                }
                if (--h == 0)
                    break;
                END_Y_LOOP(sskip, dskip);
            }
        }
    }
#undef END_Y_LOOP
#undef NEXT_X_CHUNK
#undef optr
    if ((rw -= w) > 0) {
        x += w;
        w = tiles->size.x;
        bptr = source_data;
        skew = dbit = x & chunk_align_bit_mask;
        goto outer;
    }
    return 0;
#endif /* !USE_COPY_ROP */
}

/* ================ "Word"-oriented device ================ */

/* Note that on a big-endian machine, this is the same as the */
/* standard byte-oriented-device. */

#if !arch_is_big_endian

/* Procedures */
static dev_proc_copy_mono(mem1_word_copy_mono);
static dev_proc_fill_rectangle(mem1_word_fill_rectangle);

#define mem1_word_strip_tile_rectangle gx_default_strip_tile_rectangle

/* Here is the device descriptor. */
const gx_device_memory mem_mono_word_device =
mem_full_alpha_device("image1w", 0, 1, mem_open,
                      mem_mono_map_rgb_color, mem_mono_map_color_rgb,
       mem1_word_copy_mono, gx_default_copy_color, mem1_word_fill_rectangle,
                      gx_default_map_cmyk_color, gx_no_copy_alpha,
                      mem1_word_strip_tile_rectangle, gx_no_strip_copy_rop,
                      mem_word_get_bits_rectangle);

/* Fill a rectangle with a color. */
static int
mem1_word_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                         gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *base;
    uint raster;

    fit_fill(dev, x, y, w, h);
    base = scan_line_base(mdev, y);
    raster = mdev->raster;
    mem_swap_byte_rect(base, raster, x, w, h, true);
    bits_fill_rectangle(base, x, raster, -(int)(mono_fill_chunk) color, w, h);
    mem_swap_byte_rect(base, raster, x, w, h, true);
    return 0;
}

/* Copy a bitmap. */
static int
mem1_word_copy_mono(gx_device * dev,
 const byte * source_data, int source_x, int source_raster, gx_bitmap_id id,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *row;
    uint raster;
    bool store;

    fit_copy(dev, source_data, source_x, source_raster, id, x, y, w, h);
    row = scan_line_base(mdev, y);
    raster = mdev->raster;
    store = (color0 != gx_no_color_index && color1 != gx_no_color_index);
    mem_swap_byte_rect(row, raster, x, w, h, store);
    mem_mono_copy_mono(dev, source_data, source_x, source_raster, id,
                       x, y, w, h, color0, color1);
    mem_swap_byte_rect(row, raster, x, w, h, false);
    return 0;
}

#endif /* !arch_is_big_endian */
