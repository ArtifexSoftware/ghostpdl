/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_huffman.h,v 1.2 2001/06/10 07:09:18 giles Exp $
*/

typedef struct _Jbig2HuffmanState Jbig2HuffmanState;
typedef struct _Jbig2HuffmanTable Jbig2HuffmanTable;
typedef struct _Jbig2HuffmanParams Jbig2HuffmanParams;

Jbig2HuffmanState *
jbig2_huffman_new (Jbig2WordStream *ws);

int32
jbig2_huffman_get (Jbig2HuffmanState *hs,
		   const Jbig2HuffmanTable *table, bool *oob);

Jbig2HuffmanTable *
jbig2_build_huffman_table (const Jbig2HuffmanParams *params);
