/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Miscellaneous support for platform facilities */

#include "close_.h"
#include "stdio_.h"
#include "stat_.h"
#include <fcntl.h>
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
	case '+':
	    flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
	    break;
	default:		/* e.g., 'b' */
	    break;
	}
    fildes = open(fname, flags, S_IRUSR | S_IWUSR);
    if (fildes < 0)
	return 0;
    file = fdopen(fildes, mode);
    if (file == 0)
	close(fildes);
    return file;
}
