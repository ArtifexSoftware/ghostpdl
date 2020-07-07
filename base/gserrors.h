/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* Error code definitions */

#ifndef gserrors_INCLUDED
#  define gserrors_INCLUDED

/* A procedure that may return an error always returns */
/* a non-negative value (zero, unless otherwise noted) for success, */
/* or negative for failure. */
/* We don't use a typedef internally to avoid a lot of casting. */

enum gs_error_type {
    gs_error_ok = 0,
    gs_error_unknownerror = -1,	/* unknown error */
    gs_error_dictfull = -2,
    gs_error_dictstackoverflow = -3,
    gs_error_dictstackunderflow = -4,
    gs_error_execstackoverflow = -5,
    gs_error_interrupt = -6,
    gs_error_invalidaccess = -7,
    gs_error_invalidexit = -8,
    gs_error_invalidfileaccess = -9,
    gs_error_invalidfont = -10,
    gs_error_invalidrestore = -11,
    gs_error_ioerror = -12,
    gs_error_limitcheck = -13,
    gs_error_nocurrentpoint = -14,
    gs_error_rangecheck = -15,
    gs_error_stackoverflow = -16,
    gs_error_stackunderflow = -17,
    gs_error_syntaxerror = -18,
    gs_error_timeout = -19,
    gs_error_typecheck = -20,
    gs_error_undefined = -21,
    gs_error_undefinedfilename = -22,
    gs_error_undefinedresult = -23,
    gs_error_unmatchedmark = -24,
    gs_error_VMerror = -25,		/* must be the last Level 1 error */

        /* ------ Additional Level 2 errors (also in DPS, ------ */

    gs_error_configurationerror = -26,
    gs_error_undefinedresource = -27,

    gs_error_unregistered = -28,
    gs_error_invalidcontext = -29,
/* invalidid is for the NeXT DPS extension. */
    gs_error_invalidid = -30,

        /* ------ Pseudo-errors used internally ------ */

    gs_error_hit_detected = -99,

    gs_error_Fatal = -100,
/*
 * Internal code for the .quit operator.
 * The real quit code is an integer on the operand stack.
 * gs_interpret returns this only for a .quit with a zero exit code.
 */
    gs_error_Quit = -101,

/*
 * Internal code for a normal exit from the interpreter.
 * Do not use outside of interp.c.
 */
    gs_error_InterpreterExit = -102,

/* Need the remap color error for high level pattern support */
    gs_error_Remap_Color = -103,

/*
 * Internal code to indicate we have underflowed the top block
 * of the e-stack.
 */
    gs_error_ExecStackUnderflow = -104,

/*
 * Internal code for the vmreclaim operator with a positive operand.
 * We need to handle this as an error because otherwise the interpreter
 * won't reload enough of its state when the operator returns.
 */
    gs_error_VMreclaim = -105,

/*
 * Internal code for requesting more input from run_string.
 */
    gs_error_NeedInput = -106,

/*
 * Internal code to all run_string to request that the data is rerun
 * using run_file.
 */
    gs_error_NeedFile = -107,

/*
 * Internal code for a normal exit when usage info is displayed.
 * This allows Window versions of Ghostscript to pause until
 * the message can be read.
 */
    gs_error_Info = -110,

/* A special 'error', like reamp color above. This is used by a subclassing
 * device to indicate that it has fully processed a device method, and parent
 * subclasses should not perform any further action. Currently this is limited
 * to compositor creation.
 */
    gs_error_handled = -111
};

/* We do provide a typedef type for external API use */
typedef enum gs_error_type gs_error_t;

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
