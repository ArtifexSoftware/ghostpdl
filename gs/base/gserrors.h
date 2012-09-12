/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Error code definitions */

#ifndef gserrors_INCLUDED
#  define gserrors_INCLUDED

/* A procedure that may return an error always returns */
/* a non-negative value (zero, unless otherwise noted) for success, */
/* or negative for failure. */
/* We use ints rather than an enum to avoid a lot of casting. */

#define gs_error_unknownerror (-1)	/* unknown error */
#define gs_error_interrupt (-6)
#define gs_error_invalidaccess (-7)
#define gs_error_invalidfileaccess (-9)
#define gs_error_invalidfont (-10)
#define gs_error_ioerror (-12)
#define gs_error_limitcheck (-13)
#define gs_error_nocurrentpoint (-14)
#define gs_error_rangecheck (-15)
#define gs_error_typecheck (-20)
#define gs_error_undefined (-21)
#define gs_error_undefinedfilename (-22)
#define gs_error_undefinedresult (-23)
#define gs_error_VMerror (-25)
#define gs_error_unregistered (-28)

#define gs_error_hit_detected (-99)

#define gs_error_Fatal (-100)

/* Need the remap color error for high level pattern support */
#define gs_error_Remap_Color (-103)

int gs_log_error(int, const char *, int);
#if !defined(DEBUG) || defined(GS_THREADSAFE)
#  define gs_log_error(err, file, line) (err)
#endif
#ifdef GS_THREADSAFE
#define gs_note_error(err) (err)
#define return_error(err) return (err)
#else
#define gs_note_error(err) gs_log_error(err, __FILE__, __LINE__)
#define return_error(err) return gs_note_error(err)
#endif

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#  if defined(__GNUC__) && __GNUC__ >= 2
#    define __func__ __FUNCTION__
#  elif defined(__FUNCTION__)
#    define __func__ __FUNCTION__
#  elif defined(__FUNC__)
#    define __func__ __FUNC__
#  else
#    define __func__ "<unknown>"
#  endif
#endif

/*
 * Error reporting macros.
 *
 */

#ifndef __printflike
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#define __printflike(fmtarg, firstvararg) \
    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define __printflike(fmtarg, firstvararg)
#endif
#endif

const char *gs_errstr(int code);

#ifdef GS_THREADSAFE
/* In threadsafe builds, we just swallow errors unreported */
#define gs_throw_code(code) \
    (code)

#define gs_throw(code, fmt) \
    (code)
#define gs_throw1(code, fmt, arg1) \
    (code)
#define gs_throw2(code, fmt, arg1, arg2) \
    (code)
#define gs_throw3(code, fmt, arg1, arg2, arg3) \
    (code)
#define gs_throw4(code, fmt, arg1, arg2, arg3, arg4) \
    (code)
#define gs_throw5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    (code)
#define gs_throw6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    (code)
#define gs_throw7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    (code)
#define gs_throw8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    (code)
#define gs_throw9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    (code)

/* Bubble the code up the stack
*/
#define gs_rethrow_code(code) \
    (code)

#define gs_rethrow(code, fmt) \
    (code)
#define gs_rethrow1(code, fmt, arg1) \
    (code)
#define gs_rethrow2(code, fmt, arg1, arg2) \
    (code)
#define gs_rethrow3(code, fmt, arg1, arg2, arg3) \
    (code)
#define gs_rethrow4(code, fmt, arg1, arg2, arg3, arg4) \
    (code)
#define gs_rethrow5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    (code)
#define gs_rethrow6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    (code)
#define gs_rethrow7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    (code)
#define gs_rethrow8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    (code)
#define gs_rethrow9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    (code)

/* This will cause trouble, as it implies you are fixing an error
 * the system will spew messages
 */
#define gs_catch(code, fmt) \
    (code)
#define gs_catch1(code, fmt, arg1) \
    (code)
#define gs_catch2(code, fmt, arg1, arg2) \
    (code)
#define gs_catch3(code, fmt, arg1, arg2, arg3) \
    (code)
#define gs_catch4(code, fmt, arg1, arg2, arg3, arg4) \
    (code)
#define gs_catch5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    (code)
#define gs_catch6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    (code)
#define gs_catch7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    (code)
#define gs_catch8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    (code)
#define gs_catch9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    (code)

/* gs_warn is a printf
 */
#define gs_warn(fmt) \
    DO_NOTHING
#define gs_warn1(fmt, arg1) \
    DO_NOTHING
#define gs_warn2(fmt, arg1, arg2) \
    DO_NOTHING
#define gs_warn3(fmt, arg1, arg2, arg3) \
    DO_NOTHING
#define gs_warn4(fmt, arg1, arg2, arg3, arg4) \
    DO_NOTHING
#define gs_warn5(fmt, arg1, arg2, arg3, arg4, arg5) \
    DO_NOTHING
#define gs_warn6(fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    DO_NOTHING
#define gs_warn7(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    DO_NOTHING
#define gs_warn8(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    DO_NOTHING
#define gs_warn9(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    DO_NOTHING
#else
int gs_throw_imp(const char *func, const char *file, int line,
        int op, int code, const char *fmt, ...) __printflike(6, 7);

/* Use throw at origin of error
*/
#define gs_throw_code(code) \
    gs_throw1((code), "%s", gs_errstr((code)))

#define gs_throw(code, fmt) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt)
#define gs_throw1(code, fmt, arg1) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1)
#define gs_throw2(code, fmt, arg1, arg2) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2)
#define gs_throw3(code, fmt, arg1, arg2, arg3) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3)
#define gs_throw4(code, fmt, arg1, arg2, arg3, arg4) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4)
#define gs_throw5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5)
#define gs_throw6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define gs_throw7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define gs_throw8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define gs_throw9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)

/* Bubble the code up the stack
*/
#define gs_rethrow_code(code) \
    gs_rethrow1((code), "%s", gs_errstr((code)))

#define gs_rethrow(code, fmt) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt)
#define gs_rethrow1(code, fmt, arg1) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1)
#define gs_rethrow2(code, fmt, arg1, arg2) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2)
#define gs_rethrow3(code, fmt, arg1, arg2, arg3) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3)
#define gs_rethrow4(code, fmt, arg1, arg2, arg3, arg4) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4)
#define gs_rethrow5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5)
#define gs_rethrow6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define gs_rethrow7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define gs_rethrow8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define gs_rethrow9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)

/* This will cause trouble, as it implies you are fixing an error
 * the system will spew messages
 */
#define gs_catch(code, fmt) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt)
#define gs_catch1(code, fmt, arg1) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1)
#define gs_catch2(code, fmt, arg1, arg2) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2)
#define gs_catch3(code, fmt, arg1, arg2, arg3) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3)
#define gs_catch4(code, fmt, arg1, arg2, arg3, arg4) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4)
#define gs_catch5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5)
#define gs_catch6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define gs_catch7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define gs_catch8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define gs_catch9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    gs_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)

/* gs_warn is a printf
 */
#define gs_warn(fmt) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt)
#define gs_warn1(fmt, arg1) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1)
#define gs_warn2(fmt, arg1, arg2) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2)
#define gs_warn3(fmt, arg1, arg2, arg3) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3)
#define gs_warn4(fmt, arg1, arg2, arg3, arg4) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4)
#define gs_warn5(fmt, arg1, arg2, arg3, arg4, arg5) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5)
#define gs_warn6(fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define gs_warn7(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define gs_warn8(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define gs_warn9(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    (void)gs_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
#endif

/* just in case you don't know 0 means no error
 * other return codes are errors.
 */
#define gs_okay 0

#endif /* gserrors_INCLUDED */
