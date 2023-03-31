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


/* Interpreter definitions for Pattern color */

#ifndef ipcolor_INCLUDED
#  define ipcolor_INCLUDED

#include "iref.h"

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
