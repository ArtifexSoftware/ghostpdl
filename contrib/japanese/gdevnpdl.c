/* Copyright (C) 1989, 1992, 1993 Aladdin Enterprises.  All rights reserved.

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

/* gdevnpdl.c */
/*
 * NEC NPDL/NPDL(level 2) language printer driver for Ghostscript.
 *   Version 1.5
 *
 * This driver was designed to work with any NEC NPDL/NPDL(level 2)
 * language printers such as NEC MultiWriter series.
 *
 * Any test reports, comments, suggestions, and bug reports are welcome.
 *
 *  -- Osamu Watanabe <owatanab@ceres.dti.ne.jp>
 *
 * $Id: gdevnpdl.c,v 1.10 1998/09/28 16:20:04 watanabe Exp $
 *
 */

/*
   Modified by Norihito Ohmori, May 15 1999
   1. Landscape Support
   2. PostCard Size Support
   3. Output Speed UP
   4. Copy Mode Support (-dNumCopies Option)
   5. Default Resolution Chage from 400 dpi to 240 dpi
   6. NegativePrint Support
 */

/*
   Modified by Norihito Ohmori, Jul 18 1999
   1. Envelope 4 and BPostCard Size Support
   2. Tumble (When Duplex) Support
   3. Margin size change.
 */

#include "gdevlprn.h"

/*
 * Valid values for IMAGE resolutions, that is, X_DPI and Y_DPI, are
 * 160, 200, 240, and 400 (Mr. Keita Kawabe reported that the 600dpi
 * printer, MultiWriter 2000X, accepts 600).
 * These two values must be the same.
 *
 * The resolution specified below is the default value.
 * You can also specify the resolution at runtime with -r option.
 */
#define X_DPI 240
#define Y_DPI 240

/*
 * Left, bottom, right, and top margins.
 *
 * The values defined below are more accurate for my printer,
 * and it is recommended that you adjust these values suitable to yours.
 * The commented values are from the PC-PR1000E/4 User's Manual.
 */
/* margins of A3 paper */
#define L_MARGIN_A3 0.20f
#define B_MARGIN_A3 0.24f
#define R_MARGIN_A3 0.20f
#define T_MARGIN_A3 0.20f
/* margins of A4 paper */
#define L_MARGIN_A4 0.31f
#define B_MARGIN_A4 0.20f
#define R_MARGIN_A4 0.16f
#define T_MARGIN_A4 0.20f
/* margins of A5 paper */
#define L_MARGIN_A5 0.31f
#define B_MARGIN_A5 0.16f
#define R_MARGIN_A5 0.16f
#define T_MARGIN_A5 0.20f
/* margins of B4 paper */
#define L_MARGIN_B4 0.31f
#define B_MARGIN_B4 0.24f
#define R_MARGIN_B4 0.31f
#define T_MARGIN_B4 0.20f
/* margins of B5 paper */
#define L_MARGIN_B5 0.31f
#define B_MARGIN_B5 0.24f
#define R_MARGIN_B5 0.16f
#define T_MARGIN_B5 0.20f
/* margins of letter size paper */
#define L_MARGIN_LETTER 0.31f
#define B_MARGIN_LETTER 0.24f
#define R_MARGIN_LETTER 0.20f
#define T_MARGIN_LETTER 0.20f
/* margins of postcard size paper */
#define L_MARGIN_POSTCARD 0.31f
#define B_MARGIN_POSTCARD 0.12f
#define R_MARGIN_POSTCARD 0.24f
#define T_MARGIN_POSTCARD 0.20f
/* margins of env4 size paper */
#define L_MARGIN_ENV4 0.20f
#define B_MARGIN_ENV4 0.20f
#define R_MARGIN_ENV4 0.20f
#define T_MARGIN_ENV4 0.20f

