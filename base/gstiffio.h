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

#ifndef gstiffio_INCLUDED
#  define gstiffio_INCLUDED

#include <tiffio.h>
#include <tiffvers.h>
#include "gdevprn.h"

TIFF *
tiff_from_filep(gx_device_printer *dev,  const char *name, gp_file *filep, int big_endian, bool usebigtiff);
void tiff_set_handlers (void);
int tiff_filename_from_tiff(TIFF *t, char **name);

#endif /* gstiffio_INCLUDED */
