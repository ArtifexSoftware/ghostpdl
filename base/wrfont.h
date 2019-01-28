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
Header for support functions to serialize fonts as PostScript code that can
then be passed to FreeType via the FAPI FreeType bridge.
Started by Graham Asher, 9th August 2002.
*/

#ifndef wrfont_INCLUDED
#define wrfont_INCLUDED

#include "stdpre.h"
#include "std.h"

typedef struct WRF_output_
{
    unsigned char *m_pos;
    long m_limit;
    long m_count;
    bool m_encrypt;
    unsigned short m_key;
} WRF_output;

void WRF_init(WRF_output * a_output, unsigned char *a_buffer,
              long a_buffer_size);
void WRF_wbyte(const gs_memory_t *memory, WRF_output * a_output, unsigned char a_byte);
void WRF_wtext(const gs_memory_t *memory, WRF_output * a_output, const unsigned char *a_string,
               long a_length);
void WRF_wstring(const gs_memory_t *memory, WRF_output * a_output, const char *a_string);
void WRF_wfloat(const gs_memory_t *memory, WRF_output * a_output, double a_float);
void WRF_wint(const gs_memory_t *memory, WRF_output * a_output, long a_int);

#endif
