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


/* Reusable stream implementation */

#ifndef zfrsd_INCLUDED
#  define zfrsd_INCLUDED

#include "iostack.h"

/* Make a reusable string stream. */
int
make_rss(i_ctx_t *i_ctx_p, os_ptr op, const byte * data, uint size,
   uint string_space, long offset, long length, bool is_bytestring);

#endif /* zfrsd_INCLUDED */
