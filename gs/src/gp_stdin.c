/* Copyright (C) 2001 artofcode LLC.  All rights reserved.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.
*/

/*$Id$ */
/* Read stdin on platforms that do not support non-blocking reads */

#include "stdio_.h"
#include "gx.h"
#include "gp.h"

/* Configure stdin for non-blocking reads if possible. */
int gp_stdin_init(int fd)
{
    /* do nothing */
    return 0;
}

/* Read bytes from stdin, using non-blocking if possible. */
int gp_stdin_read(char *buf, int len, int interactive, int fd)
{
    return read(buf, 1, interactive ? 1 : len, fd);
}

