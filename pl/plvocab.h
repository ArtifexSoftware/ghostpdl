/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plvocab.h */
/* Map between glyph vocabularies */

/* Map a MSL glyph code to a Unicode character code. */
/* Note that the symbol set is required, because some characters map */
/* differently in different symbol sets. */
uint pl_map_MSL_to_Unicode(P2(uint msl, uint symbol_set));

/* Map a Unicode character code to a MSL glyph code similarly. */
uint pl_map_Unicode_to_MSL(P2(uint unicode, uint symbol_set));
