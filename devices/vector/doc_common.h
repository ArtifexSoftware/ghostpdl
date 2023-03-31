/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

#ifndef gdevtxtw_INCLUDED
#define gdevtxtw_INCLUDED

#include "gsccode.h"
#include "gsccode.h"
#include "gsdevice.h"
#include "gsfont.h"
#include "gsgstate.h"
#include "gsmatrix.h"
#include "gstypes.h"

/*
 * Some structures and functions that are used by gdevdocxw.c and gdevtxtw.c.
 */

/*
 * Define the structure used to return glyph width information.  Note that
 * there are two different sets of width information: real-number (x,y)
 * values, which give the true advance width, and an integer value, which
 * gives an X advance width for WMode = 0 or a Y advance width for WMode = 1.
 * The return value from txt_glyph_width() indicates which of these is/are
 * valid.
 */
typedef struct txt_glyph_width_s {
    double w;
    gs_point xy;
    gs_point v;				/* glyph origin shift */
} txt_glyph_width_t;

typedef struct txt_glyph_widths_s {
    txt_glyph_width_t Width;		/* unmodified, for Widths */
    txt_glyph_width_t real_width;	/* possibly modified, for rendering */
    bool replaced_v;
} txt_glyph_widths_t;

int
txt_glyph_widths(gs_font *font, int wmode, gs_glyph glyph,
                 gs_font *orig_font, txt_glyph_widths_t *pwidths,
                 const double cdevproc_result[10]);

/* Try to convert glyph names/character codes to Unicode. We first try to see
 * if we have any Unicode information either from a ToUnicode CMap or GlyphNames2Unicode
 * table. If that fails we look at the glyph name to see if it starts 'uni'
 * in which case we assume the remainder of the name is the Unicode value. If
 * its not a glyph of that form then we search a bunch of tables whcih map standard
 * glyph names to Unicode code points. If that fails we finally just return the character code.
 */
int txt_get_unicode(gx_device *dev, gs_font *font, gs_glyph glyph, gs_char ch, unsigned short *Buffer);

void
txt_char_widths_to_uts(gs_font *font /* may be NULL for non-Type3 */,
                       txt_glyph_widths_t *pwidths);

float txt_calculate_text_size(gs_gstate *pgs, gs_font *ofont,
                              const gs_matrix *pfmat, gs_matrix *smat, gs_matrix *tmat,
                              gs_font *font, gx_device *pdev);

#endif
