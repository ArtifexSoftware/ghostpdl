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
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxpath.h"
/* We really shouldn't need the following, but currently they are needed */
/* for pgs->path and penum->log2_current_scale in pl_tt_build_char. */
#include "gxfixed.h"
#include "gxchar.h"
#include "gxfcache.h"
#include "gzstate.h"
/* prescribed freetype include file setup */
#include <ftbuild.h>
#include FT_FREETYPE_H

/*
 * As of gs library version 5.86, the log2_current_scale member of the
 * gs_show_enum structure is renamed log2_scale.  The following test
 * handles this, by checking for a #define that was added in the same
 * version.
 */
#ifdef TEXT_FROM_ANY
# define log2_current_scale log2_scale
#endif

# undef gs_show_move
# define gs_show_move TEXT_PROCESS_INTERVENE

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
pl_font_glyph_t *
pl_font_lookup_glyph(const pl_font_t *plfont, gs_glyph glyph)
{	uint size = plfont->glyphs.size;
	uint skip = plfont->glyphs.skip;
	uint index = glyph % size;
	pl_font_glyph_t *pfg;
	pl_font_glyph_t *result = 0;

	while ( (pfg = plfont->glyphs.table + index)->data ?
		pfg->glyph != glyph : pfg->glyph != 0
	      )
	  { if ( !pfg->data )
	      result = pfg;
	    index = (index >= skip ? index : index + size) - skip;
	  }
	return (!pfg->data && result) ? result : pfg;
}

/* ---------------- Bitmap font support ---------------- */

/* Encode a character for a bitmap font.  This is simple, because */
/* bitmap fonts are always bound. */
private gs_glyph
pl_bitmap_encode_char(gs_font *pfont, gs_char chr, gs_glyph not_used)
{	return (gs_glyph)chr;
}

/* Get character existence and escapement for a bitmap font. */
/* This is simple for the same reason. */
private int
pl_bitmap_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{	
    const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;

    pwidth->x = pwidth->y = 0;
    if ( !pwidth ) {
	return (cdata == 0 ? 1 : 0);
	dprintf( "Warning should not call width function without width\n" );
    }
    if ( cdata == 0 ) { 
	return 1;
    }
    if ( cdata[0] == 0 ) { /* PCL XL characters don't have an escapement. */
	    pwidth->x = pwidth->y = 0;
	    return 0;
    }

    { 
	const byte *params = cdata + 6;
	pwidth->x = (plfont->header[13] ? /* variable pitch */
		     s16(params + 8) * 0.25 :
		     s16(params) /*lsb*/ + u16(params + 4) /*width*/);
    }
    return 0;
}

