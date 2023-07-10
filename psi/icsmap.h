/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Interface to shared routines for loading the cached color space maps. */

#ifndef icsmap_INCLUDED
#  define icsmap_INCLUDED

#include "iref.h"
#include "gscspace.h"

/*
 * Set up to load a cached map for an Indexed or substituted Separation
 * color space.  The implementation is in zcsindex.c.  When the map1
 * procedure is called, the following structure is on the e_stack:
 */
#define num_csme 5
#  define csme_cspace (-4)	/* t_struct (bytes) */
#  define csme_num_components (-3)	/* t_integer */
#  define csme_proc (-2)	/* -procedure- */
#  define csme_hival (-1)	/* t_integer */
#  define csme_index 0		/* t_integer */
/*
 * Note that the underlying color space parameter is a direct space, not a
 * base space, since the underlying space of an Indexed color space may be
 * a Separation or DeviceN space.
 */
int zcs_begin_map(i_ctx_t *i_ctx_p, gs_color_space *pcs, gs_indexed_map ** pmap,
                  const ref * pproc, int num_entries,
                  const gs_color_space * base_space,
                  op_proc_t map1);

#endif /* icsmap_INCLUDED */
