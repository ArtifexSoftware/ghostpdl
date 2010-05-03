/* Copyright (C) 2009 Artifex Software, Inc.
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
/* Definitions for SHA256Encode filter */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef ssha2_INCLUDED
#  define ssha2_INCLUDED

#include "sha2.h"

/*
 * The SHA256Encode filter accepts an arbitrary amount of input data,
 * and then, when closed, emits the 32-byte SHA-256 digest.
 */
typedef struct stream_SHA256E_state_s {
    stream_state_common;
    SHA256_CTX sha256;
} stream_SHA256E_state;

#define private_st_SHA256E_state()	/* in ssha2.c */\
  gs_private_st_simple(st_SHA256E_state, stream_SHA256E_state,\
    "SHA256Encode state")
extern const stream_template s_SHA256E_template;

stream *s_SHA256E_make_stream(gs_memory_t *mem, byte *digest, int digest_size);

#endif /* ssha2_INCLUDED */
