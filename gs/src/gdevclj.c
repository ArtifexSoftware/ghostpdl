/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*

 *
 * H-P Color LaserJet 5/5M device; based on the PaintJet.
 */
#include "math_.h"
#include "gdevprn.h"
#include "gdevpcl.h"


/*
 * The HP Color LaserJet 5/5M provides a rather unexpected speed/performance
 * tradeoff.
 *
 * When generating rasters, only the fixed (simple) color spaces provide
 * reasonable performance (in this case, reasonable != good). However, in
 * these modes, certain of the fully-saturated primary colors (cyan, blue,
 * green, and red) are rendered differently as rasters as opposed to colored
 * geometric objects. Hence, the color of the output will be other than what
 * is expected.
 *
 * Alternatively, the direct color, 1-bit per pixel scheme can be used. This
 * will produce the expected colors, but performance will deteriorate
 * significantly (observed printing time will be about 3 times longer than
 * when using the simple color mode).
 *
 * Note that when using the latter mode to view output from the PCL
 * interpreter, geometric objects and raster rendered with other than
 * geometric color spaces will have the same appearance as if sent directly
 * to the CLJ, but rasters generated from simple color spaces will have a
 * different appearance. To make the latter rasters match in appearance, the
 * faster printing mode must be used (in which the case the other objects
 * will not have the same appearance).
 */
#define USE_FAST_MODE


/* X_DPI and Y_DPI must be the same */
#define X_DPI 300
#define Y_DPI 300


/*
 * Array of paper sizes, and the corresponding offsets.
 */
typedef struct clj_paper_size_s {
    uint        tag;                /* paper type tag */
    int         orient;             /* logical page orientation to use */
    float       width, height;      /* in pts; +- 5 pts */
    gs_point    offsets;            /* offsets in the given orientation */
} clj_paper_size;

/*
 * The Color LaserJet prints page sizes up to 11.8" wide (A4 size) in
 * long-edge-feed (landscape) orientation. Only executive, letter, and
 * A4 size are supported for color, so we don't bother to list the others.
 */
private const clj_paper_size    paper_sizes[] = {
    {   1,  1, 10.50 * 72.0, 7.25 * 72.0, { .200 * 72.0, 0.0 } },
    {   2,  1, 11.00 * 72.0, 8.50 * 72.0, { .200 * 72.0, 0.0 } },
    {  26,  1, 11.69 * 72.0, 8.27 * 72.0, { .197 * 72.0, 0.0 } }
};

/*
 * The supported set of resolutions.
 *
 * The Color LaserJet 5/5M is actually a pseudo-contone device, with hardware
 * capable of providing about 16 levels of intensity. The current code does
 * not take advantage of this feature, because it is not readily controllable
 * via PCL. Rather, the device is modeled as a bi-level device in each of
 * three color planes. The maximum supported resolution for such an arrangement
 * is 300 dpi.
 *
 * The CLJ does support raster scaling, but to invoke that scaling, even for
 * integral factors, involves a large performance penalty. Hence, only those
 * resolutions that can be supported without invoking raster scaling are
 * included here. These resolutions are always the same in the fast and slow
 * scan directions, so only a single value is listed here.
 *
 * All valuse are in dots per inch.
 */
private const float supported_resolutions[] = { 75.0, 100.0, 150.0, 300.0 };


/* indicate the maximum supported resolution and scan-line length (pts) */
#define CLJ_MAX_RES        300.0
#define CLJ_MAX_SCANLINE   (12.0 * 72.0)


/*
 * Determine a requested resolution pair is supported.
 */
  private bool
is_supported_resolution(
    const float HWResolution[2]
)
{
    int     i;

    for (i = 0; i < countof(supported_resolutions); i++) {
        if (HWResolution[0] == supported_resolutions[i])
            return HWResolution[0] == HWResolution[1];
    }
    return false;
}

/*
 * Find the paper size information corresponding to a given pair of dimensions.
 * If rotatep != 0, *rotatep is set to true if the page must be rotated 90
 * degrees to fit.
 *
 * A return value of 0 indicates the paper size is not supported.
 */
  private const clj_paper_size *
