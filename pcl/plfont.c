/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plfont.c */
/* PCL font handling library -- operations on entire fonts */
#include "memory_.h"
#include "stdio_.h"
#include "gdebug.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "plvalue.h"

/* Structure descriptors */
private_st_pl_font();

/* Imported procedures */
int gs_type42_get_metrics(P3(gs_font_type42 *pfont, uint glyph_index,
  float psbw[4]));

/* Define accessors for unaligned, big-endian quantities. */
#define u16(bptr) pl_get_uint16(bptr)
#define s16(bptr) pl_get_int16(bptr)
/**** ASSUME uint >= 32 BITS ****/
#define u32(bptr) (uint)pl_get_uint32(bptr)

/* ---------------- Utilities ---------------- */

/* Free a font.  This is the freeing procedure in the font dictionary. */
void
pl_free_font(gs_memory_t *mem, void *plf, client_name_t cname)
{	pl_font_t *plfont = plf;

	/* Free the characters. */
	if ( plfont->glyphs.table )
	  { uint i;
	    for ( i = plfont->glyphs.size; i > 0; )
	      { void *data = plfont->glyphs.table[--i].data;
	        if ( data )
		  gs_free_object(mem, data, cname);
	      }
	  }
	/* Free the font data itself. */
	gs_free_object(mem, (void *)plfont->char_glyphs.table, cname);
	gs_free_object(mem, (void *)plfont->glyphs.table, cname);
	gs_free_object(mem, (void *)plfont->header, cname);
	if ( plfont->pfont )	/* might be only partially constructed */
	  { gs_purge_font(plfont->pfont);
	    gs_free_object(mem, plfont->pfont, cname);
	  }
	gs_free_object(mem, plf, cname);
}

/* ---------------- Library callbacks ---------------- */

/* Get the name of a glyph.  We don't need to support this. */
private const char *
pl_glyph_name(gs_glyph glyph, uint *plen)
{	return 0;
}

/* Get a glyph from a known encoding.  We don't support this either. */
private gs_glyph
pl_known_encode(gs_char chr, int encoding_index)
{	return gs_no_glyph;
}

/* ---------------- Public procedures ---------------- */

/* Allocate a font. */
pl_font_t *
pl_alloc_font(gs_memory_t *mem, client_name_t cname)
{	pl_font_t *plfont =
	  gs_alloc_struct(mem, pl_font_t, &st_pl_font, cname);

	if ( plfont )
	  { /* Initialize pointers. */
	    plfont->pfont = 0;
	    plfont->header = 0;
	    plfont->glyphs.table = 0;
	    plfont->char_glyphs.table = 0;
	    /* Initialize other defaults. */
	    plfont->resolution.x = plfont->resolution.y = 0;
	    plfont->params.proportional_spacing = true;
	    memset(plfont->character_complement, 0xff, 8);
	    plfont->offsets.GC = plfont->offsets.GT = plfont->offsets.VT = -1;
	  }
	return plfont;
}

/* Fill in generic font boilerplate. */
int
pl_fill_in_font(gs_font *pfont, pl_font_t *plfont, gs_font_dir *pdir,
  gs_memory_t *mem)
{	plfont->pfont = pfont;
	/* Initialize generic font data. */
	pfont->next = pfont->prev = 0;
	pfont->memory = mem;
	pfont->dir = pdir;
	pfont->base = pfont;
	pfont->client_data = plfont;
	pfont->WMode = 0;
	pfont->PaintType = 0;
	pfont->StrokeWidth = 0;
	pfont->procs.init_fstack = gs_default_init_fstack;
	pfont->procs.next_char = gs_default_next_char;
	pfont->procs.callbacks.glyph_name = pl_glyph_name;
	pfont->procs.callbacks.known_encode = pl_known_encode;
	pfont->procs.define_font = gs_no_define_font;
	pfont->procs.make_font = gs_no_make_font;
	pfont->key_name.size = 0;
	pfont->font_name.size = 0;
	return 0;
}

/* Fill in bitmap font boilerplate. */
void
pl_fill_in_bitmap_font(gs_font_base *pfont, long unique_id)
{	/* It appears that bitmap fonts don't need a FontMatrix. */
	gs_make_identity(&pfont->FontMatrix);
	pfont->FontType = ft_user_defined;
	pfont->BitmapWidths = true;
	pfont->ExactSize = fbit_use_bitmaps;
	pfont->InBetweenSize = fbit_use_bitmaps;
	pfont->TransformedChar = fbit_transform_bitmaps;
	pl_bitmap_init_procs(pfont);
	/* We have no idea what the FontBBox should be. */
	pfont->FontBBox.p.x = pfont->FontBBox.p.y =
	  pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
}

