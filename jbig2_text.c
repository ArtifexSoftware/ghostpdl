/*
    jbig2dec
    
    Copyright (C) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_text.c,v 1.1 2002/06/20 15:42:48 giles Exp $
*/

#include <stddef.h>
#include <stdint.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"
#include "jbig2_generic.h"
#include "jbig2_symbol_dict.h"

/**
 * jbig2_read_text_info: read a text region segment header
 **/
int
jbig2_read_text_info(Jbig2Ctx *ctx, Jbig2SegmentHeader *sh, const byte *segment_data)
{
    int offset = 0;
    Jbig2RegionSegmentInfo region_info;
    uint32_t num_instances;
    uint16_t segment_flags;
    uint16_t huffman_flags;
    int8_t sbrat[4];	/* FIXME: needs to be explicitly signed? */
    
    /* 7.4.1 */
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;
    
    /* 7.4.3.1.1 */
    segment_flags = jbig2_get_int16(segment_data + offset);
    offset += 2;
    
    if (segment_flags & 1)	/* Huffman coding */
      {
        /* 7.4.3.1.2 */
        huffman_flags = jbig2_get_int16(segment_data + offset);
        offset += 2;
      }
    else	/* arithmetic coding */
      {
        /* 7.4.3.1.3 */
        if ((segment_flags & 0x02) && !(segment_flags & 0x80)) /* SBREFINE & !SBRTEMPLATE */
          {
            sbrat[0] = segment_data[offset];
            sbrat[0] = segment_data[offset + 1];
            sbrat[0] = segment_data[offset + 2];
            sbrat[0] = segment_data[offset + 3];
            offset += 4;
      }
    
    /* 7.4.3.1.4 */
    num_instances = jbig2_get_int32(segment_data + offset);
    offset += 4;
    
    /* 7.4.3.1.7 */
    if (segment_flags & 0x01) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
            "symbol id huffman table decoding NYI");
    }
    
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
        "text region: %d x %d @ (%d,%d) %d symbols",
        region_info.width, region_info.height,
        region_info.x, region_info.y, num_instances);
    }
}