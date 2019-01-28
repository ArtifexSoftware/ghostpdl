/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Utilities for Level 2 filters */

#ifndef ifilter2_INCLUDED
#  define ifilter2_INCLUDED

#include "iostack.h"
#include "scfx.h"
#include "slzwx.h"
#include "spdiffx.h"
#include "spngpx.h"

/* Import setup code from zfdecode.c */
int zcf_setup(os_ptr op, stream_CF_state * pcfs, gs_ref_memory_t *imem);
int zlz_setup(os_ptr op, stream_LZW_state * plzs);
int zpd_setup(os_ptr op, stream_PDiff_state * ppds);
int zpp_setup(os_ptr op, stream_PNGP_state * ppps);

#endif /* ifilter2_INCLUDED */
