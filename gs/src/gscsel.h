/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Definitions for color operand selection */

#ifndef gscsel_INCLUDED
#  define gscsel_INCLUDED

/*
 * Define whether we are mapping a "source" or a "texture" color for
 * RasterOp.  Right the source and texture only have separate halftone
 * phases in the graphics state, but someday they might have more.
 */
typedef enum {
    gs_color_select_all = -1,	/* for setting only, not for reading */
    gs_color_select_texture = 0,	/* 0 is the one is used for currenthtphase */
    gs_color_select_source = 1
} gs_color_select_t;

#define gs_color_select_count 2

#endif /* gscsel_INCLUDED */
