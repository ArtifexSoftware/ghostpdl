/* Copyright (C) 1995, 1997 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/* szlibc.c */
/* Code common to zlib encoding and decoding streams */
#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "strimpl.h"
#include "szlibx.h"
#include "zconf.h"

public_st_zlib_state();

#define ss ((stream_zlib_state *)st)

/* Set defaults for stream parameters. */
void
s_zlib_set_defaults(stream_state * st)
{
    ss->windowBits = MAX_WBITS;
    ss->no_wrapper = false;
    ss->level = Z_DEFAULT_COMPRESSION;
    ss->method = Z_DEFLATED;
    /* DEF_MEM_LEVEL should be in zlib.h or zconf.h, but it isn't. */
    ss->memLevel = min(MAX_MEM_LEVEL, 8);
    ss->strategy = Z_DEFAULT_STRATEGY;
}

#undef ss

/* Provide zlib-compatible allocation and freeing functions. */
void *
s_zlib_alloc(void *mem, uint items, uint size)
{
    void *address =
    gs_alloc_byte_array_immovable((gs_memory_t *) mem, items, size, "zlib");

    return (address == 0 ? Z_NULL : address);
}
void
s_zlib_free(void *mem, void *address)
{
    gs_free_object((gs_memory_t *) mem, address, "zlib");
}
