/*
   Copyright (C) 2000 Hewlett-Packard Company
   Portions Copyright (C) 1996-1998  <Uli Wortmann uliw@erdw.ethz.ch>.
   Portions Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to:

   Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor
   Boston, MA 02110-1301
   USA

   This program may also be distributed as part of Aladdin Ghostscript,
   under the terms of the Aladdin Free Public License (the "License").

   Every copy of Aladdin Ghostscript must include a copy of the
   License, normally in a plain ASCII text file named PUBLIC.  The
   License grants you the right to copy, modify and redistribute
   Aladdin Ghostscript, but only under certain conditions described in
   the License.  Among other things, the License requires that the
   copyright notice and this notice be preserved on all copies.
 */

/* 	$Id: gdevcd8.c,v 1.5 2002/07/30 18:53:21 easysw Exp $	 */

/*----------------------------------------------------------------

   A printer driver for the HP670, HP690, HP850, HP855
   HP870, HP890, HP1100, HP1600 and HP2200 color printers.
   Also work with HP DesignJet 500 large-format color printer.
   To be used with the Ghostscript printing system.

   CREDITS: Much of the driver is based on ideas derived
            from the cdj550 driver of George Cameron.

            The support for the hp670, hp690, hp890
            and hp1600 was added by Martin Gerbershagen.

            The support for the hp2200 was added by Siow-Kiat Tan.

            The support for the dnj500 was added by Timur Maximov.

-------------------------------------------------------------------*/

/* Note: Depending on how you transfered the files,
   you might need to remove some CR-codes used on intel-based machines:

   simply type:  unzip -a hp850.zip

   to compile with gs5.x, simply add

   DEVICE_DEVS4=cdj850.dev cdj670.dev cdj890.dev cdj1600.dev

   to your makefile.

   BTW, it is always a good idea to read Make.htm found in the
   gs-distrib before attempting to recompile.....

 */

/* 1999-01-07 edited by L. Peter Deutsch <ghost@aladdin.com> to eliminate
   non-const statics and otherwise bring up to date with Ghostscript coding
   style guidelines. */

/* 01.06.98   Version 1.3  Due to the most welcome contribution
   of Martin Gerbershagen (ger@ulm.temic.de),
   support for the hp670, hp690 and hp890
   and hp1600 has been added. Martin has also
   resolved all known bugs.

   Problems  :  Dark colors are still pale.

   The driver no longer needs special switches to be invoked
   except -sDEVICE=cdj850, or -sDEVICE=CDJ890, or sDEVICE=CDJ670
   or -sDEVICE=CDJ1600

   The hp690 is supported through the hp670 device, the hp855, hp870
   and the hp1100 through the hp850 device.

   The driver implements the following switches:

   -dPapertype= 0  plain paper [default]
   1  bond paper
   2  special paper
   3  glossy film
   4  transparency film

   Note, currently the lookuptables are not suited
   for printing on special paper or transperencies.
   Please revert to the gamma functions in this case.

   -dQuality=  -1 draft
   0 normal       [default]
   1 presentation

   -dRetStatus= 0 C-RET off
   1 C-RET on [default]

   -dMasterGamma= 3.0 [default = 1.0]
   __Note__: To take advantage of the calibrated color-transfer
   functions, be sure not to have any Gamma-Statements
   left! If you need to (i.e. overhead sheets),
   you still can use the gamma-functions, but they will
   override the built-in calibration. To use gamma in the
   traditional way, set MasterGamma to any value greater
   1.0 and less 10.0. To adjust individual gamma-values,
   you have to additionally set MasterGamma to a value
   greater 1.0 and less 10.0

   With the next release, gamma functions will be dropped.

   When using the driver, be aware that printing in 600dpi involves
   processing of large amounts of data (> 188MB !). Therefore, the
   driver is not what you would expect to be a fast driver ;-)
   This is no problem when printing a full sized color page (because
   printing itself is slow), but it's really annoying if yoy print only
   text pages. Maybe I can optimize the code for text-only pages in a
   later release. Right now, it is recommended to use the highest
   possible optimisation level your compiler offers....
   For the time beeing, use the cdj550 device with -sBitsPerPixel=3
   for fast proof-prints. If you simply want to print 600dpi b/w data,
   use the cdj550 device with -sBitsPerPixel=8 (or 1).

   Since the printer itself is slow, it may help to set the
   process-priority of the gs-process to regular or even less. On a
   486/100MHZ this is still sufficient to maintain a continuos
   data-flow.
   Note to OS/2 users: Simply put the gs-window into the background,
   or minimize it. Also make sure, that print01.sys is invoked without
   the /irq switch (great speed improvement under warp4).

   The printer default settings compensate for dot-gain by a
   calibrated color-transfer function. If this appears to be to light
   for your business-graphs, or for overhead-sheets, feel free to set
   -dMasterGamma=1.7.

   Furthermore, you may tweak the gammavalues independently by setting
   -dGammaValC, -dGammaValM, -dGammaValY or -dGammaValK (if not set,
   the values default to MasterGamma). This will only work, when
   -dMasterGamma is set to a value greater than 1.0.

   If you want to learn more about gamma, see:

   http://www.erdw.ethz.ch/~bonk/ftp/misc/gammafaq.pdf
   http://www.erdw.ethz.ch/~bonk/ftp/misc/colorfaq.pdf

   Further information, bugs, tips etc, can be found
   at my website.

   Have fun!

   Uli

   http://www.erdw.ethz.ch/~bonk/bonk.html

 */

/* 25.08.97  Version 1.2. Resolved all but one of the
   known bugs, introduced a couple
   of perfomance improvements. Complete
   new color-transfer-function handling.
   (see gamma). */

/* 04.05.97  Version 1.1. For added features, */
/* resolved bugs and so forth, please see   */
/* http://bonk.ethz.ch                      */

/* 11.11.96. Initial release of the driver */

#include "math_.h"
#include <stdlib.h>		/* for rand() */
#include "assert_.h"
#include "gdevprn.h"
#include "gdevpcl.h"
#include "gsparam.h"

#include "gdebug.h"
#include <string.h>

/* Conversion stuff. */
#include "gxlum.h"

#define near

/*  This holds the initialisation data of the hp850.  These are sent to
 *  the printer with the Config Raster command (Esc * g # W ...) during
 *  printer initialization.  See the functions cdj850_start_raster_mode(),
 *  cdj880_start_raster_mode(), and cdj1600_start_raster_mode().
 */
typedef struct hp850_cmyk_init_s {
    byte a[26];
} hp850_cmyk_init_t;
static const hp850_cmyk_init_t hp850_cmyk_init =
{
    {
        0x02,			/* format */
        0x04,			/* number of components */
      /* black */
        0x01,			/* MSB x resolution */
        0x2c,			/* LSB x resolution */
        0x01,			/* MSB y resolution */
        0x2c,			/* LSB y resolution */
        0x00,			/* MSB intensity levels */
        0x02,			/* LSB intensity levels */

      /* cyan */
        0x01,			/* MSB x resolution */
        0x2c,			/* LSB x resolution */
        0x01,			/* MSB y resolution */
        0x2c,			/* LSB y resolution */
        0x00,			/* MSB intensity levels */
        0x02,			/* LSB intensity levels */

      /* magenta */
        0x01,			/* MSB x resolution */
        0x2c,			/* LSB x resolution */
        0x01,			/* MSB y resolution */
        0x2c,			/* LSB y resolution */
        0x00,			/* MSB intensity levels */
        0x02,			/* LSB intensity levels */

      /* yellow */
        0x01,			/* MSB x resolution */
        0x2c,			/* LSB x resolution */
        0x01,			/* MSB y resolution */
        0x2c,			/* LSB y resolution */
        0x00,			/* MSB intensity levels */
        0x02			/* LSB intensity levels */
    }
};

/* this holds the color lookuptable data of the hp850 */
typedef struct {
    byte c[256];		/* Lookuptable for cyan */
    byte m[256];		/* dito for magenta */
    byte y[256];		/* dito for yellow */
    byte k[256];		/* dito for black  */
    int correct[256];		/* potential undercolor black correction */
} Gamma;

static const Gamma gammat850 =
{
  /* Lookup values for cyan */
    {0, 0, 0, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 6, 7, 7, 6, 7, 7, 7, 8, 8,
     8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 11, 11, 12, 12, 12,
     12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 16, 16, 15, 16, 16, 17, 17,
     17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 20, 20, 20, 21, 21, 21, 22,
     22, 23, 23, 23, 23, 23, 24, 24, 25, 25, 26, 26, 26, 26, 26, 27, 27,
     27, 27, 28, 28, 29, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 34,
     35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 41, 41, 42, 42, 42,
     43, 43, 43, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 50, 50, 51, 51,
     52, 52, 53, 54, 54, 54, 55, 55, 56, 57, 58, 58, 59, 60, 60, 61, 62,
     62, 63, 65, 65, 66, 67, 67, 68, 69, 69, 70, 72, 73, 73, 74, 75, 75,
     76, 77, 79, 79, 80, 81, 82, 83, 83, 84, 86, 87, 88, 88, 89, 90, 91,
     92, 93, 94, 95, 96, 97, 97, 99, 100, 101, 102, 103, 104, 105, 106,
     108, 109, 110, 111, 112, 114, 115, 117, 119, 120, 122, 124, 125, 127,
     129, 131, 132, 135, 136, 138, 140, 142, 144, 146, 147, 150, 152, 154,
     157, 159, 162, 164, 166, 168, 171, 174, 176, 180, 182, 187, 192, 197,
     204, 215, 255},
  /* Lookup values for magenta */
    {0, 0, 0, 1, 1, 1, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 7, 7, 7,
     7, 8, 8, 8, 9, 9, 10, 10, 9, 10, 10, 10, 11, 11, 11, 11, 11, 12, 12,
     12, 13, 13, 13, 14, 14, 15, 15, 15, 16, 16, 16, 16, 16, 17, 17, 17,
     17, 17, 18, 18, 19, 19, 19, 19, 19, 20, 20, 20, 21, 21, 22, 22, 22,
     23, 23, 24, 24, 25, 25, 25, 26, 26, 27, 27, 28, 29, 29, 29, 29, 30,
     30, 31, 30, 31, 31, 32, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36,
     36, 37, 37, 38, 38, 39, 39, 40, 40, 41, 41, 42, 42, 43, 43, 44, 44,
     45, 45, 46, 46, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 53, 53, 54,
     54, 55, 55, 56, 57, 57, 58, 58, 59, 60, 60, 61, 61, 62, 63, 64, 65,
     66, 66, 67, 68, 68, 70, 71, 71, 72, 73, 73, 74, 76, 77, 77, 78, 79,
     79, 80, 81, 82, 83, 84, 85, 86, 87, 87, 88, 89, 90, 91, 91, 92, 93,
     94, 95, 96, 97, 98, 99, 100, 100, 101, 102, 103, 105, 106, 107, 108,
     109, 112, 113, 114, 115, 116, 118, 119, 121, 123, 124, 125, 128, 129,
     130, 133, 134, 135, 138, 139, 142, 144, 145, 148, 150, 152, 154, 157,
     159, 162, 164, 168, 169, 170, 172, 175, 177, 179, 182, 185, 189, 193,
     198, 204, 215, 255},
  /* Lookup values for yellow */
    {0, 0, 0, 2, 2, 2, 3, 3, 3, 5, 5, 5, 7, 7, 6, 7, 7, 6, 7, 7, 7, 8, 8,
     8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11,
     12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 16, 16, 16, 17, 17, 18, 18,
     18, 19, 18, 19, 19, 19, 20, 20, 21, 21, 21, 22, 22, 22, 22, 22, 23,
     23, 24, 24, 25, 25, 25, 26, 27, 28, 28, 29, 29, 29, 30, 30, 30, 30,
     31, 31, 32, 32, 33, 33, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37,
     38, 38, 39, 39, 40, 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 45, 45,
     45, 46, 46, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 53, 53, 54, 54,
     55, 55, 56, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 64, 65, 66, 67,
     67, 68, 69, 69, 70, 71, 72, 73, 74, 74, 75, 76, 77, 77, 78, 79, 79,
     80, 81, 82, 83, 84, 85, 86, 87, 87, 88, 89, 90, 91, 91, 93, 94, 95,
     96, 97, 98, 100, 101, 102, 102, 103, 104, 106, 107, 108, 109, 110,
     111, 113, 114, 115, 116, 117, 118, 119, 121, 123, 124, 126, 128, 130,
     131, 134, 135, 137, 139, 140, 143, 145, 146, 148, 150, 152, 154, 156,
     158, 160, 163, 166, 167, 169, 171, 173, 176, 178, 181, 184, 188, 192,
     198, 204, 215, 255},
  /* Lookup values for black */
    {0, 0, 0, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 2, 4, 3, 3, 3, 3, 3, 4, 4,
     4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 9, 9, 8,
     8, 8, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13,
     12, 12, 12, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15, 16, 16,
     16, 17, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 20, 20, 20, 21, 21,
     21, 21, 22, 22, 22, 22, 23, 22, 23, 23, 24, 24, 24, 24, 25, 25, 26,
     26, 26, 26, 27, 27, 28, 28, 28, 28, 29, 29, 30, 30, 31, 31, 31, 32,
     32, 33, 33, 34, 34, 35, 36, 36, 36, 37, 37, 37, 38, 38, 40, 40, 40,
     41, 41, 42, 43, 43, 43, 43, 44, 45, 45, 46, 47, 47, 48, 49, 49, 50,
     52, 52, 53, 54, 54, 56, 56, 57, 58, 59, 60, 60, 61, 62, 63, 63, 64,
     65, 66, 67, 68, 69, 70, 71, 72, 72, 73, 75, 75, 76, 77, 78, 80, 81,
     82, 82, 83, 84, 85, 86, 88, 89, 90, 91, 94, 95, 96, 98, 99, 100, 101,
     103, 105, 106, 107, 110, 111, 112, 115, 116, 118, 120, 121, 124, 126,
     127, 131, 133, 134, 138, 140, 141, 146, 148, 151, 154, 156, 160, 163,
     166, 169, 174, 177, 182, 187, 194, 203, 215, 255}
};

static const Gamma gammat890 =
{
/* Lookup values for cyan */
{0, 2, 3, 5, 7, 8, 10, 12, 13, 15, 16, 18, 20, 21, 23, 25, 26, 28, 29,
31, 32, 34, 36, 37, 39, 40, 42, 43, 45, 46, 48, 50, 51, 53, 54, 56, 57,
59, 60, 62, 63, 65, 66, 68, 69, 70, 72, 73, 75, 76, 78, 79, 81, 82, 83,
85, 86, 88, 89, 91, 92, 93, 95, 96, 97, 99, 100, 102, 103, 104, 106,
107, 108, 110, 111, 112, 114, 115, 116, 118, 119, 120, 121, 123, 124,
125, 127, 128, 129, 130, 132, 133, 134, 135, 137, 138, 139, 140, 141,
143, 144, 145, 146, 147, 149, 150, 151, 152, 153, 154, 155, 157, 158,
159, 160, 161, 162, 163, 164, 166, 167, 168, 169, 170, 171, 172, 173,
174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201,
201, 202, 203, 204, 205, 206, 207, 208, 208, 209, 210, 211, 212, 213,
213, 214, 215, 216, 217, 217, 218, 219, 220, 220, 221, 222, 223, 223,
224, 225, 225, 226, 227, 228, 228, 229, 230, 230, 231, 231, 232, 233,
233, 234, 235, 235, 236, 236, 237, 238, 238, 239, 239, 240, 240, 241,
241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247,
248, 248, 249, 249, 249, 250, 250, 250, 251, 251, 251, 252, 252, 252,
252, 253, 253, 253, 253, 254, 254, 254, 254, 254, 255, 255, 255, 255,
255, 255, 255},

/* Lookup values for magenta */ /* gamma 0.6 */ /* 0.8 */

{0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
21, 22, 23, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 52, 53, 54, 55, 56, 57, 58,
59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 86, 87, 88, 89, 90, 91, 92, 93,
94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108,
109, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
122, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 132, 133,
134, 135, 136, 137, 138, 139, 140, 140, 141, 142, 143, 144, 145, 146,
147, 147, 148, 149, 150, 151, 152, 153, 153, 154, 155, 156, 157, 158,
158, 159, 160, 161, 162, 163, 163, 164, 165, 166, 167, 168, 168, 169,
170, 171, 172, 172, 173, 174, 175, 176, 176, 177, 178, 179, 179, 180,
181, 182, 182, 183, 184, 185, 185, 186, 187, 188, 188, 189, 190, 191,
191, 192, 193, 193, 194, 195, 196, 196, 197, 198, 198, 199, 200, 200,
201, 202, 202, 203, 204, 204, 205, 205, 206, 207, 207, 208, 209, 209,
210, 210, 211, 211, 212, 213, 213, 214, 214, 215, 215, 216, 216, 217,
217, 218, 218, 218, 219, 219, 219, 220, 220},

/* Lookup values for yellow */
/* gamma 0.7 */

{0, 1, 3, 4, 6, 7, 9, 10, 11, 13, 14, 16, 17, 18, 20, 21, 23, 24, 25,
27, 28, 29, 31, 32, 34, 35, 36, 38, 39, 40, 42, 43, 44, 46, 47, 48, 50,
51, 52, 54, 55, 56, 58, 59, 60, 62, 63, 64, 66, 67, 68, 70, 71, 72, 73,
75, 76, 77, 79, 80, 81, 82, 84, 85, 86, 88, 89, 90, 91, 93, 94, 95, 96,
97, 99, 100, 101, 102, 104, 105, 106, 107, 109, 110, 111, 112, 113, 115,
116, 117, 118, 119, 120, 122, 123, 124, 125, 126, 127, 129, 130, 131,
132, 133, 134, 136, 137, 138, 139, 140, 141, 142, 143, 145, 146, 147,
148, 149, 150, 151, 152, 153, 154, 155, 157, 158, 159, 160, 161, 162,
163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
191, 192, 193, 194, 195, 196, 196, 197, 198, 199, 200, 201, 202, 203,
204, 205, 205, 206, 207, 208, 209, 210, 211, 211, 212, 213, 214, 215,
216, 216, 217, 218, 219, 220, 220, 221, 222, 223, 223, 224, 225, 226,
226, 227, 228, 229, 229, 230, 231, 232, 232, 233, 234, 234, 235, 236,
236, 237, 238, 238, 239, 239, 240, 241, 241, 242, 242, 243, 244, 244,
245, 245, 246, 246, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251,
251, 252, 252, 253, 253, 253, 254, 254, 254, 254, 255, 255, 255, 255},

/* Lookup values for black */
/* gamma 3.3 */

{0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 7,
8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14,
14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 20,
21, 21, 21, 22, 22, 22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 26, 27, 27,
27, 28, 28, 29, 29, 29, 30, 30, 31, 31, 32, 32, 32, 33, 33, 34, 34, 34,
35, 35, 36, 36, 37, 37, 37, 38, 38, 39, 39, 40, 40, 41, 41, 41, 42, 42,
43, 43, 44, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50, 50, 51, 51,
52, 52, 53, 53, 54, 54, 55, 55, 56, 56, 57, 57, 58, 58, 59, 60, 60, 61,
61, 62, 62, 63, 64, 64, 65, 65, 66, 67, 67, 68, 68, 69, 70, 70, 71, 72,
72, 73, 74, 74, 75, 76, 76, 77, 78, 78, 79, 80, 80, 81, 82, 83, 83, 84,
85, 86, 86, 87, 88, 89, 90, 91, 91, 92, 93, 94, 95, 96, 97, 97, 98, 99,
100, 101, 102, 103, 104, 105, 106, 107, 108, 110, 111, 112, 113, 114,
115, 117, 118, 119, 120, 122, 123, 124, 126, 127, 129, 130, 132, 134,
135, 137, 139, 141, 143, 145, 147, 149, 152, 154, 157, 159, 162, 166,
169, 173, 178, 183, 189, 196, 207, 255},
};

