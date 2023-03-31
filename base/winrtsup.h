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

#include "windows_.h"

DWORD GetTempPathWRT(DWORD nBufferLength, LPWSTR lpBuffer);

UINT GetTempFileNameWRT(LPCWSTR lpPathName, LPCWSTR lpPrefixString, LPWSTR lpTempFileName);

void OutputDebugStringWRT(LPCSTR str, DWORD len);
