/*
WRITE_T2.H
Copyright (C) 2002 Artifex, Inc.

Header for functions to serialize a type 2 (CFF) font so that it can
then be passed to FreeType via the FAPI FreeType bridge.
Started by Graham Asher, 9th August 2002.

This software is provided as is with no warranty, either express or implied.

This software is distributed under license and may not be copied,
modified or distributed except as expressly authorized under the terms
of the license contained in the file LICENSE in this distribution.

For more information about licensing, please refer to
http://www.ghostscript.com/licensing/. For information on
commercial licensing, go to http://www.artifex.com/licensing/ or
contact Artifex Software, Inc., 101 Lucas Valley Road #110,
San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

#ifndef write_t2_INCLUDED
#define write_t2_INCLUDED

#include "ifapi.h"

long FF_serialize_type2_font(FAPI_font* a_fapi_font,unsigned char* a_buffer,long a_buffer_size);

#endif
