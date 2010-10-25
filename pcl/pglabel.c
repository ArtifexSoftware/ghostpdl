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

/* pglabel.c - HP-GL/2 label commands */

#include "math_.h"
#include "memory_.h"
#include "ctype_.h"
#include "stdio_.h"		/* for gdebug.h */
#include "gdebug.h"
#include "pcparse.h"
#include "plvalue.h"
#include "pgmand.h"
#include "pginit.h"
#include "pgfont.h"
#include "pgdraw.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcpage.h"
#include "pcfsel.h"
#include "pcsymbol.h"
#include "pcpalet.h"
#include "pcdraw.h"
#include "gscoord.h"
#include "gsline.h"
#include "gspath.h"
#include "gsutil.h"
#include "gxchar.h"		/* for show enumerator */
#include "gxfont.h"
#include "gxstate.h"		/* for gs_state_client_data */

#define STICK_FONT_TYPEFACE 48

/* NB this next constant is not quite right.  The STICK_FONT_TYPEFACE
   definition is used in the code to cover both stick and arc fonts.
   Typically the prescription was to choose the stick font typeface
   (48) and enable proportional spacing to get the arc font.  More
   recently we discovered a new typeface family number that can be
   used for the proportionally spaced arc fonts (50).  This has been
   reflected in the hpgl/2 selection code but nowhere else. */
#define ARC_FONT_TYPEFACE 50

/* currently selected font */
static pl_font_t *
hpgl_currentfont(const hpgl_state_t *pgls)
{
    return pgls->g.font_selection[pgls->g.font_selected].font;
}

static bool
hpgl_is_currentfont_stick(const hpgl_state_t *pgls)
{
    pl_font_t *plfont = hpgl_currentfont(pgls);
    if (!plfont)
        return false;
    return ( ((plfont->params.typeface_family & 0xfff) == STICK_FONT_TYPEFACE) &&
            (plfont->params.proportional_spacing == false) );
}

/* convert points 2 plu - agfa uses 72.307 points per inch */
static floatp
hpgl_points_2_plu(const hpgl_state_t *pgls, floatp points)
{	
    const pcl_font_selection_t *pfs =
	&pgls->g.font_selection[pgls->g.font_selected];
    floatp ppi = 72.0;
    if ( pfs->font->scaling_technology == plfst_Intellifont )
	ppi = 72.307;
    return points * (1016.0 / ppi);
}

/* ------ Next-character procedure ------ */

/* is it a printable character - duplicate of pcl algorithm in
   pctext.c */
static bool
hpgl_is_printable(
    const pl_symbol_map_t * psm,
    gs_char                 chr,
    bool                    is_stick,
    bool                    transparent
)
{    
    if (transparent)
        return true;
    if ( is_stick )
	return (chr >= ' ') && (chr <= 0xff);
    if ((psm == 0) || (psm->type >= 2))
        return true;
    else if (psm->type == 1)
        chr &= 0x7f;
    return (chr >= ' ') && (chr <= '\177');
}

/*
 * Map a character through the symbol set, if needed.
 */
static gs_char
hpgl_map_symbol(uint chr, const hpgl_state_t *pgls)
{	
    const pcl_font_selection_t *pfs =
        &pgls->g.font_selection[pgls->g.font_selected];
    const pl_symbol_map_t *psm = pfs->map;
    
    return pl_map_symbol(psm, chr,
                         pfs->font->storage == pcds_internal,
                         pfs->font->font_type == plgv_MSL,
                         false);
}

/* ------ Font selection ------- */

/* Select primary (0) or alternate (1) font. */
static void
hpgl_select_font_pri_alt(hpgl_state_t *pgls, int index)
{
    if ( pgls->g.font_selected != index ) {
        hpgl_free_stick_fonts(pgls);
	pgls->g.font_selected = index;
	pgls->g.font = 0;
    }
    return;
}

/* forward decl */
static int hpgl_recompute_font(hpgl_state_t *pgls);

/* Ensure a font is available. */
static int
hpgl_ensure_font(hpgl_state_t *pgls) 
{
    if ( ( pgls->g.font == 0 ) || ( pgls->g.font->pfont == 0 ) )
	hpgl_call(hpgl_recompute_font(pgls));
    return 0;
}

/*
 * The character complement for the stick font is puzzling: it doesn't seem
 * to correspond directly to any of the MSL *or* Unicode symbol set bits
 * described in the Comparison Guide.  We set the bits for MSL Basic Latin
 * (63) and for Unicode ASCII (31), and Latin 1 (30).
 */
static const byte stick_character_complement[8] = {
  0x7f, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xfe
};

/* Select the stick font, creating it if necessary. */
/* We break this out only for readability: it's only called in one place. */
static int
hpgl_select_stick_font(hpgl_state_t *pgls)
{	pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];
	pl_font_t *font = &pgls->g.stick_font[pgls->g.font_selected]
	  [pfs->params.proportional_spacing];
        gs_font_base *pfont;
        int code;
	/* Create a gs_font if none has been created yet. */
        hpgl_free_stick_fonts(pgls);
	pfont = gs_alloc_struct(pgls->memory, gs_font_base, &st_gs_font_base,
			      "stick/arc font");
        

        if ( pfont == 0 )
            return_error(e_Memory);
        code = pl_fill_in_font((gs_font *)pfont, font, pgls->font_dir,
                               pgls->memory, "stick/arc font");
        if ( code < 0 )
            return code;
        if ( pfs->params.proportional_spacing )
            hpgl_fill_in_arc_font(pfont, gs_next_ids(pgls->memory, 1));
        else
            hpgl_fill_in_stick_font(pfont, gs_next_ids(pgls->memory, 1));
        font->pfont = (gs_font *)pfont;
        font->scaling_technology = plfst_TrueType;/****** WRONG ******/
        font->font_type = plft_Unicode;
        memcpy(font->character_complement, stick_character_complement, 8);
	/*
	 * The stick/arc font is protean: set its proportional spacing,
	 * style, and stroke weight parameters to the requested ones.
	 * We could fill in some of the other characteristics earlier,
	 * but it's simpler to do it here.
	 */
	font->params = pfs->params;
	font->params.typeface_family = STICK_FONT_TYPEFACE;
	/*
	 * The stick font is defined in a cell that's only 2/3
	 * the size of the actual character.
	 */
	pl_fp_set_pitch_cp(&font->params, 1000.0*2/3);
	pfs->font = font;
	{ 
	    byte id[2];

	    id[0] = pfs->params.symbol_set >> 8;
	    id[1] = pfs->params.symbol_set & 0xff;
	    pfs->map = pcl_find_symbol_map(pgls,
					   id, plgv_Unicode,
                                           font->font_type == plft_16bit);
	} 
	return 0;
}

