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


/* stdio back door */

#ifndef gxstdio_INCLUDED
#  define gxstdio_INCLUDED

/*
 * The library and interpreter never use stdin/out/err directly, but
 * for the moment we have to keep them around for contributed drivers,
 * some of which write to stdout or stderr.
 */
#include "gsio.h"

#undef stdin
#define stdin gs_stdin
#undef stdout
#define stdout gs_stdout
#undef stderr
#define stderr gs_stderr
#undef fgetchar

#endif /* gxstdio_INCLUDED */
