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


/* SVG interpreter - combined internal header */

#include "memory_.h"
#include "math_.h"

#include <stdlib.h>
#include <ctype.h> /* for toupper() */

#include "gsgc.h"
#include "gstypes.h"
#include "gsstate.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "gsdevice.h"
#include "scommon.h"
#include "gserrors.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsimage.h"
#include "gscspace.h"
#include "gsptype1.h"
#include "gscolor2.h"
#include "gscolor3.h"
#include "gsutil.h"
#include "gsicc.h"

#include "gstrans.h"

#include "gxpath.h"     /* gsshade.h depends on it */
#include "gxfixed.h"    /* gsshade.h depends on it */
#include "gxmatrix.h"	/* gxtype1.h depends on it */
#include "gsshade.h"
#include "gsfunc.h"
#include "gsfunc3.h"    /* we use stitching and exponential interp */

#include "gxfont.h"
#include "gxchar.h"
#include "gxtype1.h"
#include "gxfont1.h"
#include "gxfont42.h"
#include "gxfcache.h"
#include "gxistate.h"

#include "gzstate.h"
#include "gzpath.h"

#include "zlib.h"

/* override the debug printfs */
#ifndef DEBUG
#undef _dpl
#define _dpl
#undef dpf
#define dpf
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif
#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif

/*
 * Forward declarations.
 */

typedef struct svg_context_s svg_context_t;
typedef struct svg_item_s svg_item_t;

/*
 * Context and memory.
 */

#define svg_alloc(ctx, size) \
    ((void*)gs_alloc_bytes(ctx->memory, size, __func__));
#define svg_realloc(ctx, ptr, size) \
    gs_resize_object(ctx->memory, ptr, size, __func__);
#define svg_strdup(ctx, str) \
    svg_strdup_imp(ctx, str, __func__);
#define svg_free(ctx, ptr) \
    gs_free_object(ctx->memory, ptr, __func__);

size_t svg_strlcpy(char *destination, const char *source, size_t size);
size_t svg_strlcat(char *destination, const char *source, size_t size);

char *svg_strdup_imp(svg_context_t *ctx, const char *str, const char *function);
char *svg_clean_path(char *name);
void svg_absolute_path(char *output, char *pwd, char *path);

/* end of page device callback foo */
int svg_show_page(svg_context_t *ctx, int num_copies, int flush);

int svg_utf8_to_ucs(int *p, const char *s, int n);

/*
 * Parse basic data type units.
 */

float svg_parse_length(char *str, float percent, float font_size);
float svg_parse_angle(char *str);

int svg_parse_color(char *str, float *rgb);

int svg_is_whitespace_or_comma(int c);
int svg_is_whitespace(int c);
int svg_is_alpha(int c);
int svg_is_digit(int c);

/*
 * Graphics content parsing.
 */

int svg_parse_common(svg_context_t *ctx, svg_item_t *node);

int svg_parse_transform(svg_context_t *ctx, char *str);

void svg_set_color(svg_context_t *ctx, float *rgb);
void svg_set_fill_color(svg_context_t *ctx);
void svg_set_stroke_color(svg_context_t *ctx);

int svg_parse_image(svg_context_t *ctx, svg_item_t *node);

int svg_parse_path(svg_context_t *ctx, svg_item_t *node);

int svg_parse_rect(svg_context_t *ctx, svg_item_t *node);
int svg_parse_circle(svg_context_t *ctx, svg_item_t *node);
int svg_parse_ellipse(svg_context_t *ctx, svg_item_t *node);
int svg_parse_line(svg_context_t *ctx, svg_item_t *node);
int svg_parse_polyline(svg_context_t *ctx, svg_item_t *node);
int svg_parse_polygon(svg_context_t *ctx, svg_item_t *node);

int svg_parse_text(svg_context_t *ctx, svg_item_t *node);

/*
 * Global context and XML parsing.
 */

int svg_parse_element(svg_context_t *ctx, svg_item_t *root);

int svg_open_xml_parser(svg_context_t *ctx);
int svg_feed_xml_parser(svg_context_t *ctx, const char *buf, int len);
svg_item_t * svg_close_xml_parser(svg_context_t *ctx);
int svg_parse_document(svg_context_t *ctx, svg_item_t *root);

svg_item_t * svg_next(svg_item_t *item);
svg_item_t * svg_down(svg_item_t *item);
void svg_free_item(svg_context_t *ctx, svg_item_t *item);
char * svg_tag(svg_item_t *item);
char * svg_att(svg_item_t *item, const char *att);
void svg_debug_item(svg_item_t *item, int level);

struct svg_item_s
{
    char *name;
    char **atts;
    svg_item_t *up;
    svg_item_t *down;
    svg_item_t *next;
};

struct svg_context_s
{
    void *instance;
    gs_memory_t *memory;
    gs_state *pgs;
    gs_font_dir *fontdir;
    gs_color_space *srgb;

    int fill_rule;

    int fill_is_set;
    float fill_color[3];

    int stroke_is_set;
    float stroke_color[3];

    float viewbox_width;
    float viewbox_height;
    float viewbox_size;

    svg_item_t *root;
    svg_item_t *head;
    const char *error;
    void *parser; /* Expat XML_Parser */
};
