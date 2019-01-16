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


#include "windows_.h"

int utf8_to_wchar(wchar_t *out, const char *in)
{
    unsigned int i;
    unsigned int len = 1;
    unsigned char c;

    if (out) {
        while (i = *(unsigned char *)in++) {
            if (i < 0x80) {
                *out++ = (wchar_t)i;
                len++;
            } else if ((i & 0xE0) == 0xC0) {
                i &= 0x1F;
                c = (unsigned char)*in++;
                if ((c & 0xC0) != 0x80)
                    return -1;
                i = (i<<6) | (c & 0x3f);
                *out++ = (wchar_t)i;
                len++;
            } else if ((i & 0xF0) == 0xE0) {
                i &= 0xF;
                c = (unsigned char)*in++;
                if ((c & 0xC0) != 0x80)
                    return -1;
                i = (i<<6) | (c & 0x3f);
                c = (unsigned char)*in++;
                if ((c & 0xC0) != 0x80)
                    return -1;
                i = (i<<6) | (c & 0x3f);
                *out++ = (wchar_t)i;
                len++;
            } else {
                return -1;
            }
        }
        *out = 0;
    } else {
        while (i = *(unsigned char *)in++) {
            if (i < 0x80) {
                len++;
            } else if ((i & 0xE0) == 0xC0) {
                in++;
                len++;
            } else if ((i & 0xF0) == 0xE0) {
                in+=2;
                len++;
            } else {
                return -1;
            }
        }
    }
    return len;
}

int wchar_to_utf8(char *out, const wchar_t *in)
{
    unsigned int i;
    unsigned int len = 1;

    if (out) {
        while (i = (unsigned int)*in++) {
            if (i < 0x80) {
                *out++ = (char)i;
                len++;
            } else if (i < 0x800) {
                *out++ = 0xC0 | ( i>> 6        );
                *out++ = 0x80 | ( i      & 0x3F);
                len+=2;
            } else /* if (i < 0x10000) */ {
                *out++ = 0xE0 | ( i>>12        );
                *out++ = 0x80 | ((i>> 6) & 0x3F);
                *out++ = 0x80 | ( i      & 0x3F);
                len+=3;
            }
        }
        *out = 0;
    } else {
        while (i = (unsigned int)*in++) {
            if (i < 0x80) {
                len++;
            } else if (i < 0x800) {
                len += 2;
            } else /* if (i < 0x10000) */ {
                len += 3;
            }
        }
    }
    return len;
}
