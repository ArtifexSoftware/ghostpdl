/* Copyright (C) 1991 Aladdin Enterprises.  All rights reserved.
   Distributed by Free Software Foundation, Inc.

This file is part of Ghostscript.

Ghostscript is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves any
particular purpose or works at all, unless he says so in writing.  Refer
to the Ghostscript General Public License for full details.

Everyone is granted permission to copy, modify and redistribute
Ghostscript, but only under the conditions described in the Ghostscript
General Public License.  A copy of this license is supposed to have been
given to you along with Ghostscript so you can know your rights and
responsibilities.  It should be in a file named COPYING.  Among other
things, the copyright notice and this notice must be preserved on all
copies.  */

/* gdevml6.c */
/* OKI MICROLINE 620CL IPL mode driver for Ghostscript */
/*
        Mar. 14, 1998	N.Tagawa

        $Id: gdevml6.c,v 1.4 2002/07/30 18:53:22 easysw Exp $
*/

#include "gdevprn.h"

/* The device descriptors */
static dev_proc_open_device(ml600_open);
static dev_proc_close_device(ml600_close);
static dev_proc_print_page(ml600_print_page);

static gx_device_procs ml600_procs =
        prn_procs(ml600_open, gdev_prn_output_page, ml600_close);

gx_device_printer gs_ml600_device =
  prn_device(ml600_procs, "ml600",
        83,				/* width_10ths, 8.3" */
        117,				/* height_10ths, 11.7" */
        600, 600,
        0.20, 0.20, 0.20, 0.20,		/* margins, lbrt */
        1, ml600_print_page);

/* ------ prototype ------ */
static int
ml_finish(
        gx_device_printer *pdev,
        gp_file           *fp);

static int
ml_init(
        gx_device_printer *pdev,
        gp_file           *fp);

static int
move_pos(
        gp_file *fp,
        int      n,
        int      m);

static int
make_line_data(
        byte	*curr_data,
        byte	*last_data,
        int	line_size,
        byte	*buf);

static int
send_line(
        byte    *buf,
        int      cnt,
        gp_file *fp);

static int
page_header(
        gx_device_printer *pdev,
        gp_file           *fp);

/* ------ Internal routines ------ */

#define ESC 0x1b
#define LINE_SIZE ((7016+7) / 8)	/* bytes per line (A4 600 DPI) */
#define ppdev	((gx_device_printer *)pdev)

static int
ml600_open(
        gx_device *pdev)
{
        int        code = gdev_prn_open(pdev);
        gp_file   *prn_stream;

        /* dprintf("gdevml6: ml600_open called\n");*/

        if (code < 0)
                return code;

        code = gdev_prn_open_printer(pdev, true);
        if (code < 0)
                return code;

        prn_stream = ppdev->file;
        return ml_init(ppdev, prn_stream);
}

static int
ml600_close(
        gx_device *pdev)
{
        int        code = gdev_prn_open_printer(pdev, true);
        gp_file   *prn_stream;

        /* dprintf("gdevml6: ml600_close called\n"); */
        if (code < 0)
                return code;

        prn_stream = ppdev->file;
        ml_finish(ppdev, prn_stream);

        return gdev_prn_close(pdev);
}

/* Send the page to the printer.  */
static int
ml600_print_page(
        gx_device_printer *pdev,
        gp_file           *prn_stream)
{
        int	ystep;
        byte	data[2][LINE_SIZE*2];
        byte	buf[LINE_SIZE*2];
        int	skip;
        int	current;
        int	c_size;
        int	lnum;
        int	line_size;
        byte	rmask;
        int	i;

#define LAST	((current ^ 1) & 1)

        /* initialize this page */
        ystep = page_header(pdev, prn_stream);
        ystep /= 300;

        /* clear last sent line buffer */
        skip = 0;
        current = 0;
        for (i = 0; i < LINE_SIZE*2; i++)
                data[LAST][i] = 0;

        /* Send each scan line */
        line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        if (line_size > LINE_SIZE || line_size == 0)
                return 0;

        rmask = (byte)(0xff << (-pdev->width & 7));	/* right edge */

        for (lnum = 0; lnum < pdev->height; lnum++) {
                (void)gdev_prn_copy_scan_lines(pdev, lnum, data[current],
                                                                line_size);
                /* Mask right edge bits */
                *(data[current] + line_size - 1) &= rmask;

                /* blank line ? */
                for (i = 0; i < line_size; i++)
                        if (data[current][i] != 0)
                                break;

                if (i == line_size) {
                        skip = 1;
                        current = LAST;
                        continue;
                }

                /* move position, if previous lines are skipped */
                if (skip) {
                        move_pos(prn_stream, lnum / ystep, lnum % ystep);
                        skip = 0;
                }

                /* create one line data */
                c_size = make_line_data(data[current], data[LAST],
                                                                line_size, buf);

                /* send one line data */
                send_line(buf, c_size, prn_stream);

                /* swap line buffer */
                current = LAST;
        }

        /* eject page */
        gp_fprintf(prn_stream, "\014");

        return 0;
}