/* Fill in TrueType font boilerplate. */
void
pl_fill_in_tt_font(gs_font_type42 *pfont, void *data, long unique_id)
{	gs_make_identity(&pfont->FontMatrix);
	pfont->FontType = ft_TrueType;
	pfont->BitmapWidths = true;
	pfont->ExactSize = fbit_use_outlines;
	pfont->InBetweenSize = fbit_use_outlines;
	pfont->TransformedChar = fbit_use_outlines;
	/* Initialize base font data. */
	/* We can't set the FontBBox until we've initialized */
	/* the Type 42 specific data. */
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
	/* Initialize Type 42 specific data. */
	pfont->data.proc_data = data;
	pl_tt_init_procs(pfont);
	gs_type42_font_init(pfont);
	pl_tt_finish_init(pfont, !data);
}

/*
 * Set large_sizes, scaling_technology, character_complement, offsets
 * (for TrueType fonts), and resolution (for bitmap fonts) by scanning
 * the segments of a segmented downloaded font.
 * This is used for PCL5 Format 15 and 16 fonts and for PCL XL fonts.
 * fst_offset is the offset of the Font Scaling Technology and Variety bytes;
 * the segment data runs from start_offset up to end_offset.
 * large_sizes = false indicates 2-byte segment sizes, true indicates 4-byte.
 */
int
pl_font_scan_segments(pl_font_t *plfont, int fst_offset, int start_offset,
  long end_offset, bool large_sizes, const pl_font_offset_errors_t *pfoe)
{	const byte *header = plfont->header;
	pl_font_scaling_technology_t fst = header[fst_offset];
	int wsize = (large_sizes ? 4 : 2);
	const byte *segment = header + start_offset;
	const byte *end = header + end_offset;
	const byte *null_segment = end - (2 + wsize);
	bool found = false;
	ulong seg_size;
	int illegal_font_data = pfoe->illegal_font_data;
#define return_scan_error(err)\
  return_error((err) ? (err) : illegal_font_data);

	if ( memcmp(null_segment, "\377\377", 2) /* NULL segment header */ )
	  return_scan_error(pfoe->missing_required_segment);
	if ( memcmp(null_segment + 2, "\0\0\0\0", wsize) /* NULL segment size */ )
	  return_scan_error(pfoe->illegal_null_segment_size);
	switch ( fst )
	  {
	  case plfst_bitmap:
	  case plfst_TrueType:
	    break;
	  default:
	    return_scan_error(pfoe->illegal_font_header_fields);
	  }
	if ( header[fst_offset + 1] ) /* variety, must be 0 */
	  return_scan_error(pfoe->illegal_font_header_fields);
	/* Scan the segments. */
	for ( ; end - segment >= 2 + wsize; segment += 2 + wsize + seg_size )
	    { uint seg_id = u16(segment);
	      const byte *sdata = segment + 2 + wsize;
#define id2(c1,c2) (((uint)(c1) << 8) + (c2))

	      seg_size = (large_sizes ? u32(segment + 2) : u16(segment + 2));
	      if ( seg_size + 2 + wsize > end - segment )
		return_error(illegal_font_data);
	      /* Handle segments common to all fonts. */
	      switch ( seg_id )
		{
		case 0xffff:		/* NULL segment ID */
		  if ( segment != null_segment )
		    return_error(illegal_font_data);
		  continue;
		case id2('V','I'):
		  continue;
		case id2('C', 'C'):
		  if ( seg_size != 8 )
		    return_error(illegal_font_data);
		  memcpy(plfont->character_complement, sdata, 8);
		  continue;
		default:
		  ;
		}
	      /* Handle segments specific to the scaling technology. */
	      if ( fst == plfst_bitmap )
		switch ( seg_id )
		  {
		  case id2('B','R'):
		    if ( seg_size != 4 )
		      return_scan_error(pfoe->illegal_BR_segment);
		    { uint xres = pl_get_uint16(sdata);
		      uint yres = pl_get_uint16(sdata + 2);
		      if ( xres == 0 || yres == 0 )
			return_scan_error(pfoe->illegal_BR_segment);
		      plfont->resolution.x = xres;
		      plfont->resolution.y = yres;
		    }
		    found = true;
		    break;
		  default:
		    if ( pfoe->illegal_font_segment < 0 )
		      return_error(pfoe->illegal_font_segment);
		  }
	      else		/* fst == plfst_TrueType */
		switch ( seg_id )
		  {
		  case id2('G','T'):
		    /*
		     * We don't do much checking here, but we do check that
		     * the segment starts with a table directory that
		     * includes at least 5 elements (gdir, head, hhea, hmtx,
		     * maxp -- but we don't check the actual names).
		     */
		    if ( seg_size < 12 + 5 * 16 ||
			 memcmp(sdata, "\000\001\000\000", 4) ||
			 u16(sdata + 4) < 5
		       )
		      return_scan_error(pfoe->illegal_GT_segment);
		    plfont->offsets.GT = segment - header;
		    found = true;
		    break;
		  case id2('G','C'):
		    if ( seg_size < 6 || u16(sdata) != 0 ||
			 seg_size != u16(sdata + 4) * 6 + 6
		       )
		      return_scan_error(pfoe->illegal_GC_segment);
		    plfont->offsets.GC = segment - header;
		    break;
		  case id2('V','T'):
		    /* Check for end of table mark */
		    if ( (seg_size & 3) != 0 || seg_size < 4 ||
			 u16(sdata + seg_size - 4) != 0xffff
		       )
		      return_scan_error(pfoe->illegal_VT_segment);
		    /* Check for table sorted by horizontal glyph ID */
		    { uint i;
		      for ( i = 0; i < seg_size - 4; i += 4 )
			if ( u16(sdata + i) >= u16(sdata + i + 4) )
			  return_scan_error(pfoe->illegal_VT_segment);
		    }
		    plfont->offsets.VT = segment - header;
		    break;
		  default:
		    if ( pfoe->illegal_font_segment < 0 )
		      return_error(pfoe->illegal_font_segment);
		  }
#undef id2
	    }
	  if ( !found )
	    return_scan_error(pfoe->missing_required_segment);
	  if ( segment != end )
	    return_error(illegal_font_data);
	  plfont->large_sizes = large_sizes;
	  plfont->scaling_technology = fst;
	  return 0;
#undef return_scan_error
}

