/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcsfont.c */
/* PCL5 soft font creation commands */
#include "memory_.h"
#include "stdio_.h"		/* needed for pl_load_tt_font */
#include "pcommand.h"
#include "pcfont.h"
#include "pcstate.h"
#include "pldict.h"
#include "plvalue.h"
#include "gsbitops.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"

/* Import procedures from pcifont.c. */
extern void pcl_fill_in_intelli_font(P2(gs_font_base *pfont, long unique_id));

/* Define the downloaded character data formats. */
typedef enum {
  pccd_bitmap = 4,
  pccd_intellifont = 10,
  pccd_truetype = 15
} pcl_character_format;


/* ------ Internal procedures ------ */

/* Delete a soft font.  If it is the currently selected font, or the */
/* current primary or secondary font, cause a new one to be chosen. */
private void
pcl_delete_soft_font(pcl_state_t *pcls, const byte *key, uint ksize,
  void *value)
{	if ( value == NULL )
	  { if ( !pl_dict_find(&pcls->soft_fonts, key, ksize, &value) )
	      return;		/* not a defined font */
	  }
	if ( pcls->font_selection[0].font == value )
	  pcls->font_selection[0].font = 0;
	if ( pcls->font_selection[1].font == value )
	  pcls->font_selection[1].font = 0;
	pcls->font = pcls->font_selection[pcls->font_selected].font;
	pl_dict_undef(&pcls->soft_fonts, key, ksize);
}

/* ------ Commands ------ */

private int /* ESC * c <id> D */
pcl_assign_font_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint id = uint_arg(pargs);
	id_set_value(pcls->font_id, id);
	return 0;
}

private int /* ESC * c <fc_enum> F */
pcl_font_control(pcl_args_t *pargs, pcl_state_t *pcls)
{	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	switch ( uint_arg(pargs) )
	  {
	  case 0:
	    { /* Delete all soft fonts. */
	      int i;
	      for ( i = 0; i < 2; ++i )
		if ( pcls->font_selection[i].font != 0 &&
		     (pcls->font_selection[i].font->storage & pcds_downloaded)
		   )
		  pcls->font_selection[i].font = 0;
	      pcls->font = pcls->font_selection[pcls->font_selected].font;
	      pl_dict_release(&pcls->soft_fonts);
	    }
	    return 0;
	  case 1:
	    { /* Delete all temporary soft fonts. */
	      pl_dict_enum_stack_begin(&pcls->soft_fonts, &denum, false);
	      while ( pl_dict_enum_next(&denum, &key, &value) )
		if ( ((pl_font_t *)value)->storage == pcds_temporary )
		  pcl_delete_soft_font(pcls, key.data, key.size, value);
	    }
	    return 0;
	  case 2:
	    { /* Delete soft font <font_id>. */
	      pcl_delete_soft_font(pcls, id_key(pcls->font_id), 2, NULL);
	    }
	    return 0;
	  case 3:
	    { /* Delete character <font_id, character_code>. */
	      if ( pl_dict_find(&pcls->soft_fonts, id_key(pcls->font_id), 2, &value) )
		pl_font_remove_glyph((pl_font_t *)value, pcls->character_code);
	      return 0;
	    }
	  case 4:
	    { /* Make soft font <font_id> temporary. */
	      if ( pl_dict_find(&pcls->soft_fonts, id_key(pcls->font_id), 2, &value) )
		((pl_font_t *)value)->storage = pcds_temporary;
	    }
	    return 0;
	  case 5:
	    { /* Make soft font <font_id> permanent. */
	      if ( pl_dict_find(&pcls->soft_fonts, id_key(pcls->font_id), 2, &value) )
		((pl_font_t *)value)->storage = pcds_permanent;
	    }
	    return 0;
	  case 6:
	    { /* Assign <font_id> to copy of current font. */
	    }
	    return e_Unimplemented;
	  default:
	    return 0;
	  }
}

