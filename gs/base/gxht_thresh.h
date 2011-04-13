/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gsht_thresh.h  $ */
/* Threshhold based halftoning prototypes */

#ifndef gsht_thresh_INCLUDED
#  define gsht_thresh_INCLUDED

#define RAW_HT_DUMP 0
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
void gx_ht_threshold_landscape(byte *contone_align, byte *thresh_align,
                    ht_landscape_info_t ht_landscape, byte *halftone,
                    int data_length);
int gxht_thresh_image_init(gx_image_enum *penum);
int  gxht_thresh_plane(gx_image_enum *penum, gx_ht_order *d_order,  
                  fixed xrun, int dest_width, int dest_height,
                  byte *thresh_align, byte *contone_align, int contone_stride, 
                  gx_device * dev); 
#endif /* gshtx_INCLUDED */

