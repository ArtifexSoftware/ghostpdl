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


/* $Id: gsht_thresh.h  $ */
/* Threshhold based halftoning prototypes */

#ifndef gsht_thresh_INCLUDED
#  define gsht_thresh_INCLUDED

#include "gxdda.h"
#include "gsiparam.h"
#include "gxiclass.h"

#define RAW_HT_DUMP 0
#define USE_FAST_HT_CODE 1
/* #define PACIFY_VALGRIND */

#if RAW_HT_DUMP
void gx_ht_threshold_row_byte(byte *contone, byte *threshold_strip,
                              int contone_stride, byte *halftone,
                              int dithered_stride, int width, int num_rows);
#endif
void gx_ht_threshold_row_bit(byte *contone,  byte *threshold_strip,
                             int contone_stride, byte *halftone,
                             int dithered_stride, int width, int num_rows,
                             int offset_bits);
void gx_ht_threshold_row_bit_sub(byte *contone,  byte *threshold_strip,
                             int contone_stride, byte *halftone,
                             int dithered_stride, int width, int num_rows,
                             int offset_bits);
void gx_ht_threshold_landscape(byte *contone_align, byte *thresh_align,
                    ht_landscape_info_t *ht_landscape, byte *halftone,
                    int data_length);
void gx_ht_threshold_landscape_sub(byte *contone_align, byte *thresh_align,
                    ht_landscape_info_t *ht_landscape, byte *halftone,
                    int data_length);
int gxht_thresh_image_init(gx_image_enum *penum);
int gxht_thresh_planes(gx_image_enum *penum, fixed xrun, int dest_width,
                       int dest_height, byte *thresh_align, gx_device * dev,
                       int offset_contone[], int contone_stride);

/* Helper function for an operation performed several times */
int gxht_dda_length(gx_dda_fixed *dda, int src_size);

#endif /* gshtx_INCLUDED */
