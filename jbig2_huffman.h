/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id: jbig2_huffman.h,v 1.4 2002/06/15 16:02:53 giles Exp $
*/

#include "jbig2_hufftab.h"

Jbig2HuffmanState *
jbig2_huffman_new (Jbig2WordStream *ws);

int32_t
jbig2_huffman_get (Jbig2HuffmanState *hs,
		   const Jbig2HuffmanTable *table, bool *oob);

Jbig2HuffmanTable *
jbig2_build_huffman_table (const Jbig2HuffmanParams *params);
