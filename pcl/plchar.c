/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plchar.c */
/* PCL font handling library -- operations on individual characters */
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"		/* for gdebug.h */
#include "gdebug.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gsimage.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsbittab.h"
#include "gxarith.h"		/* for igcd */
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "plvalue.h"
#include "gscoord.h"
#include "gsstate.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxpath.h"
/* We really shouldn't need the following, but currently they are needed */
/* for pgs->path and penum->log2_current_scale in pl_tt_build_char. */
#include "gxfixed.h"
#include "gxchar.h"
#include "gxfcache.h"
#include "gzstate.h"

/* Define whether to cache bitmap characters. */
/* Logically this seems unnecessary, but it enables a much faster path */
/* through the character rendering code. */
#define CACHE_BITMAP_CHARS

/* Define whether to cache TrueType characters. */
/* This would only be disabled for debugging. */
#define CACHE_TRUETYPE_CHARS

/* Structure descriptors */
gs_private_st_ptrs1(st_pl_font_glyph, pl_font_glyph_t, "pl_font_glyph_t",
  pl_font_glyph_enum_ptrs, pl_font_glyph_reloc_ptrs, data);
gs_private_st_element(st_pl_font_glyph_element, pl_font_glyph_t,
  "pl_font_glyph_t[]",
  pl_font_glyph_elt_enum_ptrs, pl_font_glyph_elt_reloc_ptrs, st_pl_font_glyph);

/* Define accessors for unaligned, big-endian quantities. */
#define u16(bptr) pl_get_uint16(bptr)
#define s16(bptr) pl_get_int16(bptr)
/**** ASSUME uint >= 32 BITS ****/
#define u32(bptr) (uint)pl_get_uint32(bptr)

/* ---------------- Utilities ---------------- */

/* Look up a glyph in a font.  Return a pointer to the glyph's slot */
/* (data != 0) or where it should be added (data == 0). */
private pl_font_glyph_t *
pl_font_lookup_glyph(const pl_font_t *plfont, gs_glyph glyph)
{	uint size = plfont->glyphs.size;
	uint skip = plfont->glyphs.skip;
	uint index = glyph % size;
	pl_font_glyph_t *pfg;

	while ( (pfg = plfont->glyphs.table + index)->data &&
		pfg->glyph != glyph
	      )
	  index = (index >= skip ? index : index + size) - skip;
	return pfg;
}

/* ---------------- Bitmap font support ---------------- */

/* Encode a character for a bitmap font.  This is simple, because */
/* bitmap fonts are always bound. */
private gs_glyph
pl_bitmap_encode_char(gs_show_enum *penum, gs_font *pfont, gs_char *pchr)
{	return (gs_glyph)*pchr;
}

/* Get character existence and escapement for a bitmap font. */
/* This is simple for the same reason. */
private int
pl_bitmap_char_width(const pl_font_t *plfont, const pl_symbol_map_t *map,
  const gs_matrix *pmat, uint char_code, gs_point *pwidth)
{	const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;

	if ( !pwidth )
	  return (cdata == 0 ? 1 : 0);
	if ( cdata == 0 )
	  { pwidth->x = pwidth->y = 0;
	    return 1;
	  }
	if ( cdata[0] == 0 )
	  { /* PCL XL characters don't have an escapement. */
	    pwidth->x = pwidth->y = 0;
	    return 0;
	  }
	{ const byte *params = cdata + 6;
	  floatp wx =
	    (plfont->header[13] ? /* variable pitch */
	     s16(params + 8) * 0.25 :
	     s16(params) /*lsb*/ + u16(params + 4) /*width*/);

	  return gs_distance_transform(wx, 0.0, pmat, pwidth);
	}
}

/*
 * For pseudo-bolding, we have to "smear" a bitmap horizontally and
 * vertically by ORing together a rectangle of bits below and to the left of
 * each output bit.  We do this separately for horizontal and vertical
 * smearing.  Eventually, we will replace the current procedure, which takes
 * time proportional to W * H * (N + log2(N)), with one that is only
 * proportional to N (but takes W * N additional buffer space).
 */

/* Allocate the line buffer for bolding.  We need 2 + bold scan lines. */
private byte *
alloc_bold_lines(gs_memory_t *mem, uint width, int bold, client_name_t cname)
{	return gs_alloc_byte_array(mem, 2 + bold, bitmap_raster(width + bold),
				   cname);
}

/* Merge one (aligned) scan line into another, for vertical smearing. */
private void
bits_merge(byte *dest, const byte *src, uint nbytes)
{	long *dp = (long *)dest;
	const long *sp = (const long *)src;
	uint n = (nbytes + sizeof(long) - 1) >> arch_log2_sizeof_long;

	for ( ; n >= 4; sp += 4, dp += 4, n -= 4 )
	  dp[0] |= sp[0], dp[1] |= sp[1], dp[2] |= sp[2], dp[3] |= sp[3];
	for ( ; n; ++sp, ++dp, --n )
	  *dp |= *sp;
}