/* Check whether the stick font supports a given symbol set. */
static bool
hpgl_stick_font_supports(const pcl_state_t *pcs, uint symbol_set)
{	
    pl_glyph_vocabulary_t gv = 
        pl_complement_to_vocab(stick_character_complement);
    byte id[2];
    pl_symbol_map_t *map;

    id[0] = symbol_set >> 8;
    id[1] = symbol_set & 0xff;
    if ( (map = pcl_find_symbol_map(pcs, id, gv, false)) == 0 )
        return false;
    return pcl_check_symbol_support(map->character_requirements,
                                    stick_character_complement);
}

/* Recompute the current font if necessary. */
static int
hpgl_recompute_font(hpgl_state_t *pgls)
{	pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];

       if (( ((pfs->params.typeface_family & 0xfff) == STICK_FONT_TYPEFACE || 
	      (pfs->params.typeface_family & 0xfff) == ARC_FONT_TYPEFACE )
	     && pfs->params.style == 0 /* upright */
	     && hpgl_stick_font_supports(pgls,
					 pfs->params.symbol_set))
	     /* rtl only has stick fonts */
	     || ( pgls->personality == rtl )
	   )
	  hpgl_call(hpgl_select_stick_font(pgls));
	else
         { int code = pcl_reselect_font(pfs, pgls, false);

	    if ( code < 0 )
	      return code;
	  }
	pgls->g.font = pfs->font;
	pgls->g.map = pfs->map;
	return pl_load_resident_font_data_from_file(pgls->memory, pfs->font);
}

/* ------ Position management ------ */

/* accessor for character extra space takes line feed direction into account */
static inline hpgl_real_t
hpgl_get_character_extra_space_x(const hpgl_state_t *pgls) 
{
    return (pgls->g.character.line_feed_direction < 0) ? 
	pgls->g.character.extra_space.y :
	pgls->g.character.extra_space.x;
}

static inline hpgl_real_t
hpgl_get_character_extra_space_y(const hpgl_state_t *pgls) 
{
    return (pgls->g.character.line_feed_direction < 0) ? 
	pgls->g.character.extra_space.x :
	pgls->g.character.extra_space.y;
}

/* Get a character width in the current font, plus extra space if any. */
/* If the character isn't defined, return 1, otherwise return 0. */
static int
hpgl_get_char_width(const hpgl_state_t *pgls, gs_char ch, hpgl_real_t *width)
{
    gs_glyph glyph = hpgl_map_symbol(ch, pgls);
    const pcl_font_selection_t *pfs =
	&pgls->g.font_selection[pgls->g.font_selected];
    int code = 0;
    gs_point gs_width;
    if ( pgls->g.character.size_mode == hpgl_size_not_set ) {
	if ( pfs->params.proportional_spacing ) {
	    code = pl_font_char_width(pfs->font, (void *)(pgls->pgs), glyph, &gs_width);
            /* hack until this code gets written properly, the
               following should amount to a percentage of the
               em-square the space character would occupy... */
            if (code == 1) {
                gs_width.y = 0;
                gs_width.x = pl_fp_pitch_cp(&pfs->font->params) / 1000.0;
            }
                
	    if ( !pl_font_is_scalable(pfs->font) ) {
		if ( code == 0 )
		    *width = gs_width.x	* inches_2_plu(1.0 / pfs->font->resolution.x); 
		else
		    *width = coord_2_plu(pl_fp_pitch_cp(&pfs->font->params));
		goto add;
	    }
	    else if ( code >= 0 ) { 
		*width = gs_width.x * hpgl_points_2_plu(pgls, pfs->params.height_4ths / 4.0);
		goto add;
	    }
	    code = 1;
	}
	*width = hpgl_points_2_plu(pgls, pl_fp_pitch_cp(&pfs->params) / 100.0);
    } else {
	*width = pgls->g.character.size.x;
	if (pgls->g.character.size_mode == hpgl_size_relative)
	    *width *= pgls->g.P2.x - pgls->g.P1.x;

    }
 add:	

    if ( hpgl_get_character_extra_space_x(pgls) != 0 ) {
	/* Add extra space. */
	if ( pfs->params.proportional_spacing && ch != ' ' ) {
	    /* Get the width of the space character. */
	    int scode =
		pl_font_char_width(pfs->font, (void *)(pgls->pgs), hpgl_map_symbol(' ', pgls), &gs_width);
	    hpgl_real_t extra;

	    if ( scode >= 0 )
		extra = gs_width.x * hpgl_points_2_plu(pgls, pfs->params.height_4ths / 4.0);
	    else
		extra = hpgl_points_2_plu(pgls, (pl_fp_pitch_cp(&pfs->params)) / 10.0);
	    *width += extra * hpgl_get_character_extra_space_x(pgls);
	} else {
	    /* All characters have the same width, */
	    /* or we're already getting the width of a space. */
	    *width *= 1.0 + hpgl_get_character_extra_space_x(pgls);
	}
    }
    return code;
}
/* Get the cell height or character height in the current font, */
/* plus extra space if any. */
static int
hpgl_get_current_cell_height(const hpgl_state_t *pgls, hpgl_real_t *height)
{
	const pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];

        if ( pfs->font->scaling_technology != plfst_bitmap ) {
            gs_point scale = hpgl_current_char_scale(pgls);
            *height = fabs(scale.y);
        } else { 
            /* NB temporary not correct */
            *height = hpgl_points_2_plu(pgls, pfs->params.height_4ths / 4.0);
        }

        /* the HP manual says linefeed distance or cell height is 1.33
           times point size for stick fonts and "about" 1.2 times the
           point size for "most" fonts.  Empirical results suggest the
           stick font value is 1.28.  NB This value appears to be
           slightly different for the proportional arc font. */
        if (pgls->g.character.text_path == hpgl_text_right || pgls->g.character.text_path == hpgl_text_left) {
            if ( hpgl_is_currentfont_stick(pgls) )
                *height *= 1.28;
            else
                *height *= 1.2;
        } else {
            /* INC */
            if ( hpgl_is_currentfont_stick(pgls) )
                *height *= .96;
            else
                *height *= .898;
        }
            

	*height *= 1.0 + hpgl_get_character_extra_space_y(pgls);
	return 0;
}

