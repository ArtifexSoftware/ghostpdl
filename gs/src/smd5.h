/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Definitions for MD5Encode filter */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef smd5_INCLUDED
#  define smd5_INCLUDED

#include "md5.h"

/*
 * The MD5Encode filter accepts an arbitrary amount of input data, and then,
 * when closed, emits the 16-byte MD5 digest.
 */
typedef struct stream_MD5E_state_s {
    stream_state_common;
    md5_state_t md5;
} stream_MD5E_state;

#define private_st_MD5E_state()	/* in smd5.c */\
  gs_private_st_simple(st_MD5E_state, stream_MD5E_state,\
    "MD5Encode state")
extern const stream_template s_MD5E_template;

#endif /* smd5_INCLUDED */
