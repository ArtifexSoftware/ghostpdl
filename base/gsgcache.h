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


/* Glyph data cache definition. */

#ifndef gxgcache_INCLUDED
#  define gxgcache_INCLUDED

#include "stdpre.h"
#include "scommon.h"

/* Using : */

typedef struct gs_font_type42_s gs_font_type42;

typedef struct gs_glyph_data_s gs_glyph_data_t;

/* Data type definition : */

typedef struct gs_glyph_cache_s gs_glyph_cache;

typedef int (*get_glyph_data_from_file)(gs_font_type42 *pfont, stream *s, uint glyph_index,
                gs_glyph_data_t *pgd);

/* Methods : */

gs_glyph_cache *gs_glyph_cache__alloc(gs_font_type42 *pfont, stream *s,
                        get_glyph_data_from_file read_data);
int gs_glyph_cache__release(void /* gs_glyph_cache */ *data, void*);
int gs_get_glyph_data_cached(gs_font_type42 *pfont, uint glyph_index, gs_glyph_data_t *pgd);

#endif /* gxgcache_INCLUDED */
