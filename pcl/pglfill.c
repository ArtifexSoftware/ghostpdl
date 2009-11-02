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

/* pglfill.c - HP-GL/2 line and fill attributes commands */
#include "memory_.h"
#include "pcparse.h"
#include "pgmand.h"
#include "pginit.h"
#include "pggeom.h"
#include "pgdraw.h"
#include "pgmisc.h"
#include "pcdraw.h"
#include "gsuid.h"		/* for gxbitmap.h */
#include "gstypes.h"		/* for gxbitmap.h */
#include "gsstate.h"            /* needed by gsrop.h */
#include "gsrop.h"
#include "gxbitmap.h"
#include "pcpalet.h"
#include "pcpatrn.h"

/*
 * AC [x,y]; Anchor corner for fill offsets, note that this is
 * different than the anchor corner of the pcl picture frame.
 */
  int
hpgl_AC(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    hpgl_real_t     x, y;

    if (hpgl_arg_units(pgls->memory, pargs, &x)) {
        if (!hpgl_arg_units(pgls->memory, pargs, &y))
	    return e_Range;
    } else {
        x = 0.0;
        y = 0.0;
    }

    /* draw the current path */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    pgls->g.anchor_corner.x = x;
    pgls->g.anchor_corner.y = y;

    return 0;
}

/*
 * FT 1|2;
 * FT 3[,spacing[,angle]];
 * FT 4[,spacing[,angle]];
 * FT 10[,level];
 * FT 11[,index][,use_pen_1_or_cur_pen]; - varies from documentation
 * FT 21[,type];
 * FT 22[,id];
 * FT;
 */
  int
