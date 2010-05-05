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

/* pxfont.c */
/* PCL XL font operators */

#include "math_.h"
#include "stdio_.h"
#include "string_.h"
#include "gdebug.h"
#include "plvalue.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxfont.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gschar.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gsimage.h"
#include "gsutil.h"			/* for string_match */
#include "gxfont.h"
#include "gxfont42.h"
#include "gxfixed.h"
#include "gxchar.h"
#include "gxpath.h"
#include "gzstate.h"
#include "pxptable.h"

/* ---------------- Initialization ---------------- */

int
pxfont_init(px_state_t *pxs)
{	/* Allocate the font directory now. */
	pxs->font_dir = gs_font_dir_alloc(pxs->memory);
	if ( pxs->font_dir == 0 )
	  return_error(errorInsufficientMemory);
	return 0;
}

/* ---------------- Operator utilities ---------------- */

static const pl_symbol_map_t *
pxl_find_symbol_map(uint symbol_set)
{
    const pl_symbol_map_t **ppsm = pl_built_in_symbol_maps;

    while ( *ppsm != 0 && pl_get_uint16((*ppsm)->id) != symbol_set )
        ++ppsm;
    return (*ppsm ? *ppsm : NULL);
}


/* Compute the symbol map from the font and symbol set. */
static void
set_symbol_map(px_state_t *pxs, bool wide16)
{	px_gstate_t *pxgs = pxs->pxgs;
	uint symbol_set = pxgs->symbol_set;
        pxgs->symbol_map = pxl_find_symbol_map(symbol_set);
}

static int
px_set_char_matrix(px_state_t *pxs)
{	px_gstate_t *pxgs = pxs->pxgs;
	px_font_t *pxfont = pxgs->base_font;
	gs_matrix mat;

	if ( pxfont == 0 )
	  return_error(errorNoCurrentFont);
	if ( pxfont->scaling_technology == plfst_bitmap )
	  { /*
	     * Bitmaps don't scale, shear, or rotate; however, we have to
	     * scale them to make the resolution match that of the device.
	     * Note that we disregard the character size, and, in px_text,
	     * all but the translation and orientation components of the
	     * CTM.
	     */
	    if ( pxgs->char_angle != 0 ||
		 pxgs->char_shear.x != 0 || pxgs->char_shear.y != 0 ||
		 pxgs->char_scale.x != 1 || pxgs->char_scale.y != 1
	       )
	      return_error(errorIllegalFontData);

	    /* remove negative scale component */
	    gs_make_scaling( pxs->units_per_measure.x / pxfont->resolution.x,
                             pxs->units_per_measure.y / pxfont->resolution.y,
                             &mat );
	    
	    /*
	     * Rotate the bitmap to undo the effect of its built-in
	     * orientation.
	     */
            gs_matrix_rotate(&mat,
                             90.0 * pxfont->header[1],
                             &mat);
	  }
	else
	  { float char_size = pxgs->char_size;
	    int i;

	    gs_make_identity(&mat);
	    /* H-P and Microsoft have Y coordinates running opposite ways. */
	    gs_matrix_scale(&mat, char_size, -char_size, &mat);
	    /* Apply the character transformations in the reverse order. */
	    for ( i = 0; i < 3; ++i )
	      switch ( pxgs->char_transforms[i] )
		{
		case pxct_rotate:
		  if ( pxgs->char_angle != 0 )
		    gs_matrix_rotate(&mat, 
				     pxgs->char_angle,
				     &mat);
		  break;
		case pxct_shear:
		  if ( pxgs->char_shear.x != 0 || pxgs->char_shear.y != 0 )
		    { gs_matrix smat;
		      gs_make_identity(&smat);
		      smat.yx = pxgs->char_shear.x;
		      smat.xy = pxgs->char_shear.y;
		      gs_matrix_multiply(&smat, &mat, &mat);
		    }
		  break;
		case pxct_scale:
		  if ( pxgs->char_scale.x != 1 || pxgs->char_scale.y != 1 )
		    gs_matrix_scale(&mat, pxgs->char_scale.x,
				    pxgs->char_scale.y, &mat);
		  break;
		}
	  }
	pxgs->char_matrix = mat;
	pxgs->char_matrix_set = true;
	return 0;
}

/* ---------------- Operator implementations ---------------- */

