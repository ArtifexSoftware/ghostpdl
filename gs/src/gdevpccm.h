/* Copyright (C) 1992, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

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
