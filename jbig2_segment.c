/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_segment.c,v 1.11 2002/07/04 16:33:44 giles Exp $
*/

#include <stdlib.h> /* size_t */
#include <stdint.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_symbol_dict.h"

Jbig2Segment *
jbig2_parse_segment_header (Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size,
			    size_t *p_header_size)
{
  Jbig2Segment *result;
  uint8_t rtscarf;
  uint32_t rtscarf_long;
  uint32_t *referred_to_segments;
  int referred_to_segment_count;
  int referred_to_segment_size;
  int pa_size;
  int offset;

  /* minimum possible size of a jbig2 segment header */
  if (buf_size < 11)
    return NULL;

  result = (Jbig2Segment *)jbig2_alloc(ctx->allocator,
					     sizeof(Jbig2Segment));

  /* 7.2.2 */
  result->number = jbig2_get_int32 (buf);

  /* 7.2.3 */
  result->flags = buf[4];

  /* 7.2.4 */
  rtscarf = buf[5];
  if ((rtscarf & 0xe0) == 0xe0)
    {
      rtscarf_long = jbig2_get_int32(buf + 5);
      referred_to_segment_count = rtscarf_long & 0x1fffffff;
      offset = 5 + 4 + (referred_to_segment_count + 1) / 8;
    }
  else
    {
      referred_to_segment_count = (rtscarf >> 5);
      offset = 5 + 1;
    }
  result->referred_to_segment_count = referred_to_segment_count;

  /* 7.2.5 */
  if (referred_to_segment_count)
    {
      int i;

      referred_to_segment_size = result->number <= 256 ? 1:
        result->number <= 65536 ? 2 : 4;
      referred_to_segments = jbig2_alloc(ctx->allocator, referred_to_segment_count * referred_to_segment_size);
    
      for (i = 0; i < referred_to_segment_count; i++) {
        referred_to_segments[i] = 
          (referred_to_segment_size == 1) ? buf[offset] :
          (referred_to_segment_size == 2) ? jbig2_get_int16(buf+offset) :
            jbig2_get_int32(buf + offset);
        offset += referred_to_segment_size;
      }
      result->referred_to_segments = referred_to_segments;
    }
  else /* no referred-to segments */
    {
      result->referred_to_segments = NULL;
    }
  
  /* 7.2.6 */
  pa_size = result->flags & 0x40 ? 4 : 1;

  if (offset + pa_size + 4 > buf_size)
    {
      jbig2_free (ctx->allocator, result);
      return NULL;
    }
  
  if (result->flags & 0x40) {
	result->page_association = jbig2_get_int32(buf + offset);
	offset += 4;
  } else {
	result->page_association = buf[offset++];
  }
  
  /* 7.2.7 */
  result->data_length = jbig2_get_int32 (buf + offset);
  *p_header_size = offset + 4;

  return result;
}

void
jbig2_free_segment (Jbig2Ctx *ctx, Jbig2Segment *segment)
{
  if (segment->referred_to_segments != NULL) {
    jbig2_free(ctx->allocator, segment->referred_to_segments);
  }
  /* todo: free result */
  jbig2_free (ctx->allocator, segment);
}

void
jbig2_get_region_segment_info(Jbig2RegionSegmentInfo *info,
			      const byte *segment_data)
{
  /* 7.4.1 */
  info->width = jbig2_get_int32(segment_data);
  info->height = jbig2_get_int32(segment_data + 4);
  info->x = jbig2_get_int32(segment_data + 8);
  info->y = jbig2_get_int32(segment_data + 12);
  info->flags = segment_data[16];
}

int jbig2_parse_segment (Jbig2Ctx *ctx, Jbig2Segment *segment,
			 const uint8_t *segment_data)
{
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	      "Segment %d, flags=%x, type=%d, data_length=%d",
	      segment->number, segment->flags, segment->flags & 63,
	      segment->data_length);
  switch (segment->flags & 63)
    {
    case 0:
      return jbig2_symbol_dictionary(ctx, segment, segment_data);
    case 4:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'intermediate text region'");
    case 6:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'immediate text region'");
    case 7:
      return jbig2_read_text_info(ctx, segment, segment_data);
    case 16:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'pattern dictionary'");
    case 20:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'intermediate halftone region'");
    case 22:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'immediate halftone region'");
    case 23:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'immediate lossless halftone region'");
    case 36:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'intermediate generic region'");
    case 38:
      return jbig2_immediate_generic_region(ctx, segment, segment_data);
    case 39:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'immediate lossless generic region'");
    case 40:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'intermediate generic refinement region'");
    case 42:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'immediate generic refinement region'");
    case 43:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'immediate lossless generic refinement region'");
    case 48:
      return jbig2_read_page_info(ctx, segment, segment_data);
    case 49:
      return jbig2_complete_page(ctx, segment, segment_data);
    case 50:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'end of stripe'");
    case 51:
      ctx->state = JBIG2_FILE_EOF;
      return jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "end of file");
    case 52:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled segment type 'profile'");
    case 53:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled table segment");
    case 62:
      return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled extension segment");
    default:
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
          "unknown segment type %d", segment->flags & 63);
    }
  return 0;
}
