/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
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
