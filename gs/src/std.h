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

/*$RCSfile$ $Revision$ */
/* Standard definitions for Aladdin Enterprises code */

#ifndef std_INCLUDED
#  define std_INCLUDED

#include "stdpre.h"

/* Include the architecture definitions. */
#include "arch.h"

/*
 * Define lower-case versions of the architecture parameters for backward
 * compatibility.
 */
#define arch_align_short_mod ARCH_ALIGN_SHORT_MOD
#define arch_align_int_mod ARCH_ALIGN_INT_MOD
#define arch_align_long_mod ARCH_ALIGN_LONG_MOD
#define arch_align_ptr_mod ARCH_ALIGN_PTR_MOD
#define arch_align_float_mod ARCH_ALIGN_FLOAT_MOD
#define arch_align_double_mod ARCH_ALIGN_DOUBLE_MOD
#define arch_align_struct_mod ARCH_ALIGN_STRUCT_MOD
#define arch_log2_sizeof_short ARCH_LOG2_SIZEOF_SHORT
#define arch_log2_sizeof_int ARCH_LOG2_SIZEOF_INT
#define arch_log2_sizeof_long ARCH_LOG2_SIZEOF_LONG
#define arch_sizeof_ptr ARCH_SIZEOF_PTR
#define arch_sizeof_float ARCH_SIZEOF_FLOAT
#define arch_sizeof_double ARCH_SIZEOF_DOUBLE
#define arch_float_mantissa_bits ARCH_FLOAT_MANTISSA_BITS
#define arch_double_mantissa_bits ARCH_DOUBLE_MANTISSA_BITS
#define arch_max_uchar ARCH_MAX_UCHAR
#define arch_max_ushort ARCH_MAX_USHORT
#define arch_max_uint ARCH_MAX_UINT
#define arch_max_ulong ARCH_MAX_ULONG
#define arch_cache1_size ARCH_CACHE1_SIZE
#define arch_cache2_size ARCH_CACHE2_SIZE
#define arch_is_big_endian ARCH_IS_BIG_ENDIAN
#define arch_ptrs_are_signed ARCH_PTRS_ARE_SIGNED
#define arch_floats_are_IEEE ARCH_FLOATS_ARE_IEEE
#define arch_arith_rshift ARCH_ARITH_RSHIFT
#define arch_can_shift_full_long ARCH_CAN_SHIFT_FULL_LONG
/*
 * Define the alignment that the memory manager must preserve.
 * We assume all alignment moduli are powers of 2.
 * NOTE: we require that malloc align blocks at least this strictly.
 */
#define ARCH_ALIGN_MEMORY_MOD\
  (((ARCH_ALIGN_LONG_MOD - 1) | (ARCH_ALIGN_PTR_MOD - 1) |\
    (ARCH_ALIGN_DOUBLE_MOD - 1) | (ARCH_ALIGN_STRUCT_MOD - 1)) + 1)
#define arch_align_memory_mod ARCH_ALIGN_MEMORY_MOD

/* Define integer data type sizes in terms of log2s. */
#define ARCH_SIZEOF_SHORT (1 << ARCH_LOG2_SIZEOF_SHORT)
#define ARCH_SIZEOF_INT (1 << ARCH_LOG2_SIZEOF_INT)
#define ARCH_SIZEOF_LONG (1 << ARCH_LOG2_SIZEOF_LONG)
#define ARCH_INTS_ARE_SHORT (ARCH_SIZEOF_INT == ARCH_SIZEOF_SHORT)
/* Backward compatibility */
#define arch_sizeof_short ARCH_SIZEOF_SHORT
#define arch_sizeof_int ARCH_SIZEOF_INT
#define arch_sizeof_long ARCH_SIZEOF_LONG
#define arch_ints_are_short ARCH_INTS_ARE_SHORT

/* Define whether we are on a large- or small-memory machine. */
/* Currently, we assume small memory and 16-bit ints are synonymous. */
#define ARCH_SMALL_MEMORY (ARCH_SIZEOF_INT <= 2)
/* Backward compatibility */
#define arch_small_memory ARCH_SMALL_MEMORY

