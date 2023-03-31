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


/* Interface to zfcid.c */

#ifndef ifcid_INCLUDED
#  define ifcid_INCLUDED

#include "gxfcid.h"
#include "icid.h"
#include "iostack.h"

/* Get the CIDSystemInfo of a CIDFont. */
int cid_font_system_info_param(gs_cid_system_info_t *pcidsi,
                               const ref *prfont);

/* Get the additional information for a CIDFontType 0 or 2 CIDFont. */
int cid_font_data_param(os_ptr op, gs_font_cid_data *pdata,
                        ref *pGlyphDirectory);

#endif /* ifcid_INCLUDED */