private int /* ESC ) s <count> W */ 
pcl_font_header(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = uint_arg(pargs);
	const byte *data = arg_data(pargs);
#define pfh ((const pcl_font_header_t *)data)
	uint desc_size;
	pl_font_scaling_technology_t fst;
	gs_memory_t *mem = pcls->memory;
	pl_font_t *plfont;
	byte *header;
	int code;

	if ( count < 64 )
	  return e_Range;
	desc_size =
	  (pfh->FontDescriptorSize[0] << 8) + pfh->FontDescriptorSize[1];
	/* Dispatch on the header format. */
	switch ( pfh->HeaderFormat )
	  {
	  case pcfh_bitmap:
	  case pcfh_resolution_bitmap:
	    fst = plfst_bitmap;
	    break;
	  case pcfh_intellifont_bound:
	  case pcfh_intellifont_unbound:
	    fst = plfst_Intellifont;
	    break;
	  case pcfh_truetype:
	  case pcfh_truetype_large:
	    fst = plfst_TrueType;
	    break;
	  default:
	    return_error(gs_error_invalidfont);
	  }
	/* Delete any previous font with this ID. */
	pcl_delete_soft_font(pcls, id_key(pcls->font_id), 2, NULL);
	/* Create the generic font information. */
	plfont = pl_alloc_font(mem, "pcl_font_header(pl_font_t)");
	header = gs_alloc_bytes(mem, count, "pcl_font_header(header)");
	if ( plfont == 0 || header == 0 )
	  return_error(e_Memory);
	memcpy(header, data, count);
	plfont->storage = pcds_temporary;
	plfont->header = header;
	plfont->header_size = count;
	plfont->scaling_technology = fst;
	plfont->font_type = pfh->FontType;
	code = pl_font_alloc_glyph_table(plfont, 256, mem,
					 "pcl_font_header(char table)");
	if ( code < 0 )
	  return code;
	/* Create the actual font. */
	switch ( fst )
	  {
	  case plfst_bitmap:
	  {
	    gs_font_base *pfont =
	      gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
			      "pcl_font_header(bitmap font)");

	    if ( pfont == 0 )
	      return_error(e_Memory);
	    code = pl_fill_in_font((gs_font *)pfont, plfont, pcls->font_dir,
				   mem);
	    if ( code < 0 )
	      return code;
	    pl_fill_in_bitmap_font(pfont, gs_next_ids(1));
	    /* Extract parameters from the font header. */
	    if ( pfh->HeaderFormat == pcfh_resolution_bitmap )
	      {
#define pfhx ((const pcl_resolution_bitmap_header_t *)pfh)
		plfont->resolution.x = pl_get_uint16(pfhx->XResolution);
	        plfont->resolution.y = pl_get_uint16(pfhx->YResolution);
#undef pfhx
	      }
	    else
	      plfont->resolution.x = plfont->resolution.y = 300;
	    { ulong pitch_1024th_dots =
		((ulong)pl_get_uint16(pfh->Pitch) << 8) + pfh->PitchExtended;
	      uint pitch_cp = (uint)
		(pitch_1024th_dots / 1024.0	/* dots */
		 / plfont->resolution.x	/* => inches */
		 * 7200.0);

	      pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
	    }
	    plfont->params.height_4ths = pl_get_uint16(pfh->Height);
	    break;
	  }
	  case plfst_TrueType:
	  {
	    gs_font_type42 *pfont =
	      gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
			      "pcl_font_header(truetype font)");

	    if ( pfont == 0 )
	      return_error(e_Memory);
	    { static const pl_font_offset_errors_t errors = {
	        gs_error_invalidfont, 0
	      };
	      code =
		pl_font_scan_segments(plfont, 70, desc_size, (ulong)count - 2,
				      pfh->HeaderFormat == pcfh_truetype_large,
				      &errors);
	      if ( code < 0 )
		return code;
	    }
	    { uint num_chars = pl_get_uint16(pfh->LastCode);

	      if ( num_chars < 20 )
		num_chars = 20;
	      else if ( num_chars > 300 )
		num_chars = 300;
	      code = pl_tt_alloc_char_glyphs(plfont, num_chars, mem,
					     "pcl_font_header(char_glyphs)");
	      if ( code < 0 )
		return code;
	    }
	    code = pl_fill_in_font((gs_font *)pfont, plfont, pcls->font_dir,
				   mem);
	    if ( code < 0 )
	      return code;
	    pl_fill_in_tt_font(pfont, NULL, gs_next_ids(1));
	    /* pfh->Pitch is design unit width for scalable fonts. */
	    { uint pitch_cp =
		pl_get_uint16(pfh->Pitch) * 100 / pfont->data.unitsPerEm;
	      pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
	    }
	  }
	    break;
	  case plfst_Intellifont:
	  {
	    gs_font_base *pfont =
	      gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
			      "pcl_font_header(bitmap font)");

	    if ( pfont == 0 )
	      return_error(e_Memory);
	    code = pl_fill_in_font((gs_font *)pfont, plfont, pcls->font_dir,
				   mem);
	    if ( code < 0 )
	      return code;
	    pcl_fill_in_intelli_font(pfont, gs_next_ids(1));
	    /* pfh->Pitch is design unit width for scalable fonts. */
	    { uint pitch_cp =
		pl_get_uint16(pfh->Pitch) * 100 * pfont->FontMatrix.xx;
	      pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
	    }
	    break;
	  }
	  default:
	    return_error(gs_error_invalidfont);	/* can't happen */
	  }
	/* Extract common parameters from the font header. */
	plfont->params.symbol_set = pl_get_uint16(pfh->SymbolSet);
	plfont->params.proportional_spacing = pfh->Spacing;
	plfont->params.style = (pfh->StyleMSB << 8) + pfh->StyleLSB;
	plfont->params.stroke_weight = /* signed byte */
	  (int)(pfh->StrokeWeight ^ 0x80) - 0x80;
	plfont->params.typeface_family =
	  (pfh->TypefaceMSB << 8) + pfh->TypefaceLSB;
	pl_dict_put(&pcls->soft_fonts, id_key(pcls->font_id), 2, plfont);
	plfont->pfont->procs.define_font = gs_no_define_font;
	return gs_definefont(pcls->font_dir, plfont->pfont);
}