/* Smear a scan line horizontally.  Note that the output is wider than */
/* the input by the amount of bolding (smear_width). */
private void
bits_smear_horizontally(byte *dest, const byte *src, uint width,
  uint smear_width)
{	uint bits_on = 0;
	const byte *sp = src;
	uint sbyte = *sp;
	byte *dp = dest;
	uint dbyte = sbyte;
	uint sdmask = 0x80;
	const byte *zp = src;
	uint zmask = 0x80;
	uint i = 0;

	/* Process the first smear_width bits. */
	{ uint stop = min(smear_width, width);

	  for ( ; i < stop; ++i ) {
	    if ( sbyte & sdmask )
	      bits_on++;
	    else if ( bits_on )
	      dbyte |= sdmask;
	    if ( (sdmask >>= 1) == 0 )
	      sdmask = 0x80, *dp++ = dbyte, dbyte = sbyte = *++sp;
	  }
	}
	        
	/* Process all but the last smear_width bits. */
	{ for ( ; i < width; ++i ) {
	    if ( sbyte & sdmask )
	      bits_on++;
	    else if ( bits_on )
	      dbyte |= sdmask;
	    if ( *zp & zmask )
	      --bits_on;
	    if ( (sdmask >>= 1) == 0 ) {
	      sdmask = 0x80;
	      *dp++ = dbyte;
on:	      switch ( (dbyte = sbyte = *++sp) ) {
		case 0xff:
		  if ( width - i <= 8 )
		    break;
		  *dp++ = 0xff;
		  bits_on += 8 -
		    byte_count_bits[(*zp & (zmask - 1)) + (zp[1] & -zmask)];
		  ++zp;
		  i += 8;
		  goto on;
		case 0:
		  if ( bits_on || width - i <= 8 )
		    break;
		  *dp++ = 0;
		  /* We know there can't be any bits to be zeroed, */
		  /* because bits_on can't go negative. */
		  ++zp;
		  i += 8;
		  goto on;
		default:
		  ;
	      }
	    }
	    if ( (zmask >>= 1) == 0 )
	      zmask = 0x80, ++zp;
	  }
	}

	/* Process the last smear_width bits. */
	/****** WRONG IF width < smear_width ******/
	{ uint stop = width + smear_width;

	  for ( ; i < stop; ++i ) {
	    if ( bits_on )
	      dbyte |= sdmask;
	    if ( (sdmask >>= 1) == 0 )
	      sdmask = 0x80, *dp++ = dbyte, dbyte = 0;
	    if ( *zp & zmask )
	      --bits_on;
	    if ( (zmask >>= 1) == 0 )
	      zmask = 0x80, ++zp;
	  }
	}

	if ( sdmask != 0x80 )
	  *dp = dbyte;
}

/* Image a bitmap character, with or without bolding. */
private int
image_bitmap_char(gs_image_enum *ienum, const gs_image_t *pim,
  const byte *bitmap_data, uint sraster, int bold, byte *bold_lines,
  gs_state *pgs)
{	uint dest_bytes = (pim->Width + 7) >> 3;
	gx_device *dev = pgs->device;
	void *iinfo;
	const byte *planes[1];
	int code;

	gx_set_dev_color(pgs);
	code = (*dev_proc(dev, begin_image))
	  (dev, (const gs_imager_state *)pgs, pim, gs_image_format_chunky,
	   NULL, pgs->dev_color, pgs->clip_path, pgs->memory, &iinfo);
	if ( code < 0 )
	  return code;
	if ( bold )
	  { /* Pass individual smeared lines. */
	    uint src_width = pim->Width - bold;
	    uint src_height = pim->Height - bold;
	    uint dest_raster = bitmap_raster(pim->Width);
	    int n1 = bold + 1;
#define merged_line(i) (bold_lines + ((i) % n1 + 1) * dest_raster)
	    int y;

	    planes[0] = bold_lines;
	    for ( y = 0; y < pim->Height; ++y ) {
	      int y0 = (y < bold ? 0 : y - bold);
	      int y1 = min(y + 1, src_height);

	      if ( y < src_height ) {
		bits_smear_horizontally(merged_line(y),
					bitmap_data + y * sraster,
					src_width, bold);
		{ /* Now re-establish the invariant -- see below. */
		  int kmask = 1;

		  for ( ; (y & kmask) == kmask && y - kmask >= y0;
			kmask = (kmask << 1) + 1
		      )
		    bits_merge(merged_line(y - kmask),
			       merged_line(y - (kmask >> 1)),
			       dest_bytes);
		}
	      }

	      /*
	       * As of this point in the loop, we maintain the following
	       * invariant to cache partial merges of groups of lines: for
	       * each Y, y0 <= Y < y1, let K be the maximum k such that Y
	       * mod 2^k = 0 and Y + 2^k < y1; then merged_line(Y) holds
	       * the union of horizontally smeared source lines Y through
	       * Y + 2^k - 1.  The idea behind this is similar to the idea
	       * of quicksort.
	       */

	      { /* Now construct the output line. */
		bool first = true;
		int iy;

		for ( iy = y1 - 1; iy >= y0; --iy ) {
		  int kmask = 1;

		  while ( (iy & kmask) == kmask && iy - kmask >= y0 )
		    iy -= kmask, kmask <<= 1;
		  if ( first ) {
		    memcpy(bold_lines, merged_line(iy), dest_bytes);
		    first = false;
		  }
		  else
		    bits_merge(bold_lines, merged_line(iy), dest_bytes);
		}
	      }


	      code = (*dev_proc(dev, image_data))
		(dev, iinfo, planes, 0, dest_bytes, 1);
	      if ( code != 0 )
		break;
	    }
#undef merged_line
	  }
	else
	  { /* Pass the entire image at once. */
	    planes[0] = bitmap_data;
	    code = (*dev_proc(dev, image_data))
	      (dev, iinfo, planes, 0, dest_bytes, pim->Height);
	  }
	(*dev_proc(dev, end_image))(dev, iinfo, code >= 0);
	return code;
}

