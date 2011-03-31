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

/* pgconfig.c */
/* HP-GL/2 configuration and status commands */
#include "gx.h"
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"            /* for gscoord.h */
#include "gscoord.h"
#include "pgmand.h"
#include "pcparse.h"
#include "pgdraw.h"
#include "pginit.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcursor.h"
#include "pcpage.h"
#include "pcpalet.h"
#include "pcdraw.h"

/* contrary to the documentation HP also seems to parse CO.*; as a
   legitimate comment as well as a string enclosed in quotation marks.  */
int
hpgl_CO(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;

	while ( p < rlimit ) {
	    if ( !pargs->phase ) {
		switch ( *++p ) {
		case ' ':
		    /* Ignore spaces between command and opening ". */
		    continue;
		case '"':
		    pargs->phase = 1;
		    break;
		default:
		    /* Search for semicolon */
		    pargs->phase = 2;
		    break;

		}
	    } else {
		/* Scanning for closing " or closing ';' */
		switch ( pargs->phase ) {
		case 1:
		    if ( *++p == '"' ) {
			pargs->source.ptr = p;
			return 0;
		    }
		    /* syntax error on some hp devices */
		    if ( *p == '\\' ) {
			pargs->source.ptr = p;
			return 0;
		    }
		    break;
		case 2:
		    if ( *++p == ';' ) {
			pargs->source.ptr = p;
			return 0;
		    }
		    break;
		default:
		    dprintf("HPGL CO automata is in an unknown state\n" );
		    pargs->source.ptr = p;
		    return 0;
		}
	    }
	}
	pargs->source.ptr = p;
	return e_NeedData;
}

#ifdef DEBUG

/* debug hpgl/2 operator equivalent to using -Z on the command line.
   This is useful for systems that don't have access to the debugging
   command line options.  Sets debug flags until a terminating ";" is
   received.  GL/2 example: IN;SP1;ZZPB; is equivalent to running the
   interpreter with the option -ZPB which turns on debug trace for
   paths and bitmaps */

