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
    DWORD version = GetVersion();

    if (((HIWORD(version) & 0x8000) != 0) && ((HIWORD(version) & 0x4000) == 0))
	is_win32s = TRUE;

    phInstance = hInst;
    return TRUE;
}

/* DLL entry point for Microsoft Visual C++ */
GSDLLEXPORT BOOL WINAPI
DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID lpReserved)
{
    return DllEntryPoint(hInst, fdwReason, lpReserved);
}


