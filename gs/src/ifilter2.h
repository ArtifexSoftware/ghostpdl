/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
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