get_paper_size(
    const float             MediaSize[2],
    bool *                  rotatep
)
{
    static const float      tolerance = 5.0;
    float                   width = MediaSize[0];
    float                   height = MediaSize[1];
    const clj_paper_size *  psize = 0;
    int                     i;

    for (i = 0, psize = paper_sizes; i < countof(paper_sizes); i++, psize++) {
        if ( (fabs(width - psize->width) <= tolerance)  &&
             (fabs(height - psize->height) <= tolerance)  ) {
            if (rotatep != 0)
                *rotatep = false;
            return psize;
        } else if ( (fabs(width - psize->height) <= tolerance) &&
                    (fabs(height - psize->width) <= tolerance)   ) {
            if (rotatep != 0)
                *rotatep = true;
            return psize;
        }
    }

    return 0;
}

/*
 * Get the (PostScript style) default matrix for the current page size.
 *
 * For all of the supported sizes, the page will be printed with long-edge
 * feed (the CLJ does support some additional sizes, but only for monochrome).
 * As will all HP laser printers, the printable region marin is 12 pts. from
 * the edge of the physical page.
 */
  private void
clj_get_initial_matrix(
    gx_device *             pdev,
    gs_matrix *             pmat
)
{
    bool                    rotate;
    const clj_paper_size *  psize = get_paper_size(pdev->MediaSize, &rotate);
    floatp                  fs_res = pdev->HWResolution[0] / 72.0;
    floatp                  ss_res = pdev->HWResolution[1] / 72.0;

    /* if the paper size is not recognized, not much can be done */
    if (psize == 0) {
        pmat->xx = fs_res;
        pmat->xy = 0.0;
        pmat->yx = 0.0;
        pmat->yy = -ss_res;
        pmat->tx = 0.0;
        pmat->ty = pdev->MediaSize[1] * ss_res;
        return;
    }

    /* all the pages are rotated, but we include code for both cases */
    if (rotate) {
        pmat->xx = 0.0;
        pmat->xy = ss_res;
        pmat->yx = fs_res;
        pmat->yy = 0.0;
        pmat->tx = -psize->offsets.x * fs_res;
        pmat->ty = -psize->offsets.y * ss_res;
    } else {
        pmat->xx = fs_res;
        pmat->xy = 0.0;
        pmat->yx = 0.0;
        pmat->yy = -ss_res;
        pmat->tx = -psize->offsets.x * fs_res;
        pmat->ty = pdev->height + psize->offsets.y * ss_res;
    }
}

/*
 * Special put-params routine, to intercept changes in the MediaSize, and to
 * make certain the desired MediaSize and HWResolution are supported.
 *
 * Though contrary to the documentation provided in gdevcli.h, this routine
 * uses the same method of operation as gdev_prn_put_params. Specifically,
 * if an error is detected at this level, that error is returned prior to
 * invoking the next lower level routine (in this case gdev_prn_put_params;
 * the latter procedure behaves the same way with respect to
 * gx_default_put_params). This approach is much simpler than having to change
 * the device parameters and subsequently undo the change (because several
 * parameters depend on resolution), and it is not completely clear when the
 * latter approach would be necessary (all errors other than invalid resolution
 * and/or MediaSize are ignored at this level).
 */
  private int
clj_put_params(
    gx_device *             pdev,
    gs_param_list *         plist
)
{
    gs_param_float_array    farray;
    int                      code = 0;

    if ( ((param_read_float_array(plist, "HWResolution", &farray) == 0) &&
          !is_supported_resolution(farray.data)                           ) ||
         ((param_read_float_array(plist, "PageSize", &farray) == 0) &&
          (get_paper_size(farray.data, NULL) == 0)                    )     ||
         ((param_read_float_array(plist, ".MediaSize", &farray) == 0) &&
          (get_paper_size(farray.data, NULL) == 0)                      )     )
        return_error(gs_error_rangecheck);
          
    if ((code = gdev_prn_put_params(pdev, plist)) >= 0) {
        bool                    rotate;
        const clj_paper_size *  psize = get_paper_size(pdev->MediaSize, &rotate);
        floatp                  fs_res = pdev->HWResolution[0] / 72.0;
        floatp                  ss_res = pdev->HWResolution[1] / 72.0;

        if (rotate) {
            pdev->width = (pdev->MediaSize[1] - 2 * psize->offsets.x) * fs_res;
            pdev->height = (pdev->MediaSize[0] - 2 * psize->offsets.y) * ss_res;
        } else {
            pdev->width = (pdev->MediaSize[0] - 2 * psize->offsets.x) * fs_res;
            pdev->height = (pdev->MediaSize[1] - 2 * psize->offsets.y) * ss_res;
        }
    }

    return code;
}

