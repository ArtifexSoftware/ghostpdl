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
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "plvalue.h"

/* agfa stuff */
#undef true
#undef false
#undef frac_bits
#include "cgconfig.h"     /* this must be first  */
#include "ufstport.h"         /* this must be second */
#include "shareinc.h"
#include "strmio.h"

/* Structure descriptors */
private_st_pl_font();

/* Define accessors for unaligned, big-endian quantities. */
#define u16(bptr) pl_get_uint16(bptr)
#define s16(bptr) pl_get_int16(bptr)
/**** ASSUME uint >= 32 BITS ****/
#define u32(bptr) (uint)pl_get_uint32(bptr)


extern void pl_init_fc(
    const pl_font_t *   plfont,
    gs_state *          pgs,
    int                 need_outline,
    FONTCONTEXT *       pfc,
    bool                width_request);

/* ---------------- Utilities ---------------- */

/* Free a font.  This is the freeing procedure in the font dictionary. */
void
pl_free_font(gs_memory_t *mem, void *plf, client_name_t cname)
{	pl_font_t *plfont = plf;
	/* Free the characters. */
        if ( !plfont->data_are_permanent )
	  { if ( plfont->glyphs.table )
	     { uint i;
	       for ( i = plfont->glyphs.size; i > 0; )
                 { void *data = (void *)plfont->glyphs.table[--i].data;
	           if ( data )
		     gs_free_object(mem, data, cname);
	         }  
	     }
	     gs_free_object(mem, (void *)plfont->header, cname);
	     plfont->header = 0; /* see hack note above */
	  }
	/* Free the font data itself. */
	gs_free_object(mem, (void *)plfont->char_glyphs.table, cname);
	gs_free_object(mem, (void *)plfont->glyphs.table, cname);
	if ( plfont->pfont )	/* might be only partially constructed */
	  { gs_purge_font(plfont->pfont);
	    gs_free_object(mem, plfont->pfont, cname);
	  }
	if ( plfont->font_file ) {
	    gs_free_object(mem, plfont->font_file, cname);
	}
	gs_free_object(mem, plf, cname);
}

/* ---------------- Library callbacks ---------------- */

/* Get the name of a glyph.  We don't need to support this. */
private const char *
pl_glyph_name(gs_glyph glyph, uint *plen)
{	return 0;
}

extern gs_char last_char;
/* Get the unicode valude for a glyph */
private gs_char
pl_decode_glyph(gs_font *font,  gs_glyph glyph)
{	
    dprintf("Pdfwrite is not supported with AGFA UFST!\n");
    // NB glyph is unicode for builtin fonts 
    // this fails for downloaded fonts where there is often 
    // no association other than sequential numbering 
    return last_char;
}

/* ---------------- Public procedures ---------------- */

/* character width */
int pl_font_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{
    return (*(plfont)->char_width)(plfont, pgs, char_code, pwidth);
}

/* character width */
int pl_font_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])
{
    return (*(plfont)->char_metrics)(plfont, pgs, char_code, metrics);
}

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
	    plfont->landscape = false;
	    plfont->bold_fraction = 0;
	    plfont->font_file = 0;
	    plfont->resolution.x = plfont->resolution.y = 0;
	    plfont->params.proportional_spacing = true;
	    memset(plfont->character_complement, 0xff, 8);
	    plfont->offsets.GC = plfont->offsets.GT = plfont->offsets.VT = -1;
	    plfont->pts_per_inch = 72.0;   /* normal value */
	  }
	return plfont;
}
/* import from plchar.c - used to determine if this is a downloaded
   true type font or a resident font - yuck. */
extern int pl_tt_get_outline(gs_font_type42 *pfont, uint index, gs_glyph_data_t *pdata);

/* Structure descriptors for cloning fonts */
gs_private_st_ptrs1(st_pl_font_glyph_f, pl_font_glyph_t, "pl_font_glyph_t",
  pl_font_glyph_enum_ptrs_f, pl_font_glyph_reloc_ptrs_f, data);
