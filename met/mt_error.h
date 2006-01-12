/* Portions Copyright (C) 2005, 2006 Artifex Software Inc.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id:*/

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

int mt_throw_imp(const char *func, const char *file, int line,
        int op, int code, const char *fmt, ...) __printflike(6, 7);



/* Use throw at origin of error 
*/
#define mt_throw(code, fmt) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt)
#define mt_throw1(code, fmt, arg1) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1)
#define mt_throw2(code, fmt, arg1, arg2) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2)
#define mt_throw3(code, fmt, arg1, arg2, arg3) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3)
#define mt_throw4(code, fmt, arg1, arg2, arg3, arg4) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4)
#define mt_throw5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5)
#define mt_throw6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define mt_throw7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define mt_throw8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define mt_throw9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 0, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)


/* Bubble the code up the stack 
*/
#define mt_rethrow(code, fmt) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt)
#define mt_rethrow1(code, fmt, arg1) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1)
#define mt_rethrow2(code, fmt, arg1, arg2) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2)
#define mt_rethrow3(code, fmt, arg1, arg2, arg3) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3)
#define mt_rethrow4(code, fmt, arg1, arg2, arg3, arg4) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4)
#define mt_rethrow5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5)
#define mt_rethrow6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define mt_rethrow7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define mt_rethrow8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define mt_rethrow9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 1, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)



/* This will cause trouble, as it implies you are fixing an error
 * the system will spew messages 
 */
#define mt_catch(code, fmt) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt)
#define mt_catch1(code, fmt, arg1) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1)
#define mt_catch2(code, fmt, arg1, arg2) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2)
#define mt_catch3(code, fmt, arg1, arg2, arg3) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3)
#define mt_catch4(code, fmt, arg1, arg2, arg3, arg4) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4)
#define mt_catch5(code, fmt, arg1, arg2, arg3, arg4, arg5) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5)
#define mt_catch6(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define mt_catch7(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define mt_catch8(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define mt_catch9(code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    mt_throw_imp(__func__, __FILE__, __LINE__, 2, code, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)


/* mt_warn is a printf
 */
#define mt_warn(fmt) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt)
#define mt_warn1(fmt, arg1) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1)
#define mt_warn2(fmt, arg1, arg2) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2)
#define mt_warn3(fmt, arg1, arg2, arg3) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3)
#define mt_warn4(fmt, arg1, arg2, arg3, arg4) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4)
#define mt_warn5(fmt, arg1, arg2, arg3, arg4, arg5) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5)
#define mt_warn6(fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define mt_warn7(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define mt_warn8(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define mt_warn9(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
    (void)mt_throw_imp(__func__, __FILE__, __LINE__, 3, 0, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)


/* just in case you don't know 0 means no error
 * other return codes are errors.
 */
#define mt_okay 0

