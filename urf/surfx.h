/* Copyright (C) 2017 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Definitions for URF compatible RunLength filters */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef surfx_INCLUDED
#  define surfx_INCLUDED

/* Common state */
#define stream_URF_state_common\
        stream_state_common;\
        int width;\
        int bpp;\
        byte white /* 0 for CMYK, FF otherwise */

/* No encode for now */

/* URF RunLengthDecode */
typedef struct stream_URFD_state_s {
    stream_URF_state_common;
    int line_pos; /* Byte Position on the current line (0 to bpp*width-1) */
    int line_rep; /* Number of times this line should be repeated */
    byte *line_buffer; /* Pointer to line buffer */
    int state; /* 0 = Waiting for line_rep byte, 1 = Waiting for repeat, > 1 => copy n-1 bytes literally into linebuffer, < 0 => -n bytes of repeats */
} stream_URFD_state;

/* Needed a default width. Stole it from fax. */
#define URF_default_width (1728)
#define URF_default_bpp (8)

#define private_st_URFD_state()	/* in surfd.c */\
  gs_private_st_simple(st_URFD_state, stream_URFD_state, "URFDecode state")
extern const stream_template s_URFD_template;

#endif /* surfx_INCLUDED */
