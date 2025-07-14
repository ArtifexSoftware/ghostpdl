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

/* PLanar Interlaced Banded device */
#include "gdevprn.h"
#include "gscdefs.h"
#include "gscspace.h" /* For pnm_begin_typed_image(..) */
#include "gxgetbit.h"
#include "gxlum.h"
#include "gxiparam.h" /* For pnm_begin_typed_image(..) */
#include "gdevmpla.h"
#include "gdevplnx.h"
#include "gdevppla.h"
#include "gdevplib.h" /* Band donor functions */
#include "gdevmem.h"

/* This file defines 5 different devices:
 *
 *  plib   24 bit RGB (8 bits per channel)
 *  plibg   8 bit Grayscale
 *  plibm   1 bit Monochrome
 *  plibc  32 bit CMYK (8 bits per channel)
 *  plibk   4 bit CMYK (1 bit per channel)
 *
 * It is intended that this device will be built on top of a 'Band Donor'
 * that will be responsible for allocating and pass us band buffers for us
 * to fill, and to process them as it wishes on completion.
 *
 * If the band_donor functions are not thread safe, or modify the device, then
 * the gdev_prn_bg_output_page should be changed to use gdev_prn_output_page.
 *
 * For debugging/QA purposes this file can be built with the following
 * define enabled, and stub versions of these band donor functions will
 * be included here.
 */
#define TESTING_WITH_NO_BAND_DONOR

/* Define DEBUG_PRINT to enable some debugging printfs. */
#undef DEBUG_PRINT

/* Define DEBUG_DUMP to dump the data to the output stream. */
#define DEBUG_DUMP

/* Define HT_RAW_DUMP to store the output as a raw CMYK buffer with the
   data size packed into the file name.  Photoshop does not handle pam
   cmyk properly so we resort to this for debugging */
#define HT_RAW_DUMP

/* Define SHORTSTOP_MEMCPY_ETC to enable braindead implementations of memcpy
 * and memset etc in this file. This serves to help profiling on some
 * systems, though it should be noted that our implementations here are NOT
 * anywhere near as efficient as typical C libraries ones. */
#undef SHORTSTOP_MEMCPY_ETC

#ifdef SHORTSTOP_MEMCPY_ETC

void *memset(void *s_, int c, size_t n)
{
  byte *s = (byte *)s_;
  while (n--)
    *s++ = (unsigned char)c;
  return s;
}

void __aebi_memset8(void *dest, size_t n, int c)
{
  memset(dest, c,n);
}
void __aebi_memset4(void *dest, size_t n, int c)
{
  memset(dest, c,n);
}
void __aebi_memset(void *dest, size_t n, int c)
{
  memset(dest, c,n);
}

void __aebi_memclr8(void *dest, size_t n)
{
  memset(dest, 0,n);
}
void __aebi_memclr4(void *dest, size_t n)
{
  memset(dest, 0,n);
}
void __aebi_memclr(void *dest, size_t n)
{
  memset(dest, 0,n);
}

void *memcpy(void *s_, const void *t_, size_t n)
{
  byte *s = (byte *)s_;
  const byte *t = (const byte *)t_;
  while (n--)
    *s++ = *t++;
  return s;
}

void __aebi_memcpy8(void *dest, const void *src, size_t n)
{
  memcpy(dest, src,n);
}
void __aebi_memcpy4(void *dest, const void *src, size_t n)
{
  memcpy(dest, src,n);
}
void __aebi_memcpy(void *dest, const void *src, size_t n)
{
  memcpy(dest, src,n);
}

void *memmove(void *s_, const void *t_, size_t n)
{
  byte *s = (byte *)s_;
  const byte *t = (const byte *)t_;

  if (s < t) {
    while (n--)
      *s++ = *t++;
  } else {
    s += n;
    t += n;
    while (n--)
      *--s = *--t;
  }
  return s;
}

void __aebi_memmove8(void *dest, const void *src, size_t n)
{
  memmove(dest, src,n);
}
void __aebi_memcmove4(void *dest, const void *src, size_t n)
{
  memmove(dest, src,n);
}
void __aebi_memmove(void *dest, const void *src, size_t n)
{
  memmove(dest, src,n);
}

#endif

#ifdef  TESTING_WITH_NO_BAND_DONOR

#include <malloc_.h>

static void *my_buffer;

