/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gdevht.h */
/* Definitions for halftoning device */
/* Requires gxdevice.h */
#include "gzht.h"

/*
 * A halftoning device converts between a non-halftoned device color space
 * (e.g., 8-bit gray) and a halftoned space (e.g., 1-bit black and white).
 * Currently, the target space must not exceed 8 bits per pixel, so that
 * we can pack two target colors and a halftone level into a gx_color_index.
 */
#define ht_target_max_depth 8
#define ht_level_depth (sizeof(gx_color_index) * 8 - ht_target_max_depth * 2)
typedef struct gx_device_ht_s {
	gx_device_forward_common;
		/* Following are set before opening. */
	const gx_device_halftone *dev_ht;
	gs_int_point ht_phase;		/* halftone phase from gstate */
		/* Following are computed when device is opened. */
	gs_int_point phase;		/* halftone tile offset */
} gx_device_ht;

/* Macro for casting gx_device argument */
#define htdev ((gx_device_ht *)dev)
