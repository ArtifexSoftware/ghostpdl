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


/* ASCII85 filter interface */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef sa85x_INCLUDED
#  define sa85x_INCLUDED

#include "sa85d.h"

/* ASCII85Encode */
typedef struct stream_A85E_state_s {
    stream_state_common;
    /* The following change dynamically. */
    int count;			/* # of digits since last EOL */
    int last_char;		/* last character written */
} stream_A85E_state;

#define private_st_A85E_state()	/* in sfilter2.c */\
  gs_private_st_simple(st_A85E_state, stream_A85E_state,\
    "ASCII85Encode state")
#define s_A85E_init_inline(ss)\
  ((ss)->count = 0, (ss)->last_char = '\n', 0)
extern const stream_template s_A85E_template;

#endif /* sa85x_INCLUDED */
