/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* PC color mapping support */
/* Requires gxdevice.h */

#ifndef gdevpccm_INCLUDED
#  define gdevpccm_INCLUDED

/* Color mapping routines for EGA/VGA-style color. */
dev_proc_map_rgb_color(pc_4bit_map_rgb_color);
dev_proc_map_color_rgb(pc_4bit_map_color_rgb);
#define dci_pc_4bit dci_values(3, 4, 3, 2, 4, 3)

/* Color mapping routines for 8-bit color (with a fixed palette). */
dev_proc_map_rgb_color(pc_8bit_map_rgb_color);
dev_proc_map_color_rgb(pc_8bit_map_color_rgb);
#define dci_pc_8bit dci_values(3, 8, 6, 6, 7, 7)

/* Write the palette on a file. */
int pc_write_palette(P3(gx_device *, uint, FILE *));

#endif /* gdevpccm_INCLUDED */