int gs_band_donor_init(void        **opaque,
                       gs_memory_t  *mem)
{
#ifdef DEBUG_PRINT
    emprintf(mem, "gs_band_donor_init\n");
#endif
    *opaque = NULL;
    return 0;
}

void *gs_band_donor_band_get(void *opaque,
                             uint  uWidth,
                             uint  uHeight,
                             uint  uBitDepth,
                             uint  uComponents,
                             uint  uStride,
                             uint  uBandHeight)
{
#ifdef DEBUG_PRINT
    eprintf6("gs_band_donor_band_get[%dx%dx%dx%d (stride=%d bandHeight=%d)]\n",
             uWidth, uHeight, uBitDepth, uComponents, uStride, uBandHeight);
#endif
    my_buffer = (void *)malloc((size_t)uStride * uComponents * uBandHeight);

#ifdef DEBUG_PRINT
    q = my_buffer;
    for (y = uBandHeight; y > 0; y--) {
        for (p = 0; p < uComponents; p++) {
            memset(q, 0x10+p, uStride);
            q += uStride;
        }
    }
#endif
    return my_buffer;
}

int gs_band_donor_band_full(void *opaque, uint nLines)
{
#ifdef DEBUG_PRINT
    eprintf1("gs_band_donor_band_full[%d]\n", nLines);
#endif
    return 0;
}

int gs_band_donor_band_release(void *opaque)
{
#ifdef DEBUG_PRINT
    eprintf("gs_band_donor_band_release\n");
#endif
    free(my_buffer);
    my_buffer = NULL;
    return 0;
}

void gs_band_donor_fin(void *opaque)
{
#ifdef DEBUG_PRINT
    eprintf("gs_band_donor_fin\n");
#endif
}
#endif

/* Sanit requires us to work in bands of at least 200 lines */
#define MINBANDHEIGHT 200

/* Structure for plib devices, which extend the generic printer device. */

struct gx_device_plib_s {
    gx_device_common;
    gx_prn_device_common;
    /* Additional state for plib device */
    void *opaque;
};
typedef struct gx_device_plib_s gx_device_plib;

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 600
#define Y_DPI 600

/* For all but mono, we need our own color mapping and alpha procedures. */
static dev_proc_decode_color(plib_decode_color);
static dev_proc_encode_color(plibg_encode_color);
static dev_proc_decode_color(plibg_decode_color);
static dev_proc_decode_color(plibc_decode_color);
static dev_proc_encode_color(plibc_encode_color);
static dev_proc_map_color_rgb(plibc_map_color_rgb);

static dev_proc_open_device(plib_open);
static dev_proc_close_device(plib_close);

static dev_proc_put_params(plib_put_params);

/* And of course we need our own print-page routines. */
static dev_proc_print_page(plib_print_page);

static int plib_print_page(gx_device_printer * pdev, gp_file * pstream);
static int plibm_print_page(gx_device_printer * pdev, gp_file * pstream);
static int plibg_print_page(gx_device_printer * pdev, gp_file * pstream);
static int plibc_print_page(gx_device_printer * pdev, gp_file * pstream);
static int plibk_print_page(gx_device_printer * pdev, gp_file * pstream);

/* The device procedures */

static void
plib_base_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, open_device, plib_open);
    set_dev_proc(dev, close_device, plib_close);
    set_dev_proc(dev, put_params, plib_put_params);
}

static void
plibm_initialize_device_procs(gx_device *dev)
{
    plib_base_initialize_device_procs(dev);

    set_dev_proc(dev, encode_color, gdev_prn_map_rgb_color);
    set_dev_proc(dev, decode_color, gdev_prn_map_color_rgb);
}

static void
plibg_initialize_device_procs(gx_device *dev)
{
    plib_base_initialize_device_procs(dev);

    set_dev_proc(dev, encode_color, plibg_encode_color);
    set_dev_proc(dev, decode_color, plibg_decode_color);
}

static void
plib_initialize_device_procs(gx_device *dev)
{
    plib_base_initialize_device_procs(dev);

    set_dev_proc(dev, encode_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, plib_decode_color);
}

static void
plibc_initialize_device_procs(gx_device *dev)
{
    plib_base_initialize_device_procs(dev);

    set_dev_proc(dev, map_color_rgb, plibc_map_color_rgb);
    set_dev_proc(dev, encode_color, plibc_encode_color);
    set_dev_proc(dev, decode_color, plibc_decode_color);
}

