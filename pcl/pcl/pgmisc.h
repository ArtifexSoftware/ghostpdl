/* Copyright (C) 2001-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pgmisc.h */
/* definitions for HP-GL/2 lost mode and error handling routines. */

#ifndef pgmisc_INCLUDED
#  define pgmisc_INCLUDED

void hpgl_set_lost_mode(hpgl_state_t * pgls, hpgl_lost_mode_t lost_mode);

/* get the current setting of the edge pen set by CF, NB this should
   be in a different header file */
int32 hpgl_get_character_edge_pen(hpgl_state_t * pgls);

/* macro to see if we are in lost mode */
#define hpgl_lost (pgls->g.lost_mode == hpgl_lost_mode_entered)

/* a macro that calls a function and returns an error code if the code
   returned is less than 0.  Most of the hpgl and gs functions return
   if the calling function is less than 0 so this avoids cluttering up
   the code with the if statement and debug code. */

#ifdef DEBUG

void hpgl_error(void);

int hpgl_print_error(const gs_memory_t * mem,
                     const char *function, const char *file, int line,
                     int code);

# ifdef __GNUC__
#  define hpgl_call_note_error(mem, code)\
     hpgl_print_error(mem, __FUNCTION__, __FILE__, __LINE__, code)
# else
#  define hpgl_call_note_error(mem, code)\
     hpgl_print_error(mem, (const char *)0, __FILE__, __LINE__, code)
# endif

#else /* !DEBUG */

#define hpgl_call_note_error(mem, code) (code)

#endif

/* We use the do ... while(0) in order to make the call be a statement */
/* syntactically. */

#define hpgl_call_and_check(mem, call, if_check_else)\
do {\
  int hpgl_call_and_check_code;\
  if ((hpgl_call_and_check_code = (call)) < 0)\
    { if_check_else()\
        return hpgl_call_note_error(mem, hpgl_call_and_check_code);\
    }\
} while (0)

/* Ordinary function calls */

#define hpgl_no_check()         /* */

#define hpgl_call(call)\
  hpgl_call_and_check(pgls->memory, call, hpgl_no_check)

#define hpgl_call_mem(mem, call)\
  hpgl_call_and_check(mem, call, hpgl_no_check)

/* Function calls that can set LOST mode */

#define hpgl_limitcheck_set_lost()\
  if ( code == gs_error_limitcheck )\
    hpgl_set_lost_mode(pgls, hpgl_lost_mode_entered);\
  else

#define hpgl_call_check_lost(call)\
  hpgl_call_and_check(pgls->memory, call, hpgl_limitcheck_set_lost)

/* We don't have a header file for exporting gl/2 character routines
   yet */
gs_point hpgl_current_char_scale(const hpgl_state_t * pgls);

bool hpgl_is_currentfont_stick_or_arc(const hpgl_state_t * pgls);

#endif /* pgmisc_INCLUDED */
