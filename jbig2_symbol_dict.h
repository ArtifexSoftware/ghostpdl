/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_symbol_dict.h,v 1.4 2002/06/22 21:20:38 giles Exp $

    symbol dictionary header
 */

/* the results of decoding a symbol dictionary */
typedef struct {
    int n_symbols;
    Jbig2Image **glyphs;
} Jbig2SymbolDict;

/* decode a symbol dictionary segment and store the results */
int
jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment,
			const byte *segment_data);
