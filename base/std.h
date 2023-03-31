/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Standard definitions for Ghostscript code */

#ifndef std_INCLUDED
#  define std_INCLUDED

#include "stdpre.h"

/* Include the architecture definitions. */
#include "arch.h"

/*
 * Define the alignment that the memory manager must preserve.
 * We assume all alignment moduli are powers of 2.
 * NOTE: we require that malloc align blocks at least this strictly.
 */
#define ARCH_ALIGN_MEMORY_MOD\
  (((ARCH_ALIGN_LONG_MOD - 1) | (ARCH_ALIGN_PTR_MOD - 1) |\
    (ARCH_ALIGN_DOUBLE_MOD - 1)) + 1)

/* Define integer data type sizes in terms of log2s. */
#define ARCH_SIZEOF_CHAR (1 << ARCH_LOG2_SIZEOF_CHAR)
#define ARCH_SIZEOF_SHORT (1 << ARCH_LOG2_SIZEOF_SHORT)
#define ARCH_SIZEOF_INT (1 << ARCH_LOG2_SIZEOF_INT)
#define ARCH_SIZEOF_LONG (1 << ARCH_LOG2_SIZEOF_LONG)
#define ARCH_SIZEOF_LONG_LONG (1 << ARCH_LOG2_SIZEOF_LONG_LONG)
#define ARCH_INTS_ARE_SHORT (ARCH_SIZEOF_INT == ARCH_SIZEOF_SHORT)
#define ARCH_SIZEOF_INT64_T 8

/* Define whether we are on a large- or small-memory machine. */
/* Currently, we assume small memory and 16-bit ints are synonymous. */
#define ARCH_SMALL_MEMORY (ARCH_SIZEOF_INT <= 2)

/* Define unsigned 16- and 32-bit types.  These are needed in */
/* a surprising number of places that do bit manipulation. */
#if ARCH_SIZEOF_SHORT == 2      /* no plausible alternative! */
typedef ushort bits16;
#endif
#if ARCH_SIZEOF_INT == 4
typedef uint bits32;
#else
# if ARCH_SIZEOF_LONG == 4
typedef ulong bits32;
# endif
#endif

/* Minimum and maximum values for the signed types. */
/* Avoid casts, to make them acceptable to strict ANSI compilers. */
#define min_short (-1 << (ARCH_SIZEOF_SHORT * 8 - 1))
#define max_short (~min_short)
#define min_int (-1 << (ARCH_SIZEOF_INT * 8 - 1))
#define max_int (~min_int)
#define min_long (-1L << (ARCH_SIZEOF_LONG * 8 - 1))
#define max_long (~min_long)

#define min_int64_t (-((int64_t)1) << (sizeof(int64_t) * 8 - 1))
#define max_int64_t (~min_int64_t)
#define max_uint64_t ((uint64_t)~((uint64_t)0) + (uint64_t)0)

/*
 * The maximum values for the unsigned types are defined in arch.h,
 * because so many compilers handle unsigned constants wrong.
 * In particular, most of the DEC VMS compilers incorrectly sign-extend
 * short unsigned constants (but not short unsigned variables) when
 * widening them to longs.  We program around this on a case-by-case basis.
 * Some compilers don't truncate constants when they are cast down.
 * The UTek compiler does special weird things of its own.
 * All the rest (including gcc on all platforms) do the right thing.
 */
#define max_uchar ARCH_MAX_UCHAR
#define max_ushort ARCH_MAX_USHORT
#define max_uint ARCH_MAX_UINT
#define max_ulong ARCH_MAX_ULONG
#define max_size_t ARCH_MAX_SIZE_T

/* Minimum and maximum values for pointers. */
#if ARCH_PTRS_ARE_SIGNED
#  define min_ptr min_long
#  define max_ptr max_long
#else
#  define min_ptr ((ulong)0)
#  define max_ptr max_ulong
#endif

/* Define a reliable arithmetic right shift. */
/* Must use arith_rshift_1 for a shift by a literal 1. */
#define arith_rshift_slow(x,n) ((x) < 0 ? ~(~(x) >> (n)) : (x) >> (n))
#if ARCH_ARITH_RSHIFT == 2
#  define arith_rshift(x,n) ((x) >> (n))
#  define arith_rshift_1(x) ((x) >> 1)
#else
#if ARCH_ARITH_RSHIFT == 1      /* OK except for n=1 */
#  define arith_rshift(x,n) ((x) >> (n))
#  define arith_rshift_1(x) arith_rshift_slow(x,1)
#else
#  define arith_rshift(x,n) arith_rshift_slow(x,n)
#  define arith_rshift_1(x) arith_rshift_slow(x,1)
#endif
#endif

