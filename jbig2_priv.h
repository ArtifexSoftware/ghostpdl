void *
jbig2_alloc (Jbig2Allocator *allocator, size_t size);

void
jbig2_free (Jbig2Allocator *allocator, void *p);

void *
jbig2_realloc (Jbig2Allocator *allocator, void *p, size_t size);

int
jbig2_error (Jbig2Ctx *ctx, Jbig2Severity severity, int32_t seg_idx,
	     const char *fmt, ...);

typedef uint8_t byte;
typedef int bool;

typedef enum {
  JBIG2_FILE_HEADER,
  JBIG2_FILE_SEQUENTIAL_HEADER,
  JBIG2_FILE_SEQUENTIAL_BODY,
  JBIG2_FILE_RANDOM_HEADERS,
  JBIG2_FILE_RANDOM_BODIES,
  JBIG2_FILE_EOF
} Jbig2FileState;

struct _Jbig2Ctx {
  Jbig2Allocator *allocator;
  Jbig2Options options;
  const Jbig2Ctx *global_ctx;
  Jbig2ErrorCallback error_callback;
  void *error_callback_data;

  byte *buf;
  int buf_size;
  int buf_rd_ix;
  int buf_wr_ix;

  Jbig2FileState state;

  byte file_header_flags;
  int32_t n_pages;

  int n_sh;
  int n_sh_max;
  Jbig2SegmentHeader **sh_list;
  int sh_ix;
};

int32_t
jbig2_get_int32 (const byte *buf);

/* The word stream design is a compromise between simplicity and
   trying to amortize the number of method calls. Each ::get_next_word
   invocation pulls 4 bytes from the stream, packed big-endian into a
   32 bit word. The offset argument is provided as a convenience. It
   begins at 0 and increments by 4 for each successive invocation. */
typedef struct _Jbig2WordStream Jbig2WordStream;

struct _Jbig2WordStream {
  uint32_t (*get_next_word) (Jbig2WordStream *self, int offset);
};
