/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Unix-like file name syntax platform routines for Ghostscript */
#include "gx.h"
#include "gp.h"

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ':';

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char gp_fmode_binary_suffix[] = "";

/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "r";
const char gp_fmode_wb[] = "w";

/* Answer whether a file name contains a directory/device specification, */
/* i.e. is absolute (not directory- or device-relative). */
bool
gp_file_name_is_absolute(const char *fname, unsigned len)
{				/* A file name is absolute if it starts with a 0 or more .s */
    /* followed by a /. */
    while (len && *fname == '.')
	++fname, --len;
    return (len && *fname == '/');
}

/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name.  The file name is known to not be absolute. */
const char *
gp_file_name_concat_string(const char *prefix, unsigned plen,
			   const char *fname, unsigned len)
{
    if (plen > 0 && prefix[plen - 1] == '/')
	return "";
    return "/";
}
