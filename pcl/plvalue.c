/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
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
pl_get_int16(const byte *bptr)
{	return get_int16(bptr);
}

uint
pl_get_uint16(const byte *bptr)
{	return get_uint16(bptr);
}

long
pl_get_int32(const byte *bptr)
{	return ((long)get_int16(bptr) << 16) | get_uint16(bptr + 2);
}

ulong
pl_get_uint32(const byte *bptr)
{	return ((ulong)get_uint16(bptr) << 16) | get_uint16(bptr + 2);
}