gs_private_st_element(st_pl_font_glyph_element_f, pl_font_glyph_t,
  "pl_font_glyph_t[]",
  pl_font_glyph_elt_enum_ptrs_f, pl_font_glyph_elt_reloc_ptrs_f, st_pl_font_glyph_f);

int pl_fill_in_font(gs_font *pfont, pl_font_t *plfont, gs_font_dir *pdir, gs_memory_t *mem, const char *font_name);
void pl_fill_in_mt_font(gs_font_base *pfont, SW16 data, long unique_id);


pl_font_t *
pl_clone_font(const pl_font_t *src, gs_memory_t *mem, client_name_t cname)
{
    pl_font_t *plfont;
    ulong header_size;
    plfont = (pl_font_t *)gs_alloc_bytes(mem, gs_object_size(mem, src), cname);
    if ( plfont == 0 )
        return 0;
    *plfont = *src;
    /* NB gs_object_size returns uint */
    header_size = (ulong)gs_object_size(mem, src->header);
    if (header_size != 0) {
        plfont->header = gs_alloc_bytes(mem, header_size, cname);
        if ( plfont->header == 0 )
            return 0;
        memcpy(plfont->header, src->header, header_size);
    }
    /* technology specific setup */
    switch ( plfont->scaling_technology ) {
    case plfst_bitmap:
        {
            gs_font_base *pfont =
                gs_alloc_struct(mem, gs_font_base, &st_gs_font_base, cname);
            if ( pfont == 0 )
                return 0;
            pl_fill_in_font((gs_font *)pfont, plfont, src->pfont->dir, mem, "illegal font");
            pl_fill_in_bitmap_font(pfont, gs_next_ids(mem, 1));
            break;
        }
    case plfst_Intellifont:
        {
            gs_font_base *pfont =
                gs_alloc_struct(mem, gs_font_base, &st_gs_font_base, cname);
            if ( pfont == 0 )
                return 0;
            pl_fill_in_font((gs_font *)pfont, plfont, src->pfont->dir, mem, "illegal font");
            pl_fill_in_intelli_font(pfont, gs_next_ids(mem, 1));
            break;
        }
    case plfst_TrueType:
        {
            {
                gs_font_type42 *pfont =
                    gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42, cname);
                if ( pfont == 0 )
                    return 0;
                pl_fill_in_font((gs_font *)pfont,
                                plfont, src->pfont->dir,
                                mem, "illegal font");
                pl_fill_in_tt_font(pfont, NULL, gs_next_ids(mem, 1));
            }
            break;
        }
    case plfst_MicroType:
        {
            gs_font_base *pfont =
                gs_alloc_struct(mem, gs_font_base, &st_gs_font_base, cname);
            if ( pfont == 0 )
                return 0;
            pl_fill_in_font((gs_font *)pfont, plfont, src->pfont->dir, mem, "illegal font");
            pl_fill_in_mt_font(pfont, 0, 0);
            pfont->UID = ((gs_font_base *)(src->pfont))->UID; /*** HACK ***/
            break;
        }
    }
    if ( src->char_glyphs.table != 0 ) {
        pl_tt_char_glyph_t *char_glyphs =
            (pl_tt_char_glyph_t *) gs_alloc_byte_array(mem,
                                                       src->char_glyphs.size,
                                                       sizeof(pl_tt_char_glyph_t), cname);
        int i;
        if ( char_glyphs == 0 )
            return 0;
        for ( i = 0; i < src->char_glyphs.size; i++ )
            char_glyphs[i] = src->char_glyphs.table[i];
        /* once again a copy struct shortcut and then are restore
           of the char_glyphs.table pointer */
        plfont->char_glyphs = src->char_glyphs;
        plfont->char_glyphs.table = char_glyphs;
    }

    if ( src->glyphs.table != 0 ) {
        int i;
        plfont->glyphs.table = 
            gs_alloc_struct_array(mem, src->glyphs.size, pl_font_glyph_t,
                                  &st_pl_font_glyph_element_f, cname);
        plfont->glyphs.used = src->glyphs.used;
        plfont->glyphs.limit = src->glyphs.limit;
        plfont->glyphs.size = src->glyphs.size;
        plfont->glyphs.skip = src->glyphs.skip;
        for ( i = 0; i < src->glyphs.size; i++ ) {
            byte *data = src->glyphs.table[i].data;
            byte *char_data;
            plfont->glyphs.table[i].glyph =
                src->glyphs.table[i].glyph;
            plfont->glyphs.table[i].data = 0;
            if ( data ) {
                uint size = gs_object_size(mem, data);
                char_data = gs_alloc_bytes(mem, size, cname);
                if ( char_data == 0 )
                    return 0;
                memcpy(char_data, data, size);
                plfont->glyphs.table[i].data = char_data;
            }
        }
    }
    return plfont;
}
	
