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
