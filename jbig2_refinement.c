/*
    jbig2dec
    
    Copyright (c) 2004 artofcode LLC.
    
    This software is provided AS-IS with no warranty,
    either express or implied.
                                                                                
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
                                                                                
    $Id$
*/

/**
 * Generic Refinement region handlers.
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memcpy(), memset() */

#include <stdio.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_mmr.h"

static int
jbig2_decode_refinement_template0(Jbig2Ctx *ctx,
                              Jbig2Segment *segment,
                              const Jbig2RefinementRegionParams *params,
                              Jbig2ArithState *as,
                              Jbig2Image *image,
                              Jbig2ArithCx *GB_stats)
{
  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
    "refinement region template 0 NYI");
}

/* look up a pixel value in an image.
   returns 0 outside the image frame for the convenience of
   the template code
*/
static int
jbig2_image_get_pixel(Jbig2Image *image, int x, int y)
{
  const int w = image->width;
  const int h = image->height;
  const int byte = (x >> 3) + y*image->stride;
  const int bit = x & 7;

  if ((x < 0) || (x > w)) return 0;
  if ((y < 0) || (y > h)) return 0;
  
  return ((image->data[byte]>>bit) & 1);
}

static int
jbig2_image_set_pixel(Jbig2Image *image, int x, int y, bool value)
{
  const int w = image->width;
  const int h = image->height;
  int i, scratch, mask;
  int bit, byte;

  if ((x < 0) || (x > w)) return 0;
  if ((y < 0) || (y > h)) return 0;

  fprintf(stderr, "set pixel called for image 0x%x (%d x %d) stride %d\n",
    image, w, h, image->stride);

  byte = (x >> 3) + y*image->stride;
  bit = x & 7;
  mask = (1 << bit) ^ 0xff;

  fprintf(stderr, "set pixel mask for bit %d of byte %d (%d,%d) is 0x%02x\n", 
    bit, byte, x, y, mask);

  scratch = image->data[byte] & mask;
  image->data[byte] = scratch | (value << bit);

  return 1;
}

static int
jbig2_decode_refinement_template1_unopt(Jbig2Ctx *ctx,
                              Jbig2Segment *segment,
                              const Jbig2RefinementRegionParams *params,
                              Jbig2ArithState *as,
                              Jbig2Image *image,
                              Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  const int dx = params->DX;
  const int dy = params->DY;
  Jbig2Image *ref = params->reference;
  uint32_t CONTEXT;
  int x,y;
  bool bit;

  for (y = 0; y < GBH; y++) {
    for (x = 0; x < GBH; x++) {
      CONTEXT = 0;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y + 0) << 0; 
      CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1; 
      CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2; 
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 3; 
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+1, y-dy+1) << 4;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy+1) << 5;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+1, y-dy+0) << 6;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy+0) << 7;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx-1, y-dy+0) << 8;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy-1) << 9;
      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
      jbig2_image_set_pixel(image, x, y, bit);
    }
  }

  {
    static count = 0;
    char name[32];
    snprintf(name, 32, "refin-%d.pbm", count);
    jbig2_image_write_pbm_file(ref, name);
    snprintf(name, 32, "refout-%d.pbm", count);
    jbig2_image_write_pbm_file(image, name);
    count++;
  }

  return 0;
}