/* The device descriptors */
static dev_proc_open_device(npdl_open);
static dev_proc_open_device(npdl_close);
static dev_proc_print_page_copies(npdl_print_page_copies);
static dev_proc_put_params(npdl_put_params);
static dev_proc_image_out(npdl_image_out);

static void
npdl_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_mono(dev);

    set_dev_proc(dev, open_device, npdl_open);
    set_dev_proc(dev, close_device, npdl_close);
    set_dev_proc(dev, get_params, lprn_get_params);
    set_dev_proc(dev, put_params, npdl_put_params);
}

gx_device_lprn far_data gs_npdl_device =
lprn_duplex_device(gx_device_lprn, npdl_initialize_device_procs, "npdl",
            X_DPI, Y_DPI,	/* default resolution */
            0.0, 0.0, 0.0, 0.0,	/* margins */
            1, npdl_print_page_copies, npdl_image_out);

/* ------ internal routines ------ */

/* paper size code */
/* modified from gdevpcl.h and gdevmjc.c */
#define PAPER_SIZE_LETTER 2
#define PAPER_SIZE_A5 25
#define PAPER_SIZE_A4 26
#define PAPER_SIZE_A3 27
#define PAPER_SIZE_B4 30
#define PAPER_SIZE_B5 31
#define PAPER_SIZE_POSTCARD 32
#define PAPER_SIZE_BPOSTCARD 33
#define PAPER_SIZE_ENV4 34

/* terminating code */
static char
terminating[2][64][13] = {{
/* white */
"00110101",     "000111",       "0111",         "1000",         /*  0 -  3 */
"1011",         "1100",         "1110",         "1111",         /*  4 -  7 */
"10011",        "10100",        "00111",        "01000",        /*  8 - 11 */
"001000",       "000011",       "110100",       "110101",       /* 12 - 15 */
"101010",       "101011",       "0100111",      "0001100",      /* 16 - 19 */
"0001000",      "0010111",      "0000011",      "0000100",      /* 20 - 23 */
"0101000",      "0101011",      "0010011",      "0100100",      /* 24 - 27 */
"0011000",      "00000010",     "00000011",     "00011010",     /* 28 - 31 */
"00011011",     "00010010",     "00010011",     "00010100",     /* 32 - 35 */
"00010101",     "00010110",     "00010111",     "00101000",     /* 36 - 39 */
"00101001",     "00101010",     "00101011",     "00101100",     /* 40 - 43 */
"00101101",     "00000100",     "00000101",     "00001010",     /* 44 - 47 */
"00001011",     "01010010",     "01010011",     "01010100",     /* 48 - 51 */
"01010101",     "00100100",     "00100101",     "01011000",     /* 52 - 55 */
"01011001",     "01011010",     "01011011",     "01001010",     /* 56 - 59 */
"01001011",     "00110010",     "00110011",     "00110100"      /* 60 - 63 */
},{
/* black */
"0000110111",   "010",          "11",           "10",           /*  0 -  3 */
"011",          "0011",         "0010",         "00011",        /*  4 -  7 */
"000101",       "000100",       "0000100",      "0000101",      /*  8 - 11 */
"0000111",      "00000100",     "00000111",     "000011000",    /* 12 - 15 */
"0000010111",   "0000011000",   "0000001000",   "00001100111",  /* 16 - 19 */
"00001101000",  "00001101100",  "00000110111",  "00000101000",  /* 20 - 23 */
"00000010111",  "00000011000",  "000011001010", "000011001011", /* 24 - 27 */
"000011001100", "000011001101", "000001101000", "000001101001", /* 28 - 31 */
"000001101010", "000001101011", "000011010010", "000011010011", /* 32 - 35 */
"000011010100", "000011010101", "000011010110", "000011010111", /* 36 - 39 */
"000001101100", "000001101101", "000011011010", "000011011011", /* 40 - 43 */
"000001010100", "000001010101", "000001010110", "000001010111", /* 44 - 47 */
"000001100100", "000001100101", "000001010010", "000001010011", /* 48 - 51 */
"000000100100", "000000110111", "000000111000", "000000100111", /* 52 - 55 */
"000000101000", "000001011000", "000001011001", "000000101011", /* 56 - 59 */
"000000101100", "000001011010", "000001100110", "000001100111"  /* 60 - 63 */
}};