/*
 * Standard error printing macros.
 *
 * Use dmprintf (and dmputs, dmputc, dmflush etc for messages intended for
 * stdout). If you would like an optional filename/line number (where
 * available) prepended, use dmlprintf (and family).
 *
 * Use emprintf (and emputs, emputc, emflush etc for messages intended for
 * stderr).
 *
 * All these above functions/macros take a "const gs_memory_t *" pointer to
 * ensure that the correct stdout/stderr is used for the given gsapi instance.
 *
 * If you do not have a gs_memory_t * to hand, then you may call dprintf (and
 * family) and eprintf(and family) insteads. Be aware that these functions
 * compile away to nothing in non-DEBUG builds (and even in some DEBUG
 * environments), as they rely on platform specific threading tricks.
 *
 * Since we all stdout/stderr output must go via outprintf/errprintf,
 * we have to define dputc and dputs in terms of errprintf also.
 */

/*
 * We would prefer not to include stdio.h here, but we need it for
 * the FILE * argument of the printing procedures.
 */
#include <stdio.h>

/*
 * Not a very good place to define this, but we can't find a better one.
 */
#ifndef gs_memory_DEFINED
#  define gs_memory_DEFINED
typedef struct gs_memory_s gs_memory_t;
#endif

#define init_proc(proc)\
  int proc(gs_memory_t *)

/* dpf and epf may be redefined */
#define dpfm errprintf
#define epfm errprintf

#define dpf errprintf_nomem
#define epf errprintf_nomem

/* To allow stdout and stderr to be redirected, all stdout goes
 * though outwrite and all stderr goes through errwrite.
 */

int outwrite(const gs_memory_t *mem, const char *str, int len);
int errwrite(const gs_memory_t *mem, const char *str, int len);
void outflush(const gs_memory_t *mem);
void errflush(const gs_memory_t *mem);

/* Formatted output to outwrite and errwrite.
 * The maximum string length is 1023 characters.
 */
#ifdef __PROTOTYPES__
#  ifndef __printflike
   /* error checking for printf format args gcc only */
#    if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#      define __printflike(fmtarg, firstvararg) \
             __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#    else
#      define __printflike(fmtarg, firstvararg)
#    endif
#  endif
int outprintf(const gs_memory_t *mem, const char *fmt, ...) __printflike(2, 3);
int errprintf(const gs_memory_t *mem, const char *fmt, ...) __printflike(2, 3);
int errprintf_nomem(const char *fmt, ...) __printflike(1, 2);
#else
int outprintf();
int errprintf();
int errprintf_nomem();
#endif

/* Print the program line # for debugging */
/* These versions do not take a memory pointer, and hence have to
 * appeal to platform specific methods to get one from thread local
 * storage. These are likely to be much slower than the other versions,
 * and, on some platforms at least, may do nothing. Avoid these if
 * possible. */
#if __LINE__                    /* compiler provides it */
void dprintf_file_and_line(const char *, int);
#  define _dpl dprintf_file_and_line(__FILE__, __LINE__),
#else
void dprintf_file_only(const char *);
#  define _dpl dprintf_file_only(__FILE__),
#endif

void dflush(void);              /* flush stderr */
#define dputc(chr) dprintf1("%c", chr)
#define dlputc(chr) dlprintf1("%c", chr)
#define dputs(str) dprintf1("%s", str)
#define dlputs(str) dlprintf1("%s", str)
#define dprintf(str)\
  dpf(str)
#define dlprintf(str)\
  (_dpl dpf(str))
#define dprintf1(str,arg1)\
  dpf(str, arg1)
#define dlprintf1(str,arg1)\
  (_dpl dprintf1(str, arg1))
#define dprintf2(str,arg1,arg2)\
  dpf(str, arg1, arg2)
#define dlprintf2(str,arg1,arg2)\
  (_dpl dprintf2(str, arg1, arg2))
#define dprintf3(str,arg1,arg2,arg3)\
  dpf(str, arg1, arg2, arg3)
#define dlprintf3(str,arg1,arg2,arg3)\
  (_dpl dprintf3(str, arg1, arg2, arg3))
#define dprintf4(str,arg1,arg2,arg3,arg4)\
  dpf(str, arg1, arg2, arg3, arg4)
