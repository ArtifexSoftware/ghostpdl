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
int put_param1_bool(pcl_state_t *, gs_param_name pkey, bool val);
int put_param1_float(pcl_state_t *, gs_param_name pkey, floatp val);
int put_param1_int(pcl_state_t *, gs_param_name pkey, int val);
int put_param1_float_array(pcl_state_t *, gs_param_name pkey, float val_array[2]);
int put_param1_string(pcl_state_t *pcs, gs_param_name pkey, const char *val);

#endif				/* pcparam_INCLUDED */
