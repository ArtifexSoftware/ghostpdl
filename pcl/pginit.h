/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pginit.h - Interface to initialize/reset procedures in pginit.c */

#ifndef pginit_INCLUDED
#define pginit_INCLUDED

#include "gx.h"
#include "pcstate.h"
#include "pcommand.h"
#include "pgmand.h"

/* Reset a set of font parameters to their default values. */
extern  void    hpgl_default_font_params(P1( pcl_font_selection_t * pfs ));

/* Reset all the fill patterns to solid fill. */
extern  void    hpgl_default_all_fill_patterns(P1( hpgl_state_t * pgls ));

/* Reset (parts of) the HP-GL/2 state. */
extern  void    hpgl_do_reset(P2( pcl_state_t * pcls, pcl_reset_type_t type ));

#endif  	/* pginit_INCLUDED */
