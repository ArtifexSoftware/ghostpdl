/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gsargs.h */
/* Command line argument list management */

#ifndef gsargs_INCLUDED
#  define gsargs_INCLUDED

/*
 * We need to handle recursion into @-files.
 * The following structures keep track of the state.
 * Defining a maximum argument length and a maximum nesting depth
 * decreases generality, but eliminates the need for dynamic allocation.
 */
#define arg_str_max 512
#define arg_depth_max 10
typedef struct arg_source_s {
	bool is_file;
	union _u {
	  const char *str;
	  FILE *file;
	} u;
} arg_source;
typedef struct arg_list_s {
	bool expand_ats;	/* if true, expand @-files */
	FILE *(*arg_fopen)(P2(const char *fname, void *fopen_data));
	void *fopen_data;
	const char **argp;
	int argn;
	int depth;		/* depth of @-files */
	char cstr[arg_str_max + 1];
	arg_source sources[arg_depth_max];
} arg_list;

/* Initialize an arg list. */
void arg_init(P5(arg_list *pal, const char **argv, int argc,
		 FILE *(*arg_fopen)(P2(const char *fname, void *fopen_data)),
		 void *fopen_data));

/* Push a string onto an arg list. */
/* This may also be used (once) to "unread" the last argument. */
void arg_push_string(P2(arg_list *pal, const char *str));

/* Clean up an arg list before exiting. */
void arg_finit(P1(arg_list *pal));

/* Get the next arg from a list. */
/* Note that these are not copied to the heap. */
const char *arg_next(P1(arg_list *pal));

/* Copy an argument string to the heap. */
char *arg_copy(P2(const char *str, gs_memory_t *mem));

#endif					/* gsargs_INCLUDED */
