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

/* Table 2 */
typedef struct {
  bool MMR;
  int32_t GBW;
  int32_t GBH;
  int GBTEMPLATE;
  bool TPGDON;
  bool USESKIP;
  /* SKIP */
  byte gbat[8];
} Jbig2GenericRegionParams;

typedef struct {
  int32_t width;
  int32_t height;
  int32_t x;
  int32_t y;
  byte flags;
} Jbig2RegionSegmentInfo;

typedef struct {
  Jbig2WordStream super;
  const byte *data;
  size_t size;
} Jbig2WordStreamBuf;

static uint32_t
jbig2_word_stream_buf_get_next_word(Jbig2WordStream *self, int offset)
{
  Jbig2WordStreamBuf *z = (Jbig2WordStreamBuf *)self;
  const byte *data = z->data;
  uint32_t result;

  if (offset + 4 < z->size)
    result = (data[offset] << 24) | (data[offset + 1] << 16) |
      (data[offset + 2] << 8) | data[offset + 3];
  else
    {
      int i;

      result = 0;
      for (i = 0; i < z->size - offset; i++)
	result |= data[offset + i] << ((3 - i) << 3);
    }
  return result;
}

Jbig2WordStream *
jbig2_word_stream_buf_new(Jbig2Ctx *ctx, const byte *data, size_t size)
{
  Jbig2WordStreamBuf *result = (Jbig2WordStreamBuf *)jbig2_alloc(ctx->allocator, sizeof(Jbig2WordStreamBuf));

  result->super.get_next_word = jbig2_word_stream_buf_get_next_word;
  result->data = data;
  result->size = size;

  return &result->super;
}

void
jbig2_word_stream_buf_free(Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
  jbig2_free(ctx->allocator, ws);
}

static int
jbig2_decode_generic_template1(Jbig2Ctx *ctx,
			       int32_t seg_number,
			       const Jbig2GenericRegionParams *params,
			       const byte *data, size_t size,
			       byte *gbreg)
{
  Jbig2ArithCx GB_stats[8192];
  Jbig2WordStream *ws = jbig2_word_stream_buf_new(ctx, data, size);
  /* todo: ctx needs to be an argument; fix up memory allocation */
  Jbig2ArithState *as = jbig2_arith_new(ws);
  int GBW = params->GBW;
  int rowstride = (GBW + 7) >> 3;
  int x, y;
  byte *gbreg_line = gbreg;
  bool LTP = 0;

  memset(GB_stats, 0, sizeof(GB_stats));
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
      CONTEXT = ((line_m1 >> 1) & 0x1f8) | ((line_m2 >> 4) & 0x1e00);

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

/**
 * jbig2_decode_generic_region: Decode a generic region.
 * @ctx: The context for allocation and error reporting.
 * @params: Parameters, as specified in Table 2.
 * @data: The input data.
 * @size: The size of the input data, in bytes.
 * @gbreg: Where to store the decoded data.
 *
 * Decodes a generic region, according to section 6.2. The caller should
 * have allocated the memory for @gbreg, which is packed 8 pixels to a
 * byte, scanlines aligned to one byte boundaries.
 *
 * Todo: I think the stats need to be an argument.
 *
 * Return code: 0 on success.
 **/
int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
			    int32_t seg_number,
			    const Jbig2GenericRegionParams *params,
			    const byte *data, size_t size,
			    byte *gbreg)
{
  if (!params->MMR && params->GBTEMPLATE == 1)
    return jbig2_decode_generic_template1(ctx, seg_number,
					  params, data, size, gbreg);
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

  /* 7.4.6 */
  if (sh->data_length < 18)
    {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, sh->segment_number,
		  "Segment too short");
      return -1;
    }

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
	return -1;
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

  code = jbig2_decode_generic_region(ctx, sh->segment_number, &params,
				     segment_data + offset,
				     sh->data_length - offset,
				     gbreg);

  /* todo: stash gbreg as segment result */

  return code;
}
