/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_segment.c,v 1.1 2002/06/15 16:16:20 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_symbol_dict.h"

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
