/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* plvalue.h */
/* Accessors for big-endian multi-byte values */

#ifndef plvalue_INCLUDED
#  define plvalue_INCLUDED

int pl_get_int16(const byte * bptr);

uint pl_get_uint16(const byte * bptr);

long pl_get_int32(const byte * bptr);

ulong pl_get_uint32(const byte * bptr);

#endif /* plvalue_INCLUDED */