/* Define a font.  The caller must fill in pxfont->storage and ->font_type. */
/* This procedure implements FontHeader loading; it is exported for */
/* initializing the error page font. */
int
px_define_font(px_font_t *pxfont, byte *header, ulong size, gs_id id, px_state_t *pxs)
{	gs_memory_t *mem = pxs->memory;
	uint num_chars;

	/* Check for a valid font. */
	if ( size < 8 /* header */ + 6 /* 1 required segment */ +
	       6 /* NULL segment */
	   )
	  return_error(errorIllegalFontData);
	if ( header[0] != 0 /* format */ ||
	     header[5] != 0 /* variety */
	   )
	  return_error(errorIllegalFontHeaderFields);

	pxfont->header = (byte *)header; /* remove const cast */
 	pxfont->header_size = size;
	{ static const pl_font_offset_errors_t errors = {
	    errorIllegalFontData,
	    errorIllegalFontSegment,
	    errorIllegalFontHeaderFields,
	    errorIllegalNullSegmentSize,
	    errorMissingRequiredSegment,
	    errorIllegalGlobalTrueTypeSegment,
	    errorIllegalGalleyCharacterSegment,
	    errorIllegalVerticalTxSegment,
	    errorIllegalBitmapResolutionSegment
	  };
	  int code = pl_font_scan_segments(mem, pxfont, 4, 8, size, true, &errors);

	  if ( code < 0 )
	    return code;
	}
	num_chars = pl_get_uint16(header + 6);
	/* Allocate the character table. */
	{ /* Some fonts ask for unreasonably large tables.... */
	  int code = pl_font_alloc_glyph_table(pxfont, min(num_chars, 300),
					       mem, "px_define_font(glyphs)");
	  if ( code < 0 )
	    return code;
	}
	/* Now construct a gs_font. */
	if ( pxfont->scaling_technology == plfst_bitmap )
	  { /* Bitmap font. */
	    gs_font_base *pfont =
	      gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
			      "px_define_font(gs_font_base)");
	    int code;

	    if ( pfont == 0 )
	      return_error(errorInsufficientMemory);
	    code = px_fill_in_font((gs_font *)pfont, pxfont, pxs);
	    if ( code < 0 )
	      return code;
	    pl_fill_in_bitmap_font(pfont, id);
	  }
	else
	  { /* TrueType font. */
	    gs_font_type42 *pfont =
	      gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
			      "px_define_font(gs_font_type42)");
	    int code;

	    if ( pfont == 0 )
	      return_error(errorInsufficientMemory);
	    /* Some fonts ask for unreasonably large tables.... */
	    code = pl_tt_alloc_char_glyphs(pxfont, min(num_chars, 300), mem,
					   "px_define_font(char_glyphs)");
	    if ( code < 0 )
	      return code;

	    code = px_fill_in_font((gs_font *)pfont, pxfont, pxs);
	    if ( code < 0 )
	      return code;
            {
                /* some pcl xl drivers generate an incorrect sfnt
                   version, in particular they will use a true type
                   collection header where truetype was intended.  The
                   hp printer does not detect this problem.  Brutishly,
                   we write in what the driver writers intended here
                   and bypass and avoid later failures that would
                   result from an incorrect header. */
                static const byte version1_0[4] = {0, 1, 0, 0};
                /* offset to the sfnt version.   */
                uint offs = pxfont->offsets.GT +
                    (pxfont->large_sizes ? 6 : 4);

                if (gs_object_size(mem, header) >= offs + sizeof(version1_0))
                    memcpy(header + offs, version1_0, sizeof(version1_0));
            }
	    code = pl_fill_in_tt_font(pfont, NULL, id);
            if ( code < 0 )
                return code;
	  }
	pxfont->params.symbol_set = pl_get_uint16(header + 2);

        if ( header[4] == plfst_TrueType ) {
            pxfont->is_xl_format = true;
            pl_prepend_xl_dummy_header(mem, &header);
            pxfont->header = header;
            pxfont->header_size = gs_object_size(mem, header);
        } else {
            pxfont->is_xl_format = false;
        }

	return gs_definefont(pxs->font_dir, pxfont->pfont);
}

/* Concatenate a widened (16-bit) font name onto an error message string. */
void
px_concat_font_name(char *message, uint max_message, const px_value_t *pfnv)
{	char *mptr = message + strlen(message);
	uint fnsize = pfnv->value.array.size;
	uint i;

	/*
	 **** We truncate 16-bit font name chars to 8 bits
	 **** for the message.
	 */
	for ( i = 0; i < fnsize && mptr - message < max_message; ++mptr, ++i )
	  if ( (*mptr = (byte)integer_elt(pfnv, i)) < 32 )
	    *mptr = '?';
	*mptr = 0;
}

