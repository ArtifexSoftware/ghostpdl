/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/
/* We need several tables, to hold the glyph names and a variable number of
 * Unicode code points.
 */

typedef struct pdfi_single_glyph_list_s {
  const char *Glyph;
  unsigned short Unicode;
} pdfi_single_glyph_list_t;

typedef struct pdfi_double_glyph_list_s {
  const char *Glyph;
  unsigned short Unicode[2];
} pdfi_double_glyph_list_t;

typedef struct pdfi_treble_glyph_list_s {
  const char *Glyph;
  unsigned short Unicode[3];
} pdfi_treble_glyph_list_t;

typedef struct pdfi_quad_glyph_list_s {
  const char *Glyph;
  short Unicode[4];
} pdfi_quad_glyph_list_t;