static void
plibk_initialize_device_procs(gx_device *dev)
{
    plib_base_initialize_device_procs(dev);

    set_dev_proc(dev, map_color_rgb, plibc_map_color_rgb);
    set_dev_proc(dev, encode_color, plibc_encode_color);
    set_dev_proc(dev, decode_color, plibc_decode_color);
}

/* Macro for generating device descriptors. */
/* Ideally we'd use something like:
 * #define plib_prn_device(procs, dev_name, num_comp, depth, max_gray, max_rgb, print_page) \
 * {       prn_device_body(gx_device_plib, procs, dev_name,\
 *          DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI,\
 *          0, 0, 0, 0,\
 *          num_comp, depth, max_gray, max_rgb, max_gray + 1, max_rgb + 1,\
 *          print_page)\
 * }
 * But that doesn't let us override the band space params. So we have to do
 * it the large way.
 */
#define plib_prn_device(init, dev_name, num_comp, depth, max_gray, max_rgb, print_page) \
{       std_device_full_body_type(gx_device_plib, init, dev_name, &st_device_printer,\
          (int)((float)(DEFAULT_WIDTH_10THS) * (X_DPI) / 10 + 0.5),\
          (int)((float)(DEFAULT_HEIGHT_10THS) * (Y_DPI) / 10 + 0.5),\
          X_DPI, Y_DPI,\
          num_comp, depth, max_gray, max_rgb, max_gray + 1, max_rgb + 1,\
          (float)(0), (float)(0),\
          (float)(0), (float)(0),\
          (float)(0), (float)(0)\
        ),\
        prn_device_body_rest2_(print_page, gx_default_print_page_copies, -1)}

/* The device descriptors themselves */
const gx_device_plib gs_plib_device =
  plib_prn_device(plib_initialize_device_procs, "plib",
                  3, 24, 255, 255, plib_print_page);
const gx_device_plib gs_plibg_device =
  plib_prn_device(plibg_initialize_device_procs, "plibg",
                  1, 8, 255, 0, plibg_print_page);
const gx_device_plib gs_plibm_device =
  plib_prn_device(plibm_initialize_device_procs, "plibm",
                  1, 1, 1, 0, plibm_print_page);
const gx_device_plib gs_plibk_device =
  plib_prn_device(plibk_initialize_device_procs, "plibk",
                  4, 4, 1, 1, plibk_print_page);
const gx_device_plib gs_plibc_device =
  plib_prn_device(plibc_initialize_device_procs, "plibc",
                  4, 32, 255, 255, plibc_print_page);

/* ------ Initialization ------ */

/*
 * We need to create custom memory buffer devices that just point into the
 * bandBuffer we've got from the digicolor system.
 */
static byte *bandBufferBase = NULL;
static int   bandBufferStride = 0;

#ifdef DEBUG_DUMP
static int dump_w;
static int dump_nc;
static int dump_l2bits;

static void dump_start(int w, int h, int num_comps, int log2bits,
                       gp_file *dump_file)
{
    if ((num_comps == 3) && (log2bits == 3)) {
        /* OK */
    } else if ((num_comps == 1) && (log2bits == 0)) {
        /* OK */
    } else if ((num_comps == 1) && (log2bits == 3)) {
        /* OK */
    } else if ((num_comps == 4) && (log2bits == 0)) {
        /* OK */
    } else if ((num_comps == 4) && (log2bits == 3)) {
        /* OK */
    } else
        return;
    dump_nc = num_comps;
    dump_l2bits = log2bits;
    if (dump_file == NULL)
        return;
    if (dump_nc == 3)
        gp_fprintf(dump_file, "P6 %d %d 255\n", w, h);
    else if (dump_nc == 4) {
        gp_fprintf(dump_file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\n"
                "MAXVAL 255\nTUPLTYPE CMYK\nENDHDR\n", w, h);
    } else if (log2bits == 0)
        gp_fprintf(dump_file, "P4 %d %d\n", w, h);
    else
        gp_fprintf(dump_file, "P5 %d %d 255\n", w, h);
    dump_w = w;
}

