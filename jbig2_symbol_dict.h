/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

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
