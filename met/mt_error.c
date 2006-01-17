/* Portions Copyright (C) 2005, 2006 Artifex Software Inc.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id:*/
#include "std.h"
#include "gsmemory.h"
#include "gserrors.h"

#include "stdarg.h"

#include "mt_error.h"

int mt_throw_imp(const char *func, const char *file, int line, int op, int code, const char *fmt, ...)
{
    char msg[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    msg[sizeof(msg) - 1] = 0;
    va_end(ap);

    if (!gs_debug_c('#')) {
	; /* NB: gs_log_errors  
	   * we could disable these printfs, and probably will when, 
	   * the code becomes more stable: 
	   * return code;
	   */
    }
       

    /* throw */
    if (op == 0)
	errprintf(NULL, "- %s:%d: %s(%d): %s\n", file, line, func, code, msg);

    /* rethrow */
    if (op == 1)
	errprintf(NULL, "| %s:%d: %s(): %s\n", file, line, func, msg);

    /* catch */
    if (op == 2)
	errprintf(NULL, "\\ %s:%d: %s(): %s\n", file, line, func, msg);

    /* warn */
    if (op == 3)
	errprintf(NULL, "  %s:%d: %s(): %s\n", file, line, func, msg);

    return code;
}

const char *gs_errstr(int code)
{
    switch (code) {
    default:
    case gs_error_unknownerror: return "unknownerror";
    case gs_error_interrupt: return "interrupt";
    case gs_error_invalidaccess: return "invalidaccess";
    case gs_error_invalidfileaccess: return "invalidfileaccess";
    case gs_error_invalidfont: return "invalidfont";
    case gs_error_ioerror: return "ioerror";
    case gs_error_limitcheck: return "limitcheck";
    case gs_error_nocurrentpoint: return "nocurrentpoint";
    case gs_error_rangecheck: return "rangecheck";
    case gs_error_typecheck: return "typecheck";
    case gs_error_undefined: return "undefined";
    case gs_error_undefinedfilename: return "undefinedfilename";
    case gs_error_undefinedresult: return "undefinedresult";
    case gs_error_VMerror: return "vmerror";
    case gs_error_unregistered: return "unregistered";
    case gs_error_hit_detected: return "hit_detected";
    case gs_error_Fatal: return "Fatal";
    }
}

