/* Copyright (C) 1992, 1995, 1997, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* A bridge to True Type interpreter. */

#ifndef gxttfb_INCLUDED
#  define gxttfb_INCLUDED

#include "ttfoutl.h"

#ifndef gx_ttfReader_DEFINED
#  define gx_ttfReader_DEFINED
typedef struct gx_ttfReader_s gx_ttfReader;
#endif

#ifndef gs_font_type42_DEFINED
#  define gs_font_type42_DEFINED
typedef struct gs_font_type42_s gs_font_type42;
#endif

struct gx_ttfReader_s {
    ttfReader super;
    int pos;
    bool error;
    bool glyph_data_occupied;
    gs_glyph_data_t glyph_data;
    gs_font_type42 *pfont;
    gs_memory_t *memory;
};

gx_ttfReader *gx_ttfReader__create(gs_memory_t *mem, gs_font_type42 *pfont);
void gx_ttfReader__destroy(gx_ttfReader *this);
ttfFont *ttfFont__create(gs_memory_t *mem);
void ttfFont__destroy(ttfFont *this);
int ttfFont__Open_aux(ttfFont *this, ttfReader *r, unsigned int nTTC);

#endif /* gxttfb_INCLUDED */