static inline bool
px_downloaded_and_bound(pl_font_t *plfont)
{
    return (plfont->storage != pxfsInternal && pl_font_is_bound(plfont));
}

/** Convert pxl text arguments into an array of gs_chars 
 * caller must allocate the correct size array pchar and free it later
 */
static void
px_str_to_gschars( px_args_t *par, px_state_t *pxs, gs_char *pchr)
{
    const px_value_t *pstr = par->pv[0];
    const unsigned char *str = (const unsigned char *)pstr->value.array.data;
    uint len = pstr->value.array.size;
    int i;
    gs_char chr;
    const pl_symbol_map_t *psm = pxs->pxgs->symbol_map;
    uint symbol_set = pxs->pxgs->symbol_set;
    bool db = px_downloaded_and_bound(pxs->pxgs->base_font);
    for (i=0; i < len; i++) {
        if (pstr->type & pxd_ubyte) {
            chr = str[i];
        } else {
            chr = uint16at(&str[i << 1], (pstr->type & pxd_big_endian));
        }
        pchr[i] = pl_map_symbol((db ? NULL : psm), chr,
                                pxs->pxgs->base_font->storage == pxfsInternal,
                                false /* pxl does not support MSL */,
                                symbol_set == 590);
    }
}

/* startup for the processing text */
static int
px_text_setup(gs_state * pgs, const gs_char * str, uint size,
		const float *x_widths, const float *y_widths,
		uint widths_size, gs_memory_t * mem, gs_text_enum_t ** ppte,
                bool to_path, bool can_cache)
{
    gs_text_params_t text;
    int code;

    text.operation = TEXT_FROM_CHARS | TEXT_REPLACE_WIDTHS | TEXT_RETURN_WIDTH;
    if (to_path)
        text.operation |= TEXT_DO_TRUE_CHARPATH;
    else
        text.operation |= TEXT_DO_DRAW;
    text.data.chars = str;
    text.size = size;
    text.x_widths = x_widths;
    text.y_widths = y_widths;
    text.widths_size = widths_size;
    code = gs_text_begin(pgs, &text, mem, ppte);
    if (!can_cache) {
        /* NB breaks API, needs a better solution. */
        gs_show_enum *penum = (gs_show_enum *)*ppte;
        penum->can_cache = -1;
    }
    return code;
}

/* Paint text or add it to the path. */
/* This procedure implements the Text and TextPath operators. */
/* Attributes: pxaTextData, pxaXSpacingData, pxaYSpacingData. */

