/* Copyright (C) 1992, 1993, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$RCSfile$ $Revision$ */
/* Common routines for MS-DOS (any compiler) and DesqView/X, */
/* which has a MS-DOS-like file system. */
#include "dos_.h"
#include "gx.h"
#include "gp.h"

/* ------ Printer accessing ------ */

/* Put a printer file (which might be stdout) into binary or text mode. */
/* This is not a standard gp procedure, */
/* but all MS-DOS configurations need it. */
void
gp_set_file_binary(int prnfno, bool binary)
{
    union REGS regs;

    regs.h.ah = 0x44;		/* ioctl */
    regs.h.al = 0;		/* get device info */
    regs.rshort.bx = prnfno;
    intdos(&regs, &regs);
    if (regs.rshort.cflag != 0 || !(regs.h.dl & 0x80))
	return;			/* error, or not a device */
    if (binary)
	regs.h.dl |= 0x20;	/* binary (no ^Z intervention) */
    else
	regs.h.dl &= ~0x20;	/* text */
    regs.h.dh = 0;
    regs.h.ah = 0x44;		/* ioctl */
    regs.h.al = 1;		/* set device info */
    intdos(&regs, &regs);
}

/* ------ File accessing ------ */

/* Set a file into binary or text mode. */
int
gp_setmode_binary(FILE * pfile, bool binary)
{
    gp_set_file_binary(fileno(pfile), binary);
    return 0;			/* Fake out dos return status */
}

/* ------ File names ------ */

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ';';

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char gp_fmode_binary_suffix[] = "b";

/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "rb";
const char gp_fmode_wb[] = "wb";

/* Answer whether a file name contains a directory/device specification, */
/* i.e. is absolute (not directory- or device-relative). */
bool
gp_file_name_is_absolute(const char *fname, unsigned len)
{
    /* A file name is absolute if it contains a drive specification */
    /* (second character is a :) or if it start with 0 or more .s */
    /* followed by a / or \. */
    if (len >= 2 && fname[1] == ':')
	return true;
    while (len && *fname == '.')
	++fname, --len;
    return (len && (*fname == '/' || *fname == '\\'));
}

/* Answer whether the file_name references the directory	*/
/* containing the specified path (parent). 			*/
bool
gp_file_name_references_parent(const char *fname, unsigned len)
{
    int i = 0, last_sep_pos = -1;

    /* A file name references its parent directory if it starts */
    /* with ../ or ..\  or if one of these strings follows / or \ */
    while (i < len) {
	if (fname[i] == '/' || fname[i] == '\\') {
	    last_sep_pos = i++;
	    continue;
	}
	if (fname[i++] != '.')
	    continue;
        if (i > last_sep_pos + 2 || (i < len && fname[i] != '.'))
	    continue;
	i++;
	/* have separator followed by .. */
	if (i < len && (fname[i] == '/' || fname[i++] == '\\'))
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
    if (plen > 0)
	switch (prefix[plen - 1]) {
	    case ':':
	    case '/':
	    case '\\':
		return "";
	};
    return "/";
}
