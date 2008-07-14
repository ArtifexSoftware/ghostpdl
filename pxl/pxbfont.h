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

/* pxbfont.h */
/* Interface to PCL XL built-in bitmap font */

/*
 * This bitmap font is included in the interpreter solely for the
 * purpose of producing error pages.
 */

extern const int px_bitmap_font_point_size;
extern const int px_bitmap_font_resolution;
extern const byte px_bitmap_font_header[];
extern const uint px_bitmap_font_header_size;
extern const byte px_bitmap_font_char_data[];
