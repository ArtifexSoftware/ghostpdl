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


/* PC color mapping support */
/* Requires gxdevice.h */

#ifndef gdevpccm_INCLUDED
#  define gdevpccm_INCLUDED

#include "gxdevcli.h"

/* Color mapping routines for EGA/VGA-style color. */
dev_proc_map_rgb_color(pc_4bit_map_rgb_color);
dev_proc_map_color_rgb(pc_4bit_map_color_rgb);
#define dci_pc_4bit dci_values(3, 4, 1, 1, 2, 2)

/* Color mapping routines for 8-bit color (with a fixed palette). */
dev_proc_map_rgb_color(pc_8bit_map_rgb_color);
dev_proc_map_color_rgb(pc_8bit_map_color_rgb);
#define dci_pc_8bit dci_values(3, 8, 5, 5, 6, 6)

/* Write the palette on a file. */
int pc_write_palette(gx_device *, uint, gp_file *);

#endif /* gdevpccm_INCLUDED */
