/* Copyright (C) 2001-2018 Artifex Software, Inc.
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


/* pldraw.h */
/* Interface to pldraw.c */

#ifndef pldraw_INCLUDED
#  define pldraw_INCLUDED

#include "gsiparam.h"
#include "gsimage.h"
#include "gsgstate.h"

/* Begin an image with parameters derived from a graphics state. */
int pl_begin_image(gs_gstate * pgs, const gs_image_t * pim, void **pinfo);

/* draw image data */
int pl_image_data(gs_gstate * pgs, void *info, const byte ** planes,
                  int data_x, uint raster, int height);

/* end image */
int pl_end_image(gs_gstate * pgs, void *info, bool draw_last);


/* NEW API */
int pl_begin_image2(gs_image_enum ** ppenum, gs_image_t * pimage,
                    gs_gstate * pgs);

int pl_image_data2(gs_image_enum * penum, const byte * row, uint size,
                   uint * pused);

int pl_end_image2(gs_image_enum * penum, gs_gstate * pgs);

#endif /* pldraw_INCLUDED */
