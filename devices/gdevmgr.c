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

/* MGR device driver */
#include "gdevprn.h"
#include "gdevpccm.h"
#include "gdevmgr.h"

/* Structure for MGR devices, which extend the generic printer device. */
struct gx_device_mgr_s {
        gx_device_common;
        gx_prn_device_common;
        /* Add MGR specific variables */
        int mgr_depth;
        /* globals for greymapped printing */
        unsigned char bgreytable[16];
        unsigned char bgreybacktable[16];
        unsigned char bgrey256table[256];
        unsigned char bgrey256backtable[256];
        struct nclut clut[256];
};
typedef struct gx_device_mgr_s gx_device_mgr;

static unsigned int clut2mgr(int, int);
static void swap_bwords(unsigned char *, int);

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

/* Macro for generating MGR device descriptors. */
#define mgr_prn_device(procs, dev_name, num_comp, depth, mgr_depth,\
        max_gray, max_rgb, dither_gray, dither_rgb, print_page)\
{	prn_device_body(gx_device_mgr, procs, dev_name,\
          DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI,\
          0, 0, 0, 0,\
          num_comp, depth, max_gray, max_rgb, dither_gray, dither_rgb,\
          print_page),\
          mgr_depth\
}

/* For all mgr variants we do some extra things at opening time. */
/* static dev_proc_open_device(gdev_mgr_open); */
#define gdev_mgr_open gdev_prn_open		/* no we don't! */

/* And of course we need our own print-page routines. */
static dev_proc_print_page(mgr_print_page);
static dev_proc_print_page(mgrN_print_page);
static dev_proc_print_page(cmgrN_print_page);

/* The device procedures */
/* Since the print_page doesn't alter the device, this device can print in the background */
static void
mgr_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_mono_bg(dev);

    set_dev_proc(dev, open_device, gdev_mgr_open);

    set_dev_proc(dev, encode_color, gx_default_b_w_mono_encode_color);
    set_dev_proc(dev, decode_color, gx_default_b_w_mono_decode_color);
}

static void
mgrN_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray_bg(dev);

    set_dev_proc(dev, open_device, gdev_mgr_open);
    set_dev_proc(dev, encode_color, gx_default_gray_encode_color);
    set_dev_proc(dev, decode_color, gx_default_gray_decode_color);
}

static void
cmgr4_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, open_device, gdev_mgr_open);
    set_dev_proc(dev, map_rgb_color, pc_4bit_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, pc_4bit_map_color_rgb);
    set_dev_proc(dev, encode_color, pc_4bit_map_rgb_color);
    set_dev_proc(dev, decode_color, pc_4bit_map_color_rgb);
}

static void
cmgr8_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, open_device, gdev_mgr_open);
    set_dev_proc(dev, map_rgb_color, mgr_8bit_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, mgr_8bit_map_color_rgb);
    set_dev_proc(dev, encode_color, mgr_8bit_map_rgb_color);
    set_dev_proc(dev, decode_color, mgr_8bit_map_color_rgb);
}

/* The device descriptors themselves */
gx_device_mgr far_data gs_mgrmono_device =
  mgr_prn_device( mgr_initialize_device_procs,  "mgrmono", 1,  1, 1,   1,   0, 2, 0, mgr_print_page);
gx_device_mgr far_data gs_mgrgray2_device =
  mgr_prn_device(mgrN_initialize_device_procs,  "mgrgray2",1,  8, 2, 255,   0, 4, 0, mgrN_print_page);
gx_device_mgr far_data gs_mgrgray4_device =
  mgr_prn_device(mgrN_initialize_device_procs,  "mgrgray4",1,  8, 4, 255,   0,16, 0, mgrN_print_page);
gx_device_mgr far_data gs_mgrgray8_device =
  mgr_prn_device(mgrN_initialize_device_procs,  "mgrgray8",1,  8, 8, 255,   0, 0, 0, mgrN_print_page);
gx_device_mgr far_data gs_mgr4_device =
  mgr_prn_device(cmgr4_initialize_device_procs, "mgr4",    3,  8, 4,   1,   1, 2, 2, cmgrN_print_page);
gx_device_mgr far_data gs_mgr8_device =
  mgr_prn_device(cmgr8_initialize_device_procs, "mgr8",    3,  8, 8, 255, 255, 6, 5, cmgrN_print_page);

/* ------ Internal routines ------ */

/* Define a "cursor" that keeps track of where we are in the page. */
typedef struct mgr_cursor_s {
        gx_device_mgr *dev;
        int bpp;			/* bits per pixel */
        uint line_size;			/* bytes per scan line */
        byte *data;			/* output row buffer */
        int lnum;			/* row within page */
} mgr_cursor;

