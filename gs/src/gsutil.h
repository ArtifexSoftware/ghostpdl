/* Copyright (C) 1992, 1993, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Prototypes for procedures in gsutil.c */

#ifndef gsutil_INCLUDED
#  define gsutil_INCLUDED

/* ------ Unique IDs ------ */

/* Generate a block of unique IDs. */
gs_id gs_next_ids(P1(uint count));

/* ------ Memory utilities ------ */

/* Transpose an 8 x 8 block of bits. */
/* line_size is the raster of the input data; */
/* dist is the distance between output bytes. */
/* Dot matrix printers need this. */
/* Note that with a negative dist value, */
/* this will rotate an 8 x 8 block 90 degrees counter-clockwise. */
void memflip8x8(P4(const byte * inp, int line_size, byte * outp, int dist));

/* Get an unsigned, big-endian 32-bit value. */
ulong get_u32_msb(P1(const byte *p));

/* ------ String utilities ------ */

/* Compare two strings, returning -1 if the first is less, */
/* 0 if they are equal, and 1 if first is greater. */
/* We can't use memcmp, because we always use unsigned characters. */
int bytes_compare(P4(const byte * str1, uint len1,
		     const byte * str2, uint len2));

/* Test whether a string matches a pattern with wildcards. */
/* If psmp == NULL, use standard parameters: '*' = any substring, */
/* '?' = any character, '\\' quotes next character, don't ignore case. */
typedef struct string_match_params_s {
    int any_substring;		/* '*' */
    int any_char;		/* '?' */
    int quote_next;		/* '\\' */
    bool ignore_case;
    bool slash_equiv;	/* '\\' is equivalent to '/' for Windows filename matching */
} string_match_params;
extern const string_match_params string_match_params_default;
bool string_match(P5(const byte * str, uint len,
		     const byte * pstr, uint plen,
		     const string_match_params * psmp));

#endif /* gsutil_INCLUDED */
