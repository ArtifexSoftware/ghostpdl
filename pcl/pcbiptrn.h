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

/* pcbiptrn.h - interface for PCL's built-in patterns */

#ifndef pcbiptrn_INCLUDED
#define pcbiptrn_INCLUDED

#include "pcpatrn.h"

/*
 * Initialize the built-in pattern machinery. This routine copies "const" to
 * "non-const" structures, to facilitate working with systems that install all
 * initialized global data in ROM.
 */
void pcl_pattern_init_bi_patterns( pcl_state_t *pcs );

/*
 * Clear the renderings of the built-in patterns. This may be called during
 * a reset to conserve memory.
 */
void pcl_pattern_clear_bi_patterns(pcl_state_t *pcs);

/*
 * For a given intensity value, return the corresponding shade pattern. A
 * null return indicates that a solid pattern should be used - the caller
 * must look at the intensity to determine if it is black or white.
 */
pcl_pattern_t * pcl_pattern_get_shade( pcl_state_t *pcs, int inten);

/*
 * For a given index value, return the corresponding cross-hatch pattern. A
 * null return indicates that the pattern is out of range. The caller must
 * determine what to do in this case.
 */
pcl_pattern_t * pcl_pattern_get_cross(pcl_state_t *pcs, int indx);

/*
 * Return a solid, 1 x 1 pattern for use with rasters. See the comments in
 * pcbiptrn.c for why this is necessary.
 */
pcl_pattern_t * pcl_pattern_get_solid_pattern(pcl_state_t *pcs);

/*
 * Return an "unsolid", 1 x 1 pattern for use with GL/2. See the comments in
 * pcbiptrn.c for why this is necessary.
 */
pcl_pattern_t * pcl_pattern_get_unsolid_pattern(pcl_state_t *pcs);

#endif  	/* pcbiptrn_INCLUDED */
