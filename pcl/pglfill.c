/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pglfill.c - HP-GL/2 line and fill attributes commands */
#include "memory_.h"
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

    if (hpgl_arg_units(pargs, &x)) {
        if (!hpgl_arg_units(pargs, &y))
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
 * FT 11[,index];
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

    hpgl_arg_int(pargs, &type);
    switch (type) {

      case hpgl_FT_pattern_solid_pen1:	/* 1 */
      case hpgl_FT_pattern_solid_pen2: /* 2 */
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

      case hpgl_FT_pattern_one_line:	/* 3 */
	params = &pgls->g.fill.param.hatch;
	goto hatch;

      case hpgl_FT_pattern_two_lines: /* 4 */
	params = &pgls->g.fill.param.crosshatch;
hatch:
	{
            hpgl_real_t spacing = params->spacing;
	    hpgl_real_t angle = params->angle;

	    if (hpgl_arg_real(pargs, &spacing)) {
                if (spacing < 0)
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

      case hpgl_FT_pattern_shading: /* 10 */
	{
            int     level;

	    if (hpgl_arg_c_int(pargs, &level)) {
                if ((level < 0) || (level > 100))
		    return e_Range;
		pgls->g.fill.param.shading = level;
	    }
	}
	break;

      case hpgl_FT_pattern_RF: /* 11 */
	{
            int     index;

	    if (hpgl_arg_c_int(pargs, &index)) {
                if ((index < 1) || (index > 8))
		    return e_Range;
		pgls->g.fill.param.pattern_id = index;
	    }
	}
	break;

      case hpgl_FT_pattern_cross_hatch: /* 21 */
	{
            int     pattern;

	    if (hpgl_arg_c_int(pargs, &pattern)) {
                if ((pattern < 1) || (pattern > 6))
		    return e_Range;
		pgls->g.fill.param.pattern_type = pattern;
	    }
	}
	break;

      case hpgl_FT_pattern_user_defined: /* 22 */
	{
            int32   id;

	    if (hpgl_arg_int(pargs, &id)) {
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

    while (hpgl_arg_c_int(pargs, &kind)) {
	no_args=false;
	switch (kind) {

	  case 1:		/* line ends */
	    if ( !hpgl_arg_c_int(pargs, &cap) || (cap < 1) || (cap > 4) )
		return e_Range;
	    break;

	  case 2:		/* line joins */
	    if ( !hpgl_arg_c_int(pargs, &join) || (join < 1) || (join > 6) )
		return e_Range;
	    break;

	  case 3:		/* miter limit */
	    if ( !hpgl_arg_c_real(pargs, &miter_limit) )
		return e_Range;
	    if (miter_limit < 1)
		miter_limit = 1;
	    break;

	  default:
	    return e_Range;
	}
    }

    /* why is this here as opposed to the start or end? -- jan */
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

private const hpgl_line_type_t  hpgl_fixed_pats[8] = {
    { 2, { 0.0, 1.0 } },
    { 2, { 0.5, 0.5 } },
    { 2, { 0.7, 0.3 } },
    { 4, { 0.8, 0.1, 0.0, 0.1 } },
    { 4, { 0.7, 0.1, 0.1, 0.1 } },
    { 6, { 0.5, 0.1, 0.1, 0.1, 0.1, 0.1 } },
    { 6, { 0.7, 0.1, 0.0, 0.1, 0.0, 0.1 } },
    { 8, { 0.5, 0.1, 0.0, 0.1, 0.1, 0.1, 0.0, 0.1 } }
};

private const hpgl_line_type_t  hpgl_adaptive_pats[8] = {
    { 3, { 0.0, 1.0, 0.0 } },
    { 3, { 0.25, 0.5, 0.25 } },
    { 3, { 0.35, 0.3, 0.35 } },
    { 5, { 0.4, 0.1, 0.0, 0.1, 0.4 } },
    { 5, { 0.35, 0.1, 0.1, 0.1, 0.35 } },
    { 7, { 0.25, 0.1, 0.1, 0.1, 0.1, 0.1, 0.25 } },
    { 7, { 0.35, 0.1, 0.0, 0.1, 0.0, 0.1, 0.35 } },
    { 9, { 0.25, 0.1, 0.0, 0.1, 0.1, 0.1, 0.0, 0.1, 0.25 } }
};

/*
 * LT type[,length[,mode]];
 * LT99;
 * LT;
 */
  int
hpgl_LT(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{	
    int             type = 0;

    /*
     * Draw the current path for any LT command irrespective of whether
     * or not the the LT changes anything
     */
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));

    if (hpgl_arg_c_int(pargs, &type)) {

	/*
         * restore old saved line if we have a solid line,
         * otherwise the instruction is ignored.
         */
	if (type == 99) {
	    if (pgls->g.line.current.is_solid == true)
		pgls->g.line.current = pgls->g.line.saved;
	    return 0;
        } else {	
	    hpgl_real_t length = 0.04;
	    int         mode = 0;

	    if ( (type < -8)                                                  ||
                 (type > 8)                                                   ||
		 ( hpgl_arg_c_real(pargs, &length)                         &&
		   ((length <= 0)                                       || 
                    (hpgl_arg_c_int(pargs, &mode) && ((mode & ~1) != 0))  )  )  )
		  return e_Range;
	    pgls->g.line.current.pattern_length = length;
	    pgls->g.line.current.pattern_length_relative = (mode == 0);
	    pgls->g.line.current.is_solid = false;
	}
    } else {
	/* no args restore defaults and set previous line type. */
	/*
         * HAS **BUG**BUG** initial value of line type
         * is not supplied unless it gets set by previous LT command.
         */
	pgls->g.line.saved = pgls->g.line.current;
	pgls->g.line.current.is_solid = true;
	memcpy( &pgls->g.fixed_line_type,
                &hpgl_fixed_pats,
                sizeof(hpgl_fixed_pats)
                );
	memcpy( &pgls->g.adaptive_line_type,
		&hpgl_adaptive_pats,
		sizeof(hpgl_adaptive_pats)
                );
    }

    pgls->g.line.current.type = type;
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

    if (hpgl_arg_c_int(pargs, &mode) && ((mode & ~1) != 0))
	return e_Range;
    opcode = mode ? 168 : 252;
    if ((mode != 0) && hpgl_arg_c_int(pargs, &opcode)) {
	if ((opcode < 0) || (opcode > 255)) {
	    pgls->logical_op = 252;
	    return e_Range;
	}
    }
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
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

    if (hpgl_arg_c_int(pargs, &mode)) {
	if ((mode < 0) || (mode > 1))
	    return e_Range;
    }
    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
    pgls->grid_adjust = (mode == 0) ? 0.5 : 0.0;
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

    /*
     * we maintain the line widths in plotter units, irrespective
     * of current units (WU).  They width argument to PW are mm
     * (I suspect this is incorrect - jan)
     */
    if (hpgl_arg_c_real(pargs, &param)) {
        if (hpgl_arg_c_int(pargs, &pmin)) {
	    if ((pmin < 0) || (pmin > pmax))
		return e_Range;
            pmax = pmin;
        }
    }

    /* width is maintained in plu only */
    width_plu = ( (pgls->g.pen.width_relative)
                    ? ( (param / 100.0) * hpgl_compute_distance( pgls->g.P1.x,
                                                                 pgls->g.P1.y,
							         pgls->g.P2.x,
                                                                 pgls->g.P2.y
                                                                 ) )
                    : mm_2_plu(param) );

    /*
     * PCLTRM 22-38 metric widths scaled scaled by the ratio of
     * the picture frame to plot size.  Note that we always store
     * the line width in PU not MM.
     */
    if (pgls->g.pen.width_relative)
	width_plu *= min( (hpgl_real_t)pgls->g.picture_frame_height
                             / (hpgl_real_t)pgls->g.plot_height,
			  (hpgl_real_t)pgls->g.picture_frame_width
                             / (hpgl_real_t)pgls->g.plot_width
                          );
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
    byte *          data;

    if (pargs->phase == 0) {

	if (!hpgl_arg_c_int(pargs, (int *)&index)) {
	    hpgl_default_all_fill_patterns(pgls);
	    return 0;
	}
	if ((index < 1) || (index > 8))
	    return e_Range;

	if (!hpgl_arg_c_int(pargs, (int *)&width)) {
	    pcl_pattern_RF(index, NULL, pgls->memory);
            return 0;
	}
	if ( (width < 1)                            ||
             (width > 255)                          ||
	     !hpgl_arg_c_int(pargs, (int *)&height) ||
	     (height < 1)                           ||
             (height > 255)                           )
	      return e_Range;

        /* allocate enough memory for pattern header and data */
        data = gs_alloc_bytes(pgls->memory, width * height, "hpgl raster fill");
        if (data == 0)
	    return e_Memory;

        /*
         * All variables must be saved in globals since the parser
         * the parser reinvokes hpgl_RF() while processing data.
         * (is this really the case?? - jan)
         */
	pgls->g.raster_fill.width = width;
	pgls->g.raster_fill.height = height;
        pgls->g.raster_fill.data = data;

        /* set bitmap to 0, as not all pens need be provided */
	memset(data, 0, width * height);
	pargs->phase = 1;

    } else {
        width = pgls->g.raster_fill.width;
        height = pgls->g.raster_fill.height;
        data = pgls->g.raster_fill.data;
    }

    while ((pargs->phase - 1) < width * height) {
	int     pixel;

	if (!hpgl_arg_c_int(pargs, &pixel))
	    break;
	if (pixel != 0)
            data[pargs->phase - 1] = pixel;
        hpgl_next_phase(pargs);
    }

    /* set up the pixmap */
    pixmap.data = data;
    pixmap.raster = pgls->g.raster_fill.width;
    pixmap.size.x = pgls->g.raster_fill.width;
    pixmap.size.y = pgls->g.raster_fill.height;
    pixmap.id = 0;
    pixmap.pix_depth = 8;
    pixmap.num_comps = 1;

    if ((code = pcl_pattern_RF(index, &pixmap, pgls->memory)) < 0)
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
    int             npen = pcl_palette_get_num_entries(pgls->ppalet);

    if (hpgl_arg_c_int(pargs, &pen)) {
        if (pen < 0)
            return e_Range;
        else if (pen > npen)
            pen = (pen % (npen - 1)) + 1;
    }
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
    if (hpgl_arg_c_int(pargs, &type)) {

	switch (type) {

	  case hpgl_SV_pattern_solid_pen: /* 0 */
	    break;

	  case hpgl_SV_pattern_shade: /* 1 */
	    {
                int     level;

	        if ( !hpgl_arg_c_int(pargs, &level) ||
                     (level < 0)                    ||
                     (level > 100)                    )
		    return e_Range;
		pgls->g.screen.param.shading = level;
	    }
	    break;

	  case hpgl_SV_pattern_RF:	/* 2 */
	    {
                int     index, mode;

		if ( !hpgl_arg_int(pargs, &index)  ||
                     (index < 1)                   ||
                     (index > 8)                   ||
		     !hpgl_arg_c_int(pargs, &mode) ||
                     ((mode & ~1) != 0)              )
		    return e_Range;
		pgls->g.screen.param.user_defined.pattern_index = index;
		pgls->g.screen.param.user_defined.use_current_pen = mode;
	    }
	    break;

	  case hpgl_SV_pattern_cross_hatch: /* 21 */
	    {
                int     pattern;

	        if ( !hpgl_arg_c_int(pargs, &pattern) ||
		     (pattern < 1)                    ||
                     (pattern > 6)                      )
		    return e_Range;
		pgls->g.screen.param.pattern_type = pattern;
	    }
	    break;

	  case hpgl_SV_pattern_user_defined: /* 22 */
	    {
                int32   id;

	        if (!hpgl_arg_int(pargs, &id) || (id < 0) || (id > 0xffff))
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
 */
  int
hpgl_TR(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{
    int             mode = 1;

    if (hpgl_arg_c_int(pargs, &mode) && ((mode & ~1) != 0))
	return e_Range;

    /* HAS not sure if pcl's state is updated as well */
    pgls->g.source_transparent = mode;
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

    if (hpgl_arg_c_int(pargs, &index)) {
	hpgl_real_t gap[20];
        double      total = 0;
        int         i, k;

        if ((index < -8) || (index > 8) || (index == 0))
	    return e_Range;
	for (i = 0; (i < 20) && hpgl_arg_c_real(pargs, &gap[i]); ++i ) {
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
	/* no args same as LT; */
	hpgl_args_setup(pargs);
	hpgl_LT(pargs, pgls);
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

    if (hpgl_arg_c_int(pargs, &mode)) {
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
  private int
pglfill_do_init(
    gs_memory_t *   mem
)
{
    /* Register commands */
    DEFINE_HPGL_COMMANDS
    HPGL_COMMAND('A', 'C', hpgl_AC, 0),
    HPGL_COMMAND('F', 'T', hpgl_FT, 0),
    HPGL_COMMAND('L', 'A', hpgl_LA, 0),
    HPGL_COMMAND('L', 'T', hpgl_LT, 0),
    HPGL_COMMAND('M', 'C', hpgl_MC, 0),
    HPGL_COMMAND('P', 'W', hpgl_PW, 0),
    HPGL_COMMAND('P', 'P', hpgl_PP, 0),
    HPGL_COMMAND('R', 'F', hpgl_RF, 0),		/* + additional I parameters */

    /*
     * SM has special argument parsing, so it must handle skipping
     * in polygon mode itself.`
     */
    HPGL_COMMAND('S', 'M', hpgl_SM, 0),
    HPGL_COMMAND('S', 'P', hpgl_SP, 0),
    HPGL_COMMAND('S', 'V', hpgl_SV, 0),
    HPGL_COMMAND('T', 'R', hpgl_TR, 0),
    HPGL_COMMAND('U', 'L', hpgl_UL, 0),
    HPGL_COMMAND('W', 'U', hpgl_WU, 0),
    END_HPGL_COMMANDS
    return 0;
}

const pcl_init_t    pglfill_init = { pglfill_do_init, 0, 0 };