/* Begin an MGR output page. */
/* Write the header information and initialize the cursor. */
static int
mgr_begin_page(gx_device_mgr *bdev, gp_file *pstream, mgr_cursor *pcur)
{	struct b_header head;
        uint line_size =
                gdev_prn_raster((gx_device_printer *)bdev) + 3;
        /* FIXME: Note that there does not seem to free for 'data' LEAK ALERT */
        byte *data = (byte *)gs_malloc(bdev->memory, line_size, 1, "mgr_begin_page");
        if ( data == 0 )
                return_error(gs_error_VMerror);

        /* Write the header */
        B_PUTHDR8(&head, bdev->width, bdev->height, bdev->mgr_depth);
        if ( gp_fwrite(&head, 1, sizeof(head), pstream) < sizeof(head) )
                return_error(gs_error_ioerror);
        gp_fflush(pstream);

        /* Initialize the cursor. */
        pcur->dev = bdev;
        pcur->bpp = bdev->color_info.depth;
        pcur->line_size = line_size;
        pcur->data = data;
        pcur->lnum = 0;
        return 0;
}

/* Advance to the next row.  Return 0 if more, 1 if done. <0 if error */
static int
mgr_next_row(mgr_cursor *pcur)
{	int code = 0;

        if ( pcur->lnum >= pcur->dev->height )
        {	gs_free(((gx_device_printer *)pcur->dev)->memory,
                        (char *)pcur->data, pcur->line_size, 1,
                        "mgr_next_row(done)");
                return 1;
         }
        code = gdev_prn_copy_scan_lines((gx_device_printer *)pcur->dev,
                                 pcur->lnum++, pcur->data, pcur->line_size);
        return code < 0 ? code : 0;
}

/* ------ Individual page printing routines ------ */

#define bdev ((gx_device_mgr *)pdev)

/* Print a monochrome page. */
static int
mgr_print_page(gx_device_printer *pdev, gp_file *pstream)
{	mgr_cursor cur;
        int mgr_wide;
        int mask = 0xff;
        int code = mgr_begin_page(bdev, pstream, &cur);
        if ( code < 0 ) return code;

        mgr_wide = bdev->width;
        if (mgr_wide & 7)
        {
           mask <<= (mgr_wide&7);
           mgr_wide += 8 - (mgr_wide & 7);
        }

        mgr_wide >>= 3;
        while ( !(code = mgr_next_row(&cur)) )
        {
            cur.data[mgr_wide-1] &= mask;
            if ( gp_fwrite(cur.data, sizeof(char), mgr_wide, pstream) <
                    mgr_wide)
                return_error(gs_error_ioerror);
        }
        return (code < 0 ? code : 0);
}