int
px_text(px_args_t *par, px_state_t *pxs, bool to_path)
{
    gs_memory_t *mem = pxs->memory;
    gs_state *pgs = pxs->pgs;
    px_gstate_t *pxgs = pxs->pxgs;
    gs_text_enum_t *penum;
    const px_value_t *pstr = par->pv[0];
    uint len = pstr->value.array.size;
    const px_value_t *pxdata = par->pv[1];
    const px_value_t *pydata = par->pv[2];
    gs_font *pfont = gs_currentfont(pgs);
    int code = 0;
    gs_char *pchr = 0;
    pl_font_t *plfont;

    if ( pfont == 0 )
	return_error(errorNoCurrentFont);

    plfont = (pl_font_t *)pfont->client_data;
    if ( (pxdata != 0 && pxdata->value.array.size != len) ||
	 (pydata != 0 && pydata->value.array.size != len)
	)
	return_error(errorIllegalArraySize);
    if ( !pxgs->base_font )
	return_error(errorNoCurrentFont);
    if ( !pxgs->char_matrix_set ) {
        gs_matrix *cm = &pxgs->char_matrix;
        float det;
        code = px_set_char_matrix(pxs);
        if ( code < 0 )
            return code;
        /* check for a singular matrix - this does not generate an
           interpreter error on hp.  Casts prevent double precision
           temporary variables, as in gsmatrix.c. */
        det = (float)(cm->xx * cm->yy) - (float)(cm->xy * cm->yx);
        if ( det == 0 )
            return 0;
    }

    /* set the character matrix in the graphics state */
    gs_setcharmatrix(pgs, &pxgs->char_matrix);
    /* The character matrix is not visible to devices.  High level
       devices get character scaling information from the font's
       matrix (FontMatrix).  */
    gs_matrix_multiply(&pxgs->char_matrix, &pfont->orig_FontMatrix,
                       &pfont->FontMatrix);
    /* we don't need to consider the vertical mess for resident fonts */
    if (plfont->storage != pxfsDownLoaded) {
        pfont->WMode = 0; /* horizontal */
        plfont->allow_vertical_substitutes = false;
    } else { /* downloaded */
        pfont->WMode = pxgs->writing_mode;
        /* allow vertical substitutes for non-bitmap characters if requested. */
        if ( pxgs->char_sub_mode == eVerticalSubstitution &&
             plfont->scaling_technology != plfst_bitmap )
            plfont->allow_vertical_substitutes = true;
        else 
            plfont->allow_vertical_substitutes = false;
    }

    /* set bold fraction - charpaths are not algorithmically boldened */
    if (to_path == false )
        plfont->bold_fraction = pxgs->char_bold_value;

    pchr = (gs_char *)gs_alloc_byte_array(mem, len, sizeof(gs_char),
                                          "px_text gs_char[]");
    if (pchr == 0) 
	return_error(errorInsufficientMemory);

    px_str_to_gschars(par, pxs, pchr);

    {
        uint i;
	float *fxvals = 0;
        float *fyvals = 0;
	
	if ( len > 0 ) {
	    fxvals = (float *)gs_alloc_byte_array(mem, len+1, sizeof(float), "px_text fxvals");
            fyvals = (float *)gs_alloc_byte_array(mem, len+1, sizeof(float) * 2, "px_text fyals");
	    if ( fxvals == 0 || fyvals == 0 )
		return_error(errorInsufficientMemory);
	}
        for ( i = 0; i < len; i++ ) {
            fxvals[i] = pxdata ? real_elt(pxdata, i) : 0.0;
            fyvals[i] = pydata ? real_elt(pydata, i) : 0.0;
        }

        code = px_text_setup(pgs, pchr, len, fxvals, fyvals,
                             len, mem, &penum, to_path,
                             pxgs->char_bold_value == 0);

        if ( code >= 0 ) {
            code = gs_text_process(penum);
	    gs_text_release(penum, "pxtext");
	}
	if ( fxvals ) 
	    gs_free_object( mem, fxvals, "px_text fvals" );
	if ( fyvals ) 
	    gs_free_object( mem, fyvals, "py_text fvals" );
    }

    gs_free_object( mem, pchr, "px_text gs_char" );
    return (code == gs_error_invalidfont ?
	    gs_note_error(errorBadFontData) : code);
}


/* ---------------- Operators ---------------- */

const byte apxSetFont[] = {
  pxaFontName, pxaCharSize, pxaSymbolSet, 0, 0
};
int
pxSetFont(px_args_t *par, px_state_t *pxs)
{	px_gstate_t *pxgs = pxs->pxgs;
	px_font_t *pxfont;
	px_value_t *pfnv = par->pv[0];
	uint symbol_set = par->pv[2]->value.i;
	int code = px_find_font(pfnv, symbol_set, &pxfont, pxs);

	if ( code < 0 )
	  { switch ( code )
	    {
	    case errorFontUndefined:
	      strcpy(pxs->error_line, "FontUndefined - ");
	      goto undef;
	    case errorFontUndefinedNoSubstituteFound:
	      strcpy(pxs->error_line, "FontUndefinedNoSubstituteFound - ");
undef:	      px_concat_font_name(pxs->error_line, px_max_error_line, pfnv);
	      break;
	    case errorSymbolSetRemapUndefined:
	      strcpy(pxs->error_line, "SymbolSetRemapUndefined - ");
	      px_concat_font_name(pxs->error_line, px_max_error_line, pfnv);
	      { char setstr[26];	/* 64-bit value plus message */
	        sprintf(setstr, " : %d", symbol_set);
		strncat(pxs->error_line, setstr,
			px_max_error_line - strlen(pxs->error_line));
		pxs->error_line[px_max_error_line] = 0;
	      }
	      break;
	    }
	    return code;
	  }
	code = gs_setfont(pxs->pgs, pxfont->pfont);
	if ( code < 0 )
	  return code;
	pxgs->char_size = real_value(par->pv[1], 0);
	pxgs->symbol_set = symbol_set;
	pxgs->base_font = pxfont;
	set_symbol_map(pxs, pxfont->font_type == plft_16bit);
	pxgs->char_matrix_set = false;
	return 0;
}

const byte apxBeginFontHeader[] = {
  pxaFontName, pxaFontFormat, 0, 0};
