/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
        
    $Id: jbig2_generic.c,v 1.4 2002/04/25 23:24:08 raph Exp $
*/

/**
 * Generic region handlers.
 **/

#define OUTPUT_PBM

#include <stdint.h>
#include <stddef.h>
#ifdef OUTPUT_PBM
#include <stdio.h>
#endif
#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_mmr.h"

typedef struct {
  int32_t width;
  int32_t height;
  int32_t x;
  int32_t y;
  byte flags;
} Jbig2RegionSegmentInfo;

static int
jbig2_decode_generic_template0(Jbig2Ctx *ctx,
			       int32_t seg_number,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       byte *gbreg,
			       Jbig2ArithCx *GB_stats)
{
  int GBW = params->GBW;
  int rowstride = (GBW + 7) >> 3;
  int x, y;
  byte *gbreg_line = gbreg;
  bool LTP = 0;

  /* todo: currently we only handle the nominal gbat location */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, params->GBH);
#endif

  for (y = 0; y < params->GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 6 : 0;
      CONTEXT = (line_m1 & 0x7f0) | (line_m2 & 0xf800);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 6: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0x7bf7) << 1) | bit |
		((line_m1 >> (7 - x_minor)) & 0x10) |
		((line_m2 >> (7 - x_minor)) & 0x800);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template1(Jbig2Ctx *ctx,
			       int32_t seg_number,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       byte *gbreg,
			       Jbig2ArithCx *GB_stats)
{
  int GBW = params->GBW;
  int rowstride = (GBW + 7) >> 3;
  int x, y;
  byte *gbreg_line = gbreg;
  bool LTP = 0;

  /* todo: currently we only handle the nominal gbat location */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, params->GBH);
#endif

  for (y = 0; y < params->GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 5 : 0;
      CONTEXT = ((line_m1 >> 1) & 0x1f8) | ((line_m2 >> 1) & 0x1e00);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 5: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0xefb) << 1) | bit |
		((line_m1 >> (8 - x_minor)) & 0x8) |
		((line_m2 >> (8 - x_minor)) & 0x200);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template2(Jbig2Ctx *ctx,
			       int32_t seg_number,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       byte *gbreg,
			       Jbig2ArithCx *GB_stats)
{
  int GBW = params->GBW;
  int rowstride = (GBW + 7) >> 3;
  int x, y;
  byte *gbreg_line = gbreg;
  bool LTP = 0;

  /* todo: currently we only handle the nominal gbat location */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, params->GBH);
#endif

  for (y = 0; y < params->GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 4 : 0;
      CONTEXT = ((line_m1 >> 3) & 0x7c) | ((line_m2 >> 3) & 0x380);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 4: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0x1bd) << 1) | bit |
		((line_m1 >> (10 - x_minor)) & 0x4) |
		((line_m2 >> (10 - x_minor)) & 0x80);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template2a(Jbig2Ctx *ctx,
			       int32_t seg_number,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       byte *gbreg,
			       Jbig2ArithCx *GB_stats)
{
  int GBW = params->GBW;
  int rowstride = (GBW + 7) >> 3;
  int x, y;
  byte *gbreg_line = gbreg;
  bool LTP = 0;

  /* This is a special case for GBATX1 = 3, GBATY1 = -1 */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, params->GBH);
#endif

  for (y = 0; y < params->GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 4 : 0;
      CONTEXT = ((line_m1 >> 3) & 0x78) | ((line_m1 >> 2) & 0x4) | ((line_m2 >> 3) & 0x380);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 4: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0x1b9) << 1) | bit |
		((line_m1 >> (10 - x_minor)) & 0x8) |
		((line_m1 >> (9 - x_minor)) & 0x4) |
		((line_m2 >> (10 - x_minor)) & 0x80);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

/**
 * jbig2_decode_generic_region: Decode a generic region.
 * @ctx: The context for allocation and error reporting.
 * @params: Parameters, as specified in Table 2.
 * @as: Arithmetic decoder state.
 * @gbreg: Where to store the decoded data.
 * @GB_stats: Arithmetic stats.
 *
 * Decodes a generic region, according to section 6.2. The caller should
 * have allocated the memory for @gbreg, which is packed 8 pixels to a
 * byte, scanlines aligned to one byte boundaries.
 *
 * Because this API is based on an arithmetic decoding state, it is
 * not suitable for MMR decoding.
 *
 * Return code: 0 on success.
 **/
