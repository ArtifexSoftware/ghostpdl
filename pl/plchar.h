/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* plchar.h */
/* Interface to PCL character routines */

#ifndef plchar_INCLUDED
#  define plchar_INCLUDED

int pl_image_bitmap_char(gs_image_enum *ienum, const gs_image_t *pim,
                         const byte *bitmap_data, uint sraster,
                         int bold, byte *bold_lines, gs_state *pgs);


int pl_tt_get_outline(gs_font_type42 *pfont, uint index, gs_glyph_data_t *pdata);

ulong tt_find_table(gs_font_type42 *pfont, const char *tname, uint *plen);

#endif				/* plchar_INCLUDED */
