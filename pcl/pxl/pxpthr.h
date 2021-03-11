/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* pxpthr.c.c */
/* Definitions for PCL XL passtrough mode */

#ifndef pxpthr_INCLUDED
#  define pxpthr_INCLUDED

/* set passthrough contiguous mode */
void pxpcl_passthroughcontiguous(px_state_t * pxs, bool contiguous);

/* end passthrough contiguous mode */
void pxpcl_endpassthroughcontiguous(px_state_t * pxs);

/* reset pcl's page */
void pxpcl_pagestatereset(px_state_t* pxs);

/* release the passthrough state */
void pxpcl_release(px_state_t* pxs);

/* set variables in pcl's state that are special to pass through mode,
   these override the default pcl state variables when pcl is
   entered. */
int pxPassthrough_pcl_state_nonpage_exceptions(px_state_t * pxs);

int pxpcl_selectfont(px_args_t * par, px_state_t * pxs);

#endif /* pxpthr_INCLUDED */
