/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Definitions for ByteTranslate filters */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef sbtx_INCLUDED
#  define sbtx_INCLUDED

/* ByteTranslateEncode/Decode */
typedef struct stream_BT_state_s {
    stream_state_common;
    byte table[256];
} stream_BT_state;
typedef stream_BT_state stream_BTE_state;
typedef stream_BT_state stream_BTD_state;

#define private_st_BT_state()	/* in sfilter1.c */\
  gs_private_st_simple(st_BT_state, stream_BT_state,\
    "ByteTranslateEncode/Decode state")
extern const stream_template s_BTE_template;
extern const stream_template s_BTD_template;

#endif /* sbtx_INCLUDED */