/* make-up code */
static char
makeup[2][40][14] = {{
/* white */
"11011",        "10010",        "010111",       "0110111",      /*   64- 256 */
"00110110",     "00110111",     "01100100",     "01100101",     /*  320- 512 */
"01101000",     "01100111",     "011001100",    "011001101",    /*  576- 768 */
"011010010",    "011010011",    "011010100",    "011010101",    /*  832-1024 */
"011010110",    "011010111",    "011011000",    "011011001",    /* 1088-1280 */
"011011010",    "011011011",    "010011000",    "010011001",    /* 1344-1536 */
"010011010",    "011000",       "010011011",    "00000001000",  /* 1600-1792 */
"00000001100",  "00000001101",  "000000010010", "000000010011", /* 1856-2048 */
"000000010100", "000000010101", "000000010110", "000000010111", /* 2112-2304 */
"000000011100", "000000011101", "000000011110", "000000011111"  /* 2368-2560 */
},{
/* black */
"0000001111",   "000011001000", "000011001001", "000001011011", /*   64- 256 */
"000000110011", "000000110100", "000000110101", "0000001101100",/*  320- 512 */
"0000001101101","0000001001010","0000001001011","0000001001100",/*  576- 768 */
"0000001001101","0000001110010","0000001110011","0000001110100",/*  832-1024 */
"0000001110101","0000001110110","0000001110111","0000001010010",/* 1088-1280 */
"0000001010011","0000001010100","0000001010101","0000001011010",/* 1344-1536 */
"0000001011011","0000001100100","0000001100101","00000001000",  /* 1600-1792 */
"00000001100",  "00000001101",  "000000010010", "000000010011", /* 1856-2048 */
"000000010100", "000000010101", "000000010110", "000000010111", /* 2112-2304 */
"000000011100", "000000011101", "000000011110", "000000011111"  /* 2368-2560 */
}};

/* EOL (end of line) code */
static char
     eol[] = "000000000001";

/* FILL bit */
static char
     fill[] = "0";

#define MAX_RUNLENGTH 2623	/* Max value of makeup(2560) + terminating(63) */
#define EOL_SIZE 12		/* Size of EOL (end of line) code */

