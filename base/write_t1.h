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


/*
Header for functions to serialize a type 1 font as PostScript code that can
then be passed to FreeType via the FAPI FreeType bridge.
Started by Graham Asher, 26th July 2002.
*/

#ifndef write_t1_INCLUDED
#define write_t1_INCLUDED

#include "gxfapi.h"

long gs_fapi_serialize_type1_font(gs_fapi_font * a_fapi_font,
                                  unsigned char *a_buffer,
                                  long a_buffer_size);
long gs_fapi_serialize_type1_font_complete(gs_fapi_font * a_fapi_font,
                                           unsigned char *a_buffer,
                                           long a_buffer_size);

#endif