/* Print a gray-mapped page. */
static int
mgrN_print_page(gx_device_printer *pdev, gp_file *pstream)
{	mgr_cursor cur;
        int i = 0, j, k, mgr_wide;
        uint mgr_line_size;
        byte *bp, *data = NULL, *dp;
        gx_device_mgr *mgr = (gx_device_mgr *)pdev;

        int code = mgr_begin_page(bdev, pstream, &cur);
        if ( code < 0 ) return code;

        mgr_wide = bdev->width;
        if ( bdev->mgr_depth == 2 && mgr_wide & 3 )
            mgr_wide += 4 - (mgr_wide & 3);
        if ( bdev->mgr_depth == 4 && mgr_wide & 1 )
            mgr_wide++;
        mgr_line_size = mgr_wide / ( 8 / bdev->mgr_depth );

        if ( bdev->mgr_depth == 4 )
            for ( i = 0; i < 16; i++ ) {
                mgr->bgreytable[i] = mgrlut[LUT_BGREY][RGB_RED][i];
                mgr->bgreybacktable[mgr->bgreytable[i]] = i;
            }

        if ( bdev->mgr_depth == 8 ) {
            for ( i = 0; i < 16; i++ ) {
                mgr->bgrey256table[i] = mgrlut[LUT_BGREY][RGB_RED][i] << 4;
                mgr->bgrey256backtable[mgr->bgrey256table[i]] = i;
            }
            for ( i = 16,j = 0; i < 256; i++ ) {
                for ( k = 0; k < 16; k++ )
                  if ( j == mgrlut[LUT_BGREY][RGB_RED][k] << 4 ) {
                    j++;
                    break;
                  }
                mgr->bgrey256table[i] = j;
                mgr->bgrey256backtable[j++] = i;
            }
        }

        if ( bdev->mgr_depth != 8 ) {
            data = (byte *)gs_malloc(pdev->memory, mgr_line_size, 1, "mgrN_print_page");
            if (data == NULL)
                return_error(gs_error_VMerror);
        }

        while ( !(code = mgr_next_row(&cur)) )
           {
                switch (bdev->mgr_depth) {
                        case 2:
                                for (i = 0,dp = data,bp = cur.data; i < mgr_line_size; i++) {
                                        *dp =	*(bp++) & 0xc0;
                                        *dp |= (*(bp++) & 0xc0) >> 2;
                                        *dp |= (*(bp++) & 0xc0) >> 4;
                                    *(dp++) |= (*(bp++) & 0xc0) >> 6;
                                }
                                if ( gp_fwrite(data, sizeof(byte), mgr_line_size, pstream) < mgr_line_size )
                                        return_error(gs_error_ioerror);
                                break;

                        case 4:
                        {
                                int size = mgr_line_size - (bdev->width & 1);
                                for (i = 0,dp = data, bp = cur.data; i < size; i++) {
                                        *dp =  mgr->bgreybacktable[*(bp++) >> 4] << 4;
                                    *(dp++) |= mgr->bgreybacktable[*(bp++) >> 4];
                                }
                                if (bdev->width & 1) {
                                    *dp =  mgr->bgreybacktable[*(bp++) >> 4] << 4;
                                }
                                if ( gp_fwrite(data, sizeof(byte), mgr_line_size, pstream) < mgr_line_size )
                                        return_error(gs_error_ioerror);
                                break;
                        }
                        case 8:
                                for (i = 0,bp = cur.data; i < mgr_line_size; i++, bp++)
                                      *bp = mgr->bgrey256backtable[*bp];
                                if ( gp_fwrite(cur.data, sizeof(cur.data[0]), mgr_line_size, pstream)
                                        < mgr_line_size )
                                        return_error(gs_error_ioerror);
                                break;
                }
           }
        if (bdev->mgr_depth != 8)
            gs_free(bdev->memory, (char *)data, mgr_line_size, 1, "mgrN_print_page(done)");

        if (bdev->mgr_depth == 2) {
            for (i = 0; i < 4; i++) {
               mgr->clut[i].colnum = i;
               mgr->clut[i].red    = mgr->clut[i].green = mgr->clut[i].blue = clut2mgr(i, 2);
            }
        }
        if (bdev->mgr_depth == 4) {
            for (i = 0; i < 16; i++) {
               mgr->clut[i].colnum = i;
               mgr->clut[i].red    = mgr->clut[i].green = mgr->clut[i].blue = clut2mgr(mgr->bgreytable[i], 4);
            }
        }
        if (bdev->mgr_depth == 8) {
            for (i = 0; i < 256; i++) {
               mgr->clut[i].colnum = i;
               mgr->clut[i].red    = mgr->clut[i].green = mgr->clut[i].blue = clut2mgr(mgr->bgrey256table[i], 8);
            }
        }
#if !ARCH_IS_BIG_ENDIAN
        swap_bwords( (unsigned char *) mgr->clut, sizeof( struct nclut ) * i );
#endif
        if ( gp_fwrite(&mgr->clut, sizeof(struct nclut), i, pstream) < i )
            return_error(gs_error_ioerror);
        return (code < 0 ? code : 0);
}

