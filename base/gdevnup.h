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


/* Common definitions for N-up device */

#ifndef gdevnup_INCLUDED
#  define gdevnup_INCLUDED

#include "gxdevice.h"

typedef struct gx_device_s gx_device_nup;

int gx_device_nup_device_install(gx_device *dev);

typedef struct {
    subclass_common;
    int PageCount;
    int PagesPerNest;
    int NupH;
    int NupV;
    float PageW;                /* points width of carrier page */
    float PageH;                /* points height of carrier page */
    float NestedPageW;		/* points -- from MediaSize */
    float NestedPageH;		/* points -- from MediaSize */
    float Scale;		/* 1:1 aspect ratio */
    float HMargin;
    float VMargin;
    float HSize;
    float VSize;
} Nup_device_subclass_data;

extern_st(st_device_nup);

#endif /* gdevnup_INCLUDED */
