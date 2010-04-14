/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* zlib filter initialization for RAM-based band lists */
/* Must be compiled with -I$(ZSRCDIR) */
#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxclmem.h"
#include "szlibx.h"

/* Return the prototypes for compressing/decompressing the band list. */
const stream_template *
clist_compressor_template(void)
{
    return &s_zlibE_template;
}
const stream_template *
clist_decompressor_template(void)
{
    return &s_zlibD_template;
}
void
clist_compressor_init(stream_state *state)
{
    s_zlib_set_defaults(state);
    ((stream_zlib_state *)state)->no_wrapper = true;
    state->template = &s_zlibE_template;
}
void
clist_decompressor_init(stream_state *state)
{
    s_zlib_set_defaults(state);
    ((stream_zlib_state *)state)->no_wrapper = true;
    state->template = &s_zlibD_template;
}
