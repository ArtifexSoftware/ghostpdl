/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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

/* ------ Next-character procedure ------ */

/*
 * Parse one character from a string.  Return 2 if the string is exhausted,
 * 0 if a character was parsed, -1 if the string ended in the middle of
 * a double-byte character.
 */
private int
hpgl_next_char(gs_show_enum *penum, const hpgl_state_t *pgls, gs_char *pchr)
{	uint i = penum->index;
	uint size = penum->text.size;
	const byte *p;

	if ( i >= size )
	  return 2;
	p = penum->text.data.bytes;
	if ( pgls->g.label.double_byte ) {
	  if ( i + 1 >= size )
	    return -1;
	  *pchr = (*p << 8) + p[1];
	  penum->index = i + 2;
	} else {
	  *pchr = *p + pgls->g.label.row_offset;
	  penum->index = i + 1;
	}
	return 0;
}

/*
 * Map a character through the symbol set, if needed.
 */
private uint
hpgl_map_symbol(uint chr, const hpgl_state_t *pgls)
{	const pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];
	const pl_symbol_map_t *psm = pfs->map;

	if ( psm == 0 )
	  { /* Hack: bound TrueType fonts are indexed from 0xf000. */
	    if ( pfs->font->scaling_technology == plfst_TrueType )
	      return chr + 0xf000;
	    return chr;
	  }
	{ uint first_code = pl_get_uint16(psm->first_code);
	  uint last_code = pl_get_uint16(psm->last_code);

	  /*
	   * If chr is double-byte but the symbol map is only
	   * single-byte, just return chr.
	   */
	  if ( chr < first_code || chr > last_code )
	    return (last_code <= 0xff && chr > 0xff ? chr : 0xffff);
	  else
	    return psm->codes[chr - first_code];
	}
}

/* Next-character procedure for fonts in GL/2 mode. NB.  this need to
   be reworked. */
private int
hpgl_next_char_proc(gs_show_enum *penum, gs_char *pchr)
{	const pcl_state_t *pcs = gs_state_client_data(penum->pgs);
#define pgls pcs		/****** NOTA BENE ******/
	int code = hpgl_next_char(penum, pgls, pchr);
	if ( code )
	  return code;
	*pchr = hpgl_map_symbol(*pchr, pgls);
#undef pgls
	return 0;
}

/* ------ Font selection ------- */

/* Select primary (0) or alternate (1) font. */
#define hpgl_select_font(pgls, i)\
  do {\
    if ( (pgls)->g.font_selected != (i) )\
      (pgls)->g.font_selected = (i), (pgls)->g.font = 0;\
  } while (0)

/* Ensure a font is available. */
#define hpgl_ensure_font(pgls)\
  do {\
    if ( (pgls)->g.font == 0 )\
      hpgl_call(hpgl_recompute_font(pgls));\
  } while (0)

/*
 * The character complement for the stick font is puzzling: it doesn't seem
 * to correspond directly to any of the MSL *or* Unicode symbol set bits
 * described in the Comparison Guide.  We set the bits for MSL Basic Latin
 * (63) and for Unicode ASCII (31), and Latin 1 (30).
 */
private const byte stick_character_complement[8] = {
  0x7f, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xfe
};

