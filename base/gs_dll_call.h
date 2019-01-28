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


/* calling convention macros for windows style dlls.
 */
#ifndef GS_DLL_CALL_H
#define GS_DLL_CALL_H

#ifdef __WINDOWS__
#ifndef _Windows
# define _Windows
#endif
#endif

#ifdef _Windows
# ifndef GSDLLEXPORT
/* We don't need both the "__declspec(dllexport)" declaration
 * and the listing in the .def file - having both results in
 * a linker warning on x64 builds (but is incorrect on x86, too)
 */
#  if 0
#    define GSDLLEXPORT __declspec(dllexport)
#  else
#    define GSDLLEXPORT
#  endif
# endif
# ifndef GSDLLAPI
#  define GSDLLAPI __stdcall
# endif
# ifndef GSDLLCALL
#  define GSDLLCALL __stdcall
# endif
#endif  /* _Windows */

#if defined(OS2) && defined(__IBMC__)
# ifndef GSDLLAPI
#  define GSDLLAPI _System
# endif
# ifndef GSDLLCALL
#  define GSDLLCALL _System
# endif
#endif	/* OS2 && __IBMC */

#ifdef __MACOS__
# pragma export on
#endif

#ifndef GSDLLEXPORT
# define GSDLLEXPORT
#endif
#ifndef GSDLLAPI
# define GSDLLAPI
#endif
#ifndef GSDLLCALL
# define GSDLLCALL
#endif

#if defined(__IBMC__)
# define GSDLLAPIPTR * GSDLLAPI
# define GSDLLCALLPTR * GSDLLCALL
#else
# define GSDLLAPIPTR GSDLLAPI *
# define GSDLLCALLPTR GSDLLCALL *
#endif

#endif /* GS_DLL_CALL_H */