/* Render a character for a bitmap font. */
/* This handles both format 0 (PCL XL) and format 4 (PCL5 bitmap). */
private int
pl_bitmap_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph glyph)
{	pl_font_t *plfont = (pl_font_t *)pfont->client_data;
	const byte *cdata = pl_font_lookup_glyph(plfont, glyph)->data;

	if ( cdata == 0 )
	  return 0;
	{ const byte *params;
	  const byte *bitmap_data;
	  int lsb, ascent;
	  float delta_x;
	  gs_image_t image;
	  gs_image_enum *ienum;
	  int code;
	  uint bold;
	  byte *bold_lines = 0;

	  if ( cdata[0] == 0 )
	    { /* PCL XL format */
	      params = cdata + 2;
	      bitmap_data = cdata + 10;
	      delta_x = 0;	/* irrelevant */
	    }
	  else
	    { /* PCL5 format */
	      params = cdata + 6;
	      bitmap_data = cdata + 16;
	      delta_x =
		(plfont->header[13] ? /* variable pitch */
		 s16(params + 8) * 0.25 :
		 s16(params) /*lsb*/ + u16(params + 4) /*width*/);
	    }
	  lsb = s16(params);
	  ascent = s16(params + 2);
	  ienum = gs_image_enum_alloc(pgs->memory, "pl_bitmap_build_char");
	  if ( ienum == 0 )
	    return_error(gs_error_VMerror);
	  gs_image_t_init_mask(&image, true);
	  image.Width = u16(params + 4);
	  image.Height = u16(params + 6);
	  /* Determine the amount of pseudo-bolding. */
	  if ( pfont->WMode >> 1 )
	    { float bold_fraction = (pfont->WMode >> 1) / 127.0;
	      bold = (uint)(image.Height * bold_fraction + 0.5);
	      bold_lines = alloc_bold_lines(pgs->memory, image.Width, bold,
					    "pl_bitmap_build_char(bold_line)");
	      if ( bold_lines == 0 )
		{ code = gs_note_error(gs_error_VMerror);
		  goto out;
		}
	      image.Width += bold;
	      image.Height += bold;
	      ascent += bold;
	    }
	  else
	    bold = 0;
	  gs_make_identity(&image.ImageMatrix);
	  image.ImageMatrix.tx -= lsb;
	  image.ImageMatrix.ty += ascent;
	  image.adjust = true;
#ifdef CACHE_BITMAP_CHARS
	  { float wbox[6];
	    wbox[0] = delta_x;	/* wx */
	    wbox[1] = 0;	/* wy */
	    wbox[2] = lsb;	/* llx */
	    wbox[3] = -ascent;	/* lly */
	    wbox[4] = image.Width + lsb;	/* urx */
	    wbox[5] = image.Height - ascent;	/* ury */
	    code = gs_setcachedevice(penum, pgs, wbox);
	  }
#else
	  code = gs_setcharwidth(penum, pgs, delta_x, 0);
#endif
	  if ( code < 0 )
	    return code;
	  code = image_bitmap_char(ienum, &image, bitmap_data,
				   (image.Width - bold + 7) >> 3, bold,
				   bold_lines, pgs);
out:	  gs_free_object(pgs->memory, bold_lines,
			 "pl_bitmap_build_char(bold_lines)");
	  gs_free_object(pgs->memory, ienum, "pl_bitmap_build_char");
	  return (code < 0 ? code : 0);
	}
}

/* ---------------- TrueType font support ---------------- */

/* Look up a character in the TrueType character-to-TT-glyph map. */
/* Return a pointer to the glyph's slot (chr != gs_no_char) or where */
/* it should be added (chr == gs_no_char). */
private pl_tt_char_glyph_t *
pl_tt_lookup_char(const pl_font_t *plfont, gs_glyph glyph)
{	uint size = plfont->char_glyphs.size;
	uint skip = plfont->char_glyphs.skip;
	uint index = glyph % size;
	pl_tt_char_glyph_t *ptcg;

	while ( (ptcg = plfont->char_glyphs.table + index)->chr != gs_no_char &&
		ptcg->chr != glyph
	      )
	  index = (index >= skip ? index : index + size) - skip;
	return ptcg;
}

/* Get a string from a TrueType font. */
private int
pl_tt_string_proc(gs_font_type42 *pfont, ulong offset, uint length,
  const byte **pdata)
{	pl_font_t *plfont = pfont->client_data;

	*pdata = plfont->header + plfont->offsets.GT +
	  (plfont->large_sizes ? 6 : 4) + offset;
	return 0;
}

