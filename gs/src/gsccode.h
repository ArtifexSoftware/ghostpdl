/* Copyright (C) 1993, 2000, 2002 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Types for character codes */

#ifndef gsccode_INCLUDED
#  define gsccode_INCLUDED

/*
 * Define a character code.  Normally this is just a single byte from a
 * string, but because of composite fonts, character codes must be
 * at least 32 bits.
 */
typedef ulong gs_char;

#define GS_NO_CHAR ((gs_char)~0L)
/* Backward compatibility */
#define gs_no_char GS_NO_CHAR

/*
 * Define a character glyph code, a.k.a. character name.
 * gs_glyphs from 0 to 2^31-1 are (PostScript) names; gs_glyphs 2^31 and
 * above are CIDs, biased by 2^31.
 */
typedef ulong gs_glyph;

#define GS_NO_GLYPH ((gs_glyph)0x7fffffff)
#if arch_sizeof_long > 4
#  define GS_MIN_CID_GLYPH ((gs_glyph)0x80000000L)
#else
/* Avoid compiler warnings about signed/unsigned constants. */
#  define GS_MIN_CID_GLYPH ((gs_glyph)~0x7fffffff)
#endif
#define GS_MAX_GLYPH max_ulong
/* Backward compatibility */
#define gs_no_glyph GS_NO_GLYPH
#define gs_min_cid_glyph GS_MIN_CID_GLYPH
#define gs_max_glyph GS_MAX_GLYPH

/* Define a procedure for marking a gs_glyph during garbage collection. */
typedef bool (*gs_glyph_mark_proc_t)(gs_glyph glyph, void *proc_data);

/* Define the indices for known encodings. */
typedef enum {
    ENCODING_INDEX_UNKNOWN = -1,
	/* Real encodings.  These must come first. */
    ENCODING_INDEX_STANDARD = 0,
    ENCODING_INDEX_ISOLATIN1,
    ENCODING_INDEX_SYMBOL,
    ENCODING_INDEX_DINGBATS,
    ENCODING_INDEX_WINANSI,
    ENCODING_INDEX_MACROMAN,
    ENCODING_INDEX_MACEXPERT,
#define NUM_KNOWN_REAL_ENCODINGS 7
	/* Pseudo-encodings (glyph sets). */
    ENCODING_INDEX_MACGLYPH,	/* Mac glyphs */
    ENCODING_INDEX_ALOGLYPH,	/* Adobe Latin glyph set */
    ENCODING_INDEX_ALXGLYPH,	/* Adobe Latin Extended glyph set */
    ENCODING_INDEX_CFFSTRINGS	/* CFF StandardStrings */
#define NUM_KNOWN_ENCODINGS 11
} gs_encoding_index_t;
#define KNOWN_REAL_ENCODING_NAMES\
  "StandardEncoding", "ISOLatin1Encoding", "SymbolEncoding",\
  "DingbatsEncoding", "WinAnsiEncoding", "MacRomanEncoding",\
  "MacExpertEncoding"

/*
 * For fonts that use more than one method to identify glyphs, define the
 * glyph space for the values returned by procedures that return glyphs.
 * Note that if a font uses only one method (such as Type 1 fonts, which
 * only use names, or TrueType fonts, which only use indexes), the
 * glyph_space argument is ignored.
 */
typedef enum gs_glyph_space_s {
    GLYPH_SPACE_NAME,		/* names (if available) */
    GLYPH_SPACE_INDEX		/* indexes (if available) */
} gs_glyph_space_t;

/*
 * Define a procedure for mapping a glyph to its (string) name.  This is
 * currently used only for CMaps: it is *not* the same as the glyph_name
 * procedure in fonts.
 */
typedef int (*gs_glyph_name_proc_t)(gs_glyph glyph, gs_const_string *pstr,
				    void *proc_data);

#endif /* gsccode_INCLUDED */
