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

/* pcuptrn.h - interface for PCL and GL/2 user defined patterns */

#ifndef pcuptrn_INCLUDED
#define pcuptrn_INCLUDED

#include "gx.h"
#include "pcommand.h"
#include "pcpatrn.h"

/*
 * Get a PCL user-defined pattern. A null return indicates the pattern is
 * not defined.
 */
extern  pcl_pattern_t * pcl_pattern_get_pcl_uptrn( int id );

/*
 * Get a GL/2 user defined pattern. A null return indicates there is no pattern
 * defined for the index.
 */
extern pcl_pattern_t * pcl_pattern_get_gl_uptrn( int indx );

/* pcl_pattern_RF is in pcpatrn.h */

/*
 * External access to the user defined pattern related operators.
 */
extern  const pcl_init_t    pcl_upattern_init;

#endif  	/* pcuptrn_INCLUDED */