int
pxBeginFontHeader(px_args_t *par, px_state_t *pxs)
{	px_value_t *pfnv = par->pv[0];
	gs_memory_t *mem = pxs->memory;
	px_font_t *pxfont;
	int code = px_find_existing_font(pfnv, &pxfont, pxs);

	if ( code >= 0 )
	  { strcpy(pxs->error_line, "FontNameAlreadyExists - ");
	    px_concat_font_name(pxs->error_line, px_max_error_line, pfnv);
	    return_error(errorFontNameAlreadyExists);
	  }
	/* Make a partially filled-in dictionary entry. */
	pxfont = pl_alloc_font(mem, "pxBeginFontHeader(pxfont)");
	if ( pxfont == 0 )
	  return_error(errorInsufficientMemory);
	pxfont->storage = pxfsDownLoaded;
	pxfont->data_are_permanent = false;
	code = px_dict_put(&pxs->font_dict, par->pv[0], pxfont);
	if ( code < 0 )
	  { gs_free_object(mem, pxfont, "pxBeginFontHeader(pxfont)");
	    return code;
	  }
	pxs->download_font = pxfont;
	pxs->download_bytes.data = 0;
	pxs->download_bytes.size = 0;
	return 0;
}

const byte apxReadFontHeader[] = {
  pxaFontHeaderLength, 0, 0
};
int
pxReadFontHeader(px_args_t *par, px_state_t *pxs)
{	ulong len = par->pv[0]->value.i;
	ulong left = len - par->source.position;
	int code = pxNeedData;

	if ( left > 0 )
	  { ulong pos;
	    if ( par->source.position == 0 )
	      { /* (Re-)allocate the downloaded data. */
		void *new_data;

		if ( par->source.available == 0 )
		  return code;
		new_data =
		  (pxs->download_bytes.size == 0 ?
		   gs_alloc_bytes(pxs->memory, len, "pxReadFontHeader") :
		   gs_resize_object(pxs->memory, pxs->download_bytes.data,
				    pxs->download_bytes.size + len,
				    "pxReadFontHeader"));
		if ( new_data == 0 )
		  return_error(errorInsufficientMemory);
		pxs->download_bytes.data = new_data;
		pxs->download_bytes.size += len;
	      }
	    if ( left > par->source.available )
	      left = par->source.available;
	    else
	      code = 0;
	    pos = pxs->download_bytes.size - len + par->source.position;
	    memcpy(pxs->download_bytes.data + pos, par->source.data, left);
	    par->source.position += left;
	    par->source.data += left;
	    par->source.available -= left;
	    if ( pos < 8 && pos + left >= 8 )
	      { /* Check the font header fields now. */
		const byte *data = pxs->download_bytes.data;
		if ( data[0] | data[5] )
		  return_error(errorIllegalFontHeaderFields);
		switch ( data[4] )
		  {
		  case plfst_TrueType:
		    if ( data[1] )
		      return_error(errorIllegalFontHeaderFields);
		    break;
		  case plfst_bitmap:
		    if ( data[1] & ~3 )
		      return_error(errorIllegalFontHeaderFields);
		    break;
		  default:
		    return_error(errorIllegalFontHeaderFields);
		  }
	      }
	  }
	return code;
}

const byte apxEndFontHeader[] = {0, 0};
int
pxEndFontHeader(px_args_t *par, px_state_t *pxs)
{	px_font_t *pxfont = pxs->download_font;
	int code = px_define_font(pxfont, pxs->download_bytes.data,
				  (ulong)pxs->download_bytes.size,
				  gs_next_ids(pxs->memory, 1), pxs);

	/****** HOW TO DETERMINE FONT TYPE? ******/
	pxfont->font_type = plft_16bit;
	/* Clear pointers for GC */
	pxs->download_font = 0;
	pxs->download_bytes.data = 0;
	return code;
}

const byte apxBeginChar[] = {
  pxaFontName, 0, 0
};
int
pxBeginChar(px_args_t *par, px_state_t *pxs)
{	px_value_t *pfnv = par->pv[0];
	px_font_t *pxfont;
	int code = px_find_existing_font(pfnv, &pxfont, pxs);

	if ( code >= 0 && pxfont == 0 )
	  code = gs_note_error(errorFontUndefined);
	if ( code < 0 )
	  { if ( code == errorFontUndefined )
	      { strcpy(pxs->error_line, "FontUndefined - ");
	        px_concat_font_name(pxs->error_line, px_max_error_line, pfnv);
	      }
	    return code;
	  }
	if ( pxfont->storage != pxfsDownLoaded )
	  return_error(errorCannotReplaceCharacter);
	pxs->download_font = pxfont;
	return 0;
}

