/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
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
extern  int     put_param1_bool( pcl_state_t *, gs_param_name, bool );
extern  int     put_param1_float( pcl_state_t *, gs_param_name, floatp );
extern  int     put_param1_int( pcl_state_t *, gs_param_name, int );
extern  int     put_param1_float_array( pcl_state_t *, gs_param_name, float[2] );

#endif				/* pcparam_INCLUDED */