static const Gamma * const gammat[] =
{
    &gammat850,			/* CDJ670 */
    &gammat850,			/* CDJ850 */
    &gammat850,     /* CDJ880 */
    &gammat890,			/* CDJ890 */
    &gammat850			/* CDJ1600 */
};

static int
    rescale_byte_wise1x1(int bytecount, const byte * inbytea,
                         const byte * inbyteb, byte * outbyte);
static int
    rescale_byte_wise2x1(int bytecount, const byte * inbytea,
                         const byte * inbyteb, byte * outbyte);
static int
    rescale_byte_wise1x2(int bytecount, const byte * inbytea,
                         const byte * inbyteb, byte * outbyte);
static int
    rescale_byte_wise2x2(int bytecount, const byte * inbytea,
                         const byte * inbyteb, byte * outbyte);

static int (* const rescale_color_plane[2][2]) (int, const byte *, const byte *, byte *) = {
    {
        rescale_byte_wise1x1, rescale_byte_wise1x2
    },
    {
        rescale_byte_wise2x1, rescale_byte_wise2x2
    }
};

/*
 * Drivers stuff.
 *
 */
#define DESKJET_PRINT_LIMIT  0.04f	/* 'real' top margin? */
/* Margins are left, bottom, right, top. */
#define DESKJET_MARGINS_LETTER   0.25f, 0.50f, 0.25f, 0.167f
#define DESKJET_MARGINS_A4       0.13f, 0.46f, 0.13f, 0.04f
/* Define bits-per-pixel - default is 32-bit cmyk-mode */
#ifndef BITSPERPIXEL
#  define BITSPERPIXEL 32
#endif
#define DOFFSET (dev_t_margin(pdev) - DESKJET_PRINT_LIMIT)	/* Print position */

#define W sizeof(word)
#define I sizeof(int)

/* paper types */
typedef enum {
    PLAIN_PAPER, BOND_PAPER, SPECIAL_PAPER, GLOSSY_FILM, TRANSPARENCY_FILM
} cdj_paper_type_t;

/* quality */
typedef enum {
    DRAFT = -1, NORMAL = 0, PRESENTATION = 1
} cdj_quality_t;

/* Printer types */
typedef enum {
    DJ670C, DJ850C, DJ880C, DJ890C, DJ1600C, HP2200C, DNJ500C
} cdj_printer_type_t;

/*  No. of ink jets (used to minimise head movements)
 *  NOTE:  These don't appear to actually be used anywhere.  Can they
 *         be removed?
 */
#define HEAD_ROWS_MONO 50
#define HEAD_ROWS_COLOUR 16

/*
 *  Colour mapping procedures
 */
static dev_proc_map_cmyk_color(gdev_cmyk_map_cmyk_color);
static dev_proc_map_rgb_color(gdev_cmyk_map_rgb_color);
static dev_proc_map_color_rgb(gdev_cmyk_map_color_rgb);

static dev_proc_map_rgb_color(gdev_pcl_map_rgb_color);
static dev_proc_map_color_rgb(gdev_pcl_map_color_rgb);

/*
 *  Print-page, parameters and miscellaneous procedures
 */
static dev_proc_open_device(hp_colour_open);

static dev_proc_get_params(cdj850_get_params);
static dev_proc_put_params(cdj850_put_params);

static dev_proc_print_page(cdj850_print_page);
static dev_proc_print_page(chp2200_print_page);
static dev_proc_print_page(cdnj500_print_page);

/* The device descriptors */

/* The basic structure for all printers. Note the presence of the cmyk, depth
   and correct fields even if some are not used by all printers. */

#define prn_colour_device_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page, cmyk, correct)\
    prn_device_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page), cmyk, depth /* default */, correct

#define gx_prn_colour_device_common \
    gx_prn_device_common; \
    int cmyk;	  	/* 0: not CMYK-capable, > 0: printing CMYK, */ \
                        /* < 0 : CMYK-capable, not printing CMYK */ \
    uint default_depth;	/* Used only for CMYK-capable printers now. */ \
    uint correction

/* some definitions needed later */
struct error_val_field {
    int c;		/* Current value of Cyan error during dithering */
    int m;		/* Current value of Magenta error during dithering */
    int y;		/* Current value of Yellow error during dithering */
    int k;		/* Current value of Black error during dithering */
};

/* this structure holds all the pointers to the different values
   in all those data fields */
 /*
    * The principal data pointers are stored as pairs of values, with
    * the selection being made by the 'scan' variable. The function of the
    * scan variable is overloaded, as it controls both the alternating
    * raster scan direction used in the Floyd-Steinberg dithering and also
    * the buffer alternation required for line-difference compression.
    *
    * Thus, the number of pointers required is as follows:
  */

struct ptr_arrays {
    byte *data[4];		/* 4 600dpi data, scan direction and alternating buffers */
    byte *data_c[4];		/* 4 300dpi data, as above, */
    byte *plane_data[4][4];	/*4 b/w-planes, scan direction and alternating buffers */
    byte *plane_data_c[4][8];	/* as above, but for 8 planes */
    byte *out_data;		/* output buffer for the b/w data, one 600dpi plane */
    byte *test_data[4];		/* holds a copy of the last plane */
    int *errors[2];		/* 2 b/w dithering erros (scan direction only) */
    int *errors_c[2];		/* 2 color dithering errors (scan direction only) */
    word *storage;		/* pointer to the beginning of the b/w-buffer */
    word *storage_start;	/* used for debugging */
    word *storage_end;		/* used for debugging */
    word *storage_size;		/* used for debugging */
};

/* Some miscellaneous variables */
struct misc_struct {
    int line_size;		/* size of scan_line */
    int line_size_c;		/* size of rescaled scan_line */
    int line_size_words;	/* size of scan_line in words */
    int paper_size;		/* size of paper */
    int num_comps;		/* number of color components (1 - 4) */
    int bits_per_pixel;		/* bits per pixel 1,4,8,16,24,32 */
    int storage_bpp;		/* = bits_per_pixel */
    int expanded_bpp;		/* = bits_per_pixel */
    int plane_size;		/* size of b/w bit plane */
    int plane_size_c;		/* size of color bit plane */
    int databuff_size;		/* size of databuffer for b/w data */
    int databuff_size_c;	/* size of databuffer for color data */
    int errbuff_size;		/* size of error buffer b/w -data */
    int errbuff_size_c;		/* size of error buffer color -data */
    int outbuff_size;		/* size of output buffer for b/w data */
    int scan;			/* scan-line variable [0,1] */
    int cscan;			/* dito for the color-planes */
    int is_two_pass;		/* checks if b/w data has already been printed */
    int zero_row_count;		/* How many empty lines */
    uint storage_size_words;	/* size of storage in words for b/w data */
    uint storage_size_words_c;	/* size of storage in words for c-data */
    int is_color_data;		/* indicates whether there is color data */
};

    /* function pointer typedefs for device driver struct */
typedef void (*StartRasterMode) (gx_device_printer * pdev, int paper_size,
                                 gp_file * prn_stream);
typedef void (*PrintNonBlankLines) (gx_device_printer * pdev,
                                    struct ptr_arrays *data_ptrs,
                                    struct misc_struct *misc_vars,
                                    struct error_val_field *error_values,
                                    const Gamma *gamma,
                                    gp_file * prn_stream);

typedef void (*TerminatePage) (gx_device_printer * pdev, gp_file * prn_stream);

typedef struct gx_device_cdj850_s {
    gx_device_common;
    gx_prn_colour_device_common;
    int /*cdj_quality_t*/ quality;  /* -1 draft, 0 normal, 1 best */
    int /*cdj_paper_type_t*/ papertype;  /* papertype [0,4] */
    int intensities;		/* intensity values per pixel [2,4] */
    int xscal;			/* boolean to indicate x scaling by 2 */
    int yscal;			/* boolean to indicate y scaling by 2 */
    int /*cdj_printer_type_t*/ ptype;  /* printer type, one of DJ670C, DJ850C, DJ890C, DJ1600C */
    int compression;		/* compression level */
    float mastergamma;		/* Gammavalue applied to all colors */
    float gammavalc;		/* range to which gamma-correction is
                                   applied to bw values */
    float gammavalm;		/* amount of gamma correction for bw */
    float gammavaly;		/* range to which gamma-correction i
                                   applied to color values */
    float gammavalk;		/* amount of gamma correction for color */
    float blackcorrect;		/* amount of gamma correction for color */
    StartRasterMode start_raster_mode;	/* output function to start raster mode */
    PrintNonBlankLines print_non_blank_lines;	/* output function to print a non blank line */
    TerminatePage terminate_page;	/* page termination output function */
} gx_device_cdj850;

typedef struct {
    gx_device_common;
    gx_prn_colour_device_common;
} gx_device_colour_prn;

/* Use the cprn_device macro to access generic fields (like cmyk,
   default_depth and correction), and specific macros for specific
   devices. */

#define cprn_device     ((gx_device_colour_prn*) pdev)
#define cdj850    ((gx_device_cdj850 *)pdev)

#define prn_cmyk_colour_device(dtype, procs, dev_name, x_dpi, y_dpi, bpp, print_page, correct)\
    prn_colour_device_body(dtype, procs, dev_name,\
    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, x_dpi, y_dpi, 0, 0, 0, 0,\
    ((bpp == 1 || bpp == 4) ? 1 : 4), bpp,\
    (bpp > 8 ? 255 : 1), (1 << (bpp >> 2)) - 1, /* max_gray, max_color */\
    (bpp > 8 ? 5 : 2), (bpp > 8 ? 5 : bpp > 1 ? 2 : 0),\
    print_page, 1 /* cmyk */, correct)

#define prn_cmy_colour_device(dtype, procs, dev_name, x_dpi, y_dpi, bpp, print_page, correct)\
    prn_colour_device_body(dtype, procs, dev_name,\
    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, x_dpi, y_dpi, 0, 0, 0, 0,\
    ((bpp == 1 || bpp == 4) ? 1 : 3), bpp,\
    (bpp > 8 ? 255 : 1), (bpp > 8 ? 255 : 1), /* max_gray, max_color */\
    (bpp > 8 ? 5 : 2), (bpp > 8 ? 5 : bpp > 1 ? 2 : 0),\
    print_page, -1 /* cmyk */, correct)

/* The prn_rgb_color_device is used by the HP2200 and DNJ500 */
#define prn_rgb_colour_device(dtype, procs, dev_name, x_dpi, y_dpi, bpp, print_page, correct)\
    prn_colour_device_body(dtype, procs, dev_name,\
    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, x_dpi, y_dpi, 0, 0, 0, 0,\
    3 /*3=rgb*/, bpp,\
    255, 255, /* max_gray, max_color */\
    256, 256,\
    print_page, 0 /* no cmyk */, correct)

#define cdj_850_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page, correction, quality, papertype, intensities,ptype,compression,mastergamma,gammavalc,gammavalm,gammavaly,gammavalk,blackcorrect,start_raster_mode,print_non_blank_line,terminate_page)\
{ prn_cmyk_colour_device(gx_device_cdj850, procs, dev_name, x_dpi, y_dpi, bpp, print_page, correction),\
    quality,\
    papertype,\
    intensities,\
    0, 0, /* xscal, yscal */\
    ptype,\
    compression,\
    mastergamma,\
    gammavalc,\
    gammavalm,\
    gammavaly,\
    gammavalk,\
    blackcorrect,\
    start_raster_mode,\
    print_non_blank_line,\
    terminate_page\
}

#define cdj_1600_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page, correction, quality, papertype, intensities,ptype,compression,mastergamma,gammavalc,gammavalm,gammavaly,gammavalk,blackcorrect,start_raster_mode,print_non_blank_line,terminate_page)\
{ prn_cmy_colour_device(gx_device_cdj850, procs, dev_name, x_dpi, y_dpi, bpp, print_page, correction),\
    quality,\
    papertype,\
    intensities,\
    0, 0, /* xscal, yscal */\
    ptype,\
    compression,\
    mastergamma,\
    gammavalc,\
    gammavalm,\
    gammavaly,\
    gammavalk,\
    blackcorrect,\
    start_raster_mode,\
    print_non_blank_line,\
    terminate_page\
}

/* HP2200 and DNJ500 is a RGB printer */
#define chp_2200_device(procs, dev_name, x_dpi, y_dpi, bpp, print_page, correction, quality, papertype, intensities,ptype,compression,mastergamma,gammavalc,gammavalm,gammavaly,gammavalk,blackcorrect,start_raster_mode,print_non_blank_line,terminate_page)\
{ prn_rgb_colour_device(gx_device_cdj850, procs, dev_name, x_dpi, y_dpi, bpp, print_page, correction),\
    quality,\
    papertype,\
    intensities,\
    0, 0, /* xscal, yscal */\
    ptype,\
    compression,\
    mastergamma,\
    gammavalc,\
    gammavalm,\
    gammavaly,\
    gammavalk,\
    blackcorrect,\
    start_raster_mode,\
    print_non_blank_line,\
    terminate_page\
}

/*  Printer-specific functions.  Most printers are handled by the cdj850_xx()
 *  functions.
 */
static void
     cdj850_start_raster_mode(gx_device_printer * pdev,
                              int papersize, gp_file * prn_stream);

static void
     cdj850_print_non_blank_lines(gx_device_printer * pdev,
                                  struct ptr_arrays *data_ptrs,
                                  struct misc_struct *misc_vars,
                                  struct error_val_field *error_values,
                                  const Gamma *gamma,
                                  gp_file * prn_stream);

static void
     cdj850_terminate_page(gx_device_printer * pdev, gp_file * prn_stream);

/*  The 880C and siblings need a separate set of functions because they seem
 *  to require a somewhat different version of PCL3+.
 */
static void
     cdj880_start_raster_mode(gx_device_printer * pdev,
                              int papersize, gp_file * prn_stream);

static void
     cdj880_print_non_blank_lines(gx_device_printer * pdev,
                                  struct ptr_arrays *data_ptrs,
                                  struct misc_struct *misc_vars,
                                  struct error_val_field *error_values,
                                  const Gamma *gamma,
                                  gp_file * prn_stream);

static void
     cdj880_terminate_page(gx_device_printer * pdev, gp_file * prn_stream);

/*  Functions for the 1600C.
 */
static void
     cdj1600_start_raster_mode(gx_device_printer * pdev,
                               int papersize, gp_file * prn_stream);
static void
     cdj1600_print_non_blank_lines(gx_device_printer * pdev,
                                   struct ptr_arrays *data_ptrs,
                                   struct misc_struct *misc_vars,
                                   struct error_val_field *error_values,
                                   const Gamma *gamma,
                                   gp_file * prn_stream);
static void
     cdj1600_terminate_page(gx_device_printer * pdev, gp_file * prn_stream);

/*  Functions for the HP2200C */
static void
     chp2200_start_raster_mode(gx_device_printer * pdev,
                               int papersize, gp_file * prn_stream);

static void
     chp2200_terminate_page(gx_device_printer * pdev, gp_file * prn_stream);

/*  Functions for the DNJ500C */
static void
     cdnj500_start_raster_mode(gx_device_printer * pdev,
                               int papersize, gp_file * prn_stream);

static void
     cdnj500_terminate_page(gx_device_printer * pdev, gp_file * prn_stream);

/* This decoding to RGB and conversion to CMYK simulates what */
/* gx_default_decode_color does without calling the map_color_rgb method. */
static int
cdj670_compatible_cmyk_decode_color(gx_device *dev, gx_color_index color, gx_color_value cv[4])
{
    int i, code = gdev_cmyk_map_color_rgb(dev, color, cv);
    gx_color_value min_val = gx_max_color_value;

    for (i = 0; i < 3; i++) {
        if ((cv[i] = gx_max_color_value - cv[i]) < min_val)
            min_val = cv[i];
    }
    for (i = 0; i < 3; i++)
        cv[i] -= min_val;
    cv[3] = min_val;

    return code;
}


static void
cdj670_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, hp_colour_open);
    set_dev_proc(dev, map_rgb_color, gx_error_encode_color);
    set_dev_proc(dev, map_color_rgb, gdev_cmyk_map_color_rgb);
    set_dev_proc(dev, get_params, cdj850_get_params);
    set_dev_proc(dev, put_params, cdj850_put_params);
    set_dev_proc(dev, map_cmyk_color, gdev_cmyk_map_cmyk_color);
    set_dev_proc(dev, encode_color, gdev_cmyk_map_cmyk_color);
    set_dev_proc(dev, decode_color, cdj670_compatible_cmyk_decode_color);
}

static void
cdj1600_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, hp_colour_open);
    set_dev_proc(dev, map_rgb_color, gdev_pcl_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gdev_pcl_map_color_rgb);
    set_dev_proc(dev, get_params, cdj850_get_params);
    set_dev_proc(dev, put_params, cdj850_put_params);
    set_dev_proc(dev, map_cmyk_color, gx_error_encode_color);
    set_dev_proc(dev, encode_color, gdev_pcl_map_rgb_color);
    set_dev_proc(dev, decode_color, gdev_pcl_map_color_rgb);
}

static void
chp2200_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, hp_colour_open);
    set_dev_proc(dev, map_rgb_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_rgb_map_color_rgb);
    set_dev_proc(dev, get_params, cdj850_get_params);
    set_dev_proc(dev, put_params, cdj850_put_params);
    set_dev_proc(dev, map_cmyk_color, gx_error_encode_color);
    set_dev_proc(dev, encode_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_rgb_map_color_rgb);
}

const gx_device_cdj850 gs_cdj670_device =
cdj_850_device(cdj670_initialize_device_procs, "cdj670", 600, 600, 32, cdj850_print_page, 0,
               PRESENTATION, PLAIN_PAPER, 2, DJ670C, 9,
               1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
               cdj850_start_raster_mode, cdj850_print_non_blank_lines,
               cdj850_terminate_page);

const gx_device_cdj850 gs_cdj850_device =
cdj_850_device(cdj670_initialize_device_procs, "cdj850", 600, 600, 32, cdj850_print_page, 0,
               PRESENTATION, PLAIN_PAPER, 4, DJ850C, 9,
               1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
               cdj850_start_raster_mode, cdj850_print_non_blank_lines,
               cdj850_terminate_page);

const gx_device_cdj850 gs_cdj880_device =
cdj_850_device(cdj670_initialize_device_procs, "cdj880", 600, 600, 32, cdj850_print_page, 0,
               PRESENTATION, PLAIN_PAPER, 4, DJ880C, 2,
               1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
               cdj880_start_raster_mode, cdj880_print_non_blank_lines,
               cdj880_terminate_page);

const gx_device_cdj850 gs_cdj890_device =
cdj_850_device(cdj670_initialize_device_procs, "cdj890", 600, 600, 32, cdj850_print_page, 0,
               PRESENTATION, PLAIN_PAPER, 4, DJ890C, 9,
               1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
               cdj850_start_raster_mode, cdj850_print_non_blank_lines,
               cdj850_terminate_page);