/* distance tranformation for character slant */
static int
hpgl_slant_transform_distance(hpgl_state_t *pgls, gs_point *dxy, gs_point *s_dxy)
{
    if ( pgls->g.character.slant && !pgls->g.bitmap_fonts_allowed ) {
	gs_matrix smat;
	gs_point tmp_dxy = *dxy;
	gs_make_identity(&smat);
	smat.yx = pgls->g.character.slant;
	hpgl_call(gs_distance_transform(tmp_dxy.x, tmp_dxy.y, &smat, s_dxy));
    }
    return 0;
}

/* distance tranformation for character direction */
static int
hpgl_rotation_transform_distance(hpgl_state_t *pgls, gs_point *dxy, gs_point *r_dxy)
{
    double  run = pgls->g.character.direction.x;
    double rise = pgls->g.character.direction.y;
    if ( rise != 0 ) {
	double  denom = hypot(run, rise);
	gs_point tmp_dxy = *dxy;
	gs_matrix rmat;
	gs_make_identity(&rmat);
	rmat.xx = run / denom;
	rmat.xy = rise / denom;
	rmat.yx = -rmat.xy;
	rmat.yy = rmat.xx;
	hpgl_call(gs_distance_transform(tmp_dxy.x, tmp_dxy.y, &rmat, r_dxy));
    }
    return 0;
}

/* Reposition the cursor.  This does all the work for CP, and is also */
/* used to handle some control characters within LB. */
/* If pwidth != 0, it points to a precomputed horizontal space width. */
static int
hpgl_move_cursor_by_characters(hpgl_state_t *pgls, hpgl_real_t spaces,
  hpgl_real_t lines, const hpgl_real_t *pwidth)
{
    double nx, ny;
    double dx = 0, dy = 0;

    hpgl_call(hpgl_ensure_font(pgls));

    lines *= pgls->g.character.line_feed_direction;
    /* For vertical text paths, we have to swap spaces and lines. */
    switch ( pgls->g.character.text_path )
	{
	case hpgl_text_right:
	    nx = spaces, ny = lines; break;
	case hpgl_text_down:
	    nx = lines, ny = -spaces; break;
	case hpgl_text_left:
	    nx = -spaces, ny = -lines; break;
	case hpgl_text_up:
	    nx = -lines, ny = spaces; break;
	}
    /* calculate the next label position in relative coordinates. */
    if ( nx != 0 ) {
	hpgl_real_t width;
	if ( pwidth != 0 )
	    width = *pwidth;
	else
	    hpgl_get_char_width(pgls, ' ', &width);
	dx = width * nx;
    }
    if ( ny != 0 ) {
	hpgl_real_t height;
	hpgl_call(hpgl_get_current_cell_height(pgls, &height));
        dy = ny * height;
    }

    /*
     * We just computed the deltas in user units if characters are
     * using relative sizing, and in PLU otherwise.
     * If scaling is on but characters aren't using relative
     * sizing, we have to convert the deltas to user units.
     */
    if ( pgls->g.scaling_type != hpgl_scaling_none )
	{ 
	    gs_matrix mat;
	    gs_point user_dxy;
	    hpgl_call(hpgl_compute_user_units_to_plu_ctm(pgls, &mat));
	    hpgl_call(gs_distance_transform_inverse(dx, dy, &mat, &user_dxy));
	    dx = user_dxy.x;
	    dy = user_dxy.y;
	}

    {
	gs_point dxy;
	dxy.x = dx;
	dxy.y = dy;
	hpgl_rotation_transform_distance(pgls, &dxy, &dxy);
	dx = dxy.x;
	dy = dxy.y;
    }
    /* a relative move to the new position */
    hpgl_call(hpgl_add_point_to_path(pgls, dx, dy,
				     hpgl_plot_move_relative, true));

    if ( lines != 0 ) { 
	/* update the position of the carriage return point */
	pgls->g.carriage_return_pos.x += dx;
	pgls->g.carriage_return_pos.y += dy;
    }
    /* free any selected stick fonts */
    hpgl_free_stick_fonts(pgls);
    return 0;
}

/* Execute a CR for CP or LB. */
static int
hpgl_do_CR(hpgl_state_t *pgls)
{	
    return hpgl_add_point_to_path(pgls, pgls->g.carriage_return_pos.x,
				  pgls->g.carriage_return_pos.y,
				  hpgl_plot_move_absolute,
				  true);
}

/* CP [spaces,lines]; */
/* CP [;] */
int
hpgl_CP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t spaces, lines;

	if ( hpgl_arg_c_real(pgls->memory, pargs, &spaces) )
	  {
	    if ( !hpgl_arg_c_real(pgls->memory, pargs, &lines) )
	      return e_Range;
	  }
	else
	  {
	    /* if there are no arguments a carriage return and line feed
	       is executed */
	    hpgl_call(hpgl_do_CR(pgls));
	    spaces = 0, lines = -1;
	  }
	return hpgl_move_cursor_by_characters(pgls, spaces, lines,
					      (const hpgl_real_t *)0);
}

/* ------ Label buffer management ------ */

/* initialize the character buffer, setting state pointers for the
   beginning of the character buffer and the current character within
   the buffer to position 0. */
static int
hpgl_init_label_buffer(hpgl_state_t *pgls)
{
	pgls->g.label.char_count = 0;
	pgls->g.label.buffer_size = hpgl_char_count;
	return ((pgls->g.label.buffer =
		   gs_alloc_bytes(pgls->memory, hpgl_char_count,
				  "hpgl_init_label_buffer")) == 0 ?
		e_Memory :
		0);
}

/* release the character buffer */
static int
hpgl_destroy_label_buffer(hpgl_state_t *pgls)
{
	gs_free_object(pgls->memory, pgls->g.label.buffer,
		       "hpgl_destroy_label_buffer");
	pgls->g.label.char_count = 0;
	pgls->g.label.buffer_size = 0;
	pgls->g.label.buffer = 0;
	return 0;
}

/* add a single character to the line buffer */
static int
hpgl_buffer_char(hpgl_state_t *pgls, byte ch)
{
	/* check if there is room for the new character and resize if
           necessary */
	if ( pgls->g.label.buffer_size == pgls->g.label.char_count )
	  { /* Resize the label buffer, currently by doubling its size. */
	    uint new_size = pgls->g.label.buffer_size << 1;
	    byte *new_mem =
	      gs_resize_object(pgls->memory, pgls->g.label.buffer, new_size,
			       "hpgl_resize_label_buffer");

	    if ( new_mem == 0 )
	      return_error(e_Memory);
	    pgls->g.label.buffer = new_mem;
	    pgls->g.label.buffer_size = new_size;
	  }
	/* store the character */
	pgls->g.label.buffer[pgls->g.label.char_count++] = ch;
	return 0;
}

