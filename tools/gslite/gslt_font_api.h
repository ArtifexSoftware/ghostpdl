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

/* $Id: gslt_font_api.h 2460 2006-07-14 18:49:12Z giles $ */
/* gslt OpenType font library api */

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
    float v[2];                /* origin offset (used for vertical wmode) */
    float w[2];                /* advance width */
};

struct gslt_glyph_bitmap_s
{
    float xadv, yadv;           /* pen advance */
    int w, h, stride;           /* bitmap size; stride is number of bytes per line */
    int lsb, top;               /* bitmap offset (left-side-bearing and top) */
    unsigned char *data;        /* pointer to (temporary) cache memory */
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
 * Create and initialize a font structure.
 * This retains a pointer to the memory passed in,
 * but does not assume ownership. Free the memory
 * after freeing the font.
 * Return NULL on failure.
 */
gslt_font_t * gslt_new_font(gs_memory_t *mem, gs_font_dir *cache, char *rbuf, int rlen, int wmode);

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
 * glyph bitmap cache in ghostscript.
 * This data should be considered volatile;
 * it may disappear whenever another function
 * from this API or ghostscript is called.
 *
 * If you need to hold on to it, make your own copy.
 *
 * Return -1 on failure.
 */
int gslt_render_font_glyph(gs_state *pgs, gslt_font_t *font, gs_matrix *tm, int gid, gslt_glyph_bitmap_t *slot);

/*
 * Fill in the metrics for a glyph, scaled by the internal font matrix.
 * The design space (usually 1000 or 2048 for an em) is mapped to 1.0.
 *
 * Return -1 on failure.
 */
int gslt_measure_font_glyph(gs_state *pgs, gslt_font_t *font, int gid, gslt_glyph_metrics_t *mtx);
