/*
 * Copyright (C) 1991, 1992 Aladdin Enterprises.
 * All rights reserved.
 *  
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/*

 *
 * H-P Color LaserJet 5/5M device; based on the PaintJet.
 */
#include "math_.h"
#include "gdevprn.h"
#include "gdevpcl.h"

/* X_DPI and Y_DPI must be the same */
#define X_DPI 300
#define Y_DPI 300


/*
 * Array of paper sizes, and the corresponding offsets.
 */
typedef struct clj_paper_size_s {
    uint        tag;                /* paper type tag */
    bool        rotate;             /* true ==> rotate page */
    float       width, height;      /* in pts; +- 5 pts */
    gs_point    offsets;            /* offsets in the given orientation */
} clj_paper_size;

/*
 * The Color LaserJet prints page sizes up to 11.8" wide (A4 size) in
 * long-edge-feed (landscape) orientation. Only executive, letter, and
 * A4 size are supported for color, so we don't bother to list the others.
 */
private const clj_paper_size    paper_sizes[] = {
    {   1,  true, 7.25 * 72, 10.50 * 72, { .200 * 72, 0 } },
    {   2,  true, 8.50 * 72, 11.00 * 72, { .200 * 72, 0 } },
    {  26,  true, 8.27 * 72, 11.69 * 72, { .197 * 72, 0 } }
};


/*
 * Find the paper size information corresponding to a given pair of dimensions.
 * Paper sizes are always requested in portrait orientation.
 *
 * A return value of 0 indicates the paper size is not supported.
 */
  private const clj_paper_size *
get_paper_size(
    gx_device *             pdev
)
{
    static const float      tolerance = 5.0;
    float                   width = pdev->MediaSize[0];
    float                   height = pdev->MediaSize[1];
    const clj_paper_size *  psize = 0;
    int                     i;

    for (i = 0, psize = paper_sizes; i < countof(paper_sizes); i++, psize++) {
        if ( (fabs(width - psize->width) <= tolerance)  &&
             (fabs(height - psize->height) <= tolerance)  )
            return psize;
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
    const clj_paper_size *  psize = get_paper_size(pdev);
    floatp                  fs_dim, ss_dim;     /* fast/slow scan dimension */
    floatp                  fs_res = X_DPI / 72.0;
    floatp                  ss_res = Y_DPI / 72.0;

    /* if there is no recognized page, not much can be done */
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
    if (psize->rotate) {
        fs_dim = pdev->MediaSize[1] - 2 * psize->offsets.x;
        ss_dim = pdev->MediaSize[0] - 2 * psize->offsets.y;
        pmat->xx = 0.0;
        pmat->xy = -ss_res;
        pmat->yx = -fs_res;
        pmat->yy = 0.0;
        pmat->tx = (fs_dim + psize->offsets.x) * fs_res;
        pmat->ty = (ss_dim + psize->offsets.y) * ss_res;
    } else {
        fs_dim = pdev->MediaSize[0] - 2 * psize->offsets.x;
        ss_dim = pdev->MediaSize[1] - 2 * psize->offsets.y;
        pmat->xx = fs_res;
        pmat->xy = 0.0;
        pmat->yx = 0.0;
        pmat->yy = -ss_res;
        pmat->tx = -psize->offsets.x * fs_res;
        pmat->ty = (ss_dim + psize->offsets.y) * ss_res;
    }

    pdev->width = (int)ceil(fs_dim * fs_res - 0.5);
    pdev->height = (int)ceil(ss_dim * ss_res - 0.5);
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
#define BUFF_SIZE   (1024 / sizeof(ulong))      /* ample size */
#define MASK_SHIFT  (8 * sizeof(ulong) - 1)

    ulong               buff[3 * BUFF_SIZE];
    ulong *             p_c = buff;
    ulong *             p_m = buff + BUFF_SIZE;
    ulong *             p_y = buff + 2 * BUFF_SIZE;
    ulong *             ptrs[3];
    ulong               c_val = 0L, m_val = 0L, y_val = 0L;
    ulong               mask = 1UL << MASK_SHIFT;
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
            *p_c++ = c_val;
            c_val = 0L;
            *p_m++ = m_val;
            m_val = 0L;
            *p_y++ = y_val;
            y_val = 0L;
            mask = 1UL << MASK_SHIFT;
        }
    }
    if (mask != 1UL << MASK_SHIFT) {
        *p_c++ = c_val;
        *p_m++ = m_val;
        *p_y++ = y_val;
    }

    ptrs[0] = p_c;
    ptrs[1] = p_m;
    ptrs[2] = p_y;

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

#undef  BUFF_SIZE   (3 * 1024 / sizeof(ulong))  /* ample size */
#undef  MASK_SHIFT  (8 * sizeof(ulong) - 1)

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
    const clj_paper_size *  psize = get_paper_size((gx_device *)pdev);
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
             "\033E\033&u300D\033&l%da%dx%dO\033*p0x0y-150Y\033*t%dR"
             "\033*r-3u0f%ds%dt1A\033*b2M",
             psize->tag,
             (pdev->NumCopies_set ? pdev->NumCopies : 1),
             (psize->rotate ? 3 : 0),
             X_DPI,
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
    fputs("\033rC\f", prn_stream);

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
    gdev_prn_put_params,            /* put_params */
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
gx_device_printer   gs_cljet_device = prn_device_margins(
    clj_procs,              /* procedures */
    "cljet",                /* device name */
    110,                    /* width - will be overridden subsequently */
    85,                     /* height - will be overridden subsequently */
    X_DPI, Y_DPI,           /* resolutions - current must be the same */
    0.0, 0.0,               /* no offset; handled by initial matrix */
    0.167, 0.167,           /* margins (left, bottom, right, top */
    0.167, 0.167,
    3,                      /* color depth - 3 colors, 1 bit per pixel */
    clj_print_page          /* routine to output page */
);
