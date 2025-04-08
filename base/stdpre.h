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


/* Standard definitions for Ghostscript code not needing arch.h */

#ifndef stdpre_INCLUDED
#  define stdpre_INCLUDED

/* We do not use FILE * as a rule. Instead we use gp_file * and
 * gp_ prefixed versions of fgetc, fread, fwrite, fseek, ftell
 * fclose, etc.
 */
#ifndef gp_file_DEFINED
#  define gp_file_DEFINED
typedef struct gp_file_s gp_file;
#endif

/* if we define _LARGEFILE64_SOURCE zlib tries to include unistd.h */
#ifndef _MSC_VER
/* Ghostscript uses transitional LFS functions. */
#define _LARGEFILE64_SOURCE 1
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

/*
 * Here we deal with the vagaries of various C compilers.  We assume that:
 *      ANSI-standard Unix compilers define __STDC__.
 *      gcc defines __GNUC__.
 *      Microsoft C/C++ defines _MSC_VER and _MSDOS.
 *
 * The following are compilers we no longer support:
 *      Watcom C defines __WATCOMC__ and MSDOS.
 *      MetroWerks C defines __MWERKS__.
 *      Borland Turbo C and Turbo C++ define __MSDOS__ and __TURBOC__.
 *      Borland C++ defines __BORLANDC__, __MSDOS__, and __TURBOC__.
 *
 * We arrange to define __MSDOS__ on all the MS-DOS platforms.
 */
#if (defined(MSDOS) || defined(_MSDOS)) && !defined(__MSDOS__)
#  define __MSDOS__
#endif
/*
 * Also, not used much here, but used in other header files, we assume:
 *      Unix System V environments define SYSV.
 *      The SCO ODT compiler defines M_SYSV and M_SYS3.
 *      VMS systems define VMS.
 *      OSF/1 compilers define __osf__ or __OSF__.
 *        (The VMS and OSF/1 C compilers handle prototypes and const,
 *        but do not define __STDC__.)
 *      bsd 4.2 or 4.3 systems define BSD4_2.
 *      POSIX-compliant environments define _POSIX_SOURCE.
 *      Motorola 88K BCS/OCS systems defined m88k.
 *
 * We make fairly heroic efforts to confine all uses of these flags to
 * header files, and never to use them in code.
 */
#if defined(__osf__) && !defined(__OSF__)
#  define __OSF__		/* */
#endif
#if defined(M_SYSV) && !defined(SYSV)
#  define SYSV			/* */
#endif
#if defined(M_SYS3) && !defined(__SVR3)
#  define __SVR3		/* */
#endif

#if defined(__STDC__) || defined(__MSDOS__) || defined(__convex__) || defined(VMS) || defined(__OSF__) || defined(__WIN32__) || defined(__IBMC__) || defined(M_UNIX) || defined(__GNUC__) || defined(__BORLANDC__)
# if !(defined(M_XENIX) && !defined(__GNUC__))	/* SCO Xenix cc is broken */
#  define __PROTOTYPES__	/* */
# endif
#endif

/* Define dummy values for __FILE__ and __LINE__ if the compiler */
/* doesn't provide these.  Note that places that use __FILE__ */
/* must check explicitly for a null pointer. */
#ifndef __FILE__
#  define __FILE__ NULL
#endif
#ifndef __LINE__
#  define __LINE__ 0
#endif

/* Disable 'const' and 'volatile' if the compiler can't handle them. */
#ifndef __PROTOTYPES__
#  undef const
#  define const			/* */
#  undef volatile
#  define volatile		/* */
#endif

/* restrict is standard in C99, but not in all C++ compilers. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ == 199901L /* C99 */
# if defined(HAVE_RESTRICT) && HAVE_RESTRICT==1
#  define gs_restrict restrict
# else /* defined(HAVE_RESTRICT) && HAVE_RESTRICT==1 */
#  define gs_restrict
# endif /* defined(HAVE_RESTRICT) && HAVE_RESTRICT==1 */
#elif defined(_MSC_VER) && _MSC_VER >= 1500 /* MSVC 9 or newer */
# define gs_restrict __restrict
#elif __GNUC__ >= 3 /* GCC 3 or newer */
# if defined(HAVE_RESTRICT) && HAVE_RESTRICT==1
#  define gs_restrict __restrict
# else /* defined(HAVE_RESTRICT) && HAVE_RESTRICT==1 */
#  define gs_restrict
# endif /* defined(HAVE_RESTRICT) && HAVE_RESTRICT==1 */
#else /* Unknown or ancient */
# define gs_restrict
#endif