/* Define unsigned 16- and 32-bit types.  These are needed in */
/* a surprising number of places that do bit manipulation. */
#if arch_sizeof_short == 2	/* no plausible alternative! */
typedef ushort bits16;
#endif
#if arch_sizeof_int == 4
typedef uint bits32;
#else
# if arch_sizeof_long == 4
typedef ulong bits32;
# endif
#endif

/* Minimum and maximum values for the signed types. */
/* Avoid casts, to make them acceptable to strict ANSI compilers. */
#define min_short (-1 << (arch_sizeof_short * 8 - 1))
#define max_short (~min_short)
#define min_int (-1 << (arch_sizeof_int * 8 - 1))
#define max_int (~min_int)
#define min_long (-1L << (arch_sizeof_long * 8 - 1))
#define max_long (~min_long)

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
#define max_uchar arch_max_uchar
#define max_ushort arch_max_ushort
#define max_uint arch_max_uint
#define max_ulong arch_max_ulong

/* Minimum and maximum values for pointers. */
#if arch_ptrs_are_signed
#  define min_ptr min_long
#  define max_ptr max_long
#else
#  define min_ptr ((ulong)0)
#  define max_ptr max_ulong
#endif

/* Define a reliable arithmetic right shift. */
/* Must use arith_rshift_1 for a shift by a literal 1. */
#define arith_rshift_slow(x,n) ((x) < 0 ? ~(~(x) >> (n)) : (x) >> (n))
#if arch_arith_rshift == 2
#  define arith_rshift(x,n) ((x) >> (n))
#  define arith_rshift_1(x) ((x) >> 1)
#else
#if arch_arith_rshift == 1	/* OK except for n=1 */
#  define arith_rshift(x,n) ((x) >> (n))
#  define arith_rshift_1(x) arith_rshift_slow(x,1)
#else
#  define arith_rshift(x,n) arith_rshift_slow(x,n)
#  define arith_rshift_1(x) arith_rshift_slow(x,1)
#endif
#endif

/*
 * Standard error printing macros.
 * Use dprintf for messages that just go to dpf;
 * dlprintf for messages to dpf with optional with file name (and,
 * if available, line number);
 * eprintf for error messages to epf that include the program name;
 * lprintf for debugging messages that should include line number info.
 * Since we all stdout/stderr output must go via outprintf/errprintf,
 * we have to define dputc and dputs in terms of errprintf also.
 */

/*
 * We would prefer not to include stdio.h here, but we need it for
 * the FILE * argument of the printing procedures.
 */
#include <stdio.h>

/*
 * a very good place to define this, but we can't find a better one.
 */
#ifndef gs_memory_DEFINED
#  define gs_memory_DEFINED
typedef struct gs_memory_s gs_memory_t;
#endif

#define init_proc(proc)\
  int proc(gs_memory_t *)


/* dpf and epf may be redefined */
#define dpf errprintf
#define epf errprintf

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
int outprintf(const gs_memory_t *mem, const char *fmt, ...);
int errprintf(const gs_memory_t *mem, const char *fmt, ...);


/* Print the program line # for debugging. */
#if __LINE__			/* compiler provides it */
void dprintf_file_and_line(const gs_memory_t *mem, const char *, int);
#  define _dpl(mem) dprintf_file_and_line(mem, __FILE__, __LINE__),
#else
void dprintf_file_only(const gs_memory_t *mem, const char *);
#  define _dpl(mem) dprintf_file_only(mem, __FILE__),
#endif

void dflush(const gs_memory_t *mem);		/* flush stderr */
#define dputc(mem, chr) dprintf1(mem, "%c", chr)
#define dlputc(mem, chr) dlprintf1(mem, "%c", chr)
#define dputs(mem, str) dprintf1(mem, "%s", str)
#define dlputs(mem, str) dlprintf1(mem, "%s", str)
#define dprintf(mem, str)\
  dpf(mem, str)
#define dlprintf(mem, str)\
  (_dpl(mem) dpf(mem, str))
#define dprintf1(mem, str, arg1)\
  dpf(mem, str, arg1)
#define dlprintf1(mem, str,arg1)\
  (_dpl(mem) dprintf1(mem, str, arg1))
#define dprintf2(mem, str,arg1,arg2)\
  dpf(mem, str, arg1, arg2)
