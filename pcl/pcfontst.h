/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pcfontst.h - font information structures for the PCL state */

#include "gx.h"
#include "plfont.h"

#ifndef pcfontst_INCLUDED
#define pcfontst_INCLUDED

#ifndef pl_font_t_DEFINED
#  define pl_font_t_DEFINED
typedef struct pl_font_s    pl_font_t;
#endif

typedef struct pcl_font_selection_s {
    /* Parameters used for selection, or loaded from font selected by ID. */
    pl_font_params_t    params;

    /* Font found by matching or by ID */
    pl_font_t *         font;
    uint                selected_id;

    /* The symbol map that goes with it. */
    pl_symbol_map_t *   map;
} pcl_font_selection_t;

#endif			/* pcfontst_INCLUDED */
