/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Generic substitute for Unix dirent.h */

#ifndef dirent__INCLUDED
#  define dirent__INCLUDED

/* We must include std.h before any file that includes sys/types.h. */
#include "std.h"

/*
 * The location (or existence) of certain system headers is
 * environment-dependent. We detect this in the makefile
 * and conditionally define switches in gconfig_.h.
 */
#include "gconfig_.h"

/*
 * Directory entries may be defined in quite a number of different
 * header files.  The following switches are defined in gconfig_.h.
 */
#if  defined(HAVE_DIRENT_H) && HAVE_DIRENT_H == 1
#  include <dirent.h>
typedef struct dirent dir_entry;

#else /* sys/ndir or ndir or sys/dir, i.e., no dirent */
#  if defined(HAVE_SYS_DIR_H) && HAVE_SYS_DIR_H == 1
#    include <sys/dir.h>
#  endif
#  if defined(HAVE_SYS_NDIR_H) && HAVE_SYS_NDIR_H == 1
#    include <sys/ndir.h>
#  endif
#  if defined(HAVE_NDIR_H) && HAVE_NDIR_H == 1
#    include <ndir.h>
#  endif
typedef struct direct dir_entry;

#endif /* sys/ndir or ndir or sys/dir */

#endif /* dirent__INCLUDED */
