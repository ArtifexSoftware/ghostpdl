/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*Id: szlibx.h  */
/* Must be compiled with -I$(ZSRCDIR) */

#ifndef szlibx_INCLUDED
#  define szlibx_INCLUDED

#include "zlib.h"

/*
 * We don't want to allocate zlib's private data directly from
 * the C heap, but we must allocate it as immovable; and to avoid
 * garbage collection issues, we must keep GC-traceable pointers
 * to every block allocated.  Since the stream state itself is movable,
 * we have to allocate an immovable block for the z_stream state as well.
 */
typedef struct zlib_block_s zlib_block_t;
struct zlib_block_s {
    void *data;
    zlib_block_t *next;
    zlib_block_t *prev;
};
#define private_st_zlib_block()	/* in szlibc.c */\
  gs_private_st_ptrs3(st_zlib_block, zlib_block_t, "zlib_block_t",\
    zlib_block_enum_ptrs, zlib_block_reloc_ptrs, next, prev, data)
typedef struct zlib_dynamic_state_s {
    gs_memory_t *memory;
    zlib_block_t *blocks;
    z_stream zstate;
} zlib_dynamic_state_t;
#define private_st_zlib_dynamic_state()	/* in szlibc.c */\
  gs_private_st_ptrs1(st_zlib_dynamic_state, zlib_dynamic_state_t,\
    "zlib_dynamic_state_t", zlib_dynamic_enum_ptrs, zlib_dynamic_reloc_ptrs,\
    blocks)

/*
 * Provide zlib-compatible allocation and freeing functions.
 * The mem pointer points to the dynamic state.
 */
void *s_zlib_alloc(P3(void *mem, uint items, uint size));
void s_zlib_free(P2(void *mem, void *address));

typedef struct stream_zlib_state_s {
    stream_state_common;
    /* Parameters - compression and decompression */
    int windowBits;
    bool no_wrapper;		/* omit wrapper and checksum */
    /* Parameters - compression only */
    int level;			/* effort level */
    int method;
    int memLevel;
    int strategy;
    /* Dynamic state */
    zlib_dynamic_state_t *dynamic;
} stream_zlib_state;

/*
 * The state descriptor is public only to allow us to split up
 * the encoding and decoding filters.
 */
extern_st(st_zlib_state);
#define public_st_zlib_state()	/* in szlibc.c */\
  gs_public_st_ptrs1(st_zlib_state, stream_zlib_state,\
    "zlibEncode/Decode state", zlib_state_enum_ptrs, zlib_state_reloc_ptrs,\
    dynamic)
extern const stream_template s_zlibD_template;
extern const stream_template s_zlibE_template;

/* Shared procedures */
stream_proc_set_defaults(s_zlib_set_defaults);

/* Internal procedure to allocate and free the dynamic state. */
int s_zlib_alloc_dynamic_state(P1(stream_zlib_state *ss));
void s_zlib_free_dynamic_state(P1(stream_zlib_state *ss));

#endif /* szlibx_INCLUDED */
