/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgpoly.c */
/* HP-GL/2 polygon commands */
#include "std.h"
#include "pgmand.h"

/* ------ Internal procedures ------ */

/* Define fill/edge and absolute/relative flags. */
#define DO_EDGE 1
#define DO_RELATIVE 2

/* Fill or edge a rectangle (EA, ER, RA, RR). */
private int
hpgl_rectangle(hpgl_args_t *pargs, hpgl_state_t *pgls, int flags)
{	hpgl_real_t x, y;
	if ( !hpgl_arg_units(pargs, &x) || !hpgl_arg_units(pargs, &y) )
	  return e_Range;
	return e_Unimplemented;
}

/* Fill or edge a wedge (EW, WG). */
private int
hpgl_wedge(hpgl_args_t *pargs, hpgl_state_t *pgls, int flags)
{	hpgl_real_t radius, start, sweep, chord = 5;

	if ( !hpgl_arg_units(pargs, &radius) ||
	     !hpgl_arg_c_real(pargs, &start) ||
	     !hpgl_arg_c_real(pargs, &sweep) || sweep < -360 || sweep > 360 ||
	     (hpgl_arg_c_real(pargs, &chord) && (chord < 0.5 || chord > 180))
	   )
	  return e_Range;
	return e_Unimplemented;
}

/* ------ Commands ------ */

/* EA x,y; */
int
hpgl_EA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_rectangle(pargs, pgls, DO_EDGE);
}

/* EP; */
int
hpgl_EP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return e_Unimplemented;
}

/* ER dx,dy; */
int
hpgl_ER(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_rectangle(pargs, pgls, DO_EDGE | DO_RELATIVE);
}

/* EW radius,astart,asweep[,achord]; */
int
hpgl_EW(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_wedge(pargs, pgls, DO_EDGE);
}

/* FP method; */
/* FP; */
int
hpgl_FP(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int method = 0;

	if ( hpgl_arg_c_int(pargs, &method) && (method & ~1) )
	  return e_Range;
	return e_Unimplemented;
}

/* PM op; */
int
hpgl_PM(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	int op = 0;

	if ( hpgl_arg_c_int(pargs, &op) && (op < 0 || op > 2) )
	  return e_Range;
	return e_Unimplemented;
}

/* RA x,y; */
int
hpgl_RA(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_rectangle(pargs, pgls, 0);
}

/* RR dx,dy; */
int
hpgl_RR(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_rectangle(pargs, pgls, DO_RELATIVE);
}

/* WG radius,astart,asweep[,achord]; */
int
hpgl_WG(hpgl_args_t *pargs, hpgl_state_t *pgls)
{	return hpgl_wedge(pargs, pgls, 0);
}

/* Initialization */
private int
pgpoly_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_HPGL_COMMANDS
	  HPGL_COMMAND('E', 'A', hpgl_EA),
	  HPGL_COMMAND('E', 'P', hpgl_EP),
	  HPGL_COMMAND('E', 'R', hpgl_ER),
	  HPGL_COMMAND('E', 'W', hpgl_EW),
	  HPGL_COMMAND('F', 'P', hpgl_FP),
	  HPGL_COMMAND('P', 'M', hpgl_PM),
	  HPGL_COMMAND('R', 'A', hpgl_RA),
	  HPGL_COMMAND('R', 'R', hpgl_RR),
	  HPGL_COMMAND('W', 'G', hpgl_WG),
	END_HPGL_COMMANDS
	return 0;
}
const pcl_init_t pgpoly_init = {
  pgpoly_do_init, 0
};