/*
 * Test if the gl/2 drawing primitives should draw the character or
 * "show" can be used directly.
 */
static bool
hpgl_use_show(hpgl_state_t *pgls, pl_font_t *pfont)
{

    /* NB opaque source must go through the gl/2 line drawing code and
       treated as a special effect because the normal text processing
       does not support rendering the background of characters.
       Bitmap characters must be rendered with the library's "show"
       machinery, so opaque bitmaps are not properly supported in
       gl/2.  Fortunately we don't expect to see them in the real
       world */
    if (pgls->g.source_transparent == 0 && pfont->scaling_technology != plfst_bitmap)
        return false;
        
    /* Show cannot be used if CF is not default since the character
       may require additional processing by the line drawing code. */
    if ( (pgls->g.character.fill_mode == 0 && pgls->g.character.edge_pen == 0) ||
         (pfont->scaling_technology == plfst_bitmap) )
	return true;
    else
	return false;
}
 
/* get the scaling factors for a gl/2 character */
gs_point
hpgl_current_char_scale(const hpgl_state_t *pgls)
{
    const pcl_font_selection_t *pfs =
        &pgls->g.font_selection[pgls->g.font_selected];
    pl_font_t *font = pfs->font;
    bool bitmaps_allowed = pgls->g.bitmap_fonts_allowed;

    gs_point scale;
    
    if (pgls->g.character.size_mode == hpgl_size_not_set || font->scaling_technology == plfst_bitmap) {
        if (font->scaling_technology == plfst_bitmap) {
             scale.x = inches_2_plu(1.0 / font->resolution.x);
             scale.y = -inches_2_plu(1.0 / font->resolution.y);
        /* Scale fixed-width fonts by pitch, variable-width by height. */
        } else if (pfs->params.proportional_spacing) {
            if (pl_font_is_scalable(font)) {
                scale.x = hpgl_points_2_plu(pgls, pfs->params.height_4ths / 4.0);
                scale.y = scale.x;
            } else {
                double  ratio = (double)pfs->params.height_4ths
                    / font->params.height_4ths;

                scale.x = ratio * inches_2_plu(1.0 / font->resolution.x);

                /*
                 * Bitmap fonts use the PCL coordinate system,
                 * so we must invert the Y coordinate.
                 */
                scale.y = -(ratio * inches_2_plu(1.0 / font->resolution.y));
            }
        } else {
#define PERCENT_OF_EM ((pl_fp_pitch_cp(&pfs->font->params) / 1000.0))
            scale.x = scale.y = (1.0/PERCENT_OF_EM) * hpgl_points_2_plu(pgls, pl_fp_pitch_cp(&pfs->params) / 100.0);
        }
    } else {
        /*
         * Note that the CTM takes P1/P2 into account unless
         * an absolute character size is in effect.
         */
        /* HP is really scaling the cap height not the point size.
           We assume point size is 1.5 times the point size */
        scale.x = pgls->g.character.size.x * 1.5 * 1.25;
        scale.y = pgls->g.character.size.y * 1.5;
        if (pgls->g.character.size_mode == hpgl_size_relative)
            scale.x *= pgls->g.P2.x - pgls->g.P1.x,
		scale.y *= pgls->g.P2.y - pgls->g.P1.y;
        if (bitmaps_allowed)    /* no mirroring */
            scale.x = fabs(scale.x), scale.y = fabs(scale.y);
    }
    return scale;

}
/*
 * build the path and render it
 */
