/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

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

#endif /* gsparamx_INCLUDED */
