/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
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

/*
 * At the beginning of execution, we acquire a block of N+1 unique IDs for
 * the built-in fonts: if the first ID is B, we assign B through B+N-1 for
 * the built-in fonts, and B+N for the error page font.  (The value B is
 * stored in the known_fonts_base_id element of the px_state_t.)  At the end
 * of each session, we purge from the cache all fonts whose UniqueID is
 * greater than B+N.
 */
extern const uint px_num_known_fonts;

/* Fill in generic font boilerplate. */
#define px_fill_in_font(pfont, pxfont, pxs)\
  pl_fill_in_font(pfont, pxfont, pxs->font_dir, pxs->memory)

/*
 * Define a font.  The caller must fill in pxfont->storage and ->font_type.
 */
int px_define_font(P5(px_font_t *pxfont, const byte *header, ulong size,
		      gs_id id, px_state_t *pxs));

/*
 * Look up a font name and return the base font.  This procedure implements
 * most of the SetFont operator.  Note that this procedure will widen and/or
 * byte-swap the font name if necessary.
 */
int px_find_font(P4(px_value_t *pfnv, uint symbol_set, px_font_t **ppxfont,
		    px_state_t *pxs));

/* Look up a font name and return an existing font. */
/* This procedure may widen and/or byte-swap the font name. */
/* If this font is supposed to be built in but no .TTF file is available, */
/* return >= 0 and store 0 in *ppxfont. */
int px_find_existing_font(P3(px_value_t *pfnv, px_font_t **ppxfont,
			     px_state_t *pxs));

/*
 * Concatenate a widened (16-bit) font name onto an error message string.
 */
void px_concat_font_name(P3(char *message, uint max_message,
			    const px_value_t *pfnv));

/*
 * Paint text or add it to the path.
 * This procedure implements the Text and TextPath operators.
 */
int px_text(P3(px_args_t *par, px_state_t *pxs, bool to_path));

/*
 * Free a font.  This is the freeing procedure in the font dictionary.
 * We have to define the name without parameters so that we can use it
 * as a procedure constant.
 */
/*#define px_free_font(mem, pxf, cname) pl_free_font(mem, pxf, cname)*/
#define px_free_font pl_free_font

#endif				/* pxfont_INCLUDED */
