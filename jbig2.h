/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
        
    $Id: jbig2.h,v 1.16 2003/02/05 15:09:59 giles Exp $
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _JBIG2_H
#define _JBIG2_H

/* warning levels */
typedef enum {
  JBIG2_SEVERITY_DEBUG,
  JBIG2_SEVERITY_INFO,
  JBIG2_SEVERITY_WARNING,
  JBIG2_SEVERITY_FATAL
} Jbig2Severity;

typedef enum {
  JBIG2_OPTIONS_EMBEDDED = 1
} Jbig2Options;

/* forward public structure declarations */
typedef struct _Jbig2Allocator Jbig2Allocator;
typedef struct _Jbig2Ctx Jbig2Ctx;
typedef struct _Jbig2GlobalCtx Jbig2GlobalCtx;
typedef struct _Jbig2Segment Jbig2Segment;
typedef struct _Jbig2Image Jbig2Image;

/* private structures */
typedef struct _Jbig2Page Jbig2Page;
typedef struct _Jbig2SymbolDictionary Jbig2SymbolDictionary;

/*
   this is the general image structure used by the jbig2dec library
   images are 1 bpp, packed into rows a byte at a time. stride gives
   the byte offset to the next row, while width and height define
   the size of the image area in pixels.
*/

struct _Jbig2Image {
        int             width, height, stride;
        uint8_t        *data;
};

Jbig2Image*     jbig2_image_new(Jbig2Ctx *ctx, int width, int height);
void            jbig2_image_free(Jbig2Ctx *ctx, Jbig2Image *image);


/* errors are returned from the library via a callback. If no callback
   is provided (a NULL argument is passed ot jbig2_ctx_new) a default
   handler is used which prints fatal errors to the stderr stream. */
   
/* error callback */
typedef int (*Jbig2ErrorCallback) (void *data,
				  const char *msg, Jbig2Severity severity,
				  int32_t seg_idx);

/* memory allocation is likewise done via a set of callbacks so that
   clients can better control memory usage. If a NULL is passed for
   this argumennt of jbig2_ctx_new, a default allocator based on malloc()
   is used. */
   
/* dynamic memory callbacks */
struct _Jbig2Allocator {
  void *(*alloc) (Jbig2Allocator *allocator, size_t size);
  void (*free) (Jbig2Allocator *allocator, void *p);
  void *(*realloc) (Jbig2Allocator *allocator, void *p, size_t size);
};

/* decoder context */
Jbig2Ctx *jbig2_ctx_new (Jbig2Allocator *allocator,
			 Jbig2Options options,
			 Jbig2GlobalCtx *global_ctx,
			 Jbig2ErrorCallback error_callback,
			 void *error_callback_data);
void jbig2_ctx_free (Jbig2Ctx *ctx);

/* global context for embedded streams */
Jbig2GlobalCtx *jbig2_make_global_ctx (Jbig2Ctx *ctx);
void jbig2_global_ctx_free (Jbig2GlobalCtx *global_ctx);

/* submit data to the decoder */
int jbig2_data_in (Jbig2Ctx *ctx, const unsigned char *data, size_t size);

/* get the next available decoded page image. NULL means there isn't one. */
Jbig2Image *jbig2_page_out (Jbig2Ctx *ctx);
/* mark a returned page image as no longer needed. */
int jbig2_release_page (Jbig2Ctx *ctx, Jbig2Image *image);
/* mark the current page as complete, simulating an end-of-page segment (for broken streams) */
int jbig2_complete_page (Jbig2Ctx *ctx);


/* segment header routines */

struct _Jbig2Segment {
  uint32_t number;
  uint8_t flags;
  uint32_t page_association;
  size_t data_length;
  int referred_to_segment_count;
  uint32_t *referred_to_segments;
  void *result;
};

Jbig2Segment *jbig2_parse_segment_header (Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size,
			    size_t *p_header_size);
int jbig2_parse_segment (Jbig2Ctx *ctx, Jbig2Segment *segment,
			 const uint8_t *segment_data);
void jbig2_free_segment (Jbig2Ctx *ctx, Jbig2Segment *segment);

#endif /* _JBIG2_H */

#ifdef __cplusplus
}
#endif