/* Select the stick font, creating it if necessary. */
/* We break this out only for readability: it's only called in one place. */
private int
hpgl_select_stick_font(hpgl_state_t *pgls)
{	pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];
	pl_font_t *font = &pgls->g.stick_font[pgls->g.font_selected]
	  [pfs->params.proportional_spacing];

	/* Create a gs_font if none has been created yet. */
	if ( font->pfont == 0 )
	  { gs_font_base *pfont =
	      gs_alloc_struct(pgls->memory, gs_font_base, &st_gs_font_base,
			      "stick/arc font");
	    int code;

	    if ( pfont == 0 )
	      return_error(e_Memory);
	    code = pl_fill_in_font((gs_font *)pfont, font, pgls->font_dir,
				   pgls->memory);
	    if ( code < 0 )
	      return code;
	    if ( pfs->params.proportional_spacing )
	      hpgl_fill_in_arc_font(pfont, gs_next_ids(1));
	    else
	      hpgl_fill_in_stick_font(pfont, gs_next_ids(1));
	    font->pfont = (gs_font *)pfont;
	    font->scaling_technology = plfst_TrueType;/****** WRONG ******/
	    font->font_type = plft_Unicode;
	    memcpy(font->character_complement, stick_character_complement, 8);
	  }
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
	pl_fp_set_pitch_cp(&font->params, 100.0*2/3);
	pfs->font = font;
	{ byte id[2];

	  id[0] = pfs->params.symbol_set >> 8;
	  id[1] = pfs->params.symbol_set & 255;
	  pfs->map = pcl_find_symbol_map(pgls /****** NOTA BENE ******/,
					 id, plgv_Unicode);
	}
	return 0;
}

/* Check whether the stick font supports a given symbol set. */
private bool
hpgl_stick_font_supports(const pcl_state_t *pcs, uint symbol_set)
{	pl_glyph_vocabulary_t gv = (pl_glyph_vocabulary_t)
                                   (~stick_character_complement[7] & 07);
	byte id[2];
	pl_symbol_map_t *map;

	id[0] = symbol_set >> 8;
	id[1] = symbol_set;
	if ( (map = pcl_find_symbol_map(pcs, id, gv)) == 0 )
	  return false;
	return pcl_check_symbol_support(map->character_requirements,
					stick_character_complement);
}

/* Recompute the current font if necessary. */
private int
hpgl_recompute_font(hpgl_state_t *pgls)
{	pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];

	if ( ((pfs->params.typeface_family & 0xfff) == STICK_FONT_TYPEFACE
	     && pfs->params.style == 0 /* upright */
	     && hpgl_stick_font_supports(pgls /****** NOTA BENE ******/,
					 pfs->params.symbol_set))
	     /* rtl only has stick fonts */
	     || ( pgls->personality == rtl )
	   )
	  hpgl_call(hpgl_select_stick_font(pgls));
	else
	  { int code = pcl_reselect_font(pfs, pgls /****** NOTA BENE ******/);

	    if ( code < 0 )
	      return code;
	  }
	pgls->g.font = pfs->font;
	pgls->g.map = pfs->map;
	return 0;
}

/* ------ Position management ------ */

/* Get a character width in the current font, plus extra space if any. */
/* If the character isn't defined, return 1, otherwise return 0. */
private int
hpgl_get_char_width(const hpgl_state_t *pgls, uint ch, hpgl_real_t *width)
{
    uint glyph = hpgl_map_symbol(ch, pgls);
    const pcl_font_selection_t *pfs =
	&pgls->g.font_selection[pgls->g.font_selected];
    int code = 0;
    gs_point gs_width;
    gs_matrix mat;
    if ( pgls->g.character.size_mode == hpgl_size_not_set ) {
	if ( pfs->params.proportional_spacing ) {
	    gs_make_identity(&mat);
	    code = pl_font_char_width(pfs->font, pfs->map, &mat, glyph,
				      &gs_width);
	    if ( code >= 0 ) {
		*width = gs_width.x * points_2_plu(pfs->params.height_4ths / 4.0);
		goto add;
	    }
	    code = 1;
	}
	*width = points_2_plu(pl_fp_pitch_cp(&pfs->params) / 100.0);
    } else {
	*width = pgls->g.character.size.x;
	if (pgls->g.character.size_mode == hpgl_size_relative)
	    *width *= pgls->g.P2.x - pgls->g.P1.x;

    }
 add:	

    if ( pgls->g.character.extra_space.x != 0 ) {
	/* Add extra space. */
	if ( pfs->params.proportional_spacing && ch != ' ' ) {
	    /* Get the width of the space character. */
	    int scode =
		pl_font_char_width(pfs->font, pfs->map, &mat,
				   hpgl_map_symbol(' ', pgls), &gs_width);
	    hpgl_real_t extra;

	    if ( scode >= 0 )
		extra = gs_width.x * points_2_plu(pfs->params.height_4ths / 4.0);
	    else
		extra = points_2_plu(pl_fp_pitch_cp(&pfs->params) / 100.0);
	    *width += extra * pgls->g.character.extra_space.x;
	} else {
	    /* All characters have the same width, */
	    /* or we're already getting the width of a space. */
	    *width *= 1.0 + pgls->g.character.extra_space.x;
	}
    }
    return code;
}
/* Get the cell height or character height in the current font, */
/* plus extra space if any. */
private int
hpgl_get_current_cell_height(const hpgl_state_t *pgls, hpgl_real_t *height,
  bool cell_height)
{
	const pcl_font_selection_t *pfs =
	  &pgls->g.font_selection[pgls->g.font_selected];

	if ( pgls->g.character.size_mode == hpgl_size_not_set ) {
	    *height =
		(pfs->params.proportional_spacing ?
		 points_2_plu(pfs->params.height_4ths / 4.0) :
		 1.667 * inches_2_plu(pl_fp_pitch_cp(&pfs->params) / 7200.0));
	    if ( !cell_height )
		*height *= 0.75;	/****** HACK ******/
	} else {
	    /* character size is actually the cap height see 23-6 */
	    *height = pgls->g.character.size.y;
	    if (pgls->g.character.size_mode == hpgl_size_relative)
		*height *= pgls->g.P2.y - pgls->g.P1.y;
	}
	*height *= 1.0 + pgls->g.character.extra_space.y;
	return 0;
}