static void dump_band(int y, gp_file *dump_file)
{
    byte *r = bandBufferBase;
    byte *g = r + bandBufferStride;
    byte *b = g + bandBufferStride;
    byte *k = b + bandBufferStride;

    if (dump_file == NULL)
        return;
    if (dump_nc == 3) {
         while (y--) {
            int w = dump_w;
            while (w--) {
                gp_fputc(*r++, dump_file);
                gp_fputc(*g++, dump_file);
                gp_fputc(*b++, dump_file);
            }
            r += bandBufferStride*3-dump_w;
            g += bandBufferStride*3-dump_w;
            b += bandBufferStride*3-dump_w;
        }
    } else if (dump_nc == 4) {
        if (dump_l2bits == 0) {
            while (y--) {
                int w = dump_w;
                while (w) {
                    byte C = *r++;
                    byte M = *g++;
                    byte Y = *b++;
                    byte K = *k++;
                    int s;
                    for (s=7; s>=0; s--) {
                        gp_fputc(255*((C>>s)&1), dump_file);
                        gp_fputc(255*((M>>s)&1), dump_file);
                        gp_fputc(255*((Y>>s)&1), dump_file);
                        gp_fputc(255*((K>>s)&1), dump_file);
                        w--;
                        if (w == 0) break;
                    }
                }
                r += bandBufferStride*4-((dump_w+7)>>3);
                g += bandBufferStride*4-((dump_w+7)>>3);
                b += bandBufferStride*4-((dump_w+7)>>3);
                k += bandBufferStride*4-((dump_w+7)>>3);
            }
        } else {
            while (y--) {
                int w = dump_w;
                while (w--) {
                    gp_fputc(*r++, dump_file);
                    gp_fputc(*g++, dump_file);
                    gp_fputc(*b++, dump_file);
                    gp_fputc(*k++, dump_file);
                }
                r += bandBufferStride*4-dump_w;
                g += bandBufferStride*4-dump_w;
                b += bandBufferStride*4-dump_w;
                k += bandBufferStride*4-dump_w;
            }
        }
    } else {
        if (dump_l2bits == 0) {
            while (y--) {
                int w = (dump_w+7)>>3;
                while (w--) {
                    gp_fputc(*r++, dump_file);
                }
                r += bandBufferStride - ((dump_w+7)>>3);
            }
        } else {
            while (y--) {
                int w = dump_w;
                while (w--) {
                    gp_fputc(*r++, dump_file);
                }
                r += bandBufferStride - dump_w;
            }
        }
    }
}
#endif

int
plib_put_params(gx_device * pdev, gs_param_list * plist)
{
    int ecode = 0;
    int code;
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;

    /* Assumed to be valid on entry - remember it */
    int bandHeight = ppdev->space_params.band.BandHeight;

    code = gdev_prn_put_params(pdev, plist);
    /* Note that 0 means "default". This will encounter a future check in "open" */
    if (ppdev->space_params.band.BandHeight != 0 &&
        ppdev->space_params.band.BandHeight < MINBANDHEIGHT) {
        emprintf2(pdev->memory, "BandHeight of %d not valid, BandHeight minimum is %d\n",
                  ppdev->space_params.band.BandHeight, MINBANDHEIGHT);
        ecode = gs_error_rangecheck;
        /* Restore to the previous (possibly default == 0) value */
        ppdev->space_params.band.BandHeight = bandHeight;
    }
    if (ecode >= 0)
        ecode = code;
    return ecode;
}

/*
 * Set up the scan line pointers of a memory device.
 * See gxdevmem.h for the detailed specification.
 * Sets or uses line_ptrs, base, raster; uses width, color_info.depth,
 * num_planes, plane_depths, plane_depth.
 */
static int
set_line_ptrs(gx_device_memory * mdev, byte * base, int raster,
              byte **line_ptrs, int setup_height)
{
    int num_planes = mdev->color_info.num_components;
    int pi;

    if (num_planes) {
        if (base && !mdev->plane_depth)
            return_error(gs_error_rangecheck);
    } else {
        num_planes = 1;
    }
    if (line_ptrs) {
        mdev->line_ptrs = line_ptrs;
        for (pi = 0; pi < num_planes; ++pi) {
            byte **pend = line_ptrs + setup_height;
            byte *scan_line = base;

            while (line_ptrs < pend) {
                *line_ptrs++ = scan_line;
                scan_line += raster * num_planes;
            }
            base += raster;
        }
    }

    return 0;
}

