/* Copyright (C) 1990, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Definitions for Level 2 binary tokens */

#ifndef btoken_INCLUDED
#  define btoken_INCLUDED

/* Define accessors for pointers to the system and user name tables. */
extern ref binary_token_names;	/* array of size 2 */

#define system_names_p (binary_token_names.value.refs)
#define user_names_p (binary_token_names.value.refs + 1)

/* Convert an object to its representation in a binary object sequence. */
int encode_binary_token(P4(const ref * obj, long *ref_offset, long *char_offset,
			   byte * str));

#endif /* btoken_INCLUDED */
