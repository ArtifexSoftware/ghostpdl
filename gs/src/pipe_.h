/* Copyright (C) 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Declaration of popen and pclose */

#ifndef pipe__INCLUDED
#  define pipe__INCLUDED

#include "stdio_.h"

#ifdef __WIN32__
/*
 * MS Windows has popen and pclose in stdio.h, but under different names.
 */
#  define popen(cmd, mode) _popen(cmd, mode)
#  define pclose(file) _pclose(file)
#else  /* !__WIN32__ */
/*
 * popen isn't POSIX-standard, so we declare it here.
 * Because of inconsistent (and sometimes incorrect) header files,
 * we must omit the argument list.  Unfortunately, this sometimes causes
 * more trouble than it cures.
 */
extern FILE *popen( /* P2(const char *, const char *) */ );
extern int pclose(P1(FILE *));
#endif /* !__WIN32__ */

#endif /* pipe__INCLUDED */
