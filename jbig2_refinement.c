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
      "decoding generic refinement region with offset %dx%x,\n"
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

