/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* spcxd.c */
/* PCXDecode filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "spcxx.h"

/* ------ PCXDecode ------ */

/* Refill the buffer */
private int
s_PCXD_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
	const byte *rlimit = pr->limit;
	byte *wlimit = pw->limit;
	int status = 0;

	while ( p < rlimit )
	{	int b = *++p;
		if ( b < 0xc0 )
		{	if ( q >= wlimit )
			{	--p;
				status = 1;
				break;
			}
			*++q = b;
		}
		else if ( p >= rlimit )
		  {	--p;
			break;
		  }
		else if ( (b -= 0xc0) > wlimit - q )
		{	--p;
			status = 1;
			break;
		}
		else
		{	memset(q + 1, *++p, b);
			q += b;
		}
	}
	pr->ptr = p;
	pw->ptr = q;
	return status;
}

#undef ss

/* Stream template */
const stream_template s_PCXD_template =
{	&st_stream_state, NULL, s_PCXD_process, 2, 63
};
