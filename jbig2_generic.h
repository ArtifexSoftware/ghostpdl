/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
        
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


