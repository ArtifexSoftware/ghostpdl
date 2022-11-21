/* Copyright (C) 2001-2022 Artifex Software, Inc.
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

#ifndef GP_UTF8_H
#define GP_UTF8_H

int gp_utf8_to_uint16(unsigned short *out, const char *in);
int gp_uint16_to_utf8(char *out, const unsigned short *in);

#endif
