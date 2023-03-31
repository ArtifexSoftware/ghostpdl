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


/* plvalue.h */
/* Accessors for big-endian multi-byte values */

#ifndef plvalue_INCLUDED
#  define plvalue_INCLUDED

int pl_get_int16(const byte * bptr);

uint pl_get_uint16(const byte * bptr);

long pl_get_int32(const byte * bptr);

ulong pl_get_uint32(const byte * bptr);

#endif /* plvalue_INCLUDED */
