/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* ifilter.h */
/* Filter creation utilities for Ghostscript */
/* Requires oper.h, stream.h, strimpl.h */
#include "istream.h"
#include "ivmspace.h"

/*
 * Define the utility procedures for creating filters.
 * Note that a filter will be allocated in global VM iff the source/target
 * and all relevant parameters (if any) are in global VM.
 */
int filter_read(P5(
	/* Operand stack pointer that was passed to zfxxx operator */
		   os_ptr op,
	/* # of parameters to pop off o-stack, */
	/* not counting the source/target */
		   int npop,
	/* Template for stream */
		   const stream_template *template,
	/* Initialized s_xxx_state, 0 if no separate state */
		   stream_state *st,
	/* Max of space attributes of all parameters referenced by */
	/* the state, 0 if no such parameters */
		   uint space
		   ));
int filter_write(P5(os_ptr op, int npop, const stream_template *template,
		    stream_state *st, uint space));
/*
 * Define a simplified interface for streams with no parameters or state.
 * These procedures also pop the top o-stack element if it is a dictionary.
 */
int filter_read_simple(P2(os_ptr op, const stream_template *template));
int filter_write_simple(P2(os_ptr op, const stream_template *template));

/* Mark a filter stream as temporary. */
/* See stream.h for the meaning of is_temp. */
void filter_mark_temp(P2(const ref *fop, int is_temp));

/*
 * Define the state of a procedure-based stream.
 * Note that procedure-based streams are defined at the Ghostscript
 * interpreter level, unlike all other stream types which depend only
 * on the stream package and the memory manager.
 */
typedef struct stream_proc_state_s {
	stream_state_common;
	bool eof;
	uint index;		/* current index within data */
	ref proc;
	ref data;
} stream_proc_state;
#define private_st_stream_proc_state() /* in zfproc.c */\
  gs_private_st_complex_only(st_sproc_state, stream_proc_state,\
    "procedure stream state", sproc_clear_marks, sproc_enum_ptrs, sproc_reloc_ptrs, 0)
