/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcsfont.c */
/* PCL5 soft font creation commands */
#include "stdio_.h"		/* needed for pl_load_tt_font */
#include "pcommand.h"
#include "pcfont.h"
#include "pcstate.h"
#include "pldict.h"
#include "plfont.h"
#include "plvalue.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"

/* Define our directory of fonts. */
private gs_font_dir *pcl_font_dir;

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
	    }
	    return e_Unimplemented;
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
	bool bitmap;
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
	    bitmap = true;
	    break;
	  case pcfh_intellifont_bound:
	  case pcfh_intellifont_unbound:
	    return e_Unimplemented;
	  case pcfh_truetype:
	  case pcfh_truetype_large:
	    bitmap = false;
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
	plfont->font_type = pfh->FontType;
	plfont->header = header;
	plfont->header_size = count;
	code = pl_font_alloc_glyph_table(plfont, 256, mem,
					 "pcl_font_header(char table)");
	if ( code < 0 )
	  return code;
	/* Create the actual font. */
	if ( bitmap )
	  { /* Bitmap font. */
	    gs_font_base *pfont =
	      gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
			      "pcl_font_header(bitmap font)");

	    if ( pfont == 0 )
	      return_error(e_Memory);
	    code = pl_fill_in_font((gs_font *)pfont, plfont, pcl_font_dir,
				   mem);
	    if ( code < 0 )
	      return code;
	    pl_fill_in_bitmap_font(pfont, gs_next_ids(1));
	    plfont->scaling_technology = plfst_bitmap;
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
	    /* pitch_100ths is 100ths of characters per inch; */
	    /* the Pitch value in the header is in quarter dots. */
	    { ulong pitch_1024th_dots =
		((ulong)pl_get_uint16(pfh->Pitch) << 8) + pfh->PitchExtended;
	      if ( pitch_1024th_dots == 0 )
		{ /* Something bizarre is going on, but we shouldn't crash. */
		  plfont->params.pitch_100ths = 0;
		}
	      else
		{ plfont->params.pitch_100ths = (uint)
		    (plfont->resolution.x /* dots/inch */
		     / (pitch_1024th_dots / 1024.0) /* dots/char => char/in */
		     * 100);
		}
	    }
	    plfont->params.height_4ths = pl_get_uint16(pfh->Height);
	  }
	else
	  { /* TrueType font. */
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
	    code = pl_fill_in_font((gs_font *)pfont, plfont, pcl_font_dir,
				   mem);
	    if ( code < 0 )
	      return code;
	    pl_fill_in_tt_font(pfont, NULL, gs_next_ids(1));
	    /****** HOW TO SET pitch_100ths, height_4ths? ******/
	  }
	/* Extract common parameters from the font header. */
	plfont->font_type = pfh->FontType;
	plfont->params.symbol_set = pl_get_uint16(pfh->SymbolSet);
	plfont->params.proportional_spacing = pfh->Spacing;
	plfont->params.style = (pfh->StyleMSB << 8) + pfh->StyleLSB;
	plfont->params.stroke_weight = /* signed byte */
	  (int)(pfh->StrokeWeight ^ 0x80) - 0x80;
	plfont->params.typeface_family =
	  (pfh->TypefaceMSB << 8) + pfh->TypefaceLSB;
	pl_dict_put(&pcls->soft_fonts, id_key(pcls->font_id), 2, plfont);
	plfont->pfont->procs.define_font = gs_no_define_font;
	return gs_definefont(pcl_font_dir, plfont->pfont);
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
	byte *char_data;

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
	      width = (data[10] << 8) + data[11];
	      height = (data[12] << 8) + data[13];
	      switch ( data[3] )
		{
		case 1:		/* uncompressed bitmap */
		  if ( count != 16 + ((width + 7) >> 3) * height )
		    return e_Range;
		  break;
		case 2:		/* compressed bitmap */
		  return e_Unimplemented;
		default:
		  return e_Range;
		}
	    }
	    break;
	  case pccd_intellifont:
	    if ( format != pcfh_intellifont_bound &&
		 format != pcfh_intellifont_unbound
	       )
	      return e_Range;
	    return e_Unimplemented;
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
	char_data = gs_alloc_bytes(pcls->memory, count, "pcl_character_data");
	if ( char_data == 0 )
	  return_error(e_Memory);
	memcpy(char_data, data, count);
	return pl_font_add_glyph(plfont, pcls->character_code, char_data);
