/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pglfill.c */
/* HP-GL/2 line and fill attributes commands */
#include "memory_.h"
#include "pgmand.h"
#include "pginit.h"
#include "pggeom.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "gsuid.h"		/* for gxbitmap.h */
#include "gstypes.h"		/* for gxbitmap.h */
#include "gsstate.h"            /* needed by gsrop.h */
#include "gsrop.h"              /* for source transparency */
#include "gxbitmap.h"

/* AC [x,y]; Anchor corner for fill offsets, note that this is
   different than the anchor corner of the pcl picture frame. */
int
hpgl_AC(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t x, y;

	if ( hpgl_arg_units(pargs, &x) )
	  { if ( !hpgl_arg_units(pargs, &y) )
	      return e_Range;
	  }
	else
	  {
	    x = 0.0;
	    y = 0.0;
	  }

	/* draw the current path */
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	pgls->g.anchor_corner.x = x;
	pgls->g.anchor_corner.y = y;

	return 0;
}

/* FT 1|2; */
/* FT 3[,spacing[,angle]]; */
/* FT 4[,spacing[,angle]]; */
/* FT 10[,level]; */
/* FT 11[,index]; */
/* FT 21[,type]; */
/* FT 22[,id]; */
/* FT; */
int
hpgl_FT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int type = hpgl_fill_solid;
	hpgl_hatch_params_t *params;

	hpgl_arg_int(pargs, &type);
	switch ( type )
	  {
	  case hpgl_fill_solid:	/* 1 */
	  case hpgl_fill_solid2: /* 2 */
	    /* Default all the parameters. */
	    pgls->g.fill.param.hatch.spacing = 0;
	    pgls->g.fill.param.hatch.angle = 0;
	    pgls->g.fill.param.crosshatch.spacing = 0;
	    pgls->g.fill.param.crosshatch.angle = 0;
	    pgls->g.fill.param.shading = 100;
	    pgls->g.fill.param.pattern_index = 1;  /****** NOT SURE ******/
	    pgls->g.fill.param.pattern_type = 1;  /****** NOT SURE ******/
	    pgls->g.fill.param.pattern_id = 0;  /****** NOT SURE ******/
	    break;
	  case hpgl_fill_hatch:	/* 3 */
	    params = &pgls->g.fill.param.hatch;
	    goto hatch;
	  case hpgl_fill_crosshatch: /* 4 */
	    params = &pgls->g.fill.param.crosshatch;
hatch:	    { hpgl_real_t spacing = params->spacing;
	      hpgl_real_t angle = params->angle;

	      if ( hpgl_arg_real(pargs, &spacing) )
		{ if ( spacing < 0 )
		    return e_Range;
		  hpgl_arg_real(pargs, &angle);
		}
	      /*
	       * If the specified spacing is 0, we use 1% of the P1/P2
	       * diagonal distance.  We handle this when performing the
	       * fill, not here, because this distance may change
	       * depending on the position of P1 and P2.
	       */
	      params->spacing = spacing;
	      params->angle = angle;
	    }
	    break;
	  case hpgl_fill_shaded: /* 10 */
	    { int level;
	      if ( hpgl_arg_c_int(pargs, &level) )
		{ if ( level < 0 || level > 100 )
		    return e_Range;
		  pgls->g.fill.param.shading = level;
		}
	    }
	    break;
	  case hpgl_fill_hpgl_user_defined: /* 11 */
	    { int index;
	      if ( hpgl_arg_c_int(pargs, &index) )
		{ if ( index < 1 || index > 8 )
		    return e_Range;
		  pgls->g.fill.param.pattern_index = index;
		}
	    }
	    break;
	  case hpgl_fill_pcl_crosshatch: /* 21 */
	    { int pattern;
	      if ( hpgl_arg_c_int(pargs, &pattern) )
		{ if ( pattern < 1 || pattern > 6 )
		    return e_Range;
		  pgls->g.fill.param.pattern_type = pattern;
		}
	    }
	    break;
	  case hpgl_fill_pcl_user_defined: /* 22 */
	    { int32 id;
	      if ( hpgl_arg_int(pargs, &id) )
		 { if ( id < 0 || id > 0xffff )
		     return e_Range;
		   pgls->g.fill.param.pattern_id = id;
		 }
	    }
	    break;
	  default:
	    return e_Range;
	  }
	pgls->g.fill.type = type;
	return 0;
}

