/* Copyright (C) 1996, 2000, 2001 Aladdin Enterprises.  All rights reserved.
  
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/* $Id$ */
/* Glyph data cache definition. */

#ifndef gxgcache_INCLUDED
#  define gxgcache_INCLUDED

/* Using : */

#ifndef gs_font_type42_DEFINED
#  define gs_font_type42_DEFINED
typedef struct gs_font_type42_s gs_font_type42;
#endif

#ifndef gs_glyph_data_DEFINED
#   define gs_glyph_data_DEFINED
typedef struct gs_glyph_data_s gs_glyph_data_t;
#endif

#ifndef stream_DEFINED
#  define stream_DEFINED
typedef struct stream_s stream;
#endif

/* Data type definition : */

#ifndef gs_glyph_cache_DEFINED
#  define gs_glyph_cache_DEFINED
typedef struct gs_glyph_cache_s gs_glyph_cache;
#endif

typedef int (*get_glyph_data_from_file)(gs_font_type42 *pfont, stream *s, uint glyph_index,
		gs_glyph_data_t *pgd);


/* Methods : */

gs_glyph_cache *gs_glyph_cache__alloc(gs_font_type42 *pfont, stream *s,
			get_glyph_data_from_file read_data);
void gs_glyph_cache__release(gs_glyph_cache *this);
int gs_get_glyph_data_cached(gs_font_type42 *pfont, uint glyph_index, gs_glyph_data_t *pgd);

#endif /* gxgcache_INCLUDED */