static int
plib_setup_buf_device(gx_device *bdev, byte *buffer, int bytes_per_line,
                        byte **line_ptrs, int y, int setup_height,
                        int full_height)
{
    gx_device_memory *mdev = (gx_device_memory *)bdev;
    int code;

    /* buffer is the buffer used by clist writing. We could use that as the
     * page buffer, but we'd rather use the buffer given to us by the
     * digicolor code. b */

    if (line_ptrs == NULL) {
        /* Free any existing line pointers array */
        if (mdev->line_ptrs != NULL)
            gs_free_object(mdev->line_pointer_memory, mdev->line_ptrs,
                       "mem_close");
        /*
         * Allocate line pointers now; free them when we close the device.
         * Note that for multi-planar devices, we have to allocate using
         * full_height rather than setup_height.
         */
        line_ptrs = (byte **)
            gs_alloc_byte_array(mdev->memory,
                                (mdev->num_planar_planes ?
                                 (size_t)full_height * mdev->num_planar_planes :
                                 setup_height),
                                sizeof(byte *), "setup_buf_device");
        if (line_ptrs == 0)
            return_error(gs_error_VMerror);
        mdev->line_pointer_memory = mdev->memory;
        mdev->foreign_line_pointers = false;
        mdev->line_ptrs = line_ptrs;
        mdev->raster = bandBufferStride * (mdev->num_planar_planes ? mdev->num_planar_planes : 1);
    }
    mdev->height = full_height;
    code = set_line_ptrs(mdev,
                         bandBufferBase + bandBufferStride*(mdev->num_planar_planes ? mdev->num_planar_planes : 1)*y,
                         bandBufferStride,
                         line_ptrs,
                         setup_height);
    mdev->height = setup_height;
    bdev->height = setup_height; /* do here in case mdev == bdev */
    return code;
}

static int
plib_get_bits_rectangle_mem(gx_device *pdev, const gs_int_rect *prect,
                            gs_get_bits_params_t *params)
{
    gx_device_memory *mdev = (gx_device_memory *)pdev;
    int x = prect->p.x, y = prect->p.y, h = prect->q.y - y;
    /* First off, see if we can satisfy get_bits_rectangle with just returning
     * pointers to the existing data. */
    {
        gs_get_bits_params_t copy_params;
        byte **base = &scan_line_base(mdev, y);
        int code;

        copy_params.options =
            GB_COLORS_NATIVE | GB_PACKING_PLANAR | GB_ALPHA_NONE |
            (mdev->raster ==
             bitmap_raster(mdev->width * mdev->color_info.depth) ?
             GB_RASTER_STANDARD : GB_RASTER_SPECIFIED);
        copy_params.raster = mdev->raster;
        code = gx_get_bits_return_pointer(pdev, x, h, params,
                                          &copy_params, base);
        if (code >= 0)
            return code;
    }
    return mem_get_bits_rectangle(pdev, prect, params);
}

static int
plib_create_buf_device(gx_device **pbdev, gx_device *target, int y,
   const gx_render_plane_t *render_plane, gs_memory_t *mem,
   gx_color_usage_t *color_usage)
{
    int code = gdev_prn_create_buf_planar(pbdev, target, y, render_plane,
                                          mem, color_usage);
    if (code < 0)
        return code;
    if (dev_proc((*pbdev), get_bits_rectangle) == mem_get_bits_rectangle)
        set_dev_proc((*pbdev), get_bits_rectangle, plib_get_bits_rectangle_mem);
    return 0;
}

static int
plib_size_buf_device(gx_device_buf_space_t *space, gx_device *target,
                       const gx_render_plane_t *render_plane,
                       int height, bool for_band)
{
    return gdev_prn_size_buf_planar(space, target, render_plane,
                                    height, for_band);
}

/*
 * Define a special open procedure that changes create_buf_device to use
 * a planar device.
 */
