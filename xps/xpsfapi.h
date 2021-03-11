/* Copyright (C) 2001-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  1305 Grant Avenue - Suite 200,
   Novato, CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* Font API support  */

#ifndef xpsfapi_h_INCLUDED
#define xpsfapi_h_INCLUDED

int
xps_fapi_rebuildfont(gs_font *pfont);

int
xps_fapi_passfont(gs_font *pfont, char *fapi_request, char *file_name,
                  byte *font_data, int font_data_len);

#endif