/* Ensure we have a definition of 'inline', even if that means
 * disabling it if the compiler can't handle it. */
#ifdef __cplusplus
 /* inline will already be defined within C++ */
#elif defined (__STDC_VERSION_) && (__STDC_VERSION__ >= 199901L)
 /* inline will already be defined within C99 */
#elif defined(_MSC_VER) && (_MSC_VER >= 1500) /* MSVC 9 or newer */
 #define inline __inline
#elif defined(__GNUC__) && (__GNUC__ >= 3) /* GCC 3 or newer */
 /* Define inline as __inline__ so -pedantic won't produce a warning. */
 #undef inline
 #define inline __inline__
#elif defined(__DECC)
 #undef inline
 #define inline __inline
#elif !(defined(__MWERKS__) || defined(inline))
 /* Unknown or ancient - disable it */
 #define inline
#endif

/* Define ourselves a 'forceinline' we can use to more forcefully
 * tell the compiler to inline something. On all but MSVC this can
 * drop back to inline. */
#ifdef _MSC_VER
#define forceinline __forceinline
#else
#define forceinline inline
#endif

/*
 * Provide a way to include inline procedures in header files, regardless of
 * whether the compiler (A) doesn't support inline at all, (B) supports it
 * but also always compiles a closed copy, (C) supports it but somehow only
 * includes a single closed copy in the executable, or (D) supports it and
 * also supports a different syntax if no closed copy is desired.
 *
 * The code that appears just after this comment indicates which compilers
 * are of which kind.  (Eventually this might be determined automatically.)
 *	(A) and (B) require nothing here.
 *	(C) requires
 *		#define extern_inline inline
 *	(D) requires
 *		#define extern_inline extern inline  // or whatever
 * Note that for case (B), the procedure will only be declared inline in
 * the .c file where its closed copy is compiled.
 */
#ifdef __GNUC__
#  define extern_inline extern inline
#endif

/*
 * To include an inline procedure xyz in a header file abc.h, use the
 * following template in the header file:

extern_inline int xyz(<<parameters>>)
#if HAVE_EXTERN_INLINE || defined(INLINE_INCLUDE_xyz)
{
    <<body>>
}
#else
;
#endif

 * And use the following in whichever .c file takes responsibility for
 * including the closed copy of xyz:

#define EXTERN_INCLUDE_xyz	// must precede all #includes
#include "abc.h"

 * The definitions of the EXTERN_INCLUDE_ macros must precede *all* includes
 * because there is no way to know whether some other .h file #includes abc.h
 * indirectly, and because of the protection against double #includes, the
 * EXTERN_INCLUDE_s must be defined before the first inclusion of abc.h.
 */

/*
 * The following is generic code that does not need per-compiler
 * customization.
 */
#ifdef extern_inline
#  define HAVE_EXTERN_INLINE 1
#else
#  define extern_inline /* */
#  define HAVE_EXTERN_INLINE 0
#endif

/*
 * Some compilers give a warning if a function call that returns a value
 * is used as a statement; a few compilers give an error for the construct
 * (void)0, which is contrary to the ANSI standard.  Since we don't know of
 * any compilers that do both, we define a macro here for discarding
 * the value of an expression statement, which can be defined as either
 * including or not including the cast.  (We don't conditionalize this here,
 * because no commercial compiler gives the error on (void)0, although
 * some give warnings.)  */
#define DISCARD(expr) ((void)(expr))
/* Backward compatibility */
#define discard(expr) DISCARD(expr)

/*
 * Some versions of the Watcom compiler give a "Comparison result always
 * 0/1" message that we want to suppress because it gets in the way of
 * meaningful warnings.
 */
