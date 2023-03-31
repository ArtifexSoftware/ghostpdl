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

#pragma once

#ifndef __AFXWIN_H__
        #error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols

// CICC_CreatorApp:
// See ICC_Creator.cpp for the implementation of this class
//

class CICC_CreatorApp : public CWinApp
{
public:
        CICC_CreatorApp();

// Overrides
        public:
        virtual BOOL InitInstance();

// Implementation

        DECLARE_MESSAGE_MAP()
};

extern CICC_CreatorApp theApp;
