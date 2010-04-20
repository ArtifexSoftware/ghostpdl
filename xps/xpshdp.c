/* Copyright (C) 2006-2010 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* XPS interpreter - HD-Photo support */

#include "ghostxps.h"

/* HD-Photo decoder should go here. */

int
xps_decode_hdphoto(gs_memory_t *mem, byte *buf, int len, xps_image_t *image)
{
    return gs_throw(-1, "HD-Photo codec is not available");
}