const gx_device_cdj850 gs_cdj1600_device =
cdj_1600_device(cdj1600_initialize_device_procs, "cdj1600", 300, 300, 24, cdj850_print_page, 0,
                PRESENTATION, PLAIN_PAPER, 2, DJ1600C, 3,
                1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
                cdj1600_start_raster_mode, cdj1600_print_non_blank_lines,
                cdj1600_terminate_page);

/* HP2200 does not need color matching and halftoning parameters */
const gx_device_cdj850 gs_chp2200_device =
chp_2200_device(chp2200_initialize_device_procs, "chp2200", 300, 300, 24, chp2200_print_page, 0,
               NORMAL, PLAIN_PAPER, 0 /*unused*/, HP2200C, 10,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0, /*all unused*/
               chp2200_start_raster_mode, NULL /*unused*/,
               chp2200_terminate_page);

/* DNJ500 does not need color matching and halftoning parameters */
const gx_device_cdj850 gs_cdnj500_device =
chp_2200_device(chp2200_initialize_device_procs, "cdnj500", 300, 300, 24, cdnj500_print_page, 0,
               NORMAL, PLAIN_PAPER, 0 /*unused*/, DNJ500C, 10,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0, /*all unused*/
               cdnj500_start_raster_mode, NULL /*unused*/,
               cdnj500_terminate_page);

/* Forward references */
static int cdj_put_param_int(gs_param_list *, gs_param_name,
                              int *, int, int, int);
static int cdj_put_param_float(gs_param_list *, gs_param_name, float
                                *, float, float, int);
static int cdj_put_param_bpp(gx_device *, gs_param_list *, int, int, int);
static int cdj_set_bpp(gx_device *, int, int);

/*  hp_colour_open()
 *
 *  Open the printer and set up the margins.  Also, set parameters for
 *  the printer depending on document type and print settings.
 *
 *  Inputs:  gx_device                  ptr to the device
 */
static int
hp_colour_open(gx_device * pdev)
{				/* Change the margins if necessary. */
    static const float dj_a4[4] = {
        DESKJET_MARGINS_A4
    };
    static const float dj_letter[4] = {
        DESKJET_MARGINS_LETTER
    };

    /* margins for DJ1600C from manual */
    static const float m_cdj1600[4] = {
        0.25f, 0.5f, 0.25f, 0.5f
    };

    /* margins for HP2200C */
    static const float chp2200_a4[4] = {
        0.13f, 0.46f, 0.13f, 0.08f
    };
    static const float chp2200_letter[4] = {
        0.25f, 0.46f, 0.25f, 0.08f
    };

    /* margins for DNJ500C */
    static const float cdnj500[4] = {
        0.00f, 0.00f, 0.00f, 0.00f
    };

    const float *m = (float *)0;

    /* Set up colour params if put_params has not already done so */
    if (pdev->color_info.num_components == 0) {
        int code = cdj_set_bpp(pdev, pdev->color_info.depth,
                               pdev->color_info.num_components);

        if (code < 0)
            return code;
    }
    /* assign printer type and set resolution dependent on printer type */
    switch (cdj850->ptype) {
    case DJ670C:
        if (cdj850->papertype <= SPECIAL_PAPER) {	/* paper */
            if (cdj850->quality == DRAFT) {
                gx_device_set_resolution(pdev, 300.0, 300.0);
                cdj850->xscal = 0;
                cdj850->yscal = 0;
            } else if (cdj850->quality == NORMAL) {
                gx_device_set_resolution(pdev, 600.0, 300.0);
                cdj850->xscal = 1;
                cdj850->yscal = 0;
            } else {		/* quality == PRESENTATION */
                gx_device_set_resolution(pdev, 600.0, 600.0);
                cdj850->xscal = 1;
                cdj850->yscal = 1;
            }
        } else {		/* film */
            gx_device_set_resolution(pdev, 600.0, 300.0);
            cdj850->xscal = 1;
            cdj850->yscal = 0;
        }
        m = (gdev_pcl_paper_size(pdev) == PAPER_SIZE_A4 ? dj_a4 : dj_letter);
        break;
    case DJ850C:
      if (cdj850->quality == DRAFT) {
            gx_device_set_resolution(pdev, 300.0, 300.0);
            cdj850->xscal = 0;
            cdj850->yscal = 0;
            cdj850->intensities = 2;
        } else if (cdj850->quality == NORMAL) {
            gx_device_set_resolution(pdev, 600.0, 600.0);
            cdj850->xscal = 1;
            cdj850->yscal = 1;
            /* only 3 intensities for normal paper */
            if (cdj850->papertype <= PLAIN_PAPER) {
                cdj850->intensities = 3;
            }			/* else cdj850->intensities = 4 from initialization */
        } else {		/* quality == PRESENTATION */
            gx_device_set_resolution(pdev, 600.0, 600.0);
            cdj850->xscal = 1;
            cdj850->yscal = 1;
            /* intensities = 4 from initialization */
        }
        m = (gdev_pcl_paper_size(pdev) == PAPER_SIZE_A4 ? dj_a4 : dj_letter);
        break;
    case DJ880C:
        if (cdj850->quality == DRAFT) {
            gx_device_set_resolution(pdev, 300.0, 300.0);
            cdj850->xscal = 0;
            cdj850->yscal = 0;
            cdj850->intensities = 2;
        } else if (cdj850->quality == NORMAL) {
            gx_device_set_resolution(pdev, 600.0, 300.0);
            cdj850->xscal = 1;
            cdj850->yscal = 0;
            /* only 3 intensities for normal paper */
            if (cdj850->papertype <= PLAIN_PAPER) {
                cdj850->intensities = 4;
            }			/* else cdj850->intensities = 4 from initialization */
        } else {		/* quality == PRESENTATION */
            gx_device_set_resolution(pdev, 600.0, 600.0);
            cdj850->xscal = 0;    /* color is also 600dpi in PRESENTATION mode */
            cdj850->yscal = 0;
          cdj850->intensities = 4;
        }
        m = (gdev_pcl_paper_size(pdev) == PAPER_SIZE_A4 ? dj_a4 : dj_letter);
        break;
    case DJ890C:
        if (cdj850->quality == DRAFT) {
            gx_device_set_resolution(pdev, 300.0, 300.0);
            cdj850->xscal = 0;
            cdj850->yscal = 0;
            cdj850->intensities = 2;
        } else if (cdj850->quality == NORMAL) {
            gx_device_set_resolution(pdev, 600.0, 300.0);
            cdj850->xscal = 1;
            cdj850->yscal = 0;
            /* only 3 intensities for normal paper */
            if (cdj850->papertype <= PLAIN_PAPER) {
                cdj850->intensities = 3;
            }			/* else cdj850->intensities = 4 from initialization */
        } else {		/* quality == PRESENTATION */
            gx_device_set_resolution(pdev, 600.0, 600.0);
            cdj850->xscal = 1;
            cdj850->yscal = 1;
            /* intensities = 4 from initialization */
        }
        m = (gdev_pcl_paper_size(pdev) == PAPER_SIZE_A4 ? dj_a4 : dj_letter);
        break;
    case DJ1600C:
        gx_device_set_resolution(pdev, 300.0, 300.0);
        m = m_cdj1600;
        break;
    /* HP2200 supports 300dpi draft/normal and 600dpi normal/best
       for all media types.  For normal, we are only using 300dpi here*/
    case HP2200C:
        cdj850->xscal = 0; /* unused */
        cdj850->yscal = 0; /* unused */
        cdj850->intensities = 0; /* unused */
        if (cdj850->quality == DRAFT) {
            gx_device_set_resolution(pdev, 300.0, 300.0);
        } else if (cdj850->quality == NORMAL) {
            gx_device_set_resolution(pdev, 300.0, 300.0);
        } else {  /* quality == PRESENTATION */
            gx_device_set_resolution(pdev, 600.0, 600.0);
        }
        m = (gdev_pcl_paper_size(pdev) == PAPER_SIZE_A4 ? chp2200_a4 : chp2200_letter);
        break;
    /* DNJ500 supports 300dpi and 600dpi with any combinations */
    case DNJ500C:
        cdj850->xscal = 0; /* unused */
        cdj850->yscal = 0; /* unused */
        cdj850->intensities = 0; /* unused */
        if (cdj850->quality == DRAFT) {
            gx_device_set_resolution(pdev, 300.0, 300.0);
        } else if (cdj850->quality == NORMAL) {
            gx_device_set_resolution(pdev, 600.0, 600.0);
        } else {  /* quality == PRESENTATION */
            gx_device_set_resolution(pdev, 600.0, 600.0);
        }
        m = cdnj500;
        break;
    default:
        assert(0);
    }
    if (m != NULL)
        gx_device_set_margins(pdev, m, true);
    return gdev_prn_open(pdev);
}

/* Added parameters for DeskJet 850C */
static int
cdj850_get_params(gx_device * pdev, gs_param_list * plist)
{
    int code = gdev_prn_get_params(pdev, plist);

    if (code < 0 ||
        (code = param_write_int(plist, "Quality", &cdj850->quality)) < 0 ||
        (code = param_write_int(plist, "Papertype", &cdj850->papertype)) < 0 ||
        (code = param_write_float(plist, "MasterGamma", &cdj850->gammavalc))
        < 0 ||
        (code = param_write_float(plist, "GammaValC", &cdj850->gammavalc)) <
        0 ||
        (code = param_write_float(plist, "GammaValM", &cdj850->gammavalm)) <
        0 ||
        (code = param_write_float(plist, "GammaValY", &cdj850->gammavaly)) <
        0 ||
        (code = param_write_float(plist, "GammaValK", &cdj850->gammavalk)) <
        0 ||
        (code = param_write_float(plist, "BlackCorrect",
                                  &cdj850->blackcorrect)) < 0
        )
        return code;

    return code;
}

static int
cdj850_put_params(gx_device * pdev, gs_param_list * plist)
{
    int quality = cdj850->quality;
    int papertype = cdj850->papertype;
    float mastergamma = cdj850->mastergamma;
    float gammavalc = cdj850->gammavalc;
    float gammavalm = cdj850->gammavalm;
    float gammavaly = cdj850->gammavaly;
    float gammavalk = cdj850->gammavalk;
    float blackcorrect = cdj850->blackcorrect;
    int bpp = 0;
    int code = 0;

    code = cdj_put_param_int(plist, "BitsPerPixel", &bpp, 1, 32, code);
    code = cdj_put_param_int(plist, "Quality", &quality, 0, 2, code);
    code = cdj_put_param_int(plist, "Papertype", &papertype, 0, 4, code);
    code = cdj_put_param_float(plist, "MasterGamma", &mastergamma, 0.1f, 9.0f, code);
    code = cdj_put_param_float(plist, "GammaValC", &gammavalc, 0.0f, 9.0f, code);
    code = cdj_put_param_float(plist, "GammaValM", &gammavalm, 0.0f, 9.0f, code);
    code = cdj_put_param_float(plist, "GammaValY", &gammavaly, 0.0f, 9.0f, code);
    code = cdj_put_param_float(plist, "GammaValK", &gammavalk, 0.0f, 9.0f, code);
    code = cdj_put_param_float(plist, "BlackCorrect", &blackcorrect, 0.0f,
                               9.0f, code);

    if (code < 0)
        return code;
    code = cdj_put_param_bpp(pdev, plist, bpp, bpp, 0);
    if (code < 0)
        return code;

    cdj850->quality = quality;
    cdj850->papertype = papertype;
    cdj850->mastergamma = mastergamma;
    cdj850->gammavalc = gammavalc;
    cdj850->gammavalm = gammavalm;
    cdj850->gammavaly = gammavaly;
    cdj850->gammavalk = gammavalk;
    cdj850->blackcorrect = blackcorrect;
    return 0;
}

/* ------ Internal routines ------ */
/* The DeskJet850C can compress (mode 9) */

/* Some convenient shorthand .. */
#define x_dpi        (pdev->x_pixels_per_inch)
#define y_dpi        (pdev->y_pixels_per_inch)

/* To calculate buffer size as next greater multiple of both parameter and W */
#define calc_buffsize(a, b) (((((a) + ((b) * W) - 1) / ((b) * W))) * W)

/* internal functions */
static void
     FSDlinebw(int scan, int plane_size,
               struct error_val_field *error_values,
               byte * kP,
               int n, int *ep, byte * dp);
static void
     FSDlinec2(int scan, int plane_size,
               struct error_val_field *error_values,
               byte * cPa, byte * mPa, byte * yPa, int n,
               byte * dp, int *ep);
static void
     FSDlinec3(int scan, int plane_size,
               struct error_val_field *error_values,
               byte * cPa, byte * mPa, byte * yPa,
               byte * cPb, byte * mPb, byte * yPb,
               int n, byte * dp, int *ep);
static void
     FSDlinec4(int scan, int plane_size,
               struct error_val_field *error_values,
               byte * cPa, byte * mPa, byte * yPa,
               byte * cPb, byte * mPb, byte * yPb,
               int n, byte * dp, int *ep);
static void
     init_error_buffer(struct misc_struct *misc_vars,
                       struct ptr_arrays *data_ptrs);
static void
     do_floyd_steinberg(int scan, int cscan, int plane_size,
                        int plane_size_c, int n,
                        struct ptr_arrays *data_ptrs,
                        gx_device_printer * pdev,
                        struct error_val_field *error_values);
static int
do_gcr(int bytecount, byte * inbyte, const byte kvalues[256],
       const byte cvalues[256], const byte mvalues[256],
       const byte yvalues[256], const int kcorrect[256],
       word * inword);

/* UNUSED
 *static int
 *test_scan (P4(int size,
 *            byte * current,
 *            byte * last,
 *            byte * control));
 *static void
 *save_color_data(P3(int size,
 *                 byte * current,
 *                 byte * saved));
 *
 */
static void
     send_scan_lines(gx_device_printer * pdev,
                     struct ptr_arrays *data_ptrs,
                     struct misc_struct *misc_vars,
                     struct error_val_field *error_values,
                     const Gamma *gamma,
                     gp_file * prn_stream);
#ifdef UNUSED
static void
     do_gamma(float mastergamma, float gammaval, byte * values);
#endif
static void
do_black_correction(float kvalue, int kcorrect[256]);

static void
     init_data_structure(gx_device_printer * pdev,
                         struct ptr_arrays *data_ptrs,
                         struct misc_struct *misc_vars);
static void
     calculate_memory_size(gx_device_printer * pdev,
                           struct misc_struct *misc_vars);

static void
assign_dpi(int dpi, byte * msb)
{
    if (dpi == 600) {
        msb[0] = 0x02;
        msb[1] = 0x58;
    } else {
        msb[0] = 0x01;
        msb[1] = 0x2c;
    }
}

static void
cdj850_terminate_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    gp_fputs("0M", prn_stream);	/* Reset compression */
    gp_fputs("\033*rC\033E", prn_stream);	/* End Graphics, Reset */
    gp_fputs("\033&l0H", prn_stream);	/* eject page */
}

static void
cdj880_terminate_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    gp_fputs("\033*rC\f\033E", prn_stream);  /* End graphics, FF, Reset */
    gp_fputs("\033%-12345X", prn_stream);
}

/* HP2200 terminate page routine */
static void
chp2200_terminate_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    gp_fputs("\033*rC\f\033E", prn_stream);  /* End graphics, FF, Reset */
    gp_fputs("\033%-12345X@PJL EOJ\012\033%-12345X", prn_stream); /* Send the PJL EOJ */
}

/* DNJ500 terminate page routine */
static void
cdnj500_terminate_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    gp_fputs("\033*rC", prn_stream);  /* End graphics */
    gp_fputs("\033%-12345X@PJL EOJ \n", prn_stream); /* Send the PJL EOJ */
}

/* Here comes the hp850 output routine -------------------- */
static int
cdj850_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{

    struct error_val_field error_values;
    struct ptr_arrays data_ptrs;
    struct misc_struct misc_vars;
    int i;

    Gamma gamma;

    /* make a local writable copy of the Gamma tables */
    memcpy(&gamma, gammat[cdj850->ptype], sizeof(Gamma));
    for (i=0; i<256; i++)
      gamma.k[i] = (int)(((float)(i*i*i))/(256.0*256.0));
/*      gamma.k[i] = (int)(sqrt((float)i)/16);*/
    /* if mastergamma, don't use the built in functions */
/*    if (cdj850->mastergamma > 1.0) {*/
        /* prepare the bw lookup table */
/*	do_gamma(cdj850->mastergamma, cdj850->gammavalk, gamma.k);*/
        /* prepare the color lookup table */
    for (i=0; i<256; i++)
      gamma.c[i] = (int)(((float)(i*i*i))/(256.0*256.0));
/*	do_gamma(cdj850->mastergamma, cdj850->gammavalc, gamma.c);*/
    for (i=0; i<256; i++)
      gamma.m[i] = (int)(((float)(i*i*i))/(256.0*256.0));
/*	do_gamma(cdj850->mastergamma, cdj850->gammavalm, gamma.m);*/
    for (i=0; i<256; i++)
      gamma.y[i] = (int)(((float)(i*i*i))/(256.0*256.0));
/*	do_gamma(cdj850->mastergamma, cdj850->gammavaly, gamma.y);*/
/*    }*/
    /* prepare the black correction table for the unbunt mask */
    do_black_correction(cdj850->blackcorrect, gamma.correct);

    /* Calculate the needed memory */
    calculate_memory_size(pdev, &misc_vars);

    /* and allocate the memory */

    /* Since we need 600 and 300 dpi, we set up several buffers:
       storage contains the data as copied from gs, as well as the
       plane-data and the out_row buffer.
       storagec will contain the rescaled color data. It also contains the
       plane_data for the color-planes - these are needed by the
       compression routine, but would be overwritten by the
       b/w-dithering. The color planes allow for overwriting the
       color-data by the error-data. Since we might use the
       2bpp feature of the hp850 someday, it is sized like storage.
       storagee contains the errors from b/w fs-ditherng */

    data_ptrs.storage = (ulong *) gs_malloc(pdev->memory->non_gc_memory, misc_vars.storage_size_words, W,
                                            "cdj850_print_page");

    /* if we can't allocate working area */
    if (data_ptrs.storage == 0) {
        return_error(gs_error_VMerror);
    }
    /* Initialise the needed pointers */
    init_data_structure(pdev, &data_ptrs, &misc_vars);

    /* Start Raster mode */
    (*cdj850->start_raster_mode) (pdev, misc_vars.paper_size, prn_stream);

    /* Send each scan line in turn */
    send_scan_lines(pdev, &data_ptrs, &misc_vars,
                    &error_values, &gamma, prn_stream);

    /* terminate page and eject paper */
    (*cdj850->terminate_page) (pdev, prn_stream);

    /* Free Memory */
    gs_free(pdev->memory->non_gc_memory, (char *)data_ptrs.storage, misc_vars.storage_size_words, W,
            "hp850_print_page");

    return 0;
}

/* HP2200C compression routines*/

#define kWhite 0x00FFFFFE
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define MAX(x,y) ((x)>(y) ? (x) : (y))

enum
{
    eLiteral        = 0,
    eRLE            = 0x80
};

enum
{
    eeNewPixel      = 0x0,
    eeWPixel        = 0x20,
    eeNEPixel       = 0x40,
    eeCachedColor   = 0x60,
};

enum
{
    eNewColor       = 0x0,
    eWestColor      = 0x1,
    eNorthEastColor = 0x2,
    eCachedColor    = 0x3
};

