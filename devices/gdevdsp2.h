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

/* gdevdsp2.c */

#ifndef gdevdsp2_INCLUDED
#  define gdevdsp2_INCLUDED

#include "gdevdevn.h"
#include "gdevdsp.h"
#include "gxclist.h"

typedef struct gx_device_display_s gx_device_display;

#define gx_device_display_common\
        display_callback *callback;\
        void *pHandle;\
        int pHandle_set;\
        int nFormat;\
        void *pBitmap;\
        size_t zBitmapSize;\
        int HWResolution_set;\
        gs_devn_params devn_params;\
        equivalent_cmyk_color_params equiv_cmyk_colors;\
        gx_device_procs mutated_procs

/* The device descriptor */
struct gx_device_display_s {
    gx_device_common;
    gx_device_clist_mutatable_common;
    gx_device_display_common;
};

extern_st(st_device_display);
#define public_st_device_display()	/* in gdevdsp.c */\
  gs_public_st_composite_use_final(st_device_display, gx_device_display,\
    "gx_device_display", display_enum_ptrs, display_reloc_ptrs,\
    gx_device_finalize)

#endif /* gdevdsp2_INCLUDED */