/* Get the outline data for a glyph in a downloaded TrueType font. */
private int
pl_tt_get_outline(gs_font_type42 *pfont, uint index, const byte **pdata)
{	pl_font_t *plfont = pfont->client_data;
	const pl_font_glyph_t *pfg = pl_font_lookup_glyph(plfont, index);
	const byte *cdata = pfg->data;

	if ( cdata == 0 )
	  *pdata = 0;		/* undefined glyph */
	else
	  { uint desc_size =
	      (*cdata == 15 ? cdata[2] /* PCL5 */ : 0 /* PCL XL */);
	    uint data_size = pl_get_uint16(cdata + 2 + desc_size);

	    if ( data_size <= 4 )
	      *pdata = 0;		/* empty outline */
	    else
	      *pdata = cdata + 6 + desc_size;
	  }
	return 0;
}

#define access(base, length, vptr)\
  (*pfont->data.string_proc)(pfont, (ulong)(base), length, &vptr)

/* Find a table in a TrueType font. */
/* Return the data offset of the table; store the length in *plen. */
/* If the table is missing, return 0. */
private ulong
tt_find_table(gs_font_type42 *pfont, const char *tname, uint *plen)
{	const byte *OffsetTable;
	uint numTables;
	const byte *TableDirectory;
	uint i;

	access(0, 12, OffsetTable);
	numTables = u16(OffsetTable + 4);
	access(12, numTables * 16, TableDirectory);
	for ( i = 0; i < numTables; ++i )
	  { const byte *tab = TableDirectory + i * 16;
	    if ( !memcmp(tab, tname, 4) )
	      { if ( plen )
		  *plen = u32(tab + 12);
		return u32(tab + 8);
	      }
	  }
	return 0;
}

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif

/* Opaque type for a path */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;
#endif

/* Imported procedures */
int gs_type42_append(P7(uint glyph_index, gs_imager_state *pis,
  gx_path *ppath, const gs_log2_scale_point *pscale, bool charpath_flag,
  int paint_type, gs_font_type42 *pfont));
int gs_type42_get_metrics(P3(gs_font_type42 *pfont, uint glyph_index,
  float psbw[4]));

/* Encode a character using the TrueType cmap tables. */
/* (We think this is never used for downloaded fonts.) */
private gs_glyph
pl_tt_cmap_encode_char(const gs_font_type42 *pfont, ulong cmap_offset,
  uint cmap_len, gs_char chr)
{	const byte *cmap;
	const byte *cmap_sub;
	const byte *table;

	access(cmap_offset, cmap_len, cmap);
	/* Since the Apple cmap format is of no help in determining */
	/* the encoding, look for a Microsoft table; but if we can't */
	/* find one, take the first one. */
	cmap_sub = cmap + 4;
	{ uint i;
	  for ( i = 0; i < u16(cmap + 2); ++i )
	    if ( u16(cmap_sub + i * 8) == 3 )
	      { cmap_sub += i * 8;
		break;
	      }
	}
	{ uint offset = cmap_offset + u32(cmap_sub + 4);
	  access(offset, cmap_offset + cmap_len - offset, table);
	}
	/* Dispatch according to the table type. */
	switch ( u16(table) )
	  {
	  case 0:
	    {	/* Apple standard 1-to-1 mapping. */
		return table[chr + 6];
	    }
	  case 4:
	    {	/* Microsoft/Adobe segmented mapping.  What a mess! */
		uint segCount2 = u16(table + 6);
		const byte *endCount = table + 14;
		const byte *startCount = endCount + segCount2 + 2;
		const byte *idDelta = startCount + segCount2;
		const byte *idRangeOffset = idDelta + segCount2;
		/*const byte *glyphIdArray = idRangeOffset + segCount2;*/
		uint i2;

		for ( i2 = 0; i2 < segCount2 - 3; i2 += 2 )
		  { int delta, roff;
		    uint start = u16(startCount + i2);
		    uint glyph;

		    if ( chr < start )
		      return 0;
		    if ( chr > u16(endCount + i2) )
		      continue;
		    delta = s16(idDelta + i2);
		    roff = s16(idRangeOffset + i2);
		    if ( roff == 0 )
		      return chr + delta;
		    glyph = u16(idRangeOffset + i2 + roff +
				((chr - start) << 1));
		    return (glyph == 0 ? 0 : glyph + delta);
		  }
		return 0;		/* shouldn't happen */
	    }
	  case 6:
	    {	/* Single interval lookup. */
		uint firstCode = u16(table + 6);
		uint entryCount = u16(table + 8);

		return (chr < firstCode || chr >= firstCode + entryCount ? 0 :
			u16(table + 10 + ((chr - firstCode) << 1)));
	    }
	  default:
	    discard(gs_note_error(gs_error_invalidfont));
	    return 0;
	  }

}

/* Encode a character using the map built for downloaded TrueType fonts. */
private gs_glyph
pl_tt_dynamic_encode_char(const gs_font_type42 *pfont, gs_char chr)
{	pl_font_t *plfont = pfont->client_data;
	const pl_tt_char_glyph_t *ptcg = pl_tt_lookup_char(plfont, chr);

	return (ptcg->chr == gs_no_char ? gs_no_glyph : ptcg->glyph);
}

