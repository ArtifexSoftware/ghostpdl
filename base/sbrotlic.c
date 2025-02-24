/* Copyright (C) 2025 Artifex Software, Inc.
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


/* Code common to brotli encoding and decoding streams */
#include "std.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "strimpl.h"
#include "sbrotlix.h"

/* Provide brotli-compatible allocation and freeing functions. */
void *
s_brotli_alloc(void *zmem, size_t size)
{
    gs_memory_t *mem = zmem;
    return gs_alloc_bytes(mem, size, "s_brotli_alloc");
}

void
s_brotli_free(void *zmem, void *data)
{
    gs_memory_t *mem = zmem;

    gs_free_object(mem, data, "s_brotli_free");
}
