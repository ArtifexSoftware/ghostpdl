/* Copyright (C) 2001-2026 Artifex Software, Inc.
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


/* Export zopen_file() for zfrsd.c */

#ifndef zfile_INCLUDED
#  define zfile_INCLUDED

#include "iref.h"
#include "gsfname.h"

int zopen_file(i_ctx_t *i_ctx_p, const gs_parsed_file_name_t *pfn,
           const char *file_access, stream **ps, gs_memory_t *mem);

#endif
