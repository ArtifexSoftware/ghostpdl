/* Copyright (C) 2001-2026 Artifex Software, Inc.
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


/* Generic substitute for stdint.h */

#ifndef stdint__INCLUDED
#  define stdint__INCLUDED

/*
 * ensure our standard defines have been included
 */
#include "std.h"

/* Define some stdint.h types. The jbig2dec headers and ttf bytecode
 * interpreter require these and they're generally useful to have around
 * now that there's a standard.

 */

/* Some systems are guaranteed to have stdint.h
 * but don't use the autoconf detection
 */
#ifndef HAVE_STDINT_H
# if defined(__MACOS__) || (defined(__APPLE__) && defined(__MACH__))
#   define HAVE_STDINT_H
# endif
#endif

/* try a generic header first */
#if defined(HAVE_STDINT_H)
# include <stdint.h>
# define STDINT_TYPES_DEFINED
#elif defined(SYS_TYPES_HAS_STDINT_TYPES)
  /* std.h will have included sys/types.h */
# define STDINT_TYPES_DEFINED
#endif

/* try platform-specific defines */
#ifndef STDINT_TYPES_DEFINED
# if defined(__WIN32__) /* MSVC currently doesn't provide C99 headers */
   typedef signed char             int8_t;
   typedef short int               int16_t;
   typedef int                     int32_t;
   typedef __int64                 int64_t;
   typedef unsigned char           uint8_t;
   typedef unsigned short int      uint16_t;
   typedef unsigned int            uint32_t;
   typedef unsigned __int64        uint64_t;
#  define STDINT_TYPES_DEFINED
# elif defined(__VMS) || defined(__osf__) /* OpenVMS and Tru64 provide these types in inttypes.h */
#  include <inttypes.h>
#  define STDINT_TYPES_DEFINED
# elif defined(__CYGWIN__)
#  include <sys/types.h>  /* Old Cygwin has some of C99 types here. */
   typedef unsigned char uint8_t;
   typedef unsigned short uint16_t;
   typedef unsigned int uint32_t;
   typedef unsigned long long uint64_t;
#  define STDINT_TYPES_DEFINED
# endif
   /* other archs may want to add defines here,
      or use the fallbacks in std.h */
#endif

/* fall back to tests based on arch.h */
#ifndef STDINT_TYPES_DEFINED
/* 8 bit types */
# if ARCH_SIZEOF_CHAR == 1
typedef signed char int8_t;
typedef unsigned char uint8_t;
# endif
/* 16 bit types */
# if ARCH_SIZEOF_SHORT == 2
typedef signed short int16_t;
typedef unsigned short uint16_t;
# else
#  if ARCH_SIZEOF_INT == 2
typedef signed int int16_t;
typedef unsigned int uint16_t;
#  endif
# endif
/* 32 bit types */
# if ARCH_SIZEOF_INT == 4
typedef signed int int32_t;
typedef unsigned int uint32_t;
# else
#  if ARCH_SIZEOF_LONG == 4
typedef signed long int32_t;
typedef unsigned long uint32_t;
#  else
#   if ARCH_SIZEOF_SHORT == 4
typedef signed short int32_t;
typedef unsigned short uint32_t;
#   endif
#  endif
# endif
/* 64 bit types */
# if ARCH_SIZEOF_INT == 8
typedef signed int int64_t;
typedef unsigned int uint64_t;
# else
#  if ARCH_SIZEOF_LONG == 8
typedef signed long int64_t;
typedef unsigned long uint64_t;
#  else
#   ifdef ARCH_SIZEOF_LONG_LONG
#    if ARCH_SIZEOF_LONG_LONG == 8
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#    endif
#   endif
#  endif
# endif

#  define STDINT_TYPES_DEFINED
#endif /* STDINT_TYPES_DEFINED */


/* We really want our offset type to be 64 bit for large file support
 * but this allows a particular port to specficy a prefered data type size
 */
#ifdef ARCH_SIZEOF_GS_OFFSET_T
# if ARCH_SIZEOF_GS_OFFSET_T == 8
typedef int64_t gs_offset_t;off64_t
# elif ARCH_SIZEOF_GS_OFFSET_T == 4
typedef int32_t gs_offset_t;
# else
UNSUPPORTED
# endif
#else
# define ARCH_SIZEOF_GS_OFFSET_T 8
typedef int64_t gs_offset_t;
#endif

#if !defined(_AIX) || !defined(__GNUC__) || !defined(_LARGE_FILE_API)
typedef int64_t off64_t;
#endif

#if defined(HAVE_INTTYPES_H) && HAVE_INTTYPES_H == 1
   /* _XOPEN_SOURCE 500 define is needed to get
    * access to pread and pwrite and, on some platforms
    * the PRI macros
    * The "undef" avoids warnings about a redefinition
    */
# if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 500
#  undef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 500
# endif

# ifndef __USE_UNIX98
#  define __USE_UNIX98
# endif

# include <inttypes.h>
#else
# if defined(__WIN32__)
#  define PRId32 "I32d"
#  define PRId64 "I64d"
#  define PRIi32 "I32i"
#  define PRIi64 "I64i"
#  define PRIu32 "I32u"
#  define PRIu64 "I64u"
#  define PRIx64 "I64x"
#  if ARCH_SIZEOF_SIZE_T == 4
#   define PRIxSIZE "x"
#   define PRIuSIZE "u"
#   define PRIdSIZE "d"
#   define PRIiSIZE "i"
#  else
#   define PRIxSIZE "I64x"
#   define PRIuSIZE "I64u"
#   define PRIdSIZE "I64d"
#   define PRIiSIZE "I64i"
#  endif
# endif
#endif

