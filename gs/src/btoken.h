/* Copyright (C) 1990, 1998, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Definitions for Level 2 binary tokens */

#ifndef btoken_INCLUDED
#  define btoken_INCLUDED

/* Define accessors for pointers to the system and user name tables. */
extern ref binary_token_names;	/* array of size 2 */

#define system_names_p (binary_token_names.value.refs)
#define user_names_p (binary_token_names.value.refs + 1)

/* Convert an object to its representation in a binary object sequence. */
int encode_binary_token(P5(i_ctx_t *i_ctx_p, const ref *obj, long *ref_offset,
			   long *char_offset, byte *str));

/* Define the current binary object format for operators. */
/* This is a ref so that it can be managed properly by save/restore. */
#define ref_binary_object_format_container i_ctx_p
#define ref_binary_object_format\
  (ref_binary_object_format_container->binary_object_format)

#endif /* btoken_INCLUDED */