static int
jbig2_decode_refinement_template1(Jbig2Ctx *ctx,
                              Jbig2Segment *segment,
                              const Jbig2RefinementRegionParams *params,
                              Jbig2ArithState *as,
                              Jbig2Image *image,
                              Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  const int stride = image->stride;
  const int refstride = params->reference->stride;
  const int dx = params->DX;
  const int dy = params->DY;
  byte *grreg_line = (byte *)image->data;
  byte *grref_line = (byte *)params->reference->data;
  int x,y;

  for (y = 0; y < GBH; y++) {
    const int padded_width = (GBW + 7) & -8;
    uint32_t CONTEXT;
    uint32_t refline_m1; /* previous line of the reference bitmap */
    uint32_t refline_0;  /* current line of the reference bitmap */
    uint32_t refline_1;  /* next line of the reference bitmap */
    uint32_t line_m1;    /* previous line of the decoded bitmap */

    line_m1 = (y >= 1) ? grreg_line[-stride] : 0;
    refline_m1 = ((y-dy) >= 1) ? grref_line[(-1-dy)*stride] << 2: 0;
    refline_0  = (((y-dy) > 0) && ((y-dy) < GBH)) ? grref_line[(0-dy)*stride] << 4 : 0;
    refline_1  = (y < GBW - 1) ? grref_line[(+1-dy)*stride] << 7 : 0;
    CONTEXT = ((line_m1 >> 5) & 0x00e) |
	      ((refline_1 >> 5) & 0x030) |
	      ((refline_0 >> 5) & 0x1c0) |
	      ((refline_m1 >> 5) & 0x200);

    for (x = 0; x < padded_width; x += 8) {
      byte result = 0;
      int x_minor;
      const int minor_width = GBW - x > 8 ? 8 : GBW - x;

      if (y >= 1) {
	line_m1 = (line_m1 << 8) | 
	  (x + 8 < GBW ? grreg_line[-stride + (x >> 3) + 1] : 0);
	refline_m1 = (refline_m1 << 8) | 
	  (x + 8 < GBW ? grref_line[-refstride + (x >> 3) + 1] << 2 : 0);
      }

      refline_0 = (refline_0 << 8) |
	  (x + 8 < GBW ? grref_line[(x >> 3) + 1] << 4 : 0);

      if (y < GBH - 1)
	refline_1 = (refline_1 << 8) |
	  (x + 8 < GBW ? grref_line[+refstride + (x >> 3) + 1] << 7 : 0);
      else
	refline_1 = 0;

      /* this is the speed critical inner-loop */
      for (x_minor = 0; x_minor < minor_width; x_minor++) {
	bool bit;

	bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	result |= bit << (7 - x_minor);
	CONTEXT = ((CONTEXT & 0x0d6) << 1) | bit |
	  ((line_m1 >> (9 - x_minor)) & 0x002) |
	  ((refline_1 >> (9 - x_minor)) & 0x010) |
	  ((refline_0 >> (9 - x_minor)) & 0x040) |
	  ((refline_m1 >> (9 - x_minor)) & 0x200);
      }

      grreg_line[x>>3] = result;

    }

    grreg_line += stride;
    grref_line += refstride;

  }

  return 0;

}


/**
 * jbig2_decode_refinement_region: Decode a generic refinement region.
 * @ctx: The context for allocation and error reporting.
 * @segment: A segment reference for error reporting.
 * @params: Decoding parameter set.
 * @as: Arithmetic decoder state.
 * @image: Where to store the decoded image.
 * @GB_stats: Arithmetic stats.
 *
 * Decodes a generic refinement region, according to section 6.3.
 * an already allocated Jbig2Image object in @image for the result.
 *
 * Because this API is based on an arithmetic decoding state, it is
 * not suitable for MMR decoding.
 *
 * Return code: 0 on success.
 **/
int
jbig2_decode_refinement_region(Jbig2Ctx *ctx,
			    Jbig2Segment *segment,
			    const Jbig2RefinementRegionParams *params,
			    Jbig2ArithState *as,
			    Jbig2Image *image,
			    Jbig2ArithCx *GB_stats)
{
  {
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
      "decoding generic refinement region with offset %d,%x,\n"
      "  GRTEMPLATE=%d, TPGDON=%d, RA1=(%d,%d) RA2=(%d,%d)\n",
      params->DX, params->DY, params->GRTEMPLATE, params->TPGDON,
      params->grat[0], params->grat[1], params->grat[2], params->grat[3]);
  }
  if (params->TPGDON)
    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "decode_refinement_region: typical prediction coding NYI");
  if (params->GRTEMPLATE)
    return jbig2_decode_refinement_template1_unopt(ctx, segment, params, 
                                             as, image, GB_stats);
  else
    return jbig2_decode_refinement_template0(ctx, segment, params,
                                             as, image, GB_stats);
}

