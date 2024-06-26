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

/* Support routines for PC color mapping */
#include "gx.h"
#include "gsmatrix.h"		/* for gxdevice.h */
#include "gxdevice.h"
#include "gdevpccm.h"		/* interface */

/* Color mapping routines for EGA/VGA-style color. */

/* ------ EGA/VGA (4-bit) color mapping ------ */

/*
 * Colors are 4 bits: 8=intensity (always set except black), 4=R, 2=G, 1=B.
 * Note: We only use eight colors.  The halftoning logic requires that we
 * have the same number for shades for each component.
 */
gx_color_index
pc_4bit_map_rgb_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index r, g, color;

    r = (cv[0] > (gx_max_color_value/2));
    g = (cv[1] > (gx_max_color_value/2));
    color = (cv[2] > (gx_max_color_value/2));
    color += (r << 2) + (g << 1);
    if (color > 0)		/* If the color is not black */
        color += 8;		/* Turn on intensity bit */
    return color;
}

int
pc_4bit_map_color_rgb(gx_device * dev, gx_color_index color,
                      gx_color_value prgb[3])
{
#define icolor (int)color
    prgb[0] = (icolor & 4 ? gx_max_color_value : 0);
    prgb[1] = (icolor & 2 ? gx_max_color_value : 0);
    prgb[2] = (icolor & 1 ? gx_max_color_value : 0);
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
pc_write_palette(gx_device * dev, uint max_index, gp_file * file)
{
    uint i, c;
    gx_color_value rgb[3];

    for (i = 0; i < max_index; i++) {
        (*dev_proc(dev, map_color_rgb)) (dev, (gx_color_index) i, rgb);
        for (c = 0; c < 3; c++) {
            byte b = rgb[c] >> (gx_color_value_bits - 8);

            gp_fputc(b, file);
        }
    }
    return 0;
}
