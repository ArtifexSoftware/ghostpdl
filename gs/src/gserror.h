/* Copyright (C) 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Error return macros */

#ifndef gserror_INCLUDED
#  define gserror_INCLUDED

int gs_log_error(P3(int, const char *, int));
#ifndef DEBUG
#  define gs_log_error(err, file, line) (err)
#endif
#define gs_note_error(err) gs_log_error(err, __FILE__, __LINE__)
#define return_error(err) return gs_note_error(err)
#define return_if_error(expr)\
  BEGIN int code_ = (expr); if ( code_ < 0 ) return code_; END

#endif /* gserror_INCLUDED */