/* Print a color page. */
static int
cmgrN_print_page(gx_device_printer *pdev, gp_file *pstream)
{	mgr_cursor cur;
        int i = 0, j, mgr_wide, r, g, b, colors8 = 0;
        uint mgr_line_size;
        byte *bp, *data, *dp;
        ushort prgb[3];
        unsigned char table[256], backtable[256];
        gx_device_mgr *mgr = (gx_device_mgr *)pdev;
	int mask = 0xff;

        int code = mgr_begin_page(bdev, pstream, &cur);
        if ( code < 0 ) return code;

        memset(backtable, 0x00, 256);

        mgr_wide = bdev->width;
        if (bdev->mgr_depth == 4 && mgr_wide & 1)
	{
            mgr_wide++;
	    mask = 0;
	}
        mgr_line_size = mgr_wide / (8 / bdev->mgr_depth);
        data = (byte *)gs_malloc(pdev->memory, mgr_line_size, 1, "cmgrN_print_page");
        if (data == NULL)
            return_error(gs_error_VMerror);

        if ( bdev->mgr_depth == 8 ) {
            memset( table, 0, sizeof(table) );
            for ( r = 0; r <= 6; r++ )
                for ( g = 0; g <= 6; g++ )
                    for ( b = 0; b <= 6; b++ )
                        if ( r == g && g == b )
                            table[ r + (256-7) ] = 1;
                        else
                            table[ (r << 5) + (g << 2) + (b >> 1) ] = 1;
            for ( i = j = 0; i < sizeof(table); i++ )
                if ( table[i] == 1 ) {
                    backtable[i] = j;
                    table[j++] = i;
                }
            colors8 = j;
        }
        while ( !(code = mgr_next_row(&cur)) )
           {
                switch (bdev->mgr_depth) {
                        case 4:
                                for (i = 0,dp = data, bp = cur.data; i < mgr_line_size; i++) {
                                        *dp =  *(bp++) << 4;
                                    *(dp++) |= *(bp++) & 0x0f;
                                }
				dp[-1] &= mask;
                                if ( gp_fwrite(data, sizeof(byte), mgr_line_size, pstream) < mgr_line_size )
                                        return_error(gs_error_ioerror);
                                break;

                        case 8:
                                for (i = 0,bp = cur.data; i < mgr_line_size; i++, bp++)
                                      *bp = backtable[*bp] + MGR_RESERVEDCOLORS;
                                if ( gp_fwrite(cur.data, sizeof(cur.data[0]), mgr_line_size, pstream) < mgr_line_size )
                                        return_error(gs_error_ioerror);
                                break;
                }
           }
        gs_free(bdev->memory, (char *)data, mgr_line_size, 1, "cmgrN_print_page(done)");

        if (bdev->mgr_depth == 4) {
            for (i = 0; i < 16; i++) {
               pc_4bit_map_color_rgb((gx_device *)0, (gx_color_index) i, prgb);
               mgr->clut[i].colnum = i;
               mgr->clut[i].red    = clut2mgr(prgb[0], 16);
               mgr->clut[i].green  = clut2mgr(prgb[1], 16);
               mgr->clut[i].blue   = clut2mgr(prgb[2], 16);
            }
        }
        if (bdev->mgr_depth == 8) {
            for (i = 0; i < colors8; i++) {
               mgr_8bit_map_color_rgb((gx_device *)0, (gx_color_index)
                   table[i], prgb);
               mgr->clut[i].colnum = MGR_RESERVEDCOLORS + i;
               mgr->clut[i].red    = clut2mgr(prgb[0], 16);
               mgr->clut[i].green  = clut2mgr(prgb[1], 16);
               mgr->clut[i].blue   = clut2mgr(prgb[2], 16);
            }
        }
#if !ARCH_IS_BIG_ENDIAN
        swap_bwords( (unsigned char *) mgr->clut, sizeof( struct nclut ) * i );
#endif
        if ( gp_fwrite(&mgr->clut, sizeof(struct nclut), i, pstream) < i )
            return_error(gs_error_ioerror);
        return (code < 0 ? code : 0);
}

/* Color mapping routines for 8-bit color with a fixed palette */
/* (3 bits of R, 3 bits of G, 2 bits of B). */
/* We have to trade off even spacing of colors along each axis */
/* against the desire to have real gray shades; */
/* MGR compromises by using a 7x7x4 "cube" with extra gray shades */
/* (1/6, 1/2, and 5/6), instead of the obvious 8x8x4. */

gx_color_index
mgr_8bit_map_rgb_color(gx_device *dev, const gx_color_value cv[])
{
        uint rv = cv[0] / (gx_max_color_value / 7 + 1);
        uint gv = cv[1] / (gx_max_color_value / 7 + 1);
        uint bv = cv[2] / (gx_max_color_value / 7 + 1);
        return (gx_color_index)
                (rv == gv && gv == bv ? rv + (256-7) :
                 (rv << 5) + (gv << 2) + (bv >> 1));
}
int
mgr_8bit_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	static const gx_color_value ramp[8] =
        {	0, gx_max_color_value / 6, gx_max_color_value / 3,
                gx_max_color_value / 2, 2 * (gx_max_color_value / 3),
                5 * (gx_max_color_value / 6), gx_max_color_value,
                /* The 8th entry is not actually ever used, */
                /* except to fill out the palette. */
                gx_max_color_value
        };
#define icolor (uint)color
        if ( icolor >= 256-7 )
        {	prgb[0] = prgb[1] = prgb[2] = ramp[icolor - (256-7)];
        }
        else
        {	prgb[0] = ramp[(icolor >> 5) & 7];
                prgb[1] = ramp[(icolor >> 2) & 7];
                prgb[2] = ramp[(icolor & 3) << 1];
        }
#undef icolor
        return 0;
}

/* convert the 8-bit look-up table into the standard MGR look-up table */
static unsigned int
clut2mgr(
  register int v,		/* value in clut */
  register int bits		/* number of bits in clut */
)
{
  register unsigned int i;

  i = (unsigned int) 0xffffffff / ((1<<bits)-1);
  return((v*i)/0x10000);
}

/*
 * s w a p _ b w o r d s
 */
static void
swap_bwords(register unsigned char *p, int n)
{
  register unsigned char c;

  n /= 2;

  for (; n > 0; n--, p += 2) {
    c    = p[0];
    p[0] = p[1];
    p[1] = c;
  }
}
