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
/*$Id$ */

/* pgfdata.h */
/* Interface to HP-GL/2 stick and arc font data */

/* font type - stick or arc */
typedef enum {
    HPGL_ARC_FONT,
    HPGL_STICK_FONT
} hpgl_font_type_t;

/* Enumerate the segments of a stick/arc character. */
int hpgl_stick_arc_segments(const gs_memory_t *mem,
			    void *data, uint char_index, hpgl_font_type_t font_type);

/* Get the unscaled width of a stick/arc character. */
int hpgl_stick_arc_width(uint char_index, hpgl_font_type_t font_type);
