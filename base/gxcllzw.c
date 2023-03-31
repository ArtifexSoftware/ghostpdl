/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


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
    state->templat = &s_LZWE_template;
}
void
clist_decompressor_init(stream_state *state)
{
    s_LZW_set_defaults(state);
    state->templat = &s_LZWD_template;
}
