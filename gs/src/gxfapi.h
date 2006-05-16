/* Copyright (C) 2001-2006 artofcode LLC.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Font API support */

#ifndef gxfapi_INCLUDED
#  define gxfapi_INCLUDED

void gx_set_UFST_Callbacks(LPUB8 (*p_PCLchId2ptr)(IF_STATE *pIFS, UW16 chId));

void gx_reset_UFST_Callbacks(void);

#endif /* gxfapi_INCLUDED */
