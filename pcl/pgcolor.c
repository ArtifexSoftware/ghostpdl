/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgcolor.c */
/* HP-GL/2 color vector graphics commands */
#include "std.h"
#include "pgmand.h"
#include "pginit.h"
#include "gsstate.h"		/* for gs_setfilladjust */

/* ------ Commands ------ */

/* PC [pen[,primary1,primary2,primary3]; */
int
hpgl_PC(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int32 pen;
	hpgl_real_t primary[3];

	if ( hpgl_arg_int(pargs, &pen) )
	  { if ( pen < 0 || pen >= pgls->g.number_of_pens )
	      return e_Range;
	    if ( hpgl_arg_c_real(pargs, &primary[0]) )
	      { int c;
	        hpgl_real_t limit;

	        if ( !hpgl_arg_c_real(pargs, &primary[1]) ||
		     !hpgl_arg_c_real(pargs, &primary[2])
		   )
		  return e_Range;
		for ( c = 0; c < 3; ++c )
		  pgls->g.pen_color[pen].rgb[c] =
		    (primary[c] < (limit = pgls->g.color_range[c].cmin) ||
		     primary[c] > (limit = pgls->g.color_range[c].cmax) ?
		     limit : primary[c]);
	      }
	    else
	      hpgl_default_pen_color(pgls, (int)pen);
	  }
	else
	  { int i;

	    for ( i = 0; i < min(pgls->g.number_of_pens, 8); ++i )
	      hpgl_default_pen_color(pgls, i);
	  }
	return 0;
}

/* NP [n]; */
int
hpgl_NP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int32 n = 8;

	if ( hpgl_arg_int(pargs, &n) && (n < 2 || n > 32768) )
	  return e_Range;
	/* Round up to a power of 2. */
	while ( n & (n - 1) )
	  n = (n | (n - 1)) + 1;
	/**** REALLOCATE ARRAYS IF NOT SIMPLE PALETTE, AND THEN: ****/
	/*pgls->g.number_of_pens = n;*/
	return 0;
}

/* CR [b_red, w_red, b_green, w_green, b_blue, w_blue]; */
int
hpgl_CR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	hpgl_real_t range[6];
	int i;

	range[0] = range[2] = range[4] = 0;
	range[1] = range[3] = range[5] = 255;
	for ( i = 0; i < 6 && hpgl_arg_c_real(pargs, &range[i]); ++i )
	  ;
	if ( range[0] == range[1] || range[2] == range[3] ||
	     range[4] == range[5]
	   )
	  return e_Range;
	for ( i = 0; i < 6; i += 2 )
	  { /**** REMAP PEN COLORS ****/
	    pgls->g.color_range[i >> 1].cmin = range[i];
	    pgls->g.color_range[i >> 1].cmax = range[i + 1];
	  }
	return 0;
}

/* Initialization */
private int
pgcolor_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('C', 'R', hpgl_CR, 0),
	  HPGL_COMMAND('N', 'P', hpgl_NP, 0),
	  HPGL_COMMAND('P', 'C', hpgl_PC, 0),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgcolor_init = {
  pgcolor_do_init, 0
};