/* distance tranformation for character slant */
 private int
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
 private int
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
private int
hpgl_move_cursor_by_characters(hpgl_state_t *pgls, hpgl_real_t spaces,
  hpgl_real_t lines, const hpgl_real_t *pwidth)
{
    int nx, ny;
    double dx = 0, dy = 0;

    hpgl_ensure_font(pgls);

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
    if ( nx ) {
	hpgl_real_t width;
	if ( pwidth != 0 )
	    width = *pwidth;
	else
	    hpgl_get_char_width(pgls, ' ', &width);
	dx = width * nx;
    }
    if ( ny ) {
	hpgl_real_t height;
	hpgl_call(hpgl_get_current_cell_height(pgls, &height, true));
	dy = height * ny;
	if ( pgls->g.character.size_mode != hpgl_size_not_set )
	    dy *= 2;
    }

    /*
     * We just computed the deltas in user units if characters are
     * using relative sizing, and in PLU otherwise.
     * If scaling is on but characters aren't using relative
     * sizing, we have to convert the deltas to user units.
     */
    if ( pgls->g.scaling_type != hpgl_scaling_none &&
	 pgls->g.character.size_mode != hpgl_size_relative
	 )
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
    return 0;
}

/* Execute a CR for CP or LB. */
private int
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

	if ( hpgl_arg_c_real(pargs, &spaces) )
	  {
	    if ( !hpgl_arg_c_real(pargs, &lines) )
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
 private int
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
 private int
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
 private int
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
 * Test whether we can use show instead of charpath followed by
 * a GL/2 fill.  If so, the library will be able to use its
 * character cache.
 */

private bool
hpgl_use_show(hpgl_state_t *pgls)
{
    /* Show cannot be used if CF is not default since the character
       may require additional processing by the line drawing code. */
    if ( (pgls->g.character.fill_mode == 0) &&
	 (pgls->g.character.edge_pen == 0) )
	return true;
    else
	return false;
}
 
/*
 * build the path and render it
 */
 private int
hpgl_print_char(
    hpgl_state_t *                  pgls,
    uint                            ch
)
{
    int                             text_path = pgls->g.character.text_path;
    const pcl_font_selection_t *    pfs =
	                      &pgls->g.font_selection[pgls->g.font_selected];
    const pl_font_t *               font = pfs->font;
    gs_state *                      pgs = pgls->pgs;
    gs_point                        pos;
    gs_matrix                       save_ctm;

    /* Set up the graphics state. */
    pos = pgls->g.pos;

    /*
     * Reset the 'have path' flag so that the CTM gets reset.
     * This is a hack; doing things properly requires a more subtle
     * approach to handling pen up/down and path construction.
     */
    hpgl_call(hpgl_clear_current_path(pgls));

    /*
     * All character data is relative, but we have to start at
     * the right place.
     */
    hpgl_call( hpgl_add_point_to_path( pgls,
                                       pos.x,
                                       pos.y,
				       hpgl_plot_move_absolute,
                                       true
                                       ) );
    hpgl_call(gs_currentmatrix(pgs, &save_ctm));

    /*
     * Reset the CTM if GL/2 scaling is on but we aren't using
     * relative-size characters (SR).
     */
    hpgl_call(hpgl_set_plu_ctm(pgls));

    /*
     * We know that the drawing machinery only sets the CTM
     * once, at the beginning of the path.  We now impose the scale
     * (and other transformations) on the CTM so that we can add
     * the symbol outline based on a 1x1-unit cell.
     */
    {
        gs_font *       pfont = pgls->g.font->pfont;
        const float *   widths = pcl_palette_get_pen_widths(pgls->ppalet);
        float           save_width = widths[hpgl_get_selected_pen(pgls)];
        bool            save_relative = pgls->g.pen.width_relative;
        gs_point        scale;
        bool            bitmaps_allowed = pgls->g.bitmap_fonts_allowed;
        bool            use_show = hpgl_use_show(pgls);
        gs_matrix       pre_rmat, rmat, advance_mat;
        int             angle = -1;	/* a multiple of 90 if used */
        int             weight = pfs->params.stroke_weight;
	gs_show_enum *  penum = gs_show_enum_alloc( pgls->memory,
                                                    pgs,
                                                    "hpgl_print_char"
                                                    );
        byte            str[1];
        int             code;
        gs_point        start_pt, end_pt;
	hpgl_real_t     space_width;
	int             space_code;
	hpgl_real_t     width;

	if (penum == 0)
	    return_error(e_Memory);

	/* Handle size. */
	if (pgls->g.character.size_mode == hpgl_size_not_set) {

            /* Scale fixed-width fonts by pitch, variable-width by height. */
	    if (pfs->params.proportional_spacing) {
	        if (pl_font_is_scalable(font)) {
		    scale.x = points_2_plu(pfs->params.height_4ths / 4.0);
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
		scale.x = points_2_plu( pl_fp_pitch_cp(&pfs->params)
                                        / pl_fp_pitch_cp(&font->params) );
                scale.y = scale.x;
	    }

        } else {
            /*
	     * Note that the CTM takes P1/P2 into account unless
	     * an absolute character size is in effect.
	     */
	    /*
             * HACKS - I am not sure what this should be the
             * actual values ??? 
             */
	    scale.x = pgls->g.character.size.x * 3.0/2.0 * 3.0/2.0;
	    scale.y = pgls->g.character.size.y * 3.0/2.0;
	    if (pgls->g.character.size_mode == hpgl_size_relative)
		scale.x *= pgls->g.P2.x - pgls->g.P1.x,
		scale.y *= pgls->g.P2.y - pgls->g.P1.y;
	    if (bitmaps_allowed)    /* no mirroring */
		scale.x = fabs(scale.x), scale.y = fabs(scale.y);
	}
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

	/*
	 * Patch the next-character procedure.
	 */
	pfont->procs.next_char = hpgl_next_char_proc;
	gs_setfont(pgs, pfont);

	/*
	 * Adjust the initial position of the character according to
	 * the text path.
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

	/*
	 * If we're using a stroked font, patch the pen width to reflect
	 * the stroke weight.  Note that when the font's build_char
	 * procedure calls stroke, the CTM is still scaled.
	 ****** WHAT IF scale.x != scale.y? ******
	 */
	if (pfont->PaintType != 0) {
            int     code = 0;
            int     pen = hpgl_get_selected_pen(pgls);
            floatp  nwidth;

            if (weight == 9999)
                nwidth = save_width / scale.y;
            else {
                nwidth = 0.05 + weight * (weight < 0 ? 0.005 : 0.010);
	        pgls->g.pen.width_relative = true;
            }
            if ((code = pcl_palette_PW(pgls, pen, nwidth)) < 0) {
	        pgls->g.pen.width_relative = save_relative;
                return code;
            }

            gs_setlinewidth(pgs, nwidth);
	}

	str[0] = ch;

	/* If SP is a control code, get the width of the space character. */
	if (ch == ' ') {
            space_code = hpgl_get_char_width(pgls, ' ', &space_width);
	    if (space_code == 1) {
                /* Space is a control code. */
		space_width = pl_fp_pitch_cp(&pfs->params) / 100.0
                                * points_2_plu(1.0) / scale.x
                                * (1.0 + pgls->g.character.extra_space.x);
            }
	}

	/* Check for SP control code. */
	if (ch == ' ' && space_code != 0) {
            /* Space is a control code.  Just advance the position. */
	    gs_setmatrix(pgs, &advance_mat);
	    hpgl_call(hpgl_add_point_to_path(pgls, space_width, 0.0,
					     hpgl_plot_move_relative, false));
	    hpgl_call(gs_currentpoint(pgs, &end_pt));
	} else {
            if (use_show) {
		hpgl_call(hpgl_set_drawing_color(pgls, hpgl_rm_character));
	        code = gs_show_n_init(penum, pgs, (char *)str, 1);
	    } else
		code = gs_charpath_n_init(penum, pgs, (char *)str, 1, true);
	    if ((code < 0) || ((code = gs_show_next(penum)) != 0) ) {
                gs_show_enum_release(penum, pgls->memory);
		return (code < 0 ? code : gs_error_Fatal);
	    }
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
	    if ( (text_path == hpgl_text_right)        &&
		 (pgls->g.character.extra_space.x != 0)  ) {
		hpgl_get_char_width(pgls, ch, &width);
		end_pt.x = start_pt.x + width / scale.x;
		hpgl_call(hpgl_add_point_to_path(pgls, end_pt.x, end_pt.y, hpgl_plot_move_absolute, false));
	    }
	}
	gs_free_object(pgls->memory, penum, "hpgl_print_char");

	/*
	 * Adjust the final position according to the text path.
	 */
	switch (text_path) {

	  case hpgl_text_right:
	    break;

	  case hpgl_text_down:
	    {
                hpgl_real_t     height;

	        hpgl_call( hpgl_get_current_cell_height( pgls,
                                                         &height,
                                                         true
                                                         ) );
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

	        hpgl_call(hpgl_get_current_cell_height( pgls,
							&height,
							true
							));

		hpgl_call(hpgl_add_point_to_path(pgls, start_pt.x, 
						 end_pt.y + height / scale.y, hpgl_plot_move_absolute, false));
	    }
	    break;
	}

	gs_setmatrix(pgs, &save_ctm); 
	hpgl_call(gs_currentpoint(pgs, &end_pt));
	if (!use_show)
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_character));
        (void )pcl_palette_PW(pgls, hpgl_get_selected_pen(pgls), save_width);

	pgls->g.pen.width_relative = save_relative;
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
private bool
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
 private int
hpgl_get_character_origin_offset(hpgl_state_t *pgls, int origin,
				 hpgl_real_t width, hpgl_real_t height,
				 gs_point *offset)
{
    double pos_x = 0.0, pos_y = 0.0;
    double off_x, off_y;
    /* stickfonts are offset by 16 grid units or .33 times the
       point size.  HAS need to support other font types. */
    if ( origin > 10 )
	off_x = off_y = 0.33 * height;
    switch ( origin )
	{
	case 11:
	    pos_x = -off_x;
	    pos_y = -off_y;
	case 1:
	    break;
	case 12:
	    pos_x = -off_x;
	case 2:
	    pos_y = .5 * height;
	    break;
	case 13:
	    pos_x = -off_x;
	    pos_y = off_y;
	case 3:
	    pos_y += height;
	    break;
	case 14:
	    pos_y = -off_y;
	case 4:
	    pos_x = .5 * width;
	    break;
	case 15:
	case 5:
	    pos_x = .5 * width;
	    pos_y = .5 * height;
	    break;
	case 16:
	    pos_y = off_y;
	case 6:
	    pos_x = .5 * width;
	    pos_y += height;
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
	    pos_y = .5 * height;
	    break;
	case 19:
	    pos_x = off_x;
	    pos_y = off_y;
	case 9:
	    pos_x += width;
	    pos_y += height;
	    break;
	case 21:
	    {
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
    return 0;
}
			  
/* Prints a buffered line of characters. */
/* If there is a CR, it is the last character in the buffer. */
 private int
hpgl_process_buffer(hpgl_state_t *pgls)
{
	gs_point offset;
	hpgl_real_t label_length = 0.0, label_height = 0.0;
	bool vertical = hpgl_text_is_vertical(pgls->g.character.text_path);
	/*
	 * NOTE: the two loops below must be consistent with each other!
	 */

	{
	  hpgl_real_t width = 0.0, height = 0.0;
	  int save_index = pgls->g.font_selected;
	  int i = 0;

	  while ( i < pgls->g.label.char_count )
	    {
	      byte ch = pgls->g.label.buffer[i++];

	      if ( ch < 0x20 && !pgls->g.transparent_data )
		switch (ch)
		{
		case BS :
		  if ( width == 0.0 ) /* BS as first char of string */
		    { hpgl_ensure_font(pgls);
		      hpgl_get_char_width(pgls, ' ', &width);
		      hpgl_call(hpgl_get_current_cell_height(pgls, &height, vertical));
		    }
		  if ( vertical )
		    { /* Vertical text path, back up in Y. */
		      label_height -= height;
		      if ( label_height < 0.0 )
			label_height = 0.0;
		    }
		  else
		    { /* Horizontal text path, back up in X. */
		      label_length -= width;
		      if ( label_length < 0.0 )
			label_length = 0.0;
		    }
		  continue;
		case LF :
		  continue;
		case CR :
		  continue;
		case FF :
		  continue;
		case HT :
		  hpgl_ensure_font(pgls);
		  hpgl_get_char_width(pgls, ' ', &width);
		  width *= 5;
		  goto acc_ht;
		case SI :
		  hpgl_select_font(pgls, 0);
		  continue;
		case SO :
		  hpgl_select_font(pgls, 1);
		  continue;
		default :
		  break;
		}
	      hpgl_ensure_font(pgls);
	      hpgl_get_char_width(pgls, ch, &width);
acc_ht:	      hpgl_call(hpgl_get_current_cell_height(pgls, &height, vertical));
	      if ( vertical )
		{ /* Vertical text path: sum heights, take max of widths. */
		  if ( width > label_length )
		    label_length = width;
		  label_height += height;
		}
	      else
		{ /* Horizontal text path: sum widths, take max of heights. */
		  label_length += width;
		  if ( height > label_height )
		    label_height = height;
		}
	    }
	  hpgl_select_font(pgls, save_index);
	}
	hpgl_call(hpgl_get_character_origin_offset(pgls, pgls->g.label.origin,
						   label_length, label_height,
						   &offset));
	switch ( pgls->g.character.text_path )
	  {
	  case hpgl_text_left:
	    offset.x -= label_length;
	    break;
	  case hpgl_text_down:
	    offset.y -= label_height;
	    break;
	  default:
	    DO_NOTHING;
	  }
	
	/* these units should be strictly plu except for LO 21 */
	if ( pgls->g.label.origin != 21 ) {  
	    gs_matrix mat;
	    hpgl_compute_user_units_to_plu_ctm(pgls, &mat);
	    offset.x /= mat.xx;
	    offset.y /= mat.yy;
	}
	/* now add the offsets in relative plu coordinates */
	hpgl_call(hpgl_add_point_to_path(pgls, -offset.x, -offset.y,
					 hpgl_plot_move_relative, false));
	{
	  int i;
	  for ( i = 0; i < pgls->g.label.char_count; ++i )
	    { byte ch = pgls->g.label.buffer[i];

	      if ( ch < 0x20 && !pgls->g.transparent_data )
		{ hpgl_real_t spaces, lines;

		  switch (ch)
		    {
		    case BS :
		      spaces = -1, lines = 0;
		      break;
		    case LF :
		      /*
		       * If the text path is vertical, we must use the
		       * computed label (horizontal) width, not the width
		       * of a space.
		       */
		      if ( vertical )
			{ hpgl_move_cursor_by_characters(pgls, 0, -1,
							 &label_length);
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
		      hpgl_select_font(pgls, 0);
		      continue;
		    case SO :
		      hpgl_select_font(pgls, 1);
		      continue;
		    default :
		      goto print;
		    }
		  hpgl_move_cursor_by_characters(pgls, spaces, lines,
						 (const hpgl_real_t *)0);
		  continue;
		}
print:	      hpgl_ensure_font(pgls);
	      hpgl_call(hpgl_print_char(pgls, ch));
	    }
	}

	pgls->g.label.char_count = 0;
	return 0;
}

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
	    pargs->phase = 1;
	  }

	while ( p < rlimit )
	  { byte ch = *++p;
	    if_debug1('I',
		      (ch == '\\' ? " \\%c" : ch >= 33 && ch <= 126 ? " %c" :
		       " \\%03o"),
		      ch);
	    if ( ch == pgls->g.label.terminator )
	      {
		if ( !print_terminator )
		  {
		    hpgl_call(hpgl_process_buffer(pgls));
		    hpgl_call(hpgl_destroy_label_buffer(pgls));
		    pargs->source.ptr = p;
		    /*
		     * Depending on the DV/LO combination, conditionally
		     * restore the initial position, per TRM 23-78.
		     */
		    if ( !hpgl_can_concat_labels(pgls) )
		      { /* HAS not sure if this is correct.  But
                           restoring Y causes problems printing
                           captions in the FTS. */
			hpgl_call(hpgl_add_point_to_path(pgls,
					pgls->g.label.initial_pos.x,
					pgls->g.pos.y,
					hpgl_plot_move_absolute, true));
		      }
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

	    hpgl_call(hpgl_buffer_char(pgls, ch));
	    if ( ch == CR && !pgls->g.transparent_data )
	      {
		hpgl_call(hpgl_process_buffer(pgls));
		hpgl_call(hpgl_destroy_label_buffer(pgls));
		hpgl_call(hpgl_init_label_buffer(pgls));
	      }
	  }
	pargs->source.ptr = p;
	return e_NeedData;
}

 int
hpgl_print_symbol_mode_char(hpgl_state_t *pgls)
{	
        /* save the original origin since symbol mode character are
           always centered */
	int saved_origin = pgls->g.label.origin;
	gs_point save_pos = pgls->g.pos;
	hpgl_call(hpgl_gsave(pgls));
	/* HAS this need checking.  I don't know how text direction
           and label origin interact in symbol mode */
	pgls->g.label.origin = 5;
	/* HAS - alot of work for one character */
	hpgl_call(hpgl_clear_current_path(pgls));
	hpgl_call(hpgl_init_label_buffer(pgls));
	hpgl_call(hpgl_buffer_char(pgls, pgls->g.symbol_mode));
	hpgl_call(hpgl_process_buffer(pgls));
	hpgl_call(hpgl_destroy_label_buffer(pgls));
	hpgl_call(hpgl_grestore(pgls));
	/* restore the origin */
	pgls->g.label.origin = saved_origin;
	hpgl_call(hpgl_set_current_position(pgls, &save_pos));
	return 0;
}

/* Initialization */
private int
pglabel_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('C', 'P', hpgl_CP, hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
	  /* LB also has special argument parsing. */
	  HPGL_COMMAND('L', 'B', hpgl_LB, hpgl_cdf_polygon|hpgl_cdf_lost_mode_cleared|hpgl_cdf_pcl_rtl_both),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pglabel_init = {
  pglabel_do_registration, 0
};
