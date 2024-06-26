/* Copyright (C) 2001-2023 Artifex Software, Inc.
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

#ifndef plfapi_h_INCLUDED
#define plfapi_h_INCLUDED

/* Font API support  */

int pl_fapi_rebuildfont(gs_font * pfont);

int
pl_fapi_passfont(pl_font_t * plfont, int subfont, char *fapi_request,
                 char *file_name, byte * font_data, int font_data_len);

int pl_fapi_get_mtype_font_name(gs_font * pfont, byte * data, int *size);

int pl_fapi_get_mtype_font_number(gs_font * pfont, int *font_number);

int pl_fapi_get_mtype_font_spaceBand(gs_font * pfont, uint * spaceBand);

int pl_fapi_get_mtype_font_scaleFactor(gs_font * pfont, uint * scaleFactor);

const char *pl_fapi_ufst_get_fco_list(gs_memory_t * mem);

const char *pl_fapi_ufst_get_font_dir(gs_memory_t * mem);

bool pl_fapi_ufst_available(gs_memory_t * mem);

#endif
