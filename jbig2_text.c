/*
    jbig2dec
    
    Copyright (C) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_text.c,v 1.20 2002/08/15 14:54:45 giles Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memset() */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"
#include "jbig2_arith_iaid.h"
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
    int SBDSOFFSET;
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


/**
 * jbig2_decode_text_region: decode a text region segment
 *
 * @ctx: jbig2 decoder context
 * @segment: jbig2 segment (header) structure
 * @params: parameters from the text region header
 * @dicts: an array of referenced symbol dictionaries
 * @n_dicts: the number of referenced symbol dictionaries
 * @image: image structure in which to store the decoded region bitmap
 * @data: pointer to text region data to be decoded
 * @size: length of text region data
 *
 * Implements the text region decoding proceedure
 * described in section 6.4 of the JBIG2 spec.
 *
 * returns: 0 on success
 **/
static int
jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                             const Jbig2TextRegionParams *params,
                             const Jbig2SymbolDict * const *dicts, const int n_dicts,
                             Jbig2Image *image,
                             const byte *data, const size_t size)
{
    /* relevent bits of 6.4.4 */
    uint32_t NINSTANCES;
    uint32_t ID;
    int32_t STRIPT;
    int32_t FIRSTS;
    int32_t DT;
    int32_t DFS;
    int32_t IDS;
    int32_t CURS;
    int32_t CURT;
    int S,T;
    int x,y;
    bool first_symbol;
    uint32_t index, max_id;
    Jbig2Image *IB;
    Jbig2ArithState *as;
    Jbig2ArithIntCtx *IADT = NULL;
    Jbig2ArithIntCtx *IAFS = NULL;
    Jbig2ArithIntCtx *IADS = NULL;
    Jbig2ArithIntCtx *IAIT = NULL;
    Jbig2ArithIaidCtx *IAID = NULL;
    int code;
    
    max_id = 0;
    for (index = 0; index < n_dicts; index ++) {
        max_id += dicts[index]->n_symbols;
    }
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "symbol list contains %d glyphs in %d dictionaries", max_id, n_dicts);
    
    if (params->SBREFINE) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "text regions with refinement bitmaps NYI");
    }
    
    if (!params->SBHUFF) {
	int SBSYMCODELEN;

        Jbig2WordStream *ws = jbig2_word_stream_buf_new(ctx, data, size);
        as = jbig2_arith_new(ctx, ws);
        IADT = jbig2_arith_int_ctx_new(ctx);
        IAFS = jbig2_arith_int_ctx_new(ctx);
        IADS = jbig2_arith_int_ctx_new(ctx);
        IAIT = jbig2_arith_int_ctx_new(ctx);
	/* Table 31 */
	for (SBSYMCODELEN = 0; (1 << SBSYMCODELEN) < max_id; SBSYMCODELEN++);
        IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
    }
    
    /* 6.4.5 (1) */
    memset(image->data, image->stride*image->height, params->SBDEFPIXEL ? 0xFF: 0x00);
    
    /* 6.4.6 */
    if (params->SBHUFF) {
        /* todo */
    } else {
        code = jbig2_arith_int_decode(IADT, as, &STRIPT);
    }

    /* 6.4.5 (2) */
    STRIPT *= -(params->SBSTRIPS);
    FIRSTS = 0;
    NINSTANCES = 0;
    
    /* 6.4.5 (3) */
    while (NINSTANCES < params->SBNUMINSTANCES) {
        /* (3b) */
        if (params->SBHUFF) {
            /* todo */
        } else {
            code = jbig2_arith_int_decode(IADT, as, &DT);
        }
        DT *= params->SBSTRIPS;
        STRIPT += DT;
       
	first_symbol = TRUE;
	/* 6.4.5 (3c) - decode symbols in strip */
	for (;;) {
	    /* (3c.i) */
	    if (first_symbol) {
		/* 6.4.7 */
		if (params->SBHUFF) {
		    /* todo */
		} else {
		    code = jbig2_arith_int_decode(IAFS, as, &DFS);
		}
		FIRSTS += DFS;
		CURS = FIRSTS;
		first_symbol = FALSE;

	    } else {
		/* (3c.ii / 6.4.8) */
		if (params->SBHUFF) {
		    /* todo */
		} else {
		    code = jbig2_arith_int_decode(IADS, as, &IDS);
		}
		if (code) {
		    break;
		}
		CURS += IDS + params->SBDSOFFSET;
	    }

	    /* (3c.iii / 6.4.9) */
	    if (params->SBSTRIPS == 1) {
		CURT = 0;
	    } else if (params->SBHUFF) {
		/* todo */
	    } else {
		code = jbig2_arith_int_decode(IAIT, as, &CURT);
	    }
	    T = STRIPT + CURT;

	    /* (3b.iv / 6.4.10) decode the symbol id */
	    if (params->SBHUFF) {
		/* todo */
	    } else {
		code = jbig2_arith_iaid_decode(IAID, as, &ID);
	    }
	    if (ID < 0 || ID >= max_id) {
		return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "symbol id out of range! (%d/%d)", ID, max_id);
	    }

	    /* (3c.v) look up the symbol bitmap IB */
	    {
		int id = ID;

		index = 0;
		while (id >= dicts[index]->n_symbols)
		    id -= dicts[index++]->n_symbols;
		IB = dicts[index]->glyphs[id];
	    }
        
	    /* (3c.vi) */
	    if ((!params->TRANSPOSED) && (params->REFCORNER > 1)) {
		CURS += IB->width - 1;
	    } else if ((params->TRANSPOSED) && !(params->REFCORNER & 1)) {
		CURS += IB->height - 1;
	    }
        
	    /* (3c.vii) */
	    S = CURS;
        
	    /* (3c.viii) */
	    if (!params->TRANSPOSED) {
		switch (params->REFCORNER) {  /* FIXME: double check offsets */
		case JBIG2_CORNER_TOPLEFT: x = S; y = T; break;
		case JBIG2_CORNER_TOPRIGHT: x = S - IB->width + 1; y = T; break;
		case JBIG2_CORNER_BOTTOMLEFT: x = S; y = T - IB->height + 1; break;
		case JBIG2_CORNER_BOTTOMRIGHT: x = S - IB->width + 1; y = T - IB->height + 1; break;
		}
	    } else { /* TRANSPOSED */
		switch (params->REFCORNER) {
		case JBIG2_CORNER_TOPLEFT: y = T; x = T; break;
		case JBIG2_CORNER_TOPRIGHT: y = S - IB->width + 1; x = T; break;
		case JBIG2_CORNER_BOTTOMLEFT: y = S; x = T - IB->height + 1; break;
		case JBIG2_CORNER_BOTTOMRIGHT: y = S - IB->width + 1; x = T - IB->height + 1; break;
		}
	    }
        
	    /* (3c.ix) */
#ifdef DEBUG
	    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
			"composing glyph id %d: %dx%d @ (%d,%d) symbol %d/%d", 
			ID, IB->width, IB->height, x, y, NINSTANCES + 1,
			params->SBNUMINSTANCES);
