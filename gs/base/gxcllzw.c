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
/* LZW filter initialization for RAM-based band lists */
#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxclmem.h"
#include "slzwx.h"

/* Return the prototypes for compressing/decompressing the band list. */
const stream_template *clist_compressor_template(void)
{
    return &cl_LZWE_template;
}
const stream_template *
clist_decompressor_state(void)
{
    return &cl_LZWD_state;
}
void
clist_compressor_init(stream_state *state)
{
    s_LZW_set_defaults(state);
    state->template = &s_LZWE_template;
}
void
clist_decompressor_init(stream_state *state)
{
    s_LZW_set_defaults(state);
    state->template = &s_LZWD_template;
}
