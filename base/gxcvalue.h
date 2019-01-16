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


/* Definition of device color values */

#ifndef gxcvalue_INCLUDED
#  define gxcvalue_INCLUDED

/* Define the type for gray or RGB values at the driver interface. */
typedef unsigned short gx_color_value;

/* We might use less than the full range someday. */
/* ...bits must lie between 8 and 16. */
#define gx_color_value_bits (sizeof(gx_color_value) * 8)
#define gx_max_color_value ((gx_color_value)((1L << gx_color_value_bits) - 1))

/* See the comments below on why this is no longer a simple truncation */
#define gx_color_value_to_byte(cv)\
    ((gx_color_value)((((unsigned int)cv)*0xFF01 + 0x800000)>>24))
#define gx_color_value_from_byte(cb)\
  (((cb) << (gx_color_value_bits - 8)) + ((cb) >> (16 - gx_color_value_bits)))

/* Convert between gx_color_values and fracs. */
#define frac2cv(fr) frac2ushort(fr)
#define cv2frac(cv) ushort2frac(cv)

/* Convert between gx_color_values and frac31s. */
#define frac312cv(fr) frac312ushort(fr)
#define cv2frac31(cv) ushort2frac31(cv)

/* At various points in the code (particularly inside encode_color and
 * decode_color functions), we need to convert colour values between
 * different bitdepths.
 *
 * A typical example is where we have an 8 bit color value (0xab say)
 * which we need to expand to be a 16 bit color value. In order to
 * fill the range without biasing, we 'duplicate' the top bits down
 * (so giving 0xabab rather than just 0xab00).
 *
 * Traditionally, when converting to smaller numbers, we have simply
 * truncated (so 0xabcd => 0xab), but this doesn't actually give us the
 * best results possible consider for example truncating 0x0081 - this
 * would give 0x00, which when converted back to 16 bits later would give
 * 0x0000, a difference of 0x81. If instead we converted 0x0081 to 0x01,
 * when that got converted back it would go to be 0x0101, a difference of
 * 0x80.
 *
 * This disparity first came to light in bug 690538; lcms returns 16 bit
 * colors that we use for lineart, but returns image data in 8 bits. At the
 * time of the bug, gs would convert the 16 bit lineart colors to 8 bit
 * via simple truncation to plot them in the device, resulting in being
 * off by 1 to the image colors.
 *
 * We therefore adopt lcms's rounding scheme here. To convert from 16 to 8
 * bit, lcms does: col8 = (col16 * 0xFF01 + 0x800000)>>24 (all unsigned ints)
 *
 * Unfortunately we do not have the luxury of only working in 16 or 8 bits
 * so we generalise that to convert from FROM to TO bits as follows:
 *
 *  M = (((1<<TO)-1)<<(FROM-TO))+1;
 *  S = 2*FROM-TO;
 *  R = 1<<(S-1);
 *  out = (M*in + R)>>S
 *
 * We express this operation in macros here so it can be reused in many
 * places in the code while keeping the complexity in one place.
 */
#define COLROUND_VARS  unsigned int COLROUND_M, COLROUND_S, COLROUND_R
#define COLROUND_SETUP(TO) \
  do { COLROUND_M = (((1<<TO)-1)<<(gx_color_value_bits-TO))+1; \
       COLROUND_S = (gx_color_value_bits*2-TO); \
       COLROUND_R = 1<<(COLROUND_S-1); \
  } while (0 == 1)
#define COLROUND_ROUND(X) \
  ((gx_color_index)(((COLROUND_M*(unsigned int)(X))+COLROUND_R)>>COLROUND_S))

#define COLDUP_VARS unsigned int COLDUP_M, COLDUP_S
#define COLDUP_MULTS_STRING "\x00\x00\xFF\xFF\x55\x55\x92\x49\x11\x11\x84\x21\x10\x41\x40\x81\x01\x01\x02\x01\x04\x01\x08\x01\x10\x01\x20\x01\x40\x01\x80\x01"
#define COLDUP_SETUP(FROM) \
  do { COLDUP_M = (((unsigned char)COLDUP_MULTS_STRING[2*FROM])<<8)|\
                    (unsigned char)COLDUP_MULTS_STRING[2*FROM+1]; \
       COLDUP_S = (FROM-(gx_color_value_bits % FROM)) % FROM; \
  } while (0 == 1)
#define COLDUP_DUP(X) \
  ((gx_color_value)(((unsigned int)X) * COLDUP_M)>>COLDUP_S)
#endif /* gxcvalue_INCLUDED */
