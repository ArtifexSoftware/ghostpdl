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
/* Code common to LZW encoding and decoding streams */
#include "std.h"
#include "strimpl.h"
#include "slzwx.h"

/* Define the structure for the GC. */
public_st_LZW_state();

/* Set defaults */
void
s_LZW_set_defaults(stream_state * st)
{
    stream_LZW_state *const ss = (stream_LZW_state *) st;

    s_LZW_set_defaults_inline(ss);
}

/* Release a LZW filter. */
void
s_LZW_release(stream_state * st)
{
    stream_LZW_state *const ss = (stream_LZW_state *) st;

    gs_free_object(st->memory, ss->table.decode, "LZW(close)");
}