private int
pl_bitmap_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])
{
    gs_point width;
    const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;
    /* never a vertical substitute */
    metrics[0] = metrics[1] = metrics[2] = metrics[3] = 0;
    /* no data - character not found */
    if ( cdata == 0 )
	return 1;
    /* We are not concerned about PCL XL characters */
    if ( cdata[0] == 0 )
	return 0;
    
    metrics[0] = s16(cdata + 6);
    pl_bitmap_char_width(plfont, pgs, char_code, &width);
    metrics[2] = width.x;
    return 0;
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
	   NULL, pgs->dev_color, pgs->clip_path, pgs->memory, (gx_image_enum_common_t **)&iinfo);
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
	bool landscape = plfont->landscape;

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
	      lsb = s16(params);
	      ascent = s16(params + 2);
	    }
	  else
	    { /* PCL5 format */
	      params = cdata + 6;
	      bitmap_data = cdata + 16;
	      delta_x = (plfont->header[13] ? /* variable pitch */
			 s16(params + 8) * 0.25 :
			 s16(params) /*lsb*/ + u16(params + 4) /*width*/);
	      lsb = s16(params);
	      ascent = s16(params + 2);
	    }
	  ienum = gs_image_enum_alloc(pgs->memory, "pl_bitmap_build_char");
	  if ( ienum == 0 )
	    return_error(gs_error_VMerror);
	  gs_image_t_init_mask(&image, true);
	  image.Width = u16(params + 4);
	  image.Height = u16(params + 6);
	  /* Determine the amount of pseudo-bolding. */
	  if ( plfont->bold_fraction != 0 ) { 
	      bold = (uint)(image.Height * plfont->bold_fraction + 0.5);
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
	  if ( landscape ) {
	      /* note that we don't even use the font metrics for lsb.
                 It appears in landscape mode the bitmaps account for
                 this, very peculiar */
	      gs_matrix_rotate(&image.ImageMatrix, -90, &image.ImageMatrix);
	      gs_matrix_translate(&image.ImageMatrix, -image.Height, image.Width, &image.ImageMatrix);
	      /* for the landscape case apparently we adjust by the
                 width + left offset and height - top offset.  Note
                 the landscape left offset is always negative in pcl's
                 coordinate system. */
	      image.ImageMatrix.tx -= (image.Width + s16(params)) /* left offset */;
	      image.ImageMatrix.ty += (s16(params+2) - image.Height);
	  }
	  else {
	      image.ImageMatrix.tx -= lsb;
	      image.ImageMatrix.ty += ascent;
	  }

	  image.adjust = true;
	  code = gs_setcharwidth(penum, pgs, delta_x, 0);
	  if ( code < 0 )
	    return code;
#ifdef DEBUG	      
	  if ( gs_debug_c('B') ) {
	      int i;
	      int pixels = round_up(image.Width,8) * image.Height;
	      dprintf7("bitmap font data chr=%ld, width=%d, height=%d, lsb=%d, ascent=%d, top offset=%d left offset=%d\n",
		       chr, image.Width, image.Height, lsb, ascent, s16(params + 2), s16(params));
	      for ( i = 0; i < pixels; i++ ) {
		  if ( i % round_up(image.Width, 8) == 0 )
		      dprintf("\n");
		  dprintf1( "%d", bitmap_data[i >> 3] & (128 >> (i & 7)) ? 1 : 0);
	      }
	      dprintf("\n");
	  }
#endif
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
	pl_tt_char_glyph_t *result = 0;

	while ( (ptcg = plfont->char_glyphs.table + index)->chr != gs_no_char ?
		ptcg->chr != glyph : ptcg->glyph
	      )
	  { if ( ptcg->chr == gs_no_char )
	      result = ptcg;
	    index = (index >= skip ? index : index + size) - skip;
	  }
	return (result ? result : ptcg);
}

