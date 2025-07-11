/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/
/* Parameters derived from machine and compiler architecture. */
/* This file was generated mechanically by genarch.c, for a 64bit */
/* Microsoft Windows machine, compiling with MSVC. */

	 /* ---------------- Scalar alignments ---------------- */

#define ARCH_ALIGN_SHORT_MOD 2
#define ARCH_ALIGN_INT_MOD 4
#define ARCH_ALIGN_LONG_MOD 4
#define ARCH_ALIGN_SIZE_T_MOD 8
#define ARCH_ALIGN_PTR_MOD 8
#define ARCH_ALIGN_FLOAT_MOD 4
#define ARCH_ALIGN_DOUBLE_MOD 8
#define ARCH_ALIGN_UINT64_T_MOD 8

	 /* ---------------- Scalar sizes ---------------- */

#define ARCH_LOG2_SIZEOF_CHAR 0
#define ARCH_LOG2_SIZEOF_SHORT 1
#define ARCH_LOG2_SIZEOF_INT 2
#define ARCH_LOG2_SIZEOF_LONG 2
#define ARCH_LOG2_SIZEOF_SIZE_T 3
#define ARCH_SIZEOF_SIZE_T 8

#ifndef ARCH_SIZEOF_GX_COLOR_INDEX
#define ARCH_SIZEOF_GX_COLOR_INDEX 8
#endif

#define ARCH_SIZEOF_PTR 8
#define ARCH_SIZEOF_FLOAT 4
#define ARCH_SIZEOF_DOUBLE 8
#define ARCH_FLOAT_MANTISSA_BITS 24
#define ARCH_DOUBLE_MANTISSA_BITS 53

	 /* ---------------- Unsigned max values ---------------- */

#define ARCH_MAX_UCHAR ((unsigned char)0xff + (unsigned char)0)
#define ARCH_MAX_USHORT ((unsigned short)0xffff + (unsigned short)0)
#define ARCH_MAX_UINT ((unsigned int)~0 + (unsigned int)0)
#define ARCH_MAX_ULONG ((unsigned long)~0L + (unsigned long)0)
#define ARCH_MAX_SIZE_T ((size_t)~0L + (size_t)0)

	 /* ---------------- Miscellaneous ---------------- */

#define ARCH_IS_BIG_ENDIAN 0
#define ARCH_PTRS_ARE_SIGNED 0
#define ARCH_FLOATS_ARE_IEEE 1
#define ARCH_ARITH_RSHIFT 2
#define ARCH_DIV_NEG_POS_TRUNCATES 1
