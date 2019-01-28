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

#if __x86_64__ == 1
         /* ---------------- Scalar alignments ---------------- */

#define ARCH_ALIGN_SHORT_MOD                 2
#define ARCH_ALIGN_INT_MOD                   4
#define ARCH_ALIGN_LONG_MOD                  8
#define ARCH_ALIGN_PTR_MOD                   8
#define ARCH_ALIGN_FLOAT_MOD                 4
#define ARCH_ALIGN_DOUBLE_MOD                8

         /* ---------------- Scalar sizes ---------------- */

#define ARCH_LOG2_SIZEOF_CHAR                0
#define ARCH_LOG2_SIZEOF_SHORT               1
#define ARCH_LOG2_SIZEOF_INT                 2
#define ARCH_LOG2_SIZEOF_LONG                3
#define ARCH_LOG2_SIZEOF_LONG_LONG           3
#define ARCH_LOG2_SIZEOF_PTR                 3
#define ARCH_LOG2_SIZEOF_FLOAT               2
#define ARCH_LOG2_SIZEOF_DOUBLE              3

#define ARCH_SIZEOF_PTR                      8
#define ARCH_SIZEOF_FLOAT                    4
#define ARCH_SIZEOF_DOUBLE                   8

         /* ---------------- Unsigned max values ---------------- */

#define ARCH_MAX_UCHAR                       ((unsigned char)~(unsigned char)0 + (unsigned char)0)
#define ARCH_MAX_USHORT                      ((unsigned short)~(unsigned short)0 + (unsigned short)0)
#define ARCH_MAX_UINT                        ((unsigned int)~0 + (unsigned int)0)
#define ARCH_MAX_ULONG                       ((unsigned long)~0L + (unsigned long)0)

         /* ---------------- Floating point ---------------- */

#ifndef FLOATS_ARE_IEEE

#if defined(__STDC_IEC_559__) && __STDC_IEC_559__!=0

# define FLOATS_ARE_IEEE 1

#elif (defined(FLT_RADIX) && FLT_RADIX == 2) && (defined(FLT_MANT_DIG) && FLT_MANT_DIG == 24) \
   && (defined(FLT_MIN_EXP) && FLT_MIN_EXP == -125) && (defined(FLT_MAX_EXP) && FLT_MAX_EXP == 128)

# define FLOATS_ARE_IEEE 1

#elif (defined(__FLT_RADIX__) && __FLT_RADIX__ == 2) && (defined(__FLT_MANT_DIG__) && __FLT_MANT_DIG__ == 24) \
   && (defined(__FLT_MIN_EXP__) && __FLT_MIN_EXP__ == -125) && (defined(__FLT_MAX_EXP__) && __FLT_MAX_EXP__ == 128)

# define FLOATS_ARE_IEEE 1

#else

# define FLOATS_ARE_IEEE 0

#endif

#endif /* FLOATS_ARE_IEEE */


#if FLOATS_ARE_IEEE == 1
# define ARCH_FLOATS_ARE_IEEE                1
# define ARCH_FLOAT_MANTISSA_BITS            24
# define ARCH_DOUBLE_MANTISSA_BITS           53
#else /* FLOATS_ARE_IEEE*/
# define ARCH_FLOATS_ARE_IEEE 0
/*
 * There isn't any general way to compute the number of mantissa
 * bits accurately, especially if the machine uses hex rather
 * than binary exponents.  Use conservative values, assuming
 * the exponent is stored in a 16-bit word of its own.
 */
# define ARCH_FLOAT_MANTISSA_BITS            (sizeof(float) * 8 - 17)
# define ARCH_DOUBLE_MANTISSA_BITS           (sizeof(double) * 8 - 17)
#endif /* FLOATS_ARE_IEEE*/

         /* ---------------- Miscellaneous ---------------- */

#define ARCH_IS_BIG_ENDIAN                   0
#define ARCH_PTRS_ARE_SIGNED                 0
#define ARCH_DIV_NEG_POS_TRUNCATES           1
#define ARCH_ARITH_RSHIFT                    2
#define ARCH_SIZEOF_GX_COLOR_INDEX           8

