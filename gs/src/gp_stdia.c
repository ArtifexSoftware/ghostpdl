/* Copyright (C) 2001 artofcode LLC.  All rights reserved.
  
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/* $Id$ */
/* Read stdin on platforms that support unbuffered read. */
/* We want unbuffered for console input and pipes. */

#include "stdio_.h"
#include "time_.h"
#include "unistd_.h"
#include "gx.h"
#include "gp.h"


/* Read bytes from stdin, unbuffered if possible. */
int gp_stdin_read(char *buf, int len, int interactive, FILE *f)
{
    return read(fileno(f), buf, len);
}

