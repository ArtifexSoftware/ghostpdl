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
/* Image operator entry points */
/* Requires gscspace.h, gxiparam.h */

#ifndef iimage_INCLUDED
#  define iimage_INCLUDED

/* These procedures are exported by zimage.c for other modules. */

/* Exported for zcolor1.c */
int zimage_opaque_setup(P6(i_ctx_t *i_ctx_p, os_ptr op, bool multi,
			   gs_image_alpha_t alpha, const gs_color_space * pcs,
			   int npop));

/* Exported for zimage2.c */
int zimage_setup(P5(i_ctx_t *i_ctx_p, const gs_pixel_image_t * pim,
		    const ref * sources, bool uses_color, int npop));

/* Exported for zcolor3.c */
int zimage_data_setup(P5(i_ctx_t *i_ctx_p, const gs_pixel_image_t * pim,
			 gx_image_enum_common_t * pie,
			 const ref * sources, int npop));

/* Exported for zcolor1.c and zdpnext.c. */
int zimage_multiple(P2(i_ctx_t *i_ctx_p, bool has_alpha));

#endif /* iimage_INCLUDED */
