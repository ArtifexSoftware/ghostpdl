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


/* pxfont.h */
/* Interface to PCL XL utilities */

#ifndef pxfont_INCLUDED
#  define pxfont_INCLUDED

/* Rename types. */
#ifdef px_font_t_DEFINED
#  define pl_font_t_DEFINED
#endif
#define pl_font_s px_font_s
#define pl_font_t px_font_t
#include "plfont.h"
#define px_font_t_DEFINED

/*
 * This file provides a layer of specialization and renaming around
 * the font handling library.
 *
 * We store all font names as Unicode strings.  Each element of a font
 * name is a char16 (a 16-bit unsigned value in native byte order).
 */

/* Define storage locations for fonts. */
typedef enum {
  pxfsDownLoaded,
  pxfsInternal,
  pxfsMassStorage
} px_font_storage_t;

/* Fill in generic font boilerplate. */
#define px_fill_in_font(pfont, pxfont, pxs)\
  pl_fill_in_font(pfont, pxfont, pxs->font_dir, pxs->memory, "nameless_font")

/*
 * Define a font.  The caller must fill in pxfont->storage and ->font_type.
 */
int px_define_font(px_font_t *pxfont, byte *header, ulong size,
                   gs_id id, px_state_t *pxs);

/*
 * Look up a font name and return the base font.  This procedure implements
 * most of the SetFont operator.  Note that this procedure will widen and/or
 * byte-swap the font name if necessary.
 */
int px_find_font(px_value_t *pfnv, uint symbol_set, px_font_t **ppxfont,
                 px_state_t *pxs);

/* Look up a font name and return an existing font. */
/* This procedure may widen and/or byte-swap the font name. */
/* If this font is supposed to be built in but no .TTF file is available, */
/* return >= 0 and store 0 in *ppxfont. */
int px_find_existing_font(px_value_t *pfnv, px_font_t **ppxfont,
                          px_state_t *pxs);

/*
 * Concatenate a widened (16-bit) font name onto an error message string.
 */
void px_concat_font_name(char *message, uint max_message,
                         const px_value_t *pfnv);

/*
 * Paint text or add it to the path.
 * This procedure implements the Text and TextPath operators.
 */
int px_text(px_args_t *par, px_state_t *pxs, bool to_path);

/*
 * Free a font.  This is the freeing procedure in the font dictionary.
 * We have to define the name without parameters so that we can use it
 * as a procedure constant.
 */
/*#define px_free_font(mem, pxf, cname) pl_free_font(mem, pxf, cname)*/
#define px_free_font pl_free_font

#endif				/* pxfont_INCLUDED */