#undef plfont
}

/* Initialization */
private int
pcsfont_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'c', 'D', {pcl_assign_font_id, pca_neg_error|pca_big_error}},
	  {'c', 'F', {pcl_font_control, pca_neg_error|pca_big_error}},
	END_CLASS
	DEFINE_CLASS_COMMAND_ARGS(')', 's', 'W', pcl_font_header, pca_bytes)
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'E', pcl_character_code, pca_neg_error|pca_big_ok)
	DEFINE_CLASS_COMMAND_ARGS('(', 's', 'W', pcl_character_data, pca_bytes)
		/* One-time state initialization */
	pcl_font_dir = gs_font_dir_alloc(mem);
	return 0;
}
private void
pcsfont_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { id_set_value(pcls->font_id, 0);
	    pcls->character_code = 0;
	    if ( type & pcl_reset_initial )
	      {
	        pl_dict_init(&pcls->soft_fonts, pcls->memory, pl_free_font);
		/* Make hard fonts the parent, assuming that if we want to
		 * look at soft fonts we'll want hard fonts afterward, but
		 * not the reverse. */
		pl_dict_set_parent(&pcls->soft_fonts, &pcls->hard_fonts);
/* XXX =================================================================
 * Hack in some fonts.  This is not the right place to do this, let
 * alone the right way!  Expect to find some TT fonts in the current
 * directory; if so, pull in all we find and make them look like soft
 * fonts. */ 
		{
#include "stdio_.h"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#define	FONTDIR	"."
		    typedef struct font_hack {
			char	*ext_name;
			pl_font_params_t params;
		    } font_hack_t;
		    font_hack_t hack_table[] = {
			/* (typeface family values are faked, do not
			 * match the actual fonts) */
			{"arial",	{0, 1, 0, 0, 0, 0, 16602} },
			{"arialbd",	{0, 1, 0, 0, 0, 3, 16602} },
			{"arialbi",	{0, 1, 0, 0, 1, 3, 16602} },
			{"ariali",	{0, 1, 0, 0, 1, 0, 16602} },
			{"cour",	{0, 0, 0, 0, 0, 0, 4099} },
			{"courbd",	{0, 0, 0, 0, 0, 3, 4099} },
			{"courbi",	{0, 0, 0, 0, 1, 3, 4099} },
			{"couri",	{0, 0, 0, 0, 1, 0, 4099} },
			{"times",	{0, 1, 0, 0, 0, 0, 16901} },
			{"timesbd",	{0, 1, 0, 0, 0, 3, 16901} },
			{"timesbi",	{0, 1, 0, 0, 1, 3, 16901} },
			{"timesi",	{0, 1, 0, 0, 1, 0, 16901} },
			{NULL,		{0, 0, 0, 0, 0, 0, 0} }
		    };
		    DIR		*dp;
		    struct dirent *dep;
		    FILE	*fnp;
		    pl_font_t	*plfont;
		    font_hack_t	*hackp;
		    int		id = 0;
		    byte	key[2];

		    if ((dp=opendir(FONTDIR)) == NULL) {
			perror(FONTDIR);
			return;
		    }
		    while ((dep=readdir(dp)) != NULL) {
			if (strcmp(".ttf", &dep->d_name[dep->d_reclen-4])
				== 0) {

			    if ((fnp=fopen(dep->d_name, "r")) == NULL) {
				perror(dep->d_name);
				continue;
			    }
			    pl_load_tt_font(fnp, pcl_font_dir, pcls->memory,
				gs_next_ids(1), &plfont);
			    plfont->storage = pcds_internal;
			    /* extraordinary hack: get the font character-
			     * istics from a hardwired table; ignore the
			     * font if we don't know it. */
			    for (hackp=hack_table; hackp->ext_name; hackp++) {
				if (strncmp(hackp->ext_name, dep->d_name,
					dep->d_reclen - 4) == 0)
				    break;
			    }
			    if (hackp->ext_name == NULL)
				continue;	/* not in table */
			    plfont->params = hackp->params;
			    id++;
			    key[0] = id >> 8;
			    key[1] = id;
			    pl_dict_put(&pcls->soft_fonts, key, 2, plfont);
			}
		    }
		    closedir(dp);
		}
/* XXX end of font-init hack to be removed. */
	      }
	    else
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
