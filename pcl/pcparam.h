/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/*
 * pcparam.h - Definitions for PCL5 parameter setting
 * Requires gsmemory.h
 */

#ifndef pcparam_INCLUDED
#  define pcparam_INCLUDED

#include "gsparam.h"

/*
 * Put a single device parameter.
 * Return 0 if OK, 1 if must reopen device, or an error code.
 */
extern  int     put_param1_bool( pcl_state_t *, gs_param_name, bool );
extern  int     put_param1_float( pcl_state_t *, gs_param_name, floatp );
extern  int     put_param1_int( pcl_state_t *, gs_param_name, int );
extern  int     put_param1_float_array( pcl_state_t *, gs_param_name, float[2] );

#endif				/* pcparam_INCLUDED */
