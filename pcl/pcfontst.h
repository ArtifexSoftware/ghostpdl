/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

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
