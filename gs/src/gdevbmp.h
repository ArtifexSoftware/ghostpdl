/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* .BMP file format definitions and utility interfaces */

#ifndef gdevbmp_INCLUDED
#  define gdevbmp_INCLUDED

/* Define the default X and Y resolution. */
#define X_DPI 72
#define Y_DPI 72

/* Write the BMP file header.  This procedure is used for all formats. */
int write_bmp_header(P2(gx_device_printer *pdev, FILE *file));

/* Write a BMP header for separated CMYK output. */
int write_bmp_separated_header(P2(gx_device_printer *pdev, FILE *file));

/* 24-bit color mappers */
dev_proc_map_rgb_color(bmp_map_16m_rgb_color);
dev_proc_map_color_rgb(bmp_map_16m_color_rgb);

#endif				/* gdevbmp_INCLUDED */
