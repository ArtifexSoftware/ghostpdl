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
/* Read stdin on platforms that support select and non-blocking read */

#include "stdio_.h"
#include "unistd_.h"
#include "fcntl_.h"
#include "errno_.h"
#include "gx.h"
#include "gp.h"
#include "errors.h"

/* Configure stdin for non-blocking reads if possible. */
int gp_stdin_init(int fd)
{
    /* set file to non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK))
	return e_ioerror;
    return 0;
}

/* Read bytes from stdin, using non-blocking if possible. */
int gp_stdin_read(char *buf, int len, int interactive, int fd)
{
    fd_set rfds;
    int count;
    for (;;) {
	count = read(fd, buf, len);
	if (count >= 0)
	    break;
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    FD_ZERO(&rfds);
	    FD_SET(fd, &rfds);
	    select(1, &rfds, NULL, NULL, NULL);
	} else if (errno != EINTR) {
	    break;
	}
    }
    return count;
}

