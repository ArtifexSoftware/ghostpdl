/* Copyright (C) 1994, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Interface to shared routines for loading the cached color space maps. */

#ifndef icsmap_INCLUDED
#  define icsmap_INCLUDED

/*
 * Set up to load a cached map for an Indexed or substituted Separation
 * color space.  The implementation is in zcsindex.c.  When the map1
 * procedure is called, the following structure is on the e_stack:
 */
#define num_csme 5
#  define csme_num_components (-4)	/* t_integer */
#  define csme_map (-3)		/* t_struct (bytes) */
#  define csme_proc (-2)	/* -procedure- */
#  define csme_hival (-1)	/* t_integer */
#  define csme_index 0		/* t_integer */
/*
 * Note that the underlying color space parameter is a direct space, not a
 * base space, since the underlying space of an Indexed color space may be
 * a Separation or DeviceN space.
 */
int zcs_begin_map(P6(i_ctx_t *i_ctx_p, gs_indexed_map ** pmap,
		     const ref * pproc, int num_entries,
		     const gs_direct_color_space * base_space,
		     op_proc_t map1));

#endif /* icsmap_INCLUDED */
