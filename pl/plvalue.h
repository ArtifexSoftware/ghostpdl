/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* plvalue.h */
/* Accessors for big-endian multi-byte values */

#ifndef plvalue_INCLUDED
#  define plvalue_INCLUDED

int pl_get_int16(const byte *bptr);
uint pl_get_uint16(const byte *bptr);
long pl_get_int32(const byte *bptr);
ulong pl_get_uint32(const byte *bptr);

#endif				/* plvalue_INCLUDED */
