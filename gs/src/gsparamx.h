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

/*$RCSfile$ $Revision$ */
/* Interface to extended parameter dictionary utilities */

#ifndef gsparamx_INCLUDED
#  define gsparamx_INCLUDED

/* Test whether a parameter's string value is equal to a C string. */
bool gs_param_string_eq(P2(const gs_param_string *pcs, const char *str));

/*
 * Put parameters of various types.  These propagate ecode, presumably
 * the previous accumulated error code.
 */
int param_put_enum(P5(gs_param_list * plist, gs_param_name param_name,
		      int *pvalue, const char *const pnames[], int ecode));
int param_put_bool(P4(gs_param_list * plist, gs_param_name param_name,
		      bool * pval, int ecode));
int param_put_int(P4(gs_param_list * plist, gs_param_name param_name,
		     int * pval, int ecode));
int param_put_long(P4(gs_param_list * plist, gs_param_name param_name,
		      long * pval, int ecode));

/* Copy one parameter list to another, recursively if necessary. */
int param_list_copy(P2(gs_param_list *plto, gs_param_list *plfrom));

#endif /* gsparamx_INCLUDED */
