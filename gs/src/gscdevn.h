/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Client interface to DeviceN color */

#ifndef gscdevn_INCLUDED
#  define gscdevn_INCLUDED

#include "gscspace.h"

/*
 * Allocate and fill in a DeviceN color space.
 * Note that the client is responsible for memory management of the
 * name array and (if used) the tint transform Function.
 */
int gs_cspace_build_DeviceN(P5(
			       gs_color_space **ppcspace,
			       gs_separation_name *psnames,
			       uint num_components,
			       const gs_color_space *palt_cspace,
			       gs_memory_t *pmem
			       ));

/* Set the tint transformation procedure for a DeviceN color space. */
/* VMS limits procedure names to 31 characters, and some systems only */
/* compare the first 23 characters. */
extern int gs_cspace_set_devn_proc(P3(
				      gs_color_space * pcspace,
			int (*proc)(P5(const gs_device_n_params *,
				       const float *,
				       float *,
				       const gs_imager_state *,
				       void *
				       )),
				      void *proc_data
				      ));

/* Set the DeviceN tint transformation procedure to a Function. */
#ifndef gs_function_DEFINED
typedef struct gs_function_s gs_function_t;
#  define gs_function_DEFINED
#endif
int gs_cspace_set_devn_function(P2(gs_color_space *pcspace,
				   gs_function_t *pfn));

/*
 * If the DeviceN tint transformation procedure is a Function,
 * return the function object, otherwise return 0.
 */
gs_function_t *gs_cspace_get_devn_function(P1(const gs_color_space *pcspace));

#endif /* gscdevn_INCLUDED */
