/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_segment.c,v 1.2 2002/06/17 21:06:38 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_symbol_dict.h"

Jbig2SegmentHeader *
jbig2_parse_segment_header (Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size,
			    size_t *p_header_size)
{
  Jbig2SegmentHeader *result;
  uint8_t rtscarf;
  uint32_t rtscarf_long;
  int referred_to_segment_count;
  int referred_to_segment_size;
  int pa_size;
  int offset;

  /* minimum possible size of a jbig2 segment header */
  if (buf_size < 11)
    return NULL;

  result = (Jbig2SegmentHeader *)jbig2_alloc(ctx->allocator,
					     sizeof(Jbig2SegmentHeader));

  /* 7.2.2 */
  result->segment_number = jbig2_get_int32 (buf);

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
  /* todo: read referred to segment numbers */
  /* For now, we skip them. */
  referred_to_segment_size = result->segment_number <= 256 ? 1:
    result->segment_number <= 65536 ? 2:
    4;
  offset += referred_to_segment_count * referred_to_segment_size;

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
jbig2_free_segment_header (Jbig2Ctx *ctx, Jbig2SegmentHeader *sh)
{
  jbig2_free (ctx->allocator, sh);
}

int jbig2_write_segment (Jbig2Ctx *ctx, Jbig2SegmentHeader *sh,
			 const uint8_t *segment_data)
{
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
	      "Segment %d, flags=%x, type=%d, data_length=%d",
	      sh->segment_number, sh->flags, sh->flags & 63,
	      sh->data_length);
  switch (sh->flags & 63)
    {
    case 0:
      return jbig2_symbol_dictionary(ctx, sh, segment_data);
    case 38:
      return jbig2_immediate_generic_region(ctx, sh, segment_data);
    case 48:
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
        "unhandled segment type 'page info'");
      break;
    case 49:
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
        "unhandled segment type 'end of page'");
      break;
    case 50:
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
        "unhandled segment type 'end of stripe'");
      break;
    case 51:
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
        "unhandled segment type 'end of file'");
      break;
    case 52:
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
        "unhandled segment type 'profile'");
      break;
    case 53:
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
        "unhandled table segment");
      break;
    case 62:
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
        "unhandled extension segment");
      break;
    default:
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
          "unknown segment type %d", sh->flags & 63);
    }
  return 0;
}

#ifdef GILES

int dump_segment (Jbig2Ctx *ctx)
{
  Jbig2SegmentHeader *sh;
  Jbig2SymbolDictionary *sd;
  Jbig2PageInfo	*page_info;

  sh = jbig2_read_segment_header(ctx);
  if (sh == NULL)
    return TRUE;
  
  printf("segment %d (%d bytes)\t", sh->segment_number, sh->data_length);
  switch (sh->flags & 63)
    {
    case 0:
      sd = jbig2_read_symbol_dictionary(ctx);
	  printf("\n");
      dump_symbol_dictionary(sd);
      break;
	case 4:
		printf("intermediate text region:");
		break;
	case 6:
		printf("immediate text region:");
		break;
	case 7:
		printf("immediate lossless text region:");
		break;
	case 16:
		printf("pattern dictionary:");
		break;
	case 20:
		printf("intermediate halftone region:");
		break;
	case 22:
		printf("immediate halftone region:");
		break;
	case 23:
		printf("immediate lossless halftone region:");
		break;
	case 36:
		printf("intermediate generic region:");
		break;
	case 38:
		printf("immediate generic region:");
		break;
	case 39:
		printf("immediate lossless generic region:");
		break;
	case 40:
		printf("intermediate generic refinement region:");
		break;
	case 42:
		printf("immediate generic refinement region:");
		break;
	case 43:
		printf("immediate lossless generic refinement region:");
		break;
	case 48:
		page_info = jbig2_read_page_info(ctx);
		printf("page info:\n");
		if (page_info) dump_page_info(page_info);
		break;
	case 49:
		printf("end of page");
		break;
	case 50:
		printf("end of stripe");
		break;
        case 51:
            printf("end of file\n");
            return TRUE;
            break;
	case 52:
		printf("profiles:");
		break;
	case 53:
		printf("tables:");
		break;
	case 62:
		printf("extension:");
		break;
	default:
		printf("UNKNOWN SEGMENT TYPE!!!");
    }
	printf("\tflags = %02x, page %d\n",
	  sh->flags, sh->page_association);

  ctx->offset += sh->data_length;
  return FALSE;
}

#endif /* GILES */
