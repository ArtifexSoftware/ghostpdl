/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pgchar.c */
/* HP-GL/2 font and character commands */
#include "math_.h"
#include "stdio_.h"		/* for gdebug.h */
#include "gdebug.h"
#include "pgmand.h"
#include "pginit.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcfsel.h"
#include "pcpalet.h"

/* ------ Internal procedures ------ */

/* Define font parameters (AD, SD). */
private int
hpgl_font_definition(hpgl_args_t *pargs, hpgl_state_t *pgls, int index)
{	/*
	 * Since these commands take an arbitrary number of arguments,
	 * we reset the argument bookkeeping after each group.
	 * We reset phase to 1, 2, or 3 after seeing the first pair,
	 * so we can tell whether there were any arguments at all.
	 * (1 means no parameter changed, >1 means some parameter changed.)
	 */
	pcl_font_selection_t *pfs = &pgls->g.font_selection[index];
#define pfp (&pfs->params)
	int kind;

	for ( ; hpgl_arg_c_int(pargs, &kind); pargs->phase |= 1 )
	  switch ( kind )
	    {
	    case 1:		/* symbol set */
	      { int32 sset;
	        if ( !hpgl_arg_int(pargs, &sset) )
		  return e_Range;
		if ( pfp->symbol_set != (uint)sset )
		  pfp->symbol_set = (uint)sset,
		    pargs->phase |= 2;
	      }
	      break;
	    case 2:		/* spacing */
	      { int spacing;
	        if ( !hpgl_arg_c_int(pargs, &spacing) || (spacing & ~1) )
		  return e_Range;
		if ( pfp->proportional_spacing != spacing )
		  pfp->proportional_spacing = spacing,
		    pargs->phase |= 2;
	      }
	      break;
	    case 3:		/* pitch */
	      { hpgl_real_t pitch;
	        if ( !hpgl_arg_c_real(pargs, &pitch) || pitch < 0 )
		  return e_Range;
		if ( pl_fp_pitch_per_inch(pfp) != pitch )
		  pl_fp_set_pitch_per_inch(pfp, pitch),
		    pargs->phase |= 2;
	      }
	      break;
	    case 4:		/* height */
	      { hpgl_real_t height;
	        if ( !hpgl_arg_c_real(pargs, &height) || height < 0 )
		  return e_Range;
		if ( pfp->height_4ths != (uint)(height * 4) )
		  pfp->height_4ths = (uint)(height * 4),
		    pargs->phase |= 2;
	      }
	      break;
	    case 5:		/* posture */
	      { int posture;
	        if ( !hpgl_arg_c_int(pargs, &posture) ||
		     posture < 0 || posture > 2
		   )
		  return e_Range;
		if ( pfp->style != posture )
		  pfp->style = posture,
		    pargs->phase |= 2;
	      }
	      break;
	    case 6:		/* stroke weight */
	      { int weight;
	        if ( !hpgl_arg_c_int(pargs, &weight) ||
		     weight < -7 || (weight > 7 && weight != 9999)
		   )
		  return e_Range;
		if ( pfp->stroke_weight != weight )
		  pfp->stroke_weight = weight,
		    pargs->phase |= 2;
	      }
	      break;
	    case 7:		/* typeface */
	      { int32 face;
	        if ( !hpgl_arg_int(pargs, &face) )
		  return e_Range;
		if ( pfp->typeface_family != (uint)face )
		  pfp->typeface_family = (uint)face,
		    pargs->phase |= 2;
	      }
	      break;
	    default:
	      return e_Range;
	    }
	/* If there were no arguments at all, default all values. */
	if ( !pargs->phase )
	  hpgl_default_font_params(pfs);
	if ( pargs->phase != 1 )
	  { /* A value changed, or we are defaulting.  Decache the font. */
	    pfs->font = 0;
	    if ( index == pgls->g.font_selected )
	      pgls->g.font = 0;
	  }
	return 0;
}
/* Define label drawing direction (DI, DR). */
private int
hpgl_label_direction(hpgl_args_t *pargs, hpgl_state_t *pgls, bool relative)
{	hpgl_real_t run = 1, rise = 0;

	if ( hpgl_arg_c_real(pargs, &run) )
	  { if ( !hpgl_arg_c_real(pargs, &rise) || (run == 0 && rise == 0) )
	      return e_Range;
	    { double hyp = hypot(run, rise);
	      run /= hyp;
	      rise /= hyp;
	    }
	  }
	pgls->g.character.direction.x = run;
	pgls->g.character.direction.y = rise;
	pgls->g.character.direction_relative = relative;
	pgls->g.carriage_return_pos = pgls->g.pos;
	return 0;
}

