/*
    jbig2dec
    
    Copyright (C) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_text.c,v 1.3 2002/06/24 15:51:57 giles Exp $
*/

#include <stddef.h>
#include <stdint.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"
#include "jbig2_generic.h"
#include "jbig2_symbol_dict.h"

typedef enum {
    JBIG2_CORNER_BOTTOMLEFT = 0,
    JBIG2_CORNER_TOPLEFT = 1,
    JBIG2_CORNER_BOTTOMRIGHT = 2,
    JBIG2_CORNER_TOPRIGHT = 3
} Jbig2RefCorner;

typedef struct {
    bool SBHUFF;
    bool SBREFINE;
    bool SBDEFPIXEL;
    Jbig2ComposeOp SBCOMBOP;
    bool TRANSPOSED;
    Jbig2RefCorner REFCORNER;
    int SBDOFFSET;
    /* SBW */
    /* SBH */
    uint32_t SBNUMINSTANCES;
    int SBSTRIPS;
    /* SBNUMSYMS */
    int *SBSYMCODES;
    /* SBSYMCODELEN */
    /* SBSYMS */
    int SBHUFFFS;
    int SBHUFFDS;
    int SBHUFFDT;
    int SBHUFFRDW;
    int SBHUFFRDH;
    int SBHUFFRDX;
    int SBHUFFRDY;
    bool SBHUFFRSIZE;
    bool SBRTEMPLATE;
    int8_t sbrat[4];
} Jbig2TextRegionParams;

int jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                             const Jbig2TextRegionParams *params,
                             const Jbig2RegionSegmentInfo *info,
                             Jbig2Image *image,
                             const byte *data, size_t size)
{
}

/**
 * jbig2_read_text_info: read a text region segment header
 **/
int
jbig2_read_text_info(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    int offset = 0;
    Jbig2RegionSegmentInfo region_info;
    Jbig2TextRegionParams params;
    Jbig2Image *image, *page_image;
    int code;
    uint32_t num_instances;
    uint16_t segment_flags;
    uint16_t huffman_flags;
    int8_t sbrat[4];
    
    /* 7.4.1 */
    if (segment->data_length < 17)
        goto too_short;
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;
    
    /* 7.4.3.1.1 */
    segment_flags = jbig2_get_int16(segment_data + offset);
    offset += 2;
    
    if (segment_flags & 0x01)	/* Huffman coding */
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
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "symbol id huffman table decoding NYI");
    }
    
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "text region: %d x %d @ (%d,%d) %d symbols",
        region_info.width, region_info.height,
        region_info.x, region_info.y, num_instances);
    }
    
    page_image = ctx->pages[ctx->current_page].image;
    image = jbig2_image_new(ctx, region_info.width, region_info.height);

    code = jbig2_decode_text_region(ctx, segment, &params,
                &region_info, image,
                segment_data + offset, segment->data_length - offset);

    /* todo: check errors */

    jbig2_image_compose(ctx, page_image, image, region_info.x, region_info.y, JBIG2_COMPOSE_OR);
    if (image != page_image)
        jbig2_image_free(ctx, image);

    /* success */            
    return 0;
    
    too_short:
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Segment too short");
}