/*
 * Pack and then compress a scanline of data. Return the size of the compressed
 * data produced.
 *
 * Input is arranged with one byte per pixel, but only the three low-order bits
 * are used. These bits are in order ymc, with yellow being the highest order
 * bit.
 *
 * Output is arranged in three planes, with one bit per pixel per plane. The
 * Color LaserJet 5/5M does support more congenial pixel encodings, but use
 * of anything other than the fixed palettes seems to result in very poor
 * performance.
 *
 * Only compresion mode 2 is used. Compression mode 1 (pure run length) has
 * an advantage over compression mode 2 only in cases in which very long runs
 * occur (> 128 bytes). Since both methods provide good compression in that
 * case, it is not worth worrying about, and compression mode 2 provides much
 * better worst-case behavior. Compression mode 3 requires considerably more
 * effort to generate, so it is useful only when it is known a prior that
 * scanlines repeat frequently.
 */
  private void
pack_and_compress_scanline(
    const byte *        pin,
    int                 in_size,
    byte  *             pout[3],
    int                 out_size[3]
)
{
#define BUFF_SIZE                                                           \
    ( ((int)(CLJ_MAX_RES * CLJ_MAX_SCANLINE / 72.0) + sizeof(ulong) - 1)    \
         / sizeof(ulong) )

    ulong               buff[3 * BUFF_SIZE];
    byte *              p_c = (byte *)buff;
    byte *              p_m = (byte *)(buff + BUFF_SIZE);
    byte *              p_y = (byte *)(buff + 2 * BUFF_SIZE);
    ulong *             ptrs[3];
    byte                c_val = 0, m_val = 0, y_val = 0;
    ulong               mask = 0x80;
    int                 i;

    /* pack the input for 4-bits per index */
    for (i = 0; i < in_size; i++) {
        uint    ival = *pin++;

        if (ival != 0) {
            if ((ival & 0x4) != 0)
                y_val |= mask;
            if ((ival & 0x2) != 0)
                m_val |= mask;
            if ((ival & 0x1) != 0)
                c_val |= mask;
        }

        if ((mask >>= 1) == 0) {
            /* NB - write out in byte units */
            *p_c++ = c_val;
            c_val = 0L;
            *p_m++ = m_val;
            m_val = 0L;
            *p_y++ = y_val;
            y_val = 0L;
            mask = 0x80;
        }
    }
    if (mask != 0x80) {
        /* NB - write out in byte units */
        *p_c++ = c_val;
        *p_m++ = m_val;
        *p_y++ = y_val;
    }

    /* clear to up a longword boundary */
    while ((((ulong)p_c) & (sizeof(ulong) - 1)) != 0) {
        *p_c++ = 0;
        *p_m++ = 0;
        *p_y++ = 0;
    }

    ptrs[0] = (ulong *)p_c;
    ptrs[1] = (ulong *)p_m;
    ptrs[2] = (ulong *)p_y;

    for (i = 0; i < 3; i++) {
        ulong * p_start = buff + i * BUFF_SIZE;
        ulong * p_end = ptrs[i];

        /* eleminate trailing 0's */
        while ((p_end > p_start) && (p_end[-1] == 0))
            p_end--;

        if (p_start == p_end)
            out_size[i] = 0;
        else
            out_size[i] = gdev_pcl_mode2compress(p_start, p_end, pout[i]);
    }

#undef BUFF_SIZE
}

/*
 * Send the page to the printer.  Compress each scan line.
 */
  private int
