/*
    jbig2dec
    
    Copyright (C) 2002-2006 Artifex Software, Inc.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
        
    $Id: jbig2_priv.h 420 2005-07-27 23:55:54Z giles $
    
    text region header
*/

/* jbig2_symbol_dict.h must be included first */

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
    int LOGSBSTRIPS;
    int SBSTRIPS;
    /* SBNUMSYMS */
    /* SBSYMCODES */
    /* SBSYMCODELEN */
    /* SBSYMS */
    Jbig2HuffmanTable *SBHUFFFS;
    Jbig2HuffmanTable *SBHUFFDS;
    Jbig2HuffmanTable *SBHUFFDT;
    Jbig2HuffmanTable *SBHUFFRDW;
    Jbig2HuffmanTable *SBHUFFRDH;
    Jbig2HuffmanTable *SBHUFFRDX;
    Jbig2HuffmanTable *SBHUFFRDY;
    Jbig2HuffmanTable *SBHUFFRSIZE;
    bool SBRTEMPLATE;
    int8_t sbrat[4];
} Jbig2TextRegionParams;

int
jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
			     const Jbig2TextRegionParams *params,
			     const Jbig2SymbolDict * const *dicts, const int n_dicts,
			     Jbig2Image *image,
			     const byte *data, const size_t size,
			     Jbig2ArithCx *GR_stats);
