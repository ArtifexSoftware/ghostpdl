#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  JBIG2_SEVERITY_INFO,
  JBIG2_SEVERITY_WARNING,
  JBIG2_SEVERITY_FATAL
} Jbig2Severity;

typedef enum {
  JBIG2_OPTIONS_EMBEDDED = 1
} Jbig2Options;

typedef struct _Jbig2Allocator Jbig2Allocator;
typedef struct _Jbig2Ctx Jbig2Ctx;
typedef struct _Jbig2GlobalCtx Jbig2GlobalCtx;
typedef struct _Jbig2SegmentHeader Jbig2SegmentHeader;

struct _Jbig2SegmentHeader {
  int32_t segment_number;
  uint8_t flags;
  int referred_to_segment_count;
  int32_t page_association;
  int data_length;
};

typedef int (*Jbig2ErrorCallback) (void *data,
				  const char *msg, Jbig2Severity severity,
				  int32_t seg_idx);

struct _Jbig2Allocator {
  void *(*alloc) (Jbig2Allocator *allocator, size_t size);
  void (*free) (Jbig2Allocator *allocator, void *p);
  void *(*realloc) (Jbig2Allocator *allocator, void *p, size_t size);
};

Jbig2Ctx *jbig2_ctx_new (Jbig2Allocator *allocator,
			 Jbig2Options options,
			 Jbig2GlobalCtx *global_ctx,
			 Jbig2ErrorCallback error_callback,
			 void *error_callback_data);

int jbig2_write (Jbig2Ctx *ctx, const unsigned char *data, size_t size);

/* get_bits */

void jbig2_ctx_free (Jbig2Ctx *ctx);

Jbig2GlobalCtx *jbig2_make_global_ctx (Jbig2Ctx *ctx);

void jbig2_global_ctx_free (Jbig2GlobalCtx *global_ctx);

int jbig2_write_segment (Jbig2Ctx *ctx, Jbig2SegmentHeader *sh,
			 const uint8_t *segment_data);

#ifdef __cplusplus
}
#endif
