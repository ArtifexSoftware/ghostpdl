/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gsparams.h */
/* Serializer/expander for gs_parm_list's */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */

#include "gsparam.h"

/* -------------------------------------------------------------------------- */
int	/* ret -ve err, else # bytes needed to represent param list, whether */
	/* or not it actually fit into buffer. List was successully */
	/* serialized only if if this # is <= supplied buf size. */
gs_param_list_serialize(P3(
	gs_param_list	*list,		/* root of list to serialize */
					/* list MUST BE IN READ MODE */	
	byte		*buf,		/* destination buffer (can be 0) */
	int		buf_sizeof	/* # bytes available in buf (can be 0) */
));

/* Expand a buffer into a gs_param_list (including sub-dicts) */
int	/* ret -ve err, +ve # of chars read from buffer */
gs_param_list_unserialize(P2(
	gs_param_list	*list,		/* root of list to expand to */
					/* list MUST BE IN WRITE MODE */	
	const byte		*buf		/* source buffer, MUST BE void* aligned */
));

