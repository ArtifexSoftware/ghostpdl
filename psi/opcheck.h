/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Definitions for operator operand checking */
/* Requires ialloc.h (for imemory), iref.h, ierrors.h */

#ifndef opcheck_INCLUDED
#  define opcheck_INCLUDED

#include "iref.h"

/*
 * Check the type of an object.  Operators almost always use check_type,
 * which is defined in oper.h; check_type_only is for checking
 * subsidiary objects obtained from places other than the stack.
 */
#define check_type_only(rf,typ)\
  BEGIN if ( !r_has_type(&rf,typ) ) return_error(gs_error_typecheck); END
#define check_stype_only(rf,styp)\
  BEGIN if ( !r_has_stype(&rf,imemory,styp) ) return_error(gs_error_typecheck); END
/* Check for array */
#define check_array_else(rf,errstat)\
  BEGIN if ( !r_has_type(&rf, t_array) ) errstat; END
#define check_array_only(rf)\
  check_array_else(rf, return_error(gs_error_typecheck))
/* Check for procedure.  check_proc_failed includes the stack underflow */
/* check, but it doesn't do any harm in the off-stack case. */
int check_proc_failed(const ref *);

#define check_proc(rf)\
  BEGIN if ( !r_is_proc(&rf) ) return_error(check_proc_failed(&rf)); END
#define check_proc_only(rf) check_proc(rf)

/* Check for read, write, or execute access. */
#define check_access(rf,acc1)\
  BEGIN if ( !r_has_attr(&rf,acc1) ) return_error(gs_error_invalidaccess); END
#define check_read(rf) check_access(rf,a_read)
#define check_write(rf) check_access(rf,a_write)
#define check_execute(rf) check_access(rf,a_execute)
#define check_type_access_only(rf,typ,acc1)\
  BEGIN\
    if ( !r_has_type_attrs(&rf,typ,acc1) )\
      return_error((!r_has_type(&rf,typ) ? gs_error_typecheck : gs_error_invalidaccess));\
  END
#define check_read_type_only(rf,typ)\
  check_type_access_only(rf,typ,a_read)
#define check_write_type_only(rf,typ)\
  check_type_access_only(rf,typ,a_write)

/* Check for an integer value within an unsigned bound. */
#define check_int_leu(orf, u)\
  BEGIN\
    check_type(orf, t_integer);\
    if ( (ulong)(orf).value.intval > (u) ) return_error(gs_error_rangecheck);\
  END
#define check_int_leu_only(rf, u)\
  BEGIN\
    check_type_only(rf, t_integer);\
    if ( (ulong)(rf).value.intval > (u) ) return_error(gs_error_rangecheck);\
  END
#define check_int_ltu(orf, u)\
  BEGIN\
    check_type(orf, t_integer);\
    if ( (ulong)(orf).value.intval >= (u) ) return_error(gs_error_rangecheck);\
  END

#endif /* opcheck_INCLUDED */