static inline unsigned int
getPixel(byte* pixAddress, unsigned int pixelOffset)
{
    pixAddress += (pixelOffset*3);    /* assume 3 bytes for each RGB pixel */
    /* RGBRGB format */
    return (((unsigned int)(*(pixAddress)) << 16) +
            ((unsigned int)(*(pixAddress+1)) << 8 ) +
            ((*(pixAddress+2)) & kWhite));

    /* BGRBGR format */
    /*
    return (((unsigned int)(*(pixAddress+2)) << 16) +
            ((unsigned int)(*(pixAddress+1)) << 8 ) +
            ((*(pixAddress)) & kWhite));
    */
}

static inline void
putPixel(byte* pixAddress, unsigned int pixelOffset, unsigned int pixel)
{
    pixAddress += (pixelOffset*3);    /* assume 3 bytes for each RGB pixel*/
    /* RGBRGB format */
    *(pixAddress) = (byte) (pixel >> 16);
    *(pixAddress+1) = (byte) (pixel >> 8);
    *(pixAddress+2) = (byte) (pixel & 0xFE);

    /* BGRBGR format */
    /*
    *(pixAddress+2) = (byte) (pixel >> 16);
    *(pixAddress+1) = (byte) (pixel >> 8);
    *(pixAddress) = (byte) (pixel & 0xFE);
    */
}

static inline unsigned int
ShortDelta(byte* curPtr, byte* seedPtr, unsigned int pixelOffset)
{
    int dr, dg, db;
    curPtr += (pixelOffset*3);
    seedPtr += (pixelOffset*3);
    /* RGBRGB format */
    dr = (int)((unsigned int)(*(curPtr)) - (unsigned int)(*(seedPtr)));
    dg = (int)((unsigned int)(*(curPtr+1)) - (unsigned int)(*(seedPtr+1)));
    db = (int)((unsigned int)((*(curPtr+2))&0xFE) - (unsigned int)((*(seedPtr+2))&0xFE));

    /* BGRBGR format */
    /*
    dr = (int)((unsigned int)(*(curPtr+2)) - (unsigned int)(*(seedPtr+2)));
    dg = (int)((unsigned int)(*(curPtr+1)) - (unsigned int)(*(seedPtr+1)));
    db = (int)((unsigned int)((*(curPtr))&0xFE) - (unsigned int)((*(seedPtr))&0xFE));
    */

    if ((dr <= 15) && (dr >= -16) && (dg <= 15) && (dg >= -16) && (db <= 30) && (db >= -32))
    {   /* Note db is divided by 2 to double it's range from -16..15 to -32..30 */
        return (unsigned int)(((dr << 10) & 0x7C00) | (((dg << 5) & 0x03E0) | ((db >> 1) & 0x001F) | 0x8000));   /* set upper bit to signify short delta*/
    }
    return 0;
}

/* HP2200C - mode 10 compression for RGB data */
static unsigned int
Mode10(unsigned int planeWidthInPixels,
       byte * pbyColorScanPtr, /*input scanline in RGBRGBRGB format*/
       byte * pbyColorSeedPtr,
       byte * pbyColorOutputPtr)
{
    unsigned int curPixel = 0;
    unsigned int seedRowPixelCopyCount;
    unsigned int cachedColor = kWhite;
    unsigned int lastPixel = planeWidthInPixels - 1;
    unsigned int realLastPixel = 0;
    unsigned int temp1, temp2, temp3;

    byte * curPtr = pbyColorScanPtr;
    byte * compressedDataPtr = pbyColorOutputPtr;
    byte * seedPtr = pbyColorSeedPtr;

    byte * compressedDataStart = compressedDataPtr;

    /* Setup sentinal value to replace last pixel of curRow.
       Simplifies future end condition checking.*/
    realLastPixel = getPixel(curPtr, lastPixel);

    temp1 = getPixel(curPtr, lastPixel-1);
    temp2 = getPixel(seedPtr, lastPixel);
    temp3 = realLastPixel;
    while ( (temp1 == temp3) || (temp2 == temp3))
    {
        *(curPtr+1+lastPixel*3) += 1; /* add one to green. */
        temp3 = getPixel(curPtr, lastPixel);
    }

    do /* all pixels in row */
    {
        byte CMDByte = eLiteral;
        unsigned int replacementCount = 0;
        int pixelSource = 0;

        /* Find seedRowPixelCopyCount for upcoming copy */
        seedRowPixelCopyCount = curPixel;
        while ( getPixel(seedPtr, curPixel) ==
                getPixel(curPtr, curPixel))
        {
            curPixel++;
        }
        seedRowPixelCopyCount = curPixel - seedRowPixelCopyCount;

            /* On last pixel of row. RLE could also leave us on the last pixel of the row
           from the previous iteration. */
        if (curPixel == lastPixel)
        {
            putPixel(curPtr, lastPixel, realLastPixel);

            if (getPixel(seedPtr, curPixel) ==
                realLastPixel)
            {
                return compressedDataPtr - compressedDataStart;
            }
            else /* code last pix as a literal */
            {
                CMDByte = eLiteral;
                pixelSource = eeNewPixel;
                replacementCount = 1;
                curPixel++;
            }
        }
        else /* prior to last pixel of row */
        {
            unsigned int RLERun = 0;
            replacementCount = curPixel;
            RLERun = getPixel(curPtr, curPixel);

            curPixel++; /* Adjust for next pixel.*/
            while (RLERun == getPixel(curPtr, curPixel))
            {
                curPixel++;
            }
            curPixel--; /* snap back to current.*/
            replacementCount = curPixel - replacementCount;

            if (replacementCount > 0) /* Adjust for total occurance and move to next pixel to do.*/
            {
                curPixel++;
                replacementCount++;

                if (cachedColor == RLERun)
                    pixelSource = eeCachedColor;
                else if (getPixel(seedPtr, curPixel-replacementCount+1) == RLERun)
                    pixelSource = eeNEPixel;
                else if ((curPixel-replacementCount > 0) &&  (getPixel(curPtr, curPixel-replacementCount-1) == RLERun))
                    pixelSource = eeWPixel;
                else
                {
                    pixelSource = eeNewPixel;
                    cachedColor = RLERun;
                }

                CMDByte = eRLE; /* Set default for later.*/
            }

            if (curPixel == lastPixel)
            {
                /* Already found some RLE pixels */
                /* Add to current RLE. Otherwise it'll be part of the literal
                   from the seedrow section above on the next iteration.*/
                if (realLastPixel == RLERun)
                {
                    putPixel(curPtr, lastPixel, realLastPixel);
                    replacementCount++;
                    curPixel++;
                }
            }

            if (0 == replacementCount) /* no RLE so it's a literal by default.*/
            {
                unsigned int tempPixel = getPixel(curPtr, curPixel);
                        unsigned int tempPixel2 = 0;
                CMDByte = eLiteral;

                if (cachedColor == tempPixel)
                {
                    pixelSource = (byte) eeCachedColor;
                }
                else if (getPixel(seedPtr, curPixel+1) == tempPixel)
                {
                    pixelSource = (byte)eeNEPixel;
                }
                else if ((curPixel > 0) &&  (getPixel(curPtr, curPixel-1) == tempPixel))
                {
                    pixelSource = (byte) eeWPixel;
                }
                else
                {
                    pixelSource = (byte) eeNewPixel;
                    cachedColor = tempPixel;
                }

                replacementCount = curPixel;
                do
                {
                    if (++curPixel == lastPixel)
                    {
                        putPixel(curPtr, lastPixel, realLastPixel);
                        curPixel++;
                        break;
                    }

                    tempPixel2 = getPixel(curPtr, curPixel);

                } while ((tempPixel2 != getPixel(curPtr, curPixel+1)) &&
                         (tempPixel2 != getPixel(seedPtr, curPixel)));

                replacementCount = curPixel - replacementCount;
            }
        }

        /* Write out compressed data next.*/
        if (eLiteral == CMDByte)
        {
            unsigned int totalReplacementCount = 0;
            unsigned int upwardPixelCount = 0;

            replacementCount --; /* normalize it*/

            CMDByte |= (byte) pixelSource; /* Could put this directly into CMDByte above.*/
            CMDByte |= (byte)(MIN(3, seedRowPixelCopyCount) << 3);
            CMDByte |= (byte)MIN(7, replacementCount);

            *compressedDataPtr++ = (byte)CMDByte;

            if (seedRowPixelCopyCount >= 3)
            {
                byte temp;
                int number = seedRowPixelCopyCount - 3;
                do
                {
                    *compressedDataPtr++ = temp = (byte)(MIN(number, 255));
                    if (255 == number)
                    {
                        *compressedDataPtr++ = 0;
                    }
                } while(number -= temp);
            }

            replacementCount ++; /* denormalize it*/

            totalReplacementCount = replacementCount;
            upwardPixelCount = 1;

            if (eeNewPixel != pixelSource)
            {
                /* Do not encode 1st pixel of run since it comes from an alternate location.*/
                replacementCount --;

                upwardPixelCount = 2;
            }

            for ( ; upwardPixelCount <= totalReplacementCount; upwardPixelCount++)
            {
                unsigned int tempCount = curPixel-replacementCount;
                unsigned int compressedPixel = ShortDelta(curPtr, seedPtr, tempCount);

                if (compressedPixel)
                {
                    *compressedDataPtr++ = (byte)(compressedPixel >> 8);
                    *compressedDataPtr++ = (byte)(compressedPixel);
                }
                else
                {
                    unsigned int uncompressedPixel = getPixel(curPtr, tempCount) >>1;
                    *compressedDataPtr++ = (byte)(uncompressedPixel >> 16);
                    *compressedDataPtr++ = (byte)(uncompressedPixel >> 8);
                    *compressedDataPtr++ = (byte)(uncompressedPixel);
                }
                /* See if it's time to spill a single VLI byte.*/
                if (upwardPixelCount>=8 && ((upwardPixelCount-8) % 255) == 0)
                {
                    *compressedDataPtr++ = (byte)(MIN(255, totalReplacementCount - upwardPixelCount));
                }

                replacementCount--;
            }
        }
        else /* RLE */
        {
            replacementCount -= 2; /* normalize it*/

            CMDByte |= pixelSource; /* Could put this directly into CMDByte above.*/
            CMDByte |= MIN(3, seedRowPixelCopyCount) << 3;
            CMDByte |= MIN(7, replacementCount);

            *compressedDataPtr++ = (byte)CMDByte;

            if (seedRowPixelCopyCount >= 3)
            {
                byte temp;
                int number = seedRowPixelCopyCount - 3;
                do
                {
                    *compressedDataPtr++ = temp = (byte)(MIN(number, 255));
                    if (255 == number)
                    {
                        *compressedDataPtr++ = 0;
                    }

                } while(number -= temp);
            }

            replacementCount += 2; /* denormalize it*/

            if (eeNewPixel == pixelSource)
            {
                unsigned int tempCount = curPixel - replacementCount;
                unsigned int compressedPixel = ShortDelta(curPtr, seedPtr, tempCount);

                if (compressedPixel)
                {
                    *compressedDataPtr++ = (byte)(compressedPixel >> 8);
                    *compressedDataPtr++ = (byte)compressedPixel;
                }
                else
                {
                    unsigned int uncompressedPixel = getPixel(curPtr, tempCount) >>1;
                    *compressedDataPtr++ = (byte)(uncompressedPixel >> 16);
                    *compressedDataPtr++ = (byte)(uncompressedPixel >> 8);
                    *compressedDataPtr++ = (byte)uncompressedPixel;
                }
            }

            if (replacementCount >= 9)
            {
                byte temp;
                int number = replacementCount - 9;
                do
                {
                    *compressedDataPtr++ = temp = (byte)(MIN(number, 255));
                    if (255 == number)
                    {
                        *compressedDataPtr++ = 0;
                    }

                } while(number -= temp);
            }
        }
    } while (curPixel <= lastPixel);

    /*return # of compressed bytes*/
    return compressedDataPtr - compressedDataStart;
}

/* HP2200 helper function to check if a scanline has any non-white pixels.
   This allows us to optimise printing by using PCL moves instead of sending
   white rasters.

   Since white is 0xFFFFFF (24 bits), there must be non-white pixels if
   there is a byte which is not 0xFF
*/
static int
IsScanlineDirty(byte* pScanline, int iWidth)
{
    byte * pCurr = pScanline;
    byte * pStop = pCurr + iWidth;

    while (pStop-pCurr)
    {
        if (*pCurr - 0xFF)
        {
            /* not 0xFF => dirty */
            return 1;
        }
        ++pCurr;
    }
    return 0;
}

#define INIT_WHITE(pBuf, uiWidth)     memset((void*)(pBuf), 0xFF, (uiWidth))

/* HP2200 output routine -------------------- */
static int
chp2200_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    gs_memory_t *mem = pdev->memory;
    int width_in_pixels = pdev->width;
    size_t width_in_bytes = width_in_pixels * 3; /* assume 24 bits (3 bytes per pixel) */
    byte *lbuf = gs_alloc_bytes(mem, width_in_bytes,
                                "(input)chp2200_print_page");
    byte *lseedbuf = gs_alloc_bytes(mem, width_in_bytes,
                                "(seed)chp2200_print_page");
    /* allocate twice the input size for worse case compressed output*/
    byte *loutputbuf = gs_alloc_bytes(mem, width_in_bytes*2,
                                "(output)chp2200_print_page");

    int lnum = 0;
    int iEmptyRows = 0;
    byte *data = lbuf;

    if ((lbuf == 0) || (lseedbuf == 0) || (loutputbuf == 0))
            return_error(gs_error_VMerror);

    /* Start Raster mode */
    (*cdj850->start_raster_mode) (pdev,
                                  gdev_pcl_paper_size((gx_device *)pdev),
                                  prn_stream);

    /* start the scanline */
    gp_fputs("\033*b", prn_stream);

    /* initialise buffers */
    INIT_WHITE(lseedbuf, width_in_bytes);

    for (lnum = 0; lnum < pdev->height; ++lnum)
    {
        int result = -1;

        /*gdev_prn_get_bits(pdev, lnum, lbuf, &data);*/
        result = gdev_prn_copy_scan_lines(pdev, lnum, data, width_in_bytes);

        if ((result == 1) && IsScanlineDirty(data, width_in_bytes))
        {
                unsigned int OutputLen = 0;

            if (iEmptyRows)
            {
                /* send vertical Y move */
                        gp_fprintf(prn_stream, "%dy", iEmptyRows);

                /* reset empty row count */
                iEmptyRows = 0;

                /* reset seed buffer */
                INIT_WHITE(lseedbuf, width_in_bytes);
            }

            OutputLen = Mode10(width_in_pixels,
                               data,
                               lseedbuf,
                               loutputbuf);

            if (OutputLen)
            {
                gp_fprintf(prn_stream, "%dw", OutputLen);
                gp_fwrite(loutputbuf, sizeof(byte), OutputLen, prn_stream);

                /* save the current scanline as the seed for the next scanline*/
                memcpy((void*)lseedbuf, (const void*)data, width_in_bytes);
            }
            else
            {
                        gp_fputs("0w", prn_stream);
            }
        }
        else
        {
            iEmptyRows++;
        }
    }

    /* terminate the scanline */
    gp_fputs("0Y", prn_stream);

    /* terminate page and eject paper */
    (*cdj850->terminate_page) (pdev, prn_stream);

    gs_free_object(mem, lbuf, "(input)chp2200_print_page");
    gs_free_object(mem, lseedbuf, "(seed)chp2200_print_page");
    gs_free_object(mem, loutputbuf, "(output)chp2200_print_page");

    return 0;
}

#define HIBYTE(w)	((byte)(((unsigned int) (w) >> 8) & 0xFF))
#define LOBYTE(w)	((byte)(w))

/* DNJ500 output routine -------------------- */
static int
cdnj500_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    byte  CRD_SeqC[]     = {0x1b, 0x2a, 0x67, 0x31, 0x32, 0x57, 0x06, 0x1F, 0x00, 0x01,
    /*                      Esc   *     |g    |# of bytes |W    |frmt |SP   |# of cmpnts*/
                            0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x20, 0x01};
    /*                      |Horz Res   |Vert Rez   |compr|orien|bits |planes*/

    /* x,y resolution for color planes, assume x=y */
    int xres = (int)cdj850->x_pixels_per_inch;
    int yres = (int)cdj850->y_pixels_per_inch;

    gs_memory_t *mem = pdev->memory;
    int width_in_pixels = pdev->width;
    size_t width_in_bytes = width_in_pixels * 3; /* assume 24 bits (3 bytes per pixel) */
    byte *lbuf = gs_alloc_bytes(mem, width_in_bytes,
                                "(input)cdnj500_print_page");
    byte *lseedbuf = gs_alloc_bytes(mem, width_in_bytes,
                                "(seed)cdnj500_print_page");
    /* allocate twice the input size for worse case compressed output*/
    byte *loutputbuf = gs_alloc_bytes(mem, width_in_bytes*2,
                                "(output)cdnj500_print_page");

    int lnum = 0;
    int iEmptyRows = 0;
    int iBlock = 0;
    bool begin = true;
    byte *data = lbuf;

    if ((lbuf == 0) || (lseedbuf == 0) || (loutputbuf == 0))
            return_error(gs_error_VMerror);

    /* Start Raster mode */
    (*cdj850->start_raster_mode) (pdev,
                                  gdev_pcl_paper_size((gx_device *)pdev),
                                  prn_stream);

    /* This will configure the raster-mode */
    CRD_SeqC[10] = HIBYTE(xres);
    CRD_SeqC[11] = LOBYTE(xres);
    CRD_SeqC[12] = HIBYTE(yres);
    CRD_SeqC[13] = LOBYTE(yres);

    for (lnum = 0; lnum < pdev->height; ++lnum)
    {
        int result = -1;

        /*gdev_prn_get_bits(pdev, lnum, lbuf, &data);*/
        result = gdev_prn_copy_scan_lines(pdev, lnum, data, width_in_bytes);

        if ((result == 1) && IsScanlineDirty(data, width_in_bytes))
        {
            unsigned int OutputLen = 0;

            /*
             * Printers with low memory (64 MB or less) can run out of memory during decompressing
             * the image data and will abort the job. To prevent this, restart raster command.
             * Raghu (hpijs)
             */
            if (iBlock == 448) /* from DesignJet 500 winNT driver */
            {
                /* terminate the scanline */
                gp_fputs("0Y", prn_stream);

                /* End graphics */
                gp_fputs("\033*rC", prn_stream);

                /* Reset in block lines counter */
                iBlock = 0;
            }
            if (iBlock == 0)
            {
                /* Send CRD */
                gp_fwrite(CRD_SeqC, sizeof(byte), sizeof(CRD_SeqC), prn_stream);

                /* Raster mode */
                gp_fputs("\033*r1A", prn_stream);

                /* start the scanline */
                gp_fputs("\033*b", prn_stream);

                /* reset seed buffer */
                INIT_WHITE(lseedbuf, width_in_bytes);
            }
            iBlock++;

            if (iEmptyRows)
            {
                /* send vertical Y move */
                        gp_fprintf(prn_stream, "%dy", iEmptyRows);

                /* reset empty row count */
                iEmptyRows = 0;

                /* reset seed buffer */
                INIT_WHITE(lseedbuf, width_in_bytes);
            }

            OutputLen = Mode10(width_in_pixels,
                               data,
                               lseedbuf,
                               loutputbuf);

            if (OutputLen)
            {
                gp_fprintf(prn_stream, "%dw", OutputLen);
                gp_fwrite(loutputbuf, sizeof(byte), OutputLen, prn_stream);

                /* save the current scanline as the seed for the next scanline*/
                memcpy((void*)lseedbuf, (const void*)data, width_in_bytes);
            }
            else
            {
                        gp_fputs("0w", prn_stream);
            }

            /* Content printing already started */
            begin = false;
        }
        else
        {
            /* Skip empty area on top page */
            if (begin == false) iEmptyRows++;
        }
    }

    /* terminate the scanline */
    gp_fputs("0Y", prn_stream);

    /* terminate page and eject paper */
    (*cdj850->terminate_page) (pdev, prn_stream);

    gs_free_object(mem, lbuf, "(input)cdnj500_print_page");
    gs_free_object(mem, lseedbuf, "(seed)cdnj500_print_page");
    gs_free_object(mem, loutputbuf, "(output)cdnj500_print_page");

    return 0;
}

