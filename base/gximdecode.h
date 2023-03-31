/* Copyright (C) 2014-2023 Artifex Software, Inc.
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

/* Prototypes for decoding and unpacking image data.  Used for color
   monitoring in clist and for creating TIFF files for xpswrite device */

#ifndef gximdecode_h_INCLUDED
#define gximdecode_h_INCLUDED

#include "gx.h"
#include "gxfixed.h"
#include "gximage.h"
#include "gxsample.h"
#include "gxfrac.h"

typedef void(*applymap_t) (sample_map map[], const void *psrc, int spp,
    void *pdes, void *bufend);

/* Define the structure the image_enums can use for handling decoding */
typedef struct image_decode_s {
    int bps;
    int spp;
    SAMPLE_UNPACK_PROC((*unpack));
    int spread;
    sample_map map[GS_IMAGE_MAX_COMPONENTS];
    applymap_t applymap;
} image_decode_t;

void get_unpack_proc(gx_image_enum_common_t *pie, image_decode_t *imd,
    gs_image_format_t format, const float *decode);
void get_map(image_decode_t *imd, gs_image_format_t format, const float *decode);
void applymap16(sample_map map[], const void *psrc, int spp, void *pdes, void *bufend);
void applymap8(sample_map map[], const void *psrc, int spp, void *pdes, void *bufend);

#endif
