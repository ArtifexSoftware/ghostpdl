/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plvalue.h */
/* Accessors for big-endian multi-byte values */

#ifndef plvalue_INCLUDED
#  define plvalue_INCLUDED

int pl_get_int16(P1(const byte *bptr));
uint pl_get_uint16(P1(const byte *bptr));
long pl_get_int32(P1(const byte *bptr));
ulong pl_get_uint32(P1(const byte *bptr));

#endif				/* plvalue_INCLUDED */
