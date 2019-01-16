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


/* plvalue.c */
/* Accessors for big-endian multi-byte values */

#include "std.h"
#include "plvalue.h"

#define get_uint16(bptr)\
  (((bptr)[0] << 8) | (bptr)[1])
#define get_int16(bptr)\
  (((int)get_uint16(bptr) ^ 0x8000) - 0x8000)

int
pl_get_int16(const byte * bptr)
{
    return get_int16(bptr);
}

uint
pl_get_uint16(const byte * bptr)
{
    return get_uint16(bptr);
}

long
pl_get_int32(const byte * bptr)
{
    return ((long)get_int16(bptr) << 16) | get_uint16(bptr + 2);
}

ulong
pl_get_uint32(const byte * bptr)
{
    return ((ulong) get_uint16(bptr) << 16) | get_uint16(bptr + 2);
}
