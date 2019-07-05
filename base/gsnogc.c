/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Use the normal ghostscript chunk allocator without support for garbage
   collection or strings */
#include "gx.h"
#include "gsmdebug.h"
#include "gsnogc.h"
#include "gsstruct.h"
#include "gxalloc.h"

/* String allocations reduces to simple byte allocations. */
static byte *
nogc_alloc_string(gs_memory_t * mem, size_t nbytes, client_name_t cname)
{
    return gs_alloc_bytes(mem, nbytes, cname);
}

/* Free a string. */
static void
nogc_free_string(gs_memory_t * mem, byte * str, size_t size, client_name_t cname)
{
    gs_free_object(mem, str, cname);
}

static byte *
nogc_alloc_string_immovable(gs_memory_t * mem, size_t nbytes, client_name_t cname)
{
    return gs_alloc_bytes(mem, nbytes, cname);
}

static byte *
nogc_resize_string(gs_memory_t * mem, byte * data, size_t old_num, size_t new_num, client_name_t cname)
{
    return gs_resize_object(mem, data, new_num, cname);
}

/*
 * This procedure has the same API as the garbage collector used by the
 * PostScript interpreter, but it is designed to be used in environments
 * that don't need garbage collection and don't use save/restore.  All it
 * does is coalesce free blocks at the high end of the object area of each
 * chunk, and free completely empty chunks.
 *
 */

static void set_procs(gs_ref_memory_t *mem)
{
    /* Change the allocator to use string freelists in the future.  */
    mem->procs.alloc_string = nogc_alloc_string;
    mem->procs.free_string = nogc_free_string;
    mem->procs.resize_string = nogc_resize_string;
    mem->procs.alloc_string_immovable = nogc_alloc_string_immovable;
}

void
gs_nogc_reclaim(vm_spaces * pspaces, bool global)
{

    int space;
    gs_ref_memory_t *mem_prev = 0;

    for (space = 0; space < countof(pspaces->memories.indexed); ++space) {
        gs_ref_memory_t *mem = pspaces->memories.indexed[space];

        if (mem == 0 || mem == mem_prev)
            continue;

        mem_prev = mem;
        set_procs(mem);
        gs_consolidate_free((gs_memory_t *)mem);
        if (mem->stable_memory != (gs_memory_t *)mem &&
            mem->stable_memory != 0
            ) {
            set_procs((gs_ref_memory_t *)mem->stable_memory);
            gs_consolidate_free((gs_memory_t *)mem->stable_memory);
        }
    }
}