#define odd(i) ((i & 01) != 0)

static int
GetScanLine(gx_device_printer * pdev, int *lnum,
            struct ptr_arrays *data_ptrs,
            struct misc_struct *misc_vars,
            word rmask)
{
    word *data_words = (word *) data_ptrs->data[misc_vars->scan];
    register word *end_data = data_words + misc_vars->line_size_words;

    ++(*lnum);
    gdev_prn_copy_scan_lines(pdev, *lnum, (byte *) data_words, misc_vars->line_size);

    misc_vars->scan = !misc_vars->scan;	/* toggle scan direction */
    misc_vars->is_two_pass = odd(*lnum);	/* color output for odd lines */

    /* Mask off 1-bits beyond the line width. */
    end_data[-1] &= rmask;

    /* Remove trailing 0s. */
    while (end_data > data_words && end_data[-1] == 0)
        end_data--;

    return end_data - data_words;
}

/* Send the scan lines to the printer */
static void
send_scan_lines(gx_device_printer * pdev,
                struct ptr_arrays *data_ptrs,
                struct misc_struct *misc_vars,
                struct error_val_field *error_values,
                const Gamma *gamma,
                gp_file * prn_stream)
{
    int lnum, lend, llen;
    int num_blank_lines = 0;

    word rmask =
    ~(word) 0 << ((-pdev->width * misc_vars->storage_bpp) & (W * 8 - 1));

    lend = (int)(pdev->height - (dev_t_margin(pdev) + dev_b_margin(pdev)) * y_dpi);

    error_values->c = error_values->m = error_values->y =
        error_values->k = 0;

    /* init the error buffer */
    init_error_buffer(misc_vars, data_ptrs);

    misc_vars->zero_row_count = 0;
    lnum = -1;
    llen = GetScanLine(pdev, &lnum, data_ptrs, misc_vars, rmask);
    while (lnum < lend) {
        num_blank_lines = 0;
        while (lnum < lend && llen == 0) {
            ++num_blank_lines;
            llen = GetScanLine(pdev, &lnum, data_ptrs, misc_vars, rmask);
        }
        if (lnum >= lend) {
            break;
        }
        /* Skip blank lines if any */
        if (num_blank_lines > 0) {
            gp_fprintf(prn_stream, "\033*b%dY", num_blank_lines / (cdj850->yscal + 1));
            memset(data_ptrs->plane_data[0][0], 0,
                   (misc_vars->plane_size * 2 * misc_vars->num_comps));
            memset(data_ptrs->plane_data_c[0][0], 0,
                   (misc_vars->plane_size_c * 2 * misc_vars->num_comps));

        }
        /* all blank lines printed, now for the non-blank lines */
        if (cdj850->yscal && odd(lnum)) {
            /* output a blank black plane for odd lines */
            gp_fprintf(prn_stream, "\033*b0V");
        }
        /* now output all non blank lines */
        while (lnum < lend && llen != 0) {
            misc_vars->is_color_data = 0;	/* maybe we have color ? */
            (*cdj850->print_non_blank_lines) (pdev, data_ptrs, misc_vars,
                                              error_values, gamma, prn_stream);
            llen = GetScanLine(pdev, &lnum, data_ptrs, misc_vars, rmask);
        }
        if (cdj850->yscal && odd(lnum)) {	/* output empty line for odd lines */
            (*cdj850->print_non_blank_lines) (pdev, data_ptrs, misc_vars,
                                              error_values, gamma, prn_stream);
        }
        /* the current line is empty => run the next iteration */
    }
}

/* print_line compresses (mode 9) and outputs one plane */
static void
print_c9plane(gp_file * prn_stream, char plane_code, int plane_size,
              const byte * curr, const byte * prev, byte * out_data)
{
    /* Compress the output data */
    int out_count = gdev_pcl_mode9compress(plane_size, curr, prev, out_data);

    /* and output the data */
        gp_fprintf(prn_stream, "%d%c", out_count, plane_code);
    if (out_count > 0) {
        gp_fwrite(out_data, sizeof(byte), out_count, prn_stream);
    }
}

/*  print_c2plane()
 *
 *  Compresses a single plane with mode 2 (TIFF 4 format) and sends the
 *  result to the output.
 */
static void
print_c2plane(gp_file *prn_stream, char plane_code, int plane_size,
              const byte *curr, byte *out_data)
{
  /*
   *  Compress the output data
   */
  int out_count = gdev_pcl_mode2compress((const word *)curr,
                  (const word *)(curr +plane_size -2), out_data);

  /*
   *  Send it out
   */
  gp_fprintf(prn_stream, "%d%c", out_count, plane_code);
  if (out_count > 0)
      gp_fwrite(out_data, sizeof(byte), out_count, prn_stream);
}

#ifdef UNUSED
/*  print_c0plane()
 *
 *  Outputs a plane with no compression.
 */
static void
print_c0plane(gp_file *prn_stream, char plane_code, int plane_size,
              const byte *curr, byte *out_data)
{
  gp_fprintf(prn_stream, "%d%c", plane_size, plane_code);
  if (plane_size > 0)
    gp_fwrite(curr, sizeof(byte), plane_size, prn_stream);
}
#endif

/* Printing non-blank lines */
static void
cdj850_print_non_blank_lines(gx_device_printer * pdev,
                             struct ptr_arrays *data_ptrs,
                             struct misc_struct *misc_vars,
                             struct error_val_field *error_values,
                             const Gamma *gamma,
                             gp_file * prn_stream)
{
    static const char *const plane_code[2] =
    {"wvvv", "vvvv"};

    int i;
    byte *kP = data_ptrs->plane_data[misc_vars->scan + 2][3];
    byte *dp = data_ptrs->data[misc_vars->scan + 2];
    int *ep = data_ptrs->errors[misc_vars->scan];

    /* we need cmyk color separation befor all the rest, since
       black may be contained in the color fields. This needs to
       be done on all pixel-rows, since even unused color-bytes
       might generate black */

    misc_vars->is_color_data =
        do_gcr(misc_vars->databuff_size, data_ptrs->data[misc_vars->scan],
               gamma->k, gamma->c, gamma->m, gamma->y, gamma->correct,
               (word *) data_ptrs->data[misc_vars->scan]);

    /* dithering the black-plane */
    FSDlinebw(misc_vars->scan, misc_vars->plane_size,
              error_values, kP, misc_vars->num_comps, ep, dp);

    /* output the black plane */
    print_c9plane(prn_stream, 'v', misc_vars->plane_size,
                  data_ptrs->plane_data[misc_vars->scan][3],
                  data_ptrs->plane_data[1 - misc_vars->scan][3],
                  data_ptrs->out_data);

    /* since color resolution is only half of the b/w-resolution,
       we only output every second row */
    if (!cdj850->yscal || misc_vars->is_two_pass) {

        int plane_size_c = (*rescale_color_plane[cdj850->xscal][cdj850->yscal])
        (misc_vars->databuff_size,
         data_ptrs->data[misc_vars->scan],
         data_ptrs->data[!misc_vars->scan],
         data_ptrs->data_c[misc_vars->cscan]) / misc_vars->storage_bpp;

        /* dither the color planes */
        do_floyd_steinberg(misc_vars->scan, misc_vars->cscan,
                           misc_vars->plane_size, plane_size_c,
                           misc_vars->num_comps, data_ptrs, pdev, error_values);

        /* Transfer raster graphics in the order C, M, Y, that is
           planes 2,1,0 */
        for (i = misc_vars->num_comps - 2; i >= 0; i--) {

            /* output the lower color planes */
            print_c9plane(prn_stream, plane_code[cdj850->intensities > 2][i],
                          plane_size_c,
                          data_ptrs->plane_data_c[misc_vars->cscan][i],
                          data_ptrs->plane_data_c[1 - misc_vars->cscan][i],
                          data_ptrs->out_data);

            /* output the upper color planes */
            if (cdj850->intensities > 2) {
                print_c9plane(prn_stream, plane_code[0][i], plane_size_c,
                              data_ptrs->plane_data_c[misc_vars->cscan][i + 4],
                              data_ptrs->plane_data_c[1 -
                                                                            misc_vars->cscan][i
                                                                            + 4],
                              data_ptrs->out_data);
            }			/* end cdj850->intensities > 2 */
        }			/* End For i = num_comps */
        misc_vars->cscan = 1 - misc_vars->cscan;
    }				/* End of is_two_pass */
    return;
}

/* Printing non-blank lines */
static void
cdj880_print_non_blank_lines(gx_device_printer * pdev,
                             struct ptr_arrays *data_ptrs,
                             struct misc_struct *misc_vars,
                             struct error_val_field *error_values,
                             const Gamma *gamma,
                             gp_file * prn_stream)
{
    static const char *const plane_code[2] =
    {"WVVV", "VVVV"};

    int i;
    byte *kP = data_ptrs->plane_data[misc_vars->scan + 2][3];
    byte *dp = data_ptrs->data[misc_vars->scan + 2];
    int *ep = data_ptrs->errors[misc_vars->scan];

    /* we need cmyk color separation befor all the rest, since
       black may be contained in the color fields. This needs to
       be done on all pixel-rows, since even unused color-bytes
       might generate black */

    misc_vars->is_color_data =
        do_gcr(misc_vars->databuff_size, data_ptrs->data[misc_vars->scan],
               gamma->k, gamma->c, gamma->m, gamma->y, gamma->correct,
               (word *) data_ptrs->data[misc_vars->scan]);

    /* dithering the black-plane */
    FSDlinebw(misc_vars->scan, misc_vars->plane_size,
              error_values, kP, misc_vars->num_comps, ep, dp);

    /* output the black plane */
    gp_fputs("\033*b", prn_stream);
    print_c2plane(prn_stream, 'V', misc_vars->plane_size,
                  data_ptrs->plane_data[misc_vars->scan][3],
/*		  data_ptrs->plane_data[1 - misc_vars->scan][3],*/
                  data_ptrs->out_data);
/*    gp_fputs("\033*b0V", prn_stream);*/

    /* since color resolution is only half of the b/w-resolution,
       we only output every second row */
    if (!cdj850->yscal || misc_vars->is_two_pass) {

        int plane_size_c = (*rescale_color_plane[cdj850->xscal][cdj850->yscal])
        (misc_vars->databuff_size,
         data_ptrs->data[misc_vars->scan],
         data_ptrs->data[!misc_vars->scan],
         data_ptrs->data_c[misc_vars->cscan]) / misc_vars->storage_bpp;

        /* dither the color planes */
        do_floyd_steinberg(misc_vars->scan, misc_vars->cscan,
                           misc_vars->plane_size, plane_size_c,
                           misc_vars->num_comps, data_ptrs, pdev, error_values);

        /* Transfer raster graphics in the order C, M, Y, that is
           planes 2,1,0 */
        for (i = misc_vars->num_comps - 2; i >= 0; i--) {

            /* output the lower color planes */
      gp_fputs("\033*b", prn_stream);
            print_c2plane(prn_stream, plane_code[cdj850->intensities > 2][i],
                          plane_size_c,
                          data_ptrs->plane_data_c[misc_vars->cscan][i],
/*			  data_ptrs->plane_data_c[1 - misc_vars->cscan][i],*/
                          data_ptrs->out_data);

            /* output the upper color planes */
            if (cdj850->intensities > 2) {
    gp_fputs("\033*b", prn_stream);
                print_c2plane(prn_stream, plane_code[0][i], plane_size_c,
                              data_ptrs->plane_data_c[misc_vars->cscan][i + 4],
/*			      data_ptrs->plane_data_c[1 -
                                                    misc_vars->cscan][i
                                                    + 4],*/
                              data_ptrs->out_data);
            }			/* end cdj850->intensities > 2 */
        }			/* End For i = num_comps */
        misc_vars->cscan = 1 - misc_vars->cscan;
    }				/* End of is_two_pass */
    return;
}

/* moved that code into his own subroutine, otherwise things get
   somewhat clumsy */
static void
do_floyd_steinberg(int scan, int cscan, int plane_size,
                   int plane_size_c, int n,
                   struct ptr_arrays *data_ptrs,
                   gx_device_printer * pdev,
                   struct error_val_field *error_values)
{
    /* the color pointers */
    byte *cPa, *mPa, *yPa, *cPb, *mPb, *yPb;
    byte *dpc;
    int *epc;

    /* the color pointers, lower byte */
    cPa = data_ptrs->plane_data_c[cscan + 2][2];
    mPa = data_ptrs->plane_data_c[cscan + 2][1];
    yPa = data_ptrs->plane_data_c[cscan + 2][0];
    /* upper byte */
    cPb = data_ptrs->plane_data_c[cscan + 2][6];
    mPb = data_ptrs->plane_data_c[cscan + 2][5];
    yPb = data_ptrs->plane_data_c[cscan + 2][4];
    /* data and error */
    dpc = data_ptrs->data_c[cscan + 2];
    epc = data_ptrs->errors_c[cscan];

    switch (cdj850->intensities) {
        case 2:
        FSDlinec2(cscan, plane_size_c, error_values,
                  cPa, mPa, yPa, n, dpc, epc);
        break;
        case 3:
        FSDlinec3(cscan, plane_size_c, error_values,
                  cPa, mPa, yPa, cPb, mPb, yPb, n, dpc, epc);
        break;
        case 4:
        FSDlinec4(cscan, plane_size_c, error_values,
                  cPa, mPa, yPa, cPb, mPb, yPb, n, dpc, epc);
        break;
        default:
        assert(0);
    }
    return;
}

#ifdef UNUSED
/* here we do our own gamma-correction */
static void
do_gamma(float mastergamma, float gammaval, byte values[256])
{
    int i;
    float gamma;

    if (gammaval > 0.0) {
        gamma = gammaval;
    } else {
        gamma = mastergamma;
    }

    for (i = 0; i < 256; i++) {
        values[i] = (byte) (255.0 *
                            (1.0 - pow(((double)(255.0 - (float)i) / 255.0),
                             (double)(1.0 / gamma))));
    }

    return;
}
#endif

/* here we calculate a lookup-table which is used to compensate the
   relative loss of color due to undercolor-removal */
static void
do_black_correction(float kvalue, int kcorrect[256])
{
    int i;

    for (i = 0; i < 256; i++) {
      kcorrect[i] = 0;
#if 0
        kcorrect[i] = (int)
            (100.0 * kvalue * (
                                  pow(10.0,
                                      pow((i / 255.0), 3.0)
                                  )
                                  - 1.0
             )
            );
#endif /* 0 */
    }

    return;
}

/* For Better Performance we use a macro here */
#define DOUCR(col1, col2, col3, col4)\
{\
  /* determine how far we are from the grey axis. This is  */\
  /* traditionally done by computing MAX(CMY)-MIN(CMY).    */\
  /* However, if two colors are very similar, we could     */\
  /* as either CMYRGB and K. Therefore we calculate the    */\
  /* the distance col1-col2 and col2-col3, and use the     */\
  /* smaller one.                                          */\
  a = *col1 - *col2;\
  b = *col2 - *col3;\
  if (a >= b) {\
    grey_distance = 1.0 - (b/255.0);\
  } else {\
    grey_distance = 1.0 - (a/255.0);\
  }\
  ucr   = (byte) (*col3 * grey_distance); \
  *col4 = *col4 + ucr;  /* add removed black to black */\
  /* remove only as much color as black is surviving the   */\
  /* gamma correction */\
  ucr   = *(kvalues + ucr);\
  *col1 = *col1 - ucr ;\
  *col2 = *col2 - ucr ;\
  *col3 = *col3 - ucr ;\
}

/* For Better Performance we use a macro here */
#define DOGCR(col1, col2, col3, col4)\
{\
  int ucr = (int) *col3;\
  *col1 -= ucr ;\
  *col2 -= ucr ;\
  *col3 -= ucr ;\
  *col4 += ucr;  /* add removed black to black */\
  kadd  = ucr + *(kcorrect + ucr);\
  uca_fac = 1.0 + (kadd/255.0);\
  *col1 *= uca_fac;\
  *col2 *= uca_fac;\
}

#define NOBLACK(col1, col2, col3, col4)\
{\
  ucr = (int) *col4;\
  *col1 += ucr;\
  *col2 += ucr;\
  *col3 += ucr;\
  *col4 = 0;\
}

/* Since resolution can be different on different planes, we need to
   do real color separation, here we try a real grey component
   replacement */
static int
do_gcr(int bytecount, byte * inbyte, const byte kvalues[256],
       const byte cvalues[256], const byte mvalues[256],
       const byte yvalues[256], const int kcorrect[256],
       word * inword)
{
  int i, ucr, is_color = 0;
  byte *black, *cyan, *magenta, *yellow;
  word last_color_value = 0;
  word *last_color;

  /* initialise *last_color with a dummmy value */
  last_color = &last_color_value;
  /* Grey component replacement */
  for (i = 0; i < bytecount; i += 4) {

    /* Assign to black the current address of  inbyte */
    black = inbyte++;
    cyan = inbyte++;
    magenta = inbyte++;
    yellow = inbyte++;

    if (*black > 0)
      NOBLACK(cyan, magenta, yellow, black);

    if (*magenta + *yellow + *cyan > 0) {	/* if any color at all */

#if 0
      if ((*cyan > 0) && (*magenta > 0) && (*yellow > 0))
      {
        char output[255];
        gs_snprintf(output, sizeof(output), "%3d %3d %3d %3d - ", *cyan, *magenta, *yellow, *black);
        debug_print_string(output, strlen(output));
      }
#endif /* 0 */

      is_color = 1;

      /* Test whether we 've already computet the value */
      if (*inword == last_color_value) {
        /* save a copy of the current color before it will be modified */
        last_color_value = *inword;
/*	debug_print_string("\n", 1);*/
        /* copy the result of the old value onto the new position */
        *inword = *last_color;
      } else {
        /* save a copy of the current color before it will be modified */
        last_color_value = *inword;
  NOBLACK(cyan, magenta, yellow, black);
        if ((*cyan >= *magenta)
            && (*magenta >= *yellow)
            && (*yellow > 0)) {	/* if any grey component */
          NOBLACK(cyan, magenta, yellow, black);
        } else if ((*cyan >= *yellow)
                   && (*yellow >= *magenta)
                   && (*magenta > 0)) {
          NOBLACK(cyan, yellow, magenta, black);
        } else if ((*yellow >= *magenta)
                   && (*magenta >= *cyan)
                   && (*cyan > 0)) {
          NOBLACK(yellow, magenta, cyan, black);
        } else if ((*yellow >= *cyan)
                   && (*cyan >= *magenta)
                   && (*magenta > 0)) {
          NOBLACK(yellow, cyan, magenta, black);
        } else if ((*magenta >= *yellow)
                   && (*yellow >= *cyan)
                   && (*cyan > 0)) {
          NOBLACK(magenta, yellow, cyan, black);
        } else if ((*magenta >= *cyan)
                   && (*cyan >= *yellow)
                   && (*yellow > 0)) {
          NOBLACK(magenta, cyan, yellow, black);
        } else {		/* do gamma only if no black */
        }
#if 0
        if (ucr > 0)
        {
          char output[255];
          gs_snprintf(output, sizeof(output), "%3d %3d %3d %3d - %5d\n", *cyan, *magenta, *yellow, *black, ucr);
          debug_print_string(output, strlen(output));
        }
#endif /* 0 */
        if (   *cyan > 255)    *cyan = 255;
        if (*magenta > 255) *magenta = 255;
        if ( *yellow > 255)  *yellow = 255;

        *cyan = *(cvalues + *cyan);
        *magenta = *(mvalues + *magenta);
        *yellow = *(yvalues + *yellow);
        last_color =  inword; /* save pointer */
      }/* end current_color */
    }			/* end of if c+m+y > 0 */
    *black = *(kvalues + *black);
    inword = inword + 1;
  } /* end of for bytecount */
  return is_color;
}

