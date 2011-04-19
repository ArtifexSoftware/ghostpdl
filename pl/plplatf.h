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
/*$Id$ */

/* plplatf.h   Platform-related utils */

#ifndef plplatf_INCLUDED
#   define plplatf_INCLUDED

/* ------------- Platform de/init --------- */
void
pl_platform_init(FILE *debug_out);

void
pl_platform_dnit(int exit_status);

/*----- The following is declared here, but must be implemented by client ----*/
/* Terminate execution */
void pl_exit(int exit_status);

#endif     /* plplatf_INCLUDED */