static int
move_pos(
        gp_file *fp,
        int      n,
        int      m)
{
        gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa6, 1, 0, 2);
        gp_fprintf(fp, "%c%c%c%c%c%c%c%c%c", ESC, 0x7c, 0xa4, 4, 0,
                                            (n >> 8) & 0xff, n & 0xff, 0, 0);
        if (m > 0) {
                int	i;

                gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa6, 1, 0, 0);
                for (i = 0; i < m; i++) {
                        gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa7, 0, 1, 0);
                }
        }
        gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa6, 1, 0, 3);

        return 0;
}

static int
make_line_data(
        byte	*curr_data,
        byte	*last_data,
        int	line_size,
        byte	*buf)
{
        int	i;	/* raster data index */
        int	n;	/* output data index */
        int	bytes;	/* num of bitimage bytes in one block */
        int	offs;

#define TOP_BYTE(bytes,offs)	((((bytes - 1) & 7) << 5) | (offs & 0x1f))

        i = 0;
        n = 0;

        while (i < line_size) {
                /* skip unchanged bytes */
                offs = 0;
                while (i < line_size && curr_data[i] == last_data[i]) {
                        i++;
                        offs++;
                }
                if (i >= line_size)
                        break;

                /* count changed bytes (max 8) */
                bytes = 1;
                while (i + bytes < line_size && bytes < 8 &&
                                curr_data[i + bytes] != last_data[i + bytes]) {
                        bytes++;
                }

                /* make data */
                if (offs < 0x1f) {
                        buf[n++] = TOP_BYTE(bytes, offs);
                }
                else {
                        offs -= 0x1f;
                        buf[n++] = TOP_BYTE(bytes, 0x1f);

                        while (offs >= 0xff) {
                                offs -= 0xff;
                                buf[n++] = 0xff;
                        }
                        buf[n++] = offs;
                }

                /* write bitimage */
                while (bytes > 0) {
                        buf[n++] = curr_data[i++];
                        --bytes;
                }
        }

        return n;
}

static int
send_line(
        byte    *buf,
        int      cnt,
        gp_file *fp)
{
        gp_fprintf(fp, "%c%c%c", ESC, 0x7c, 0xa7);
        gp_fprintf(fp, "%c%c", (cnt >> 8) & 0xff, cnt & 0xff);
        return gp_fwrite(buf, sizeof(byte), cnt, fp);
}

static int
ml_init(
        gx_device_printer *pdev,
        gp_file           *fp)
{
        /* dprintf("gdevml6: ml_init called\n"); */

        gp_fprintf(fp, "%c%c%c", ESC, 0x2f, 0xf2);

        return 0;
}

static int
page_header(
        gx_device_printer *pdev,
        gp_file           *fp)
{
        int	ydpi;

        gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa0, 1, 0, 1);
        gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa1, 1, 0, 1);
        gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa2, 1, 0, 1);
        if (pdev->y_pixels_per_inch > 600) {	/* 600 x 1200 dpi */
                gp_fprintf(fp, "%c%c%c%c%c%c%c%c%c",
                                ESC, 0x7c, 0xa5, 4, 0, 2, 0x58, 4, 0xb0);
                ydpi = 1200;
        }
        else if (pdev->y_pixels_per_inch > 300) {	/* 600 dpi */
                gp_fprintf(fp, "%c%c%c%c%c%c%c", ESC, 0x7c, 0xa5, 2, 0, 2, 0x58);
                ydpi = 600;
        }
        else {
                gp_fprintf(fp, "%c%c%c%c%c%c%c", ESC, 0x7c, 0xa5, 2, 0, 1, 0x2c);
                ydpi = 300;
        }
        gp_fprintf(fp, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                   ESC, 0x7c, 0xf1, 0x0c, 0, 0, 1 , 0, 2, 0, 0, 0, 0, 0, 0, 0, 0);
        gp_fprintf(fp, "%c%c%c%c%c%c", ESC, 0x7c, 0xa6, 1, 0, 3);

        return ydpi;
}

static int
ml_finish(
        gx_device_printer *pdev,
        gp_file           *fp)
{
        gp_fprintf(fp, "%c%c%c", ESC, 0x2f, 0xfe);

        return 0;
}
