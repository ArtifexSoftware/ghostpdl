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


/* Implementation definitions for brotli interface */
/* Must be compiled with -I$(BROTLIDIR) */

#ifndef sbrotlixx_INCLUDED
#  define sbrotlixx_INCLUDED

#include "scommon.h"

/* These have to be open for the postscript filters
 * to be able to set params. */
typedef struct stream_brotlid_state_s {
    stream_state_common;
    void *dec_state;
} stream_brotlid_state;

typedef struct stream_brotlie_state_s {
    stream_state_common;
    void *enc_state;
    int mode;
    int windowBits;
    int level;
} stream_brotlie_state;

extern const stream_template s_brotliD_template;
extern const stream_template s_brotliE_template;

/* Shared procedures */
stream_proc_set_defaults(s_brotliE_set_defaults);

/*
 * Provide brotli-compatible allocation and freeing functions.
 * The mem pointer actually points to the dynamic state.
 */
void *s_brotli_alloc(void *mem, size_t size);
void s_brotli_free(void *mem, void *address);

#endif /* sbrotlixx_INCLUDED */
