typedef unsigned char byte;
typedef int bool;
typedef unsigned int uint32;
typedef int int32;
typedef short int16;
typedef signed char int8;

#define TRUE 1
#define FALSE 0

typedef struct _Jbig2Ctx Jbig2Ctx;

int32
get_bytes (Jbig2Ctx *ctx, byte *buf, int size, int off);

int16
get_int16 (Jbig2Ctx *ctx, int off);

int32
get_int32 (Jbig2Ctx *ctx, int off);

/* The word stream design is a compromise between simplicity and
   trying to amortize the number of method calls. Each ::get_next_word
   invocation pulls 4 bytes from the stream, packed big-endian into a
   32 bit word. The offset argument is provided as a convenience. It
   begins at 0 and increments by 4 for each successive invocation. */
typedef struct _Jbig2WordStream Jbig2WordStream;

struct _Jbig2WordStream {
  int32 (*get_next_word) (Jbig2WordStream *self, int offset);
};
