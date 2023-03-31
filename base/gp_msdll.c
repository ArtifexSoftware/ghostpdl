/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/*
 * Microsoft Windows DLL support for Ghostscript.
 *
 */
#include "windows_.h"
#include "iapi.h"
#include "gp_mswin.h"

/* DLL entry point for Borland C++ */
GSDLLEXPORT BOOL WINAPI
DllEntryPoint(HINSTANCE hInst, DWORD fdwReason, LPVOID lpReserved)
{
    /* Win32s: HIWORD bit 15 is 1 and bit 14 is 0 */
    /* Win95:  HIWORD bit 15 is 1 and bit 14 is 1 */
    /* WinNT:  HIWORD bit 15 is 0 and bit 14 is 0 */
    /* WinNT Shell Update Release is WinNT && LOBYTE(LOWORD) >= 4 */
#ifdef METRO
	is_win32s = FALSE;
#else
    DWORD version = GetVersion();

    if (((HIWORD(version) & 0x8000) != 0) && ((HIWORD(version) & 0x4000) == 0))
        is_win32s = TRUE;
#endif

    phInstance = hInst;
    return TRUE;
}

/* DLL entry point for Microsoft Visual C++ */
GSDLLEXPORT BOOL WINAPI
DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID lpReserved)
{
    return DllEntryPoint(hInst, fdwReason, lpReserved);
}