static int
plib_open(gx_device * pdev)
{
    gx_device_plib * const bdev = (gx_device_plib *)pdev;
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int code;

#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plib_open\n");
#endif
    bdev->printer_procs.buf_procs.create_buf_device = plib_create_buf_device;
    bdev->printer_procs.buf_procs.setup_buf_device = plib_setup_buf_device;
    bdev->printer_procs.buf_procs.size_buf_device = plib_size_buf_device;
    pdev->num_planar_planes = 1;

    bdev->space_params.banding_type = BandingAlways;

    /* You might expect us to call gdev_prn_open_planar rather than
     * gdev_prn_open, but if we do that, it overwrites the 2 function
     * pointers we've just overwritten! */
    code = gdev_prn_open(pdev);
    if (code < 0)
        return code;
    if (ppdev->space_params.band.BandHeight < MINBANDHEIGHT) {
        emprintf2(pdev->memory, "BandHeight of %d not valid, BandHeight minimum is %d\n",
                  ((gx_device_printer *)pdev)->space_params.band.BandHeight,
                  MINBANDHEIGHT);

        return_error(gs_error_rangecheck);
    }
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    set_linear_color_bits_mask_shift(pdev);

    /* Start the actual job. */
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "calling job_begin\n");
#endif
    code = gs_band_donor_init(&bdev->opaque, pdev->memory);
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "called\n");
#endif

    return code;
}

static int
plib_close(gx_device *pdev)
{
    gx_device_plib *pldev = (gx_device_plib *)pdev;

#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plib_close\n");
#endif
    gs_band_donor_fin(pldev->opaque);
    pldev->opaque = NULL;

    return gdev_prn_close(pdev);
}

/* ------ Color mapping routines ------ */

/* Map an RGB color to a gray value. */
static gx_color_index
plibg_encode_color(gx_device * pdev, const gx_color_value cv[])
{                               /* We round the value rather than truncating it. */
    gx_color_value gray;
    gx_color_value r, g, b;

    r = cv[0]; g = cv[0]; b = cv[0];
    gray = ((r * (ulong) lum_red_weight) +
     (g * (ulong) lum_green_weight) +
     (b * (ulong) lum_blue_weight) +
     (lum_all_weights / 2)) / lum_all_weights
    * pdev->color_info.max_gray / gx_max_color_value;

    return gray;
}

/* Map a gray value back to an RGB color. */
static int
plibg_decode_color(gx_device * dev, gx_color_index color,
                   gx_color_value prgb[])
{
    gx_color_value gray =
    color * gx_max_color_value / dev->color_info.max_gray;

    prgb[0] = gray;
    prgb[1] = gray;
    prgb[2] = gray;
    return 0;
}

/* Map an rgb color tuple back to an RGB color. */
static int
plib_decode_color(gx_device * dev, gx_color_index color,
                  gx_color_value prgb[])
{
    uint bitspercolor = dev->color_info.depth / 3;
    uint colormask = (1 << bitspercolor) - 1;
    uint max_rgb = dev->color_info.max_color;

    prgb[0] = ((color >> (bitspercolor * 2)) & colormask) *
        (ulong) gx_max_color_value / max_rgb;
    prgb[1] = ((color >> bitspercolor) & colormask) *
        (ulong) gx_max_color_value / max_rgb;
    prgb[2] = (color & colormask) *
        (ulong) gx_max_color_value / max_rgb;
    return 0;
}

/* Map a cmyk color tuple back to CMYK colorants. */
static int
plibc_decode_color(gx_device * dev, gx_color_index color,
                   gx_color_value prgb[])
{
    uint bitspercolor = dev->color_info.depth / 4;
    uint colormask = (1 << bitspercolor) - 1;
    uint c, m, y, k;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / colormask))

    k = color & colormask;
    color >>= bitspercolor;
    y = color & colormask;
    color >>= bitspercolor;
    m = color & colormask;
    c = color >> bitspercolor;
    prgb[0] = cvalue(c);
    prgb[1] = cvalue(m);
    prgb[2] = cvalue(y);
    prgb[3] = cvalue(k);
    return 0;
}

/* Map CMYK to color. */
static gx_color_index
plibc_encode_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth / 4;
    gx_color_index color;
    COLROUND_VARS;

    COLROUND_SETUP(bpc);
    color = ((((((COLROUND_ROUND(cv[0]) << bpc) +
                 COLROUND_ROUND(cv[1])) << bpc) +
               COLROUND_ROUND(cv[2])) << bpc) +
             COLROUND_ROUND(cv[3]));

    /* The bitcmyk device does this:
     * return (color == gx_no_color_index ? color ^ 1 : color);
     * But I don't understand why.
     */
    return color;
}

