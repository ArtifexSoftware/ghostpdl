/* Copyright (C) 1992, 1993, 1998 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Support routines for PC color mapping */
#include "gx.h"
#include "gsmatrix.h"		/* for gxdevice.h */
#include "gxdevice.h"
#include "gdevpccm.h"		/* interface */

/* Color mapping routines for EGA/VGA-style color. */
/* Colors are 4 bits: 8=intensity, 4=R, 2=G, 1=B. */

/* Define the color spectrum */
#define v_black 0
#define v_blue 1
#define v_green 2
#define v_cyan 3
#define v_red 4
#define v_magenta 5
#define v_brown 6
#define v_white 7
#define v_dgray 8		/* dark gray is not very usable */
#define v_lblue 9
#define v_lgreen 10
#define v_lcyan 11
#define v_lred 12
#define v_lmagenta 13
#define v_yellow 14
#define v_bwhite 15

/* ------ EGA/VGA (4-bit) color mapping ------ */

gx_color_index
pc_4bit_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
#define Nb gx_color_value_bits
    static const byte grays[4] =
    {v_black, v_dgray, v_white, v_bwhite};

#define tab3(v0,v1,v23) { v0, v1, v23, v23 }
    static const byte g0r0[4] = tab3(v_black, v_blue, v_lblue);
    static const byte g0r1[4] = tab3(v_red, v_magenta, v_lmagenta);
    static const byte g0r2[4] = tab3(v_lred, v_lmagenta, v_lmagenta);
    static const byte *const g0[4] = tab3(g0r0, g0r1, g0r2);
    static const byte g1r0[4] = tab3(v_green, v_cyan, v_lcyan);
    static const byte g1r1[4] = tab3(v_brown, v_white, v_lcyan);
    static const byte g1r2[4] = tab3(v_yellow, v_lred, v_lmagenta);
    static const byte *const g1[4] = tab3(g1r0, g1r1, g1r2);
    static const byte g2r0[4] = tab3(v_lgreen, v_lgreen, v_lcyan);
    static const byte g2r1[4] = tab3(v_lgreen, v_lgreen, v_lcyan);
    static const byte g2r2[4] = tab3(v_yellow, v_yellow, v_bwhite);
    static const byte *const g2[4] = tab3(g2r0, g2r1, g2r2);
    static const byte *const *const ga[4] = tab3(g0, g1, g2);

#undef tab3
#define q4mask (-1 << (Nb - 2))
    gx_color_value r, g, b;
    r = cv[0]; g = cv[1]; b = cv[2];
    if (!((r ^ g) & q4mask) && !((g ^ b) & q4mask))	/* gray */
#undef q4mask
	return (gx_color_index) grays[r >> (Nb - 2)];
    else
#define q3cv(v) ((v - (v >> 2)) >> (Nb - 2))
	return (gx_color_index) ga[q3cv(g)][q3cv(r)][q3cv(b)];
#undef q3cv
#undef Nb
}
int
pc_4bit_map_color_rgb(gx_device * dev, gx_color_index color,
		      gx_color_value prgb[3])
{
#define icolor (int)color
    gx_color_value one;

    switch (icolor) {
	case v_white:		/* "dark white" */
	    prgb[0] = prgb[1] = prgb[2] =
		gx_max_color_value - (gx_max_color_value / 3);
	    return 0;
	case v_dgray:
	    prgb[0] = prgb[1] = prgb[2] = gx_max_color_value / 3;
	    return 0;
    }
    one = (icolor & 8 ? gx_max_color_value : gx_max_color_value / 2);
    prgb[0] = (icolor & 4 ? one : 0);
    prgb[1] = (icolor & 2 ? one : 0);
    prgb[2] = (icolor & 1 ? one : 0);
    return 0;
#undef icolor
}

/* ------ SVGA 8-bit color mapping ------ */
/*
 * For 8-bit color, we use a 6x6x6 "cube".  This only provides 216
 * different colors.  The halftoning logic assumes that we have the same
 * number of shades of each color.  Thus asymetric cubes like 8x8x4 or
 * 7x7x5 do not work properly.
 */

gx_color_index
pc_8bit_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_value r, g, b;
    uint rv, gv;
    r = cv[0]; g = cv[1]; b = cv[2];
    rv = r / (gx_max_color_value / 6 + 1);
    gv = g / (gx_max_color_value / 6 + 1);

    return (gx_color_index)
	 (rv * 6 + gv) * 6 + b / (gx_max_color_value / 6 + 1);
}
int
pc_8bit_map_color_rgb(gx_device * dev, gx_color_index color,
		      gx_color_value prgb[3])
{
    static const gx_color_value ramp6[6] =
    {0,
     gx_max_color_value / 5,
     2 * gx_max_color_value / 5,
     3 * gx_max_color_value / 5,
     gx_max_color_value - (gx_max_color_value / 5),
     gx_max_color_value
    };
    
#define icolor (uint)color
    if (icolor >= 216) {
	prgb[0] = prgb[1] = prgb[2] = 0;
    } else {
	prgb[0] = ramp6[icolor / 36];
	prgb[1] = ramp6[(icolor / 6) % 6];
	prgb[2] = ramp6[icolor % 6];
    }
#undef icolor
    return 0;
}

/* Write a palette on a file. */
int
pc_write_palette(gx_device * dev, uint max_index, FILE * file)
{
    uint i, c;
    gx_color_value rgb[3];

    for (i = 0; i < max_index; i++) {
	(*dev_proc(dev, map_color_rgb)) (dev, (gx_color_index) i, rgb);
	for (c = 0; c < 3; c++) {
	    byte b = rgb[c] >> (gx_color_value_bits - 8);

	    fputc(b, file);
	}
    }
    return 0;
}
