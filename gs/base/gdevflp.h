/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Common definitions for "first/last page" device */

#ifndef gdevflp_INCLUDED
#  define gdevflp_INCLUDED

typedef struct gx_device_s gx_device_flp;

/* Initialize a first/last page device. */
void gx_device_flp_init(gx_device_flp * dev);

typedef int (t_dev_proc_create_compositor) (gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte, gs_imager_state *pis, gs_memory_t *memory, gx_device *cdev);

#define subclass_common\
    t_dev_proc_create_compositor *saved_compositor_method

typedef struct {
    subclass_common;
    unsigned int private_data_size;
    int PageCount;
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

int gx_copy_device_procs(gx_device_procs *dest_procs, gx_device_procs *src_procs, gx_device_procs *prototype_procs);
int gx_device_subclass(gx_device *dev_to_subclass, gx_device *new_prototype, unsigned int private_data_size);
int gx_unsubclass_device(gx_device *dev);
int gx_update_from_subclass(gx_device *dev);
int gx_subclass_create_compositor(gx_device *dev, gx_device **pcdev, const gs_composite_t *pcte,
    gs_imager_state *pis, gs_memory_t *memory, gx_device *cdev);

#endif /* gdevflp_INCLUDED */