/* LA [kind1,value1[,kind2,value2[,kind3,value3]]]; */
int
hpgl_LA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int cap = pgls->g.line.cap, join = pgls->g.line.join;
	hpgl_real_t miter_limit = pgls->g.miter_limit;
	bool no_args=true;
	int kind;

	while ( hpgl_arg_c_int(pargs, &kind) )
	  {
	    no_args=false;
	    switch ( kind )
	      {
	      case 1:		/* line ends */
		if ( !hpgl_arg_c_int(pargs, &cap) || cap < 1 || cap > 4 )
		  return e_Range;
		break;
	      case 2:		/* line joins */
		if ( !hpgl_arg_c_int(pargs, &join) || join < 1 || join > 6 )
		  return e_Range;
		break;
	      case 3:		/* miter limit */
		if ( !hpgl_arg_c_real(pargs, &miter_limit) )
		  return e_Range;
		if ( miter_limit < 1 )
		  miter_limit = 1;
		break;
	      default:
		return e_Range;
	      }
	  }
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	/* LA1,1,2,1,3,5 is the same as LA */
	if (no_args)
	  {
	    hpgl_args_setup(pargs); 
	    hpgl_args_add_int(pargs, 1);
	    hpgl_args_add_int(pargs, 1);
	    hpgl_args_add_int(pargs, 2);
	    hpgl_args_add_int(pargs, 1);
	    hpgl_args_add_int(pargs, 3);
	    hpgl_args_add_real(pargs, 5.0);
	    hpgl_LA(pargs, pgls);
	    return 0;
	  }

	pgls->g.line.cap = cap;
	pgls->g.line.join = join;
	pgls->g.miter_limit = miter_limit;
	return 0;
}

/* constant data for default pattern percentages,  */

private const hpgl_line_type_t hpgl_fixed_pats[8] = {
  {2, {0.0, 1.0}},
  {2, {0.5, 0.5}},
  {2, {0.7, 0.3}},
  {4, {0.8, 0.1, 0.0, 0.1}},
  {4, {0.7, 0.1, 0.1, 0.1}},
  {6, {0.5, 0.1, 0.1, 0.1, 0.1, 0.1}},
  {6, {0.7, 0.1, 0.0, 0.1, 0.0, 0.1}},
  {8, {0.5, 0.1, 0.0, 0.1, 0.1, 0.1, 0.0, 0.1}}
};

private const hpgl_line_type_t hpgl_adaptive_pats[8] = {
  {3, {0.0, 1.0, 0.0}},
  {3, {0.25, 0.5, 0.25}},
  {3, {0.35, 0.3, 0.35}},
  {5, {0.4, 0.1, 0.0, 0.1, 0.4}},
  {5, {0.35, 0.1, 0.1, 0.1, 0.35}},
  {7, {0.25, 0.1, 0.1, 0.1, 0.1, 0.1, 0.25}},
  {7, {0.35, 0.1, 0.0, 0.1, 0.0, 0.1, 0.35}},
  {9, {0.25, 0.1, 0.0, 0.1, 0.1, 0.1, 0.0, 0.1, 0.25}}
};

/* LT type[,length[,mode]]; */
/* LT99; */
/* LT; */
int
hpgl_LT(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	int type = 0;

	/* Draw the current path for any LT command irrespective if
           the LT changes anything */
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

	if ( hpgl_arg_c_int(pargs, &type) )
	  { 
	    /* restore old saved line if we have a solid line,
               otherwise the instruction is ignored. */
	    if ( type == 99 )
	      {
		if ( pgls->g.line.current.is_solid == true )
		  pgls->g.line.current = pgls->g.line.saved;
		return 0;
	      }
	    else
	      {	
		hpgl_real_t length = 0.04;
		int mode = 0;

		if ( type < -8 || type > 8 ||
		     (hpgl_arg_c_real(pargs, &length) &&
		      (length <= 0 ||
		       (hpgl_arg_c_int(pargs, &mode) && (mode & ~1))))
		     )
		  return e_Range;
		pgls->g.line.current.pattern_length = length;
		pgls->g.line.current.pattern_length_relative = mode == 0;
		pgls->g.line.current.is_solid = false;
	      }
	  } else
	    {
	      /* no args restore defaults and set previous line
                 type. */
	      /* HAS **BUG**BUG** initial value of line type
                 is not supplied unless it gets set by previous LT
                 command. */
	      pgls->g.line.saved = pgls->g.line.current;
	      pgls->g.line.current.is_solid = true;
	      memcpy(&pgls->g.fixed_line_type, 
		     &hpgl_fixed_pats, 
		     sizeof(hpgl_fixed_pats));
	      memcpy(&pgls->g.adaptive_line_type, 
		     &hpgl_adaptive_pats, 
		     sizeof(hpgl_adaptive_pats));
	    }
	
	pgls->g.line.current.type = type;
	return 0;
}

