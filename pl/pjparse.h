/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pjparse.h */
/* NB the pjl parser and state should be separate. */

/* We present a simplified version of the parser and states for
   demonstration purposes.  It only implements a subset of pjl -
   specifically PDL settable PJL environmant variables.  Clients
   should initialize the pjl parser with pjl_process_init() and
   release it with pjl_process_destroy().  The principle means of
   receiving environment variables is by the function call
   pjl_get_envvar() see below.  The return value of this function call
   will return the appropriate environment value requested.  Note
   there is no mechanism for clients to receive pjl default or factory
   default environment variable.  We assume clients are PDL's and only
   have to reset their state values to the pjl environment.  See
   PJLTRM for mor details */

#ifndef pjparse_INCLUDED
#  define pjparse_INCLUDED

#include "scommon.h"  /* stream_cursor_pead */

/* opaque definition for the parser state */
typedef struct pjl_parser_state_s pjl_parser_state;

/* ---------------- Procedural interface ---------------- */

/* Initialize the PJL parser and state. */
pjl_parser_state *pjl_process_init(P1(gs_memory_t *mem));

/* Destroy an instance of the the PJL parser and state. */
void pjl_process_destroy(P2(pjl_parser_state *pst, gs_memory_t *mem));

/* Process a buffer of PJL commands. */
/* Return 1 if we are no longer in PJL mode. */
int pjl_process(P3(pjl_parser_state *pst, void *pstate,
		   stream_cursor_read *pr));

/* Discard the remainder of a job.  Return true when we reach a UEL. */
/* The input buffer must be at least large enough to hold an entire UEL. */
bool pjl_skip_to_uel(P1(stream_cursor_read *pr));
/* return the current setting of a pjl environment variable.  The
   input parameter should be the exact string used in PJLTRM.
   Sample Usage: 
         char *formlines = pjl_get_envvar(pst, "formlines");
	 if (formlines) {
	     int fl = atoi(formlines);
	     .
	     .
	 }
   Both variables and values are case insensitive.
*/
char *pjl_get_envvar(P2(pjl_parser_state *pst, const char *pjl_var));

/* compare pjl string values.  This is just a case-insensitive string compare */
int pjl_compare(P2(const char *s1, const char *s2));

/* map a pjl symbol set name to a pcl integer */
int pjl_map_pjl_sym_to_pcl_sym(P1(const char *symname));

#endif				/* pjparse_INCLUDED */
