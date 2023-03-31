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

/* Minimum feature size support function definitions for fax/tiff devices */

#ifndef minftrsz_INCLUDED
#  define minftrsz_INCLUDED

#include "std.h"

int min_feature_size_init(gs_memory_t *mem, int min_feature_size,
                          int width, int height, void **min_feature_data);

int min_feature_size_dnit(void *min_feature_data);

int min_feature_size_process(byte *line, void *min_feature_data);

int fax_adjusted_width(int width, int adjust_width);

#endif /* minftrsz_INCLUDED */
