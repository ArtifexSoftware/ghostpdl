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


/* Export zopen_file() for zfrsd.c */

#ifndef zfile_INCLUDED
#  define zfile_INCLUDED

#include "iref.h"
#include "gsfname.h"

int zopen_file(i_ctx_t *i_ctx_p, const gs_parsed_file_name_t *pfn,
           const char *file_access, stream **ps, gs_memory_t *mem);

/* z_check_file_permissions: a callback (via mem->gs_lib_ctx->client_check_file_permission)
 * to allow applying the above permissions checks when opening file(s) from
 * the graphics library
 */
int
z_check_file_permissions(gs_memory_t *mem, const char *fname,
                                 const int len, const char *permission);
#endif