/* Select font by ID (FI, FN). */
private int
hpgl_select_font_by_id(hpgl_args_t *pargs, hpgl_state_t *pgls, int index)
{	pcl_font_selection_t *pfs = &pgls->g.font_selection[index];
	int32 id;
	int code;

	if ( !hpgl_arg_c_int(pargs, &id) || id < 0 )
	  return e_Range;
	code = pcl_select_font_by_id(pfs, id, pgls /****** NOTA BENE ******/);
	switch ( code )
	  {
	  default:		/* error */
	    return code;
	  case 1:		/* ID not found, no effect */
	    return 0;
	  case 0:		/* ID found */
	    break;
	  }
	pgls->g.font = pfs->font;
	pgls->g.map = pfs->map;
	/*
	 * If we just selected a bitmap font, force the equivalent of SB1.
	 * See TRM 23-65 and 23-81.
	 */
	if ( pfs->font->scaling_technology == plfst_bitmap )
	  pgls->g.bitmap_fonts_allowed = true;
	return 0;
}

/* Select font (SA, SS). */
private int
hpgl_select_font(hpgl_state_t *pgls, int index)
{
	if ( pgls->g.font_selected != index )
	  { pgls->g.font_selected = index;
	    pgls->g.font = pgls->g.font_selection[index].font;
	    pgls->g.map = pgls->g.font_selection[index].map;
	  }
	return 0;
}

/* ------ Commands ------ */

/* AD [kind,value...]; */
 int
hpgl_AD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_font_definition(pargs, pgls, 1);
}

/*
 * CF [mode[,pen]];
 */
 int
hpgl_CF(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             mode = 0;
    int             npen = pcl_palette_get_num_entries(pgls->ppalet);
    int32           pen = 0;

    if (hpgl_arg_c_int(pargs, &mode)) {
        if ((mode & ~3) != 0)
	    return e_Range;
	/* a careful read of the PCLTRM indicates the default values
           for CF edge pen are the current pen for modes 3 & 4.  NB
           not sure if it is the current pen at the time the character
           is rendered or at the time of the CF but this passes the
           relevant fts test. */
	if (mode == 0 || mode == 3)
	    pen = pgls->g.pen.selected;

	/* With only 1 argument, leave the current pen unchanged. */
	if (hpgl_arg_int(pargs, &pen)) {
            if ((pen < 0) || (pen >= npen)) 
		return e_Range;
	} else
	      pen = pgls->g.character.edge_pen;
    }
    pgls->g.character.fill_mode = mode;
    pgls->g.character.edge_pen = pen;
    return 0;
}

/* DI [run,rise]; */
 int
hpgl_DI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_label_direction(pargs, pgls, false);
}

/* DR [run,rise]; */
 int
hpgl_DR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_label_direction(pargs, pgls, true);
}

/* DT terminator[,mode]; */
 int
hpgl_DT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;
	byte ch = (byte)pargs->phase;
	int mode = 1;

	/* We use phase to remember the terminator character */
	/* in case we had to restart execution. */
	if ( p >= rlimit )
	  return e_NeedData;
	if ( !ch )
	  switch ( (ch = *++p) )
	    {
	    case ';':
	      pargs->source.ptr = p;
	      pgls->g.label.terminator = 3;
	      pgls->g.label.print_terminator = false;
	      return 0;
	    case 0: case 10: case 27:
	      return e_Range;
	    default:
	      if ( p >= rlimit )
		return e_NeedData;
	      if ( *++p ==',' )
		{ pargs->source.ptr = p;
		  pargs->phase = ch;
		}
	    }
	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	pgls->g.label.terminator = ch;
	pgls->g.label.print_terminator = !mode;
	return 0;
}

/* DV [path[,line]]; */
 int
hpgl_DV(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int path = 0, line = 0;

	hpgl_arg_c_int(pargs, &path);
	hpgl_arg_c_int(pargs, &line);
	if ( (path & ~3) | (line & ~1) )
	  return e_Range;
	pgls->g.character.text_path = path;
	pgls->g.character.line_feed_direction = (line ? -1 : 1);
	pgls->g.carriage_return_pos = pgls->g.pos;
	return 0;
}

/* ES [width[,height]]; */
 int
hpgl_ES(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t width = 0, height = 0;

	hpgl_arg_c_real(pargs, &width);
	hpgl_arg_c_real(pargs, &height);
	pgls->g.character.extra_space.x = width;
	pgls->g.character.extra_space.y = height;
	return 0;
}

/* FI fontid; */
 int
hpgl_FI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_select_font_by_id(pargs, pgls, 0);
}

/* FN fontid; */
 int
hpgl_FN(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_select_font_by_id(pargs, pgls, 1);
}

/* The following is an extension documented in the Comparison Guide. */
/* LM [mode[, row number]]; */
 int
hpgl_LM(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 0, row_number = 0;
	int old_mode =
	  (pgls->g.label.double_byte ? 1 : 0) +
	  (pgls->g.label.write_vertical ? 2 : 0);

	hpgl_arg_c_int(pargs, &mode);
	hpgl_arg_c_int(pargs, &row_number);
	pgls->g.label.row_offset =
	  (row_number < 0 ? 0 : row_number > 255 ? 255 : row_number) << 8;
	mode &= 3;
	pgls->g.label.double_byte = (mode & 1) != 0;
	pgls->g.label.write_vertical = (mode & 2) != 0;
	/*
	 * The documentation says "When LM switches modes, it turns off
	 * symbol mode."  We take this literally: LM only turns off
	 * symbol mode if the new label mode differs from the old one.
	 */
	if ( mode != old_mode )
	  pgls->g.symbol_mode = 0;
	return 0;
}

