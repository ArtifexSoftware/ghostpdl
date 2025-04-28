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


/* $Id: setjmp_.h 6651 2006-03-13 16:18:19Z raph $ */
/* Macros for making sure setjmp buffers are correctly aligned. */

#ifndef setjmp_INCLUDED
#  define setjmp_INCLUDED

#include <setjmp.h>

#ifndef OPTIMISE_SETJMP
#  if defined(__clang__) && __clang__==1
#    define OPTIMIZE_SETJMP __attribute__((optnone))
#  else
#    define OPTIMIZE_SETJMP
#  endif
#endif

#ifndef INIT_GS_JMPBUF
#  if defined(__clang__) && __clang__==1
#    define INIT_GS_JMPBUF 1
#  else
#    define INIT_GS_JMPBUF 0
#  endif
#endif

typedef struct {
    char c;
    jmp_buf j;
} gsfix_jmp_buf_test;

#define gsfix_jmp_buf_align ((size_t)&((gsfix_jmp_buf_test*)0)->j)

/* We previously used sizeof(jmp_buf) + gsfix_jmp_buf_align for the
   content of gsfix_jmp_buf, but the compiler (gcc/clang) considered
   gsfix_jmp_buf_align to be a variable which meant a variable sized
   array, and that's not allowed.
   Using 2 * sizeof(jmp_buf) solves that. It is slightly wasteful but
   this is not an object that many instances will exist at a given time.
 */
typedef struct {
    unsigned char stuff[sizeof(jmp_buf) * 2];
} gsfix_jmp_buf;

/* This could be moved into a function, but it's fairly harmless as a macro. */
#define find_jmp_buf(gsfjb) 				\
  (*(jmp_buf *)(					\
   ((size_t)(gsfjb).stuff + gsfix_jmp_buf_align) 	\
   & ~(size_t)(gsfix_jmp_buf_align-1)			\
  ))

#endif /* setjmp_INCLUDED */
