/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* Miscellaneous support for platform facilities */

#include "unistd_.h"
#include "fcntl_.h"
#include "stdio_.h"
#include "stat_.h"
#include "gp.h"
#include "gpgetenv.h"
#include "gpmisc.h"

/*
 * Get the name of the directory for temporary files, if any.  Currently
 * this checks the TMPDIR and TEMP environment variables, in that order.
 * The return value and the setting of *ptr and *plen are as for gp_getenv.
 */
int
gp_gettmpdir(char *ptr, int *plen)
{
    int max_len = *plen;
    int code = gp_getenv("TMPDIR", ptr, plen);

    if (code != 1)
	return code;
    *plen = max_len;
    return gp_getenv("TEMP", ptr, plen);
}

/*
 * Open a temporary file, using O_EXCL and S_I*USR to prevent race
 * conditions and symlink attacks.
 */
FILE *
gp_fopentemp(const char *fname, const char *mode)
{
    int flags = O_EXCL;
    /* Scan the mode to construct the flags. */
    const char *p = mode;
    int fildes;
    FILE *file;

    while (*p)
	switch (*p++) {
	case 'a':
	    flags |= O_CREAT | O_APPEND;
	    break;
	case 'r':
	    flags |= O_RDONLY;
	    break;
	case 'w':
	    flags |= O_CREAT | O_WRONLY | O_TRUNC;
	    break;
#ifdef O_BINARY
	    /* Watcom C insists on this non-ANSI flag being set. */
	case 'b':
	    flags |= O_BINARY;
	    break;
#endif
	case '+':
	    flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
	    break;
	default:		/* e.g., 'b' */
	    break;
	}
    fildes = open(fname, flags, S_IRUSR | S_IWUSR);
    if (fildes < 0)
	return 0;
    /*
     * The DEC VMS C compiler incorrectly defines the second argument of
     * fdopen as (char *), rather than following the POSIX.1 standard,
     * which defines it as (const char *).  Patch this here.
     */
    file = fdopen(fildes, (char *)mode); /* still really const */
    if (file == 0)
	close(fildes);
    return file;
}