#ifdef __WATCOMC__
#  pragma disable_message(124);
#endif

/*
 * Some versions of gcc have a bug such that after
        byte *p;
        ...
        x = *(long *)p;
 * the compiler then thinks that p always points to long-aligned data.
 * Detect this here so it can be handled appropriately in the few places
 * that (we think) matter.
 */
#ifdef __GNUC__
# if __GNUC__ == 2 & (7 < __GNUC_MINOR__ <= 95)
#  define ALIGNMENT_ALIASING_BUG
# endif
#endif

/*
 * The SVR4.2 C compiler incorrectly considers the result of << and >>
 * to be unsigned if the left operand is signed and the right operand is
 * unsigned.  We believe this only causes trouble in Ghostscript code when
 * the right operand is a sizeof(...), which is unsigned for this compiler.
 * Therefore, we replace the relevant uses of sizeof with size_of:
 */
#define size_of(x) ((int)(sizeof(x)))

/*
 * far_data was formerly used for static data that had to be assigned its
 * own segment on PCs with 64K segments.  This was supported in Borland C++,
 * but none of the other compilers.  Since we no longer support
 * small-segment systems, far_data is vacuous.
 */
#undef far_data
#define far_data /* */

/*
 * Get the number of elements of a statically dimensioned array.
 * Note that this also works on array members of structures.
 */
#define countof(a) (sizeof(a) / sizeof((a)[0]))
#define count_of(a) (size_of(a) / size_of((a)[0]))

/*
 * Get the offset of a structure member.  Amazingly enough, the simpler
 * definition works on all compilers except for one broken MIPS compiler
 * and the IBM RS/6000.  Unfortunately, because of these two compilers,
 * we have to use the more complex definition.  Even more unfortunately,
 * the more complex definition doesn't work on the MetroWerks
 * CodeWarrior compiler (Macintosh and BeOS).
 */
#ifdef __MWERKS__
#define offset_of(type, memb)\
 ((int) &((type *) 0)->memb)
#else
#define offset_of(type, memb)\
 ((int) ( (char *)&((type *)0)->memb - (char *)((type *)0) ))
#endif

/*
 * Get the alignment of a pointer modulo a given power of 2.
 * There is no portable way to do this, but the following definition
 * works on all reasonable systems.
 */
#define ALIGNMENT_MOD(ptr, modu)\
  ((uint)( ((const char *)(ptr) - (const char *)0) & ((modu) - 1) ))

/* Define short names for the unsigned types. */
typedef unsigned char byte;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

/* And for signed char */
typedef signed char schar;

/* Since sys/types.h may define one or more of these (depending on
 * the platform), we have to take steps to prevent name clashes.
 * Unfortunately this can clobber valid definitions for the size-
 * specific types, but there's no simple solution.
 *
 * NOTE: This requires that you include std.h *before* any other
 * header file that includes sys/types.h.
 *
 */
#undef bool
#define bool bool_		/* (maybe not needed) */
#define uchar uchar_
#define uint uint_
#define ushort ushort_
#define ulong ulong_
#define schar schar_
#include <sys/types.h>
#undef bool
#undef uchar
#undef uint
#undef ushort
#undef ulong
#undef schar

/*
 * Define a Boolean type.  Even though we would like it to be
 * unsigned char, it pretty well has to be int, because
 * that's what all the relational operators and && and || produce.
 * We can't make it an enumerated type, because ints don't coerce
 * freely to enums (although the opposite is true).
 * Unfortunately, at least some C++ compilers have a built-in bool type,
 * and the MetroWerks C++ compiler insists that bool be equivalent to
 * unsigned char.
 */

/* C23 has bool as a builtin type. */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#ifdef __BEOS__
typedef unsigned char bool;
#else
typedef int bool;
#endif
#endif
/*
 * Older versions of MetroWerks CodeWarrior defined true and false, but they're now
 * an enum in the (MacOS) Universal Interfaces. The only way around this is to escape
 * our own definitions wherever MacTypes.h is included.
 */
