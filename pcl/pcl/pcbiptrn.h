/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
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
void pcl_pattern_init_bi_patterns(pcl_state_t * pcs);

/*
 * Clear the renderings of the built-in patterns. This may be called during
 * a reset to conserve memory.
 */
void pcl_pattern_clear_bi_patterns(pcl_state_t * pcs);

/*
 * For a given intensity value, return the corresponding shade pattern. A
 * null return indicates that a solid pattern should be used - the caller
 * must look at the intensity to determine if it is black or white.
 */
pcl_pattern_t *pcl_pattern_get_shade(pcl_state_t * pcs, int inten);

/*
 * For a given index value, return the corresponding cross-hatch pattern. A
 * null return indicates that the pattern is out of range. The caller must
 * determine what to do in this case.
 */
pcl_pattern_t *pcl_pattern_get_cross(pcl_state_t * pcs, int indx);

/*
 * Return a solid, 1 x 1 pattern for use with rasters. See the comments in
 * pcbiptrn.c for why this is necessary.
 */
pcl_pattern_t *pcl_pattern_get_solid_pattern(pcl_state_t * pcs);

/*
 * Return an "unsolid", 1 x 1 pattern for use with GL/2. See the comments in
 * pcbiptrn.c for why this is necessary.
 */
pcl_pattern_t *pcl_pattern_get_unsolid_pattern(pcl_state_t * pcs);

#endif /* pcbiptrn_INCLUDED */
