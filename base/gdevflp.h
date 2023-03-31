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


/* Common definitions for "first/last page" device */

#ifndef gdevflp_INCLUDED
#  define gdevflp_INCLUDED

#ifndef gxdevice_INCLUDED
#include "gxdevice.h"
#endif

typedef struct gx_device_s gx_device_flp;

/* Initialize a first/last page device. */
void gx_device_flp_init(gx_device_flp * dev);

typedef enum {none, even, odd} flp_EOType;

typedef struct {
    subclass_common;
    int PageCount;
    int *page_range_array;
} first_last_subclass_data;

typedef struct flp_text_enum_s {
    gs_text_enum_common;
} flp_text_enum_t;
#define private_st_flp_text_enum()\
  extern_st(st_gs_text_enum);\
  gs_private_st_suffix_add0(st_flp_text_enum, flp_text_enum_t,\
    "flp_text_enum_t", flp_text_enum_enum_ptrs, flp_text_enum_reloc_ptrs,\
    st_gs_text_enum)

extern_st(st_device_flp);
#define public_st_device_flp()	/* in gdevbflp.c */\


#endif /* gdevflp_INCLUDED */
