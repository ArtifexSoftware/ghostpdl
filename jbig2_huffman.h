/*
    jbig2dec
    
    Copyright (c) 2001-2005 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id$
*/

#ifndef JBIG2_HUFFMAN_H
#define JBIG2_HUFFMAN_H

/* Huffman coder interface */

typedef struct _Jbig2HuffmanEntry Jbig2HuffmanEntry;
typedef struct _Jbig2HuffmanState Jbig2HuffmanState;
typedef struct _Jbig2HuffmanTable Jbig2HuffmanTable;
typedef struct _Jbig2HuffmanParams Jbig2HuffmanParams;

Jbig2HuffmanState *
jbig2_huffman_new (Jbig2Ctx *ctx, Jbig2WordStream *ws);

void
jbig2_huffman_free (Jbig2Ctx *ctx, Jbig2HuffmanState *hs);

int32_t
jbig2_huffman_get (Jbig2HuffmanState *hs,
		   const Jbig2HuffmanTable *table, bool *oob);

Jbig2HuffmanTable *
jbig2_build_huffman_table (Jbig2Ctx *ctx,
			   const Jbig2HuffmanParams *params);

#endif /* JBIG2_HUFFMAN_H */
