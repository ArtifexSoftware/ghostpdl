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
/* Interface for creating a sample device CRD */

#ifndef gdevdcrd_INCLUDED
#define gdevdcrd_INCLUDED

/* Implement get_params for a sample device CRD. */
int sample_device_crd_get_params(P3(gx_device *pdev, gs_param_list *plist,
				    const char *crd_param_name));

#endif	/* gdevdcrd_INCLUDED */
