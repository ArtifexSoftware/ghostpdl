/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgconfig.c */
/* HP-GL/2 configuration and status commands */
#include "std.h"
#include "gstypes.h"		/* for gsstate.h */
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"            /* for gscoord.h */
#include "gscoord.h"
#include "pgmand.h"
#include "pgdraw.h"
#include "pginit.h"
#include "pggeom.h"
#include "pgmisc.h"
#include "pcdraw.h"

/* CO"text" */
int
hpgl_CO(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	const byte *p = pargs->source.ptr;
	const byte *rlimit = pargs->source.limit;

	while ( p < rlimit )
	  if ( !pargs->phase )
	    { /* Scanning for opening " */
	      switch ( *++p )
		{
		case ' ':
		  /* Ignore spaces between command and opening ". */
		  continue;
		case '"':
		  pargs->phase++;
		  break;
		default:
		  /* Bad syntax, just terminate the command. */
		  --p;
		  return 0;
		}
	    }
	  else
	    { /* Scanning for closing " */
	      if ( *++p == '"' )
		return 0;
	    }
	return e_NeedData;
}

/* DF; sets programmable features except P1 and P2 */
int
hpgl_DF(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_args_t args;
	hpgl_args_setup(&args);
	hpgl_AC(&args, pgls);
	hpgl_args_setup(&args);
	hpgl_AD(&args, pgls);
	
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
	
	hpgl_args_setup(&args);
	hpgl_FT(&args, pgls);
	
	hpgl_args_setup(&args);
	hpgl_IW(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_LA(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_LM(&args, pgls);

	hpgl_args_set_int(&args, 1);
	hpgl_LO(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_LT(&args, pgls);

	/* we do this instead of calling SC directly */
	pgls->g.scaling_type = hpgl_scaling_none;

	hpgl_args_set_int(&args,0);
	hpgl_PM(&args, pgls);

	hpgl_args_set_int(&args,2);
	hpgl_PM(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_RF(&args, pgls);

	hpgl_args_set_int(&args,0);
	hpgl_SB(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_SV(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_SD(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_SI(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_SL(&args, pgls);

	/* HAS needs to be consistant */
	pgls->g.symbol_mode = 0;
	/*	hpgl_args_setup(&args);
		hpgl_SM(&args, pgls); */

	hpgl_args_setup(&args);
	hpgl_SS(&args, pgls);

	hpgl_args_set_int(&args,1);
	hpgl_TR(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_TD(&args, pgls);
	
	hpgl_args_setup(&args);
	hpgl_UL(&args, pgls);

	return 0;
}

/* IN = DF (see below) */
int
hpgl_IN(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	hpgl_args_t args;

	/* restore defaults */
	hpgl_DF(&args, pgls);

	/* cancel rotation */
        hpgl_args_setup(&args);
	hpgl_RO(&args, pgls);
	
	/* defaults P1 and P2 */
	hpgl_args_setup(&args);
	hpgl_IP(&args, pgls);
	
	/* HAS temporarily hardwired. Should use NP but that is broken
           at the moment.  */
	pgls->g.number_of_pens = 2;

	/* HAS: difficult to tell in SP has a default value.  We shall
           use 1 for now. */
	hpgl_args_set_int(&args,1);
	hpgl_SP(&args, pgls);

	/* pen width units - metric */
	hpgl_args_setup(&args);
	hpgl_WU(&args, pgls);
	
	/* pw .35 */
	hpgl_args_set_real(&args,0.35);
	hpgl_PW(&args, pgls);
	
	/* HAS Unclear what to do here.  state hardwired to 2 above */
#ifdef LATER
	hpgl_args_set_int(&args,2);
	hpgl_NP(&args, pgls);
#endif
	/* pen up-absolute position and set gl/2 current positon to
	   0,0 or the lower left of the picture frame.  Simply sets
	   the gl/2 state, we subsequently clear the path because we
	   do not want to create a live gs path. */

	hpgl_args_setup(&args);
	hpgl_PU(&args, pgls);
	hpgl_args_set_real2(&args, 0.0, 0.0);
	hpgl_PA(&args, pgls);
	hpgl_call(hpgl_clear_current_path(pgls));
	return 0;
}

/* derive the current picture frame coordinates */

 private int 
hpgl_picture_frame_coords(hpgl_state_t *pgls, gs_int_rect *gl2_win)
{

	gs_rect dev_win; /* device window */
	gs_rect pcl_win; /* pcl window -- upper left and lower right */
	hpgl_real_t x1 = pgls->g.picture_frame.anchor_point.x;
	hpgl_real_t y1 = pgls->g.picture_frame.anchor_point.y;
	hpgl_real_t x2 = x1 + pgls->g.picture_frame_width;
	hpgl_real_t y2 = y1 + pgls->g.picture_frame_height;
	pcl_set_ctm(pgls, false);
	hpgl_call(gs_transform(pgls->pgs, x1, y1, &dev_win.p));
	hpgl_call(gs_transform(pgls->pgs, x2, y2, &dev_win.q));
	hpgl_call(hpgl_set_plu_ctm(pgls));
	hpgl_call(gs_itransform(pgls->pgs, 
			       dev_win.p.x,
			       dev_win.p.y,
			       &pcl_win.p));
	hpgl_call(gs_itransform(pgls->pgs, 
			       dev_win.q.x,
			       dev_win.q.y,
			       &pcl_win.q));
	/* now win.p is the upper left and win.q the lower right, gl/2
           likes to use the lower left and upper right for boxes */
/* HAS have not checked if this is properly rounded or truncated */
#define round(x) (((x) < 0.0) ? (ceil ((x) - 0.5)) : (floor ((x) + 0.5)))
	gl2_win->p.x = round(pcl_win.p.x);
	gl2_win->p.y = round(pcl_win.q.y); /* !! */
	gl2_win->q.x = round(pcl_win.q.x);
	gl2_win->q.y = round(pcl_win.p.y); /* !! */
#undef round
	return 0;
}
	
  
/* IP p1x,p1y[,p2x,p2y]; */
/* IP; */
int
hpgl_IP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int32 ptxy[4];
	int i;
	gs_int_rect win;
	/* get the default picture frame coordinates */
	hpgl_call(hpgl_picture_frame_coords(pgls, &win));

	/* round the picture frame coordinates */
	ptxy[0] = win.p.x; ptxy[1] = win.p.y;
	ptxy[2] = win.q.x; ptxy[3] = win.q.y;
	for ( i = 0; i < 4 && hpgl_arg_int(pargs, &ptxy[i]); ++i )
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
	/* HAS more error checking ??? */

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

	for ( i = 0; i < 4 && hpgl_arg_c_real(pargs, &rptxy[i]); ++i )
	  ;
	if ( i & 1 )
	  return e_Range;

	/* get the PCL picture frame coordinates */
	hpgl_call(hpgl_picture_frame_coords(pgls, &win));
	hpgl_args_setup(&args);
	hpgl_args_add_int(&args, win.p.x + (win.q.x - win.p.x) *
			  rptxy[0] / 100.0);

	hpgl_args_add_int(&args, win.p.y + (win.q.y - win.p.y) *
			  rptxy[1] / 100.0);

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

	/* get the default picture frame coordinates.  HAS this need
           to be redone.  I don't think it is necessary. */
	hpgl_call(hpgl_picture_frame_coords(pgls, &win));
	wxy[0] = win.p.x;
	wxy[1] = win.p.y;
	wxy[2] = win.q.x;
	wxy[3] = win.q.y;
	for ( i = 0; i < 4 && hpgl_arg_units(pargs, &wxy[i]); ++i )
	  ;
	if ( i & 3 )
	  return e_Range;
	
	/* no args case disables the soft clip window */
	if ( i == 0 ) {
	  pgls->g.soft_clip_window.state = inactive;
	  return 0;
	}
	
	/* HAS needs error checking */
	pgls->g.soft_clip_window.rect.p.x = wxy[0];
	pgls->g.soft_clip_window.rect.p.y = wxy[1];
	pgls->g.soft_clip_window.rect.q.x = wxy[2];
	pgls->g.soft_clip_window.rect.q.y = wxy[3];
	pgls->g.soft_clip_window.state = active;
	return 0;
}

/* PG; */
int
hpgl_PG(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	
	return e_Unimplemented;
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

	if ( hpgl_arg_c_int(pargs, &angle) )
	    switch ( angle )
	      {
	      case 0: case 90: case 180: case 270:
		break;
	      default:
		return e_Range;
	      }

	/* HAS need documentation */
        if ( angle != pgls->g.rotation )
	  {
	    hpgl_args_t args;
	    
	    hpgl_pen_state_t saved_pen_state;
	    hpgl_save_pen_state(pgls, &saved_pen_state, hpgl_pen_down | hpgl_pen_relative);
	    hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	    pgls->g.rotation = angle;
	    hpgl_call(hpgl_set_ctm(pgls));
	    hpgl_call(gs_itransform(pgls->pgs, dev_pt.x, dev_pt.y, &point));
	    /* set the pen to the up position */
	    hpgl_args_setup(&args);
	    hpgl_PU(&args, pgls);
	    /* now add a moveto the using the current ctm */
	    hpgl_args_set_real(&args, point.x);
	    hpgl_args_add_real(&args, point.y);
	    hpgl_call(hpgl_PA(&args, pgls));
	    /* HAS this is added to clear first moveto yuck */
	    hpgl_call(hpgl_clear_current_path(pgls));
	    hpgl_restore_pen_state(pgls, &saved_pen_state, hpgl_pen_down | hpgl_pen_relative);
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
	gs_point point, dev_pt;

	scale_params = pgls->g.scaling_params;
	hpgl_call(hpgl_set_ctm(pgls));
	hpgl_call(hpgl_get_current_position(pgls, &point));
	hpgl_call(gs_transform(pgls->pgs, point.x, point.y, &dev_pt));

	for ( i = 0; i < 4 && hpgl_arg_real(pargs, &xy[i]); ++i )
	  ;
	switch ( i )
	  {
	  case 0:		/* set defaults */
	    type = hpgl_scaling_none;
	    break;
	  default:
	    return e_Range;
	  case 4:
	    type = hpgl_scaling_anisotropic;
	    hpgl_arg_c_int(pargs, &type);
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
		  if ( (hpgl_arg_c_real(pargs, &left) &&
			(left < 0 || left > 100 ||
			 !hpgl_arg_c_real(pargs, &bottom) ||
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

	{
	  hpgl_args_t args;
	    
	  hpgl_pen_state_t saved_pen_state;
	  hpgl_save_pen_state(pgls, &saved_pen_state, hpgl_pen_down);
	  hpgl_call(hpgl_draw_current_path(pgls, hpgl_rm_vector));
	  pgls->g.scaling_params = scale_params;
	  pgls->g.scaling_type = type;
	  
	  /* HAS probably only needs to be done if the scaling state
	     has changed */
	  hpgl_call(hpgl_set_ctm(pgls));
	  hpgl_call(gs_itransform(pgls->pgs, dev_pt.x, dev_pt.y, &point));
	  /* we must clear the current path because we set up the CTM
	     when issuing the first point, and the CTM is a function of
	     SC. */

	  /* now add a moveto the using the current ctm */
	  hpgl_args_set_real(&args, point.x);
	  hpgl_args_add_real(&args, point.y);
	  hpgl_call(hpgl_PU(&args, pgls));
	  hpgl_restore_pen_state(pgls, &saved_pen_state, hpgl_pen_down);
	}

	return 0;
}

/* BP - BreakPoint - add this to the HPGL stream and set a breakpoint
   on this function.  This function is only defined for debugging
   system. */
#ifdef DEBUG
 private int
hpgl_BP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{
	return 0;
}
#endif

/* Initialization */
private int
pgconfig_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  /* CO has special argument parsing, so it must handle skipping */
	  /* in polygon mode itself. */
	  HPGL_COMMAND('C', 'O', hpgl_CO, hpgl_cdf_polygon),
	  HPGL_COMMAND('D', 'F', hpgl_DF, 0),
	  HPGL_COMMAND('I', 'N', hpgl_IN, 0),
	  HPGL_COMMAND('I', 'P', hpgl_IP, 0),
	  HPGL_COMMAND('I', 'R', hpgl_IR, 0),
	  HPGL_COMMAND('I', 'W', hpgl_IW, 0),
	  HPGL_COMMAND('P', 'G', hpgl_PG, 0),
	  HPGL_COMMAND('R', 'O', hpgl_RO, 0),
	  HPGL_COMMAND('R', 'P', hpgl_RP, 0),
	  HPGL_COMMAND('S', 'C', hpgl_SC, 0),
#ifdef DEBUG
	  HPGL_COMMAND('B', 'P', hpgl_BP, 0),
#endif
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgconfig_init = {
  pgconfig_do_init, 0
};
