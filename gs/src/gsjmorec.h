/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* "Wrapper" for Independent JPEG Group code jmorecfg.h */

#ifndef gsjmorec_INCLUDED
#  define gsjmorec_INCLUDED

#include "jmcorig.h"

/* Remove unwanted / unneeded features. */
#undef DCT_IFAST_SUPPORTED
#if FPU_TYPE <= 0
#  undef DCT_FLOAT_SUPPORTED
#endif
#undef C_MULTISCAN_FILES_SUPPORTED
#undef C_PROGRESSIVE_SUPPORTED
#undef ENTROPY_OPT_SUPPORTED
#undef INPUT_SMOOTHING_SUPPORTED

/****** Comment out the next two lines to add progressive decoding. ******/
#undef D_MULTISCAN_FILES_SUPPORTED
#undef D_PROGRESSIVE_SUPPORTED

#undef BLOCK_SMOOTHING_SUPPORTED
#undef IDCT_SCALING_SUPPORTED
#undef UPSAMPLE_SCALING_SUPPORTED
#undef UPSAMPLE_MERGING_SUPPORTED
#undef QUANT_1PASS_SUPPORTED
#undef QUANT_2PASS_SUPPORTED
/*
 * Read "JPEG" files with up to 64 blocks/MCU for Adobe compatibility.
 * Note that this #define will have no effect in pre-v6 IJG versions.
 */
#define D_MAX_BLOCKS_IN_MCU   64

#endif /* gsjmorec_INCLUDED */
