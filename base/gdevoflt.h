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


/* Common definitions for "object filter" device */

#ifndef gdev_obj_filter_INCLUDED
#  define gdev_obj_filter_INCLUDED

#ifndef gxdevice_INCLUDED
#include "gxdevice.h"
#endif

typedef struct gx_device_s gx_device_obj_filter;

/* Initialize a object filter device. */
void gx_device_obj_filter_init(gx_device_obj_filter * dev);

typedef struct {
    subclass_common;
} obj_filter_subclass_data;

typedef struct obj_filter_text_enum_s {
    gs_text_enum_common;
} obj_filter_text_enum_t;
#define private_st_obj_filter_text_enum()\
  extern_st(st_gs_text_enum);\
  gs_private_st_suffix_add0(st_obj_filter_text_enum, obj_filter_text_enum_t,\
    "obj_filter_text_enum_t", obj_filter_text_enum_enum_ptrs, obj_filter_text_enum_reloc_ptrs,\
    st_gs_text_enum)

extern_st(st_device_obj_filter);
#define public_st_device_obj_filter()	/* in gdevoflt.c */\

#endif /* gdev_obj_filter_INCLUDED */
