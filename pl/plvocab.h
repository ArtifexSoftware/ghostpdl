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
/*$Id$ */

/* plvocab.h */
/* Map between glyph vocabularies */

/* Map a MSL glyph code to a Unicode character code. */
/* Note that the symbol set is required, because some characters map */
/* differently in different symbol sets. */
uint pl_map_MSL_to_Unicode(uint msl, uint symbol_set);

/* Map a Unicode character code to a MSL glyph code similarly. */
uint pl_map_Unicode_to_MSL(uint unicode, uint symbol_set);