static int
hpgl_print_char(
    hpgl_state_t *                  pgls,
    uint                            ch
)
{
    int                             text_path = pgls->g.character.text_path;
    const pcl_font_selection_t *    pfs =
	                      &pgls->g.font_selection[pgls->g.font_selected];
    pl_font_t *               font = pfs->font;
    gs_state *                      pgs = pgls->pgs;
    gs_matrix                       save_ctm;
    gs_font *       pfont = pgls->g.font->pfont;
    gs_point scale = hpgl_current_char_scale(pgls);
    /*
     * All character data is relative, but we have to start at
     * the right place.
     */
    hpgl_call( hpgl_add_point_to_path( pgls,
                                       pgls->g.pos.x,
                                       pgls->g.pos.y,
				       hpgl_plot_move_absolute,
                                       true
                                       ) );
    hpgl_call(gs_currentmatrix(pgs, &save_ctm));

    /*
     * Use plotter unit ctm.
     */
    hpgl_call(hpgl_set_plu_ctm(pgls));

    /* ?? WRONG and UGLY */
    {
        float metrics[4];
        if ( (pl_font_char_metrics(font, (void *)(pgls->pgs), 
                                   hpgl_map_symbol(ch, pgls), metrics)) == 1)
            ch = ' ';
    }

	/*
	 * If we're using a stroked font, patch the pen width to reflect
	 * the stroke weight.  Note that when the font's build_char
	 * procedure calls stroke, the CTM is still scaled.
	 ****** WHAT IF scale.x != scale.y? ****** this should be unnecessary.
	 */

    if (pfont->PaintType != 0) {
        const float *   widths = pcl_palette_get_pen_widths(pgls->ppalet);
        float           save_width = widths[hpgl_get_selected_pen(pgls)];
        int             weight = pfs->params.stroke_weight;
        floatp  nwidth;

        if (weight == 9999)
            nwidth = save_width;
        else {
            nwidth = 0.06 + weight * (weight < 0 ? 0.005 : 0.010);
            nwidth *= min(scale.x, scale.y) * (hpgl_width_scale(pgls));
        }
        /* in points */
        gs_setlinewidth(pgs, nwidth * (72.0/1016.0));
    }


    /*
     * We know that the drawing machinery only sets the CTM
     * once, at the beginning of the path.  We now impose the scale
     * (and other transformations) on the CTM so that we can add
     * the symbol outline based on a 1x1-unit cell.
     */
    {
        gs_font *       pfont = pgls->g.font->pfont;
        bool            bitmaps_allowed = pgls->g.bitmap_fonts_allowed;
        bool            use_show = hpgl_use_show(pgls, font);
        gs_matrix       pre_rmat, rmat, advance_mat;
        int             angle = -1;	/* a multiple of 90 if used */
	gs_text_enum_t *penum;
        byte            str[2];
        int             code;
        gs_point        start_pt, end_pt;
	hpgl_real_t     space_width;
	int             space_code;
	hpgl_real_t     width;

	/* Handle size. */

	gs_scale(pgs, scale.x, scale.y);
	/* Handle rotation. */
	{
            double  run = pgls->g.character.direction.x,
	            rise = pgls->g.character.direction.y;

	    if (pgls->g.character.direction_relative)
	        run *= pgls->g.P2.x - pgls->g.P1.x,
		rise *= pgls->g.P2.y - pgls->g.P1.y;
	    gs_make_identity(&rmat);
	    if ((run < 0) || (rise != 0)) {
                double  denom = hypot(run, rise);

		rmat.xx = run / denom;
		rmat.xy = rise / denom;
		rmat.yx = -rmat.xy;
		rmat.yy = rmat.xx;
		if ( bitmaps_allowed &&
		     (run != 0)      &&
                     (rise != 0)       ) {  /* not a multple of 90 degrees */
		    /*
		     * If bitmap fonts are allowed, rotate to the nearest
		     * multiple of 90 degrees.  We have to do something
		     * special at the end to create the correct escapement.
		     */
		    gs_currentmatrix(pgs, &pre_rmat);
		    if (run >= 0) {
		        if (rise >= 0)
			    angle = (run >= rise ? 0 : 90);
		        else
			    angle = (-rise >= run ? 270 : 0);
		    } else {
		        if (rise >= 0)
			    angle = (rise >= -run ? 90 : 180);
		        else
			    angle = (-run >= -rise ? 180 : 270);
		    }
		}
		gs_concat(pgs, &rmat);
	    }
        }

	/* Handle slant. */
	if (pgls->g.character.slant && !bitmaps_allowed) {
            gs_matrix   smat;

	    gs_make_identity(&smat);
	    smat.yx = pgls->g.character.slant;
	    gs_concat(pgs, &smat);
	}

	gs_setfont(pgs, pfont);
        pfont->FontMatrix = pfont->orig_FontMatrix;

	/*
	 * Adjust the initial position of the character according to
	 * the text path.  And the left side bearing.  It appears
	 * HPGL/2 renders all characters without a left side bearing.  
	 */
	hpgl_call(gs_currentpoint(pgs, &start_pt));
	if (text_path == hpgl_text_left) {
            hpgl_get_char_width(pgls, ch, &width);
	    start_pt.x -= width / scale.x;
	    hpgl_call(hpgl_add_point_to_path(pgls, start_pt.x, start_pt.y,
					     hpgl_plot_move_absolute, false));

	}

	/*
	 * Reset the rotation if we're using a bitmap font.
	 */
	gs_currentmatrix(pgs, &advance_mat);
	if (angle >= 0) {
            gs_setmatrix(pgs, &pre_rmat);
	    gs_rotate(pgs, (floatp)angle);
	}

	str[0] = ch;
	str[1] = 0;

	/* If SP is a control code, get the width of the space character. */
	if (ch == ' ') {
            space_code = hpgl_get_char_width(pgls, ' ', &space_width);
            
	    if ( 0 == space_code && 
		 pl_font_is_scalable(font) && 
		 pfs->params.proportional_spacing )
		space_code = 1; /* NB hpgl_get_width lies. */

	    if (space_code == 1) {
                /* Space is a control code. */
		if ( pl_font_is_scalable(font) ) {
		    if (pfs->params.proportional_spacing)
			space_width = 
			    (coord_2_plu((pl_fp_pitch_cp(&pfs->font->params) / 10)
					 * pfs->params.height_4ths / 4) ) / scale.x;
		    else
			space_width = 1.0; 
		        /* error! NB scalable fixed pitch space_code == 0 */
		} else
		    space_width =
                        coord_2_plu(pl_fp_pitch_cp(&pfs->font->params)) / scale.x;
		space_width *= (1.0 + hpgl_get_character_extra_space_x(pgls));
            }
	}

	/* Check for SP control code. */
	if (ch == ' ' && space_code != 0) {
            /* Space is a control code.  Just advance the position. */
	    gs_setmatrix(pgs, &advance_mat);
	    hpgl_call(hpgl_add_point_to_path(pgls, space_width, 0.0,
					     hpgl_plot_move_relative, false));
	    hpgl_call(gs_currentpoint(pgs, &end_pt));
            /* at this point we will assume the page is marked */
            pgls->page_marked = true;
	} else {
            gs_text_params_t text;
            gs_char mychar_buff[1];
            mychar_buff[0] = hpgl_map_symbol(ch, pgls);
            if (use_show) {
                /* not a path that needs to be drawn by the hpgl/2
                   vector drawing code. */
		hpgl_call(hpgl_set_drawing_color(pgls, hpgl_rm_character));
                text.operation = TEXT_FROM_CHARS | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
	    } else 
                text.operation = TEXT_FROM_CHARS | TEXT_DO_TRUE_CHARPATH | TEXT_RETURN_WIDTH;
            text.data.chars = mychar_buff;
            /* always on char (gs_chars (ints)) at a time */
            text.size = 1;
            code = gs_text_begin(pgs, &text, pgls->memory, &penum);
	    if ( code >= 0 )
		code = gs_text_process(penum);

            if ( code >= 0 ) {
                /* we just check the current position for
                   "insidedness" - this seems to address the dirty
                   page issue in practice. */
                pcl_mark_page_for_current_pos(pgls);
                gs_text_release(penum, "hpgl_print_char");
            }
	    if ( code < 0 )
		return code;
	    gs_setmatrix(pgs, &advance_mat);
	    if (angle >= 0) {
                /* Compensate for bitmap font non-rotation. */
		if (text_path == hpgl_text_right) {
                    hpgl_get_char_width(pgls, ch, &width);
		    hpgl_call(hpgl_add_point_to_path(pgls, start_pt.x + width / scale.x,
						      start_pt.y, hpgl_plot_move_absolute, false));
		}
            }
	    hpgl_call(gs_currentpoint(pgs, &end_pt));
	    if ( start_pt.x == end_pt.x && start_pt.y == end_pt.y ) { 
		/* freetype doesn't move currentpoint in gs_show(),
		 * since gs cache is not used.  NB we don't support
		 * freetype anymore is this necessary?
		 */
		hpgl_get_char_width(pgls, ch, &width);
		hpgl_call(hpgl_add_point_to_path(pgls,  width / scale.x, 0.0,
						 hpgl_plot_move_relative, false));
		hpgl_call(gs_currentpoint(pgs, &end_pt));
	    }
	    if ( (text_path == hpgl_text_right)        &&
		 (hpgl_get_character_extra_space_x(pgls) != 0)  ) {
		hpgl_get_char_width(pgls, ch, &width);
		end_pt.x = start_pt.x + width / scale.x;
		hpgl_call(hpgl_add_point_to_path(pgls, end_pt.x, end_pt.y, hpgl_plot_move_absolute, false));
	    }
	}
	/*
	 * Adjust the final position according to the text path.
	 */
	switch (text_path) {
	  case hpgl_text_right:
	    break;

	  case hpgl_text_down:
	    {
                hpgl_real_t     height;

	        hpgl_call(hpgl_get_current_cell_height(pgls, &height));
                hpgl_call( hpgl_add_point_to_path(pgls, start_pt.x, end_pt.y - height / scale.y,
						  hpgl_plot_move_absolute, false) );

	    }
	    break;

	  case hpgl_text_left:
	      hpgl_call(hpgl_add_point_to_path(pgls, start_pt.x, start_pt.y, 
					       hpgl_plot_move_absolute, false));
	      break;
	  case hpgl_text_up:
	    {
                hpgl_real_t height;

	        hpgl_call(hpgl_get_current_cell_height(pgls, &height));
		hpgl_call(hpgl_add_point_to_path(pgls, start_pt.x, 
						 end_pt.y + height / scale.y, hpgl_plot_move_absolute, false));
	    }
	    break;
	}

	gs_setmatrix(pgs, &save_ctm);
	hpgl_call(gs_currentpoint(pgs, &end_pt));
	if (!use_show)
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_character));

	hpgl_call( hpgl_add_point_to_path( pgls,
                                           end_pt.x,
                                           end_pt.y,
					   hpgl_plot_move_absolute,
                                           true
                                           ) );
    }

    return 0;
}

