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

/*$RCSfile$ $Revision$ */
/* DataSource procedures */

#include "memory_.h"
#include "gx.h"
#include "gsdsrc.h"
#include "gserrors.h"
#include "stream.h"

/* GC descriptor */
public_st_data_source();
private 
ENUM_PTRS_WITH(data_source_enum_ptrs, gs_data_source_t *psrc)
{
    if (psrc->type == data_source_type_string)
	ENUM_RETURN_CONST_STRING_PTR(gs_data_source_t, data.str);
    else if (psrc->type == data_source_type_stream)
	ENUM_RETURN_PTR(gs_data_source_t, data.strm);
    else			/* bytes or floats */
	ENUM_RETURN_PTR(gs_data_source_t, data.str.data);
}
ENUM_PTRS_END
private RELOC_PTRS_WITH(data_source_reloc_ptrs, gs_data_source_t *psrc)
{
    if (psrc->type == data_source_type_string)
	RELOC_CONST_STRING_PTR(gs_data_source_t, data.str);
    else if (psrc->type == data_source_type_stream)
	RELOC_PTR(gs_data_source_t, data.strm);
    else			/* bytes or floats */
	RELOC_PTR(gs_data_source_t, data.str.data);
}
RELOC_PTRS_END

/* Access data from a string or a byte object. */
/* Does *not* check bounds. */
int
data_source_access_string(const gs_data_source_t * psrc, ulong start,
			  uint length, byte * buf, const byte ** ptr)
{
    const byte *p = psrc->data.str.data + start;

    if (ptr)
	*ptr = p;
    else
	memcpy(buf, p, length);
    return 0;
}
/* access_bytes is identical to access_string, but has a different */
/* GC procedure. */
int
data_source_access_bytes(const gs_data_source_t * psrc, ulong start,
			 uint length, byte * buf, const byte ** ptr)
{
    const byte *p = psrc->data.str.data + start;

    if (ptr)
	*ptr = p;
    else
	memcpy(buf, p, length);
    return 0;
}

/* Access data from a stream. */
/* Returns gs_error_rangecheck if out of bounds. */
int
data_source_access_stream(const gs_data_source_t * psrc, ulong start,
			  uint length, byte * buf, const byte ** ptr)
{
    stream *s = psrc->data.strm;
    const byte *p;

    if (start >= s->position &&
	(p = start - s->position + s->cbuf) + length <=
	s->cursor.r.limit + 1
	) {
	if (ptr)
	    *ptr = p;
	else
	    memcpy(buf, p, length);
    } else {
	uint nread;
	int code = sseek(s, start);

	if (code < 0)
	    return_error(gs_error_rangecheck);
	code = sgets(s, buf, length, &nread);
	if (code < 0)
	    return_error(gs_error_rangecheck);
	if (nread != length)
	    return_error(gs_error_rangecheck);
	if (ptr)
	    *ptr = buf;
    }
    return 0;
}