/* Get the outline data for a glyph in a downloaded TrueType font. */
int
pl_tt_get_outline(gs_font_type42 *pfont, uint index, gs_const_string *pdata)
{	pl_font_t *plfont = pfont->client_data;
	const pl_font_glyph_t *pfg = pl_font_lookup_glyph(plfont, index);
	const byte *cdata = pfg->data;

	if ( cdata == 0 )
	  { pdata->data = 0;		/* undefined glyph */
            pdata->size = 0;
          }
	else
	  { uint desc_size =
	      (*cdata == 15 ? cdata[2] /* PCL5 */ : 0 /* PCL XL */);
	    uint data_size = pl_get_uint16(cdata + 2 + desc_size);

	    if ( data_size <= 4 )
	     { pdata->data = 0;		/* empty outline */
	       pdata->size = 0;
             }
	    else
	      { pdata->data = cdata + 6 + desc_size;
	        pdata->size = data_size - 4;
	      }
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

/*
 * Map a key through a cmap sub-table.  We export this so we can use
 * it someday for mapping between glyph vocabularies.  If the key is
 * not mappable, return gs_error_undefined; if the sub-table type is
 * unknown, return gs_error_invalidfont.
 */
int
pl_cmap_lookup(uint key, const byte *table, uint *pvalue)
{	/* Dispatch according to the table type. */
	switch ( u16(table) )
	  {
	  case 0:
	    {	/* Apple standard 1-to-1 mapping. */
		*pvalue = table[key + 6];
		if_debug2('J', "[J]%u => %u\n", key, *pvalue);
		break;
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

		    if_debug4('J', "[J]start=%u end=%u delta=%d roff=%d\n",
			      start, u16(endCount + i2), s16(idDelta + i2),
			      s16(idRangeOffset + i2));
		    if ( key < start )
		      { if_debug1('J', "[J]%u out of range\n", key);
		        return_error(gs_error_undefined);
		      }
		    if ( key > u16(endCount + i2) )
		      continue;
		    delta = s16(idDelta + i2);
		    roff = s16(idRangeOffset + i2);
		    if ( roff == 0 )
		      { *pvalue = ( key + delta ) & 0xffff; /* mod 65536 */
		        if_debug2('J', "[J]%u => %u\n", key, *pvalue);
		        return 0;
		      }
		    glyph = u16(idRangeOffset + i2 + roff +
				((key - start) << 1));
		    *pvalue = (glyph == 0 ? 0 : glyph + delta);
		    if_debug2('J', "[J]%u => %u\n", key, *pvalue);
		    return 0;
		  }
		/*
		 * The TrueType documentation says that the last range is
		 * always supposed to end with 0xffff, so this shouldn't
		 * happen; however, in some real fonts, it does.
		 */
		if_debug1('J', "[J]%u out of range\n", key);
		return_error(gs_error_undefined);
	    }
	  case 6:
	    {	/* Single interval lookup. */
		uint firstCode = u16(table + 6);
		uint entryCount = u16(table + 8);

		if ( key < firstCode || key >= firstCode + entryCount )
		  { if_debug1('J', "[J]%u out of range\n", key);
		    return_error(gs_error_undefined);
		  }
		*pvalue = u16(table + 10 + ((key - firstCode) << 1));
		if_debug2('J', "[J]%u => %u\n", key, *pvalue);
		break;
	    }
	  default:
	    return_error(gs_error_invalidfont);
	  }
	return 0;
}

gs_glyph
pl_ft_encode_char(gs_font *pfont_generic, gs_char chr, gs_glyph not_used)
{	
    return (gs_glyph)chr;
}

/* get the glyph container for a character, maybe should return the
   instance as well */
private FT_Error
ft_get_face(const pl_font_t *plfont, const void *pgs, uint char_code, FT_Face *pface)
{
    gs_font *pfont = plfont->pfont;
    gs_char chr = char_code;
    gs_glyph unused_glyph = gs_no_glyph;
    /* NB wrong */
    gs_glyph glyph = pl_ft_encode_char(pfont, chr, unused_glyph);
    /* HACK - face pointer pickled into the id */
    unsigned long id = ((gs_font_base *)(plfont->pfont))->UID.id;
    FT_Error error;
    int xres = gs_currentdevice((gs_state *)pgs)->HWResolution[0];
    int yres = gs_currentdevice((gs_state *)pgs)->HWResolution[1];
    floatp hx;
    floatp hy;
    int pointsize; /* height */
    int setsize;   /* width */
    gs_matrix mat;

    /* same HACK - face pointer pickled into the id */
    *pface = (FT_Face)id;
    gs_currentmatrix((gs_state *)pgs, &mat);
    hx = hypot(mat.xx, mat.xy);
    hy = hypot(mat.yx, mat.yy);
    pointsize = (int)((hy  * plfont->pts_per_inch / yres) + 0.5);
    setsize = (int)((hx * plfont->pts_per_inch / xres) + 0.5);
    /* NB needs width set the character size 0 for width - height
       points in 1/64'ths... nb just load then set char size if
       rendering */
    error = FT_Set_Char_Size(*pface, 
                             setsize * 64, /* width */ 
			     pointsize * 64, /* height */ 
			     xres, yres);
    if ( error )
        return error;
    
    /* set transformation for skew - 16.16 fixed */
    {
        FT_Matrix m;
        m.xx = ( 1 << 16 ) * mat.xx / hx;
        m.xy = ( 1 << 16 ) * mat.xy / hx;
        m.yx = ( 1 << 16 ) * -mat.yx / hy;
        m.yy = ( 1 << 16 ) * -mat.yy / hy;
        FT_Set_Transform(*pface, &m, 0);
    }
    {

        /* find our character map - we always use unicode but we'll do
           this exercise anyway. */
        FT_CharMap  found = 0;
        FT_CharMap  charmap;
        int         n;
        for ( n = 0; n < (*pface)->num_charmaps; n++ ) {
            charmap = (*pface)->charmaps[n];
            if ( (charmap->platform_id == 3 && charmap->encoding_id == 1 )  ||
                 (charmap->platform_id == 0 && charmap->encoding_id == 0 ) ) {
                found = charmap;
                break;
            }
        }

        if ( !found ) {
            dprintf( "charmap not found\n" );
            return error;
        }

        /* now, select the charmap (what we found above) for the face object */
        error = FT_Set_Charmap( *pface, found );
        if ( error ) {
            dprintf( "unable to set charmap not found\n" );
            return error;
        }
        /* load the glyph */
        error = FT_Load_Glyph( *pface, FT_Get_Char_Index( *pface, (FT_UInt)glyph ), 
                               FT_LOAD_LINEAR_DESIGN );
        if ( error ) {
            dprintf( "unable to load glyph\n" );
            return error;
        }
    }
    return 0;
}

/* Get character existence and escapement for a TrueType font. */
private int
pl_ft_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{	
    FT_Face face;
    FT_Error error = ft_get_face(plfont, pgs, char_code, &face);
    if ( error )
	return -1;
    {
        pwidth->x = (double)face->glyph->linearHoriAdvance / (double)face->units_per_EM;
        pwidth->y = 0;
    }
    return 0;
}

/* Get metrics */
private int
pl_ft_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])
{
    FT_Face face;
    FT_Error error = ft_get_face(plfont, pgs, char_code, &face);
    if ( error )
	return -1;
    /* lsb as proportion of Em square */
    dprintf("warning: using 0 for left side bearing\n");
    metrics[0] = 0;
    metrics[1] = 0;
    /* width as a proportion of em square */
    metrics[2] = (double)face->glyph->linearHoriAdvance / (double)face->units_per_EM;
    metrics[3] = 0;
    return 0;
}
    