clj_print_page(
    gx_device_printer *     pdev,
    FILE *                  prn_stream
)
{
    const clj_paper_size *  psize = get_paper_size(pdev->MediaSize, NULL);
    int                     lsize = pdev->width;
    int                     clsize = (lsize + (lsize + 255) / 128) / 8;
    byte *                  data = 0;
    byte *                  cdata[3];
    int                     blank_lines = 0;
    int                     i;

    /* no paper size at this point is a serious error */
    if (psize == 0)
        return_error(gs_error_unregistered);

    /* allocate memory for the raw and compressed data */
    if ((data = gs_malloc(lsize, 1, "clj_print_page(data)")) == 0)
        return_error(gs_error_VMerror);
    if ((cdata[0] = gs_malloc(3 *clsize, 1, "clj_print_page(cdata)")) == 0) {
        gs_free((char *)data, lsize, 1, "clj_print_page(data)");
        return_error(gs_error_VMerror);
    }
    cdata[1] = cdata[0] + clsize;
    cdata[2] = cdata[1] + clsize;

    /* start the page */
    fprintf( prn_stream,
             "\033E\033&u300D\033&l%da1x%dO\033*p0x0y-150Y\033*t%dR"
#ifdef USE_FAST_MODE
	     "\033*r-3U"
#else
             "\033*v6W\001\002\003\001\001\001"
#endif
             "\033*r0f%ds%dt1A\033*b2M",
             psize->tag,
             psize->orient,
             (int)(pdev->HWResolution[0]),
             pdev->width,
             pdev->height
             );

    /* process each sanline */
    for (i = 0; i < pdev->height; i++) {
        int     clen[3];

        gdev_prn_copy_scan_lines(pdev, i, data, lsize);
        pack_and_compress_scanline(data, lsize, cdata, clen);
        if ((clen[0] == 0) && (clen[1] == 0) && (clen[2] == 0))
            ++blank_lines;
        else {
            if (blank_lines != 0) {
                fprintf(prn_stream, "\033*b%dY", blank_lines);
                blank_lines = 0;
            }
            fprintf(prn_stream, "\033*b%dV", clen[0]);
            fwrite(cdata[0], sizeof(byte), clen[0], prn_stream);
            fprintf(prn_stream, "\033*b%dV", clen[1]);
            fwrite(cdata[1], sizeof(byte), clen[1], prn_stream);
            fprintf(prn_stream, "\033*b%dW", clen[2]);
            fwrite(cdata[2], sizeof(byte), clen[2], prn_stream);
        }
    }

    /* PCL will take care of blank lines at the end */
    fputs("\033*rC\f", prn_stream);

    /* free the buffers used */
    gs_free((char *)cdata[0], 3 * clsize, 1, "clj_print_page(cdata)");
    gs_free((char *)data, lsize, 1, "clj_print_page(data)");

    return 0;
}

/* CLJ device methods */
private gx_device_procs clj_procs = {
    gdev_prn_open,                  /* open_device */
    clj_get_initial_matrix,         /* get_initial matrix */
    NULL,	                    /* sync_output */
    gdev_prn_output_page,           /* output_page */
    gdev_prn_close,                 /* close_device */
    gdev_pcl_3bit_map_rgb_color,    /* map_rgb_color */
    gdev_pcl_3bit_map_color_rgb,    /* map_color_rgb */
    NULL,	                    /* fill_rectangle */
    NULL,	                    /* tile_rectangle */
    NULL,	                    /* copy_mono */
    NULL,	                    /* copy_color */
    NULL,	                    /* obsolete draw_line */
    NULL,	                    /* get_bits */
    gdev_prn_get_params,            /* get_params */
    clj_put_params,                 /* put_params */
    NULL,	                    /* map_cmyk_color */
    NULL,	                    /* get_xfont_procs */
    NULL,	                    /* get_xfont_device */
    NULL,	                    /* map_rgb_alpha_color */
    gx_page_device_get_page_device, /* get_page_device */
    NULL,	                    /* get_alpha_bits */
    NULL,	                    /* copy_alpha */
    NULL,	                    /* get_band */
    NULL,	                    /* copy_rop */
    NULL,	                    /* fill_path */
    NULL,	                    /* stroke_path */
    NULL,	                    /* fill_mask */
    NULL,	                    /* fill_trapezoid */
    NULL,	                    /* fill_parallelogram */
    NULL,	                    /* fill_triangle */
    NULL,                           /* draw_thin_line */
    NULL,                           /* begin_image */
    NULL,                           /* image_data */
    NULL,                           /* end_image */
    NULL,                           /* strip_tile_rectangle */
    NULL,                           /* strip_copy_rop, */
    NULL,                           /* get_clipping_box */
    NULL,                           /* begin_typed_image */
    NULL,                           /* get_bits_rectangle */
    NULL,                           /* map_color_rgb_alpha */
    NULL,                           /* create_compositor */
    NULL,                           /* get_hardware_params */
    NULL                            /* text_begin */
};

/* the CLJ device */
gx_device_printer   gs_cljet5_device = prn_device_margins(
    clj_procs,              /* procedures */
    "cljet5",               /* device name */
    85,                    /* width - will be overridden subsequently */
    110,                     /* height - will be overridden subsequently */
    X_DPI, Y_DPI,           /* resolutions - current must be the same */
    0.0, 0.0,               /* no offset; handled by initial matrix */
    0.167, 0.167,           /* margins (left, bottom, right, top */
    0.167, 0.167,
    3,                      /* color depth - 3 colors, 1 bit per pixel */
    clj_print_page          /* routine to output page */
);