static byte
        mask[] =
{0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

/* Write MH code to buffer */
static int
mh_write_to_buffer(byte * out, int chunk_size, int num_bits, char *code)
{
    int code_length, i, p, q;

    for (code_length = 0; code[code_length] != '\0'; code_length++);
    if (((num_bits + code_length) / 8) >= chunk_size)
        return 0;

    p = num_bits / 8;
    q = num_bits % 8;
    for (i = 0; i < code_length; i++) {
        /*
         * MH compressed data are stored from 2^0 to 2^7,
         * whereas uncompressed data are stored from 2^7 to 2^0.
         */
        if (code[i] == '0')
            out[p] &= ~(mask[7 - q]);
        else
            out[p] |= mask[7 - q];

        if (q < 7)
            q++;
        else {
            p++;
            q = 0;
        }
    }

    return code_length;
}

/* Set run-length code */
static int
mh_set_runlength(byte * out, int chunk_size, int num_bits, int phase, int count)
{
    int n, code_length;

    code_length = 0;

    /* Set makeup code */
    if (count / 64 > 0) {
        if ((n = mh_write_to_buffer(out, chunk_size, num_bits,
                                    makeup[phase][(count / 64) - 1])) == 0)
            return 0;
        code_length += n;
    }
    /* Set terminating code */
    if ((n = mh_write_to_buffer(out, chunk_size, num_bits + code_length,
                                terminating[phase][count % 64])) == 0)
        return 0;
    code_length += n;

    return code_length;
}

/* Set EOL (end of line) code */
static int
mh_set_eol(byte * out, int chunk_size, int num_bits)
{
    return mh_write_to_buffer(out, chunk_size, num_bits, eol);
}

/* Set RTC (return to control) code and FILL bits */
static int
mh_set_rtc(byte * out, int chunk_size, int num_bits)
{
    int i, n, code_length, num_fills;

    code_length = 0;

    /* Set FILL bits */
    num_fills = (EOL_SIZE * 6 + (num_bits % 8)) % 8;
    if (num_fills != 0) {
        for (i = 0; i < 8 - num_fills; i++) {
            if ((n = mh_write_to_buffer(out, chunk_size, num_bits + code_length,
                                        fill)) == 0)
                return 0;
            code_length += n;
        }
    }
    /* Set RTC (that is, six EOL) code */
    for (i = 0; i < 6; i++) {
        if ((n = mh_write_to_buffer(out, chunk_size, num_bits + code_length,
                                    eol)) == 0)
            return 0;
        code_length += n;
    }

    return code_length;
}

/* Data compression by MH coding */
static int
mh_compression(byte * in, byte * out, int line_size, int column_size)
{
    int i, p, q, r, n;
    int num_bits, phase, count;
    int chunk_size = line_size * column_size;
    byte src;

    num_bits = 0;
    for (i = 0; i < column_size; i++) {
        r = i * line_size;

        /* Synchronization of 1-dot line */
        if ((n = mh_set_eol(out, chunk_size, num_bits)) == 0)
            return 0;
        num_bits += n;

        /* Data compression of 1-dot line */
        phase = count = 0;
        for (p = 0; p < line_size; p++) {
            if (phase == 0)
                src = ~(in[r + p]);
            else
                src = in[r + p];
            for (q = 0; q < 8; q++) {

                /*
                 * If the dot has reversed, write the
                 * run-length of the continuous dots.
                 */
                if (!(src & mask[q])) {
                    if ((n = mh_set_runlength(out, chunk_size, num_bits,
                                              phase, count)) == 0)
                        return 0;
                    num_bits += n;
                    phase = (phase == 0) ? 1 : 0;
                    count = 1;
                    src = ~src;
                }
                /*
                 * If the dot is not reversed, check the
                 * length of this continuous dots,
                 */
                else {
                    if (count < MAX_RUNLENGTH)
                        count++;
                    /*
                     * and if the length >= MAX_RUNLENGTH,
                     * stop and restart counting.
                     */
                    else {
                        if ((n = mh_set_runlength(out, chunk_size, num_bits,
                                                phase, MAX_RUNLENGTH)) == 0)
                            return 0;
                        num_bits += n;
                        phase = (phase == 0) ? 1 : 0;
                        if ((n = mh_set_runlength(out, chunk_size, num_bits,
                                                  phase, 0)) == 0)
                            return 0;
                        num_bits += n;
                        phase = (phase == 0) ? 1 : 0;
                        count = 1;
                    }
                }
            }
        }

        /* Write the last run-length of 1-dot line */
        if ((n = mh_set_runlength(out, chunk_size, num_bits, phase,
                                  count)) == 0)
            return 0;
        num_bits += n;
    }

    /* RTC (return to control) */
    if ((n = mh_set_rtc(out, chunk_size, num_bits)) == 0)
        return 0;
    num_bits += n;

    return (num_bits / 8);
}

/* Get the paper size code based on the width and the height.
   modified from gdevpcl.c and gdevmjc.c */
static int
npdl_get_paper_size(gx_device * dev)
{
    float media_height = (dev->MediaSize[0] > dev->MediaSize[1]) ? dev->MediaSize[0] : dev->MediaSize[1];

    return (media_height > 1032 ? PAPER_SIZE_A3 :
            media_height > 842 ? PAPER_SIZE_B4 :
            media_height > 792 ? PAPER_SIZE_A4 :
            media_height > 756 ? PAPER_SIZE_LETTER :
            media_height > 729 ? PAPER_SIZE_ENV4 :
            media_height > 595 ? PAPER_SIZE_BPOSTCARD :
            media_height > 568 ? PAPER_SIZE_B5 :
            media_height > 419 ? PAPER_SIZE_A5 :
            PAPER_SIZE_POSTCARD);
}

static int
npdl_set_page_layout(gx_device * dev)
{
    int code;
    float margins[4];

    /* Change the margins according to the paper size. */
    switch (npdl_get_paper_size(dev)) {
        case PAPER_SIZE_A3:
            if (dev->MediaSize[0] > dev->MediaSize[1]) {	/* Landscape */
                margins[0] = L_MARGIN_A3;
                margins[1] = B_MARGIN_A3;
                margins[2] = R_MARGIN_A3;
                margins[3] = T_MARGIN_A3;
            } else {		/* Portrait */
                margins[1] = L_MARGIN_A3;
                margins[2] = B_MARGIN_A3;
                margins[3] = R_MARGIN_A3;
                margins[0] = T_MARGIN_A3;
            }
            break;
        case PAPER_SIZE_A5:
            if (dev->MediaSize[0] > dev->MediaSize[1]) {	/* Landscape */
                margins[0] = L_MARGIN_A5;
                margins[1] = B_MARGIN_A5;
                margins[2] = R_MARGIN_A5;
                margins[3] = T_MARGIN_A5;
            } else {		/* Portrait */
                margins[1] = L_MARGIN_A5;
                margins[2] = B_MARGIN_A5;
                margins[3] = R_MARGIN_A5;
                margins[0] = T_MARGIN_A5;
            }
            break;
        case PAPER_SIZE_B5:
            if (dev->MediaSize[0] > dev->MediaSize[1]) {	/* Landscape */
                margins[1] = L_MARGIN_B5;
                margins[2] = B_MARGIN_B5;
                margins[3] = R_MARGIN_B5;
                margins[0] = T_MARGIN_B5;
            } else {		/* Portrait */
                margins[0] = L_MARGIN_B5;
                margins[1] = B_MARGIN_B5;
                margins[2] = R_MARGIN_B5;
                margins[3] = T_MARGIN_B5;
            }
            break;
        case PAPER_SIZE_LETTER:
            if (dev->MediaSize[0] > dev->MediaSize[1]) {	/* Landscape */
                margins[1] = L_MARGIN_LETTER;
                margins[2] = B_MARGIN_LETTER;
                margins[3] = R_MARGIN_LETTER;
                margins[0] = T_MARGIN_LETTER;
            } else {		/* Portrait */
                margins[0] = L_MARGIN_LETTER;
                margins[1] = B_MARGIN_LETTER;
                margins[2] = R_MARGIN_LETTER;
                margins[3] = T_MARGIN_LETTER;
            }
            break;
        case PAPER_SIZE_POSTCARD:
            if (dev->MediaSize[0] > dev->MediaSize[1]) {	/* Landscape */
                margins[1] = L_MARGIN_POSTCARD;
                margins[2] = B_MARGIN_POSTCARD;
                margins[3] = R_MARGIN_POSTCARD;
                margins[0] = T_MARGIN_POSTCARD;
            } else {		/* Portrait */
                margins[0] = L_MARGIN_POSTCARD;
                margins[1] = B_MARGIN_POSTCARD;
                margins[2] = R_MARGIN_POSTCARD;
                margins[3] = T_MARGIN_POSTCARD;
            }
            break;
        case PAPER_SIZE_ENV4:
        case PAPER_SIZE_BPOSTCARD:
                margins[1] = L_MARGIN_ENV4;
                margins[2] = B_MARGIN_ENV4;
                margins[3] = R_MARGIN_ENV4;
                margins[0] = T_MARGIN_ENV4;
            break;
        default:		/* A4 */
            if (dev->MediaSize[0] > dev->MediaSize[1]) {	/* Landscape */
                margins[1] = L_MARGIN_A4;
                margins[2] = B_MARGIN_A4;
                margins[3] = R_MARGIN_A4;
                margins[0] = T_MARGIN_A4;
            } else {		/* Portrait */
                margins[0] = L_MARGIN_A4;
                margins[1] = B_MARGIN_A4;
                margins[2] = R_MARGIN_A4;
                margins[3] = T_MARGIN_A4;
            }
            break;
    }
    gx_device_set_margins(dev, margins, true);
    if (dev->is_open) {
        gdev_prn_close(dev);
        code = gdev_prn_open(dev);
        if (code < 0)
            return code;
    }
    return 0;
}

/* Open the printer, and set the margins. */
static int
npdl_open(gx_device * dev)
{
    int xdpi = (int)dev->x_pixels_per_inch;
    int ydpi = (int)dev->y_pixels_per_inch;

    /* Print Resolution Check */
    if (xdpi != ydpi)
        return_error(gs_error_rangecheck);
    else if (xdpi != 160 && xdpi != 200 && xdpi != 240 &&
             xdpi != 400 && xdpi != 600)
        return_error(gs_error_rangecheck);

    npdl_set_page_layout(dev);

    return gdev_prn_open(dev);
}

static int
npdl_close(gx_device *pdev)
{
    gx_device_printer *const ppdev = (gx_device_printer *) pdev;
    int code = gdev_prn_open_printer(pdev, 1);
    if (code >= 0)
        gp_fputs("\033c1", ppdev->file);

    return gdev_prn_close(pdev);
}

static int
npdl_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int code;

    code = lprn_put_params(pdev, plist);
    if (code < 0)
        return code;
    if (pdev->is_open && !lprn->initialized) {
        npdl_set_page_layout(pdev);
    }
    return 0;
}