#define dlprintf4(str,arg1,arg2,arg3,arg4)\
  (_dpl dprintf4(str, arg1, arg2, arg3, arg4))
#define dprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  dpf(str, arg1, arg2, arg3, arg4, arg5)
#define dlprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  (_dpl dprintf5(str, arg1, arg2, arg3, arg4, arg5))
#define dprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  dpf(str, arg1, arg2, arg3, arg4, arg5, arg6)
#define dlprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_dpl dprintf6(str, arg1, arg2, arg3, arg4, arg5, arg6))
#define dprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  dpf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define dlprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_dpl dprintf7(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define dprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  dpf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define dlprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_dpl dprintf8(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define dprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  dpf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
#define dlprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_dpl dprintf9(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define dprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  dpf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#define dlprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_dpl dprintf10(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))
#define dprintf11(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  dpf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11)
#define dlprintf11(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  (_dpl dprintf11(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11))
#define dprintf12(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  dpf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12)
#define dlprintf12(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  (_dpl dprintf12(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12))

/* Print the program line # for debugging. */
#if __LINE__                    /* compiler provides it */
void dmprintf_file_and_line(const gs_memory_t *,const char *, int);
#  define _dmpl(M) dmprintf_file_and_line(M,__FILE__, __LINE__),
#else
void dmprintf_file_only(const gs_memory_t *,const char *);
#  define _dmpl(M) dmprintf_file_only(M,__FILE__),
#endif

#define dmflush(mem) errflush(mem)
#define dmputc(mem,chr) dmprintf1(mem,"%c", chr)
#define dmlputc(mem,chr) dmlprintf1(mem,"%c", chr)
#define dmputs(mem,str) dmprintf1(mem,"%s", str)
#define dmlputs(mem,str) dmlprintf1(mem,"%s", str)
#define dmprintf(mem,str)\
  dpfm(mem,str)
#define dmlprintf(mem,str)\
  (_dmpl(mem) dpfm(mem,str))
#define dmprintf1(mem,str,arg1)\
  dpfm(mem,str, arg1)
#define dmlprintf1(mem,str,arg1)\
  (_dmpl(mem) dmprintf1(mem,str, arg1))
#define dmprintf2(mem,str,arg1,arg2)\
  dpfm(mem,str, arg1, arg2)
#define dmlprintf2(mem,str,arg1,arg2)\
  (_dmpl(mem) dmprintf2(mem,str, arg1, arg2))
#define dmprintf3(mem,str,arg1,arg2,arg3)\
  dpfm(mem,str, arg1, arg2, arg3)
#define dmlprintf3(mem,str,arg1,arg2,arg3)\
  (_dmpl(mem) dmprintf3(mem,str, arg1, arg2, arg3))
#define dmprintf4(mem,str,arg1,arg2,arg3,arg4)\
  dpfm(mem,str, arg1, arg2, arg3, arg4)
#define dmlprintf4(mem,str,arg1,arg2,arg3,arg4)\
  (_dmpl(mem) dmprintf4(mem,str, arg1, arg2, arg3, arg4))
#define dmprintf5(mem,str,arg1,arg2,arg3,arg4,arg5)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5)
#define dmlprintf5(mem,str,arg1,arg2,arg3,arg4,arg5)\
  (_dmpl(mem) dmprintf5(mem,str, arg1, arg2, arg3, arg4, arg5))
#define dmprintf6(mem,str,arg1,arg2,arg3,arg4,arg5,arg6)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6)
#define dmlprintf6(mem,str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_dmpl(mem) dmprintf6(mem,str, arg1, arg2, arg3, arg4, arg5, arg6))
#define dmprintf7(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define dmlprintf7(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_dmpl(mem) dmprintf7(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define dmprintf8(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define dmlprintf8(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_dmpl(mem) dmprintf8(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define dmprintf9(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
#define dmlprintf9(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_dmpl(mem) dmprintf9(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define dmprintf10(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#define dmlprintf10(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_dmpl(mem) dmprintf10(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))
#define dmprintf11(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11)
#define dmlprintf11(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  (_dmpl(mem) dmprintf11(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11))
#define dmprintf12(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  dpfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12)
#define dmlprintf12(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  (_dmpl(mem) dmprintf12(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12))


void printf_program_ident(const gs_memory_t *mem, const char *program_name, long revision_number);
void emprintf_program_ident(const gs_memory_t *mem,
                            const char *program_name,
                            long revision_number);
const char *gs_program_family_name(void);
const char *gs_program_name(void);
long gs_revision_number(void);

