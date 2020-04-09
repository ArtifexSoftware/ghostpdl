/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* plchar.h */
/* Interface to PCL character routines */

#ifndef plchar_INCLUDED
#  define plchar_INCLUDED

int pl_image_bitmap_char(gs_image_enum * ienum, const gs_image_t * pim,
                         const byte * bitmap_data, uint sraster,
                         int bold, byte * bold_lines, gs_gstate * pgs);


int pl_tt_get_outline(gs_font_type42 * pfont, uint index,
                      gs_glyph_data_t * pdata);

/* retrieve lsb and width metrics for Format 1 Class 2 glyphs */
int
pl_tt_f1c2_get_metrics(gs_font_type42 * pfont, uint glyph_index,
                  int wmode, float *sbw);


ulong tt_find_table(gs_font_type42 * pfont, const char *tname, uint * plen);

gs_glyph
pl_tt_encode_char(gs_font * pfont_generic, gs_char chr,
                  gs_glyph_space_t not_used);

gs_glyph pl_font_vertical_glyph(gs_glyph glyph, const pl_font_t * plfont);

int
pl_tt_get_outline(gs_font_type42 * pfont, uint index,
                  gs_glyph_data_t * pdata);

#endif /* plchar_INCLUDED */
