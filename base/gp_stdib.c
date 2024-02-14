/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Read stdin on platforms that support unbuffered read */
/* and require the user to restart interrupted system calls. */
/* We also want unbuffered for console input and pipes. */

#include "stdio_.h"
#include "time_.h"
#include "unistd_.h"
#include "errno_.h"
#include "gx.h"
#include "gp.h"

/* Read bytes from stdin, unbuffered if possible. */
int gp_stdin_read(char *buf, int len, int interactive, FILE *f)
{
    int code;

    /* Retry if interrupted, bug 706858. */
    do {
        code = read(fileno(f), buf, len);
    } while (code < 0 && errno == EINTR);
    return code;
}
