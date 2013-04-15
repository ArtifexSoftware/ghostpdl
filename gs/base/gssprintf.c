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


/* Simple interface to a sprintf/sscanf without locale */
#include "stdio_.h"             /* includes std.h */
#include "gssprintf.h"
#include "trio.h"
#include "triop.h"
#include "triodef.h"
#include "trionan.h"
#include "triostr.h"

int
gs_vsnprintf(char *buf, int n, const char *format, va_list ap)
{
    return(trio_vsnprintf(buf, n, format, ap));
}

int
gs_snprintf(char *buf, int n, const char *format, ...)
{
    int len;
    va_list ap;

    va_start(ap, format);
    len = trio_vsnprintf(buf, n, format, ap);
    va_end(ap);
    return len;
}

int
gs_vsprintf(char *buf, const char *format, va_list ap)
{
    return(trio_vsprintf(buf, format, ap));
}

int
gs_sprintf(char *buf, const char *format, ...)
{
    int len;
    va_list ap;

    va_start(ap, format);
    len = trio_vsprintf(buf, format, ap);
    va_end(ap);

    return(len);
}
