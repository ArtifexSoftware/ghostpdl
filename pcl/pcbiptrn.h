/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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

/*
 * Return an "unsolid", 1 x 1 pattern for use with GL/2. See the comments in
 * pcbiptrn.c for why this is necessary.
 */
extern  pcl_pattern_t * pcl_pattern_get_unsolid_pattern( void );

#endif  	/* pcbiptrn_INCLUDED */