/* Return the galley character for a character code, if any; */
/* otherwise return gs_no_char. */
private gs_char
pl_font_galley_character(gs_char chr, const pl_font_t *plfont)
{	long GC = plfont->offsets.GC;
	const byte *gcseg;
	uint b0, b1;
	uint i, len;
	uint default_char;

	if ( GC < 0 )
	  return gs_no_char;
	gcseg = plfont->header + GC;
	if ( plfont->large_sizes )
	  len = u32(gcseg + 2),
	  i = 12;
	else
	  len = u16(gcseg + 2),
	  i = 10;
	if ( len != u16(gcseg + i - 2) * 6 + 6 ) /* bad data */
	  return gs_no_char;
	default_char = u16(gcseg + i - 4);
	len += i - 6;
	b0 = chr >> 8;
	b1 = chr & 0xff;
	for ( ; i < len; i += 6 )
	  if ( b0 >= gcseg[i] && b0 <= gcseg[i + 1] &&
	       b1 >= gcseg[i + 2] && b1 <= gcseg[i + 3]
	     )
	    return u16(gcseg + i + 4);
	return default_char;
}

/* Encode a character for a TrueType font. */
/* What we actually return is the TT glyph index. */
gs_glyph
pl_tt_encode_char(gs_show_enum *ignore_penum, gs_font *pfont_generic,
  gs_char *pchr)
{	gs_char chr = *pchr;
	gs_font_type42 *pfont = (gs_font_type42 *)pfont_generic;
	uint cmap_len;
	ulong cmap_offset = tt_find_table(pfont, "cmap", &cmap_len);
	gs_glyph glyph = 
	  (cmap_offset == 0 ?
	    /* This is a downloaded font with no cmap. */
	   pl_tt_dynamic_encode_char(pfont, chr) :
	   pl_tt_cmap_encode_char(pfont, cmap_offset, cmap_len, chr));
	pl_font_t *plfont = pfont->client_data;
	pl_font_glyph_t *pfg;

	if ( plfont->offsets.GC < 0 )
	  return glyph;	/* no substitute */
	pfg = pl_font_lookup_glyph(plfont, glyph);
	/* If the character is missing, use the galley character instead. */
	if ( !pfg->data )
	  { gs_char galley_char = pl_font_galley_character(chr, plfont);

	    if ( galley_char != gs_no_char )
	      { *pchr = galley_char;
	        return
		  (galley_char == 0xffff ? 0 :
		   cmap_offset == 0 ?
		   pl_tt_dynamic_encode_char(pfont, galley_char) :
		   pl_tt_cmap_encode_char(pfont, cmap_offset, cmap_len,
					  galley_char));
	      }
	  }
	return glyph;	/* no substitute */
}

/* Return the vertical substitute for a glyph, if it has one; */
/* otherwise return gs_no_glyph. */
private gs_glyph
pl_font_vertical_glyph(gs_glyph glyph, const pl_font_t *plfont)
{	long VT = plfont->offsets.VT;
	const byte *vtseg;
	uint i, len;

	if ( VT < 0 )
	  return gs_no_glyph;
	vtseg = plfont->header + VT;
	if ( plfont->large_sizes )
	  len = u32(vtseg + 2),
	  i = 6;
	else
	  len = u16(vtseg + 2),
	  i = 4;
	len += i;
	for ( ; i < len; i += 4 )
	  if ( glyph == u16(vtseg + i) )
	    return u16(vtseg + i + 2);
	return gs_no_glyph;
}

/* Get character existence and escapement for a TrueType font. */
private int
pl_tt_char_width(const pl_font_t *plfont, const pl_symbol_map_t *map,
  const gs_matrix *pmat, uint char_code, gs_point *pwidth)
{	gs_font *pfont = plfont->pfont;
	gs_char chr = char_code;
	/****** DOESN'T HANDLE SYMBOL SETS PROPERLY ******/
	gs_glyph glyph = pl_tt_encode_char(NULL, pfont, &chr);
	int code;
	float sbw[4];

	/* Check for a vertical substitute. */
	if ( pfont->WMode & 1 )
	  { gs_glyph vertical = pl_font_vertical_glyph(glyph, plfont);

	    if ( vertical != gs_no_glyph )
	      glyph = vertical;
	  }
	if ( glyph == 0xffff || glyph == gs_no_glyph )
	  { if ( pwidth )
	      pwidth->x = pwidth->y = 0;
	    return 1;
	  }
	/****** WHAT UNITS FOR WIDTH? ******/
	code = gs_type42_get_metrics((gs_font_type42 *)pfont, glyph, sbw);
	if ( code < 0 )
	  return code;
	if ( pwidth )
	  return gs_distance_transform(sbw[2], sbw[3], pmat, pwidth);
	return 0;
}

