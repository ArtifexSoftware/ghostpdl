/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Interface to zfcid.c */

#ifndef ifcid_INCLUDED
#  define ifcid_INCLUDED

/* Get the CIDSystemInfo of a CIDFont. */
int cid_font_system_info_param(P2(gs_cid_system_info_t *pcidsi,
				  const ref *prfont));

/* Get the additional information for a CIDFontType 0 or 2 CIDFont. */
int cid_font_data_param(P3(os_ptr op, gs_font_cid_data *pdata,
			   ref *pGlyphDirectory));

#endif /* ifcid_INCLUDED */
