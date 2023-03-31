/* Copyright (C) 2017-2023 Artifex Software, Inc.
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


/* Definitions for PWG compatible RunLength filters */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef spwgx_INCLUDED
#  define spwgx_INCLUDED

#include "scommon.h"

/* Common state */
#define stream_PWG_state_common\
        stream_state_common;\
        int width;\
        int bpp

/* No encode for now */

/* PWG RunLengthDecode */
typedef struct stream_PWGD_state_s {
    stream_PWG_state_common;
    int line_pos; /* Byte Position on the current line (0 to bpp*width-1) */
    int line_rep; /* Number of times this line should be repeated */
    byte *line_buffer; /* Pointer to line buffer */
    int state; /* 0 = Waiting for line_rep byte, 1 = Waiting for repeat, > 1 => copy n-1 bytes literally into linebuffer, < 0 => -n bytes of repeats */
} stream_PWGD_state;

/* Needed a default width. Stole it from fax. */
#define PWG_default_width (1728)
#define PWG_default_bpp (8)

#define private_st_PWGD_state()	/* in spwgd.c */\
  gs_private_st_simple(st_PWGD_state, stream_PWGD_state, "PWGDecode state")
extern const stream_template s_PWGD_template;

#endif /* spwgx_INCLUDED */