/* Render a TrueType character. */
private int
pl_tt_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph orig_glyph)
{	gs_glyph glyph = orig_glyph;
#define pbfont ((gs_font_base *)pfont)
#define pfont42 ((gs_font_type42 *)pfont)
	int code;
	float bold_fraction =
	  (gs_show_in_charpath(penum) != cpm_show ? 0.0 :
	   (pfont->WMode >> 1) / 127.0);
	uint bold_added;
	double scale;
	float sbw[4], w2[6];
	int ipx, ipy, iqx, iqy;
	gx_device_memory mdev;

#ifdef CACHE_TRUETYPE_CHARS
#  define tt_set_cache(penum, pgs, w2)\
     gs_setcachedevice(penum, pgs, w2)
#else
#  define tt_set_cache(penum, pgs, w2)\
     gs_setcharwidth(penum, pgs, w2[0], w2[1]);
#endif

	/* Check for a vertical substitute. */

	if ( pfont->WMode & 1 )
	  { pl_font_t *plfont = pfont->client_data;
	    gs_glyph vertical = pl_font_vertical_glyph(glyph, plfont);

	    if ( vertical != gs_no_glyph )
	      glyph = vertical;
	  }

	/* Establish a current point. */

	if ( (code = gs_moveto(pgs, 0.0, 0.0)) < 0 )
	  return code;

	/* Get the metrics and set the cache device. */

	code = gs_type42_get_metrics(pfont42, glyph, sbw);
	if ( code < 0 )
	  return code;
	w2[0] = sbw[2], w2[1] = sbw[3];

	/* Adjust the bounding box for stroking if needed. */

	{ const gs_rect *pbbox = &pbfont->FontBBox;

	  w2[2] = pbbox->p.x, w2[3] = pbbox->p.y;
	  w2[4] = pbbox->q.x, w2[5] = pbbox->q.y;
	  if ( pfont->PaintType )
	    {	double expand = max(1.415, gs_currentmiterlimit(pgs)) *
		  gs_currentlinewidth(pgs) / 2;

		w2[2] -= expand, w2[3] -= expand;
		w2[4] += expand, w2[5] += expand;
	    }
	}

	/*
	 * If we want pseudo-bold, render untransformed to an intermediate
	 * bitmap, smear it, and then transform it to produce the output.
	 * This is really messy.
	 */
	if ( bold_fraction == 0 )
	  { code = tt_set_cache(penum, pgs, w2);
	    if ( code < 0 )
	      return code;
	    bold_added = 0;
	  }
	else
	  { gs_matrix mat, smat;
	    gs_rect sbox;

	    code = gs_gsave(pgs);
	    if ( code < 0 )
	      return code;
	    gs_currentmatrix(pgs, &mat);
	    /* Determine an appropriate scale for the bitmap. */
	    scale = max(fabs(mat.xx) + fabs(mat.yx),
			fabs(mat.xy) + fabs(mat.yy));
	    gs_make_scaling(scale, scale, &smat);
	    sbox.p.x = w2[2], sbox.p.y = w2[3];
	    sbox.q.x = w2[4], sbox.q.y = w2[5];
	    gs_bbox_transform(&sbox, &smat, &sbox);
	    ipx = (int)sbox.p.x, ipy = (int)sbox.p.y;
	    iqx = (int)ceil(sbox.q.x), iqy = (int)ceil(sbox.q.y);
	    /* Set up the memory device for the bitmap. */
	    gs_make_mem_mono_device(&mdev, NULL, pgs->device);
	    bold_added = (int)ceil((iqy - ipy) * bold_fraction);
	    mdev.width = iqx - ipx + bold_added;
	    mdev.height = iqy - ipy;
	    mdev.bitmap_memory = pgs->memory;
	    code = (*dev_proc(&mdev, open_device))((gx_device *)&mdev);
	    if ( code < 0 )
	      { gs_grestore(pgs);
	        return code;
	      }
	    /* Don't allow gs_setdevice to reset things. */
	    pgs->device = (gx_device *)&mdev;
	    { gs_fixed_rect cbox;
	      cbox.p.x = cbox.p.y = fixed_0;
	      cbox.q.x = int2fixed(mdev.width);
	      cbox.q.y = int2fixed(mdev.height);
	      gx_clip_to_rectangle(pgs, &cbox);
	    }
	    /* Make sure we clear the entire bitmap. */
	    memset(mdev.base, 0, bitmap_raster(mdev.width) * mdev.height);
	    gx_set_device_color_1(pgs);	/* write 1's */
	    smat.tx = -ipx;
	    smat.ty = -ipy;
	    gs_setmatrix(pgs, &smat);
	  }
	code = gs_type42_append(glyph,
				(gs_imager_state *)pgs, pgs->path,
				&penum->log2_current_scale,
				gs_show_in_charpath(penum) != cpm_show,
				pfont->PaintType, pfont42);
	if ( code >= 0 )
	  code = (pfont->PaintType ? gs_stroke(pgs) : gs_fill(pgs));
	if ( bold_added )
	  gs_grestore(pgs);
	if ( code < 0 || !bold_added )
	  return code;

	/* Now smear the bitmap and copy it to the destination. */

	{ gs_image_t image;
	  gs_image_enum *ienum =
	    gs_image_enum_alloc(pgs->memory, "pl_tt_build_char");
	  byte *bold_lines =
	    alloc_bold_lines(pgs->memory, mdev.width - bold_added, bold_added,
			     "pl_tt_build_char(bold_lines)");

	  if ( ienum == 0 || bold_lines == 0 )
	    { code = gs_note_error(gs_error_VMerror);
	      goto out;
	    }
	  gs_image_t_init_mask(&image, true);
	  image.Width = mdev.width;
	  image.Height = mdev.height + bold_added;
	  gs_make_scaling(scale, scale, &image.ImageMatrix);
	  image.ImageMatrix.tx = -ipx;
	  image.ImageMatrix.ty = -ipy;
	  image.adjust = true;
	  code = tt_set_cache(penum, pgs, w2);
	  if ( code < 0 )
	    goto out;
	  code = image_bitmap_char(ienum, &image, mdev.base,
				   bitmap_raster(mdev.width), bold_added,
				   bold_lines, pgs);
out:	  gs_free_object(pgs->memory, bold_lines, "pl_tt_build_char(bold_lines)");
	  gs_free_object(pgs->memory, ienum, "pl_tt_build_char(image enum)");
	  gs_free_object(pgs->memory, mdev.base, "pl_tt_build_char(bitmap)");
	}
	return (code < 0 ? code : 0);
#undef pfont42
#undef pbfont
}

