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

