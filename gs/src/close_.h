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

/* Declaration of close */

#ifndef close__INCLUDED
#  define close__INCLUDED

/*
 * This absurd little file is needed because even though open and creat
 * are guaranteed to be declared in <fcntl.h>, close is not: in MS
 * environments, it's declared in <io.h>, but everywhere else it's in
 * <unistd.h>.
 */

/*
 * We must include std.h before any file that includes (or might include)
 * sys/types.h.
 */
#include "std.h"

#ifdef __WIN32__
#  include <io.h>
#else  /* !__WIN32__ */
#  include <unistd.h>
#endif /* !__WIN32__ */

#endif /* close__INCLUDED */
