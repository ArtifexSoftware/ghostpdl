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

/* pgmisc.h */
/* definitions for HP-GL/2 lost mode and error handling routines. */

#ifndef pgmisc_INCLUDED
#  define pgmisc_INCLUDED

void hpgl_set_lost_mode(P2(hpgl_state_t *pgls, hpgl_lost_mode_t lost_mode));
/* get the current setting of the edge pen set by CF, NB this should
   be in a different header file */
int32 hpgl_get_character_edge_pen(P1(hpgl_state_t *pgls));

/* macro to see if we are in lost mode */
#define hpgl_lost (pgls->g.lost_mode == hpgl_lost_mode_entered)

/* a macro that calls a function and returns an error code if the code
   returned is less than 0.  Most of the hpgl and gs functions return
   if the calling function is less than 0 so this avoids cluttering up
   the code with the if statement and debug code. */

#ifdef DEBUG

void hpgl_error(P0());
int hpgl_print_error(P4(const char *function, const char *file, int line, int code));

# ifdef __GNUC__
#  define hpgl_call_note_error(code)\
     hpgl_print_error(__FUNCTION__, __FILE__, __LINE__, code)
# else
#  define hpgl_call_note_error(code)\
     hpgl_print_error((const char *)0, __FILE__, __LINE__, code)
# endif

#else				/* !DEBUG */

#define hpgl_call_note_error(code) (code)

#endif

/* We use the do ... while(0) in order to make the call be a statement */
/* syntactically. */

#define hpgl_call_and_check(call, if_check_else)\
do {						\
  int code; 					\
  if ((code = (call)) < 0)			\
    { if_check_else()				\
        return hpgl_call_note_error(code);	\
    }						\
} while (0)

/* Ordinary function calls */

#define hpgl_no_check() /* */

#define hpgl_call(call)\
  hpgl_call_and_check(call, hpgl_no_check)

/* Function calls that can set LOST mode */

#define hpgl_limitcheck_set_lost()\
  if ( code == gs_error_limitcheck )\
    hpgl_set_lost_mode(pgls, hpgl_lost_mode_entered);\
  else

#define hpgl_call_check_lost(call)\
  hpgl_call_and_check(call, hpgl_limitcheck_set_lost)

#endif                       /* pgmisc_INCLUDED */
