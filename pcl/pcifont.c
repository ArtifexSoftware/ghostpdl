/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
 * Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcifont.c */
/* Unhinted Intellifont rasterizer */
#include "std.h"
#include "gsmemory.h"
#include "gstypes.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gspaint.h"
#include "gspath.h"
#include "gxfont.h"
#include "plfont.h"
#include "plvalue.h"

/* We don't have to do any character encoding, since Intellifonts are */
/* indexed by character code (if bound) or MSL code (if unbound). */
private gs_glyph
pcl_intelli_encode_char(gs_show_enum *penum, gs_font *pfont, gs_char *pchr)
{	return (gs_glyph)*pchr;
}

/* Define the structure of the Intellifont metrics. */
typedef struct intelli_metrics_s {
  byte charSymbolBox[4][2];
  byte charEscapementBox[4][2];
  byte halfLine[2];
  byte centerline[2];
} intelli_metrics_t;

/* Render a character for an Intellifont. */
private int
pcl_intelli_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph glyph)
{	pl_font_t *plfont = (pl_font_t *)pfont->client_data;
	const byte *cdata = pl_font_lookup_glyph(plfont, glyph)->data;

	if ( cdata == 0 )
	  return 0;
	cdata += 4;		/* skip PCL character header */
	{ const intelli_metrics_t *metrics =
	    (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));
	  const byte *outlines = cdata + pl_get_uint16(cdata + 6);
	  uint num_loops = pl_get_uint16(outlines);
	  uint i;
	  int code;

	  { float wbox[6];	/* wx, wy, llx/y, urx/y */
	    wbox[0] = pl_get_int16(metrics->charEscapementBox[2]) -
	      pl_get_int16(metrics->charEscapementBox[0]);
	    wbox[1] = 0;
	    wbox[2] = pl_get_int16(metrics->charSymbolBox[0]);
	    wbox[3] = pl_get_int16(metrics->charSymbolBox[1]);
	    wbox[4] = pl_get_int16(metrics->charSymbolBox[2]);
	    wbox[5] = pl_get_int16(metrics->charSymbolBox[3]);
	    code = gs_setcachedevice(penum, pgs, wbox);
	  }
	  if ( code < 0 )
	    return code;
	  for ( i = 0; i < num_loops; ++i )
	    { const byte *xyc = cdata + pl_get_uint16(outlines + 4 + i * 8);
	      uint num_points = pl_get_uint16(xyc);
	      uint num_aux_points = pl_get_uint16(xyc + 2);
	      const byte *x_coords = xyc + 4;
	      const byte *y_coords = x_coords + num_points * 2;
	      const byte *x_aux_coords = y_coords + num_points * 2;
	      const byte *y_aux_coords = x_aux_coords + num_aux_points;
	      int x_prev, y_prev;
	      uint j;

	      /* For the moment, just draw straight lines. */
	      for ( j = 0; j < num_points; x_coords += 2, y_coords += 2, ++j )
		{ int x = pl_get_uint16(x_coords) & 0x3fff;
		  int y = pl_get_uint16(y_coords) & 0x3fff;

		  if ( j == 0 )
		    code = gs_moveto(pgs, (floatp)x, (floatp)y);
		  else
		    code = gs_lineto(pgs, (floatp)x, (floatp)y);
		  if ( code < 0 )
		    return code;
		  if ( (*x_coords & 0x80) && j != 0 )
		    { code = gs_lineto(pgs, (x + x_prev) / 2 + *x_aux_coords++,
				       (y + y_prev) / 2 + *y_aux_coords++);
		      if ( code < 0 )
			return code;
		    }
		  x_prev = x, y_prev = y;
		}
	      /****** HANDLE FINAL AUX POINT ******/
	      code = gs_closepath(pgs);
	      if ( code < 0 )
		return code;
	    }
	}
	/* Since we don't take into account which side of the loops is */
	/* outside, we take the easy way out.... */
	return gs_eofill(pgs);
}

/* Get character existence and escapement for an Intellifont. */
private int
pcl_intelli_char_width(const pl_font_t *plfont, const pl_symbol_map_t *map,
  const gs_matrix *pmat, uint char_code, gs_point *pwidth)
{	const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;

	if ( !pwidth )
	  return (cdata == 0 ? 1 : 0);
	if ( cdata == 0 )
	  { pwidth->x = pwidth->y = 0;
	    return 1;
	  }
	cdata += 4;		/* skip PCL character header */
	{ const intelli_metrics_t *metrics =
	    (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));
	  floatp wx =
	    pl_get_int16(metrics->charEscapementBox[2]) -
	      pl_get_int16(metrics->charEscapementBox[0]);

	  return gs_distance_transform(wx, 0.0, pmat, pwidth);
	}
}

/* Fill in Intellifont boilerplate. */
void
pcl_fill_in_intelli_font(gs_font_base *pfont, long unique_id)
{	/* Intellifonts have an 8782-unit design space. */
	gs_make_scaling(1.0/8782, 1.0/8782, &pfont->FontMatrix);
	pfont->FontType = ft_user_defined;
	pfont->BitmapWidths = true;
	pfont->ExactSize = fbit_use_outlines;
	pfont->InBetweenSize = fbit_use_outlines;
	pfont->TransformedChar = fbit_use_outlines;
	pfont->procs.encode_char = pcl_intelli_encode_char;
	pfont->procs.build_char = pcl_intelli_build_char;
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = pcl_intelli_char_width;
#undef plfont
	/* We have no idea what the FontBBox should be. */
	pfont->FontBBox.p.x = pfont->FontBBox.p.y =
	  pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
}
