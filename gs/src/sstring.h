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
/* are referenced, but some compilers always require strimpl.h. */

#ifndef sstring_INCLUDED
#  define sstring_INCLUDED

/* ASCIIHexEncode */
typedef struct stream_AXE_state_s {
    stream_state_common;
    /* The following are set by the client. */
    bool EndOfData;		/* if true, write > at EOD (default) */
    /* The following change dynamically. */
    int count;			/* # of digits since last EOL */
} stream_AXE_state;

#define private_st_AXE_state()	/* in sstring.c */\
  gs_private_st_simple(st_AXE_state, stream_AXE_state,\
    "ASCIIHexEncode state")
#define s_AXE_init_inline(ss)\
  ((ss)->EndOfData = true, (ss)->count = 0)
extern const stream_template s_AXE_template;

/* ASCIIHexDecode */
typedef struct stream_AXD_state_s {
    stream_state_common;
    int odd;			/* odd digit */
} stream_AXD_state;

#define private_st_AXD_state()	/* in sstring.c */\
  gs_private_st_simple(st_AXD_state, stream_AXD_state,\
    "ASCIIHexDecode state")
#define s_AXD_init_inline(ss)\
  ((ss)->min_left = 1, (ss)->odd = -1, 0)
extern const stream_template s_AXD_template;

/* PSStringDecode */
typedef struct stream_PSSD_state_s {
    stream_state_common;
    /* The following are set by the client. */
    bool from_string;		/* true if using Level 1 \ convention */
    /* The following change dynamically. */
    int depth;
} stream_PSSD_state;

#define private_st_PSSD_state()	/* in sstring.c */\
  gs_private_st_simple(st_PSSD_state, stream_PSSD_state,\
    "PSStringDecode state")
/* We define the initialization procedure here, so that the scanner */
/* can avoid a procedure call. */
#define s_PSSD_init_inline(ss)\
  ((ss)->depth = 0)
extern const stream_template s_PSSD_template;

/* PSStringEncode */
/* (no state) */
extern const stream_template s_PSSE_template;

#endif /* sstring_INCLUDED */
