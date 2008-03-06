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

/* pxvalue.c */
/* Value accessing for PCL XL */

#include "std.h"
#include "gsmemory.h"
#include "pxvalue.h"

/* Get numeric values from the input or an array. */
uint
uint16at(const byte *p, bool big_endian)
{	return (big_endian ? (p[0] << 8) + p[1] : (p[1] << 8) + p[0]);
}
int
sint16at(const byte *p, bool big_endian)
{	return ((int)uint16at(p, big_endian) ^ 0x8000) - 0x8000;
}
int32_t
uint32at(const byte *p, bool big_endian)
{	return
	  (big_endian ?
	   ((int32_t)((p[0] << 8) + p[1]) << 16) + (p[2] << 8) + p[3] :
	   ((int32_t)((p[3] << 8) + p[2]) << 16) + (p[1] << 8) + p[0]);
}
int32_t
sint32at(const byte *p, bool big_endian)
{	return (uint32at(p, big_endian) ^ 0x80000000) - 0x80000000;
}
real
real32at(const byte *p, bool big_endian)
{	    
    union {
        float   f;
        int32_t d;
    } df;

    df.d = uint32at(p, big_endian);
    return (real)df.f;
}

/* Get an element from an array. */
/* The caller does all index and type checking. */
int32_t
integer_elt(const px_value_t *pav, uint index)
{	px_data_type_t type = pav->type;
	const byte *base = pav->value.array.data;
	bool big_endian;

	if ( type & pxd_ubyte )
	  return base[index];
	big_endian = (type & pxd_big_endian) != 0;
	if ( type & pxd_uint16 )
	  return uint16at(base + (index << 1), big_endian);
	else if ( type & pxd_sint16 )
	  return sint16at(base + (index << 1), big_endian);
	else if ( type & pxd_uint32 )
	  return uint32at(base + (index << 2), big_endian);
	else /* ( type & pxd_sint32 ) */
	  return sint32at(base + (index << 2), big_endian);
}
real
real_elt(const px_value_t *pav, uint index)
{	return
	  (pav->type & pxd_real32 ?
	   real32at(pav->value.array.data + (index << 2),
		    (pav->type & pxd_big_endian) != 0) :
	   (real)integer_elt(pav, index));
}
