/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
        
    $Id: jbig2.h,v 1.6 2002/06/17 21:11:51 giles Exp $
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>	// for C99 types -- need a more portable sol'n

typedef enum {
  JBIG2_SEVERITY_DEBUG,
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
typedef struct _Jbig2PageInfo Jbig2PageInfo;
typedef struct _Jbig2SymbolDictionary Jbig2SymbolDictionary;

struct _Jbig2SegmentHeader {
  int32_t segment_number;
  uint8_t flags;
  int referred_to_segment_count;
  int32_t page_association;
  int data_length;
};

struct _Jbig2PageInfo {
	uint32_t	height, width;	/* in pixels */
	uint32_t	x_resolution,
                        y_resolution;	/* in pixels per meter */
	uint16_t	stripe_size;
	int		striped;
	uint8_t		flags;
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

Jbig2SegmentHeader *jbig2_parse_segment_header (Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size,
			    size_t *p_header_size);
int jbig2_write_segment (Jbig2Ctx *ctx, Jbig2SegmentHeader *sh,
			 const uint8_t *segment_data);
void jbig2_free_segment_header (Jbig2Ctx *ctx, Jbig2SegmentHeader *sh);

#ifdef __cplusplus
}
#endif
