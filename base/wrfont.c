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
Support functions to serialize fonts as PostScript code that can
then be passed to FreeType via the FAPI FreeType bridge.
Started by Graham Asher, 9th August 2002.
*/

#include "wrfont.h"
#include "stdio_.h"
#include "assert_.h"

#define EEXEC_KEY 55665
#define EEXEC_FACTOR 52845
#define EEXEC_OFFSET 22719

void
WRF_init(WRF_output * a_output, unsigned char *a_buffer, long a_buffer_size)
{
    a_output->m_pos = a_buffer;
    a_output->m_limit = a_buffer_size;
    a_output->m_count = 0;
    a_output->m_encrypt = false;
    a_output->m_key = EEXEC_KEY;
}

void
WRF_wbyte(const gs_memory_t *memory, WRF_output * a_output, unsigned char a_byte)
{
    if (a_output->m_count < a_output->m_limit && a_output->m_pos != NULL) {
        if (a_output->m_encrypt) {
            a_byte ^= (a_output->m_key >> 8);
            a_output->m_key =
                (unsigned short)((a_output->m_key + a_byte) * EEXEC_FACTOR +
                                 EEXEC_OFFSET);
        }
        *a_output->m_pos++ = a_byte;
    }
    a_output->m_count++;
}

void
WRF_wtext(const gs_memory_t *memory, WRF_output * a_output, const unsigned char *a_string, long a_length)
{
    while (a_length > 0) {
        WRF_wbyte(memory, a_output, *a_string++);
        a_length--;
    }
}

void
WRF_wstring(const gs_memory_t *memory, WRF_output * a_output, const char *a_string)
{
    while (*a_string)
        WRF_wbyte(memory, a_output, *a_string++);
}

void
WRF_wfloat(const gs_memory_t *memory, WRF_output * a_output, double a_float)
{
    char buffer[32];
    int l;

    l = gs_snprintf(buffer, sizeof (buffer), "%f", a_float);
    if (l > sizeof (buffer)) {
        emprintf(memory, "Warning: Font real number value truncated\n");
    }
    WRF_wstring(memory, a_output, buffer);
}

void
WRF_wint(const gs_memory_t *memory, WRF_output * a_output, long a_int)
{
    char buffer[32];
    int l;

    l = gs_snprintf(buffer, sizeof(buffer), "%ld", a_int);
    if (l > sizeof (buffer)) {
        emprintf(memory, "Warning: Font integer number value truncated\n");
    }
    WRF_wstring(memory, a_output, buffer);
}
