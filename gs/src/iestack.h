/* Copyright (C) 1989, 1992, 1993, 1994, 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Generic execution stack API */

#ifndef iestack_INCLUDED
#  define iestack_INCLUDED

#include "iesdata.h"
#include "istack.h"

/* Define pointers into the execution stack. */
typedef s_ptr es_ptr;
typedef const_s_ptr const_es_ptr;

/* Manage the current_file cache. */
#define estack_clear_cache(pes) ((pes)->current_file = 0)
#define estack_set_cache(pes,pref) ((pes)->current_file = (pref))
#define estack_check_cache(pes)\
  BEGIN\
    if (r_has_type_attrs((pes)->stack.p, t_file, a_executable))\
      estack_set_cache(pes, (pes)->stack.p);\
  END

#endif /* iestack_INCLUDED */
