/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_arith.h,v 1.3 2001/06/10 07:09:18 giles Exp $
*/

typedef struct _Jbig2ArithState Jbig2ArithState;

/* An arithmetic coding context is stored as a single byte, with the
   index in the low order 7 bits (actually only 6 are used), and the
   MPS in the top bit. */
typedef unsigned char Jbig2ArithCx;

Jbig2ArithState *
jbig2_arith_new (Jbig2WordStream *ws);

bool
jbig2_arith_decode (Jbig2ArithState *as, Jbig2ArithCx *pcx);