int
hpgl_ZZ(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
    const byte *p = pargs->source.ptr;
    const byte *rlimit = pargs->source.limit;
    while ( p < rlimit ) {
	byte ch = *++p;
	/* ; terminates the command */
	if ( ch == ';' ) {
	    pargs->source.ptr = p;
	    return 0;
	}
	else {
	    gs_debug[(int)ch] = 1;
	}
    }
    pargs->source.ptr = p;
    return e_NeedData;
}
#endif
/* The part of the DF command applicable for overlay macros */
int
hpgl_reset_overlay(hpgl_state_t *pgls)
{	hpgl_args_t args;
	hpgl_args_setup(&args);
	hpgl_AC(&args, pgls);
	hpgl_args_setup(&args);
	pgls->g.font_selected = 0;
	hpgl_AD(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_SD(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_CF(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_args_add_int(&args, 1);
	hpgl_args_add_int(&args, 0);
	hpgl_DI(&args, pgls);
	/* HAS -- Figure out some way to do this so that it is consistant */
	pgls->g.label.terminator = 3;
	pgls->g.label.print_terminator = false;
	hpgl_args_setup(&args);
	hpgl_DV(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_ES(&args, pgls);
	pgls->g.label.write_vertical = false;
	pgls->g.label.double_byte = false;
	hpgl_args_setup(&args);
	hpgl_LM(&args, pgls);
	hpgl_args_set_int(&args, 1);
	hpgl_LO(&args, pgls);
	/* we do this instead of calling SC directly */
        if ( pgls->g.scaling_type != hpgl_scaling_none ) {
            gs_point dpt, pt; /* device point and user point */
            hpgl_call(hpgl_get_current_position(pgls, &pt));
            hpgl_call(gs_transform(pgls->pgs, pt.x, pt.y, &dpt));
            pgls->g.scaling_type = hpgl_scaling_none;
            hpgl_call(hpgl_set_ctm(pgls));
            hpgl_call(gs_itransform(pgls->pgs, dpt.x, dpt.y, &pt));
            hpgl_call(hpgl_set_current_position(pgls, &pt));
        }
	pgls->g.fill_type = hpgl_even_odd_rule;
	hpgl_args_set_int(&args,0);
	hpgl_PM(&args, pgls);
	hpgl_args_set_int(&args,2);
	hpgl_PM(&args, pgls);
	pgls->g.bitmap_fonts_allowed = 0;
	hpgl_args_setup(&args);
	hpgl_SI(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_SL(&args, pgls);
	/* We initialize symbol mode directly because hpgl_SM parses
           its argument differently than most other commands */
	pgls->g.symbol_mode = 0;
	hpgl_args_setup(&args);
	hpgl_SS(&args, pgls);
	hpgl_args_set_int(&args,1);
	hpgl_TR(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_TD(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_MC(&args, pgls);
#ifdef LJ6_COMPAT
	/* LJ6 seems to reset PP with an IN command the Color Laserjet
           does not.  NB this needs to be handled with dynamic
           configuration */
	hpgl_args_setup(&args);
	hpgl_PP(&args, pgls);
#endif
        return 0;
}

/* DF; sets programmable features except P1 and P2 */
int
hpgl_DF(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
    hpgl_args_t args;
    hpgl_call(hpgl_reset_overlay(pgls));

    hpgl_args_setup(&args);
    hpgl_FT(&args, pgls);
    hpgl_args_setup(&args);
    hpgl_IW(&args, pgls);
    hpgl_set_line_attribute_defaults(pgls);
    hpgl_args_setup(&args);
    hpgl_LA(&args, pgls);
    hpgl_set_line_pattern_defaults(pgls);
    hpgl_args_setup(&args);
    hpgl_RF(&args, pgls);
    hpgl_args_set_int(&args, 0);
    hpgl_SV(&args, pgls);
    hpgl_args_setup(&args);
    hpgl_UL(&args, pgls);
    hpgl_args_setup(&args);
    hpgl_SB(&args, pgls);
    return 0;
}

/*
 * The "implicit" portion of the IN command.
 *
 * With the advent of PCL 5c, both PCL and GL want to reset the current
 * palette. The difficulty is that they want to reset it to different things.
 *
 * The proper way to handle this would be to implement IN as a reset type,
 * create a single palette reset routine, and have it do different things
 * depending on the nature of the reset.
 *
 * At the time this comment was written, such a change was larger than could
 * be easily accommodated. Hence, a less drastic alternative was employed:
 * split the IN command into implicit and explicit portions, and only use the
 * latter when the IN command is explicitly invoked.
 */
  int
hpgl_IN_implicit(
    hpgl_state_t *  pgls\
)
{
    hpgl_args_t     args;

    /* cancel rotation */
    pgls->g.rotation = 0;
    /* restore defaults */
    hpgl_DF(&args, pgls);

    /* if in RTL mode provided initial values for PS */
    if ( pgls->personality == rtl ) {
        hpgl_args_setup(&args);
        hpgl_PS(&args, pgls);
    }
            
    /* defaults P1 and P2 */
    hpgl_args_setup(&args);
    hpgl_IP(&args, pgls);

    /* pen width units - metric, also resets pen widths.   */
    hpgl_args_setup(&args);
    hpgl_WU(&args, pgls);


    /*
     * pen up-absolute position and set gl/2 current positon to
     * 0,0 or the lower left of the picture frame.  Simply sets
     * the gl/2 state, we subsequently clear the path because we
     * do not want to create a live gs path.
     */
    hpgl_args_set_real2(&args, 0.0, 0.0);
    hpgl_PU(&args, pgls);
    hpgl_args_set_real2(&args, 0.0, 0.0);
    hpgl_PA(&args, pgls);
    hpgl_call(hpgl_clear_current_path(pgls));

    return 0;
}

/*
 * IN = DF (see below)
 */
  int
hpgl_IN(
    hpgl_args_t *   pargs,
    hpgl_state_t *  pgls
)
{	
    int             code = 0;
    hpgl_args_t     args;

    /* handle the work or an implicit reset */
    code = hpgl_IN_implicit(pgls);

    /* set up the default palette (8 entries, not-fixed) */
    if (code == 0)
        code = pcl_palette_IN(pgls);

    /* default color range */
    hpgl_args_setup(&args);
    hpgl_CR(&args, pgls);

    /* pen width units - metric, also reset pen widths.  This is also
       done in hpgl_IN_implicit() above but we have to set the pen
       widths again in the case a new palette was created.  The
       default width values in a fresh palette do not account for
       scaling effects of the hpgl/2 picture frame. */
    hpgl_args_setup(&args);
    hpgl_WU(&args, pgls);

    return code;
}

/* derive the current picture frame coordinates */

static int
hpgl_picture_frame_coords(hpgl_state_t *pgls, gs_int_rect *gl2_win)
{
	gs_rect dev_win; /* device window */
	hpgl_real_t x1 = pgls->g.picture_frame.anchor_point.x;
	hpgl_real_t y1 = pgls->g.picture_frame.anchor_point.y;
	hpgl_real_t x2 = x1 + pgls->g.picture_frame_width;
	hpgl_real_t y2 = y1 + pgls->g.picture_frame_height;

	pcl_set_ctm(pgls, false);
	hpgl_call(gs_transform(pgls->pgs, x1, y1, &dev_win.p));
	hpgl_call(gs_transform(pgls->pgs, x2, y2, &dev_win.q));
	hpgl_call(hpgl_set_plu_ctm(pgls));
	/*
	 * gs_bbox_transform_inverse puts the resulting points in the
	 * correct order, with p < q.
	 */
	{ gs_matrix mat;
	  gs_rect pcl_win; /* pcl window */

	  gs_currentmatrix(pgls->pgs, &mat);
	  hpgl_call(gs_bbox_transform_inverse(&dev_win, &mat, &pcl_win));
/* Round all coordinates to the nearest integer. */
#define set_round(e) gl2_win->e = (int)floor(pcl_win.e + 0.5)
	  set_round(p.x);
	  set_round(p.y);
	  set_round(q.x);
	  set_round(q.y);
#undef set_round
	}
	/* restore the ctm */
	hpgl_call(hpgl_set_ctm(pgls));
	return 0;
}

/* IP p1x,p1y[,p2x,p2y]; */
/* IP; */
int
hpgl_IP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int32 ptxy[4];
	int i;
	gs_int_rect win;

	/* draw the current path */
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	/* get the default picture frame coordinates */
	hpgl_call(hpgl_picture_frame_coords(pgls, &win));

	/* round the picture frame coordinates */
	ptxy[0] = win.p.x; ptxy[1] = win.p.y;
	ptxy[2] = win.q.x; ptxy[3] = win.q.y;
	for ( i = 0; i < 4 && hpgl_arg_int(pgls->memory, pargs, &ptxy[i]); ++i )
	  ;
	if ( i & 1 )
	  return e_Range;

	if ( i == 2 )
	  {
	    pgls->g.P2.x = (ptxy[0] - pgls->g.P1.x) +
	      pgls->g.P2.x;
	    pgls->g.P2.y = (ptxy[1] - pgls->g.P1.y) +
	      pgls->g.P2.y;
	    pgls->g.P1.x = ptxy[0];
	    pgls->g.P1.y = ptxy[1];
	  }
	else
	  {
	    pgls->g.P1.x = ptxy[0];
	    pgls->g.P1.y = ptxy[1];
	    pgls->g.P2.x = ptxy[2];
	    pgls->g.P2.y = ptxy[3];
	  }

	/* if either coordinate is equal it is incremented by 1 */
	if ( pgls->g.P1.x == pgls->g.P2.x ) pgls->g.P2.x++;
	if ( pgls->g.P1.y == pgls->g.P2.y ) pgls->g.P2.y++;
	
	return 0;
}

/* IR r1x,r1y[,r2x,r2y]; */
/* IR; */
int
hpgl_IR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t rptxy[4];
	int i;
	hpgl_args_t args;
	gs_int_rect win;

	for ( i = 0; i < 4 && hpgl_arg_c_real(pgls->memory, pargs, &rptxy[i]); ++i )
	  ;
	if ( i & 1 )
	  return e_Range;

	/* get the PCL picture frame coordinates */
	hpgl_call(hpgl_picture_frame_coords(pgls, &win));
	hpgl_args_setup(&args);
        if ( i != 0 )
          {
            hpgl_args_add_int(&args, win.p.x + (win.q.x - win.p.x) *
                              rptxy[0] / 100.0);
            hpgl_args_add_int(&args, win.p.y + (win.q.y - win.p.y) *
                              rptxy[1] / 100.0);
          }
	if ( i == 4 )
	  {
	    hpgl_args_add_int(&args, win.p.x + (win.q.x - win.p.x) *
			      rptxy[2] / 100.0);

	    hpgl_args_add_int(&args, win.p.y + (win.q.y - win.p.y) *
			      rptxy[3] / 100.0);
	  }
	hpgl_IP( &args, pgls );
	return 0;
}

/* IW llx,lly,urx,ury; */
/* IW; */
int
hpgl_IW(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t wxy[4];
	int i;
	gs_int_rect win;

	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	/* get the default picture frame coordinates. */
	hpgl_call(hpgl_picture_frame_coords(pgls, &win));
	wxy[0] = win.p.x;
	wxy[1] = win.p.y;
	wxy[2] = win.q.x;
	wxy[3] = win.q.y;
	for ( i = 0; i < 4 && hpgl_arg_units(pgls->memory, pargs, &wxy[i]); ++i )
	  ;
	if ( i & 3 )
	  return e_Range;
	
	/* no args case disables the soft clip window */
	if ( i == 0 ) {
	  pgls->g.soft_clip_window.active = false;
          pgls->g.soft_clip_window.isbound = false;
	  return 0;
	}
	/* HAS needs error checking */
	pgls->g.soft_clip_window.rect.p.x = wxy[0];
	pgls->g.soft_clip_window.rect.p.y = wxy[1];
	pgls->g.soft_clip_window.rect.q.x = wxy[2];
	pgls->g.soft_clip_window.rect.q.y = wxy[3];
	pgls->g.soft_clip_window.active = true;
	return 0;
}

/* PG; */
int
hpgl_PG(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
    if ( pgls->personality == rtl ) {
	int dummy;
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	/* with parameter always feed, without parameter feed if marked */
	if ( pcl_page_marked(pgls) || hpgl_arg_c_int(pgls->memory, pargs, &dummy) )
	    hpgl_call(pcl_do_FF(pgls));
    }
    return 0;
}

/* enable cutter - not supported */
int
hpgl_EC(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
    return e_Unimplemented;
}

/* PS;  NB this is only a partial implementation. */
int
hpgl_PS(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
    hpgl_real_t page_dims[2];
    /* we use the pcl paper handling machinery to set the plot size */
    pcl_paper_size_t paper;
    int i;

    if ( pgls->personality != rtl )
	return 0;

    /* PS return an error if the page is dirty */
    if ( pcl_page_marked(pgls) )
        return e_Range;
    
    /* check for pjl override of the arguments - this is custom code
       for a customer and is not the normal interaction between PCL &
       PJL */
    if (!pjl_proc_compare(pgls->pjls, pjl_proc_get_envvar(pgls->pjls, "plotsizeoverride"), "on")) {
        page_dims[0] = pjl_proc_vartof(pgls->pjls, pjl_proc_get_envvar(pgls->pjls, "plotsize1"));
        page_dims[1] = pjl_proc_vartof(pgls->pjls, pjl_proc_get_envvar(pgls->pjls, "plotsize2"));
    } else {
        for ( i = 0; i < 2 && hpgl_arg_real(pgls->memory, pargs, &page_dims[i]); ++i )
            ; /* NOTHING */
        if ( i == 1 )
            page_dims[1] = page_dims[0];
        else if ( i != 2 )
            return e_Range;
    }
    paper.height = plu_2_coord(page_dims[0]);
    paper.width = plu_2_coord(page_dims[1]);
    paper.offset_portrait = 0; 
    paper.offset_landscape = 0;
    new_logical_page(pgls, 0, &paper, false, false);
    return 0;
}

/* RO angle; */
/* RO; */
int
hpgl_RO(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	int angle=0;
	gs_point point, dev_pt;

	/* this business is used by both SC and RO -- perhaps it needs
           a new home */
	hpgl_call(hpgl_set_ctm(pgls));
	hpgl_call(hpgl_get_current_position(pgls, &point));
	hpgl_call(gs_transform(pgls->pgs, point.x, point.y, &dev_pt));

	if ( hpgl_arg_c_int(pgls->memory, pargs, &angle) )
	    switch ( angle )
	      {
	      case 0: case 90: case 180: case 270:
		break;
	      default:
		return e_Range;
	      }

        if ( angle != pgls->g.rotation )
	  {
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	    pgls->g.rotation = angle;
	    hpgl_call(hpgl_set_ctm(pgls));
	    hpgl_call(gs_itransform(pgls->pgs, dev_pt.x, dev_pt.y, &point));
	    hpgl_call(hpgl_set_current_position(pgls, &point));
	    hpgl_call(hpgl_update_carriage_return_pos(pgls));
	  }
	return 0;
}

/* RP; */
int
hpgl_RP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return e_Unimplemented;
}

/* SC xmin,xmax,ymin,ymax[,type=0]; */
/* SC xmin,xmax,ymin,ymax,type=1[,left,bottom]; */
/* SC xmin,xfactor,ymin,yfactor,type=2; */
/* SC; */
int
hpgl_SC(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t xy[4];
	int i;
	int type;
	hpgl_scaling_params_t scale_params;
	gs_point point, dev_pt, dev_anchor;

	scale_params = pgls->g.scaling_params;
	hpgl_call(hpgl_get_current_position(pgls, &point));
	hpgl_call(gs_transform(pgls->pgs, point.x, point.y, &dev_pt));
	hpgl_call(gs_transform(pgls->pgs, pgls->g.anchor_corner.x, 
			       pgls->g.anchor_corner.y, &dev_anchor));
	for ( i = 0; i < 4 && hpgl_arg_real(pgls->memory, pargs, &xy[i]); ++i )
	  ;
	switch ( i )
	  {
	  case 0:		/* set defaults */
              {
                  /* a naked SC implies the soft clip window is bound
                     to plotter units.  */
                  gs_matrix umat;
                  type = hpgl_scaling_none;
                  hpgl_compute_user_units_to_plu_ctm(pgls, &umat);
                  /* in-place */
                  hpgl_call(gs_bbox_transform(&pgls->g.soft_clip_window.rect,
                                              &umat,
                                              &pgls->g.soft_clip_window.rect));
                  pgls->g.soft_clip_window.isbound = true;
                  break;
              }
	  default:
	    return e_Range;
	  case 4:
	    type = hpgl_scaling_anisotropic;
	    hpgl_arg_c_int(pgls->memory, pargs, &type);
	    switch ( type )
	      {
	      case hpgl_scaling_anisotropic: /* 0 */
		if ( xy[0] == xy[1] || xy[2] == xy[3] )
		  return e_Range;
pxy:		scale_params.pmin.x = xy[0];
		scale_params.pmax.x = xy[1];
		scale_params.pmin.y = xy[2];
		scale_params.pmax.y = xy[3];
		break;
	      case hpgl_scaling_isotropic: /* 1 */
		if ( xy[0] == xy[1] || xy[2] == xy[3] )
		  return e_Range;
		{ hpgl_real_t left = 50, bottom = 50;
		  if ( (hpgl_arg_c_real(pgls->memory, pargs, &left) &&
			(left < 0 || left > 100 ||
			 !hpgl_arg_c_real(pgls->memory, pargs, &bottom) ||
			 bottom < 0 || bottom > 100))
		     )
		    return e_Range;
		  scale_params.left = left;
		  scale_params.bottom = bottom;
		}
		goto pxy;
	      case hpgl_scaling_point_factor: /* 2 */
		if ( xy[1] == 0 || xy[3] == 0 )
		  return e_Range;
		scale_params.pmin.x = xy[0];
		scale_params.factor.x = xy[1];
		scale_params.pmin.y = xy[2];
		scale_params.factor.y = xy[3];
		break;
	      default:
		return e_Range;
	      }
	  }
	hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	pgls->g.scaling_params = scale_params;
	pgls->g.scaling_type = type;
	hpgl_call(hpgl_set_ctm(pgls));
	hpgl_call(gs_itransform(pgls->pgs, dev_pt.x, dev_pt.y, &point));
	hpgl_call(hpgl_set_current_position(pgls, &point));
	hpgl_call(gs_itransform(pgls->pgs, dev_anchor.x, dev_anchor.y, 
				&pgls->g.anchor_corner));

	/* PCLTRM 23-7 (commands the update cr position) does not list
           SC but PCL updates the position */
	hpgl_call(hpgl_update_carriage_return_pos(pgls));
	return 0;
}

/* BP - Begin Plot 
*/
static int
hpgl_BP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
    hpgl_args_t args;
    int32 command = 0;
    int32 value = 0;
    bool more = true;


    while (more) {
        more = hpgl_arg_int(pgls->memory, pargs, &command);
	if (!more) 
	    break;
	if (command == 1) {
	    /* parse string */ 
	    const byte *p = pargs->source.ptr;
	    const byte *rlimit = pargs->source.limit;	    
	    while ( p < rlimit ) {
		switch ( *++p ) {
		case ' ':
		    /* Ignore spaces between command and opening ". */
		    continue;	
		case '"':
		    if ( !pargs->phase ) {
			/* begin string */
			pargs->phase = 1;
			continue;
		    }
		    else /* end string */
			break;
		default:
		    if ( !pargs->phase ) 
			break; 			/* ill formed command exit */
		    else
			continue;               /* character inside of string */
		}
		break;  /* error or trailing " exits */
	    }
	    pargs->source.ptr = p;
	} 
	else {
	    more = hpgl_arg_int(pgls->memory, pargs, &value);
	    /* BP command value pair is currently ignored */
	}
    }

    hpgl_args_setup(&args);
    hpgl_IN(&args, pgls);
    return 0;
}

/* Initialization */
static int
pgconfig_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{		/* Register commands */
    DEFINE_HPGL_COMMANDS(mem)
	  /* CO has special argument parsing, so it must handle skipping */
	  /* in polygon mode itself. */
	  HPGL_COMMAND('C', 'O', hpgl_CO, hpgl_cdf_polygon | hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('D', 'F', hpgl_DF, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('I', 'N', hpgl_IN, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('I', 'P', hpgl_IP, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('I', 'R', hpgl_IR, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('I', 'W', hpgl_IW, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('P', 'G', hpgl_PG, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('P', 'S', hpgl_PS, hpgl_cdf_rtl),
	  HPGL_COMMAND('E','C',  hpgl_EC, hpgl_cdf_rtl),
	  HPGL_COMMAND('R', 'O', hpgl_RO, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('R', 'P', hpgl_RP, hpgl_cdf_rtl),
	  HPGL_COMMAND('S', 'C', hpgl_SC, hpgl_cdf_pcl_rtl_both),
	  HPGL_COMMAND('B', 'P', hpgl_BP, hpgl_cdf_pcl_rtl_both),
#ifdef DEBUG
	  HPGL_COMMAND('Z', 'Z', hpgl_ZZ, hpgl_cdf_pcl_rtl_both),
#endif
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgconfig_init = {
  pgconfig_do_registration, 0
};