/* Determine whether labels can concatenate. */
/* Note that LO requires a pre-scan iff this is false. */
static bool
hpgl_can_concat_labels(const hpgl_state_t *pgls)
{	/* The following is per TRM 23-78. */
	static const byte can_concat[22] = {
	  0, 9, 1, 3, 8, 0, 2, 12, 4, 6,
	  0, 9, 1, 3, 8, 0, 2, 12, 4, 6,
	  0, 9
	};

	return (can_concat[pgls->g.label.origin] &
		(1 << pgls->g.character.text_path)) != 0;
}
 

/* return relative coordinates to compensate for origin placement -- LO */
static int
hpgl_get_character_origin_offset(hpgl_state_t *pgls, int origin,
				 hpgl_real_t width, hpgl_real_t height,
				 gs_point *offset)
{
    double pos_x = 0.0, pos_y = 0.0;
    double off_x, off_y;
    hpgl_real_t adjusted_height = height;

#ifdef CHECK_UNIMPLEMENTED
    if (pgls->g.character.extra_space.x != 0 || pgls->g.character.extra_space.y != 0)
        dprintf("warning origin offset with non zero extra space not supported\n");
#endif
    adjusted_height /= 1.6;
    if (hpgl_is_currentfont_stick(pgls))
        adjusted_height /= 1.4;
    
    /* offset values specified by the documentation */
    if ( (origin >= 11 && origin <= 14) || (origin >= 16 && origin <= 19) ) {
        if (hpgl_is_currentfont_stick(pgls)) {
            off_x = off_y = 0.33 * adjusted_height;
        } else {
            /* the documentation says this should be .25, experiments
               indicate .33 like stick fonts. */
            off_x = off_y = 0.33 * adjusted_height;
        }
    }

    switch ( origin ) {
    case 11:
        pos_x = -off_x;
        pos_y = -off_y;
    case 1:
        break;
    case 12:
        pos_x = -off_x;
    case 2:
        pos_y = .5 * adjusted_height;
        break;
    case 13:
        pos_x = -off_x;
        pos_y = off_y;
    case 3:
        pos_y += adjusted_height;
        break;
    case 14:
        pos_y = -off_y;
    case 4:
        pos_x = .5 * width;
        break;
    case 15:
    case 5:
        pos_x = .5 * width;
        pos_y = .5 * adjusted_height;
        break;
    case 16:
        pos_y = off_y;
    case 6:
        pos_x = .5 * width;
        pos_y += adjusted_height;
        break;
    case 17:
        pos_x = off_x;
        pos_y = -off_y;
    case 7:
        pos_x += width;
        break;
    case 18:
        pos_x = off_x;
    case 8:
        pos_x += width;
        pos_y = .5 * adjusted_height;
        break;
    case 19:
        pos_x = off_x;
        pos_y = off_y;
    case 9:
        pos_x += width;
        pos_y += adjusted_height;
        break;
    case 21:
        {
            /* // LO21 prints at the current position  not pcl CAP.
               gs_matrix save_ctm;
               gs_point pcl_pos_dev, label_origin;

               gs_currentmatrix(pgls->pgs, &save_ctm);
               pcl_set_ctm(pgls, false);
               hpgl_call(gs_transform(pgls->pgs, (floatp)pgls->cap.x,
               (floatp)pgls->cap.y, &pcl_pos_dev));
               gs_setmatrix(pgls->pgs, &save_ctm);
               hpgl_call(gs_itransform(pgls->pgs, (floatp)pcl_pos_dev.x,
               (floatp)pcl_pos_dev.y, &label_origin));
               pos_x = -(pgls->g.pos.x - label_origin.x);
               pos_y = (pgls->g.pos.y - label_origin.y);
            */
        }
        break;
    default:
        dprintf("unknown label parameter");

    }
    /* a relative move to the new position */
    offset->x = pos_x;
    offset->y = pos_y;

    /* account for character direction and slant */
    hpgl_rotation_transform_distance(pgls, offset, offset);
    hpgl_slant_transform_distance(pgls, offset, offset);

    switch ( pgls->g.character.text_path ) {
    case hpgl_text_left:
        offset->x -= width;
        break;
    case hpgl_text_down:
        offset->y -= height;
        break;
    default:
        DO_NOTHING;
    }

    {  
        gs_matrix mat;
        hpgl_compute_user_units_to_plu_ctm(pgls, &mat);
        offset->x /= mat.xx;
        offset->y /= mat.yy;
    }
    /* convert to user units */
    return 0;
}

static gs_char
hpgl_next_char(hpgl_state_t *pgls, byte **ppb)
{
    byte *pb = *ppb;
    gs_char chr = *pb++;
    
    if (pgls->g.label.double_byte)
        chr = (chr << 8) + *pb++;
    *ppb = pb;
    return chr;
}

