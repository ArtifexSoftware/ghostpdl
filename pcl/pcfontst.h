/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
    bool                selected_by_id;

    /* The symbol map that goes with it. */
    pl_symbol_map_t *   map;
} pcl_font_selection_t;

#endif			/* pcfontst_INCLUDED */