private int /* ESC * c <char_code> E */ 
pcl_character_code(pcl_args_t *pargs, pcl_state_t *pcls)
{	pcls->character_code = uint_arg(pargs);
	return 0;
}

private int /* ESC ( s <count> W */ 
pcl_character_data(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = uint_arg(pargs);
	const byte *data = arg_data(pargs);
	void *value;
#define plfont ((pl_font_t *)value)
	pcl_font_header_format_t format;
	byte *char_data = 0;

	if ( !pl_dict_find(&pcls->soft_fonts, id_key(pcls->font_id), 2, &value) )
	  return 0;		/* font not found */
	if ( count < 4 || data[2] > count - 2 )
	  return e_Range;
	if ( data[1] )
	  { /**** HANDLE CONTINUATION ****/
	    return e_Unimplemented;
	  }
	format = ((const pcl_font_header_t *)plfont->header)->HeaderFormat;
	switch ( data[0] )
	  {
	  case pccd_bitmap:
	    { uint width, height;
	      if ( data[2] != 14 ||
		   (format != pcfh_bitmap &&
		    format != pcfh_resolution_bitmap)
		 )
		return e_Range;
	      width = pl_get_uint16(data + 10);
	      height = pl_get_uint16(data + 12);
	      switch ( data[3] )
		{
		case 1:		/* uncompressed bitmap */
		  if ( count != 16 + ((width + 7) >> 3) * height )
		    return e_Range;
		  break;
		case 2:		/* compressed bitmap */
		  { uint y = 0;
		    const byte *src = data + 16;
		    const byte *end = data + count;
		    uint width_bytes = (width + 7) >> 3;
		    byte *row;

		    char_data =
		      gs_alloc_bytes(pcls->memory, 16 + width_bytes * height,
				     "pcl_character_data(compressed bitmap)");
		    if ( char_data == 0 )
		      return_error(e_Memory);
		    memcpy(char_data, data, 16);
		    memset(char_data + 16, 0, width_bytes * height);
		    row = char_data + 16;
		    while ( src < end && y < height )
		      { /* Read the next compressed row. */
		        uint x;
			int color = 0;
			uint reps = *src++;

			for ( x = 0; src < end && x < width; color ^= 1 )
			  { /* Read the next run. */
			    uint rlen = *src++;

			    if ( rlen > width - x )
			      return e_Range;  /* row overrun */
			    if ( color )
			      { /* Set the run to black. */
				bits_fill_rectangle(row, x, width_bytes,
						    ~(mono_fill_chunk)0,
						    rlen, 1);
			      }
			    x += rlen;
			  }
			row += width_bytes;
			++y;
			/* Replicate the row if needed. */
			for ( ; reps > 0 && y < height;
			      --reps, ++y, row += width_bytes
			    )
			  memcpy(row, row - width_bytes, width_bytes);
		      }
		  } break;
		default:
		  return e_Range;
		}
	    }
	    break;
	  case pccd_intellifont:
	    if ( data[2] != 2 ||
		 (format != pcfh_intellifont_bound &&
		  format != pcfh_intellifont_unbound)
	       )
	      return e_Range;
	    switch ( data[3] )
	      {
	      case 3:		/* non-compound character */
		/* See TRM Figure 11-16 (p. 11-67) for the following. */
		if ( count < 14 )
		  return e_Range;
		{ uint data_size = pl_get_uint16(data + 4);
		  uint contour_offset = pl_get_uint16(data + 6);
		  uint metric_offset = pl_get_uint16(data + 8);
		  uint outline_offset = pl_get_uint16(data + 10);
		  uint xy_offset = pl_get_uint16(data + 12);

		  /* The contour data excludes 4 initial bytes of header */
		  /* and 2 final bytes of padding/checksum. */
		  if ( data_size != count - 6 ||
		       contour_offset < 10 ||
		       metric_offset < contour_offset ||
		       outline_offset < metric_offset ||
		       xy_offset < outline_offset ||
		       xy_offset > count - 6
		     )
		    return e_Range;
		}
		break;
	      case 4:		/* compound character */
		/* See TRM Figure 11-18 (p. 11-68) and 11-19 (p. 11-73) */
		/* for the following. */
		if ( count < 8 )
		  return e_Range;
		{ /*
		   * TRM Figure 11-18 is wrong: the number of components
		   * is a byte, not a 16-bit quantity.  (It is identified
		   * correctly as a UB on p. 11-70, however.)
		   */
		  uint num_components = data[6];

		  if ( count != 8 + num_components * 6 + 2 )
		    return e_Range;
		}
		break;
	      default:
		return e_Range;
	      }
	    break;
	  case pccd_truetype:
	    if ( format != pcfh_truetype &&
		 format != pcfh_truetype_large
	       )
	      return e_Range;
	    break;
	  default:
	    return e_Range;
	  }
	/* Register the character. */
	/**** FREE PREVIOUS DEFINITION ****/
	/* Compressed bitmaps have already allocated and filled in */
	/* the character data structure. */
	if ( char_data == 0 )
	  { char_data = gs_alloc_bytes(pcls->memory, count,
				       "pcl_character_data");
	    if ( char_data == 0 )
	      return_error(e_Memory);
	    memcpy(char_data, data, count);
	  }
	return pl_font_add_glyph(plfont, pcls->character_code, char_data);
#undef plfont
}

