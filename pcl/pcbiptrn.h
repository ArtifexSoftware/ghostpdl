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

/* pcbiptrn.h - interface for PCL's built-in patterns */

#ifndef pcbiptrn_INCLUDED
#define pcbiptrn_INCLUDED

#include "pcpatrn.h"

/*
 * Initialize the built-in pattern machinery. This routine copies "const" to
 * "non-const" structures, to facilitate working with systems that install all
 * initialized global data in ROM.
 */
extern  void    pcl_pattern_init_bi_patterns( gs_memory_t * pmem );

/*
 * Clear the renderings of the built-in patterns. This may be called during
 * a reset to conserve memory.
 */
extern  void    pcl_pattern_clear_bi_patterns( void );

/*
 * For a given intensity value, return the corresponding shade pattern. A
 * null return indicates that a solid pattern should be used - the caller
 * must look at the intensity to determine if it is black or white.
 */
extern  pcl_pattern_t * pcl_pattern_get_shade( int inten );

/*
 * For a given index value, return the corresponding cross-hatch pattern. A
 * null return indicates that the pattern is out of range. The caller must
 * determine what to do in this case.
 */
extern  pcl_pattern_t * pcl_pattern_get_cross( int indx );

/*
 * Return a solid, 1 x 1 pattern for use with rasters. See the comments in
 * pcbiptrn.c for why this is necessary.
 */
extern  pcl_pattern_t * pcl_pattern_get_solid_pattern( void );

#endif  	/* pcbiptrn_INCLUDED */