/* Fill in generic font boilerplate. */
int
pl_fill_in_font(gs_font *pfont, pl_font_t *plfont, gs_font_dir *pdir,
  gs_memory_t *mem, const char *font_name)
{	
        int i;
        plfont->pfont = pfont;
	/* Initialize generic font data. */
	pfont->next = pfont->prev = 0;
	pfont->memory = mem;
	pfont->dir = pdir;
	pfont->is_resource = false;  
	gs_notify_init(&pfont->notify_list, gs_memory_stable(mem));
	pfont->base = pfont;
	pfont->client_data = plfont;
	pfont->WMode = 0;
	pfont->PaintType = 0;
	pfont->StrokeWidth = 0;
	pfont->procs.init_fstack = gs_default_init_fstack;
	pfont->procs.next_char_glyph = gs_default_next_char_glyph;

	pfont->procs.glyph_name = pl_glyph_name;
	pfont->procs.decode_glyph = pl_decode_glyph;

	pfont->procs.define_font = gs_no_define_font;
	pfont->procs.make_font = gs_no_make_font;
	pfont->procs.font_info = gs_default_font_info;
        pfont->procs.glyph_info = gs_default_glyph_info;
        pfont->procs.glyph_outline = gs_no_glyph_outline;
	pfont->id = gs_next_ids(mem, 1);
	strncpy(pfont->font_name.chars, font_name, sizeof(pfont->font_name.chars));
	pfont->font_name.size = strlen(font_name);
	/* replace spaces with '-', seems acrobat doesn't like spaces. */
	for (i = 0; i < pfont->font_name.size; i++) {
	    if (pfont->font_name.chars[i] == ' ')
		pfont->font_name.chars[i] = '-';
	}
	strncpy(pfont->key_name.chars, font_name, sizeof(pfont->font_name.chars));
	pfont->key_name.size = strlen(font_name);
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
	/*
	 * We can't set the FontBBox correctly until we've initialized the
	 * Type 42 specific data, but we need to set it to an empty box now
	 * for the sake of gs_type42_font_init.
	 */
	pfont->FontBBox.p.x = pfont->FontBBox.p.y =
	  pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
	/* Initialize Type 42 specific data. */
	pfont->data.proc_data = data;
	pl_tt_init_procs(pfont);
	gs_type42_font_init(pfont);
	pl_tt_finish_init(pfont, !data);
}

