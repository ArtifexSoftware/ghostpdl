/* Portions Copyright (C) 2007 artofcode LLC.
   Portions Copyright (C) 2007 Artifex Software Inc.
   Portions Copyright (C) 2007 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pxpthr.c.c */
/* Definitions for PCL XL passtrough mode */

#ifndef pxpthr_INCLUDED
#  define pxpthr_INCLUDED

/* set passthrough contiguous mode */
void pxpcl_passthroughcontiguous(bool contiguous);

/* end passthrough contiguous mode */
void pxpcl_endpassthroughcontiguous(px_state_t *pxs);

/* reset pcl's page */
void pxpcl_pagestatereset(void);

/* release the passthrough state */
void pxpcl_release(void);

/* set variables in pcl's state that are special to pass through mode,
   these override the default pcl state variables when pcl is
   entered. */
void pxPassthrough_pcl_state_nonpage_exceptions(px_state_t *pxs);

#endif                /* pxpthr_INCLUDED */
