/* Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* szlibx.h */
/* Definitions for zlib filters */
/* Requires strimpl.h */
/* Must be compiled with -I$(ZSRCDIR) */
#include "zlib.h"

/* Provide zlib-compatible allocation and freeing functions. */
void *s_zlib_alloc(P3(void *mem, uint items, uint size));
void s_zlib_free(P2(void *mem, void *address));

typedef struct stream_zlib_state_s {
	stream_state_common;
  /* Parameters - compression and decompression */
	int windowBits;
	bool no_wrapper;	/* omit wrapper and checksum */
  /* Parameters - compression only */
	int level;		/* effort level */
	int method;
	int memLevel;
	int strategy;
  /* Dynamic state */
	z_stream zstate;
} stream_zlib_state;
/* The state descriptor is public only to allow us to split up */
/* the encoding and decoding filters. */
/* Note that all zlib's allocations are done as immovable bytes */
/* to avoid garbage collection issues. */
extern_st(st_zlib_state);
#define public_st_zlib_state()	/* in szlibc.c */\
  gs_public_st_simple(st_zlib_state, stream_zlib_state,\
    "zlibEncode/Decode state")
extern const stream_template s_zlibD_template;
extern const stream_template s_zlibE_template;

/* Shared procedures */
stream_proc_set_defaults(s_zlib_set_defaults);
