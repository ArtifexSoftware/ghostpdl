/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* Definitions for PCL5 parameter setting */
/* Requires gsmemory.h */

#ifndef pcparam_INCLUDED
#  define pcparam_INCLUDED

#include "gsparam.h"

/* Put a single device parameter. */
/* Return 0 if OK, 1 if must reopen device, or an error code. */
int put_param1_bool(P3(pcl_state_t *, gs_param_name, bool));
int put_param1_float(P3(pcl_state_t *, gs_param_name, floatp));
int put_param1_int(P3(pcl_state_t *, gs_param_name, int));

#endif				/* pcparam_INCLUDED */