/* Prints a buffered line of characters. */
/* If there is a CR, it is the last character in the buffer. */
static int
hpgl_process_buffer(hpgl_state_t *pgls, gs_point *offset)
{
    hpgl_real_t label_length = 0.0, label_height = 0.0;
    bool vertical = hpgl_text_is_vertical(pgls->g.character.text_path);
    int i, inc;

    /* a peculiar side effect of LABEL parsing double byte characters
       is it leaves an extra byte in the buffer - fix that now, and
       properly set the increment for the parallel loops below. */
    if ( pgls->g.label.double_byte ) {
        pgls->g.label.char_count--;
        inc = 2;
    } else {
        inc = 1;
    }
    

    /*
     * NOTE: the two loops below must be consistent with each other!
     */

    {
        hpgl_real_t width = 0.0, height = 0.0;
        int save_index = pgls->g.font_selected;
        bool first_char_on_line = true;
        byte *b = pgls->g.label.buffer;

        for ( i=0; i < pgls->g.label.char_count; i+=inc ) {
            gs_char ch = hpgl_next_char(pgls, &b);
            if ( ch < 0x20 && !pgls->g.transparent_data )
		switch (ch) {
		case BS :
                    if ( width == 0.0 ) { /* BS as first char of string */
                        hpgl_call(hpgl_ensure_font(pgls));
                        hpgl_get_char_width(pgls, ' ', &width);
                        hpgl_call(hpgl_get_current_cell_height(pgls, &height));
		    }
                    if ( vertical ) { /* Vertical text path, back up in Y. */
                        label_height -= height;
                        if ( label_height < 0.0 )
                            label_height = 0.0;
		    } else { /* Horizontal text path, back up in X. */
                        label_length -= width;
                        if ( label_length < 0.0 )
                            label_length = 0.0;
                    }
                    continue;
		case LF :
		    first_char_on_line = true;
		    continue;
		case CR :
                    continue;
		case FF :
                    continue;
		case HT :
                    hpgl_call(hpgl_ensure_font(pgls));
                    hpgl_get_char_width(pgls, ' ', &width);
                    width *= 5;
                    goto acc_ht;
		case SI :
                    hpgl_select_font_pri_alt(pgls, 0);
                    continue;
		case SO :
                    hpgl_select_font_pri_alt(pgls, 1);
                    continue;
		default :
                    break;
		}
            hpgl_call(hpgl_ensure_font(pgls));
            hpgl_get_char_width(pgls, ch, &width);
acc_ht:	    hpgl_call(hpgl_get_current_cell_height(pgls, &height));
            if ( vertical ) {
                if ( width > label_length )
                    label_length = width;
                if ( !first_char_on_line )
                    label_height += height;
                else
                    first_char_on_line = false;
            } else { /* Horizontal text path: sum widths, take max of heights. */
                label_length += width;
                if ( height > label_height )
		    label_height = height;
            }
        }
        hpgl_select_font_pri_alt(pgls, save_index);
    }
    hpgl_call(hpgl_get_character_origin_offset(pgls, pgls->g.label.origin,
                                               label_length, label_height,
                                               offset));

    /* now add the offsets in relative plu coordinates */
    hpgl_call(hpgl_add_point_to_path(pgls, -offset->x, -offset->y,
                                     hpgl_plot_move_relative, false));

    {
        byte *b = pgls->g.label.buffer;
        for ( i = 0; i < pgls->g.label.char_count; i+=inc ) { 
            gs_char ch = hpgl_next_char(pgls, &b);
            if ( ch < 0x20 && !pgls->g.transparent_data ) { 
                hpgl_real_t spaces, lines;

                switch (ch) {
                case BS :
                    spaces = -1, lines = 0;
                    break;
                case LF :
                    /*
                     * If the text path is vertical, we must use the
                     * computed label (horizontal) width, not the width
                     * of a space.
                     */
                    if ( vertical ) { 
                        const pcl_font_selection_t *    pfs =
                            &pgls->g.font_selection[pgls->g.font_selected];
                        hpgl_real_t label_advance;
                        /* Handle size. */
                        if (pgls->g.character.size_mode == hpgl_size_not_set) {
                            /* Scale fixed-width fonts by pitch, variable-width by height. */
                            if (pfs->params.proportional_spacing) {
                                if (pl_font_is_scalable(pfs->font))
                                    label_advance = hpgl_points_2_plu(pgls, pfs->params.height_4ths / 4.0);
                                else {
                                    double  ratio = (double)pfs->params.height_4ths
                                        / pfs->font->params.height_4ths;
                                    label_advance = ratio * inches_2_plu(1.0 / pfs->font->resolution.x);
                                }
                            } else
                                /*** WRONG ****/
                                label_advance = hpgl_points_2_plu(pgls, pl_fp_pitch_cp(&pfs->params) /
                                                                  ((pl_fp_pitch_cp(&pfs->font->params)/10.0)));
                            if ( hpgl_get_character_extra_space_x(pgls) != 0 ) 
                                label_advance *= 1.0 + hpgl_get_character_extra_space_x(pgls);
                        } else {
                            /*
                             * Note that the CTM takes P1/P2 into account unless
                             * an absolute character size is in effect.
                             *
                             *
                             * HACKS - I am not sure what this should be the
                             * actual values ??? 
                             */
                            label_advance = pgls->g.character.size.x * 1.5 * 1.25;
                            if (pgls->g.character.size_mode == hpgl_size_relative)
                                label_advance *= pgls->g.P2.x - pgls->g.P1.x;
                            if (pgls->g.bitmap_fonts_allowed)    /* no mirroring */
                                label_advance = fabs(label_advance);
                        }
                        hpgl_move_cursor_by_characters(pgls, 0, -1,
                                                       &label_advance);
                        continue;
                    }
                    spaces = 0, lines = -1;
                    break;
                case CR :
                    hpgl_call(hpgl_do_CR(pgls));
                    continue;
                case FF :
                    /* does nothing */
                    spaces = 0, lines = 0;
                    break;
                case HT :
                    /* appears to expand to 5 spaces */
                    spaces = 5, lines = 0;
                    break;
                case SI :
                    hpgl_select_font_pri_alt(pgls, 0);
                    continue;
                case SO :
                    hpgl_select_font_pri_alt(pgls, 1);
                    continue;
                default :
                    goto print;
                }
                hpgl_move_cursor_by_characters(pgls, spaces, lines,
                                               (const hpgl_real_t *)0);
                continue;
            }
print:	     {
                /* if this a printable character print it
                   otherwise continue, a character can be
                   printable and undefined in which case
                   it is printed as a space character */
                const pcl_font_selection_t *pfs =
                    &pgls->g.font_selection[pgls->g.font_selected];
                if ( !hpgl_is_printable(pfs->map, ch, 
                                        (pfs->params.typeface_family & 0xfff) == STICK_FONT_TYPEFACE,
                                        pgls->g.transparent_data) )
                    continue;
            }
            hpgl_call(hpgl_ensure_font(pgls));
            hpgl_call(hpgl_print_char(pgls, ch));
        }
    }
    pgls->g.label.char_count = 0;
    return 0;
}