/* Fill in Intellifont boilerplate. */
void
pl_fill_in_intelli_font(gs_font_base *pfont, long unique_id)
{	/* Intellifonts have an 8782-unit design space. */
	gs_make_identity(&pfont->FontMatrix);
	pfont->FontType = ft_user_defined;
	pfont->BitmapWidths = true;
	pfont->ExactSize = fbit_use_outlines;
	pfont->InBetweenSize = fbit_use_outlines;
	pfont->TransformedChar = fbit_use_outlines;
	/* We have no idea what the FontBBox should be. */
	pfont->FontBBox.p.x = pfont->FontBBox.p.y =
	  pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
	pl_intelli_init_procs(pfont);
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
pl_font_scan_segments(const gs_memory_t *mem,
		      pl_font_t *plfont, int fst_offset, int start_offset,
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
		     * includes at least 3 elements (gdir, head,
		     * maxp -- but we don't check the actual names).
		     */
		    if ( seg_size < 12 + 5 * 16 ||
			 /* memcmp(sdata, "\000\001\000\000", 4) ||  */
			 u16(sdata + 4) < 3
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

int
pl_free_tt_fontfile_buffer(gs_memory_t *mem, byte *ptt_font_data)
{
    gs_free_object(mem, ptt_font_data, "pl_tt_load_font data");
    return 0;
}

int 
pl_alloc_tt_fontfile_buffer(stream *in, gs_memory_t *mem, byte **pptt_font_data, ulong *size)
{
    ulong len = (sfseek(in, 0L, SEEK_END), sftell(in));
    *size = 6 + len;	/* leave room for segment header */
    if ( *size != (uint)(*size) ) { 
	/*
	 * The font is too big to load in a single piece -- punt.
	 * The error message is bogus, but there isn't any more
	 * appropriate one.
	 */
	sfclose(in);
	return_error(gs_error_VMerror);
    }
    srewind(in);
    *pptt_font_data = gs_alloc_bytes(mem, *size, "pl_tt_load_font data");
    if ( *pptt_font_data == 0 ) {
	sfclose(in);
	return_error(gs_error_VMerror);
    }
    sfread(*pptt_font_data + 6, 1, len, in);
    sfclose(in);
    return 0;
}

/* Fill in AGFA MicroType font boilerplate. */
void
pl_fill_in_mt_font(gs_font_base *pfont, SW16 data, long unique_id)
{	gs_make_identity(&pfont->FontMatrix);
        pfont->FontType = ft_MicroType;
        pfont->BitmapWidths = true;
        pfont->ExactSize = fbit_use_outlines;
        pfont->InBetweenSize = fbit_use_outlines;
        pfont->TransformedChar = fbit_use_outlines;

        pfont->FontBBox.p.x = pfont->FontBBox.p.y =
          pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
        uid_set_UniqueID(&pfont->UID, unique_id | (data << 16));
        pfont->encoding_index = 1;	/****** WRONG ******/
        pfont->nearest_encoding_index = 1;	/****** WRONG ******/
        pl_mt_init_procs(pfont);
}

/* Load a built-in AGFA MicroType font */
int
pl_load_mt_font(SW16 handle, gs_font_dir *pdir, gs_memory_t *mem,
       long unique_id, pl_font_t **pplfont)
{         
	  gs_font_base *pfont = gs_alloc_struct(mem, gs_font_base, 
	  	&st_gs_font_base, "pl_mt_load_font(gs_font_base)");
	  pl_font_t *plfont = pl_alloc_font(mem, "pl_mt_load_font(pl_font_t)");
          int code;

	  if ( pfont == 0 || plfont == 0 )
	    code = gs_note_error(gs_error_VMerror);
          else
          {   /* Initialize general font boilerplate. */
              code = pl_fill_in_font((gs_font *)pfont, plfont, pdir, mem, "illegal font");
              if ( code >= 0 )
              {   /* Initialize MicroType font boilerplate. */
                  plfont->header = 0;
                  plfont->header_size = 0;
                  plfont->scaling_technology = plfst_MicroType;
                  plfont->font_type = plft_Unicode;
                  plfont->large_sizes = true;
                  plfont->is_xl_format = false;
                  plfont->allow_vertical_substitutes = false;
                  pl_fill_in_mt_font(pfont, handle, unique_id);
                  code = gs_definefont(pdir, (gs_font *)pfont);
              }
          }
          if ( code < 0 )
          {   gs_free_object(mem, plfont, "pl_mt_load_font(pl_font_t)");
              gs_free_object(mem, pfont, "pl_mt_load_font(gs_font_base)");
              return code;
          }
          *pplfont = plfont;
          return 0;
}

/* load resident font data to ram - not needed for ufst so stubs */
int
pl_load_resident_font_data_from_file(gs_memory_t *mem, pl_font_t *plfont)
{
    return 0;
}

/* Keep resident font data in (header) and deallocate the memory */
int
pl_store_resident_font_data_in_file(char *font_file, gs_memory_t *mem, pl_font_t *plfont)
{
    return 0;
}