/* Render a TrueType character. */
private int
pl_ft_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph orig_glyph)
{	gs_glyph glyph = orig_glyph;
#define pbfont ((gs_font_base *)pfont)
#define pfont42 ((gs_font_type42 *)pfont)
	int code;
	pl_font_t *plfont = (pl_font_t *)pfont->client_data;
#ifdef CACHE_TRUETYPE_CHARS
#  define tt_set_cache(penum, pgs, w2)\
     gs_setcachedevice(penum, pgs, w2)
#else
#  define tt_set_cache(penum, pgs, w2)\
     gs_setcharwidth(penum, pgs, w2[0], w2[1]);
#endif
	/* undefined */
	if ( glyph == 0xffff )
	    return 0;
	/* Should check for vertical substitute */

	/* Establish a current point. */

	if ( (code = gs_moveto(pgs, 0.0, 0.0)) < 0 )
	  return code;

	if ( code < 0 )
	    return code;

	{
            FT_Face face;
	    gs_matrix mat;
	    gs_image_t image;
	    FT_Error error;
	    /* get the current glyph instance.  With point size and scaling */
	    gs_currentmatrix(pgs, &mat);
	    error = ft_get_face( plfont, pgs, chr, &face );
	    if ( error )
		return -1;
	    /* get an outline of the glyph... we'll scale the
	       outline by the ctm and then render the bitmap */

            {
		gs_matrix save_ctm, tmp_ctm;
                gs_image_enum *ienum;
                FT_Bitmap *ftb = &face->glyph->bitmap;
                FT_Render_Glyph(face->glyph, ft_render_mode_mono);

		if ( error )
		    return -1;
                /* move to device space */
                gs_currentmatrix(pgs, &save_ctm);
                gs_make_identity(&tmp_ctm);
                tmp_ctm.tx = save_ctm.tx;
                tmp_ctm.ty = save_ctm.ty;
                gs_setmatrix(pgs, &tmp_ctm);
#ifdef DEBUG
                if ( gs_debug_c('B') ) {
		    int i;
		    int pixels = round_up(ftb->width, 8) * ftb->rows;
		    for ( i = 0; i < pixels; i++ ) {
			if ( i % round_up(ftb->width, 8) == 0 )
			    dprintf("\n");
			dprintf1( "%d", ftb->buffer[i >> 3] & (128 >> (i & 7)) ? 1 : 0);
		    }
		    dprintf("\n");
		}
#endif
                /* set up the image */
                ienum = gs_image_enum_alloc(pgs->memory, "pl_ufst_make_char");
                if (ienum == 0) {
                    gs_setmatrix(pgs, &save_ctm);
                    return_error(gs_error_VMerror);
                }
                gs_image_t_init_mask(&image, true);
                image.Width = ftb->width;
                image.Height = ftb->rows;
                gs_make_identity(&image.ImageMatrix);
                image.ImageMatrix.tx = -face->glyph->bitmap_left;
                image.ImageMatrix.ty = face->glyph->bitmap_top;
                image.adjust = true;
                code = image_bitmap_char( ienum,
                                          &image,
                                          (byte *)ftb->buffer,
                                          ftb->width,
                                          0,
                                          NULL,
                                          pgs );
                gs_free_object(pgs->memory, ienum, "pl_ufst_make_char");
                gs_setmatrix(pgs, &save_ctm);
                return (code < 0 ? code : 0);
            }
        }
	return (code < 0 ? code : 0);
#undef pfont42
#undef pbfont
}

