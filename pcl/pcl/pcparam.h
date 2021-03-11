/* Copyright (C) 2001-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


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

int put_param1_float(pcl_state_t *, gs_param_name pkey, double val);

int put_param1_int(pcl_state_t *, gs_param_name pkey, int val);

int put_param1_float_array(pcl_state_t *, gs_param_name pkey,
                           float val_array[2]);
int put_param1_string(pcl_state_t * pcs, gs_param_name pkey, const char *val);

#endif /* pcparam_INCLUDED */