#endif
	    jbig2_image_compose(ctx, image, IB, x, y, params->SBCOMBOP);
        
	    /* (3c.x) */
	    if ((!params->TRANSPOSED) && (params->REFCORNER < 2)) {
		CURS += IB->width -1 ;
	    } else if ((params->TRANSPOSED) && (params->REFCORNER & 1)) {
		CURS += IB->height - 1;
	    }
        
	    /* (3c.xi) */
	    NINSTANCES++;
	}
        /* end strip */
    }
    /* 6.4.5 (4) */
    
    return 0;
}

/* find a segment by number */
static Jbig2Segment *
find_segment(Jbig2Ctx *ctx, uint32_t number)
{
    int index, index_max = ctx->segment_index;
    
    /* FIXME: binary search would be better? */
    for (index = index_max; index >= 0; index--)
        if (ctx->segments[index]->number == number)
            return (ctx->segments[index]);
        
    /* didn't find a match */
    return NULL;
}

/* count the number of dictionary segments referred to by the given segment */
static int
count_referred_dicts(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    int n_dicts = 0;

    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0))
            n_dicts++;
    }
    
    return (n_dicts);
}

/* return an array of pointers to symbol dictionaries referred to by the given segment */
static Jbig2SymbolDict **
list_referred_dicts(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    Jbig2SymbolDict **dicts;
    int n_dicts = count_referred_dicts(ctx, segment);
    int dindex = 0;
    
    dicts = jbig2_alloc(ctx->allocator, sizeof(Jbig2SymbolDict *) * n_dicts);
    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0))
            dicts[dindex++] = (Jbig2SymbolDict *)rsegment->result;
    }
    
    return (dicts);
}

/**
 * jbig2_read_text_info: read a text region segment header
 **/
