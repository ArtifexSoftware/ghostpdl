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


/* Dummy set_user_params for Level 1 systems */
#include "ghost.h"
#include "icontext.h"		/* for set_user_params prototype */

int
set_user_params(i_ctx_t *i_ctx_p, const ref *paramdict)
{
    return 0;
}
