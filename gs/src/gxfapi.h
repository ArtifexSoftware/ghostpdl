/* Copyright (C) 1991, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Font API support */

#ifndef gxfapi_INCLUDED
#  define gxfapi_INCLUDED

void gx_set_UFST_Callbacks(P3(LPUB8 (*p_PCLEO_charptr)(P2(LPUB8 pfont_hdr, UW16  sym_code)),
                              LPUB8 (*p_PCLchId2ptr)(P2(IF_STATE *pIFS, UW16 chId)),
                              LPUB8 (*p_PCLglyphID2Ptr)(P2(IF_STATE *pIFS, UW16 glyphID))));

void gx_reset_UFST_Callbacks(P0());

#endif /* gxfapi_INCLUDED */