/* Map a cmyk color back to an rgb tuple. */
static int
plibc_map_color_rgb(gx_device * dev, gx_color_index color,
                    gx_color_value prgb[3])
{
    uint bitspercolor = dev->color_info.depth / 4;
    uint colormask = (1 << bitspercolor) - 1;
    uint c, m, y, k;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / colormask))

    k = color & colormask;
    color >>= bitspercolor;
    y = color & colormask;
    color >>= bitspercolor;
    m = color & colormask;
    c = color >> bitspercolor;
    k = colormask - k;
    prgb[0] = cvalue((colormask - c) * k / colormask);
    prgb[1] = cvalue((colormask - m) * k / colormask);
    prgb[2] = cvalue((colormask - y) * k / colormask);
    return 0;
}

/* ------ Internal routines ------ */

/* Print a page using a given row printing routine. */
static int
plib_print_page_loop(gx_device_printer * pdev, int log2bits, int numComps,
                     gp_file *pstream)
{
    gx_device_plib *pldev = (gx_device_plib *)pdev;
    int lnum;
    int code = 0;
    byte *buffer;
    int stride = bitmap_raster(pdev->width * (1<<log2bits));
    int bandHeight = pdev->space_params.band.BandHeight;

#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "Calling page_begin\n");
#endif
    buffer = gs_band_donor_band_get(pldev->opaque,
                                    pdev->width,
                                    pdev->height,
                                    1<<log2bits,
                                    numComps,
                                    stride,
                                    bandHeight);
#ifdef DEBUG_PRINT
    emprintf1(pdev->memory, "Called page_begin %x\n", buffer);
#endif
    if (buffer == NULL)
        return_error(gs_error_VMerror);

    /* Write these into the globals here so the setup_buf_device code can
     * find it later. Nasty. */
    bandBufferBase = buffer;
    bandBufferStride = stride;

#ifdef DEBUG_DUMP
    dump_start(pdev->width, pdev->height, numComps, log2bits, pstream);
#endif
    for (lnum = 0; lnum < pdev->height; lnum += bandHeight) {
        gs_int_rect rect;
        gs_get_bits_params_t params;

        rect.p.x = 0;
        rect.p.y = lnum;
        rect.q.x = pdev->width;
        rect.q.y = lnum+bandHeight;
        if (rect.q.y > pdev->height)
                rect.q.y = pdev->height;
        memset(&params, 0, sizeof(params));
        params.options = GB_ALIGN_ANY |
                         GB_RETURN_POINTER |
                         GB_OFFSET_0 |
                         GB_RASTER_STANDARD |
                         GB_PACKING_PLANAR |
                         GB_COLORS_NATIVE |
                         GB_ALPHA_NONE;
        params.x_offset = 0;
        code = (*dev_proc(pdev, get_bits_rectangle))((gx_device *)pdev, &rect, &params);
        if (code < 0)
            break;
#ifdef DEBUG_DUMP
        dump_band(rect.q.y-rect.p.y, pstream);
#endif
#ifdef DEBUG_PRINT
        emprintf3(pdev->memory, "Calling band_full (%d->%d) of %d\n",
                  rect.p.y, rect.q.y, pdev->height);
#endif
        gs_band_donor_band_full(pldev->opaque, rect.q.y-rect.p.y);
#ifdef DEBUG_PRINT
        emprintf(pdev->memory, "Called band_full\n");
#endif
    }
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "Calling band_release\n");
#endif
    gs_band_donor_band_release(pldev->opaque);
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "Called band_release\n");
#endif
    return (code < 0 ? code : 0);
}

/* ------ Individual page printing routines ------ */

/* Print a monobit page. */
static int
plibm_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plibm_print_page\n");
#endif
    return plib_print_page_loop(pdev, 0, 1, pstream);
}

/* Print a gray-mapped page. */
static int
plibg_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plibg_print_page\n");
#endif
    return plib_print_page_loop(pdev, 3, 1, pstream);
}

/* Print a color-mapped page. */
static int
plib_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plibc_print_page\n");
#endif
    return plib_print_page_loop(pdev, 3, 3, pstream);
}

/* Print a 1 bit CMYK page. */
static int
plibk_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plibk_print_page\n");
#endif
    return plib_print_page_loop(pdev, 0, 4, pstream);
}

/* Print an 8bpc CMYK page. */
static int
plibc_print_page(gx_device_printer * pdev, gp_file * pstream)
{
#ifdef DEBUG_PRINT
    emprintf(pdev->memory, "plibc_print_page\n");
#endif
    return plib_print_page_loop(pdev, 3, 4, pstream);
}
