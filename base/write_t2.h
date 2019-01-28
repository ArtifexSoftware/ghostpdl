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


/*
Header for functions to serialize a type 2 (CFF) font so that it can
then be passed to FreeType via the FAPI FreeType bridge.
Started by Graham Asher, 9th August 2002.
*/

#ifndef write_t2_INCLUDED
#define write_t2_INCLUDED

#include "gxfapi.h"

long gs_fapi_serialize_type2_font(gs_fapi_font * a_fapi_font,
                                  unsigned char *a_buffer,
                                  long a_buffer_size);

#endif