/* We don't have to do any character encoding, since Intellifonts are */
/* indexed by character code (if bound) or MSL code (if unbound). */
private gs_glyph
pl_intelli_encode_char(gs_font *pfont, gs_char pchr, gs_glyph not_used)
{	return (gs_glyph)pchr;
}

/* Define the structure of the Intellifont metrics. */
typedef struct intelli_metrics_s {
  byte charSymbolBox[4][2];
  byte charEscapementBox[4][2];
  byte halfLine[2];
  byte centerline[2];
} intelli_metrics_t;

/* Merge the bounding box of a character into the composite box, */
/* and set the escapement.  Return true if the character is defined. */
private bool
pl_intelli_merge_box(float wbox[6], const pl_font_t *plfont, gs_glyph glyph)
{	const byte *cdata = pl_font_lookup_glyph(plfont, glyph)->data;

	if ( cdata == 0 )
	  return false;
	wbox[1] = 0;
	if ( cdata[3] == 4 )
	  { /* Compound character.  Merge the component boxes; */
	    /* use the compound character's escapement. */
	    bool found = false;
	    uint i;

	    for ( i = 0; i < cdata[6]; ++i )
	      found |= pl_intelli_merge_box(wbox, plfont,
					    pl_get_uint16(cdata + 8 + i * 6));
	    wbox[0] = pl_get_int16(cdata + 4);
	    return found;
	  }
	/* Non-compound character. */
	cdata += 4;		/* skip PCL character header */
	{ const intelli_metrics_t *metrics =
	    (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));
	  int llx = pl_get_int16(metrics->charSymbolBox[0]);
	  int lly = pl_get_int16(metrics->charSymbolBox[1]);
	  int urx = pl_get_int16(metrics->charSymbolBox[2]);
	  int ury = pl_get_int16(metrics->charSymbolBox[3]);

	  wbox[0] = pl_get_int16(metrics->charEscapementBox[2]) -
	    pl_get_int16(metrics->charEscapementBox[0]);
	  wbox[2] = min(wbox[2], llx);
	  wbox[3] = min(wbox[3], lly);
	  wbox[4] = max(wbox[4], urx);
	  wbox[5] = max(wbox[5], ury);
	}
	return true;
}

