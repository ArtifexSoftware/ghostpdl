/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pctpm.h - structures of PCL's text parsing methods */

#ifndef pctpm_INCLUDED
#define pctpm_INCLUDED

#include "gx.h"

typedef struct pcl_text_parsing_method_s {
    byte    min1, max1;
    byte    min2, max2;
} pcl_text_parsing_method_t;

#define pcl_char_is_2_byte(ch, tpm)                     \
    ( ((ch) >= (tpm)->min1) &&                          \
      ((ch) <= (tpm)->max2) &&                          \
      (((ch) <= (tpm)->max1) || ((ch) >= (tpm)->min2)) )

#define pcl_tpm_is_single_byte(tpm) ((tpm)->max1 == 0)

/* Single-byte only */
#define pcl_tpm_0_data  { 0xff, 0, 0xff, 0 }

/* 0x21-0xff are double-byte */
#define pcl_tpm_21_data { 0x21, 0xff, 0x21, 0xff }

/* 0x81-0x9f, 0xe0-0xfc are double-byte */
#define pcl_tpm_31_data { 0x81, 0x9f, 0xe0, 0xfc }

/* 0x80-0xff are double-byte */
#define pcl_tpm_38_data { 0x80, 0xff, 0x80, 0xff }

#endif			/* pctpm_INCLUDED */
