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
/* Error return macros */

#ifndef gserror_INCLUDED
#  define gserror_INCLUDED

int gs_log_error(const gs_memory_t *mem, int, const char *, int);
#ifndef DEBUG
#  define gs_log_error(mem, err, file, line) (err)
#endif
#define gs_note_error(mem, err) gs_log_error(mem, err, __FILE__, __LINE__)
#define return_error(mem, err) return gs_note_error(mem, err)



/*
 * Error reporting macros
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

int gs_throw_imp(const char *func, const char *file, int line,
        int op, int code, const char *fmt, ...) __printflike(6, 7);



/* Use throw at origin of error 
*/
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


/* just in case you don't know 0 means no error
 * other return codes are errors.
 */
#define gs_okay 0

#endif /* gserror_INCLUDED */
