/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Generic image scaling stream definitions */
/* Requires strimpl.h */

#ifndef sisparam_INCLUDED
#  define sisparam_INCLUDED

#include "gxfixed.h"	/* for fixed */
#include "gxdda.h"	/* for gx_dda_fixed_point */
/*
 * Image scaling streams all use a common set of parameters to define the
 * input and output data.  That is what we define here.
 */

/* Input values */
/*typedef byte PixelIn; */  /* per BitsPerComponentIn */
/*#define MaxValueIn 255 */  /* per MaxValueIn */

/* Output values */
/*typedef byte PixelOut; */  /* per BitsPerComponentOut */
/*#define MaxValueOut 255 */  /* per MaxValueOut */

/*
 * The 'support' S of a digital filter is the value such that the filter is
 * guaranteed to be zero for all arguments outside the range [-S..S].  We
 * limit the support so that we can put an upper bound on the time required
 * to compute an output value and on the amount of storage required for
 * X-filtered input data; this also allows us to use pre-scaled fixed-point
 * values for the weights if we wish.
 *
 * 8x8 pixels should be enough for any reasonable application....
 */
#define LOG2_MAX_ISCALE_SUPPORT 3
#define MAX_ISCALE_SUPPORT (1 << LOG2_MAX_ISCALE_SUPPORT)

/* Define image scaling stream parameters. */
/* We juggle 3 concepts here. Firstly, we are conceptually dealing with an
 * Entire image (say 256x256) - this size is EntireWidthIn by EntireHeightIn,
 * and would scale to be EntireWidthOut by EntireHeightOut.
 * Of this, because of the clist, we may only get data for a section of this
 * (say 32x32) - this size is WidthIn by HeightIn, and would scale
 * to be WidthOut by HeightOut.
 * Finally, we may actually only be rendering a smaller 'patch' of this
 * due to restriction by a clip path etc (say 16x16) - this size is
 * PatchWidthIn by PatchHeightIn, and would scale to be PatchWidthOut by
 * PatchHeightOut. We don't really need PatchHeightOut.
 */
typedef struct stream_image_scale_params_s {
    int spp_decode;		/* >= 1 */
    int spp_interp;             /* If we do CM first, may not equal spp_decode */
                                /* 0 < MaxValueIn < 1 << BitsPerComponentIn */
    int BitsPerComponentIn;	/* bits per input value, 8 or 16 */
    uint MaxValueIn;		/* max value of input component, */
    int BitsPerComponentOut;	/* bits per output value, 8 or 16 */
    uint MaxValueOut;		/* max value of output component, */
                                /* 0 < MaxValueOut < 1 << BitsPerComponentOut*/
    bool ColorPolarityAdditive;	/* needed by SpecialDownScale filter */
    bool early_cm;              /* If this is set, then we will perform
                                   color managment before interpolating */
    int src_y_offset;		/* Offset of the subimage (data) in the source image. */
    int abs_interp_limit;	/* limitation of how far to interpolate */
    int EntireWidthIn;		/* Height of entire input image. */
    int EntireHeightIn;		/* Height of entire input image. */
    int EntireWidthOut;		/* Height of entire output image. */
    int EntireHeightOut;	/* Height of entire output image. */
    int WidthIn, HeightIn;	/* The sizes for which we get data > 0 */
    int WidthOut, HeightOut;	/* > 0 */
    int PatchWidthIn, PatchHeightIn;	/* The sizes we need to render > 0 */
    int PatchWidthOut;		/* The width we need to render > 0 */
    int LeftMarginIn;
    int LeftMarginOut;
    int TopMargin;
    int Active;
    gx_dda_fixed_point scale_dda;	/* used to scale limited interpolation up to actual size */
} stream_image_scale_params_t;

/* Define a generic image scaling stream state. */

#define stream_image_scale_state_common\
    stream_state_common;\
    stream_image_scale_params_t params

typedef struct stream_image_scale_state_s {
    stream_image_scale_state_common;
} stream_image_scale_state;

#endif /* sisparam_INCLUDED */