/* ---------------- Internal initialization ---------------- */

/* Initialize the procedures for a bitmap font. */
void
pl_bitmap_init_procs(gs_font_base *pfont)
{	pfont->procs.encode_char = pl_bitmap_encode_char;
	pfont->procs.build_char = pl_bitmap_build_char;
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = pl_bitmap_char_width;
#undef plfont
}

/* Initialize the procedures for a TrueType font. */
void
pl_tt_init_procs(gs_font_type42 *pfont)
{	pfont->procs.encode_char = pl_tt_encode_char;
	pfont->procs.build_char = pl_tt_build_char;
	pfont->data.string_proc = pl_tt_string_proc;
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = pl_tt_char_width;
#undef plfont
}

/* Finish initializing a TrueType font. */
void
pl_tt_finish_init(gs_font_type42 *pfont, bool downloaded)
{	float upem = pfont->data.unitsPerEm;
	ulong head = tt_find_table(pfont, "head", NULL);
	const byte *hdata;

	if ( downloaded )
	  pfont->data.get_outline = pl_tt_get_outline;
	/* Set the FontBBox. */
	access(head, 44, hdata);
	pfont->FontBBox.p.x = s16(hdata + 36) / upem;
	pfont->FontBBox.p.y = s16(hdata + 38) / upem;
	pfont->FontBBox.q.x = s16(hdata + 40) / upem;
	pfont->FontBBox.q.y = s16(hdata + 42) / upem;
#ifdef DEBUG
	if ( gs_debug_c('m') )
	  { const byte *OffsetTable;
	    uint numTables;
	    const byte *TableDirectory;
	    uint i;

	    access(0, 12, OffsetTable);
	    numTables = u16(OffsetTable + 4);
	    access(12, numTables * 16, TableDirectory);
	    for ( i = 0; i < numTables; ++i )
	      { const byte *tab = TableDirectory + i * 16;
		dprintf6("%c%c%c%c offset = %lu length = %lu\n",
			 tab[0], tab[1], tab[2], tab[3],
			 (ulong)u32(tab + 8), (ulong)u32(tab + 12));
	      }
	  }
#endif
}

/* ---------------- Public procedures ---------------- */

/* Allocate the glyph table. */
int
pl_font_alloc_glyph_table(pl_font_t *plfont, uint num_glyphs, gs_memory_t *mem,
  client_name_t cname)
{	uint size =
	  ((num_glyphs > 300 ? (num_glyphs = 300) : 0),
	   num_glyphs + (num_glyphs >> 2) + 5);
	pl_font_glyph_t *glyphs =
	  gs_alloc_struct_array(mem, size, pl_font_glyph_t,
				&st_pl_font_glyph_element, cname);

	if ( glyphs == 0 )
	  return_error(gs_error_VMerror);
	{ uint i;
	  for ( i = 0; i < size; ++i )
	    glyphs[i].glyph = 0, glyphs[i].data = 0;
	}
	plfont->glyphs.table = glyphs;
	plfont->glyphs.used = 0;
	plfont->glyphs.limit = num_glyphs;
	plfont->glyphs.size = size;
	plfont->glyphs.skip = size * 2 / 3;
	while ( igcd(plfont->glyphs.skip, size) > 1 )
	  plfont->glyphs.skip++;
	return 0;
}

/* Expand the glyph table. */
private int
expand_glyph_table(pl_font_t *plfont, gs_memory_t *mem)
{	pl_glyph_table_t old_table;
	int code;
	uint i;

	old_table = plfont->glyphs;
	code = pl_font_alloc_glyph_table(plfont, old_table.size, mem,
					 "expand_glyph_table(new table)");
	if ( code < 0 )
	  return code;
	for ( i = 0; i < old_table.size; ++i )
	  if ( old_table.table[i].data )
	    *pl_font_lookup_glyph(plfont, old_table.table[i].glyph) =
	      old_table.table[i];
	gs_free_object(mem, old_table.table, "expand_glyph_table(old table)");
	plfont->glyphs.used = old_table.used;
	return 0;
}

