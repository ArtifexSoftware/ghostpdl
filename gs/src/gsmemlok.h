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

/* gsmemlok.h */
/* Monitor-locked heap memory allocator */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */

#if !defined(gsmemlok__INCLUDED)
 #define gsmemlok__INCLUDED

 #include "gsmemory.h"
 #include "gxsync.h"

typedef struct gs_memory_locked_s {
	gs_memory_common;			/* interface outside world sees */
	gs_memory_t		*target;	/* allocator to front */
	gx_monitor_t		*monitor;	/* monitor to serialize access to functions */
} gs_memory_locked_t;

/* ---------- Public constructors/destructors ---------- */
int	/* -ve error code or 0 */
gs_memory_locked_construct(P2(
	gs_memory_locked_t	*lmem,		/* allocator to init */
	gs_memory_t		*target		/* allocator to monitor lock */
));
void
gs_memory_locked_destruct(P1(
	gs_memory_locked_t	*lmem		/* allocator to dnit */
));

gs_memory_t *		/* returns target of this allocator */
gs_memory_locked_query_target(P1(
	gs_memory_locked_t	*lmem		/* allocator to query */
));
#endif /*!defined(gsmemlok__INCLUDED)*/

