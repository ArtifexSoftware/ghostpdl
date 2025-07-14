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


/* PCX file format drivers */
#include "gdevprn.h"
#include "gdevpccm.h"
#include "gxlum.h"

/* Thanks to Phil Conrad for donating the original version */
/* of these drivers to Aladdin Enterprises. */

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

/* Monochrome. */

static dev_proc_print_page(pcxmono_print_page);

/* Use the default RGB->color map, so we get black=0, white=1. */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pcxmono_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_rgb_color, gx_default_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_map_color_rgb);
    set_dev_proc(dev, encode_color, gx_default_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_map_color_rgb);
}

const gx_device_printer gs_pcxmono_device =
prn_device(pcxmono_initialize_device_procs, "pcxmono",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           0, 0, 0, 0,		/* margins */
           1, pcxmono_print_page);

/* Chunky 8-bit gray scale. */

static dev_proc_print_page(pcx256_print_page);

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pcxgray_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray_bg(dev);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_printer gs_pcxgray_device =
{prn_device_body(gx_device_printer, pcxgray_initialize_device_procs, "pcxgray",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 1, 8, 255, 255, 256, 256, pcx256_print_page)
};

/* 4-bit planar (EGA/VGA-style) color. */

static dev_proc_print_page(pcx16_print_page);

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pcx16_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_rgb_color, pc_4bit_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, pc_4bit_map_color_rgb);
    set_dev_proc(dev, encode_color, pc_4bit_map_rgb_color);
    set_dev_proc(dev, decode_color, pc_4bit_map_color_rgb);
}

const gx_device_printer gs_pcx16_device =
{prn_device_body(gx_device_printer, pcx16_initialize_device_procs, "pcx16",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 4, 1, 1, 2, 2, pcx16_print_page)
};

/* Chunky 8-bit (SuperVGA-style) color. */
/* (Uses a fixed palette of 3,3,2 bits.) */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pcx256_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, map_rgb_color, pc_8bit_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, pc_8bit_map_color_rgb);
    set_dev_proc(dev, encode_color, pc_8bit_map_rgb_color);
    set_dev_proc(dev, decode_color, pc_8bit_map_color_rgb);
}

const gx_device_printer gs_pcx256_device =
{prn_device_body(gx_device_printer, pcx256_initialize_device_procs, "pcx256",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 8, 5, 5, 6, 6, pcx256_print_page)
};

/* 24-bit color, 3 8-bit planes. */

static dev_proc_print_page(pcx24b_print_page);

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pcx24b_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb_bg(dev);
}

const gx_device_printer gs_pcx24b_device =
prn_device(pcx24b_initialize_device_procs, "pcx24b",
           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
           X_DPI, Y_DPI,
           0, 0, 0, 0,		/* margins */
           24, pcx24b_print_page);

/* 4-bit chunky CMYK color. */

static dev_proc_print_page(pcxcmyk_print_page);

static void
pcxcmyk_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk1_bg(dev);
}

const gx_device_printer gs_pcxcmyk_device =
{prn_device_body(gx_device_printer, pcxcmyk_initialize_device_procs, "pcxcmyk",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 4, 4, 1, 1, 2, 2, pcxcmyk_print_page)
};

/* ------ Private definitions ------ */

/* All two-byte quantities are stored LSB-first! */
#if ARCH_IS_BIG_ENDIAN
#  define assign_ushort(a,v) a = ((v) >> 8) + ((v) << 8)
#else
#  define assign_ushort(a,v) a = (v)
#endif

typedef struct pcx_header_s {
    byte manuf;			/* always 0x0a */
    byte version;
#define version_2_5			0
#define version_2_8_with_palette	2
#define version_2_8_without_palette	3
#define version_3_0 /* with palette */	5
    byte encoding;		/* 1=RLE */
    byte bpp;			/* bits per pixel per plane */
    ushort x1;			/* X of upper left corner */
    ushort y1;			/* Y of upper left corner */
    ushort x2;			/* x1 + width - 1 */
    ushort y2;			/* y1 + height - 1 */
    ushort hres;		/* horz. resolution (dots per inch) */
    ushort vres;		/* vert. resolution (dots per inch) */
    byte palette[16 * 3];	/* color palette */
    byte reserved;
    byte nplanes;		/* number of color planes */
    ushort bpl;			/* number of bytes per line (uncompressed) */
    ushort palinfo;
#define palinfo_color	1
#define palinfo_gray	2
    byte xtra[58];		/* fill out header to 128 bytes */
} pcx_header;