#define dlprintf2(mem, str,arg1,arg2)\
  (_dpl(mem) dprintf2(mem, str, arg1, arg2))
#define dprintf3(mem, str,arg1,arg2,arg3)\
  dpf(mem, str, arg1, arg2, arg3)
#define dlprintf3(mem, str,arg1,arg2,arg3)\
  (_dpl(mem) dprintf3(mem, str, arg1, arg2, arg3))
#define dprintf4(mem, str,arg1,arg2,arg3,arg4)\
  dpf(mem, str, arg1, arg2, arg3, arg4)
#define dlprintf4(mem, str,arg1,arg2,arg3,arg4)\
  (_dpl(mem) dprintf4(mem, str, arg1, arg2, arg3, arg4))
#define dprintf5(mem, str,arg1,arg2,arg3,arg4,arg5)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5)
#define dlprintf5(mem, str,arg1,arg2,arg3,arg4,arg5)\
  (_dpl(mem) dprintf5(mem, str, arg1, arg2, arg3, arg4, arg5))
#define dprintf6(mem, str,arg1,arg2,arg3,arg4,arg5,arg6)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6)
#define dlprintf6(mem, str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_dpl(mem) dprintf6(mem, str, arg1, arg2, arg3, arg4, arg5, arg6))
#define dprintf7(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define dlprintf7(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_dpl(mem) dprintf7(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define dprintf8(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define dlprintf8(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_dpl(mem) dprintf8(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define dprintf9(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
#define dlprintf9(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_dpl(mem) dprintf9(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define dprintf10(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#define dlprintf10(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_dpl(mem) dprintf10(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))
#define dprintf11(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11)
#define dlprintf11(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  (_dpl(mem) dprintf11(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11))
#define dprintf12(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  dpf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12)
#define dlprintf12(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  (_dpl(mem) dprintf12(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12))

void printf_program_ident(const gs_memory_t *mem, const char *program_name, long revision_number);
void eprintf_program_ident(const gs_memory_t *mem,const char *program_name, long revision_number);
const char *gs_program_name(void);
long gs_revision_number(void);

#define _epi(mem) eprintf_program_ident(mem, gs_program_name(), gs_revision_number()),

#define eprintf(mem, str)\
  (_epi(mem) epf(mem, str))
#define eprintf1(mem, str,arg1)\
  (_epi(mem) epf(mem, str, arg1))
#define eprintf2(mem, str,arg1,arg2)\
  (_epi(mem) epf(mem, str, arg1, arg2))
#define eprintf3(mem, str,arg1,arg2,arg3)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3))
#define eprintf4(mem, str,arg1,arg2,arg3,arg4)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3, arg4))
#define eprintf5(mem, str,arg1,arg2,arg3,arg4,arg5)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5))
#define eprintf6(mem, str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6))
#define eprintf7(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define eprintf8(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define eprintf9(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define eprintf10(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_epi(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))

#if __LINE__			/* compiler provides it */
void lprintf_file_and_line(const gs_memory_t *mem, const char *, int);
#  define _epl(mem) _epi(mem) lprintf_file_and_line(mem, __FILE__, __LINE__),
#else
void lprintf_file_only(const gs_memory_t *mem, const char *);
#  define _epl(mem) _epi(mem) lprintf_file_only(mem, __FILE__)
#endif

#define lprintf(mem, str)\
  (_epl(mem) epf(mem, str))
#define lprintf1(mem, str,arg1)\
  (_epl(mem) epf(mem, str, arg1))
#define lprintf2(mem, str,arg1,arg2)\
  (_epl(mem) epf(mem, str, arg1, arg2))
#define lprintf3(mem, str,arg1,arg2,arg3)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3))
#define lprintf4(mem, str,arg1,arg2,arg3,arg4)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3, arg4))
#define lprintf5(mem, str,arg1,arg2,arg3,arg4,arg5)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5))
#define lprintf6(mem, str,arg1,arg2,arg3,arg4,arg5,arg6)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6))
#define lprintf7(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7))
#define lprintf8(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8))
#define lprintf9(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9))
#define lprintf10(mem, str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  (_epl(mem) epf(mem, str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10))


#endif /* std_INCLUDED */