/* Allocate the TrueType character to glyph index map. */
int
pl_tt_alloc_char_glyphs(pl_font_t *plfont, uint num_chars, gs_memory_t *mem,
  client_name_t cname)
{	uint size =
	  ((num_chars > 300 ? (num_chars = 300) : 0),
	   num_chars + (num_chars >> 2) + 5);
	pl_tt_char_glyph_t *char_glyphs =
	  (pl_tt_char_glyph_t *)
	  gs_alloc_byte_array(mem, size, sizeof(pl_tt_char_glyph_t), cname);

	if ( char_glyphs == 0 )
	  return_error(gs_error_VMerror);
	{ uint i;
	  for ( i = 0; i < size; ++i )
	    char_glyphs[i].chr = gs_no_char;
	}
	plfont->char_glyphs.table = char_glyphs;
	plfont->char_glyphs.used = 0;
	plfont->char_glyphs.limit = num_chars;
	plfont->char_glyphs.size = size;
	plfont->char_glyphs.skip = size * 2 / 3;
	while ( igcd(plfont->char_glyphs.skip, size) > 1 )
	  plfont->char_glyphs.skip++;
	return 0;
}

/* Expand the character to glyph index map. */
private int
expand_char_glyph_table(pl_font_t *plfont, gs_memory_t *mem)
{	pl_tt_char_glyph_table_t old_table;
	int code;
	uint i;

	old_table = plfont->char_glyphs;
	code = pl_tt_alloc_char_glyphs(plfont, old_table.size, mem,
				       "expand_char_glyphs(new table)");
	if ( code < 0 )
	  return code;
	for ( i = 0; i < old_table.size; ++i )
	  if ( old_table.table[i].chr != gs_no_char )
	    *pl_tt_lookup_char(plfont, old_table.table[i].glyph) =
	      old_table.table[i];
	gs_free_object(mem, old_table.table, "expand_char_glyphs(old table)");
	plfont->char_glyphs.used = old_table.used;
	return 0;
}

/* Add a glyph to a font.  Return -1 if the table is full. */
typedef struct font_glyph_s {
  gs_font *font;
  gs_glyph glyph;
} font_glyph_t;
private bool
match_font_glyph(cached_char *cc, void *vpfg)
{	const font_glyph_t *pfg = vpfg;
	return (cc->pair->font == pfg->font && cc->code == pfg->glyph);
}
int
pl_font_add_glyph(pl_font_t *plfont, gs_glyph glyph, const byte *cdata)
{	gs_font *pfont = plfont->pfont;
	gs_glyph key = glyph;
	pl_tt_char_glyph_t *ptcg = 0;
	pl_font_glyph_t *pfg;

	/*
	 * If this is a downloaded TrueType font, the "glyph" is actually
	 * a character code, and the actual TrueType glyph index is in the
	 * character header.  In this case, the character data must be either
	 * a PCL5 format 15 or a PCL XL format 1 downloaded character.
	 */
tcg:	if ( plfont->char_glyphs.table )
	  { ptcg = pl_tt_lookup_char(plfont, key);
	    if ( ptcg->chr == gs_no_char &&
		 plfont->char_glyphs.used >= plfont->char_glyphs.limit
	       )
	      { /* Table is full, try to expand it. */
		int code = expand_char_glyph_table(plfont, pfont->memory);
		if ( code < 0 )
		  return code;
		goto tcg;
	      }
	    key = pl_get_uint16(cdata + (cdata[0] == 15 ? cdata[2] + 4 : 4));
	  }
fg:	pfg = pl_font_lookup_glyph(plfont, key);
	if ( pfg->data != 0 )
	  { /* Remove the glyph from the character cache. */
	    font_glyph_t match_fg;

	    match_fg.font = pfont;
	    match_fg.glyph = key;
	    gx_purge_selected_cached_chars(pfont->dir, match_font_glyph,
					   &match_fg);
	    gs_free_object(pfont->memory, pfg->data,
			   "pl_font_add_glyph(old data)");
	  }
	else
	  { if ( plfont->glyphs.used >= plfont->glyphs.limit )
	      { /* Table is full, try to expand it. */
		int code = expand_glyph_table(plfont, pfont->memory);
		if ( code < 0 )
		  return code;
		goto fg;
	      }
	    plfont->glyphs.used++;
	  }
	if ( ptcg )
	  { if ( ptcg->chr == gs_no_char )
	      plfont->char_glyphs.used++;
	    ptcg->chr = glyph;
	    ptcg->glyph = key;
	  }
	pfg->glyph = key;
	pfg->data = cdata;
	return 0;
}

/*
 * Code to map a character through a symbol set.
 ****** Not used yet. ******
 */
#if 0
	  { /* Use the symbol maps to obtain a character index. */
	    const pl_symbol_map_t *map = 0;
	    uint first_code, last_code;

	    switch ( plfont->font_type )
	      {
	      case plft_MSL: map = maps[plgv_MSL]; break;
	      case plft_Unicode: map = maps[plgv_Unicode]; break;
	      }
	    if ( map == 0 )
	      { /* No mapping available, can't process the character. */
		return -1;
	      }
	    first_code = pl_get_uint16(map->first_code);
	    last_code = pl_get_uint16(map->last_code);
	    if ( char_code < first_code || char_code > last_code )
	      goto undef;
	    char_index = map->codes[char_code];
	  }
#endif
