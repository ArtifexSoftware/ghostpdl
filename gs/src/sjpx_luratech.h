/* Copyright (C) 2003-2006 artofcode LLC.  All rights reserved.
  
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

/* $Id: sjpx.h,v 1.4 2004/12/07 05:03:37 giles Exp $ */
/* Definitions for JPXDecode filter (JPEG 2000) */
/* we link to the Luratech CSDK for the actual decoding */

#ifndef sjpx_INCLUDED
#  define sjpx_INCLUDED

/* Requires scommon.h; strimpl.h if any templates are referenced */

#include "scommon.h"
#include <lwf_jp2.h>

/* Stream state for the Luratech jp2 codec
 * We rely on our finalization call to free the
 * associated handle and pointers.
 */
typedef struct stream_jpxd_state_s
{
    stream_state_common;	/* a define from scommon.h */
    const gs_memory_t *jpx_memory;
    JP2_Decomp_Handle handle;	/* library decoder handle */
    unsigned char *inbuf;	/* input data buffer */
    unsigned long inbuf_size;
    unsigned long inbuf_fill;
    int ncomp;			/* number of image components */
    unsigned long width, height;
    unsigned long stride;
    unsigned char *image;	/* decoded image buffer */
    long offset; /* offset into the image buffer of the next
                    byte to be returned */
}
stream_jpxd_state;

#define private_st_jpxd_state()	\
  gs_private_st_simple(st_jpxd_state, stream_jpxd_state,\
    "JPXDecode filter state")
extern const stream_template s_jpxd_template;

#endif /* sjpx_INCLUDED */