#elif __i386__ == 1 /* __x86_64__ */

         /* ---------------- Scalar alignments ---------------- */

#define ARCH_ALIGN_SHORT_MOD                 2
#define ARCH_ALIGN_INT_MOD                   4
#define ARCH_ALIGN_LONG_MOD                  4
#define ARCH_ALIGN_PTR_MOD                   4
#define ARCH_ALIGN_FLOAT_MOD                 4
#define ARCH_ALIGN_DOUBLE_MOD                4

         /* ---------------- Scalar sizes ---------------- */

#define ARCH_LOG2_SIZEOF_CHAR                0
#define ARCH_LOG2_SIZEOF_SHORT               1
#define ARCH_LOG2_SIZEOF_INT                 2
#define ARCH_LOG2_SIZEOF_LONG                2
#define ARCH_LOG2_SIZEOF_LONG_LONG           3
#define ARCH_LOG2_SIZEOF_PTR                 2
#define ARCH_LOG2_SIZEOF_FLOAT               2
#define ARCH_LOG2_SIZEOF_DOUBLE              3

#define ARCH_SIZEOF_PTR                      4
#define ARCH_SIZEOF_FLOAT                    4
#define ARCH_SIZEOF_DOUBLE                   8

         /* ---------------- Unsigned max values ---------------- */

#define ARCH_MAX_UCHAR                       ((unsigned char)~(unsigned char)0 + (unsigned char)0)
#define ARCH_MAX_USHORT                      ((unsigned short)~(unsigned short)0 + (unsigned short)0)
#define ARCH_MAX_UINT                        ((unsigned int)~0 + (unsigned int)0)
#define ARCH_MAX_ULONG                       ((unsigned long)~0L + (unsigned long)0)

         /* ---------------- Floating point ---------------- */

#ifndef FLOATS_ARE_IEEE

#if defined(__STDC_IEC_559__) && __STDC_IEC_559__!=0

# define FLOATS_ARE_IEEE 1

#elif (defined(FLT_RADIX) && FLT_RADIX == 2) && (defined(FLT_MANT_DIG) && FLT_MANT_DIG == 24) \
   && (defined(FLT_MIN_EXP) && FLT_MIN_EXP == -125) && (defined(FLT_MAX_EXP) && FLT_MAX_EXP == 128)

# define FLOATS_ARE_IEEE 1

#elif (defined(__FLT_RADIX__) && __FLT_RADIX__ == 2) && (defined(__FLT_MANT_DIG__) && __FLT_MANT_DIG__ == 24) \
   && (defined(__FLT_MIN_EXP__) && __FLT_MIN_EXP__ == -125) && (defined(__FLT_MAX_EXP__) && __FLT_MAX_EXP__ == 128)

# define FLOATS_ARE_IEEE 1

#else

# define FLOATS_ARE_IEEE 0

#endif

#endif /* FLOATS_ARE_IEEE */


#if FLOATS_ARE_IEEE == 1
# define ARCH_FLOATS_ARE_IEEE                1
# define ARCH_FLOAT_MANTISSA_BITS            24
# define ARCH_DOUBLE_MANTISSA_BITS           53
#else /* FLOATS_ARE_IEEE*/
# define ARCH_FLOATS_ARE_IEEE 0
/*
 * There isn't any general way to compute the number of mantissa
 * bits accurately, especially if the machine uses hex rather
 * than binary exponents.  Use conservative values, assuming
 * the exponent is stored in a 16-bit word of its own.
 */
# define ARCH_FLOAT_MANTISSA_BITS            (sizeof(float) * 8 - 17)
# define ARCH_DOUBLE_MANTISSA_BITS           (sizeof(double) * 8 - 17)
#endif /* FLOATS_ARE_IEEE*/

         /* ---------------- Miscellaneous ---------------- */

#define ARCH_IS_BIG_ENDIAN                   0
#define ARCH_PTRS_ARE_SIGNED                 0
#define ARCH_DIV_NEG_POS_TRUNCATES           1
#define ARCH_ARITH_RSHIFT                    2
#define ARCH_SIZEOF_GX_COLOR_INDEX           8

#endif /* __i386__ */
