/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pjparse.h */
/* Interface and definitions for PJL parser */

#ifndef pjparse_INCLUDED
#  define pjparse_INCLUDED

/* ------ Parser state ------ */

typedef struct pjl_parser_state_s {
  char line[81];		/* buffered command line */
  int pos;			/* current position in line */
} pjl_parser_state_t;
#define pjl_parser_init_inline(pst)\
  ((pst)->pos = 0)

/* ---------------- Procedural interface ---------------- */

/* Initialize the PJL parser. */
void pjl_process_init(P1(pjl_parser_state_t *pst));

/* Process a buffer of PJL commands. */
/* We haven't defined what the state pointer should be. */
/* Return 1 if we are no longer in PJL mode. */
int pjl_process(P3(pjl_parser_state_t *pst, void *pstate,
		   stream_cursor_read *pr));

/* Discard the remainder of a job.  Return true when we reach a UEL. */
/* The input buffer must be at least large enough to hold an entire UEL. */
bool pjl_skip_to_uel(P1(stream_cursor_read *pr));

#endif				/* pjparse_INCLUDED */
