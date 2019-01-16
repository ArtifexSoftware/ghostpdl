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


/* Reusable stream implementation */

#ifndef zfrsd_INCLUDED
#  define zfrsd_INCLUDED

#include "iostack.h"

/* Make a reusable string stream. */
int
make_rss(i_ctx_t *i_ctx_p, os_ptr op, const byte * data, uint size,
   uint string_space, long offset, long length, bool is_bytestring);

#endif /* zfrsd_INCLUDED */