/* These versions do not take a memory pointer, and hence have to
 * appeal to platform specific methods to get one from thread local
 * storage. These are likely to be much slower than the other versions,
 * and, on some platforms at least, may do nothing. Avoid these if
 * possible. */
void eprintf_program_ident(const char *program_name, long revision_number);
#define _epi eprintf_program_ident(gs_program_name(), gs_revision_number()),

#define eprintf(str)\
  (_epi epf(str))
#define eprintf1(str,arg1)\
  (_epi epf(str, arg1))
#define eprintf2(str,arg1,arg2)\
  (_epi epf(str, arg1, arg2))
#define eprintf3(str,arg1,arg2,arg3)\
  (_epi epf(str, arg1, arg2, arg3))
#define eprintf4(str,arg1,arg2,arg3,arg4)\
  (_epi epf(str, arg1, arg2, arg3, arg4))
#define eprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  (_epi epf(str, arg1, arg2, arg3, arg4, arg5))
#define eprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_epi epf(str, arg1, arg2, arg3, arg4, arg5, arg6))
#define eprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_epi epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define eprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_epi epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define eprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_epi epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define eprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_epi epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))

#define _epim(mem) emprintf_program_ident(mem, gs_program_name(), gs_revision_number()),

#define emprintf(mem, str)\
  (_epim(mem) epfm(mem, str))
#define emprintf1(mem, str,arg1)\
  (_epim(mem) epfm(mem, str, arg1))
#define emprintf2(mem, str,arg1,arg2)\
  (_epim(mem) epfm(mem, str, arg1, arg2))
#define emprintf3(mem, str,arg1,arg2,arg3)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3))
#define emprintf4(mem, str,arg1,arg2,arg3,arg4)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3, arg4))
#define emprintf5(mem, str,arg1,arg2,arg3,arg4,arg5)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3, arg4, arg5))
#define emprintf6(mem, str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3, arg4, arg5, arg6))
#define emprintf7(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define emprintf8(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define emprintf9(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define emprintf10(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_epim(mem) epfm(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))

#if __LINE__			/* compiler provides it */
void lprintf_file_and_line(const char *, int);
#  define _epl _epi lprintf_file_and_line(__FILE__, __LINE__),
#else
void lprintf_file_only(const char *);
#  define _epl _epi lprintf_file_only(__FILE__)
#endif

#define lprintf(str)\
  (_epl epf(str))
#define lprintf1(str,arg1)\
  (_epl epf(str, arg1))
#define lprintf2(str,arg1,arg2)\
  (_epl epf(str, arg1, arg2))
#define lprintf3(str,arg1,arg2,arg3)\
  (_epl epf(str, arg1, arg2, arg3))
#define lprintf4(str,arg1,arg2,arg3,arg4)\
  (_epl epf(str, arg1, arg2, arg3, arg4))
#define lprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  (_epl epf(str, arg1, arg2, arg3, arg4, arg5))
#define lprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_epl epf(str, arg1, arg2, arg3, arg4, arg5, arg6))
#define lprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_epl epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define lprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_epl epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define lprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_epl epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define lprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_epl epf(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))

#if __LINE__			/* compiler provides it */
void mlprintf_file_and_line(const gs_memory_t *,const char *, int);
#  define _eplm(mem) _epim(mem) mlprintf_file_and_line(mem, __FILE__, __LINE__),
#else
void lprintf_file_only(const gs_memory_t *,const char *);
#  define _eplm(mem) _epim(mem) mlprintf_file_only(mem, __FILE__)
#endif

#define mlprintf(mem,str)\
  (_eplm(mem) epfm(mem,str))
#define mlprintf1(mem,str,arg1)\
  (_eplm(mem) epfm(mem,str, arg1))
#define mlprintf2(mem,str,arg1,arg2)\
  (_eplm(mem) epfm(mem,str, arg1, arg2))
#define mlprintf3(mem,str,arg1,arg2,arg3)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3))
#define mlprintf4(mem,str,arg1,arg2,arg3,arg4)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3, arg4))
#define mlprintf5(mem,str,arg1,arg2,arg3,arg4,arg5)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3, arg4, arg5))
#define mlprintf6(mem,str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6))
#define mlprintf7(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define mlprintf8(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define mlprintf9(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define mlprintf10(mem,str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_eplm(mem) epfm(mem,str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))
#define init_proc(proc)\
  int proc(gs_memory_t *)

#endif /* std_INCLUDED */