/* Since resolution can be different on different planes, we need to
   rescale the data byte by byte */
static int
rescale_byte_wise2x2(int bytecount, const byte * inbytea, const byte * inbyteb,
                     byte * outbyte)
{
    register int i, j;
    int max = bytecount / 2;

    for (i = 0; i < max; i += 4) {
        j = 2 * i;
        /* cyan */
        outbyte[i + 1] = (inbytea[j + 1] + inbytea[j + 5] + inbyteb[j + 1] +
                          inbyteb[j + 5]) / 4;
        /* magenta */
        outbyte[i + 2] = (inbytea[j + 2] + inbytea[j + 6] + inbyteb[j + 2] +
                          inbyteb[j + 6]) / 4;
        /* yellow */
        outbyte[i + 3] = (inbytea[j + 3] + inbytea[j + 7] + inbyteb[j + 3] +
                          inbyteb[j + 7]) / 4;
    }
    return max;
}

/* Since resolution can be different on different planes, we need to
   rescale the data byte by byte */
static int
rescale_byte_wise2x1(int bytecount, const byte * inbytea, const byte * inbyteb,
                     byte * outbyte)
{
    register int i, j;
    int max = bytecount / 2;

    for (i = 0; i < max; i += 4) {
        j = 2 * i;
        /* cyan */
        outbyte[i + 1] = (inbytea[j + 1] + inbytea[j + 5]) / 2;
        /* magenta */
        outbyte[i + 2] = (inbytea[j + 2] + inbytea[j + 6]) / 2;
        /* yellow */
        outbyte[i + 3] = (inbytea[j + 3] + inbytea[j + 7]) / 2;
    }
    return max;
}

/* Since resolution can be different on different planes, we need to
   rescale the data byte by byte */
static int
rescale_byte_wise1x2(int bytecount, const byte * inbytea, const byte * inbyteb,
                     byte * outbyte)
{
    register int i;

    for (i = 0; i < bytecount; i += 4) {
        /* cyan */
        outbyte[i + 1] = (inbytea[i + 1] + inbyteb[i + 1]) / 2;
        /* magenta */
        outbyte[i + 2] = (inbytea[i + 2] + inbyteb[i + 2]) / 2;
        /* yellow */
        outbyte[i + 3] = (inbytea[i + 3] + inbyteb[i + 3]) / 2;
    }
    return bytecount;
}

/* Since resolution can be different on different planes, we need to
   rescale the data byte by byte */
static int
rescale_byte_wise1x1(int bytecount, const byte * inbytea, const byte * inbyteb,
                     byte * outbyte)
{
    register int i;

    for (i = 0; i < bytecount; i += 4) {
        /* cyan */
        outbyte[i + 1] = inbytea[i + 1];
        /* magenta */
        outbyte[i + 2] = inbytea[i + 2];
        /* yellow */
        outbyte[i + 3] = inbytea[i + 3];
    }
    return bytecount;
}

/* MACROS FOR DITHERING (we use macros for compact source and faster code) */
/* Floyd-Steinberg dithering. Often results in a dramatic improvement in
 * subjective image quality, but can also produce dramatic increases in
 * amount of printer data generated and actual printing time!! Mode 9 2D
 * compression is still useful for fairly flat colour or blank areas but its
 * compression is much less effective in areas where the dithering has
 * effectively randomised the dot distribution. */

#define SHIFT  ((I * 8) - 13)
#define C 8

#define BLACKOFFSET (128 << SHIFT) /* distance from min to max */
#define THRESHOLD   (128 << SHIFT)
#define MAXVALUE    (THRESHOLD + BLACKOFFSET)

/* -64 to 64 */
#define RANDOM ((rand() << SHIFT) % THRESHOLD - (THRESHOLD >> 1))

/* --- needed for the hp850 color -- */
/*

Mode     Planes  Intensity
Low        A        1.0
Medium     B        1.5
High      A&B       2.0

Note that planes A&B is not the sum of the intensities of plane A and plane B.

Range      Planes    Max Intensity
0-63       A < 50%        0.5
64-127     A > 50%        1.0
128-191    B > 50%        1.5
192-255    A&B > 50%      2.0

Note some specific values:
Value      Planes     Intensity
0             0          0.0
32          25% A        0.25
64          50% A        0.5
128         100% A       1.0
129      99% A, 1% B     1.01
192         100% B       1.5
255         100% C       2.0

Keep in mind that only the range 0-63 has a pixel rate <50%.  All others
have a pixel rate >50%.

*/

#define COLOROFFSET ( 64 << SHIFT) /* distance between color intensities */

#define THRESHOLDS  ( 64 << SHIFT) /* intensity A */
#define THRESHOLDM  (128 << SHIFT) /* intensity B */
#define THRESHOLDL  (192 << SHIFT) /* intensities A & B */

#define MAXVALUES  (THRESHOLDS + COLOROFFSET)
#define MAXVALUEM  (THRESHOLDM + COLOROFFSET)
#define MAXVALUEL  (THRESHOLDL + COLOROFFSET)

#define CRANDOM ((rand() << SHIFT) % THRESHOLDS - (THRESHOLDS >> 1))

/* --------------------------- */

/* initialise the error_buffer */
static void
init_error_buffer(struct misc_struct * misc_vars,
                  struct  ptr_arrays * data_ptrs)
{
  int i;
  int *ep;
  int *epc;

  ep = data_ptrs->errors[0];
  epc = data_ptrs->errors_c[0];

  if (misc_vars->bits_per_pixel > 4) { /* Randomly seed initial error
                                          buffer */
    /* Otherwise, the first dithered rows would look rather uniform */
    for (i = 0; i < misc_vars->databuff_size; i++) { /* 600dpi planes */
      *ep++ = RANDOM;
    }

    /* Now for the 2 * 300dpi color planes */
    for (i = 0; i < misc_vars->databuff_size_c; i++) {
      *epc++ = CRANDOM;
    }
  }
  return;
}

#define FSdither(inP, out, errP, Err, Bit, Offset, Element)\
{\
   oldErr = Err;\
        Err = (*(errP + Element)\
               + ((Err * 7 + C) >> 4)\
               + ((int)*(inP + Element) << SHIFT));\
        if (Err > THRESHOLD || *(inP + Element) == 255 /* b/w optimization */) {\
          out |= Bit;\
          Err -= MAXVALUE;\
        }\
        *(errP + (Element + Offset)) += ((Err * 3 + C) >> 4);\
        *(errP + Element) = ((Err * 5 + oldErr + C) >> 4);\
}

/*  FSDlinebw()
 *
 *  Floyd-steinberg dithering for the black plane.  The 850C has 600dpi
 *  black and 300dpi color, but the 880C and siblings have 600dpi black
 *  AND color.  Therefore, we need an adapted dither algorithm.
 *
 *  Inputs:  scan
 *
 *           plane_size
 *
 *           error_values
 *
 *           kP
 *
 *           n
 *
 *           ep
 *
 *           dp
 *
 *  Returns: Nothing.
 */
static void
FSDlinebw(int scan, int plane_size,
          struct error_val_field * error_values,
          byte * kP, int n, int * ep, byte * dp)
{
  if (scan == 0) {       /* going_up */
    byte k, bitmask; /* k = outbyte byte, whereas bitmask defines the
                        bit to be set within k */
    int oldErr, i;

    for (i = 0; i < plane_size; i++) {
      bitmask = 0x80;
      for (k = 0; bitmask != 0; bitmask >>= 1) {
        /* dp points to the first word of the input data which is in
           kcmy-format */
        /* k points to the beginning of the first outbut byte, which
           is filled up, bit by bit while looping over bytemask */
        /* ep points to the first word of the error-plane which
           contains the errors kcmy format */
        /* err_values->k tempararily holds the error-value */
        /* bitmask selects the bit to be set in the outbyte */
        /* n gives the offset for the byte selection within
           words. With simple cmyk-printing, this should be 4 */
        /* 0 points to the active color within the input-word, i.e. 0
           = black, 1 = cyan, 2 = yellow, 3 = magenta */

        FSdither(dp, k, ep, error_values->k, bitmask, -n, 0);
        dp += n, ep += n; /* increment the input and error pointer one
                             word (=4 byte) further, in order to
                             convert the next word into an bit */
      }
      *kP++ = k; /* fill the output-plane byte with the computed byte
                    and increment the output plane pointer  one byte */
    }

  } else {		/* going_down */
    byte k, bitmask;
    int oldErr, i;
    for (i = 0; i < plane_size; i++) {
      bitmask = 0x01;
      for (k = 0; bitmask != 0; bitmask <<= 1) {
        dp -= n, ep -= n;
        FSdither(dp, k, ep, error_values->k, bitmask, n, 0);
      }
      *--kP = k;
    }
  }
  return;
}

/* Since bw has already been dithered for the hp850c, we need
   an adapted dither algorythm */
static void
FSDlinec2(int scan, int plane_size,
          struct error_val_field *error_values,
          byte * cPa, byte * mPa, byte * yPa, int n,
          byte * dp, int *ep)
{
    if (scan == 0) {		/* going_up */
        int oldErr, i;
        byte ca, ya, ma, bitmask;

        for (i = 0; i < plane_size; i++) {
            bitmask = 0x80;
            ca = ya = ma = 0;
            for (ca = 0; bitmask != 0; bitmask >>= 1) {
                FSdither(dp, ca, ep, error_values->c, bitmask, -n, n - 3);
                FSdither(dp, ma, ep, error_values->m, bitmask, -n, n - 2);
                FSdither(dp, ya, ep, error_values->y, bitmask, -n, n - 1);
                dp += n, ep += n;
            }
            *cPa++ = ca;
            *mPa++ = ma;
            *yPa++ = ya;
        }

    } else {			/* going_down */
        byte ca, ya, ma, bitmask;
        int oldErr, i;

        for (i = 0; i < plane_size; i++) {
            bitmask = 0x01;
            ca = ya = ma = 0;
            for (ca = 0; bitmask != 0; bitmask <<= 1) {
                dp -= n, ep -= n;
                FSdither(dp, ya, ep, error_values->y, bitmask, n, n - 1);
                FSdither(dp, ma, ep, error_values->m, bitmask, n, n - 2);
                FSdither(dp, ca, ep, error_values->c, bitmask, n, n - 3);
            }
            *--yPa = ya;
            *--mPa = ma;
            *--cPa = ca;
        }
    }
    return;
}

/* while printing on paper, we only use 3 -intensities */
#define FSdither8503(inP, outa, outb, errP, Err, Bit, Offset, Element)\
{\
        oldErr = Err;\
        Err = (*(errP + Element)\
               + ((Err * 7 + C) >> 4)\
               + ((int) *(inP + Element) << SHIFT));\
        if ((Err > THRESHOLDS) && (Err <= THRESHOLDM)) {\
          outa |= Bit;\
          Err -= MAXVALUES;\
        }\
        if (Err > THRESHOLDM) {\
          outb |= Bit;\
          Err -= MAXVALUEM;\
        }\
        *(errP + (Element + Offset)) += ((Err * 3 + C) >> 4);\
        *(errP + Element) = ((Err * 5 + oldErr + C) >> 4);\
}

/* On ordinary paper, we'll only use 3 intensities with the hp850  */
static void
FSDlinec3(int scan, int plane_size,
          struct error_val_field *error_values,
          byte * cPa, byte * mPa, byte * yPa,
          byte * cPb, byte * mPb, byte * yPb,
          int n, byte * dp, int *ep)
{
    if (scan == 0) {		/* going_up */
        byte ca, ya, ma, cb, yb, mb, bitmask;
        int oldErr, i;

        for (i = 0; i < plane_size; i++) {
            bitmask = 0x80;
            ca = ya = ma = cb = yb = mb = 0;
            for (ca = 0; bitmask != 0; bitmask >>= 1) {
                FSdither8503(dp, ca, cb, ep, error_values->c, bitmask, -n, n
                             - 3);
                FSdither8503(dp, ma, mb, ep, error_values->m, bitmask, -n, n
                             - 2);
                FSdither8503(dp, ya, yb, ep, error_values->y, bitmask, -n, n
                             - 1);
                dp += n, ep += n;
            }
            *cPa++ = ca;
            *mPa++ = ma;
            *yPa++ = ya;
            *cPb++ = cb;
            *mPb++ = mb;
            *yPb++ = yb;
        }
    } else {			/* going_down */
        byte ca, ya, ma, cb, yb, mb, bitmask;
        int oldErr, i;

        for (i = 0; i < plane_size; i++) {
            bitmask = 0x01;
            ca = ya = ma = cb = yb = mb = 0;
            for (ca = 0; bitmask != 0; bitmask <<= 1) {
                dp -= n, ep -= n;
                FSdither8503(dp, ya, yb, ep, error_values->y, bitmask, n, n
                             - 1);
                FSdither8503(dp, ma, mb, ep, error_values->m, bitmask, n, n
                             - 2);
                FSdither8503(dp, ca, cb, ep, error_values->c, bitmask, n, n
                             - 3);
            }
            *--yPa = ya;
            *--mPa = ma;
            *--cPa = ca;
            *--yPb = yb;
            *--mPb = mb;
            *--cPb = cb;
        }
    }
    return;
}

/* the hp850 knows about 4 different color intensities per color */
#define FSdither8504(inP, outa, outb, errP, Err, Bit, Offset, Element)\
{\
        oldErr = Err;\
        Err = (*(errP + Element)\
               + ((Err * 7 + C) >> 4)\
               + ((int) *(inP + Element) << SHIFT));\
        if ((Err > THRESHOLDS) && (Err <= THRESHOLDM)) {\
          outa |= Bit;\
          Err -= MAXVALUES;\
        }\
        if ((Err > THRESHOLDM) && (Err <= THRESHOLDL)) {\
          outb |= Bit;\
          Err -= MAXVALUEM;\
        }\
        if (Err > THRESHOLDL) {\
          outa |= Bit;\
          outb |= Bit;\
          Err -= MAXVALUEL;\
        }\
        *(errP + (Element + Offset)) += ((Err * 3 + C) >> 4);\
        *(errP + Element) = ((Err * 5 + oldErr + C) >> 4);\
}

/* The hp850c knows about 4 intensity levels per color. Once more, we need
   an adapted dither algorythm */
static void
FSDlinec4(int scan, int plane_size,
          struct error_val_field *error_values,
          byte * cPa, byte * mPa, byte * yPa,
          byte * cPb, byte * mPb, byte * yPb,
          int n, byte * dp, int *ep)
{
    if (scan == 0) {		/* going_up */
        byte ca, ya, ma, cb, yb, mb, bitmask;
        int oldErr, i;

        for (i = 0; i < plane_size; i++) {
            bitmask = 0x80;
            ca = ya = ma = cb = yb = mb = 0;
            for (ca = 0; bitmask != 0; bitmask >>= 1) {
                FSdither8504(dp, ca, cb, ep, error_values->c, bitmask, -n, n
                             - 3);
                FSdither8504(dp, ma, mb, ep, error_values->m, bitmask, -n, n
                             - 2);
                FSdither8504(dp, ya, yb, ep, error_values->y, bitmask, -n, n
                             - 1);
                dp += n, ep += n;
            }
            *cPa++ = ca;
            *mPa++ = ma;
            *yPa++ = ya;
            *cPb++ = cb;
            *mPb++ = mb;
            *yPb++ = yb;
        }
    } else {			/* going_down */
        byte ca, ya, ma, cb, yb, mb, bitmask;
        int oldErr, i;

        for (i = 0; i < plane_size; i++) {
            bitmask = 0x01;
            ca = ya = ma = cb = yb = mb = 0;
            for (ca = 0; bitmask != 0; bitmask <<= 1) {
                dp -= n, ep -= n;
                FSdither8504(dp, ya, yb, ep, error_values->y, bitmask, n, n
                             - 1);
                FSdither8504(dp, ma, mb, ep, error_values->m, bitmask, n, n
                             - 2);
                FSdither8504(dp, ca, cb, ep, error_values->c, bitmask, n, n
                             - 3);
            }
            *--yPa = ya;
            *--mPa = ma;
            *--cPa = ca;
            *--yPb = yb;
            *--mPb = mb;
            *--cPb = cb;
        }
    }
    return;
}

/* calculate the needed memory */
static void
calculate_memory_size(gx_device_printer * pdev,
                      struct misc_struct *misc_vars)
{
    int xfac = cdj850->xscal ? 2 : 1;

    misc_vars->line_size = gdev_prn_raster(pdev);
    misc_vars->line_size_c = misc_vars->line_size / xfac;
    misc_vars->line_size_words = (misc_vars->line_size + W - 1) / W;
    misc_vars->paper_size = gdev_pcl_paper_size((gx_device *) pdev);
    misc_vars->num_comps = pdev->color_info.num_components;
    misc_vars->bits_per_pixel = pdev->color_info.depth;
    misc_vars->storage_bpp = misc_vars->num_comps * 8;
    misc_vars->expanded_bpp = misc_vars->num_comps * 8;
    misc_vars->errbuff_size = 0;
    misc_vars->errbuff_size_c = 0;

    misc_vars->plane_size = calc_buffsize(misc_vars->line_size, misc_vars->storage_bpp);

    /* plane_size_c is dependedend on the bits used for
       dithering. Currently 2 bits are sufficient  */
    misc_vars->plane_size_c = 2 * misc_vars->plane_size / xfac;

    /* 4n extra values for line ends */
    /* might be wrong, see gdevcdj.c */
    misc_vars->errbuff_size =
        calc_buffsize((misc_vars->plane_size * misc_vars->expanded_bpp +
                       misc_vars->num_comps * 4) * I, 1);

    /* 4n extra values for line ends */
    misc_vars->errbuff_size_c =
        calc_buffsize((misc_vars->plane_size_c / 2 * misc_vars->expanded_bpp
                       + misc_vars->num_comps * 4) * I, 1);

    misc_vars->databuff_size =
        misc_vars->plane_size * misc_vars->storage_bpp;

    misc_vars->databuff_size_c =
        misc_vars->plane_size_c / 2 * misc_vars->storage_bpp;

    misc_vars->outbuff_size = misc_vars->plane_size * 4;

    misc_vars->storage_size_words = (((misc_vars->plane_size)
                                      * 2
                                      * misc_vars->num_comps)
                                     + misc_vars->databuff_size
                                     + misc_vars->errbuff_size
                                     + misc_vars->outbuff_size
                                     + ((misc_vars->plane_size_c)
                                        * 2
                                        * misc_vars->num_comps)
                                     + misc_vars->databuff_size_c
                                     + misc_vars->errbuff_size_c
                                     + (4 * misc_vars->plane_size_c))
        / W;

