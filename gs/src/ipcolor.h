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
/* Interpreter definitions for Pattern color */

#ifndef ipcolor_INCLUDED
#  define ipcolor_INCLUDED

/*
 * Define the structure for remembering the pattern dictionary.
 * This is the "client data" in the template.
 * See zgstate.c (int_gstate) or zfont2.c (font_data) for information
 * as to why we define this as a structure rather than a ref array.
 */
typedef struct int_pattern_s {
    ref dict;
} int_pattern;

#define private_st_int_pattern()	/* in zpcolor.c */\
  gs_private_st_ref_struct(st_int_pattern, int_pattern, "int_pattern")

/* Create an interpreter pattern structure. */
int int_pattern_alloc(int_pattern **ppdata, const ref *op,
		      gs_memory_t *mem);

#endif /* ipcolor_INCLUDED */
