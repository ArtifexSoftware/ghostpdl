/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Interface to extended parameter dictionary utilities */

#ifndef gsparamx_INCLUDED
#  define gsparamx_INCLUDED

#include "stdpre.h"
#include "gstypes.h"
#include "gsparam.h"

/* Test whether a parameter's string value is equal to a C string. */
bool gs_param_string_eq(const gs_param_string *pcs, const char *str);

/*
 * Put parameters of various types.  These propagate ecode, presumably
 * the previous accumulated error code.
 */
int param_put_enum(gs_param_list * plist, gs_param_name param_name,
                   int *pvalue, const char *const pnames[], int ecode);
int param_put_bool(gs_param_list * plist, gs_param_name param_name,
                   bool * pval, int ecode);
int param_put_int(gs_param_list * plist, gs_param_name param_name,
                  int * pval, int ecode);
int param_put_long(gs_param_list * plist, gs_param_name param_name,
                   long * pval, int ecode);

/* Copy one parameter list to another, recursively if necessary. */
int param_list_copy(gs_param_list *plto, gs_param_list *plfrom);

#endif /* gsparamx_INCLUDED */
