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

/* gsmemraw.h Abstract raw-memory allocator */

/* Initial version 02/03/1998 by John Desrosiers (soho@crl.com) */

#if !defined(gsmemraw__INCLUDED)
 #define gsmemraw__INCLUDED

typedef struct gs_memraw_s gs_memraw;

/* Procedure vector for accessing raw memory allocator */
typedef struct gs_memraw_procs_s {
	/* There is no constructor here. Constructors for subclasses must */
	/* be called statically: each constructor must *first* call its */
	/* superclass' constructor, then do its own init. */

	/* Destuctor: subclasses destruct their own stuff first, then call */
	/* superclass' destructor. */
#define mem_proc_destructor(proc)\
  void proc(P1(gs_memraw *))
	mem_proc_destructor((*destructor));

	/* Query freespace */
#define mem_proc_query_freespace(proc)\
  long proc (P1(gs_memraw *))
	mem_proc_query_freespace((*query_freespace));

	/* malloc */
#define mem_proc_malloc(proc)\
  void *proc (P2(gs_memraw *, unsigned int))
	mem_proc_malloc((*malloc));

	/* realloc */
#define mem_proc_realloc(proc)\
  void *proc (P3(gs_memraw *, void *, unsigned int))
	mem_proc_realloc((*realloc));

	/* free */
#define mem_proc_free(proc)\
  void proc(P2(gs_memraw *, void*))
	mem_proc_free((*free));
} gs_memraw_procs;


/*
 * An abstract raw-memory allocator instance.  Subclasses may have state as well.
 */
#define gs_memraw_common\
	gs_memraw_procs procs

struct gs_memraw_s {
	gs_memraw_common;
};

#endif /*!defined(gsmemraw__INCLUDED)*/