/* PW [width[,pen]]; */
int
hpgl_PW(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_real_t width = 
	  ((pgls->g.pen.width_relative) ? 
	   (0.01 * hpgl_compute_distance(pgls->g.P1.x, pgls->g.P1.y,
					 pgls->g.P2.x, pgls->g.P2.y)):
	   (0.35));
	int pmin = 0, pmax = pgls->g.number_of_pens - 1;

	/* we maintain the line widths in plotter units, irrespective
           of current units (WU) */
	if ( hpgl_arg_c_real(pargs, &width) )
	  { 
	    if ( hpgl_arg_c_int(pargs, &pmin) ) 
	      if ( pmin < 0 || pmin > pmax ) 
		return e_Range;
	      else 
		pmax = pmin;
	  }

	/* PCLTRM 22-38 metric widths scaled scaled by the ratio of
           the picture frame to plot size.  Note that we always store
           the line width in PU not MM. */
	if ( !pgls->g.pen.width_relative ) 
	  {
	    width = width * (min((hpgl_real_t)pgls->g.picture_frame_height /
				 (hpgl_real_t)pgls->g.plot_height,
				 ((hpgl_real_t)pgls->g.picture_frame_width /
				  (hpgl_real_t)pgls->g.plot_width)));
	    width = mm_2_plu(width);
	  }
	
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	
	{ 
	  int i;
	  for ( i = pmin; i <= pmax; ++i )
	    pgls->g.pen.width[i] = width;
	}
	return 0;
}

/* RF [index[,width,height,pen...]]; */
int
hpgl_RF(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int index, width, height;
	uint raster;

	if ( pargs->phase == 0 )
	  { if ( !hpgl_arg_c_int(pargs, &index) )
	      { hpgl_default_all_fill_patterns(pgls, true);
	        /**** INVALIDATE ANY CACHED VALUES ****/
		return 0;
	      }
	    if ( index < 1 || index > 8 )
	      return e_Range;
	    if ( !hpgl_arg_c_int(pargs, &width) )
	      { hpgl_default_fill_pattern(pgls, index, true);
	        /**** INVALIDATE ANY CACHED VALUES ****/
		return 0;
	      }
	    if ( width < 1 || width > 255 ||
		 !hpgl_arg_c_int(pargs, &height) ||
		 height < 1 || height > 255
	       )
	      return e_Range;
	    pgls->g.raster_fill.width = width;
	    pgls->g.raster_fill.height = height;
	    /**** MODIFY FOLLOWING FOR COLOR ****/
	    raster = bitmap_raster(width);
	    pgls->g.raster_fill.raster = raster;
	    pgls->g.raster_fill.data =
	      gs_alloc_byte_array(pgls->memory, height, raster, "hpgl_RF");
	    if ( pgls->g.raster_fill.data == 0 )
	      return_error(e_Memory);
	    memset(pgls->g.raster_fill.data, 0, raster * height);
	    pgls->g.raster_fill.index = index;
	    hpgl_next_phase(pargs);
	  }
	else
	  { width = pgls->g.raster_fill.width;
	    height = pgls->g.raster_fill.height;
	    raster = pgls->g.raster_fill.raster;
	  }
	/* We use the phase to count pixel values, 1-origin. */
	while ( pargs->phase <= width * height )
	  { int pixel;
	    if ( !hpgl_arg_c_int(pargs, &pixel) )
	      break;
	    /* HAS does not handle color */
	    if ( pixel )
	      {
		int bitindx = pargs->phase - 1;
		pgls->g.raster_fill.data[bitindx >> 3] |= (128 >> (bitindx & 7));
	      }
	    hpgl_next_phase(pargs);
	  }
	index = pgls->g.raster_fill.index - 1; /* make 0-origin */
	gs_free_object(pgls->memory, pgls->g.fill_pattern[index].data,
		       "hpgl_RF(old pattern)");
	pgls->g.fill_pattern[index].data = pgls->g.raster_fill.data;
	/**** FILL IN REST OF fill_pattern ELEMENT ****/
	return e_Unimplemented;
}

/* SM [char]; */
int
hpgl_SM(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;

	for ( ; ; )
	  { if ( p >= rlimit )
	      { pargs->source.ptr = p;
	        return e_NeedData;
	      }
	    ++p;
	    if ( *p == ' ' )
	      continue;		/* ignore initial spaces */
	    else if ( *p == ';' )
	      { pgls->g.symbol_mode = 0;
	        break;
	      }
	    /*
	     * p. 22-44 of the PCL5 manual says that the allowable codes
	     * are 33-58, 60-126, 161 and 254.  This is surely an error:
	     * it must be 161-254.
	     */
	    else if ( (*p >= 33 && *p <= 126) ||
		      (*p >= 161 && *p <= 254)
		    )
	      pgls->g.symbol_mode = *p;
	    else
	      return e_Range;
	  }
	return 0;
}

/* SP [pen]; */
int
hpgl_SP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int pen = 0;

	if ( hpgl_arg_c_int(pargs, &pen) &&
	     (pen < 0 || pen >= pgls->g.number_of_pens)
	   )
	  return e_Range;
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	pgls->g.pen.selected = pen;
	return 0;
}

