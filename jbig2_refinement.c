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

#ifdef OUTPUT_PBM
#include <stdio.h>
#endif

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
    refline_m1 = (y >= 1) ? grref_line[-stride] : 0;
    refline_0  = grref_line[0];
    refline_1  = (y < GBW - 1) ? grref_line[+stride] : 0;

    for (x = 0; x < padded_width; x += 8) {
      byte result = 0;
      int x_minor;
      const int minor_width = GBW - x > 8 ? 8 : GBW - x;

      if (y >= 1) {
	line_m1 = (line_m1 << 8) | 
	  (x + 8 < GBW ? grreg_line[-stride + (x >> 3) + 1] : 0);
	refline_m1 = (refline_m1 << 8) | 
	  (x + 8 < GBW ? grref_line[-refstride + (x >> 3) + 1] : 0);
      }

      refline_0 = (refline_0 << 8) |
	  (x + 8 < GBW ? grref_line[(x >> 3) + 1] : 0);

      if (y < GBH - 1)
	refline_1 = (refline_1 << 8) |
	  (x + 8 < GBW ? grref_line[+refstride + (x >> 3) + 1] : 0);
      else
	refline_1 = 0;

      /* this is the speed critical inner-loop */
      for (x_minor = 0; x_minor < minor_width; x_minor++) {
	bool bit;

	bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	result |= bit << (7 - x_minor);
	CONTEXT = ((CONTEXT & 0x0d6) << 1) | bit |
	  ((line_m1 >> (10 - x_minor)) &0x00e) |
	  ((refline_1 >> (10 - x_minor)) &0x030) |
	  ((refline_0 >> (10 - x_minor)) &0x1c0) |
	  ((refline_m1 >> (10 - x_minor)) &0x200);
      }

      grreg_line[x>>3] = result;

    }

    grreg_line += stride;
    grref_line += refstride;

  }

  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
    "refinement region template 1 NYI");
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
    return jbig2_decode_refinement_template1(ctx, segment, params, 
                                             as, image, GB_stats);
  else
    return jbig2_decode_refinement_template0(ctx, segment, params,
                                             as, image, GB_stats);
}

