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

#endif				/* plapi_INCLUDED */