/* Define the prototype header. */
static const pcx_header pcx_header_prototype =
{
    10,				/* manuf */
    0,				/* version (variable) */
    1,				/* encoding */
    0,				/* bpp (variable) */
    00, 00,			/* x1, y1 */
    00, 00,			/* x2, y2 (variable) */
    00, 00,			/* hres, vres (variable) */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* palette (variable) */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    0,				/* reserved */
    0,				/* nplanes (variable) */
    00,				/* bpl (variable) */
    00,				/* palinfo (variable) */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* xtra */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/*
 * Define the DCX header.  We don't actually use this yet.
 * All quantities are stored little-endian!
 bytes 0-3: ID = 987654321
 bytes 4-7: file offset of page 1
 [... up to 1023 entries ...]
 bytes N-N+3: 0 to mark end of page list
 * This is followed by the pages in order, each of which is a PCX file.
 */
#define dcx_magic 987654321
#define dcx_max_pages 1023

/* Forward declarations */
static void pcx_write_rle(const byte *, const byte *, int, gp_file *);
static int pcx_write_page(gx_device_printer *, gp_file *, pcx_header *, bool);

/* Write a monochrome PCX page. */
static int
pcxmono_print_page(gx_device_printer * pdev, gp_file * file)
{
    pcx_header header;

    header = pcx_header_prototype;
    header.version = version_2_8_with_palette;
    header.bpp = 1;
    header.nplanes = 1;
    assign_ushort(header.palinfo, palinfo_gray);
    /* Set the first two entries of the short palette. */
    memcpy((byte *) header.palette, "\000\000\000\377\377\377", 6);
    return pcx_write_page(pdev, file, &header, false);
}

/* Write an "old" PCX page. */
static const byte pcx_ega_palette[16 * 3] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0x00, 0xaa, 0xaa,
    0xaa, 0x00, 0x00, 0xaa, 0x00, 0xaa, 0xaa, 0xaa, 0x00, 0xaa, 0xaa, 0xaa,
    0x55, 0x55, 0x55, 0x55, 0x55, 0xff, 0x55, 0xff, 0x55, 0x55, 0xff, 0xff,
    0xff, 0x55, 0x55, 0xff, 0x55, 0xff, 0xff, 0xff, 0x55, 0xff, 0xff, 0xff
};
static int
pcx16_print_page(gx_device_printer * pdev, gp_file * file)
{
    pcx_header header;

    header = pcx_header_prototype;
    header.version = version_2_8_with_palette;
    header.bpp = 1;
    header.nplanes = 4;
    /* Fill the EGA palette appropriately. */
    memcpy((byte *) header.palette, pcx_ega_palette,
           sizeof(pcx_ega_palette));
    return pcx_write_page(pdev, file, &header, true);
}

/* Write a "new" PCX page. */
static int
pcx256_print_page(gx_device_printer * pdev, gp_file * file)
{
    pcx_header header;
    int code;

    header = pcx_header_prototype;
    header.version = version_3_0;
    header.bpp = 8;
    header.nplanes = 1;
    assign_ushort(header.palinfo,
                  (pdev->color_info.num_components > 1 ?
                   palinfo_color : palinfo_gray));
    code = pcx_write_page(pdev, file, &header, false);
    if (code >= 0) {		/* Write out the palette. */
        gp_fputc(0x0c, file);
        code = pc_write_palette((gx_device *) pdev, 256, file);
    }
    return code;
}

/* Write a 24-bit color PCX page. */
static int
pcx24b_print_page(gx_device_printer * pdev, gp_file * file)
{
    pcx_header header;

    header = pcx_header_prototype;
    header.version = version_3_0;
    header.bpp = 8;
    header.nplanes = 3;
    assign_ushort(header.palinfo, palinfo_color);
    return pcx_write_page(pdev, file, &header, true);
}

/* Write a 4-bit chunky CMYK color PCX page. */
static const byte pcx_cmyk_palette[16 * 3] =
{
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x0f, 0x0f, 0x00,
    0xff, 0x00, 0xff, 0x0f, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x0f, 0x00, 0x00,
    0x00, 0xff, 0xff, 0x00, 0x0f, 0x0f, 0x00, 0xff, 0x00, 0x00, 0x0f, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00, 0x0f, 0x1f, 0x1f, 0x1f, 0x0f, 0x0f, 0x0f,
};
static int
pcxcmyk_print_page(gx_device_printer * pdev, gp_file * file)
{
    pcx_header header;

    header = pcx_header_prototype;
    header.version = 2;
    header.bpp = 4;
    header.nplanes = 1;
    /* Fill the palette appropriately. */
    memcpy((byte *) header.palette, pcx_cmyk_palette,
           sizeof(pcx_cmyk_palette));
    return pcx_write_page(pdev, file, &header, false);
}