/* Even if we have inttypes.h, these may not be defined */
# ifndef PRId32
#  define PRId32 "d"
# endif

# ifndef PRId64
#  define PRId64 "lld"
# endif

# ifndef PRIi32
#  define PRIi32 "i"
# endif

# ifndef PRIi64
#  define PRIi64 "lli"
# endif

# ifndef PRIu32
#  define PRIu32 "u"
# endif

# ifndef PRIx32
#  define PRIx32 "x"
# endif

# ifndef PRIu64
#  define PRIu64 "llu"
# endif

# if ARCH_SIZEOF_SIZE_T == 4
#  ifndef PRIxSIZE
#   define PRIxSIZE "x"
#  endif

#  ifndef PRIuSIZE
#   define PRIuSIZE "u"
#  endif

#  ifndef PRIdSIZE
#   define PRIdSIZE "d"
#  endif

#  ifndef PRIiSIZE
#   define PRIiSIZE "i"
#  endif
# else
#  if ARCH_SIZEOF_LONG == 8
#   ifndef PRIxSIZE
#    define PRIxSIZE "lx"
#   endif

#   ifndef PRIuSIZE
#    define PRIuSIZE "lu"
#   endif

#   ifndef PRIdSIZE
#    define PRIdSIZE "ld"
#   endif

#   ifndef PRIiSIZE
#    define PRIiSIZE "li"
#   endif
#  else
#   ifndef PRIxSIZE
#    define PRIxSIZE "llx"
#   endif

#   ifndef PRIuSIZE
#    define PRIuSIZE "llu"
#   endif

#   ifndef PRIdSIZE
#    define PRIdSIZE "lld"
#   endif

#   ifndef PRIiSIZE
#    define PRIiSIZE "lli"
#   endif
#  endif
#  ifndef PRIx64
#    define PRIx64 PRIxSIZE
#  endif
# endif

/* Pointers are hard to do in pure PRIxPTR style, as some platforms
 * add 0x before the pointer, and others don't. To be consistent, we
 * therefore roll our own. The difference here is that we always
 * include the 0x and the % ourselves, and require the arg to be
 * cast to an intptr_t.
*/
# if ARCH_SIZEOF_SIZE_T == 4
#  define PRI_INTPTR "0x%" PRIx32
# else
#  define PRI_INTPTR "0x%" PRIx64
# endif

# if ARCH_SIZEOF_GS_OFFSET_T == 4
#  define PRIdOFFSET PRId32
# else
#  define PRIdOFFSET PRId64
# endif

/* Code to check that multiplications don't overflow. This is complicated, for signed variables,
 * by the fact that overflow of signed variable multiplication is undefined behaviour. Modern compilers
 * decide that, since its undefined, it can't happen. So they optimise away the multiplication and
 * checks, and simply end up removing the entire block of code!
 *
 * C23 has extensions to deal with this, and various compilers have their own extensions, but we
 * can't use either of those approaches because we have to continue to support old compilers and
 * building on OS's which don't use, for example, gcc or clang.
 *
 * So the only thing we can do is turn the multiplication of signed values into a multiplication
 * of unsigned values. Multiplying unsigned variables *is* defined, to wrap around.
 */
static inline int check_64bit_multiply(int64_t x, int64_t y, int64_t *result)
{
    int64_t sign = 0; /* If we have one (and only one) negative input, then this will carry the sign bit  */
    uint64_t v;

    /* If x is negative... */
    if (x & min_int64_t) {
        /* negate x to make it positive (2s complement + 1), we know a signed value will fit in an unsigned variable if we drop the sign bit */
        x = ~x + 1;
        /* If y is also negative... */
        if (y & min_int64_t) {
            /* Negate y */
            y = ~y + 1;
        } else
            /* One negative value, we need to make the final result negative (see end of function) */
            sign = min_int64_t;
    } else {
        /* x is positive, so if y is negative, then... */
        if (y & min_int64_t) {
            /* Negate y */
            y = ~y + 1;
            /* One negative value, we need to make the final result negative (see end of function) */
            sign = min_int64_t;
        }
    }

    /* Perform the multiply using unsigned values. This is defined behaviour, so compilers won't optimise it away ! */
    v = (uint64_t)x * (uint64_t)y;

    /* If the result won't fit in a *signed* variable then its an error.
     * If the result divided by one (non-zero) of the inputs is not the same as the other input, then we overflowed and wrapped, an error.
     */
    if (v > max_int64_t || (x != 0 && v / x != y))
        return -1;
    /* Otherwise apply any sign bit to the result */
    *result = v | sign;
    return 0;
}

/* This is named 'int' rather than '32bit' in case we have a platform where int is not
 * 32-bit. This is specifically to check the result of multiplying two ints fits into
 * an int without overflow.
 */
static inline int check_int_multiply(int x, int y, int *result)
{
    int sign = 0;
    unsigned int v;

    if (x & min_int) {
        x = ~x + 1;
        if (y & min_int)
            y = ~y + 1;
        else
            sign = min_int;
    } else {
        if (y & min_int) {
            y = ~y + 1;
            sign = min_int;
        }
    }
    v = (unsigned int)x * (unsigned int)y;

    if (v > max_int || (x != 0 && v / x != y))
        return -1;
    *result = v | sign;
    return 0;
}

static inline uint32_t check_uint32_multiply(uint32_t x, uint32_t y, uint32_t *result)
{
    *result = x * y;

    if (x != 0 && (*result) / x != y)
        return (uint32_t)-1;
    return 0;
}

static inline size_t check_size_multiply(size_t x, size_t y, size_t *result)
{
    *result = x * y;

    if (x != 0 && (*result) / x != y)
        return (size_t)-1;
    return 0;
}

#endif /* stdint__INCLUDED */