/* Send the page to the printer.  For speed, compress each scan line,
   since computer-to-printer communication time is often a bottleneck. */
static int
npdl_print_page_copies(gx_device_printer * pdev, gp_file * prn_stream, int num_copies)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int line_size = gdev_prn_raster(pdev);
    int x_dpi = (int)(pdev->x_pixels_per_inch);
    char paper_command[5];
    int code;
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    if (!(lprn->CompBuf = gs_malloc(pdev->memory->non_gc_memory, line_size, maxY, "npdl_print_page_copies(CompBuf)")))
        return_error(gs_error_VMerror);

        /* Initialize printer */
    if (pdev->PageCount == 0) {

      /* Initialize printer */
      gp_fputs("\033c1", prn_stream);               /* Software Reset */
      gp_fputs("\034d240.", prn_stream);            /* Page Printer Mode */

        /* Check paper size */
        switch (npdl_get_paper_size((gx_device *) pdev)) {
            case PAPER_SIZE_POSTCARD:
                gs_snprintf(paper_command, sizeof(paper_command), "PC");
                break;
            case PAPER_SIZE_A5:
                gs_snprintf(paper_command, sizeof(paper_command), "A5");
                break;
            default:
            case PAPER_SIZE_A4:
                gs_snprintf(paper_command, sizeof(paper_command), "A4");
                break;
            case PAPER_SIZE_A3:
                gs_snprintf(paper_command, sizeof(paper_command), "A3");
                break;
            case PAPER_SIZE_B5:
                gs_snprintf(paper_command, sizeof(paper_command), "B5");
                break;
            case PAPER_SIZE_B4:
                gs_snprintf(paper_command, sizeof(paper_command), "B4");
                break;
            case PAPER_SIZE_LETTER:
                gs_snprintf(paper_command, sizeof(paper_command), "LT");
                break;
            case PAPER_SIZE_ENV4:
                gs_snprintf(paper_command, sizeof(paper_command), "ENV4");
                break;
            case PAPER_SIZE_BPOSTCARD:
                gs_snprintf(paper_command, sizeof(paper_command), "UPPC");
                break;
        }

        if (lprn->ManualFeed) {
        gp_fprintf(prn_stream, "\034f%cM0.",
                   (pdev->MediaSize[0] > pdev->MediaSize[1]) ? 'L' : 'P');
        /* Page Orientation  P: Portrait, L: Landscape */
        } else {
        gp_fprintf(prn_stream, "\034f%c%s.",
                   (pdev->MediaSize[0] > pdev->MediaSize[1]) ? 'L' : 'P',
        /* Page Orientation  P: Portrait, L: Landscape */
                   paper_command);	/* Paper Size */
        }

        gp_fprintf(prn_stream, "\034<1/%d,i.", x_dpi);	/* Image Resolution */

        /* Duplex Setting */
        if (pdev->Duplex_set > 0) {
            if (pdev->Duplex) {
                if (lprn->Tumble == 0)
                  gp_fprintf(prn_stream, "\034'B,,1,0.");
                else
                  gp_fprintf(prn_stream, "\034'B,,2,0.");
            } else
              gp_fprintf(prn_stream, "\034'S,,,0.");
        }
    }

    if (num_copies > 99)
       num_copies = 99;
    gp_fprintf(prn_stream, "\034x%d.", num_copies);

    lprn->initialized = false;

    if (lprn->NegativePrint) {
        gp_fprintf(prn_stream, "\034e0,0.");	/* move to (0, 0) */
        gp_fprintf(prn_stream, "\034Y");	/* goto figure mode */
        gp_fprintf(prn_stream, "SU1,%d,0;", (int)pdev->x_pixels_per_inch);
        /* Setting Printer Unit */
        gp_fprintf(prn_stream, "SG0,0;");	/* select black color */
        gp_fprintf(prn_stream, "NP;");	/* begin path */
        gp_fprintf(prn_stream, "PA%d,0,%d,%d,0,%d;",
                   pdev->width, pdev->width, pdev->height, pdev->height);
        /* draw rectangle */
        gp_fprintf(prn_stream, "CP");	/* close path */
        gp_fprintf(prn_stream, "EP;");	/* end path */
        gp_fprintf(prn_stream, "FL0;");	/* fill path */
        gp_fprintf(prn_stream, "\034Z");	/* end of figure mode */
        gp_fprintf(prn_stream, "\034\"R.");	/* `R'eplace Mode */
    }
    code = lprn_print_image(pdev, prn_stream);
    if (code < 0)
        return code;

    /* Form Feed */
    gp_fputs("\014", prn_stream);

    gs_free(pdev->memory->non_gc_memory, lprn->CompBuf, line_size, maxY, "npdl_print_page_copies(CompBuf)");
    return 0;
}

/* Output data */
static void
npdl_image_out(gx_device_printer * pdev, gp_file * prn_stream, int x, int y, int width, int height)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int num_bytes;
    int x_dpi = (int)(pdev->x_pixels_per_inch);

    gp_fprintf(prn_stream, "\034e%d,%d.", x, y);
    /* Data compression */
    num_bytes = mh_compression(lprn->TmpBuf, lprn->CompBuf, width / 8, height);

    /*
     * If the compression ratio >= 100%, send uncompressed data
     */
    if (num_bytes == 0) {
        gp_fprintf(prn_stream, "\034i%d,%d,0,1/1,1/1,%d,%d.", width,
                   height, width * height / 8, x_dpi);
        gp_fwrite(lprn->TmpBuf, 1, width * height / 8, prn_stream);
    }
    /*
     * If the compression ratio < 100%, send compressed data
     */
    else {
        gp_fprintf(prn_stream, "\034i%d,%d,1,1/1,1/1,%d,%d.", width,
                   height, num_bytes, x_dpi);
        gp_fwrite(lprn->CompBuf, 1, num_bytes, prn_stream);
    }
}
