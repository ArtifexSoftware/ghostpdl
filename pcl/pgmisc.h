/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgmisc.h */
/* definitions for HP-GL/2 lost mode and error handling routines. */

#ifndef pgmisc_INCLUDED
#  define pgmisc_INCLUDED

extern void hpgl_set_lost_mode(P2(hpgl_state_t *pgls, hpgl_lost_mode_t lost_mode));

/* macro to see if we are in lost mode */
#define hpgl_lost (pgls->g.lost_mode == hpgl_lost_mode_entered)

/* a macro that calls a function and returns an error code if the code
   returned is less than 0.  Most of the hpgl and gs functions return
   if the calling function is less than 0 so this avoids cluttering up
   the code with the if statement and debug code. */

#ifdef DEBUG

extern void hpgl_error(P0());

# ifdef __GNUC__
#  define hpgl_call_note_error()\
     dprintf4("hpgl call failed\n\tcalled from: %s\n\tfile: %s\n\tline: %d\n\terror code: %d\n",\
	      __FUNCTION__, __FILE__, __LINE__, code);\
     hpgl_error();
# else
#  define hpgl_call_note_error()\
     dprintf3("hpgl call failed\n\tcalled from:\n\tfile: %s\n\tline: %d\n\terror code: %d\n",\
	      __FILE__, __LINE__, code);\
     hpgl_error();
# endif

#else				/* !DEBUG */

#define hpgl_call_note_error() /* */

#endif

/* We use the do ... while(0) in order to make the call be a statement */
/* syntactically. */

#define hpgl_call_and_check(call, if_check_else)\
do {						\
  int code; 					\
  if ((code = (call)) < 0)			\
    { if_check_else()				\
        { hpgl_call_note_error();		\
	  return(code);				\
	}					\
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
