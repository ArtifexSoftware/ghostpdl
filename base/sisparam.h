/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Generic image scaling stream definitions */
/* Requires strimpl.h */

#ifndef sisparam_INCLUDED
#  define sisparam_INCLUDED

#include "gxfixed.h"	/* for fixed */
#include "gxdda.h"	/* for gx_dda_fixed_point */
#include "scommon.h"

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
 *
 * There is one additional complication: Due to the 'support' requirements
 * of the scalers, we need to decode a slightly larger area than we
 * might think. Accordingly, we are fed 2 different rectangles. drect
 * tell us the area that must be decoded - we use this to calculate
 * the 'In' values. rrect tells us the area that must be rendered - we
 * use this to calculate the 'Out' values.
 *
 * Scale from:
 *
 * <-------------------EntireWidthIn--------------------->
 * +-----------------------------------------------------+  ^
 * | Conceptual Source Image                    ^        |  |
 * |                                            |        |  |
 * |                                       src_y_offset  |  |
 * |                                            |        |  |
 * |<-SX-><--------------WidthIn--------------> v        |  |
 * |      +-penum->rect-----------------------+ ^        |  |
 * |      |                   ^               | |        | EntireHeightIn
 * |      |                   |               | |        |  |
 * |      |              TopMarginIn          | |        |  |
 * |      |                   |               | |        |  |
 * |      |                   v               | |        |  |
 * |      |                +-penum->drect--+  | |        |  |
 * |      |                |    ^          |  | HeightIn |  |
 * |      |                |    |          |  | |        |  |
 * |      |<-LeftMarginIn->| PatchHeightIn |  | |        |  |
 * |      |                |    |          |  | |        |  |
 * |      |                |    v          |  | |        |  |
 * |      |                +---------------+  | |        |  |
 * |      |                <-PatchWidthIn-->  | |        |  |
 * |      +-----------------------------------+ v        |  |
 * |                                                     |  |
 * +-----------------------------------------------------+  v
 *
 * To:
 *
 * <--------------------EntireWidthOut---------------------->
 * +--------------------------------------------------------+  ^
 * | Conceptual Destination Image       ^         ^         |  |
 * |                                    |         |         |  |
 * |                                    |        DY         |  |
 * |                                    |         |         |  |
 * |<-DX-><---------------WidthOut------|-------> v         |  |
 * |      +-----------------------------|-------+ ^         |  |
 * |      |                 ^           |       | |         | EntireHeightOut
 * |      |                 |           |       | |         |  |
 * |      |         TopMarginOut  TopMarginOut2 | |         |  |
 * |      |                 |           |       | |         |  |
 * |      |                 v           v       | |         |  |
 * |      |                 +-penum->rrect*--+  | |         |  |
 * |      |                 |    ^           |  | HeightOut |  |
 * |      |                 |    |           |  | |         |  |
 * |      |<-LeftMarginOut->| PatchHeightOut |  | |         |  |
 * |      |                 |    |           |  | |         |  |
 * |      |                 |    v           |  | |         |  |
 * |      |                 +----------------+  | |         |  |
 * |      |                 <-PatchWidthOut-->  | |         |  |
 * |      +-------------------------------------+ v         |  |
 * |                                                        |  |
 * +--------------------------------------------------------+  v
 *
 *   * Note that this rectangle is derived from penum->rrect,
 *     with appropriate scale factors!
 *
 * Certain values in this diagram are not currently used within
 * gs. DX and SX are always 0, due to banding cutting only on
 * Y. If there are cases where the clist code is truncating
 * the width of images, then we will be getting incorrect (read
 * "marginally sub optimal") results. DY is not supplied, which
 * I suspect leads to each band being potentially off by a subpixel
 * amount in the Y direction.
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
    int PatchWidthIn, PatchHeightIn;	/* The sizes we need to render > 0 (source) */
    int PatchWidthOut, PatchHeightOut;	/* The sizes we need to render > 0 (destination) */
    int LeftMarginIn;
    int LeftMarginOut;
    int TopMarginIn;
    int TopMarginOut;
    int TopMarginOut2;
    int PatchHeightOut2;
    int pad_y;
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
