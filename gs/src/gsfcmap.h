/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* CMap data definition */
/* Requires gsstruct.h */

#ifndef gsfcmap_INCLUDED
#  define gsfcmap_INCLUDED

#include "gsccode.h"

/* Define the abstract type for a CMap. */
#ifndef gs_cmap_DEFINED
#  define gs_cmap_DEFINED
typedef struct gs_cmap_s gs_cmap;
#endif

/* ---------------- Procedural interface ---------------- */

/*
 * Decode a character from a string using a CMap, updating the index.
 * Return 0 for a CID or name, N > 0 for a character code where N is the
 * number of bytes in the code, or an error.  For undefined characters,
 * we return CID 0.
 */
int gs_cmap_decode_next(P6(const gs_cmap * pcmap, const gs_const_string * str,
			   uint * pindex, uint * pfidx,
			   gs_char * pchr, gs_glyph * pglyph));

#endif /* gsfcmap_INCLUDED */
