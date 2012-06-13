/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* plapi.h */
/* pcl6 as a library or dll api */

#ifndef plapi_INCLUDED
#  define plapi_INCLUDED

/*
 * This API level is intended to hide everything behind
 * a simple main_like  argc, argv  interface
 */

/* Exported functions may need different prefix
 *  GSDLLEXPORT marks functions as exported
 *  GSDLLAPI is the calling convention used on functions exported
 * When you include this header file in the caller, you may
 * need to change the definitions by defining these
 * before including this header file.
 * Make sure you get the calling convention correct, otherwise your
 * program will crash soon after returning due to stack corruption.
 */

#ifdef __WINDOWS__
# define _Windows
#endif

#ifdef _Windows
# ifndef GSDLLEXPORT
#  define GSDLLEXPORT __declspec(dllexport)
# endif
# ifndef GSDLLAPI
#  define GSDLLAPI __stdcall
# endif
#endif  /* _Windows */

#if defined(OS2) && defined(__IBMC__)
# ifndef GSDLLAPI
#  define GSDLLAPI _System
# endif
#endif	/* OS2 && __IBMC */

#ifdef __MACINTOSH__
# pragma export on
#endif

#ifndef GSDLLEXPORT
# define GSDLLEXPORT
#endif
#ifndef GSDLLAPI
# define GSDLLAPI
#endif

#if defined(__IBMC__)
# define GSDLLAPIPTR * GSDLLAPI
#else
# define GSDLLAPIPTR GSDLLAPI *
#endif

/* Run this just like you would pcl6 on the command line.
 *
 * Returns:	  0	success
 *		< 0	error code
 */
GSDLLEXPORT int GSDLLAPI
pl_main(int argc, char *argv[]);

GSDLLEXPORT int GSDLLAPI
pl_main_aux(int argc, char *argv[], void *disp);

#endif				/* plapi_INCLUDED */
