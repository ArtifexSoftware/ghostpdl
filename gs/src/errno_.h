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
/* Generic substitute for Unix errno.h */

#ifndef errno__INCLUDED
#  define errno__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/* All environments provide errno.h, but in some of them, errno.h */
/* only defines the error numbers, and doesn't declare errno. */
#include <errno.h>
#ifndef errno			/* in case it was #defined (very implausible!) */
extern int errno;

#endif

#endif /* errno__INCLUDED */
