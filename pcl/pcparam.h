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
/*$Id$ */

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
int put_param1_bool(P3(pcl_state_t *, gs_param_name, bool));
int put_param1_float(P3(pcl_state_t *, gs_param_name, floatp));
int put_param1_int(P3(pcl_state_t *, gs_param_name, int) );
int put_param1_float_array(P3(pcl_state_t *, gs_param_name, float[2]));

#endif				/* pcparam_INCLUDED */