/**
 *  used by hpgl_LB() to find the end of a label
 *  
 * 8bit and 16bit label terminator check
 *  (prev << 8) & curr -> 16 bit
 *  have_16bits allows per byte call with true on 16bit boundary.
 */ 
static 
bool is_terminator( hpgl_state_t *pgls, byte prev, byte curr, bool have_16bits )
{
    return 
	pgls->g.label.double_byte ? 
	( have_16bits && prev == 0 && curr == pgls->g.label.terminator ) :
	( curr == pgls->g.label.terminator );
}  

#define GL_LB_HAVE_16BITS (pgls->g.label.have_16bits)
#define GL_LB_CH (pgls->g.label.ch)
#define GL_LB_PREV_CH (pgls->g.label.prev_ch)

/* LB ..text..terminator */
int
hpgl_LB(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;
	bool print_terminator = pgls->g.label.print_terminator;

	if ( pargs->phase == 0 )
	  {
	    /* initialize the character buffer and CTM first time only */
 	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	    hpgl_call(hpgl_init_label_buffer(pgls));
	    hpgl_call(hpgl_set_ctm(pgls));
	    hpgl_call(hpgl_set_clipping_region(pgls, hpgl_rm_vector));
	    pgls->g.label.initial_pos = pgls->g.pos;
            GL_LB_HAVE_16BITS = true;
            GL_LB_CH = 0xff;
            GL_LB_PREV_CH = 0xff;     /* for two byte terminators */ 
	    pargs->phase = 1;
	  }

	while ( p < rlimit )
	  { 
            /* This is not ugly and unintuitive */
	    GL_LB_HAVE_16BITS = !GL_LB_HAVE_16BITS;
	    GL_LB_PREV_CH = GL_LB_CH;
	    GL_LB_CH = *++p;
	    if_debug1('I',
		      (GL_LB_CH == '\\' ? " \\%c" : GL_LB_CH >= 33 && GL_LB_CH <= 126 ? " %c" :
		       " \\%03o"),
		      GL_LB_CH);
	    if ( is_terminator(pgls, GL_LB_PREV_CH, GL_LB_CH, GL_LB_HAVE_16BITS) )
	      {
		if ( !print_terminator )
		  {
                    gs_point lo_offsets;
		    hpgl_call(hpgl_process_buffer(pgls, &lo_offsets));
		    hpgl_call(hpgl_destroy_label_buffer(pgls));
		    pargs->source.ptr = p;
		    /*
		     * Depending on the DV/LO combination, conditionally
		     * restore the initial position, per TRM 23-78.
		     */
		    if ( !hpgl_can_concat_labels(pgls) )  { 
			  hpgl_call(hpgl_add_point_to_path(pgls,
					pgls->g.carriage_return_pos.x,
					pgls->g.carriage_return_pos.y,
					hpgl_plot_move_absolute, true));
                    } else {
                        /* undo the label origin offsets */
                        hpgl_call(hpgl_add_point_to_path(pgls, lo_offsets.x, lo_offsets.y,
                                                         hpgl_plot_move_relative, false));
                    }
                    /* clear the current path terminating carriage
                       returns and linefeeds will leave "moveto's" in
                       the path */
                    hpgl_call(hpgl_clear_current_path(pgls));
                    /* also clean up stick fonts - they are likely to
                       become dangling references in the current font
                       scheme since they don't have a dictionary entry */
                    hpgl_free_stick_fonts(pgls);
                    return 0;
                  }
		/*
		 * Process the character in the ordinary way, then come here
		 * again.  We do things this way to simplify the case where
		 * the terminator is a control character.
		 */
		--p;
		print_terminator = false;
	      }

	    /* process the buffer for a carriage return so that we can
               treat the label origin correctly, and initialize a new
               buffer */

	    hpgl_call(hpgl_buffer_char(pgls, GL_LB_CH));
	    if ( GL_LB_CH == CR && !pgls->g.transparent_data )
            {
                gs_point lo_offsets;
		hpgl_call(hpgl_process_buffer(pgls, &lo_offsets));
		hpgl_call(hpgl_destroy_label_buffer(pgls));
		hpgl_call(hpgl_init_label_buffer(pgls));
	      }
	  }
	pargs->source.ptr = p;
	return e_NeedData;
}

void
hpgl_free_stick_fonts(hpgl_state_t *pgls)
{
    pcl_font_selection_t *pfs =
	&pgls->g.font_selection[pgls->g.font_selected];
    pl_font_t *font = &pgls->g.stick_font[pgls->g.font_selected]
	[pfs->params.proportional_spacing];

    /* no stick fonts - nothing to do */
    if ( font->pfont == 0 )
	return;

    gs_free_object(pgls->memory, font->pfont, "stick/arc font");
    font->pfont = 0;
    return;
}

int
hpgl_print_symbol_mode_char(hpgl_state_t *pgls)
{	
        /* save the original origin since symbol mode character are
           always centered */
	int saved_origin = pgls->g.label.origin;
	gs_point save_pos = pgls->g.pos;
        gs_point lo_offsets;
	hpgl_call(hpgl_gsave(pgls));
	/* HAS this need checking.  I don't know how text direction
           and label origin interact in symbol mode */
	pgls->g.label.origin = 5;
	/* HAS - alot of work for one character */
	hpgl_call(hpgl_clear_current_path(pgls));
	hpgl_call(hpgl_init_label_buffer(pgls));
	hpgl_call(hpgl_buffer_char(pgls, pgls->g.symbol_mode));
	hpgl_call(hpgl_process_buffer(pgls, &lo_offsets));
	hpgl_call(hpgl_destroy_label_buffer(pgls));
	hpgl_call(hpgl_grestore(pgls));
	/* restore the origin */
	pgls->g.label.origin = saved_origin;
	hpgl_call(hpgl_set_current_position(pgls, &save_pos));
        hpgl_free_stick_fonts(pgls);
	return 0;
}

/* Initialization */
static int
pglabel_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{		/* Register commands */
    DEFINE_HPGL_COMMANDS(mem)
	  HPGL_COMMAND('C', 'P', hpgl_CP, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
	  /* LB also has special argument parsing. */
	  HPGL_COMMAND('L', 'B', hpgl_LB, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
	END_HPGL_COMMANDS
	return 0;
}

const pcl_init_t pglabel_init = {
  pglabel_do_registration, 0
};