/* SV [type[,option1[,option2]]; */
int
hpgl_SV(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int type = hpgl_screen_none;

	if ( hpgl_arg_c_int(pargs, &type) )
	  switch ( type )
	    {
	    case hpgl_screen_none: /* 0 */
	      break;
	    case hpgl_screen_shaded_fill: /* 1 */
	      { int level;
	        if ( !hpgl_arg_c_int(pargs, &level) ||
		     level < 0 || level > 100
		   )
		  return e_Range;
		pgls->g.screen.param.shading = level;
	      }
	      break;
	    case hpgl_screen_hpgl_user_defined:	/* 2 */
	      { int index, mode;

		if ( !hpgl_arg_int(pargs, &index) || index < 1 || index > 8 ||
		     !hpgl_arg_c_int(pargs, &mode) || (mode & ~1)
		   )
		  return e_Range;
		pgls->g.screen.param.user_defined.pattern_index = index;
		pgls->g.screen.param.user_defined.use_current_pen = mode;
	      }
	      break;
	    case hpgl_screen_crosshatch: /* 21 */
	      { int pattern;
	        if ( !hpgl_arg_c_int(pargs, &pattern) ||
		     pattern < 1 || pattern > 6
		   )
		  return e_Range;
		pgls->g.screen.param.pattern_type = pattern;
	      }
	      break;
	    case hpgl_screen_pcl_user_defined: /* 22 */
	      { int32 id;
	        if ( !hpgl_arg_int(pargs, &id) || id < 0 || id > 0xffff )
		  return e_Range;
		pgls->g.screen.param.pattern_id = id;
	      }
	      break;
	    default:
	      return e_Range;
	    }
	pgls->g.screen.type = type;
	return 0;
}

/* TR [mode]; */
int
hpgl_TR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 1;

	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	pgls->g.source_transparent = mode;
	gs_setsourcetransparent(pgls->pgs, pgls->g.source_transparent);
	return 0;
}

/* UL [index[,gap1..20]; */
int
hpgl_UL(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int index;
	if ( hpgl_arg_c_int(pargs, &index) )
	  { hpgl_real_t gap[20];
	    double total = 0;
	    int i;

	    if ( index < -8 || index > 8 || index == 0 )
	      return e_Range;
	    for ( i = 0; i < 20 && hpgl_arg_c_real(pargs, &gap[i]); ++i )
	      { if ( gap[i] < 0 )
		  return e_Range;
	        total += gap[i];
	      }
	    if ( total == 0 )
	      return e_Range;
	    { 
	      hpgl_line_type_t *fixed_plt =
		&pgls->g.fixed_line_type[(index < 0 ? -index : index) - 1];
	      hpgl_line_type_t *adaptive_plt =
		&pgls->g.adaptive_line_type[(index < 0 ? -index : index) - 1];
	      fixed_plt->count = adaptive_plt->count = i;
	      memcpy(fixed_plt->gap, gap, i * sizeof(hpgl_real_t));
	      memcpy(adaptive_plt->gap, gap, i * sizeof(hpgl_real_t));
	    }
	  }
	else
	  {
	    /* no args same as LT; */
	    hpgl_args_setup(pargs);
	    hpgl_LT(pargs, pgls);
	  }

	return 0;
}

/* WU [mode]; */
int
hpgl_WU(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int mode = 0;

	if ( hpgl_arg_c_int(pargs, &mode) && (mode & ~1) )
	  return e_Range;
	pgls->g.pen.width_relative = mode;
	hpgl_args_setup(pargs);
	hpgl_PW(pargs, pgls);
	return 0;
}

/* Initialization */
private int
pglfill_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('A', 'C', hpgl_AC, 0),
	  HPGL_COMMAND('F', 'T', hpgl_FT, 0),
	  HPGL_COMMAND('L', 'A', hpgl_LA, 0),
	  HPGL_COMMAND('L', 'T', hpgl_LT, 0),
	  HPGL_COMMAND('P', 'W', hpgl_PW, 0),
	  HPGL_COMMAND('R', 'F', hpgl_RF, 0),		/* + additional I parameters */
	  /* SM has special argument parsing, so it must handle skipping */
	  /* in polygon mode itself. */
	  HPGL_COMMAND('S', 'M', hpgl_SM, 0),
	  HPGL_COMMAND('S', 'P', hpgl_SP, 0),
	  HPGL_COMMAND('S', 'V', hpgl_SV, 0),
	  HPGL_COMMAND('T', 'R', hpgl_TR, 0),
	  HPGL_COMMAND('U', 'L', hpgl_UL, 0),
	  HPGL_COMMAND('W', 'U', hpgl_WU, 0),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pglfill_init = {
  pglfill_do_init, 0
};
