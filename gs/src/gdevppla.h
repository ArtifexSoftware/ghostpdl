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

/*$RCSfile$ $Revision$ */
/* Support for printer devices with planar buffering. */
/* Requires gdevprn.h */

#ifndef gdevppla_INCLUDED
#  define gdevppla_INCLUDED

/* Set the buf_procs in a printer device to planar mode. */
int gdev_prn_set_procs_planar(P1(gx_device *pdev));

/* Open a printer device, conditionally setting it to be planar. */
int gdev_prn_open_planar(P2(gx_device *pdev, bool upb));

/* Augment get/put_params to add UsePlanarBuffer. */
int gdev_prn_get_params_planar(P3(gx_device * pdev, gs_param_list * plist,
				  bool *pupb));
int gdev_prn_put_params_planar(P3(gx_device * pdev, gs_param_list * plist,
				  bool *pupb));

/* Create a planar buffer device. */
/* Use this instead of the default if UsePlanarBuffer is true. */
int gdev_prn_create_buf_planar(P5(gx_device **pbdev, gx_device *target,
				  const gx_render_plane_t *render_plane,
				  gs_memory_t *mem, bool for_band));

/* Determine the space needed by a planar buffer device. */
/* Use this instead of the default if UsePlanarBuffer is true. */
int gdev_prn_size_buf_planar(P5(gx_device_buf_space_t *space,
				gx_device *target,
				const gx_render_plane_t *render_plane,
				int height, bool for_band));

#endif /* gdevppla_INCLUDED */