/* LO [origin]; */
 int
hpgl_LO(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int origin = 1;

	hpgl_arg_c_int(pargs, &origin);
	if ( origin < 1 || origin == 10 || origin == 20 || origin > 21 )
	  return e_Range;
	pgls->g.label.origin = origin;
	pgls->g.carriage_return_pos = pgls->g.pos;
	return 0;
}

/* SA; */
 int
hpgl_SA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_select_font(pgls, 1);
}

/* SB [mode]; */
 int
hpgl_SB(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 0;

	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	if ( pgls->g.bitmap_fonts_allowed != mode )
	  { int i;

	    pgls->g.bitmap_fonts_allowed = mode;
	    /*
	     * A different set of fonts is now available for consideration.
	     * Decache any affected font(s): those selected by parameter,
	     * and bitmap fonts selected by ID if bitmap fonts are now
	     * disallowed.
	     */
	    for ( i = 0; i < countof(pgls->g.font_selection); ++i )
	      { pcl_font_selection_t *pfs = &pgls->g.font_selection[i];
	        if ( !pfs->selected_by_id ||
		     (!mode && pfs->font != 0 &&
		      pfs->font->scaling_technology == plfst_bitmap)
		   )
		  { pfs->font = 0;
		    if ( i == pgls->g.font_selected )
		      pgls->g.font = 0;
		  }
	      }
	  }
	return 0;
}

/* SD [kind,value...]; */
 int
hpgl_SD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_font_definition(pargs, pgls, 0);
}

/* SI [width,height]; */
 int
hpgl_SI(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t width_cm, height_cm;

	if ( hpgl_arg_c_real(pargs, &width_cm) )
	  { if ( !hpgl_arg_c_real(pargs, &height_cm) )
	      return e_Range;
	    pgls->g.character.size.x = mm_2_plu(width_cm * 10);
	    pgls->g.character.size.y = mm_2_plu(height_cm * 10);
	    pgls->g.character.size_mode = hpgl_size_absolute;
	  }
	else
	  pgls->g.character.size_mode = hpgl_size_not_set;
	return 0;
}

/* SL [slant]; */
 int
hpgl_SL(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t slant = 0;

	hpgl_arg_c_real(pargs, &slant);
	pgls->g.character.slant = slant;
	return 0;
}

/* SR [width,height]; */
 int
hpgl_SR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t width_pct, height_pct;

	if ( hpgl_arg_c_real(pargs, &width_pct) )
	  { if ( !hpgl_arg_c_real(pargs, &height_pct) )
	      return e_Range;
	    pgls->g.character.size.x = width_pct / 100;
	    pgls->g.character.size.y = height_pct / 100;
	  }
	else
	  { pgls->g.character.size.x = 0.0075;
	    pgls->g.character.size.y = 0.015;
	  }
	pgls->g.character.size_mode = hpgl_size_relative;
	return 0;
}

/* SS; */
 int
hpgl_SS(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return hpgl_select_font(pgls, 0);
}

/* TD [mode]; */
 int
hpgl_TD(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 0;

	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	pgls->g.transparent_data = mode;
	return 0;
}

/* Initialization */
private int
pgchar_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('A', 'D', hpgl_AD, 0),		/* kind/value pairs */
	  HPGL_COMMAND('C', 'F', hpgl_CF, 0),
	  HPGL_COMMAND('D', 'I', hpgl_DI, 0),
	  HPGL_COMMAND('D', 'R', hpgl_DR, 0),
	  /* DT has special argument parsing, so it must handle skipping */
	  /* in polygon mode itself. */
	  HPGL_COMMAND('D', 'T', hpgl_DT, hpgl_cdf_polygon),
	  HPGL_COMMAND('D', 'V', hpgl_DV, 0),
	  HPGL_COMMAND('E', 'S', hpgl_ES, 0),
	  HPGL_COMMAND('F', 'I', hpgl_FI, 0),
	  HPGL_COMMAND('F', 'N', hpgl_FN, 0),
	  HPGL_COMMAND('L', 'M', hpgl_LM, 0),
	  HPGL_COMMAND('L', 'O', hpgl_LO, 0),
	  HPGL_COMMAND('S', 'A', hpgl_SA, 0),
	  HPGL_COMMAND('S', 'B', hpgl_SB, 0),
	  HPGL_COMMAND('S', 'D', hpgl_SD, 0),		/* kind/value pairs */
	  HPGL_COMMAND('S', 'I', hpgl_SI, 0),
	  HPGL_COMMAND('S', 'L', hpgl_SL, 0),
	  HPGL_COMMAND('S', 'R', hpgl_SR, 0),
	  HPGL_COMMAND('S', 'S', hpgl_SS, 0),
	  HPGL_COMMAND('T', 'D', hpgl_TD, 0),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgchar_init = {
  pgchar_do_init, 0
};