/* Do the work for rendering an Intellifont character. */
/* The caller has done the setcachedevice. */
private int
pl_intelli_show_char(gs_state *pgs, const pl_font_t *plfont, gs_glyph glyph)
{	const byte *cdata = pl_font_lookup_glyph(plfont, glyph)->data;

	if ( cdata == 0 )
	  return 0;
	if ( cdata[3] == 4 )
	  { /* Compound character */
	    gs_matrix save_ctm;
	    int i;

	    gs_currentmatrix(pgs, &save_ctm);
	    for ( i = 0; i < cdata[6]; ++i )
	      { const byte *edata = cdata + 8 + i * 6;
	        floatp x_offset = pl_get_int16(edata + 2);
		floatp y_offset = pl_get_int16(edata + 4);
		int code;

		gs_translate(pgs, x_offset, y_offset);
		code = pl_intelli_show_char(pgs, plfont, pl_get_uint16(edata));
		gs_setmatrix(pgs, &save_ctm);
		if ( code < 0 )
		  return code;
	      }
	    return 0;
	  }
	cdata += 4;		/* skip PCL character header */
	{ const byte *outlines = cdata + pl_get_uint16(cdata + 6);
	  uint num_loops = pl_get_uint16(outlines);
	  uint i;
	  int code;

	  if_debug2('1', "[1]ifont glyph %lu: loops=%u\n",
		    (ulong)glyph, num_loops);
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

	      if ( num_aux_points == 0xffff ) {
		num_aux_points = 0;
		if_debug1('1', "[1]corrupt intellifont font glyph %ld\n", glyph );
	      }
	      if_debug2('1', "[1]num_points=%u num_aux_points=%u\n",
			num_points, num_aux_points);
	      /* For the moment, just draw straight lines. */
	      for ( j = 0; j < num_points; x_coords += 2, y_coords += 2, ++j )
		{ int x = pl_get_uint16(x_coords) & 0x3fff;
		  int y = pl_get_uint16(y_coords) & 0x3fff;

		  if_debug4('1', "[1]%s (%d,%d) %s\n",
			    (*x_coords & 0x80 ? " line" : "curve"), x, y,
			    (*y_coords & 0x80 ? " line" : "curve"));
		  if ( j == 0 )
		    code = gs_moveto(pgs, (floatp)x, (floatp)y);
		  else
		    code = gs_lineto(pgs, (floatp)x, (floatp)y);
		  if ( code < 0 )
		    return code;
		  if ( num_aux_points && (!(*x_coords & 0x80) && j != 0) )
		    { /* The auxiliary dx and dy values are signed. */
		      int dx = (*x_aux_coords++ ^ 0x80) - 0x80;
		      int dy = (*y_aux_coords++ ^ 0x80) - 0x80;

		      if_debug2('1', "[1]... aux (%d,%d)\n", dx, dy);
		      code = gs_lineto(pgs, (x + x_prev) / 2 + dx,
				       (y + y_prev) / 2 + dy);
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
	return 0;
}

/* Get character existence and escapement for an Intellifont. */
 private int
pl_intelli_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{	const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;
	int wx;

	if ( !pwidth )
	  return (cdata == 0 ? 1 : 0);
	if ( cdata == 0 )
	  { pwidth->x = pwidth->y = 0;
	    return 1;
	  }
	switch ( cdata[3] )
	  {
	  case 3:		/* non-compound character */
	    cdata += 4;		/* skip PCL character header */
	    { const intelli_metrics_t *metrics =
		(const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));
	      wx =
		pl_get_int16(metrics->charEscapementBox[2]) -
		pl_get_int16(metrics->charEscapementBox[0]);
	    }
	    break;
	  case 4:		/* compound character */
	    wx = pl_get_int16(cdata + 4);
	    break;
	  default:		/* shouldn't happen */
	    pwidth->x = pwidth->y = 0;
	    return 0;
	  }
	pwidth->x = (floatp)wx / 8782.0;
	return 0;
}

private int
pl_intelli_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])
{
    gs_point width;
    const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;

    metrics[0] = metrics[1] = metrics[2] = metrics[3] = 0;

    if ( cdata == 0 ) { 
	return 1;
    }

    /* compound */
    if ( cdata[3] == 4 ) {
	dprintf( "warning compound intellifont metrics not supported" );
	return 0;
    }

    cdata += 4;
    
    {
	const intelli_metrics_t *intelli_metrics =
	    (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));

	/* NB probably not right */
	/* never a vertical substitute, doesn't yet handle compound characters */
	metrics[0] = (float)pl_get_int16(intelli_metrics->charSymbolBox[0]);
	metrics[0] /= 8782.0;
	pl_intelli_char_width(plfont, pgs, char_code, &width);
	metrics[2] = width.x;
	return 0;
    }
}

/* Render a character for an Intellifont. */
private int
pl_intelli_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph glyph)
{	const pl_font_t *plfont = (const pl_font_t *)pfont->client_data;
	float wbox[6];
	int code;

	wbox[0] = wbox[1] = 0;
	wbox[2] = wbox[4] = 65536.0;
	wbox[3] = wbox[5] = -65536.0;
	if ( !pl_intelli_merge_box(wbox, plfont, glyph) )
	  { wbox[2] = wbox[3] = wbox[4] = wbox[5] = 0;
	    code = gs_setcachedevice(penum, pgs, wbox);
	    return (code < 0 ? code : 0);
	  }
	code = gs_setcachedevice(penum, pgs, wbox);
	if ( code < 0 )
	  return code;
	code = pl_intelli_show_char(pgs, plfont, glyph);
	if ( code < 0 )
	  return code;
	/* Since we don't take into account which side of the loops is */
	/* outside, we take the easy way out.... */
	code = gs_eofill(pgs);
	return (code < 0 ? code : 0);
}