hpgl_FT(
    hpgl_args_t *           pargs,
    hpgl_state_t *          pgls
)
{
    int                     type = hpgl_FT_pattern_solid_pen1;
    hpgl_hatch_params_t *   params;

    hpgl_arg_int(pgls->memory, pargs, &type);
    switch (type) {

      case hpgl_FT_pattern_solid_pen1:	/* 1 */
      case hpgl_FT_pattern_solid_pen2: /* 2 */
	/* Default all the parameters. */
	pgls->g.fill.param.hatch.spacing = 0;
	pgls->g.fill.param.hatch.angle = 0;
	pgls->g.fill.param.crosshatch.spacing = 0;
	pgls->g.fill.param.crosshatch.angle = 0;
	pgls->g.fill.param.shading = 100;
	pgls->g.fill.param.user_defined.pattern_index = 1;
        pgls->g.fill.param.user_defined.use_current_pen = false;
	pgls->g.fill.param.pattern_type = 1;  /****** NOT SURE ******/
	pgls->g.fill.param.pattern_id = 0;  /****** NOT SURE ******/
	break;

      case hpgl_FT_pattern_one_line:	/* 3 */
	params = &pgls->g.fill.param.hatch;
	goto hatch;

      case hpgl_FT_pattern_two_lines: /* 4 */
	params = &pgls->g.fill.param.crosshatch;
hatch:
	{
            hpgl_real_t spacing = params->spacing;
	    hpgl_real_t angle = params->angle;

	    if (hpgl_arg_real(pgls->memory, pargs, &spacing)) {
                if (spacing < 0)
		    return e_Range;
		hpgl_arg_real(pgls->memory, pargs, &angle);
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

      case hpgl_FT_pattern_shading: /* 10 */
	{
            int     level;

	    if (hpgl_arg_c_int(pgls->memory, pargs, &level)) {
                if ((level < 0) || (level > 100))
		    return e_Range;
		pgls->g.fill.param.shading = level;
	    }
	}
	break;

      case hpgl_FT_pattern_RF: /* 11 */
	{
            int     index, mode;

            /* contrary to the documentation, option 2 is used */
	    if (!hpgl_arg_int(pgls->memory, pargs, &index))
                index = pgls->g.fill.param.user_defined.pattern_index;
            else if ((index < 1) || (index > 8))
                return e_Range;
	    if (!hpgl_arg_c_int(pgls->memory, pargs, &mode))
		mode = pgls->g.fill.param.user_defined.use_current_pen;
            else if ((mode & ~1) != 0)
		 return e_Range;
	    pgls->g.fill.param.user_defined.pattern_index = index;
	    pgls->g.fill.param.user_defined.use_current_pen = mode;
	}
	break;

      case hpgl_FT_pattern_cross_hatch: /* 21 */
	{
            int     pattern;

	    if (hpgl_arg_c_int(pgls->memory, pargs, &pattern)) {
                if ((pattern < 1) || (pattern > 6))
		    return e_Range;
		pgls->g.fill.param.pattern_type = pattern;
	    }
	}
	break;

      case hpgl_FT_pattern_user_defined: /* 22 */
	{
            int32   id;

	    if (hpgl_arg_int(pgls->memory, pargs, &id)) {
		if ((id < 0) || (id > 0xffff))
		    return e_Range;
		pgls->g.fill.param.pattern_id = id;
	    }
	}
	break;

      default:
	return e_Range;
    }
    pgls->g.fill.type = (hpgl_FT_pattern_source_t)type;
    return 0;
}

void
hpgl_set_line_attribute_defaults(hpgl_state_t *pgls)
{
    pgls->g.line.cap = 1; /* butt */
    pgls->g.line.join = 1; /* mitered */
    pgls->g.miter_limit = 5;
}
/*
 * LA [kind1,value1[,kind2,value2[,kind3,value3]]];
 */
  int
hpgl_LA(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             cap = pgls->g.line.cap, join = pgls->g.line.join;
    hpgl_real_t     miter_limit = pgls->g.miter_limit;
    bool            no_args=true;
    int             kind;

    while (hpgl_arg_c_int(pgls->memory, pargs, &kind)) {
	no_args=false;
	switch (kind) {

	  case 1:		/* line ends */
	    if ( !hpgl_arg_c_int(pgls->memory, pargs, &cap) || (cap < 1) || (cap > 4) )
		return e_Range;
	    break;

	  case 2:		/* line joins */
	    if ( !hpgl_arg_c_int(pgls->memory, pargs, &join) || (join < 1) || (join > 6) )
		return e_Range;
	    break;

	  case 3:		/* miter limit */
	    if ( !hpgl_arg_c_real(pgls->memory, pargs, &miter_limit) )
		return e_Range;
	    if (miter_limit < 1)
		miter_limit = 1;
	    break;

	  default:
	    return e_Range;
	}
    }

    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    /* LA1,1,2,1,3,5 is the same as LA */
    if (no_args) {
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

static const hpgl_line_type_t  hpgl_fixed_pats[8] = {
    { 2, { 0.0, 1.0 } },
    { 2, { 0.5, 0.5 } },
    { 2, { 0.7, 0.3 } },
    { 4, { 0.8, 0.1, 0.0, 0.1 } },
    { 4, { 0.7, 0.1, 0.1, 0.1 } },
    { 6, { 0.5, 0.1, 0.1, 0.1, 0.1, 0.1 } },
    { 6, { 0.7, 0.1, 0.0, 0.1, 0.0, 0.1 } },
    { 8, { 0.5, 0.1, 0.0, 0.1, 0.1, 0.1, 0.0, 0.1 } }
};

static const hpgl_line_type_t  hpgl_adaptive_pats[8] = {
    { 3, { 0.0, 1.0, 0.0 } },
    { 3, { 0.25, 0.5, 0.25 } },
    { 3, { 0.35, 0.3, 0.35 } },
    { 5, { 0.4, 0.1, 0.0, 0.1, 0.4 } },
    { 5, { 0.35, 0.1, 0.1, 0.1, 0.35 } },
    { 7, { 0.25, 0.1, 0.1, 0.1, 0.1, 0.1, 0.25 } },
    { 7, { 0.35, 0.1, 0.0, 0.1, 0.0, 0.1, 0.35 } },
    { 9, { 0.25, 0.1, 0.0, 0.1, 0.1, 0.1, 0.0, 0.1, 0.25 } }
};

 void
hpgl_set_line_pattern_defaults(hpgl_state_t *pgls)
{
    pgls->g.line.current.pattern_length_relative = 0; /* relative */
    pgls->g.line.current.pattern_length = 4.0; /* % of P1 P2 */
    pgls->g.line.current.is_solid = true;
    pgls->g.line.current.type = 0;
    memcpy( &pgls->g.fixed_line_type,
	    &hpgl_fixed_pats,
	    sizeof(hpgl_fixed_pats)
	    );
    memcpy( &pgls->g.adaptive_line_type,
	    &hpgl_adaptive_pats,
	    sizeof(hpgl_adaptive_pats)
	    );

    /* initialize the current pattern offset - this is not part of the
       command but is used internally to modulate the phase of the
       HPGL/2 vector fills.  NB - needs a new home. */
    pgls->g.line.current.pattern_offset = 0.0;
}

/*
 * LT type[,length[,mode]];
 * LT99;
 * LT;
 * NB - needs reorganizing 
 */
  int
hpgl_LT(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{	
    int             type;

    /* Draw the current path for any LT command irrespective of
       whether or not the the LT changes anything */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    /* no parameter defaults to solid line type - note line type 0 is
       for dots, this is the no parameter default.  We keep the old
       pattern parameters (no update) and save the current pattern
       since type 99 can only be invoked if the current line is solid
       (i.e. the 99 pattern only needs to be saved here */
    if ( !hpgl_arg_c_int(pgls->memory, pargs, &type) ) {
	pgls->g.line.saved = pgls->g.line.current;
        pgls->g.line.saved.pos.x = pgls->g.pos.x;
        pgls->g.line.saved.pos.y = pgls->g.pos.y;
	pgls->g.line.current.is_solid = true;
	return 0;
    }

    /* 99 restores the previous line type if a solid line type is the
       current selection (LT;) LT99 is ignored when a non-solid line
       type is in effect, of course the previous line may have been a
       solid line resulting in nop. */
    if ( type == 99 && pgls->g.line.current.is_solid == true &&
	 pgls->g.line.saved.pos.x == pgls->g.pos.x &&
	 pgls->g.line.saved.pos.y == pgls->g.pos.y ) {
	pgls->g.line.current = pgls->g.line.saved;
	return 0;
    }
	
    /* check line type range */
    if ( type < -8 || type > 8 )
	return e_Range;
    /* Initialize, get and check pattern length and mode.  If the mode
       is relative (0) the units are a % of the distance from P1 to P2
       for the absolute mode units are millimeters */
    {
	/* initialize pattern lengths to current state values */
	hpgl_real_t length = pgls->g.line.current.pattern_length;
	int mode = pgls->g.line.current.pattern_length_relative;

	/* get/check the pattern length and mode */
	if ( hpgl_arg_c_real(pgls->memory, pargs, &length) ) {
	    if ( length <= 0 )
		return e_Range;
	    if ( hpgl_arg_c_int(pgls->memory, pargs, &mode) )
		if ( (mode != 0) && (mode != 1) )
		    return e_Range;
	}

	/* if we are here this is a non-solid line and we should be
           able to set the rest of the line parameter state values.
           NB have not checked if some of these get set if there is a
           range error for pattern length or mode.  An experiment for
           another day... */

	if ( type == 0 ) { 
	    /* Spec says this is for dots, on clj4550 its solid line type!
	     * NB: need multiple device testing as its likely to be device specific.
	     * fts 1435 1451 1830 1833.      */
	    pgls->g.line.current.is_solid = true;
	}
	else
	    pgls->g.line.current.is_solid = false;
        pgls->g.line.current.type = type;
	pgls->g.line.current.pattern_length = length;
	pgls->g.line.current.pattern_length_relative = mode;
	pgls->g.line.current.type = type;
    }
    return 0;
}

/*
 * MC mode[,opcode];
 */
  int
hpgl_MC(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{	
    int             mode = 0, opcode;
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    if (hpgl_arg_c_int(pgls->memory, pargs, &mode) && ((mode & ~1) != 0))
	return e_Range;
    opcode = mode ? 168 : 252;
    if ((mode != 0) && hpgl_arg_c_int(pgls->memory, pargs, &opcode)) {
	if ((opcode < 0) || (opcode > 255)) {
	    pgls->logical_op = 252;
	    return e_Range;
	}
    }
    pgls->logical_op = opcode;
    return 0;
}

/*
 * PP [mode];
 */
  int
hpgl_PP(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             mode = 0;

    if (hpgl_arg_c_int(pgls->memory, pargs, &mode)) {
	if ((mode < 0) || (mode > 1))
	    return e_Range;
    }
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    pgls->pp_mode = mode;
    return 0;
}

/*
 * PW [width[,pen]];
 */
  int
hpgl_PW(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{	
    /*
     * we initialize the parameter to be parsed to either .1 which
     * is a % of the magnitude of P1 P2 or .35 MM WU sets up how
     * it gets interpreted.
     */
    hpgl_real_t     param = pgls->g.pen.width_relative ? .1 : .35;
    hpgl_real_t     width_plu;
    int             pmin = 0;
    int             pmax = pcl_palette_get_num_entries(pgls->ppalet) - 1;
    hpgl_real_t     pf_factor = hpgl_width_scale(pgls);

    /*
     * we maintain the line widths in plotter units, irrespective
     * of current units (WU).
     */
    if (hpgl_arg_c_real(pgls->memory, pargs, &param)) {
        if (hpgl_arg_c_int(pgls->memory, pargs, &pmin)) {
	    if ((pmin < 0) || (pmin > pmax))
		return e_Range;
            pmax = pmin;
        }
    }

    if (param == 0) {
        /* assymetric resolutions aren't documented */
        width_plu =
            inches_2_plu(1.0 / gs_currentdevice(pgls->pgs)->HWResolution[0]);
    } else {
        /* width is maintained in plu only */
        width_plu = ( (pgls->g.pen.width_relative)
                      ? ( (param / 100.0) * hpgl_compute_distance( pgls->g.P1.x,
                                                                   pgls->g.P1.y,
                                                                   pgls->g.P2.x,
                                                                   pgls->g.P2.y
                                                                   ) )
                      : mm_2_plu(param) * pf_factor );
    }

    /*
     * PCLTRM 22-38 metric widths scaled scaled by the ratio of
     * the picture frame to plot size.  Note that we always store
     * the line width in PU not MM.
     */
    if (pgls->g.pen.width_relative)
	width_plu *= pf_factor;

    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    {
	int   i;
	for (i = pmin; i <= pmax; ++i)
            pcl_palette_PW(pgls, i, width_plu);
    }
    return 0;
}

/*
 * RF [index[,width,height,pen...]];
 *
 * Nominally, patterns generated via RF are colored patterns. For backwards
 * compatibility with PCL 5e, however, they may also be uncolored patterns. The
 * determining factor is whether or not all of the entries in the pattern are
 * 0 or 1: if so, the pattern is an uncolored pattern; otherwise it is a colored
 * pattern.
 */
  int
hpgl_RF(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    uint            index, width, height;
    gs_depth_bitmap pixmap;
    int             code = 0;
    bool            is_mask = true;
    byte *          data;

    if (pargs->phase == 0) {

	if (!hpgl_arg_c_int(pgls->memory, pargs, (int *)&index)) {
	    hpgl_default_all_fill_patterns(pgls);
	    return 0;
	}
	if ((index < 1) || (index > 8))
	    return e_Range;

	if (!hpgl_arg_c_int(pgls->memory, pargs, (int *)&width)) {
	    pcl_pattern_RF(index, NULL, pgls);
            return 0;
	}
	if ( (width < 1)                            ||
             (width > 255)                          ||
	     !hpgl_arg_c_int(pgls->memory, pargs, (int *)&height) ||
	     (height < 1)                           ||
             (height > 255)                           )
	      return e_Range;

        /* allocate enough memory for pattern header and data */
        data = gs_alloc_bytes(pgls->memory, width * height, "hpgl raster fill");
        if (data == 0)
	    return e_Memory;

        /*
         * All variables must be saved in globals since the parser
         * the parser reinvokes hpgl_RF() while processing data
         * (hpgl_arg_c_int can execute a longjmp).
         */
	pgls->g.raster_fill.width = width;
	pgls->g.raster_fill.height = height;
        pgls->g.raster_fill.data = data;
        pgls->g.raster_fill.is_mask = is_mask;
	pgls->g.raster_fill.index = index;
        /* set bitmap to 0, as not all pens need be provided */
	memset(data, 0, width * height);
	/* prepare to read the pixel values */
	hpgl_next_phase(pargs);

    } else {
        width = pgls->g.raster_fill.width;
        height = pgls->g.raster_fill.height;
        data = pgls->g.raster_fill.data;
        is_mask = pgls->g.raster_fill.is_mask;
	index = pgls->g.raster_fill.index;
    }

    while ((pargs->phase - 1) < width * height) {
	int     pixel;

	if (!hpgl_arg_c_int(pgls->memory, pargs, &pixel))
	    break;
	if (pixel != 0) {
            data[pargs->phase - 1] = pixel;
            if (pixel != 1)
                is_mask = false;
        }
        hpgl_next_phase(pargs);
    }

    if ( pgls->personality == pcl5e )
	is_mask = true; /* always for a monochrome configuration */
    /* if the pattern is uncolored, collapse it to 1-bit per pixel */
    if (is_mask) {
        int     raster = (width + 7) / 8;
        byte *  mdata = gs_alloc_bytes( pgls->memory,
                                        height * raster,
                                        "hpgl mask raster fill"
                                        );
        byte *  pb1 = data;
        byte *  pb2 = mdata;
        int     i;

        if (mdata == 0) {
            gs_free_object(pgls->memory, data, "hpgl raster fill");
            return e_Memory;
        }

        for (i = 0; i < height; i++) {
            int     mask = 0x80;
            int     outval = 0;
            int     j;

            for (j = 0; j < width; j++) {
                if (*pb1++ != 0)
                    outval |= mask;
                if ((mask >>= 1) == 0) {
                    *pb2++ = outval;
                    outval = 0;
                    mask = 0x80;
		}
            }

            if (mask != 0x80)
                *pb2++ = outval;
        }

        gs_free_object(pgls->memory, data, "hpgl raster fill");
        pixmap.data = mdata;
        pixmap.raster = raster;
        pixmap.pix_depth = 1;

    } else {
        pixmap.data = data;
        pixmap.raster = width;
        pixmap.pix_depth = 8;
    }

    /* set up the pixmap */
    pixmap.size.x = width;
    pixmap.size.y = height;
    pixmap.id = 0;
    pixmap.num_comps = 1;

    if ((code = pcl_pattern_RF(index, &pixmap, pgls)) < 0)
        gs_free_object(pgls->memory, data, "hpgl raster fill");
    pgls->g.raster_fill.data = 0;
    return code;;
}

/*
 * SM [char];
 */
  int
hpgl_SM(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    const byte *    p = pargs->source.ptr;
    const byte *    rlimit = pargs->source.limit;

    for (;;) {
        if (p >= rlimit) {
            pargs->source.ptr = p;
	    return e_NeedData;
	}
	++p;
	if (*p == ' ')
	    continue;		/* ignore initial spaces */
	else if (*p == ';') {
            pgls->g.symbol_mode = 0;
	    break;
	}

	/*
	 * p. 22-44 of the PCL5 manual says that the allowable codes
	 * are 33-58, 60-126, 161 and 254.  This is surely an error:
	 * it must be 161-254.
	 */
	else if ( ((*p >= 33) && (*p <= 126)) || ((*p >= 161) && (*p <= 254)) ) {
	    pgls->g.symbol_mode = *p;
	    return 0;
        } else
	    return e_Range;
    }
    return 0;
}

/* 
 * SP [pen];
 */
  int
hpgl_SP(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             pen = 0;
    /* palette pen numbers are always consecutive integers starting
       with 0 */
    int             max_pen = pcl_palette_get_num_entries(pgls->ppalet) - 1;

    if (hpgl_arg_c_int(pgls->memory, pargs, &pen)) {
        if (pen < 0)
            return e_Range;
	while ( pen > max_pen )
	    pen = pen - max_pen;
    }

    if (pen == pgls->g.pen.selected)
        return 0;

    if ( !pgls->g.polygon_mode )
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    pgls->g.pen.selected = pen;
    return 0;
}

/*
 * SV [type[,option1[,option2]];
 */
/*
 * HAS - this should be redone with a local copy of the screen
 * parameters and need to check if we draw the current path if the
 * command is called with arguments that set the state to the current
 * state
 */
  int
hpgl_SV(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             type = hpgl_SV_pattern_solid_pen;

    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    if (hpgl_arg_c_int(pgls->memory, pargs, &type)) {

	switch (type) {

	  case hpgl_SV_pattern_solid_pen: /* 0 */
            pgls->g.screen.param.shading = 100;
            pgls->g.screen.param.user_defined.pattern_index = 1;
            pgls->g.screen.param.user_defined.use_current_pen = false;
            pgls->g.screen.param.pattern_type = 1;
            pgls->g.screen.param.pattern_id = 0;
	    break;

	  case hpgl_SV_pattern_shade: /* 1 */
	    {
                int     level;

	        if ( !hpgl_arg_c_int(pgls->memory, pargs, &level) ||
                     (level < 0)                    ||
                     (level > 100)                    )
		    return e_Range;
		pgls->g.screen.param.shading = level;
	    }
	    break;

	  case hpgl_SV_pattern_RF:	/* 2 */
	    {
                int     index, mode;

		if (!hpgl_arg_int(pgls->memory, pargs, &index))
                    index = pgls->g.screen.param.user_defined.pattern_index;
                else if ((index < 1) || (index > 8))
                    return e_Range;
		if (!hpgl_arg_c_int(pgls->memory, pargs, &mode))
		    mode = pgls->g.screen.param.user_defined.use_current_pen;
                else if ((mode & ~1) != 0)
		    return e_Range;
		pgls->g.screen.param.user_defined.pattern_index = index;
		pgls->g.screen.param.user_defined.use_current_pen = mode;
	    }
	    break;

	  case hpgl_SV_pattern_cross_hatch: /* 21 */
	    {
                int     pattern;

	        if ( !hpgl_arg_c_int(pgls->memory, pargs, &pattern) ||
		     (pattern < 1)                    ||
                     (pattern > 6)                      )
		    return e_Range;
		pgls->g.screen.param.pattern_type = pattern;
	    }
	    break;

	  case hpgl_SV_pattern_user_defined: /* 22 */
	    {
                int32   id;

	        if (!hpgl_arg_int(pgls->memory, pargs, &id) || (id < 0) || (id > 0xffff))
		    return e_Range;
		pgls->g.screen.param.pattern_id = id;
	    }
	    break;

	  default:
	    return e_Range;
	}
    }

    pgls->g.screen.type = (hpgl_SV_pattern_source_t)type;
    return 0;
}

/*
 * TR [mode];
 *
 * NB: Though termed "source transparency" in HP's documentation, GL/2
 *     transparency concept actually corresponds to pattern transparency in
 *     the PCL sense. GL/2 objects are all logically masks: they have no
 *     background, and thus are unaffected by source transparency.
 *
 *     The GL/2 source and PCL pattern transparency states are, however,
 *     maintained separately. The former affects only GL/2 objects, the
 *     latter only objects generated in PCL.
 */
  int
hpgl_TR(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             mode = 1;

    if (hpgl_arg_c_int(pgls->memory, pargs, &mode) && ((mode & ~1) != 0))
	return e_Range;

    pgls->g.source_transparent = (mode != 0);
    return 0;
}

/*
 * UL [index[,gap1..20];
 */
  int
hpgl_UL(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             index;
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    if (hpgl_arg_c_int(pgls->memory, pargs, &index)) {
	hpgl_real_t gap[20];
        double      total = 0;
        int         i, k;

        if ((index < -8) || (index > 8) || (index == 0))
	    return e_Range;
	for (i = 0; (i < 20) && hpgl_arg_c_real(pgls->memory, pargs, &gap[i]); ++i ) {
            if (gap[i] < 0)
		return e_Range;
	    total += gap[i];
	}
	if (total == 0)
	    return e_Range;

	for (k = 0; k < i; k++)
	    gap[k] /= total;

	{
            hpgl_line_type_t *  fixed_plt = 
                 &pgls->g.fixed_line_type[(index < 0 ? -index : index) - 1];
            hpgl_line_type_t *  adaptive_plt =
		 &pgls->g.adaptive_line_type[(index < 0 ? -index : index) - 1];

	    fixed_plt->count = adaptive_plt->count = i;
	    memcpy(fixed_plt->gap, gap, i * sizeof(hpgl_real_t));
	    memcpy(adaptive_plt->gap, gap, i * sizeof(hpgl_real_t));
	}

    } else {
	hpgl_set_line_pattern_defaults(pgls);
    }
    return 0;
}

/*
 * WU [mode];
 */
  int
hpgl_WU(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             mode = 0;

    if (hpgl_arg_c_int(pgls->memory, pargs, &mode)) {
	if ((mode != 0) && (mode != 1))
	    return e_Range;
    }
    pgls->g.pen.width_relative = mode;
    hpgl_args_setup(pargs);
    hpgl_PW(pargs, pgls);
    return 0;
}

/*
 * Initialization
 */
  static int
pglfill_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *   mem
)
{
    /* Register commands */
    DEFINE_HPGL_COMMANDS(mem)
    HPGL_COMMAND('A', 'C', hpgl_AC, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('F', 'T', hpgl_FT, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('L', 'A', hpgl_LA, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('L', 'T', hpgl_LT, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('M', 'C', hpgl_MC, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('P', 'W', hpgl_PW, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('P', 'P', hpgl_PP, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('R', 'F', hpgl_RF, hpgl_cdf_pcl_rtl_both),		/* + additional I parameters */

    /*
     * SM has special argument parsing, so it must handle skipping
     * in polygon mode itself.`
     */
    HPGL_COMMAND('S', 'M', hpgl_SM, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('S', 'P', hpgl_SP, hpgl_cdf_pcl_rtl_both), 
    HPGL_COMMAND('S', 'V', hpgl_SV, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('T', 'R', hpgl_TR, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('U', 'L', hpgl_UL, hpgl_cdf_pcl_rtl_both),
    HPGL_COMMAND('W', 'U', hpgl_WU, hpgl_cdf_pcl_rtl_both),
    END_HPGL_COMMANDS
    return 0;
}

const pcl_init_t    pglfill_init = { pglfill_do_registration, 0, 0 };
