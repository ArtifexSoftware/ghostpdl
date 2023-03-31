/* Copyright (C) 2020-2023 Artifex Software, Inc.
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
#ifndef PDF_CMAP_H
#define PDF_CMAP_H

#include "gxfcmap1.h"
#include "pdf_font_types.h"

/* Each pdfi_cmap_range_map_t allocations should include the
 * space required for the key and value strings, thus is
 * self contained, and avoids allocating lots of 1/2 byte
 * blocks
 */
typedef struct pdfi_cmap_range_map_s pdfi_cmap_range_map_t;

struct pdfi_cmap_range_map_s {
  gx_cmap_lookup_range_t range;
  pdfi_cmap_range_map_t *next;
};

typedef struct pdfi_cmap_range_s pdfi_cmap_range_t;

struct pdfi_cmap_range_s {
  pdfi_cmap_range_map_t *ranges;
  pdfi_cmap_range_map_t *ranges_tail;
  int numrangemaps;
};

typedef struct pdf_cmap_s pdf_cmap;

struct pdf_cmap_s
{
  pdf_obj_common;
  byte *buf;
  int buflen;
  int cmaptype;
  gs_string name;
  gs_string csi_reg;
  gs_string csi_ord;
  int csi_supplement;
  float vers;
  gs_uid uid;
  int wmode;
  gx_code_space_t code_space;
  pdfi_cmap_range_t cmap_range;
  pdfi_cmap_range_t notdef_cmap_range;
  gs_cmap_adobe1_t *gscmap;

  pdf_cmap *next;
};


int
pdfi_read_cmap(pdf_context *ctx, pdf_obj *cmap, pdf_cmap **pcmap);
int
pdfi_free_cmap(pdf_obj *cmapo);

#endif /* PDF_CMAP_H */
