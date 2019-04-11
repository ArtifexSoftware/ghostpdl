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


/* Definitions for jbig2decode filter */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef sjbig2_INCLUDED
#  define sjbig2_INCLUDED

#include "stdint_.h"
#include "scommon.h"
#include <jbig2.h>

typedef struct s_jbig2_callback_data_s
{
    gs_memory_t *memory;
    int error;
    char *last_message;
    Jbig2Severity severity;
    const char *type;
    long repeats;
} s_jbig2_callback_data_t;

/* See zfjbig2.c for details. */
typedef struct s_jbig2_global_data_s {
        void *data;
} s_jbig2_global_data_t;

/* JBIG2Decode internal stream state */
typedef struct stream_jbig2decode_state_s
{
    stream_state_common; /* a define from scommon.h */
    s_jbig2_global_data_t *global_struct; /* to protect it from freeing by GC */
    Jbig2GlobalCtx *global_ctx;
    Jbig2Ctx *decode_ctx;
    Jbig2Image *image;
    long offset; /* offset into the image bitmap of the next byte to be returned */
    s_jbig2_callback_data_t *callback_data; /* is allocated in non-gc memory */
}
stream_jbig2decode_state;

struct_proc_finalize(s_jbig2decode_finalize);

#define private_st_jbig2decode_state()	\
  gs_private_st_ptrs1_final(st_jbig2decode_state, stream_jbig2decode_state,\
    "jbig2decode filter state", jbig2decode_state_enum_ptrs,\
     jbig2decode_state_reloc_ptrs, s_jbig2decode_finalize, global_struct)
extern const stream_template s_jbig2decode_template;

/* call ins to process the JBIG2Globals parameter */
int
s_jbig2decode_make_global_data(gs_memory_t *mem, byte *data, uint length, void **result);
int
s_jbig2decode_set_global_data(stream_state *ss, s_jbig2_global_data_t *gd, void *global_ctx);
void
s_jbig2decode_free_global_data(void *data);

#endif /* sjbig2_INCLUDED */