int
jbig2_parse_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    int offset = 0;
    Jbig2RegionSegmentInfo region_info;
    Jbig2TextRegionParams params;
    Jbig2Image *image, *page_image;
    Jbig2SymbolDict **dicts;
    int n_dicts;
    uint16_t flags;
    uint16_t huffman_flags;
    int8_t sbrat[4];
    int code;
    
    /* 7.4.1 */
    if (segment->data_length < 17)
        goto too_short;
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;
    
    /* 7.4.3.1.1 */
    flags = jbig2_get_int16(segment_data + offset);
    offset += 2;
    
    params.SBHUFF = flags & 0x0001;
    params.SBREFINE = flags & 0x0002;
    params.SBSTRIPS = 1 << ((flags & 0x000c) >> 2);
    params.REFCORNER = (flags & 0x0030) >> 4;
    params.TRANSPOSED = flags & 0x0040;
    params.SBCOMBOP = (flags & 0x00e0) >> 7;
    params.SBDEFPIXEL = flags & 0x0100;
    params.SBDSOFFSET = (flags & 0x7C00) >> 10;
    params.SBRTEMPLATE = flags & 0x8000;
    
    if (params.SBHUFF)	/* Huffman coding */
      {
        /* 7.4.3.1.2 */
        huffman_flags = jbig2_get_int16(segment_data + offset);
        offset += 2;
        
        if (huffman_flags & 0x8000)
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                "reserved bit 15 of text region huffman flags is not zero");
      }
    else	/* arithmetic coding */
      {
        /* 7.4.3.1.3 */
        if ((params.SBREFINE) && !(params.SBRTEMPLATE))
          {
            sbrat[0] = segment_data[offset];
            sbrat[0] = segment_data[offset + 1];
            sbrat[0] = segment_data[offset + 2];
            sbrat[0] = segment_data[offset + 3];
            offset += 4;
      }
    
    /* 7.4.3.1.4 */
    params.SBNUMINSTANCES = jbig2_get_int32(segment_data + offset);
    offset += 4;
    
    if (params.SBHUFF) {
        /* 7.4.3.1.5 */
        /* todo: symbol ID huffman decoding table decoding */
        
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "symbol id huffman table decoding NYI");
            
        /* 7.4.3.1.6 */
        params.SBHUFFFS = (huffman_flags & 0x0003);
        params.SBHUFFDS = (huffman_flags & 0x000c) >> 2;
        params.SBHUFFDT = (huffman_flags & 0x0030) >> 4;
        params.SBHUFFRDW = (huffman_flags & 0x00c0) >> 6;
        params.SBHUFFRDH = (huffman_flags & 0x0300) >> 8;
        params.SBHUFFRDX = (huffman_flags & 0x0c00) >> 10;
        params.SBHUFFRDY = (huffman_flags & 0x3000) >> 12;
        /* todo: conformance */
        params.SBHUFFRSIZE = huffman_flags & 0x8000;
        
        /* 7.4.3.1.7 */
        /* todo: symbol ID huffman table decoding */
    }
    
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "text region: %d x %d @ (%d,%d) %d symbols",
        region_info.width, region_info.height,
        region_info.x, region_info.y, params.SBNUMINSTANCES);
    }
    
    /* compose the list of symbol dictionaries */
    n_dicts = count_referred_dicts(ctx, segment);
    if (n_dicts != 0) {
        dicts = list_referred_dicts(ctx, segment);
    } else {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "text region refers to no symbol dictionaries!");
    }
    
    page_image = ctx->pages[ctx->current_page].image;
    image = jbig2_image_new(ctx, region_info.width, region_info.height);

    code = jbig2_decode_text_region(ctx, segment, &params,
                dicts, n_dicts, image,
                segment_data + offset, segment->data_length - offset);

    /* todo: check errors */

    if ((segment->flags & 63) == 4) {
        /* we have an intermediate region here. save it for later */
        segment->result = image;
    } else {
        /* otherwise composite onto the page */
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, 
            "composing %dx%d decoded text region onto page at (%d, %d)",
            region_info.width, region_info.height, region_info.x, region_info.y);
        jbig2_image_compose(ctx, page_image, image, region_info.x, region_info.y, JBIG2_COMPOSE_OR);
        if (image != page_image)
            jbig2_image_free(ctx, image);
    }
    
    /* success */            
    return 0;
    
    too_short:
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                    "Segment too short");
}
