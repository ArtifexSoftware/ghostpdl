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

/*$RCSfile$ $Revision$ */
/* Interrupt check interface */

#ifndef gpcheck_INCLUDED
#  define gpcheck_INCLUDED

/*
 * On some platforms, the interpreter must check periodically for user-
 * initiated actions.  (Eventually, this may be extended to all platforms,
 * to handle multi-tasking through the 'context' facility.)  Routines that
 * run for a long time must periodically call gp_check_interrupts(), and
 * if it returns true, must clean up whatever they are doing and return an
 * e_interrupted (or gs_error_interrupted) exceptional condition.
 * The return_if_interrupt macro provides a convenient way to do this.
 *
 * On platforms that require an interrupt check, the makefile defines
 * a symbol CHECK_INTERRUPTS.  Currently this is only the Microsoft
 * Windows platform.
 */
int gs_return_check_interrupt(int code);

#ifdef CHECK_INTERRUPTS
int gp_check_interrupts(void);
#  define process_interrupts() discard(gp_check_interrupts())
#  define return_if_interrupt()\
    { int icode_ = gp_check_interrupts();\
      if ( icode_ )\
	return gs_note_error((icode_ > 0 ? gs_error_interrupt : icode_));\
    }
#  define return_check_interrupt(code)\
    return gs_return_check_interrupt(code)
#  define set_code_on_interrupt(pcode)\
    if (*(pcode) == 0)\
     *(pcode) = (gp_check_interrupts() != 0) ? gs_error_interrupt : 0;
#else
#  define gp_check_interrupts() 0
#  define process_interrupts() DO_NOTHING
#  define return_if_interrupt()	DO_NOTHING
#  define return_check_interrupt(code)\
    return (code)
#  define set_code_on_interrupt(pcode) DO_NOTHING
#endif

#endif /* gpcheck_INCLUDED */
