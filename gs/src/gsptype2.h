/* Copyright (C) 1997, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Client interface to PatternType 2 Patterns */

#ifndef gsptype2_INCLUDED
#  define gsptype2_INCLUDED

#include "gspcolor.h"

/* ---------------- Types and structures ---------------- */

/* PatternType 2 template */

#ifndef gs_shading_t_DEFINED
#  define gs_shading_t_DEFINED
typedef struct gs_shading_s gs_shading_t;
#endif

typedef struct gs_pattern2_template_s {
    gs_pattern_template_common;
    const gs_shading_t *Shading;
} gs_pattern2_template_t;

#define private_st_pattern2_template() /* in gsptype2.c */\
  gs_private_st_suffix_add1(st_pattern2_template,\
    gs_pattern2_template_t, "gs_pattern2_template_t",\
    pattern2_template_enum_ptrs, pattern2_template_reloc_ptrs,\
    st_pattern_template, Shading)
#define st_pattern2_template_max_ptrs (st_pattern_template_max_ptrs + 1)

/* PatternType 2 instance */

#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;
#endif

typedef struct gs_pattern2_instance_s {
    gs_pattern_instance_common;
    gs_pattern2_template_t template;
} gs_pattern2_instance_t;

#define private_st_pattern2_instance() /* in gsptype2.c */\
  gs_private_st_composite(st_pattern2_instance, gs_pattern2_instance_t,\
    "gs_pattern2_instance_t", pattern2_instance_enum_ptrs,\
    pattern2_instance_reloc_ptrs)

/* ---------------- Procedures ---------------- */

/*
 * We should provide a gs_cspace_build_Pattern2 procedure to construct
 * the color space, but we don't.
 */

/* Initialize a PatternType 2 pattern. */
void gs_pattern2_init(P1(gs_pattern2_template_t *));

#endif /* gsptype2_INCLUDED */
