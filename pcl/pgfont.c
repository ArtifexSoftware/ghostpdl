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
/*$Id$ */

/* pgfont.c */
/* HP-GL/2 stick and arc font */
#include "math_.h"
#include "gstypes.h"
#include "gsccode.h"
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"		/* for gscoord.h, gspath.h */
#include "gsmatrix.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gxfixed.h"		/* for gxchar.h */
#include "gxchar.h"
#include "gxfarith.h"
#include "gxfont.h"
#include "plfont.h"
#include "pgfdata.h"
#include "pgfont.h"

/* The client handles all encoding issues for these fonts. */
/* The fonts themselves use Unicode indexing. */
private gs_glyph
hpgl_stick_arc_encode_char(gs_font *pfont, gs_char chr, gs_glyph not_used)
{	return (gs_glyph)chr;
}

/* The stick font is fixed-pitch. */
private int
hpgl_stick_char_width(const pl_font_t *plfont, const void *pgs, uint uni_code, gs_point *pwidth)
{	
    /* NB need an interface function call to verify the character exists */
    if ( (uni_code >= 0x20)  && (uni_code <= 0xff) )
	pwidth->x = hpgl_stick_arc_width(uni_code, HPGL_STICK_FONT);
    else
	/* doesn't exist */
	return 1;
    return 0;
}

private int
hpgl_stick_char_metrics(const pl_font_t *plfont, const void *pgs, uint uni_code, float metrics[4])
{
    gs_point width;
    /* never a vertical substitute */
    metrics[1] = metrics[3] = 0;
    /* no lsb */
    metrics[0] = 0;
    /* just get the width */
    if ( (hpgl_stick_char_width(plfont, pgs, uni_code, &width)) == 1 )
	/* doesn't exist */
	return 1;
    metrics[2] = width.x;
    return 0;
}    

/* The arc font is proportionally spaced. */
private int
hpgl_arc_char_width(const pl_font_t *plfont, const void *pgs, uint uni_code, gs_point *pwidth)
{	
    /* NB need an interface function call to verify the character exists */
    if ( (uni_code >= 0x20)  && (uni_code <= 0xff) ) {
        pwidth->x = hpgl_stick_arc_width(uni_code, HPGL_ARC_FONT)
	  / 1024.0  /* convert to ratio of cell size to be multiplied by point size */
	  * 0.667;   /* TRM 23-18 cell is 2/3 of point size */
    }      
    else
	/* doesn't exist */
	return 1;
    return 0;
}

private int
hpgl_arc_char_metrics(const pl_font_t *plfont, const void *pgs, uint uni_code, float metrics[4])
{
    gs_point width;
    /* never a vertical substitute */
    metrics[1] = metrics[3] = 0;
    /* no lsb */
    metrics[0] = 0;
    /* just get the width */
    if ( (hpgl_arc_char_width(plfont, pgs, uni_code, &width)) == 1 )
	/* doesn't exist */
	return 1;
    metrics[2] = width.x;
    return 0;
}    

/* Add a symbol to the path. */
private int
hpgl_stick_arc_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_glyph uni_code, hpgl_font_type_t font_type)
{	
    int width;
    gs_matrix save_ctm;
    int code;

    /* we assert the font is present at this point */
    width = hpgl_stick_arc_width(uni_code, font_type);

    /* *** incorrect comment The TRM says the stick font is based on a
       32x32 unit cell, */
    /* but the font we're using here is only 15x15. */
    /* Also, per TRM 23-18, the character cell is only 2/3 the */
    /* point size. */
    gs_setcharwidth(penum, pgs, width / 1024.0 * 0.667, 0.0);
    gs_currentmatrix(pgs, &save_ctm);
    gs_scale(pgs, 1.0 / 1024.0 * .667, 1.0 / 1024.0 * .667);
#undef scale
    gs_moveto(pgs, 0.0, 0.0);
    code = hpgl_stick_arc_segments((void *)pgs, uni_code, font_type);
    gs_setmatrix(pgs, &save_ctm);
    if ( code < 0 )
	return code;
    /* Set predictable join and cap styles. */
    gs_setlinejoin(pgs, gs_join_round);
    gs_setmiterlimit(pgs, 2.61); /* start beveling at 45 degrees */
    gs_setlinecap(pgs, gs_cap_round);
    return gs_stroke(pgs);
}

private int
hpgl_stick_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char ignore_chr, gs_glyph uni_code)
{	return hpgl_stick_arc_build_char(penum, pgs, pfont, uni_code, HPGL_STICK_FONT);
}
private int
hpgl_arc_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char ignore_chr, gs_glyph uni_code)
{	return hpgl_stick_arc_build_char(penum, pgs, pfont, uni_code, HPGL_ARC_FONT);
					 
}

/* Fill in stick/arc font boilerplate. */
private void
hpgl_fill_in_stick_arc_font(gs_font_base *pfont, long unique_id)
{	/* The way the code is written requires FontMatrix = identity. */
	gs_make_identity(&pfont->FontMatrix);
	pfont->FontType = ft_user_defined;
	pfont->PaintType = 1;		/* stroked fonts */
	pfont->BitmapWidths = false;
	pfont->ExactSize = fbit_use_outlines;
	pfont->InBetweenSize = fbit_use_outlines;
	pfont->TransformedChar = fbit_use_outlines;
	pfont->procs.encode_char = (void *)hpgl_stick_arc_encode_char; /* FIX ME (void *) */
	/* p.y of the FontBBox is a guess, because of descenders. */
	/* Because of descenders, we have no real idea what the */
	/* FontBBox should be. */
	pfont->FontBBox.p.x = 0;
	pfont->FontBBox.p.y = -0.333;
	pfont->FontBBox.q.x = 0.667;
	pfont->FontBBox.q.y = 0.667;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
}
void
hpgl_fill_in_stick_font(gs_font_base *pfont, long unique_id)
{	hpgl_fill_in_stick_arc_font(pfont, unique_id);
#define plfont ((pl_font_t *)pfont->client_data)
	pfont->procs.build_char = (void *)hpgl_stick_build_char; /* FIX ME (void *) */
	plfont->char_width = hpgl_stick_char_width;
	plfont->char_metrics = hpgl_stick_char_metrics;
#undef plfont
}
void
hpgl_fill_in_arc_font(gs_font_base *pfont, long unique_id)
{	hpgl_fill_in_stick_arc_font(pfont, unique_id);
#define plfont ((pl_font_t *)pfont->client_data)
	pfont->procs.build_char = (void *)hpgl_arc_build_char; /* FIX ME (void *) */
	plfont->char_width = hpgl_arc_char_width;
	plfont->char_metrics = hpgl_arc_char_metrics;
#undef plfont
}
