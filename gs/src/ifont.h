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
/* Interpreter internal font representation */

#ifndef ifont_INCLUDED
#  define ifont_INCLUDED

#include "gsccode.h"		/* for gs_glyph, NUM_KNOWN_ENCODINGS */
#include "gsstype.h"		/* for extern_st */

/* The external definition of fonts is given in the PostScript manual, */
/* pp. 91-93. */

/* The structure given below is 'client data' from the viewpoint */
/* of the library.  font-type objects (t_struct/st_font, "t_fontID") */
/* point directly to a gs_font.  */

typedef struct font_data_s {
    ref dict;			/* font dictionary object */
    ref BuildChar;
    ref BuildGlyph;
    ref Encoding;
    ref CharStrings;
    union _fs {
	struct _f1 {
	    ref OtherSubrs;	/* from Private dictionary */
	    ref Subrs;		/* from Private dictionary */
	    ref GlobalSubrs;	/* from Private dictionary, */
	    /* for Type 2 charstrings */
	} type1;
	struct _f42 {
	    ref sfnts;
	    ref CIDMap;		/* for CIDFontType 2 fonts */
	    ref GlyphDirectory;
	} type42;
	struct _fc0 {
	    ref GlyphDirectory;
	    ref GlyphData;	/* (if preloaded) string or array of strings */
	    ref DataSource;	/* (if not preloaded) reusable stream */
	} cid0;
    } u;
} font_data;

/*
 * Even though the interpreter's part of the font data actually
 * consists of refs, allocating it as refs tends to create sandbars;
 * since it is always allocated and freed as a unit, we can treat it
 * as an ordinary structure.
 */
/* st_font_data is exported for zdefault_make_font in zfont.c. */
extern_st(st_font_data);
#define public_st_font_data()	/* in zbfont.c */\
  gs_public_st_ref_struct(st_font_data, font_data, "font_data")
#define pfont_data(pfont) ((font_data *)((pfont)->client_data))
#define pfont_dict(pfont) (&pfont_data(pfont)->dict)

/* Registered encodings, for the benefit of platform fonts, `seac', */
/* and compiled font initialization. */
/* This is a t_array ref that points to the encodings. */
#define registered_Encodings_countof NUM_KNOWN_ENCODINGS
extern ref registered_Encodings;

#define registered_Encoding(i) (registered_Encodings.value.refs[i])
#define StandardEncoding registered_Encoding(0)

/* Internal procedures shared between modules */

/* In zchar.c */
int font_bbox_param(P2(const ref * pfdict, double bbox[4]));

/* In zfont.c */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif
int font_param(P2(const ref * pfdict, gs_font ** ppfont));
bool zfont_mark_glyph_name(P2(gs_glyph glyph, void *ignore_data));

#endif /* ifont_INCLUDED */
