/* Copyright (C) 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

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
