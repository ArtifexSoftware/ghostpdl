/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
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
{			
    /* A file name is absolute if it starts with a 0 or more .s */
    /* followed by a /. */
    while (len && *fname == '.')
	++fname, --len;
    return (len && *fname == '/');
}

/* Answer whether the file_name references the directory	*/
/* containing the specified path (parent). 			*/
bool
gp_file_name_references_parent(const char *fname, unsigned len)
{
    int i = 0, last_sep_pos = -1;

    /* A file name references its parent directory if it starts */
    /* with ../ or if ../ follows a / */
    while (i < len) {
	if (fname[i] == '/') {
	    last_sep_pos = i++;
	    continue;
	}
	if (fname[i++] != '.')
	    continue;
        if (i > last_sep_pos + 2 || (i < len && fname[i] != '.'))
	    continue;
	i++;
	/* have separator followed by .. */
	if (i < len && fname[i] == '/')
	    return true;
    }
    return false;
}


/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name. The prefix directory/device is examined to	*/
/* determine if a separator is needed and may return an empty string	*/
const char *
gp_file_name_concat_string(const char *prefix, unsigned plen)
{
    if (plen > 0 && prefix[plen - 1] == '/')
	return "";
    return "/";
}
