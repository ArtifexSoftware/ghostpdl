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


/* Type 1 / Type 2 character rendering operator procedures */

#ifndef ichar1_INCLUDED
#  define ichar1_INCLUDED

#include "gxfont.h"
#include "gsgdata.h"
#include "gxfapi.h"
#include "gstype1.h"
#include "iref.h"

/* ---------------- Public ---------------- */

/* Render a Type 1 or Type 2 outline. */
/* This is the entire implementation of the .type1/2execchar operators. */
int charstring_execchar(i_ctx_t *i_ctx_p, int font_type_mask);

/* ---------------- Internal ---------------- */

/*
 * Get a Type 1 or Type 2 glyph outline.  This is the glyph_outline
 * procedure for the font.
 */
font_proc_glyph_outline(zchar1_glyph_outline);

/*
 * Get a glyph outline given a CharString.  The glyph_outline procedure
 * for CIDFontType 0 fonts uses this.
 */
int zcharstring_outline(gs_font_type1 *pfont, int WMode, const ref *pgref,
                        const gs_glyph_data_t *pgd,
                        const gs_matrix *pmat, gx_path *ppath, double sbw[4]);

int
z1_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
              int members, gs_glyph_info_t *info);

int z1_glyph_info_generic(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
              int members, gs_glyph_info_t *info, font_proc_glyph_info((*proc)),
              int wmode);

int z1_set_cache(i_ctx_t *i_ctx_p, gs_font_base *pbfont, ref *cnref,
            gs_glyph glyph, op_proc_t cont, op_proc_t *exec_cont);

#endif /* ichar1_INCLUDED */
