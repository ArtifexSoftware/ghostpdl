/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
 * Public types and functions for
 * OpenType font API using Ghostscript graphics library.
 *
 * This file requires either gslite.h or the internal gs headers
 * for gs_memory_t, gx_device, gs_state and gs_matrix.
 */

typedef struct gslt_font_s gslt_font_t;
typedef struct gslt_glyph_metrics_s gslt_glyph_metrics_t;
typedef struct gslt_glyph_bitmap_s gslt_glyph_bitmap_t;
typedef struct gslt_outline_walker_s gslt_outline_walker_t;

struct gslt_glyph_metrics_s
{
    float hadv, vadv, vorg;
};

struct gslt_glyph_bitmap_s
{
    int w, h, stride;           /* bitmap size; stride is number of bytes per line */
    int lsb, top;               /* bitmap offset (left-side-bearing and top) */
    unsigned char *data;        /* bitmap data */
#ifdef USE_OLD_GS
    int refs;			/* private for now */
#else
    void *cc;			/* cc slot that the data uses */
#endif
};

struct gslt_outline_walker_s
{
    void *user;
    int (*moveto)(void *user, float x, float y);
    int (*lineto)(void *user, float x, float y);
    int (*curveto)(void *user, float x0, float y0, float x1, float y1, float x2, float y2);
    int (*closepath)(void *user);
};

/*
 * Create the font cache machinery.
 */
gs_font_dir * gslt_new_font_cache(gs_memory_t *mem);

/*
 * Destroy the font machinery (and all associated fonts).
 */
void gslt_free_font_cache(gs_memory_t *mem, gs_font_dir *cache);

/*
 * Create and initialize a font structure from the
 * font file, using index to select which font in TTC font
 * collections.
 *
 * The font object retains a pointer to the memory passed in,
 * but does not assume ownership. Free the memory
 * after freeing the font.
 * Return NULL on failure.
 */
gslt_font_t * gslt_new_font(gs_memory_t *mem, gs_font_dir *cache, char *buf, int buflen, int index);

/*
 * Destroy a font structure.
 */
void gslt_free_font(gs_memory_t *mem, gslt_font_t *font);

/*
 * Return the number of encoding (cmap) sub-tables in a font.
 * Return zero if there is no cmap table at all.
 */
int gslt_count_font_encodings(gslt_font_t *font);

/*
 * Identify the platform and encoding id of a cmap subtable.
 */
int gslt_identify_font_encoding(gslt_font_t *font, int idx, int *pid, int *eid);

/*
 * Select a cmap subtable for use with the gslt_encode_font_char() function.
 */
int gslt_select_font_encoding(gslt_font_t *font, int idx);

/*
 * Encode a character and return its glyph index.
 * Glyph 0 is always the "undefined" character.
 * Any unencoded characters encoded will map to glyph 0.
 *
 * Do not call this without having selected an encoding first.
 */
int gslt_encode_font_char(gslt_font_t *font, int key);

/*
 * Walk the outline of a glyph, calling the functions specified.
 */
int gslt_outline_font_glyph(gs_state *pgs, gslt_font_t *font, int gid, gslt_outline_walker_t *walker);

/*
 * Render a glyph using a transform matrix, and fill in the
 * bitmap struct. All values are in device space.
 *
 * The data pointer will point into the internal
 * glyph bitmap cache in ghostscript, treat it as read only.
 *
 * Return -1 on failure.
 */
int gslt_render_font_glyph(gs_state *pgs, gslt_font_t *font, gs_matrix *tm, int gid, gslt_glyph_bitmap_t *slot);

/*
 * The glyph bitmap data is reference counted in the cache.
 * gslt_render_font_glyph will increase the reference count by one
 * automatically. Use the following functions to increase and decrease
 * the reference count manually. You must call gslt_release_font_glyph
 * when you don't need the bitmap anymore.
 */
void gslt_retain_font_glyph(gs_memory_t *mem, gslt_glyph_bitmap_t *slot);
void gslt_release_font_glyph(gs_memory_t *mem, gslt_glyph_bitmap_t *slot);

/*
 * Fill in the metrics for a glyph, scaled by the internal font matrix.
 * The design space (usually 1000 or 2048 for an em) is mapped to 1.0.
 *
 * Return -1 on failure.
 */
int gslt_measure_font_glyph(gs_state *pgs, gslt_font_t *font, int gid, gslt_glyph_metrics_t *mtx);