#if defined(_MSC_VER) && _MSC_VER >= 1900
/* VS 2014 defines bool already, but has it as _Bool (a 1 byte thing).
 * We can't live with that. */
#undef false
#define false ((bool)0)
#undef true
#define true ((bool)1)
#else
#ifndef __MACOS__
#undef false
#define false ((bool)0)
#undef true
#define true ((bool)1)
#endif /* __MACOS__ */
#endif

/*
 * Compilers disagree as to whether macros used in macro arguments
 * should be expanded at the time of the call, or at the time of
 * final expansion.  Even authoritative documents disagree: the ANSI
 * standard says the former, but Harbison and Steele's book says the latter.
 * In order to work around this discrepancy, we have to do some very
 * ugly things in a couple of places.  We mention it here because
 * it might well trip up future developers.
 */

/*
 * Define the type to be used for ordering pointers (<, >=, etc.).
 * This used to have 'unsigned long' for comparison of segment+offset,
 * but that hasn't been an issue for YEARS. The old version was not
 * compatible with 64-bit.
 */
typedef const char *ptr_ord_t;

/* Define all the pointer comparison operations. */
#define _PTR_CMP(p1, rel, p2)  ((ptr_ord_t)(p1) rel (ptr_ord_t)(p2))
#define PTR_LE(p1, p2) _PTR_CMP(p1, <=, p2)
#define PTR_LT(p1, p2) _PTR_CMP(p1, <, p2)
#define PTR_GE(p1, p2) _PTR_CMP(p1, >=, p2)
#define PTR_GT(p1, p2) _PTR_CMP(p1, >, p2)
#define PTR_BETWEEN(ptr, lo, hi)\
  (PTR_GE(ptr, lo) && PTR_LT(ptr, hi))

/* Define  min and max, but make sure to use the identical definition */
/* to the one that all the compilers seem to have.... */
#ifndef min
#  define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#  define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* Define a standard way to round values to a (constant) modulus. */
#define ROUND_DOWN(value, modulus)\
  ( (modulus) & ((modulus) - 1) ?	/* not a power of 2 */\
    (value) - (value) % (modulus) :\
    (value) & -(modulus) )
#define ROUND_UP(value, modulus)\
  ( (modulus) & ((modulus) - 1) ?	/* not a power of 2 */\
    ((value) + ((modulus) - 1)) / (modulus) * (modulus) :\
    ((value) + ((modulus) - 1)) & -(modulus) )
/* Backward compatibility */
#define round_up(v, m) ROUND_UP(v, m)
#define round_down(v, m) ROUND_DOWN(v, m)

/*
 * Because of C's strange insistence that ; is a terminator and not a
 * separator, compound statements {...} are not syntactically equivalent to
 * single statements.  Therefore, we define here a compound-statement
 * construct that *is* syntactically equivalent to a single statement.
 * Usage is
 *      BEGIN
 *        ...statements...
 *      END
 */
#define BEGIN	do {
#define END	} while (0)

/*
 * Define a handy macro for a statement that does nothing.
 * We can't just use an empty statement, since this upsets some compilers.
 */
#ifndef DO_NOTHING
#  define DO_NOTHING BEGIN END
#endif

/*
 * For accountability, debugging, and error messages, we pass a client
 * identification string to alloc and free, and possibly other places as
 * well.  Define the type for these strings.
 */
typedef const char *client_name_t;
/****** WHAT TO DO ABOUT client_name_string ? ******/
#define client_name_string(cname) (cname)

/*
 * Define success and failure codes for 'exit'.  The only system on which
 * they are different is VMS with older DEC C versions.  We aren't sure
 * in what version DEC C started being compatible with the rest of the
 * world, and we don't know what the story is with VAX C.  If you have
 * problems, uncomment the following line or add -DOLD_VMS_C to the C
 * command line.
 */
/*#define OLD_VMS_C*/
#if defined(VMS)
#  define exit_FAILED 18
#  if (defined(OLD_VMS_C) || !defined(__DECC))
#    define exit_OK 1
#  else
#    define exit_OK 0
#  endif
#else
#  define exit_OK 0
#  define exit_FAILED 1
#endif

#endif /* stdpre_INCLUDED */
