typedef struct _Jbig2ArithState Jbig2ArithState;

/* An arithmetic coding context is stored as a single byte, with the
   index in the low order 7 bits (actually only 6 are used), and the
   MPS in the top bit. */
typedef unsigned char Jbig2ArithCx;

Jbig2ArithState *
jbig2_arith_new (Jbig2WordStream *ws);

bool
jbig2_arith_decode (Jbig2ArithState *as, Jbig2ArithCx *pcx);
