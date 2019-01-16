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


/* pcfontst.h - font information structures for the PCL state */

#include "gx.h"
#include "plfont.h"

#ifndef pcfontst_INCLUDED
#define pcfontst_INCLUDED

typedef struct pcl_font_selection_s
{
    /* Parameters used for selection, or loaded from font selected by ID. */
    pl_font_params_t params;

    /* Font found by matching or by ID */
    pl_font_t *font;
    uint selected_id;

    /* The symbol map that goes with it. */
    pl_symbol_map_t *map;
} pcl_font_selection_t;

#endif /* pcfontst_INCLUDED */