/* Load a built-in (TrueType) font from external storage. */
int
pl_load_tt_font(FILE *in, gs_font_dir *pdir, gs_memory_t *mem,
  long unique_id, pl_font_t **pplfont)
{	ulong len = (fseek(in, 0L, SEEK_END), ftell(in));
	ulong size = 6 + len;	/* leave room for segment header */
	byte *data;

	if ( size != (uint)size )
	  { /*
	     * The font is too big to load in a single piece -- punt.
	     * The error message is bogus, but there isn't any more
	     * appropriate one.
	     */
	    fclose(in);
	    return_error(gs_error_VMerror);
	  }
	rewind(in);
	data = gs_alloc_bytes(mem, size, "pl_tt_load_font data");
	if ( data == 0 )
	  { fclose(in);
	    return_error(gs_error_VMerror);
	  }
	fread(data + 6, 1, len, in);
	fclose(in);
	/* Make a Type 42 font out of the TrueType data. */
	{ gs_font_type42 *pfont =
	    gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
			    "pl_tt_load_font(gs_font_type42)");
	  pl_font_t *plfont = pl_alloc_font(mem, "pl_tt_load_font(pl_font_t)");
	  int code;

	  if ( pfont == 0 || plfont == 0 )
	    code = gs_note_error(gs_error_VMerror);
	  else
	    { /* Initialize general font boilerplate. */
	      code = pl_fill_in_font((gs_font *)pfont, plfont, pdir, mem);
	      if ( code >= 0 )
		{ /* Initialize TrueType font boilerplate. */
		  plfont->header = data;
		  plfont->header_size = size;
		  plfont->scaling_technology = plfst_TrueType;
		  plfont->font_type = plft_Unicode;
		  plfont->large_sizes = true;
		  plfont->offsets.GT = 0;
		  pl_fill_in_tt_font(pfont, data, unique_id);
		  code = gs_definefont(pdir, (gs_font *)pfont);
		  if ( code >= 0 )
		    { /*
		       * Set the nominal design width to the width of a
		       * space character.  If there isn't one, set the
		       * design width arbitrarily at 0.6 em.
		       */
		      gs_char space = ' ';
		      uint glyph_index =
			(*pfont->procs.encode_char)
			  (NULL, (gs_font *)pfont, &space);
		      float sbw[4];

		      if ( gs_type42_get_metrics(pfont, glyph_index, sbw) < 0 )
			plfont->params.pitch_100ths = 60;
		      else
			plfont->params.pitch_100ths = sbw[2] * 100;
		    }
		}
	    }
	  if ( code < 0 )
	    { gs_free_object(mem, plfont, "pl_tt_load_font(pl_font_t)");
	      gs_free_object(mem, pfont, "pl_tt_load_font(gs_font_type42)");
	      gs_free_object(mem, data, "pl_tt_load_font data");
	      return code;
	    }
	  *pplfont = plfont;
	}
	return 0;
}