/* Initialization */
private int
pcsfont_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'c', 'D',
	     PCL_COMMAND("Assign Font ID", pcl_assign_font_id,
			 pca_neg_error|pca_big_error)},
	  {'c', 'F',
	     PCL_COMMAND("Font Control", pcl_font_control,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CLASS_COMMAND_ARGS(')', 's', 'W', "Font Header",
				  pcl_font_header, pca_bytes)
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'E', "Character Code",
				  pcl_character_code, pca_neg_error|pca_big_ok)
	DEFINE_CLASS_COMMAND_ARGS('(', 's', 'W', "Character Data",
				  pcl_character_data, pca_bytes)
	return 0;
}
private void
pcsfont_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { id_set_value(pcls->font_id, 0);
	    pcls->character_code = 0;
	    if ( !(type & pcl_reset_initial) )
	      { pcl_args_t args;
	        arg_set_uint(&args, 1); /* delete temporary fonts */
		pcl_font_control(&args, pcls);
	      }
	  }
}
private int
pcsfont_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { /* Don't restore the soft font set. */
	    /**** MUST HANDLE POSSIBILITY THAT CURRENT FONT WAS DELETED. ****/
	    psaved->soft_fonts = pcls->soft_fonts;
	  }
	return 0;
}
const pcl_init_t pcsfont_init = {
  pcsfont_do_init, pcsfont_do_reset, pcsfont_do_copy
};
