/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

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

/* Note that we misappropriate pfont->StrokeWidth for the chord angle. */

/* The client handles all encoding issues for these fonts. */
/* The fonts themselves use Unicode indexing. */
private gs_glyph
hpgl_stick_arc_encode_char(gs_show_enum *penum, gs_font *pfont, gs_char *pchr)
{	return (gs_glyph)*pchr;
}

/* The stick font is fixed-pitch. */
private int
hpgl_stick_char_width(const pl_font_t *plfont, const pl_symbol_map_t *map,
  const gs_matrix *pmat, uint uni_code, gs_point *pwidth)
{	bool in_range = hpgl_unicode_stick_index(uni_code) != 0;

	if ( pwidth ) {
	  gs_distance_transform(0.667, 0.0, pmat, pwidth);
	}
	return in_range;
}
/* The arc font is proportionally spaced. */
private int
hpgl_arc_char_width(const pl_font_t *plfont, const pl_symbol_map_t *map,
  const gs_matrix *pmat, uint uni_code, gs_point *pwidth)
{	uint char_code = hpgl_unicode_stick_index(uni_code);
	bool in_range = char_code != 0;

	if ( pwidth ) {
	  gs_distance_transform(hpgl_stick_arc_width(char_code) / 15.0
				  * 0.667 * hpgl_arc_font_condensation,
				0.0, pmat, pwidth);
	}
	return in_range;
}

/* Forward procedure declarations */
private int hpgl_arc_rmoveto(P3(void *data, int dx, int dy));
private int hpgl_arc_rlineto(P3(void *data, int dx, int dy));
private int hpgl_arc_add_arc(P6(void *data, int cx, int cy,
  const gs_int_point *start, int sweep_angle, floatp chord_angle));

/* Add a symbol to the path. */
private int
hpgl_stick_arc_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_glyph uni_code, floatp condensation)
{	uint char_index = hpgl_unicode_stick_index(uni_code);
	double chord_angle;
	int width;
	gs_matrix save_ctm;
	int code;

	if ( char_index == 0 )
	  return 0;
	chord_angle = pfont->StrokeWidth;
	width = (chord_angle == 0 ? hpgl_stick_arc_width(char_index) : 15);
	/* The TRM says the stick font is based on a 32x32 unit cell, */
	/* but the font we're using here is only 15x15. */
	/* Also, per TRM 23-18, the character cell is only 2/3 the */
	/* point size. */
	gs_setcharwidth(penum, pgs, width / 15.0 * 0.667 * condensation, 0.0);
	gs_currentmatrix(pgs, &save_ctm);
#define scale (1.0 / 15 * 2 / 3)
	gs_scale(pgs, scale * condensation, scale);
#undef scale
	gs_moveto(pgs, 0.0, 0.0);
	{ const hpgl_stick_segment_procs_t draw_procs = {
	    hpgl_arc_rmoveto, hpgl_arc_rlineto, hpgl_arc_add_arc
	  };
	  code = hpgl_stick_arc_segments(&draw_procs, (void *)pgs,
					 char_index, chord_angle);
	}
	gs_setmatrix(pgs, &save_ctm);
	if ( code < 0 )
	  return code;
	/* Set predictable join and cap styles. */
	gs_setlinejoin(pgs, gs_join_bevel);
	gs_setmiterlimit(pgs, 2.61); /* start beveling at 45 degrees */
	gs_setlinecap(pgs, gs_cap_triangle);
	return gs_stroke(pgs);
}

/* Add a point or line. */
private int
hpgl_arc_rmoveto(void *data, int dx, int dy)
{	return gs_rmoveto((gs_state *)data, (floatp)dx, (floatp)dy);
}
private int
hpgl_arc_rlineto(void *data, int dx, int dy)
{	return gs_rlineto((gs_state *)data, (floatp)dx, (floatp)dy);
}

/* Add an arc given the center, start point, and sweep angle. */
private int
hpgl_arc_add_arc(void *data, int cx, int cy, const gs_int_point *start,
  int sweep_angle, floatp chord_angle)
{	gs_state *pgs = (gs_state *)data;
	int dx = start->x - cx, dy = start->y - cy;
	double radius, angle;
	int code = 0;

	if ( dx == 0 ) {
	  radius = any_abs(dy);
	  angle = (dy >= 0 ? 90.0 : 270.0);
	} else if ( dy == 0 ) {
	  radius = any_abs(dx);
	  angle = (dx >= 0 ? 0.0 : 180.0);
	} else {
	  radius = hypot((floatp)dx, (floatp)dy);
	  angle = atan2((floatp)dy, (floatp)dx) * radians_to_degrees;
	}
	if ( chord_angle == 0 ) {
	  /* Just add the arc. */
	  code = (sweep_angle < 0 ?
		  gs_arcn(pgs, (floatp)cx, (floatp)cy, radius, angle,
			  angle + sweep_angle) :
		  gs_arc(pgs, (floatp)cx, (floatp)cy, radius, angle,
			 angle + sweep_angle));
	} else {
	  double count = ceil(any_abs(sweep_angle) / chord_angle);
	  double delta = sweep_angle / count;

	  while ( count-- ) {
	    gs_sincos_t sincos;

	    angle += delta;
	    gs_sincos_degrees(angle, &sincos);
	    code = gs_lineto(pgs, cx + sincos.cos * radius,
			     cy + sincos.sin * radius);
	    if ( code < 0 )
	      break;
	  }
	}
	return code;
}

private int
hpgl_stick_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char ignore_chr, gs_glyph uni_code)
{	return hpgl_stick_arc_build_char(penum, pgs, pfont, uni_code, 1.0);
}
private int
hpgl_arc_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char ignore_chr, gs_glyph uni_code)
{	return hpgl_stick_arc_build_char(penum, pgs, pfont, uni_code,
					 hpgl_arc_font_condensation);
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
	pfont->procs.encode_char = hpgl_stick_arc_encode_char;
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
	pfont->StrokeWidth = 22.5;
#define plfont ((pl_font_t *)pfont->client_data)
	pfont->procs.build_char = hpgl_stick_build_char;
	plfont->char_width = hpgl_stick_char_width;
#undef plfont
}
void
hpgl_fill_in_arc_font(gs_font_base *pfont, long unique_id)
{	hpgl_fill_in_stick_arc_font(pfont, unique_id);
	pfont->StrokeWidth = 0;  /* don't flatten arcs */
#define plfont ((pl_font_t *)pfont->client_data)
	pfont->procs.build_char = hpgl_arc_build_char;
	plfont->char_width = hpgl_arc_char_width;
#undef plfont
}
