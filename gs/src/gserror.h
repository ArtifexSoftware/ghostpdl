/* Copyright (C) 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Error return macros */

#ifndef gserror_INCLUDED
#  define gserror_INCLUDED

int gs_log_error(int, const char *, int);
#ifndef DEBUG
#  define gs_log_error(err, file, line) (err)
#endif
#define gs_note_error(err) gs_log_error(err, __FILE__, __LINE__)
#define return_error(err) return gs_note_error(err)


#ifndef DEBUG
# define GS_ASSERT(exp)  if (!exp) { \
	dprintf3("ASSERT FAILURE: file %s line %d :%s\n", __FILE__, __LINE__, #exp); \
	return -1; } else ((void) 0)
# define GS_DBG_ASSERT(exp) ((void) 0)
#else
  void gs_assert(long line, const char *file, const char *exp);
# define GS_DBG_ASSERT(exp) if (exp) {} else	\
	gs_assert(__LINE__, __FILE__, #exp)
# define GS_ASSERT(exp) if (exp) {} else		\
	gs_assert(__LINE__, __FILE__, #exp)
#endif

#endif /* gserror_INCLUDED */