    return;
}

/* Initialise the needed pointers */
static void
init_data_structure(gx_device_printer * pdev,
                    struct ptr_arrays *data_ptrs,
                    struct misc_struct *misc_vars)
{
    int i;
    byte *p = (byte *) data_ptrs->storage;

    misc_vars->scan = 0;
    misc_vars->cscan = 0;
    misc_vars->is_two_pass = 0;

    /* the b/w pointer */
    data_ptrs->data[0] = data_ptrs->data[1] = data_ptrs->data[2] = p;
    data_ptrs->data[3] = p + misc_vars->databuff_size;
    /* Note: The output data will overwrite part of the input-data */

    if (misc_vars->bits_per_pixel > 1) {
        p += misc_vars->databuff_size;
    }
    if (misc_vars->bits_per_pixel > 4) {
        data_ptrs->errors[0] = (int *)p + misc_vars->num_comps * 2;
        data_ptrs->errors[1] = data_ptrs->errors[0] + misc_vars->databuff_size;
        p += misc_vars->errbuff_size;
    }
    for (i = 0; i < misc_vars->num_comps; i++) {
        data_ptrs->plane_data[0][i] = data_ptrs->plane_data[2][i] = p;
        p += misc_vars->plane_size;
    }
    for (i = 0; i < misc_vars->num_comps; i++) {
        data_ptrs->plane_data[1][i] = p;
        data_ptrs->plane_data[3][i] = p + misc_vars->plane_size;
        p += misc_vars->plane_size;
    }
    data_ptrs->out_data = p;
    p += misc_vars->outbuff_size;

    /* ---------------------------------------------------------
       now for the color pointers
       --------------------------------------------------------- */

    data_ptrs->data_c[0] = data_ptrs->data_c[1] = data_ptrs->data_c[2] = p;
    data_ptrs->data_c[3] = p + misc_vars->databuff_size_c;
    /* Note: The output data will overwrite part of the input-data */

    if (misc_vars->bits_per_pixel > 1) {
        p += misc_vars->databuff_size_c;
    }
    if (misc_vars->bits_per_pixel > 4) {
        data_ptrs->errors_c[0] = (int *)p + misc_vars->num_comps * 2;
        data_ptrs->errors_c[1] = data_ptrs->errors_c[0] + misc_vars->databuff_size_c;
        p += misc_vars->errbuff_size_c;
    }
    /* pointer for the lower bits of the output data */
    for (i = 0; i < misc_vars->num_comps; i++) {
        data_ptrs->plane_data_c[0][i] = data_ptrs->plane_data_c[2][i] = p;
        p += misc_vars->plane_size_c / 2;
    }
    for (i = 0; i < misc_vars->num_comps; i++) {
        data_ptrs->plane_data_c[1][i] = p;
        data_ptrs->plane_data_c[3][i] = p + misc_vars->plane_size_c / 2;
        p += misc_vars->plane_size_c / 2;
    }

    /* pointer for the upper bits of the output data */
    for (i = 0; i < misc_vars->num_comps; i++) {
        data_ptrs->plane_data_c[0][i + 4] = data_ptrs->plane_data_c[2][i +
            4] = p;
        p += misc_vars->plane_size_c / 2;
    }
    for (i = 0; i < misc_vars->num_comps; i++) {
        data_ptrs->plane_data_c[1][i + 4] = p;
        data_ptrs->plane_data_c[3][i + 4] = p + misc_vars->plane_size_c / 2;
        p += misc_vars->plane_size_c / 2;
    }

    for (i = 0; i < misc_vars->num_comps; i++) {
        data_ptrs->test_data[i] = p;
        p += misc_vars->plane_size_c / 2;
    }

    /* Clear temp storage */
    memset(data_ptrs->storage, 0, misc_vars->storage_size_words * W);

    return;
}				/* end init_data_structure */

/* Configure the printer and start Raster mode */
static void
cdj850_start_raster_mode(gx_device_printer * pdev, int paper_size,
                         gp_file * prn_stream)
{
    int xres, yres;		/* x,y resolution for color planes */
    hp850_cmyk_init_t init;

    init = hp850_cmyk_init;
    init.a[13] = cdj850->intensities;	/* Intensity levels cyan */
    init.a[19] = cdj850->intensities;	/* Intensity levels magenta */
    init.a[25] = cdj850->intensities;	/* Intensity levels yellow */

    /* black plane resolution */
    assign_dpi((int)cdj850->x_pixels_per_inch, init.a + 2);
    assign_dpi((int)cdj850->y_pixels_per_inch, init.a + 4);
    /* color plane resolution */
    xres = (int)(cdj850->x_pixels_per_inch / (cdj850->xscal + 1));
    yres = (int)(cdj850->y_pixels_per_inch / (cdj850->yscal + 1));
    /* cyan */
    assign_dpi(xres, init.a + 8);
    assign_dpi(yres, init.a + 10);
    /* magenta */
    assign_dpi(xres, init.a + 14);
    assign_dpi(yres, init.a + 16);
    /* yellow */
    assign_dpi(xres, init.a + 20);
    assign_dpi(yres, init.a + 22);

    gp_fputs("\033*rbC", prn_stream);	/* End raster graphics */
    gp_fputs("\033E", prn_stream);	/* Reset */
    /* Page size, orientation, top margin & perforation skip */
    gp_fprintf(prn_stream, "\033&l%daolE", paper_size);

    /* Print Quality, -1 = draft, 0 = normal, 1 = presentation */
    gp_fprintf(prn_stream, "\033*o%dM", cdj850->quality);
    /* Media Type,0 = plain paper, 1 = bond paper, 2 = special
       paper, 3 = glossy film, 4 = transparency film */
    gp_fprintf(prn_stream, "\033&l%dM", cdj850->papertype);

    /* Move to top left of printed area */
    gp_fprintf(prn_stream, "\033*p%dY", (int)(600 * DOFFSET));

    /* This will start and configure the raster-mode */
    gp_fprintf(prn_stream, "\033*g%dW", (int)sizeof(init.a));	/* The new configure
                                                                           raster data comand */
    gp_fwrite(init.a, sizeof(byte), sizeof(init.a),
              prn_stream);	/* Transmit config
                                   data */
    /* From now on, all escape commands start with \033*b, so we
     * combine them (if the printer supports this). */
    gp_fputs("\033*b", prn_stream);
    /* Set compression if the mode has been defined. */
    if (cdj850->compression)
        gp_fprintf(prn_stream, "%dm", cdj850->compression);

    return;
}				/* end configure raster-mode */

/* Configure the printer and start Raster mode */
static void
cdj880_start_raster_mode(gx_device_printer * pdev, int paper_size,
                         gp_file * prn_stream)
{
    int xres, yres;		/* x,y resolution for color planes */
    hp850_cmyk_init_t init;

    init = hp850_cmyk_init;
    init.a[13] = cdj850->intensities;	/* Intensity levels cyan */
    init.a[19] = cdj850->intensities;	/* Intensity levels magenta */
    init.a[25] = cdj850->intensities;	/* Intensity levels yellow */

    /* black plane resolution */
    assign_dpi((int)cdj850->x_pixels_per_inch, init.a + 2);
    assign_dpi((int)cdj850->y_pixels_per_inch, init.a + 4);
    /* color plane resolution */
    xres = (int)(cdj850->x_pixels_per_inch / (cdj850->xscal + 1));
    yres = (int)(cdj850->y_pixels_per_inch / (cdj850->yscal + 1));
    /* cyan */
    assign_dpi(xres, init.a + 8);
    assign_dpi(yres, init.a + 10);
    /* magenta */
    assign_dpi(xres, init.a + 14);
    assign_dpi(yres, init.a + 16);
    /* yellow */
    assign_dpi(xres, init.a + 20);
    assign_dpi(yres, init.a + 22);

    gp_fputs("\033*rbC", prn_stream);	/* End raster graphics */
    gp_fputs("\033E", prn_stream);	/* Reset */

    /* Set the language to PCL3 enhanced, DeskJet 880 style */
    gp_fprintf(prn_stream, "\033%%-12345X@PJL ENTER LANGUAGE=PCL3GUI\n");

    /* Page size, orientation, top margin & perforation skip */
    gp_fprintf(prn_stream, "\033&l%daolE", paper_size);

    /* Print Quality, -1 = draft, 0 = normal, 1 = presentation */
    gp_fprintf(prn_stream, "\033*o%dM", cdj850->quality);
    /* Media Type,0 = plain paper, 1 = bond paper, 2 = special
       paper, 3 = glossy film, 4 = transparency film */
    gp_fprintf(prn_stream, "\033&l%dM", cdj850->papertype);

    /* Move to top left of printed area */
    gp_fprintf(prn_stream, "\033*p%dY", (int)(600 * DOFFSET));

    /* This will configure the raster-mode */
    gp_fprintf(prn_stream, "\033*g%dW", (int)sizeof(init.a));	/* The new configure
                                                                           raster data comand */
    gp_fwrite(init.a, sizeof(byte), sizeof(init.a),
              prn_stream);	/* Transmit config
                                   data */

    /*  Start the raster graphics mode.  The 880C needs this explicit command.
     */
    gp_fputs("\033*r1A", prn_stream);

    /* From now on, all escape commands start with \033*b, so we
     * combine them (if the printer supports this). */
/*    gp_fputs("\033*b", prn_stream);*/

    /* Set compression if the mode has been defined. */
/*    if (cdj850->compression)*/
        gp_fprintf(prn_stream, "\033*b%dm", cdj850->compression);

    return;
}				/* end configure raster-mode */

/* Start Raster mode for HP2200 */
static void
chp2200_start_raster_mode(gx_device_printer * pdev, int paper_size,
                         gp_file * prn_stream)
{
    byte  CRD_SeqC[]     = {0x1b, 0x2a, 0x67, 0x31, 0x32, 0x57, 0x06, 0x07, 0x00, 0x01,
    /*                      Esc   *     |g    |# of bytes |W    |frmt |SP   |# of cmpnts*/
                            0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x20, 0x01};
    /*                      |Horz Res   |Vert Rez   |compr|orien|bits |planes*/

    /* x,y resolution for color planes, assume x=y */
    int xres = (int)cdj850->x_pixels_per_inch;
    int yres = (int)cdj850->y_pixels_per_inch;
    int width_in_pixels = cdj850->width;

    /* Exit from any previous language */
    gp_fprintf(prn_stream, "\033%%-12345X");

    /* send @PJL JOB NAME before entering the language
     * this will be matched by a @PJL EOJ after leaving the language
     */
    gp_fprintf(prn_stream, "@PJL JOB NAME=\"ghostscript job\"\012");

    /* Set timeout value */
    gp_fprintf(prn_stream, "@PJL SET TIMEOUT=90\n");

    /* Set the language to PCL3 enhanced */
    gp_fprintf(prn_stream, "@PJL ENTER LANGUAGE=PCL3GUI\n");

    /* Paper source: assume AutoFeed */
    gp_fprintf(prn_stream, "\033&l7H");

    /* Media Type,0 = plain paper, 1 = bond paper, 2 = special
       paper, 3 = glossy film, 4 = transparency film */
    gp_fprintf(prn_stream, "\033&l%dM", cdj850->papertype);

    /* Print Quality, -1 = draft, 0 = normal, 1 = presentation */
    gp_fprintf(prn_stream, "\033*o%dM", cdj850->quality);

    /* Raster Width */
    gp_fprintf(prn_stream, "\033*r%dS", width_in_pixels);

    /* Page size */
    gp_fprintf(prn_stream, "\033&l%dA", paper_size);

    /* Load paper */
    gp_fprintf(prn_stream, "\033&l-2H");

    /* Unit of Measure*/
    gp_fprintf(prn_stream, "\033&u%dD", (int)xres);

    /* Move to top of form */
    gp_fprintf(prn_stream, "\033*p%dY", (int)(yres * DOFFSET));

    /* This will configure the raster-mode */
    CRD_SeqC[10] = HIBYTE(xres);
    CRD_SeqC[11] = LOBYTE(xres);
    CRD_SeqC[12] = HIBYTE(yres);
    CRD_SeqC[13] = LOBYTE(yres);
    gp_fwrite(CRD_SeqC, sizeof(byte), sizeof(CRD_SeqC), prn_stream);

    gp_fputs("\033*r1A", prn_stream);

    return;
}				/* end configure raster-mode */

/* Start Raster mode for DNJ500 */
static void
cdnj500_start_raster_mode(gx_device_printer * pdev, int paper_size,
                         gp_file * prn_stream)
{
    /* x,y resolution for color planes, assume x=y */
    int xres = (int)cdj850->x_pixels_per_inch;
    float x = pdev->width  / pdev->x_pixels_per_inch * 10;
    float y = pdev->height / pdev->y_pixels_per_inch * 10;

    /* Exit from any previous language */
    gp_fprintf(prn_stream, "\033%%-12345X");

    /* send @PJL JOB NAME before entering the language
     * this will be matched by a @PJL EOJ after leaving the language
     */
    gp_fprintf(prn_stream, "@PJL JOB NAME=\"GS %.2fx%.2f\" \n", x * 2.54, y * 2.54);

    /* Color use */
    gp_fprintf(prn_stream, "@PJL SET RENDERMODE = COLOR \n");

    /* Color correction */
    gp_fprintf(prn_stream, "@PJL SET COLORSPACE = SRGB \n");

    /* Predef qual set (TODO: need add options) */
    if (cdj850->quality == DRAFT) {
        gp_fprintf(prn_stream, "@PJL SET RENDERINTENT = PERCEPTUAL \n");
        gp_fprintf(prn_stream, "@PJL SET RET = ON \n");
        gp_fprintf(prn_stream, "@PJL SET MAXDETAIL = OFF \n");
    } else if (cdj850->quality == NORMAL) {
        gp_fprintf(prn_stream, "@PJL SET RENDERINTENT = PERCEPTUAL \n");
        gp_fprintf(prn_stream, "@PJL SET RET = ON \n");
        gp_fprintf(prn_stream, "@PJL SET MAXDETAIL = ON \n");
    } else {  /* quality == PRESENTATION */
        gp_fprintf(prn_stream, "@PJL SET RENDERINTENT = PERCEPTUAL \n");
        gp_fprintf(prn_stream, "@PJL SET RET = OFF \n");
        gp_fprintf(prn_stream, "@PJL SET MAXDETAIL = ON \n");
    }

    /* Set the language to PCL3 enhanced */
    gp_fprintf(prn_stream, "@PJL ENTER LANGUAGE=PCL3GUI \n");

    /* Print Quality, -1 = draft, 0 = normal, 1 = presentation */
    gp_fprintf(prn_stream, "\033*o%dM", cdj850->quality);

    /* Unit of Measure*/
    gp_fprintf(prn_stream, "\033&u%dD", (int)xres);

    return;
}   /* end configure raster-mode */

static int near
cdj_put_param_int(gs_param_list * plist, gs_param_name pname, int *pvalue,
                  int minval, int maxval, int ecode)
{
    int code, value;

    switch (code = param_read_int(plist, pname, &value)) {
        default:
        return code;
        case 1:
        return ecode;
        case 0:
        if (value < minval || value > maxval)
            param_signal_error(plist, pname, gs_error_rangecheck);
        *pvalue = value;
        return (ecode < 0 ? ecode : 1);
    }
}

static int near
cdj_put_param_float(gs_param_list * plist, gs_param_name pname, float *pvalue,
                    float minval, float maxval, int ecode)
{
    int code;
    float value;

    switch (code = param_read_float(plist, pname, &value)) {
        default:
        return code;
        case 1:
        return ecode;
        case 0:
        if (value < minval || value > maxval)
            param_signal_error(plist, pname, gs_error_rangecheck);
        *pvalue = value;
        return (ecode < 0 ? ecode : 1);
    }
}

static int
cdj_set_bpp(gx_device * pdev, int bpp, int ccomps)
{
    gx_device_color_info *ci = &pdev->color_info;

    if (ccomps && bpp == 0) {
        if (cprn_device->cmyk) {
            switch (ccomps) {
                default:
		  return_error(gs_error_rangecheck);
                /*NOTREACHED */
                break;

                case 1:
                bpp = 1;
                break;

                case 3:
                bpp = 24;
                break;

                case 4:
                switch (ci->depth) {
                    case 8:
                    case 16:
                    case 24:
                    case 32:
                    break;

                    default:
                    bpp = cprn_device->default_depth;
                    break;
                }
                break;
            }
        }
    }
    if (bpp == 0) {
        bpp = ci->depth;	/* Use the current setting. */
    }
    if (cprn_device->cmyk < 0) {

        /* Reset procedures because we may have been in another mode. */

        dev_proc(pdev, map_cmyk_color) = gdev_cmyk_map_cmyk_color;
        dev_proc(pdev, map_rgb_color) = NULL;
        dev_proc(pdev, map_color_rgb) = gdev_cmyk_map_color_rgb;

        if (pdev->is_open)
            gs_closedevice(pdev);
    }
    /* Check for valid bpp values */

    switch (bpp) {
        case 16:
        case 32:
        if (cprn_device->cmyk && ccomps && ccomps != 4)
            goto bppe;
        break;

        case 24:
        if (!cprn_device->cmyk || ccomps == 0 || ccomps == 4) {
            break;
        } else if (ccomps == 1) {
            goto bppe;
        } else {

            /* 3 components 24 bpp printing for CMYK device. */

            cprn_device->cmyk = -1;
        }
        break;

        case 8:
        if (cprn_device->cmyk) {
            if (ccomps) {
                if (ccomps == 3) {
                    cprn_device->cmyk = -1;
                    bpp = 3;
                } else if (ccomps != 1 && ccomps != 4) {
                    goto bppe;
                }
            }
            if (ccomps != 1)
                break;
        } else {
            break;
        }

        case 1:
        if (ccomps != 1)
            goto bppe;

        if (cprn_device->cmyk && bpp != pdev->color_info.depth) {
            dev_proc(pdev, map_cmyk_color) = NULL;
            dev_proc(pdev, map_rgb_color) = gdev_cmyk_map_rgb_color;

            if (pdev->is_open) {
                gs_closedevice(pdev);
            }
        }
        break;

        case 3:
        if (!cprn_device->cmyk) {
            break;
        }
        default:
    bppe:return_error(gs_error_rangecheck);
    }

    if (cprn_device->cmyk == -1) {
        dev_proc(pdev, map_cmyk_color) = NULL;
        dev_proc(pdev, map_rgb_color) = gdev_pcl_map_rgb_color;
        dev_proc(pdev, map_color_rgb) = gdev_pcl_map_color_rgb;

        if (pdev->is_open) {
            gs_closedevice(pdev);
        }
    }
    switch (ccomps) {
        case 0:
        break;

        case 1:
        if (bpp != 1 && bpp != 8)
            goto cce;
        break;

        case 4:
        if (cprn_device->cmyk) {
            if (bpp >= 8)
                break;
        }
        case 3:
        if (bpp == 1 || bpp == 3 || bpp == 8 || bpp == 16
            || bpp == 24 || bpp == 32) {
            break;
        }
        cce: default:
        return_error(gs_error_rangecheck);
    }

    if (cprn_device->cmyk) {
        if (cprn_device->cmyk > 0) {
            ci->num_components = ccomps ? ccomps : (bpp < 8 ? 1 : 4);
        } else {
            ci->num_components = ccomps ? ccomps : (bpp < 8 ? 1 : 3);
        }
        if (bpp != 1 && ci->num_components == 1) {	/* We do dithered grays. */
            bpp = bpp < 8 ? 8 : bpp;
        }
        ci->max_color = (1 << (bpp >> 2)) - 1;
        ci->max_gray = (bpp >= 8 ? 255 : 1);

        if (ci->num_components == 1) {
            ci->dither_grays = (bpp >= 8 ? 5 : 2);
            ci->dither_colors = (bpp >= 8 ? 5 : bpp > 1 ? 2 : 0);
        } else {
            ci->dither_grays = (bpp > 8 ? 5 : 2);
            ci->dither_colors = (bpp > 8 ? 5 : bpp > 1 ? 2 : 0);
        }
    } else {
        ci->num_components = (bpp == 1 || bpp == 8 ? 1 : 3);
        ci->max_color = (bpp >= 8 ? 255 : bpp > 1 ? 1 : 0);
        ci->max_gray = (bpp >= 8 ? 255 : 1);
        ci->dither_grays = (bpp >= 8 ? 5 : 2);
        ci->dither_colors = (bpp >= 8 ? 5 : bpp > 1 ? 2 : 0);
    }

    ci->depth = ((bpp > 1) && (bpp < 8) ? 8 : bpp);

    return 0;
}

