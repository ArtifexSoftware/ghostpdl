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
/* Code common to DCT encoding and decoding streams */
#include "stdio_.h"
#include "jpeglib_.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "strimpl.h"
#include "sdct.h"

public_st_DCT_state();

/* Set the defaults for the DCT filters. */
void
s_DCT_set_defaults(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;

    ss->jpeg_memory = &gs_memory_default;
    ss->data.common = 0;
	/****************
	  ss->data.common->Picky = 0;
	  ss->data.common->Relax = 0;
	 ****************/
    ss->ColorTransform = -1;
    ss->QFactor = 1.0;
    /* Clear pointers */
    ss->Markers.data = 0;
    ss->Markers.size = 0;
}
