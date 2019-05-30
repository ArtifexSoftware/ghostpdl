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


/* Entry points to the TIFF/fax writing driver */

#ifndef gdevtfax_INCLUDED
#  define gdevtfax_INCLUDED

#include "gxclist.h"
#include "scfx.h"

int gdev_fax_print_page_stripped(gx_device_printer *pdev, gp_file *prn_stream,
                                 stream_CFE_state *ss, long rows_per_strip);

#endif /* gdevtfax_INCLUDED */