int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
			    int32_t seg_number,
			    const Jbig2GenericRegionParams *params,
			    Jbig2ArithState *as,
			    byte *gbreg,
			    Jbig2ArithCx *GB_stats)
{
  if (!params->MMR && params->GBTEMPLATE == 0)
    return jbig2_decode_generic_template0(ctx, seg_number,
					  params, as, gbreg, GB_stats);
  else if (!params->MMR && params->GBTEMPLATE == 1)
    return jbig2_decode_generic_template1(ctx, seg_number,
					  params, as, gbreg, GB_stats);
  else if (!params->MMR && params->GBTEMPLATE == 2)
    {
      if (params->gbat[0] == 3 && params->gbat[1] == 255)
	return jbig2_decode_generic_template2a(ctx, seg_number,
					       params, as, gbreg, GB_stats);
      else
	return jbig2_decode_generic_template2(ctx, seg_number,
					       params, as, gbreg, GB_stats);
    }
  {
    int i;
    for (i = 0; i < 8; i++)
      printf ("gbat[%d] = %d\n", i, params->gbat[i]);
  }
  jbig2_error(ctx, JBIG2_SEVERITY_WARNING, seg_number,
	      "decode_generic_region: MMR=%d, GBTEMPLATE=%d NYI",
	      params->MMR, params->GBTEMPLATE);
  return -1;
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

int
jbig2_immediate_generic_region(Jbig2Ctx *ctx, Jbig2SegmentHeader *sh,
			       const uint8_t *segment_data)
{
  Jbig2RegionSegmentInfo rsi;
  byte seg_flags;
  int8_t gbat[8];
  int offset;
  int gbat_bytes = 0;
  Jbig2GenericRegionParams params;
  int code;
  byte *gbreg;
  Jbig2WordStream *ws;
  Jbig2ArithState *as;
  Jbig2ArithCx *GB_stats = NULL;

  /* 7.4.6 */
  if (sh->data_length < 18)
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, sh->segment_number,
		       "Segment too short");

  jbig2_get_region_segment_info(&rsi, segment_data);
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
	      "generic region: %d x %d @ (%d, %d), flags = %02x",
	      rsi.width, rsi.height, rsi.x, rsi.y, rsi.flags);

  /* 7.4.6.2 */
  seg_flags = segment_data[17];
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
	      "segment flags = %02x",
	      seg_flags);
  if ((seg_flags & 1) && (seg_flags & 6))
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
		"MMR is 1, but GBTEMPLATE is not 0");

  /* 7.4.6.3 */
  if (!(seg_flags & 1))
    {
      gbat_bytes = (seg_flags & 6) ? 2 : 8;
      if (18 + gbat_bytes > sh->data_length)
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, sh->segment_number,
			   "Segment too short");
      memcpy(gbat, segment_data + 18, gbat_bytes);
      jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
		  "gbat: %d, %d", gbat[0], gbat[1]);
    }

  offset = 18 + gbat_bytes;

  /* Table 34 */
  params.MMR = seg_flags & 1;
  params.GBTEMPLATE = (seg_flags & 6) >> 1;
  params.TPGDON = (seg_flags & 8) >> 3;
  params.USESKIP = 0;
  params.GBW = rsi.width;
  params.GBH = rsi.height;
  memcpy (params.gbat, gbat, gbat_bytes);

  gbreg = jbig2_alloc(ctx->allocator, ((rsi.width + 7) >> 3) * rsi.height);


  if (params.MMR)
    {
      code = jbig2_decode_generic_mmr(ctx, sh->segment_number, &params,
				      segment_data + offset, sh->data_length - offset,
				      gbreg);
    }
  else
    {
      int stats_size = params.GBTEMPLATE == 0 ? 65536 :
	params.GBTEMPLATE == 1 ? 8192 : 1024;
      GB_stats = jbig2_alloc(ctx->allocator, stats_size);
      memset(GB_stats, 0, stats_size);

      ws = jbig2_word_stream_buf_new(ctx,
				     segment_data + offset,
				     sh->data_length - offset);
      as = jbig2_arith_new(ctx, ws);
      code = jbig2_decode_generic_region(ctx, sh->segment_number, &params,
					 as, gbreg, GB_stats);
    }

  /* todo: stash gbreg as segment result */
  /* todo: free ws, as */

  jbig2_free(ctx->allocator, GB_stats);

  return code;
}
