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
/* Replacement for missing mktemp */
#include "stat_.h"
#include "string_.h"

/* This procedure simulates mktemp on platforms that don't provide it. */
char *
mktemp(char *fname)
{
    struct stat fst;
    int len = strlen(fname);
    char *end = fname + len - 6;
    
    if (len < 6 || strcmp(end, "XXXXXX"))
	return (char *)0;	/* invalid  */
    strcpy(end, "AA.AAA");

    while (stat(fname, &fst) == 0) {
	char *inc = fname + len - 1;

	while (*inc == 'Z' || *inc == '.') {
	    if (inc == end)
		return (char *)0;	/* failure */
	    if (*inc == 'Z')
		*inc = 'A';
	    --inc;
	}
	++*inc;
    }
    return fname;
}
