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


/* Graphical trace server for Windows */

/*  This module use Win32-specific API.
    For 16-bit Windows many of functions compile to stubs.
*/

/*  fixme : Restoring image on WM_PAINT is NOT implemented yet.
*/

#define STRICT
#include <windows.h>
#include <math.h>
#include "dwimg.h"

static COLORREF WindowsColor(unsigned long c)
{   /*  This body uses a Windows specific macro RGB, which is being
        redefined in GS include files. Please include them after
        this definition.
    */
    return RGB(c >> 16, (c >> 8) & 255, c & 255);
}

#include "gscdefs.h"
#include "stdpre.h"
#include "gsdll.h"
#include "dwtrace.h"

#ifdef __WIN32__
#    define SET_CALLBACK(I,a) I.a = dw_gt_##a
#else
#    define SET_CALLBACK(I,a) I.a = 0
#endif