/* Write out a page in PCX format. */
/* This routine is used for all formats. */
/* The caller has set header->bpp, nplanes, and palette. */
static int
pcx_write_page(gx_device_printer * pdev, gp_file * file, pcx_header * phdr,
               bool planar)
{
    size_t raster = gdev_prn_raster(pdev);
    uint rsize = ROUND_UP((pdev->width * phdr->bpp + 7) >> 3, 2);	/* PCX format requires even */
    int height = pdev->height;
    int depth = pdev->color_info.depth;
    size_t lsize = raster + rsize;
    byte *line = gs_alloc_bytes(pdev->memory, lsize, "pcx file buffer");
    byte *plane = line + raster;
    int y;
    int code = 0;		/* return code */

    if (line == 0)		/* can't allocate line buffer */
        return_error(gs_error_VMerror);

    /* Fill in the other variable entries in the header struct. */

    assign_ushort(phdr->x2, pdev->width - 1);
    assign_ushort(phdr->y2, height - 1);
    assign_ushort(phdr->hres, (int)pdev->x_pixels_per_inch);
    assign_ushort(phdr->vres, (int)pdev->y_pixels_per_inch);
    assign_ushort(phdr->bpl, (planar || depth == 1 ? rsize :
                              raster + (raster & 1)));

    /* Write the header. */

    if (gp_fwrite((const char *)phdr, 1, 128, file) < 128) {
        code = gs_error_ioerror;
        goto pcx_done;
    }
    /* Write the contents of the image. */
    for (y = 0; y < height; y++) {
        byte *row;
        byte *end;

        code = gdev_prn_get_bits(pdev, y, line, &row);
        if (code < 0)
            break;
        end = row + raster;
        if (!planar) {		/* Just write the bits. */
            if (raster & 1) {	/* Round to even, with predictable padding. */
                *end = end[-1];
                ++end;
            }
            pcx_write_rle(row, end, 1, file);
        } else
            switch (depth) {

                case 4:
                    {
                        byte *pend = plane + rsize;
                        int shift;
                        /* We'll have a run of full 32bit words,
                         * followed by some sets of stray 4-bits. */
                        int stray = pdev->width & 7;
                        byte *fend = row + ((pdev->width & ~7)>>1);

                        for (shift = 0; shift < 4; shift++) {
                            register byte *from, *to;
                            register int bright = 1 << shift;
                            register int bleft = bright << 4;

                            for (from = row, to = plane;
                                 from < fend; from += 4
                                ) {
                                *to++ =
                                    (from[0] & bleft ? 0x80 : 0) |
                                    (from[0] & bright ? 0x40 : 0) |
                                    (from[1] & bleft ? 0x20 : 0) |
                                    (from[1] & bright ? 0x10 : 0) |
                                    (from[2] & bleft ? 0x08 : 0) |
                                    (from[2] & bright ? 0x04 : 0) |
                                    (from[3] & bleft ? 0x02 : 0) |
                                    (from[3] & bright ? 0x01 : 0);
                            }
                            if (stray) {
                                byte v = (from[0] & bleft ? 0x80 : 0);
                                if (stray > 1) v |= from[0] & bright ? 0x40 : 0;
                                if (stray > 2) v |= from[1] & bleft ? 0x20 : 0;
                                if (stray > 3) v |= from[1] & bright ? 0x10 : 0;
                                if (stray > 4) v |= from[2] & bleft ? 0x08 : 0;
                                if (stray > 5) v |= from[2] & bright ? 0x04 : 0;
                                if (stray > 6) v |= from[3] & bleft ? 0x02 : 0;
                                *to++ = v;
                            }
                            /* Continue to 'raster' rather than width. */
                            while (to < pend) {
                                *to = to[-1];
                                to++;
                            }
                            pcx_write_rle(plane, pend, 1, file);
                        }
                    }
                    break;

                case 24:
                    {
                        int pnum;

                        for (pnum = 0; pnum < 3; ++pnum) {
                            pcx_write_rle(row + pnum, row + raster, 3, file);
                            if (pdev->width & 1)
                                gp_fputc(0, file);		/* pad to even */
                        }
                    }
                    break;

                default:
                    code = gs_note_error(gs_error_rangecheck);
                    goto pcx_done;

            }
    }

  pcx_done:
    gs_free_object(pdev->memory, line, "pcx file buffer");

    return code;
}

/* ------ Internal routines ------ */

/* Write one line in PCX run-length-encoded format. */
static void
pcx_write_rle(const byte * from, const byte * end, int step, gp_file * file)
{				/*
                                 * The PCX format theoretically allows encoding runs of 63
                                 * identical bytes, but some readers can't handle repetition
                                 * counts greater than 15.
                                 */
#define MAX_RUN_COUNT 15
    int max_run = step * MAX_RUN_COUNT;

    while (from < end) {
        byte data = *from;

        from += step;
        if (from >= end || data != *from) {
            if (data >= 0xc0)
                gp_fputc(0xc1, file);
        } else {
            const byte *start = from;

            while ((from < end) && (*from == data))
                from += step;
            /* Now (from - start) / step + 1 is the run length. */
            while (from - start >= max_run) {
                gp_fputc(0xc0 + MAX_RUN_COUNT, file);
                gp_fputc(data, file);
                start += max_run;
            }
            if (from > start || data >= 0xc0)
                gp_fputc((from - start) / step + 0xc1, file);
        }
        gp_fputc(data, file);
    }
#undef MAX_RUN_COUNT
}