/* ---------------- Internal initialization ---------------- */

/* Initialize the procedures for a bitmap font. */
void
pl_bitmap_init_procs(gs_font_base *pfont)
{	pfont->procs.encode_char = (void *)pl_bitmap_encode_char; /* FIX ME (void *) */
	pfont->procs.build_char = (void *)pl_bitmap_build_char; /* FIX ME (void *) */
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = pl_bitmap_char_width;
	plfont->char_metrics = pl_bitmap_char_metrics;
#undef plfont
}

/* Initialize the procedures for a TrueType font. */
void
pl_tt_init_procs(gs_font_type42 *pfont)
{	pfont->procs.encode_char = (void *)pl_ft_encode_char; /* FIX ME (void *) */
	pfont->procs.build_char = (void *)pl_ft_build_char; /* FIX ME (void *) */
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = pl_ft_char_width;
	plfont->char_metrics = pl_ft_char_metrics;
#undef plfont
}

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


void
pl_intelli_init_procs(gs_font_base *pfont)
{	pfont->procs.encode_char = (void *)pl_intelli_encode_char;  /* FIX ME (void *) */
	pfont->procs.build_char = (void *)pl_intelli_build_char; /* FIX ME (void *) */
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = pl_intelli_char_width;
	plfont->char_metrics = pl_intelli_char_metrics;
#undef plfont
}
/* ---------------- Public procedures ---------------- */

/* Allocate the glyph table. */
int
pl_font_alloc_glyph_table(pl_font_t *plfont, uint num_glyphs, gs_memory_t *mem,
  client_name_t cname)
{	uint size = num_glyphs + (num_glyphs >> 2) + 5;
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
{	uint size = num_chars + (num_chars >> 2) + 5;
	pl_tt_char_glyph_t *char_glyphs =
	  (pl_tt_char_glyph_t *)
	  gs_alloc_byte_array(mem, size, sizeof(pl_tt_char_glyph_t), cname);

	if ( char_glyphs == 0 )
	  return_error(gs_error_VMerror);
	{ uint i;
	  for ( i = 0; i < size; ++i )
	    char_glyphs[i].chr = gs_no_char, char_glyphs[i].glyph = 0;
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
	    *pl_tt_lookup_char(plfont, old_table.table[i].chr) =
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
pl_font_add_glyph(pl_font_t *plfont, gs_glyph glyph, byte *cdata)
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

/* Remove a glyph from a font.  Return 1 if the glyph was present. */
int
pl_font_remove_glyph(pl_font_t *plfont, gs_glyph glyph)
{	gs_font *pfont = plfont->pfont;
	gs_glyph key = glyph;
	pl_font_glyph_t *pfg;

	/* See above regarding downloaded TrueType fonts. */
	if ( plfont->char_glyphs.table )
	  { pl_tt_char_glyph_t *ptcg = pl_tt_lookup_char(plfont, key);
	    if ( ptcg->chr == gs_no_char )
	      return 0;
	    key = ptcg->glyph;
	    ptcg->chr = gs_no_char;
	    ptcg->glyph = 1;	/* mark as deleted */
	    plfont->char_glyphs.used--;
	  }
	pfg = pl_font_lookup_glyph(plfont, key);
	if ( pfg->data == 0 )
	  return 0;		/* character not defined */
	{ /* Remove the glyph from the character cache. */
	  font_glyph_t match_fg;

	  match_fg.font = pfont;
	  match_fg.glyph = key;
	  gx_purge_selected_cached_chars(pfont->dir, match_font_glyph,
					 &match_fg);
	  gs_free_object(pfont->memory, pfg->data,
			 "pl_font_remove_glyph(data)");
	}
	pfg->data = 0;
	pfg->glyph = 1;		/* mark as deleted */
	plfont->glyphs.used--;
	return 1;
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
