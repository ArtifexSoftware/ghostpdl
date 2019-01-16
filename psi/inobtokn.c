/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Dummy scan_binary_token for Level 1 systems */
#include "ghost.h"
#include "ierrors.h"
#include "stream.h"
#include "iscan.h"
#include "iscanbin.h"

int
scan_binary_token(i_ctx_t *i_ctx_p, ref *pref, scanner_state *pstate)
{
    return_error(gs_error_unregistered);
}
