/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Extended parameter dictionary utilities */
#include "string_.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "gsparamx.h"

/* Compare a C string and a gs_param_string. */
bool
gs_param_string_eq(const gs_param_string * pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
	    !strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Put an enumerated value. */
int
param_put_enum(gs_param_list * plist, gs_param_name param_name,
	       int *pvalue, const char *const pnames[], int ecode)
{
    gs_param_string ens;
    int code = param_read_name(plist, param_name, &ens);

    switch (code) {
	case 1:
	    return ecode;
	case 0:
	    {
		int i;

		for (i = 0; pnames[i] != 0; ++i)
		    if (gs_param_string_eq(&ens, pnames[i])) {
			*pvalue = i;
			return 0;
		    }
	    }
	    code = gs_error_rangecheck;
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, code);
    }
    return code;
}

/* Put a Boolean value. */
int
param_put_bool(gs_param_list * plist, gs_param_name param_name,
	       bool * pval, int ecode)
{
    int code;

    switch (code = param_read_bool(plist, param_name, pval)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }
    return ecode;
}

/* Put an integer value. */
int
param_put_int(gs_param_list * plist, gs_param_name param_name,
	      int *pval, int ecode)
{
    int code;

    switch (code = param_read_int(plist, param_name, pval)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }
    return ecode;
}

/* Put a long value. */
int
param_put_long(gs_param_list * plist, gs_param_name param_name,
	       long *pval, int ecode)
{
    int code;

    switch (code = param_read_long(plist, param_name, pval)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }
    return ecode;
}