const byte apxReadChar[] = {
  pxaCharCode, pxaCharDataSize, 0, 0
};
int
pxReadChar(px_args_t *par, px_state_t *pxs)
{	uint char_code = par->pv[0]->value.i;
	uint size = par->pv[1]->value.i;
	uint pos = par->source.position;

	if ( pos == 0 )
	  { /* We're starting a character definition. */
	    byte *def;

	    if ( size < 2 )
	      return_error(errorIllegalCharacterData);
	    if ( par->source.available == 0 )
	      return pxNeedData;
	    def = gs_alloc_bytes(pxs->memory, size, "pxReadChar");
	    if ( def == 0 )
	      return_error(errorInsufficientMemory);
	    pxs->download_bytes.data = def;
	    pxs->download_bytes.size = size;
	  }
	while ( pos < size )
	  { uint copy = min(par->source.available, size - pos);

	    if ( copy == 0 )
	      return pxNeedData;
	    memcpy(pxs->download_bytes.data + pos, par->source.data, copy);
	    par->source.data += copy;
	    par->source.available -= copy;
	    par->source.position = pos += copy;
	  }
	/* We have the complete character. */
	/* Do error checks before installing. */
	{ 
            /* const byte *header = pxs->download_font->header;
             see NB just below */
	  const byte *data = pxs->download_bytes.data;
	  int code = 0;

	  switch ( data[0] )
	    {
	    case 0:		/* bitmap */
                if ( false /* NB FIXME header[4] != plfst_bitmap */)
		code = gs_note_error(errorFSTMismatch);
	      else if ( data[1] != 0 )
		code = gs_note_error(errorUnsupportedCharacterClass);
	      else if ( size < 10 ||
			size != 10 + ((pl_get_uint16(data + 6) + 7) >> 3) *
			  pl_get_uint16(data + 8)
		      )
		code = gs_note_error(errorIllegalCharacterData);
	      break;
	    case 1:		/* TrueType outline */
                if ( false /* NB FIXME header[4] != plfst_TrueType */ )
		code = gs_note_error(errorFSTMismatch);
	      else if ( data[1] != 0 && data[1] != 1 && data[1] != 2 )
		code = gs_note_error(errorUnsupportedCharacterClass);
	      else if ( size < 6 || size != 2 + pl_get_uint16(data + 2) )
		code = gs_note_error(errorIllegalCharacterData);
	      break;
	    default:
	      code = gs_note_error(errorUnsupportedCharacterFormat);
	    }
	  if ( code >= 0 )
	    { 
                code = pl_font_add_glyph(pxs->download_font, char_code, (byte *)data); /* const cast */
                if ( code < 0 )
                    code = gs_note_error(errorInternalOverflow);
            }
                                                    
          if ( code < 0 )
              gs_free_object(pxs->memory, pxs->download_bytes.data,
                             "pxReadChar");
          pxs->download_bytes.data = 0;
          return code;
        }
}

const byte apxEndChar[] = {0, 0};
int
pxEndChar(px_args_t *par, px_state_t *pxs)
{	return 0;
}

const byte apxRemoveFont[] = {
  pxaFontName, 0, 0
};
int
pxRemoveFont(px_args_t *par, px_state_t *pxs)
{	px_value_t *pfnv = par->pv[0];
	px_font_t *pxfont;
	int code = px_find_existing_font(pfnv, &pxfont, pxs);
	const char *error = 0;

	if ( code < 0 )
	  error = "UndefinedFontNotRemoved - ";
	else if ( pxfont == 0 )	/* built-in font, assume internal */
	  error = "InternalFontNotRemoved - ";
	else
	  switch ( pxfont->storage )
	    {
	    case pxfsInternal:
	      error = "InternalFontNotRemoved - ";
	      break;
	    case pxfsMassStorage:
	      error = "MassStorageFontNotRemoved - ";
	      break;
	    default:		/* downloaded */
	      ;
	    }
	if ( error )
	  { /* Construct a warning message including the font name. */
	    char message[px_max_error_line + 1];

	    strcpy(message, error);
	    px_concat_font_name(message, px_max_error_line, pfnv);
	    code = px_record_warning(message, false, pxs);
	  }
	/****** WHAT IF THIS IS THE CURRENT FONT? ******/
	px_dict_undef(&pxs->font_dict, par->pv[0]);
	return 0;
}
