/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgmisc.h */
/* definitions for HP-GL/2 lost mode and error handling routines. */

#ifndef pgmisc_INCLUDED
#  define pgmisc_INCLUDED

extern void hpgl_set_lost_mode(hpgl_state_t *pgls, hpgl_lost_mode_t lost_mode);

/* a macro that calls a function and returns an error code if the code
   returned is less than 0.  Most of the hpgl and gs functions return
   if the calling function is less than 0 so this avoids cluttering up
   the code with the if statement and debug code.  HAS The current
   function name is gcc specific. HAS this is getting a bit bloated!! */

#ifdef DEBUG
extern void hpgl_error(void);

#define hpgl_call(call)				\
{ 						\
  int code; 					\
  if ((code = (call)) != 0)			\
    {						\
      dprintf4("hpgl call failed\n\tcalled from: %s\n\tfile: %s\n\tline: %d\n\terror code: %d\n", \
	       __FUNCTION__, __FILE__, __LINE__, code);	\
      hpgl_error();                             \
      return(code);				\
    }						\
}
#else
#define hpgl_call(call) 			\
{						\
  int code;					\
  if ((code = (call)) < 0)			\
    {						\
      return(code);				\
    }						\
}
#endif

/* yet another macro for function calls that can set lost mode. */

#ifdef DEBUG
extern void hpgl_error(void);

#define hpgl_call_check_lost(call)		\
{ 						\
  int code; 					\
  if ( (code = (call)) != 0 )			\
    {						\
      if ( code == gs_error_limitcheck )        \
        {                                       \
	  hpgl_set_lost_mode(pgls, hpgl_lost_mode_entered); \
        }					\
      else {	                                \
          dprintf4("hpgl call failed\n\tcalled from: %s\n\tfile: %s\n\tline: %d\n\terror code: %d\n", \
	       __FUNCTION__, __FILE__, __LINE__, code);	\
           hpgl_error();                        \
           return(code);			\
        }                                       \
    }						\
}
#else
#define hpgl_call_check_lost(call)		\
{						\
  int code;					\
  if ((code = (call)) < 0)			\
    {						\
      if ( code == gs_error_limitcheck )        \
	hpgl_set_lost_mode(pgls, hpgl_lost_mode_entered); \
      else {printf("henry");return(code);}	        \
    }                                           \
}
#endif



#endif                       /* pgmisc_INCLUDED */
