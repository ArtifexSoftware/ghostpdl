/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
        
    $Id: jbig2_generic.h,v 1.7 2002/07/04 13:34:29 giles Exp $
*/

/* Table 2 */
typedef struct {
  bool MMR;
  /* GBW */
  /* GBH */
  int GBTEMPLATE;
  bool TPGDON;
  bool USESKIP;
  /* SKIP */
  byte gbat[8];
} Jbig2GenericRegionParams;

/* 6.2 */
int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
			    Jbig2Segment *segment,
			    const Jbig2GenericRegionParams *params,
			    Jbig2ArithState *as,
			    Jbig2Image *image,
			    Jbig2ArithCx *GB_stats);


