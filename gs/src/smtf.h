/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Definitions for MoveToFront filters */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef smtf_INCLUDED
#  define smtf_INCLUDED

/* MoveToFrontEncode/Decode */
typedef struct stream_MTF_state_s {
    stream_state_common;
    /* The following change dynamically. */
    union _p {
	ulong l[256 / sizeof(long)];
	byte b[256];
    } prev;
} stream_MTF_state;
typedef stream_MTF_state stream_MTFE_state;
typedef stream_MTF_state stream_MTFD_state;

#define private_st_MTF_state()	/* in sbwbs.c */\
  gs_private_st_simple(st_MTF_state, stream_MTF_state,\
    "MoveToFrontEncode/Decode state")
extern const stream_template s_MTFE_template;
extern const stream_template s_MTFD_template;

#endif /* smtf_INCLUDED */