/*
 * Map a CMYK color to a color index. We just use depth / 4 bits per color
 * to produce the color index.
 *
 * Important note: CMYK values are stored in the order K, C, M, Y because of
 * the way the HP drivers work.
 *
 */

#define gx_color_value_to_bits(cv, b) \
    ((cv) >> (gx_color_value_bits - (b)))
#define gx_bits_to_color_value(cv, b) \
    ((cv) << (gx_color_value_bits - (b)))

#define gx_cmyk_value_bits(c, m, y, k, b) \
    ((gx_color_value_to_bits((k), (b)) << (3 * (b))) | \
     (gx_color_value_to_bits((c), (b)) << (2 * (b))) | \
     (gx_color_value_to_bits((m), (b)) << (b)) | \
     (gx_color_value_to_bits((y), (b))))

#define gx_value_cmyk_bits(v, c, m, y, k, b) \
    (k) = gx_bits_to_color_value(((v) >> (3 * (b))) & ((1 << (b)) - 1), (b)), \
    (c) = gx_bits_to_color_value(((v) >> (2 * (b))) & ((1 << (b)) - 1), (b)), \
    (m) = gx_bits_to_color_value(((v) >> (b)) & ((1 << (b)) - 1), (b)), \
    (y) = gx_bits_to_color_value((v) & ((1 << (b)) - 1), (b))

static gx_color_index
gdev_cmyk_map_cmyk_color(gx_device * pdev,
                         const gx_color_value cv[])
{

    gx_color_index color;
    gx_color_value cyan, magenta, yellow, black;

    cyan = cv[0]; magenta = cv[1]; yellow = cv[2]; black = cv[3];
    switch (pdev->color_info.depth) {
        case 1:
        color = (cyan | magenta | yellow | black) > gx_max_color_value / 2 ?
            (gx_color_index) 1 : (gx_color_index) 0;
        break;

        default:{
            int nbits = pdev->color_info.depth;

            if (cyan == magenta && magenta == yellow) {
                /* Convert CMYK to gray -- Red Book 6.2.2 */
                float bpart = ((float)cyan) * (lum_red_weight / 100.) +
                ((float)magenta) * (lum_green_weight / 100.) +
                ((float)yellow) * (lum_blue_weight / 100.) +
                (float)black;

                cyan = magenta = yellow = (gx_color_index) 0;
                black = (gx_color_index) (bpart > gx_max_color_value ?
                                          gx_max_color_value : bpart);
            }
            color = gx_cmyk_value_bits(cyan, magenta, yellow, black,
                                       nbits >> 2);
        }
    }

    return color;
}

/* Mapping of RGB colors to gray values. */

static gx_color_index
gdev_cmyk_map_rgb_color(gx_device * pdev, const gx_color_value cv[])
{
    gx_color_value r, g, b;

    r = cv[0]; g = cv[1]; b = cv[2];
    if (gx_color_value_to_byte(r & g & b) == 0xff) {
        return (gx_color_index) 0;	/* White */
    } else {
        gx_color_value c = gx_max_color_value - r;
        gx_color_value m = gx_max_color_value - g;
        gx_color_value y = gx_max_color_value - b;

        switch (pdev->color_info.depth) {
            case 1:
            return (c | m | y) > gx_max_color_value / 2 ?
                (gx_color_index) 1 : (gx_color_index) 0;
            /*NOTREACHED */
            break;

            case 8:
            return ((ulong) c * lum_red_weight * 10
                    + (ulong) m * lum_green_weight * 10
                    + (ulong) y * lum_blue_weight * 10)
                >> (gx_color_value_bits + 2);
            /*NOTREACHED */
            break;
        }
    }

    return (gx_color_index) 0;	/* This should never happen. */
}

/* Mapping of CMYK colors. */
static int
gdev_cmyk_map_color_rgb(gx_device * pdev, gx_color_index color,
                        gx_color_value prgb[3])
{
    switch (pdev->color_info.depth) {
        case 1:
        prgb[0] = prgb[1] = prgb[2] = gx_max_color_value * (1 - color);
        break;

        case 8:
        if (pdev->color_info.num_components == 1) {
            gx_color_value value = (gx_color_value) color ^ 0xff;

            prgb[0] = prgb[1] = prgb[2] = (value << 8) + value;

            break;
        }
        default:{
            unsigned long bcyan, bmagenta, byellow, black;
            int nbits = pdev->color_info.depth;

            gx_value_cmyk_bits(color, bcyan, bmagenta, byellow, black,
                               nbits >> 2);

#ifdef USE_ADOBE_CMYK_RGB

            /* R = 1.0 - min(1.0, C + K), etc. */

            bcyan += black, bmagenta += black, byellow += black;
            prgb[0] = (bcyan > gx_max_color_value ? (gx_color_value) 0 :
                       gx_max_color_value - bcyan);
            prgb[1] = (bmagenta > gx_max_color_value ? (gx_color_value) 0 :
                       gx_max_color_value - bmagenta);
            prgb[2] = (byellow > gx_max_color_value ? (gx_color_value) 0 :
                       gx_max_color_value - byellow);

#else

            /* R = (1.0 - C) * (1.0 - K), etc. */

            prgb[0] = (gx_color_value)
                ((ulong) (gx_max_color_value - bcyan) *
                 (gx_max_color_value - black) / gx_max_color_value);
            prgb[1] = (gx_color_value)
                ((ulong) (gx_max_color_value - bmagenta) *
                 (gx_max_color_value - black) / gx_max_color_value);
            prgb[2] = (gx_color_value)
                ((ulong) (gx_max_color_value - byellow) *
                 (gx_max_color_value - black) / gx_max_color_value);

#endif

        }
    }

    return 0;
}

static gx_color_index
gdev_pcl_map_rgb_color(gx_device * pdev, const gx_color_value cv[])
{
    gx_color_value r, g, b;

    r = cv[0]; g = cv[1]; b = cv[2];
    if (gx_color_value_to_byte(r & g & b) == 0xff)
        return (gx_color_index) 0;	/* white */
    else {
        gx_color_value c = gx_max_color_value - r;
        gx_color_value m = gx_max_color_value - g;
        gx_color_value y = gx_max_color_value - b;

        switch (pdev->color_info.depth) {
            case 1:
            return ((c | m | y) > gx_max_color_value / 2 ?
                    (gx_color_index) 1 : (gx_color_index) 0);
            case 8:
            if (pdev->color_info.num_components >= 3)
#define gx_color_value_to_1bit(cv) ((cv) >> (gx_color_value_bits - 1))
                return (gx_color_value_to_1bit(c) +
                        (gx_color_value_to_1bit(m) << 1) +
                        (gx_color_value_to_1bit(y) << 2));
            else
#define red_weight 306
#define green_weight 601
#define blue_weight 117
                return ((((ulong) c * red_weight +
                          (ulong) m * green_weight +
                          (ulong) y * blue_weight)
                         >> (gx_color_value_bits + 2)));
            case 16:
#define gx_color_value_to_5bits(cv) ((cv) >> (gx_color_value_bits - 5))
#define gx_color_value_to_6bits(cv) ((cv) >> (gx_color_value_bits - 6))
            return (gx_color_value_to_5bits(y) +
                    (gx_color_value_to_6bits(m) << 5) +
                    (gx_color_value_to_5bits(c) << 11));
            case 24:
            return (gx_color_value_to_byte(y) +
                    (gx_color_value_to_byte(m) << 8) +
                    ((ulong) gx_color_value_to_byte(c) << 16));
            case 32:
            {
                return ((c == m && c == y) ? ((ulong)
                                              gx_color_value_to_byte(c) << 24)
                        : (gx_color_value_to_byte(y) +
                           (gx_color_value_to_byte(m) << 8) +
                           ((ulong) gx_color_value_to_byte(c) << 16)));
            }
        }
    }
    return (gx_color_index) 0;	/* This never happens */
}

/* Map a color index to a r-g-b color. */
static int
gdev_pcl_map_color_rgb(gx_device * pdev, gx_color_index color,
                       gx_color_value prgb[3])
{
    /* For the moment, we simply ignore any black correction */
    switch (pdev->color_info.depth) {
        case 1:
        prgb[0] = prgb[1] = prgb[2] = -((gx_color_value) color ^ 1);
        break;
        case 8:
        if (pdev->color_info.num_components >= 3) {
            gx_color_value c = (gx_color_value) color ^ 7;

            prgb[0] = -(c & 1);
            prgb[1] = -((c >> 1) & 1);
            prgb[2] = -(c >> 2);
        } else {
            gx_color_value value = (gx_color_value) color ^ 0xff;

            prgb[0] = prgb[1] = prgb[2] = (value << 8) + value;
        }
        break;
        case 16:
        {
            gx_color_index c = (gx_color_index) color ^ 0xffff;
            ushort value = c >> 11;

            prgb[0] = ((value << 11) + (value << 6) + (value << 1) +
                       (value >> 4)) >> (16 - gx_color_value_bits);
            value = (c >> 6) & 0x3f;
            prgb[1] = ((value << 10) + (value << 4) + (value >> 2))
                >> (16 - gx_color_value_bits);
            value = c & 0x1f;
            prgb[2] = ((value << 11) + (value << 6) + (value << 1) +
                       (value >> 4)) >> (16 - gx_color_value_bits);
        }
        break;
        case 24:
        {
            gx_color_index c = (gx_color_index) color ^ 0xffffff;

            prgb[0] = gx_color_value_from_byte(c >> 16);
            prgb[1] = gx_color_value_from_byte((c >> 8) & 0xff);
            prgb[2] = gx_color_value_from_byte(c & 0xff);
        }
        break;
        case 32:
#define  gx_maxcol gx_color_value_from_byte(gx_color_value_to_byte(gx_max_color_value))
        {
            gx_color_value w = gx_maxcol - gx_color_value_from_byte(color >> 24);

            prgb[0] = w - gx_color_value_from_byte((color >> 16) & 0xff);
            prgb[1] = w - gx_color_value_from_byte((color >> 8) & 0xff);
            prgb[2] = w - gx_color_value_from_byte(color & 0xff);
        }
        break;
    }
    return 0;
}

/* new_bpp == save_bpp or new_bpp == 0 means don't change bpp.
   ccomps == 0 means don't change number of color comps.
   If new_bpp != 0, it must be the value of the BitsPerPixel element of
   the plist; real_bpp may differ from new_bpp.
 */
static int
cdj_put_param_bpp(gx_device * pdev, gs_param_list * plist, int new_bpp,
                  int real_bpp, int ccomps)
{
    if (new_bpp == 0 && ccomps == 0)
        return gdev_prn_put_params(pdev, plist);
    else {
        gx_device_color_info save_info;
        int save_bpp;
        int code;

        save_info = pdev->color_info;
        save_bpp = save_info.depth;
#define save_ccomps save_info.num_components
        if (save_bpp == 8 && save_ccomps == 3 && !cprn_device->cmyk)
            save_bpp = 3;
        code = cdj_set_bpp(pdev, real_bpp, ccomps);
        if (code < 0) {
            param_signal_error(plist, "BitsPerPixel", code);
            param_signal_error(plist, "ProcessColorModel", code);
            return code;
        }
        pdev->color_info.depth = new_bpp;	/* cdj_set_bpp maps 3/6 to 8 */
        code = gdev_prn_put_params(pdev, plist);
        if (code < 0) {
            cdj_set_bpp(pdev, save_bpp, save_ccomps);
            return code;
        }
        cdj_set_bpp(pdev, real_bpp, ccomps);	/* reset depth if needed */
        if ((cdj850->color_info.depth != save_bpp ||
             (ccomps != 0 && ccomps != save_ccomps))
            && pdev->is_open)
            return gs_closedevice(pdev);
        return 0;
#undef save_ccomps
    }
}

/* the following code was in the original driver but is unused

 * static int
 * x_mul_div (int a, int b, int c)
 * {
 *   int result;
 *
 *   result = (int) ((a * b) / c) ;
 *  return result;
 * }
 *
 * static void
 * save_color_data(int size,
 *              byte * current,
 *              byte * saved)
 * {
 *   int i;
 *   for (i=0;i<size;i++){
 *     *saved++ = *current++;
 *   }
 *   return;
 * }
 *
 * static int
 * test_scan (int size,
 *         byte * current,
 *         byte * last,
 *         byte * control)
 *   {
 *   int error = 0;
 *   int i;
 *
 *   for (i=0;i<size;i++){
 *     if (*control != *last){
 *       error = 1;
 *     }
 *     *control = *current;
 *
 *     control++;
 *     last++;
 *     current++;
 *   }
 *   return error;
 * }
 *
 * * Transform from cmy into hsv
 * static void
 * cmy2hsv(int *c, int *m, int *y, int *h, int *s, int *v)
 * {
 *   int hue;
 *   int r, g, b;
 *   int r1, g1, b1;
 *   int maxValue, minValue, diff;
 *
 *   r = 255 - *c;
 *   g = 255 - *m;
 *   b = 255 - *y;
 *
 *   maxValue = max(r, max(g,b));
 *   minValue = min(r,min(g,b));
 *   diff = maxValue - minValue;
 *   *v = maxValue;
 *
 *   if (maxValue != 0)
 *     *s = x_mul_div(diff,255,maxValue);
 *   else
 *     *s = 0;
 *
 *   if (*s == 0)
 *     {
 *       hue = 0;
 *     }
 *   else
 *     {
 *       r1 = x_mul_div(maxValue - r,255,diff);
 *       g1 = x_mul_div(maxValue - g,255,diff);
 *       b1 = x_mul_div(maxValue - b,255,diff);
 *
 *       if (r == maxValue)
 *      hue = b1 - g1;
 *       else if (g == maxValue)
 *      hue = 510 + r1 - b1;
 *       else
 *      hue = 1020 + g1 - r1;
 *
 *       if (hue < 0)
 *      hue += 1530;
 *     }
 *
 *   *h = (hue + 3) / 6;
 *
 *   return;
 * }
 * end of unused code */

/************************ the routines for the cdj1600 printer ***************/

/* Configure the printer and start Raster mode */
static void
cdj1600_start_raster_mode(gx_device_printer * pdev, int paper_size,
                          gp_file * prn_stream)
{
    uint raster_width = (int)(pdev->width -
                              pdev->x_pixels_per_inch *
                              (dev_l_margin(pdev) + dev_r_margin(pdev)));

    /* switch to PCL control language */
    gp_fputs("\033%-12345X@PJL enter language = PCL\n", prn_stream);

    gp_fputs("\033*rbC", prn_stream);	/* End raster graphics */
    gp_fputs("\033E", prn_stream);	/* Reset */

    /* resolution */
    gp_fprintf(prn_stream, "\033*t%dR", (int)cdj850->x_pixels_per_inch);

    /* Page size, orientation, top margin & perforation skip */
    gp_fprintf(prn_stream, "\033&l%daolE", paper_size);

    /* no negative motion */
    gp_fputs("\033&a1N", prn_stream);

    /* Print Quality, -1 = draft, 0 = normal, 1 = presentation */
    gp_fprintf(prn_stream, "\033*o%dQ", cdj850->quality);

    /* Media Type,0 = plain paper, 1 = bond paper, 2 = special
       paper, 3 = glossy film, 4 = transparency film */
    gp_fprintf(prn_stream, "\033&l%dM", cdj850->papertype);

    /* Move to top left of printed area */
    gp_fprintf(prn_stream, "\033*p%dY", (int)(300.0 * DOFFSET));

    /* raster width and number of planes */
    gp_fprintf(prn_stream, "\033*r%ds-%du0A",
            raster_width, pdev->color_info.num_components);

    /* start raster graphics */
    gp_fputs("\033*r1A", prn_stream);

    /* From now on, all escape commands start with \033*b, so we
     * combine them (if the printer supports this). */
    gp_fputs("\033*b", prn_stream);

    /* Set compression if the mode has been defined. */
    if (cdj850->compression)
        gp_fprintf(prn_stream, "%dm", cdj850->compression);

    return;
}				/* end configure raster-mode */

/* print_plane compresses (mode 3) and outputs one plane */
static void
print_c3plane(gp_file * prn_stream, char plane_code, int plane_size,
              const byte * curr, byte * prev, byte * out_data)
{
    /* Compress the output data */
    int out_count = gdev_pcl_mode3compress(plane_size, curr, prev, out_data);

    /* and output the data */
    if (out_count > 0) {
        gp_fprintf(prn_stream, "%d%c", out_count, plane_code);
        gp_fwrite(out_data, sizeof(byte), out_count, prn_stream);
    } else {
        gp_fputc(plane_code, prn_stream);
    }
}

static int
copy_color_data(byte * dest, const byte * src, int n)
{
    /* copy word by word */
    register int i = n / 4;
    register word *d = (word *) dest;
    register const word *s = (const word *)src;

    while (i-- > 0) {
        *d++ = *s++;
    }
    return n;
}

/* Printing non-blank lines */
static void
cdj1600_print_non_blank_lines(gx_device_printer * pdev,
                              struct ptr_arrays *data_ptrs,
                              struct misc_struct *misc_vars,
                              struct error_val_field *error_values,
                              const Gamma *gamma,
                              gp_file * prn_stream)
{
    int i, plane_size_c;

    /* copy data to data_c in order to make do_floyd_steinberg work */
    plane_size_c = copy_color_data
        (data_ptrs->data_c[misc_vars->cscan],
         data_ptrs->data[misc_vars->scan],
         misc_vars->databuff_size) / misc_vars->storage_bpp;

    /* dither the color planes */
    do_floyd_steinberg(misc_vars->scan, misc_vars->cscan,
                       misc_vars->plane_size, plane_size_c,
                       misc_vars->num_comps, data_ptrs, pdev, error_values);

    /* Transfer raster graphics in the order C, M, Y, that is
       planes 2,1,0 */
    for (i = misc_vars->num_comps - 1; i >= 0; i--) {

        /* output the lower color planes */
        print_c3plane(prn_stream, "wvv"[i], plane_size_c,
                      data_ptrs->plane_data_c[misc_vars->cscan][i],
                      data_ptrs->plane_data_c[1 - misc_vars->cscan][i],
                      data_ptrs->out_data);
    }				/* End For i = num_comps */
    misc_vars->cscan = 1 - misc_vars->cscan;
}

static void
cdj1600_terminate_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    cdj850_terminate_page(pdev, prn_stream);
    gp_fputs("\033%-12345X", prn_stream);
}
