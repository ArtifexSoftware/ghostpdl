/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
  
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

/*$RCSfile$ $Revision$ */
/* Utilities for Level 2 filters */

#ifndef ifilter2_INCLUDED
#  define ifilter2_INCLUDED

/* Import setup code from zfdecode.c */
int zcf_setup(P3(os_ptr op, stream_CF_state * pcfs, gs_ref_memory_t *imem));
int zlz_setup(P2(os_ptr op, stream_LZW_state * plzs));
int zpd_setup(P2(os_ptr op, stream_PDiff_state * ppds));
int zpp_setup(P2(os_ptr op, stream_PNGP_state * ppps));

#endif /* ifilter2_INCLUDED */
