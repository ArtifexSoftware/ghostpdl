/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
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

	hpgl_args_set_int(&args, 1);
	hpgl_LO(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_LT(&args, pgls);

	hpgl_args_setup(&args);
	hpgl_SC(&args, pgls);

	hpgl_args_set_real(&args, 0.0);
	hpgl_args_add_real(&args, 0.0);
	hpgl_PA(&args, pgls);

	hpgl_args_set_int(&args,0);
	hpgl_PM(&args, pgls);

	hpgl_args_set_int(&args,2);
	hpgl_PM(&args, pgls);

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
	/* pen up position */
	hpgl_args_setup(&args);
	hpgl_PU(&args, pgls);
	
	/* absolute positioning at the lower left of the picture frame */
	hpgl_args_set_real(&args, 0.0);
	hpgl_args_add_real(&args, 0.0);
	hpgl_PA(&args, pgls);

	return 0;
}

/* derive the current picture frame coordinates */

private void 
hpgl_picture_frame_coords(hpgl_state_t *pgls, hpgl_real_t *coords)
{
#define x1 coords[0]
#define y1 coords[1]
#define x2 coords[2]
#define y2 coords[3]
  	x1 = coord_2_plu(0.0);
	y1 = coord_2_plu(0.0);
	x2 = coord_2_plu(pgls->g.picture_frame_width);
	y2 = coord_2_plu(pgls->g.picture_frame_height);
#undef x1
#undef y1
#undef x2
#undef y2
}
	
  
/* IP p1x,p1y[,p2x,p2y]; */
/* IP; */
int
hpgl_IP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t pic_coords[4];
        int32 ptxy[4];
	int i;

	/* get the default picture frame coordinates */
	hpgl_picture_frame_coords(pgls, pic_coords);

/* HAS have not checked if this is properly rounded or truncated */
#define round(x) (((x) < 0.0) ? (ceil ((x) - 0.5)) : (floor ((x) + 0.5)))

	/* round the picture frame coordinates */
	for ( i = 0; i < 4; i++ )
	  ptxy[i] = round(pic_coords[i]);

#undef round

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
	int i, j;
	hpgl_args_t args;
	for ( i = 0; i < 4 && hpgl_arg_c_real(pargs, &rptxy[i]); ++i )
	  ;
	if ( i & 1 )
	  return e_Range;
	else
	  {

	    hpgl_args_setup(&args);

	    for ( j = 0; j < i / 2; j++ ) 
	      {
		
		hpgl_args_add_int(&args, pgls->g.P1.x +
				  (pgls->g.P2.x - pgls->g.P1.x) *
				  rptxy[j] / 100.0);

		hpgl_args_add_int(&args, pgls->g.P1.y +
				  (pgls->g.P2.y - pgls->g.P1.y) *
				  rptxy[j+1] / 100.0);
	      }

	    hpgl_IP( &args, pgls );

	  }

	return 0;
}

/* IW llx,lly,urx,ury; */
/* IW; */
int
hpgl_IW(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t wxy[4];
	int i;

	/* get the default picture frame coordinates */
	hpgl_picture_frame_coords(pgls, wxy);

	for ( i = 0; i < 4 && hpgl_arg_units(pargs, &wxy[i]); ++i )
	  ;
	if ( i & 3 )
	  return e_Range;
	
	/* HAS needs error checking */
	pgls->g.window.p.x = wxy[0];
	pgls->g.window.p.y = wxy[1];
	pgls->g.window.q.x = wxy[2];
	pgls->g.window.q.y = wxy[3];

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
{	int angle=0;

	if ( hpgl_arg_c_int(pargs, &angle) )
	    switch ( angle )
	      {
	      case 0: case 90: case 180: case 270:
		break;
	      default:
		return e_Range;
	      }

	/* new angle clears the pen.  Note that we delay rotating
           system until the ctm is set up */
        if ( angle != pgls->g.rotation )
	  {
	    /* HAS -- I believe we must maintain the device
               coordinates of the current pen.  I need to check this */
	    hpgl_draw_current_path(pgls, hpgl_rm_vector);
	    pgls->g.rotation = angle;
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
pxy:		pgls->g.scaling_params.pmin.x = xy[0];
		pgls->g.scaling_params.pmax.x = xy[1];
		pgls->g.scaling_params.pmin.y = xy[2];
		pgls->g.scaling_params.pmax.y = xy[3];
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
		  pgls->g.scaling_params.left = left;
		  pgls->g.scaling_params.bottom = bottom;
		}
		goto pxy;
	      case hpgl_scaling_point_factor: /* 2 */
		if ( xy[1] == 0 || xy[3] == 0 )
		  return e_Range;
		pgls->g.scaling_params.pmin.x = xy[0];
		pgls->g.scaling_params.factor.x = xy[1];
		pgls->g.scaling_params.pmin.y = xy[2];
		pgls->g.scaling_params.factor.y = xy[3];
		break;
	      default:
		return e_Range;
	      }
	  }
	pgls->g.scaling_type = type;
	return 0;
}

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
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgconfig_init = {
  pgconfig_do_init, 0
};
