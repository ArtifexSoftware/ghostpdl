/*
    jbig2dec
    
    Copyright (c) 2002-2004 artofcode LLC.
    
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
                                                                                
    $Id: jbig2_refinement.c 318 2004-08-13 00:00:17Z giles $
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

/**
 * jbig2_decode_refinement_region: Decode a generic refinement region.
 * @ctx: The context for allocation and error reporting.
 * @segment: A segment reference for error reporting.
 * @params: Decoding parameter set.
 * @as: Arithmetic decoder state.
 * @image: Where to store the decoded data.
 * @GB_stats: Arithmetic stats.
 *
 * Decodes a generic region, according to section 6.2. The caller should
 * pass an already allocated Jbig2Image object for @image
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

}
