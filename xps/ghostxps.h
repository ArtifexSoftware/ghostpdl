/* Copyright (C) 2006-2008 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* combined internal header for the XPS interpreter */

#include "memory_.h"
#include "math_.h"

#include <stdlib.h>
#include <ctype.h> /* for toupper() */

#include "gp.h"

#include "gsgc.h"
#include "gstypes.h"
#include "gsstate.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "gsdevice.h"
#include "scommon.h"
#include "gdebug.h"
#include "gserror.h"
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
 * Conditional compile time options.
 */
#define noXPS_LOAD_TYPE_MAPS

/*
 * XPS and ZIP strings
 */

#define REL_START_PART "http://schemas.microsoft.com/xps/2005/06/fixedrepresentation"
#define REL_REQUIRED_RESOURCE "http://schemas.microsoft.com/xps/2005/06/required-resource"
#define REL_REQUIRED_RESOURCE_RECURSIVE "http://schemas.microsoft.com/xps/2005/06/required-resource#recursive"

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_DATA_DESC_SIG 0x08074b50
#define ZIP_CENTRAL_DIRECTORY_SIG 0x02014b50
#define ZIP_END_OF_CENTRAL_DIRECTORY_SIG 0x06054b50

/*
 * Forward declarations.
 */

typedef struct xps_context_s xps_context_t;

typedef struct xps_entry_s xps_entry_t;
typedef struct xps_part_s xps_part_t;
typedef struct xps_relation_s xps_relation_t;
typedef struct xps_document_s xps_document_t;
typedef struct xps_page_s xps_page_t;

typedef struct xps_item_s xps_item_t;
typedef struct xps_font_s xps_font_t;
typedef struct xps_image_s xps_image_t;
typedef struct xps_resource_s xps_resource_t;
typedef struct xps_glyph_metrics_s xps_glyph_metrics_t;

/*
 * Context and memory.
 */

extern int xps_zip_trace;
extern int xps_doc_trace;

void * xps_realloc_imp(xps_context_t *ctx, void *ptr, int size, const char *func);

#define xps_alloc(ctx, size) \
    ((void*)gs_alloc_bytes(ctx->memory, size, __func__))
#define xps_realloc(ctx, ptr, size) \
    xps_realloc_imp(ctx, ptr, size, __func__)
#define xps_strdup(ctx, str) \
    xps_strdup_imp(ctx, str, __func__)
#define xps_free(ctx, ptr) \
    gs_free_object(ctx->memory, ptr, __func__)

size_t xps_strlcpy(char *destination, const char *source, size_t size);
size_t xps_strlcat(char *destination, const char *source, size_t size);

int xps_strcasecmp(char *a, char *b);
char *xps_strdup_imp(xps_context_t *ctx, const char *str, const char *function);
char *xps_clean_path(char *name);
void xps_absolute_path(char *output, char *base_uri, char *path);

/* end of page device callback foo */
int xps_show_page(xps_context_t *ctx, int num_copies, int flush);

int xps_utf8_to_ucs(int *p, const char *s, int n);

unsigned int xps_crc32(unsigned int crc, unsigned char *buf, int n);

typedef struct xps_hash_table_s xps_hash_table_t;
xps_hash_table_t *xps_hash_new(xps_context_t *ctx);
void xps_hash_free(xps_context_t *ctx, xps_hash_table_t *table);
void *xps_hash_lookup(xps_hash_table_t *table, char *key);
int xps_hash_insert(xps_context_t *ctx, xps_hash_table_t *table, char *key, void *value);
void xps_hash_debug(xps_hash_table_t *table);

/*
 * Packages, parts and relations.
 */

/* Process all of a seekable file at once */
int xps_process_file(xps_context_t *ctx, FILE *file);

/* Process one cursor full of data at a time */
int xps_process_data(xps_context_t *ctx, stream_cursor_read *buf);
int xps_process_end_of_data(xps_context_t *ctx);

struct xps_relation_s
{
    char *target;
    char *type;
    xps_relation_t *next;
};

struct xps_document_s
{
    char *name;
    xps_document_t *next;
};

struct xps_page_s
{
    char *name;
    int width;
    int height;
    xps_page_t *next;
};

struct xps_entry_s
{
    char *name;
    int offset;
    int csize;
    int usize;
};

struct xps_context_s
{
    void *instance;
    gs_memory_t *memory;
    gs_state *pgs;
    gs_font_dir *fontdir;

    gs_color_space *gray;
    gs_color_space *srgb;
    gs_color_space *scrgb;
    gs_color_space *cmyk;

    xps_hash_table_t *part_table;
    xps_part_t *part_list;

    char *start_part; /* fixed document sequence */
    xps_document_t *first_fixdoc; /* first fixed document */
    xps_document_t *last_fixdoc; /* last fixed document */
    xps_page_t *first_page; /* first page of document */
    xps_page_t *last_page; /* last page of document */

    char *base_uri; /* base uri for parsing XML and resolving relative paths */
    char *part_uri; /* part uri for parsing metadata relations */

    /* Seek mode state: */

    FILE *file;
    int zip_count;
    xps_entry_t *zip_table;

    /* Feed mode state: */

    unsigned int zip_state;
    unsigned int zip_version;
    unsigned int zip_general;
    unsigned int zip_method;
    unsigned int zip_name_length;
    unsigned int zip_extra_length;
    unsigned int zip_compressed_size;
    unsigned int zip_uncompressed_size;
    z_stream zip_stream;
    char zip_file_name[2048];

    xps_page_t *next_page; /* next page to process when its resources are completed */
    xps_part_t *current_part; /* part for the current zip entry being decompressed */

    /* Graphics context state: */

    int use_transparency; /* global toggle for transparency */

    /* Hack to workaround ghostscript's lack of understanding
     * the pdf 1.4 specification of Alpha only transparency groups.
     * We have to force all colors to be grayscale whenever we are computing
     * opacity masks.
     */
    int opacity_only;

    /* The fill_rule is set by path parsing.
     * It is used by clip/fill functions.
     * 1=nonzero, 0=evenodd
     */
    int fill_rule;

    /* We often need the bounding box for the current
     * area of the page affected by drawing operations.
     * We keep these bounds updated every time we
     * clip. The coordinates are in device space.
     */
    gs_rect bounds;
};

struct xps_part_s
{
    char *name;
    int size;
    int interleave; /* start of last interleave (for crc32 reasons) */
    int capacity;
    int complete;
    byte *data;
    xps_relation_t *relations;
    int relations_complete; /* is corresponding .rels part finished? */

    /* Some resource types have to persist across the lifetime of a job. */
    xps_font_t *font; /* parsed font resource */
    gs_color_space *icc; /* parsed icc profile resource */

    int deobfuscated; /* have we deobfuscated the font data? */

    xps_part_t *next;
};

xps_part_t *xps_read_zip_part(xps_context_t *ctx, char *name);

void xps_part_name_from_relation_part_name(char *output, char *name);

xps_part_t *xps_new_part(xps_context_t *ctx, char *name, int capacity);
xps_part_t *xps_find_part(xps_context_t *ctx, char *name);
xps_part_t *xps_read_part(xps_context_t *ctx, char *name);
void xps_free_part(xps_context_t *ctx, xps_part_t *part);
void xps_free_part_caches(xps_context_t *ctx, xps_part_t *part);
void xps_free_part_data(xps_context_t *ctx, xps_part_t *part);
void xps_release_part(xps_context_t *ctx, xps_part_t *part);

void xps_debug_item(xps_item_t *item, int level);

int xps_add_relation(xps_context_t *ctx, char *source, char *target, char *type);

void xps_free_relations(xps_context_t *ctx, xps_relation_t *node);
void xps_free_fixed_pages(xps_context_t *ctx);
void xps_free_fixed_documents(xps_context_t *ctx);

void xps_debug_parts(xps_context_t *ctx);
void xps_debug_fixdocseq(xps_context_t *ctx);

/*
 * Various resources.
 */

/* type for the information derived directly from the raster file format */

enum { XPS_GRAY, XPS_GRAY_A, XPS_RGB, XPS_RGB_A, XPS_CMYK, XPS_CMYK_A };

struct xps_image_s
{
    int width;
    int height;
    int stride;
    int colorspace;
    int comps;
    int bits;
    int xres;
    int yres;
    byte *samples;
    byte *alpha; /* isolated alpha plane */
};

int xps_decode_jpeg(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);
int xps_decode_png(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);
int xps_decode_tiff(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);
int xps_decode_hdphoto(gs_memory_t *mem, byte *buf, int len, xps_image_t *image);

/*
 * Fonts.
 */

struct xps_font_s
{
    byte *data;
    int length;
    gs_font *font;

    int subfontid;
    int cmaptable;
    int cmapsubcount;
    int cmapsubtable;
    int usepua;

    /* these are for CFF opentypes only */
    byte *cffdata;
    byte *cffend;
    byte *gsubrs;
    byte *subrs;
    byte *charstrings;
};

struct xps_glyph_metrics_s
{
    float hadv, vadv, vorg;
};


int xps_init_font_cache(xps_context_t *ctx);

xps_font_t *xps_new_font(xps_context_t *ctx, byte *buf, int buflen, int index);
void xps_free_font(xps_context_t *ctx, xps_font_t *font);

int xps_count_font_encodings(xps_font_t *font);
int xps_identify_font_encoding(xps_font_t *font, int idx, int *pid, int *eid);
int xps_select_font_encoding(xps_font_t *font, int idx);
int xps_encode_font_char(xps_font_t *font, int key);

int xps_measure_font_glyph(xps_context_t *ctx, xps_font_t *font, int gid, xps_glyph_metrics_t *mtx);
int xps_draw_font_glyph_to_path(xps_context_t *ctx, xps_font_t *font, int gid, float x, float y);
int xps_fill_font_glyph(xps_context_t *ctx, xps_font_t *font, int gid, float x, float y);

int xps_find_sfnt_table(xps_font_t *font, char *name, int *lengthp);
int xps_load_sfnt_cmap(xps_font_t *font);
int xps_load_sfnt_name(xps_font_t *font, char *namep);
int xps_init_truetype_font(xps_context_t *ctx, xps_font_t *font);
int xps_init_postscript_font(xps_context_t *ctx, xps_font_t *font);

void xps_debug_path(xps_context_t *ctx);

/*
 * XML and content.
 */

xps_item_t * xps_parse_xml(xps_context_t *ctx, byte *buf, int len);
xps_item_t * xps_next(xps_item_t *item);
xps_item_t * xps_down(xps_item_t *item);
void xps_free_item(xps_context_t *ctx, xps_item_t *item);
char * xps_tag(xps_item_t *item);
char * xps_att(xps_item_t *item, char *att);

int xps_parse_content_relations(xps_context_t *ctx, xps_part_t *part);
int xps_parse_metadata(xps_context_t *ctx, xps_part_t *part);
int xps_parse_fixed_page(xps_context_t *ctx, xps_part_t *part);
int xps_parse_canvas(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_path(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_glyphs(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_solid_color_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_image_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_visual_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_linear_gradient_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_radial_gradient_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);

void xps_free_image(xps_context_t *ctx, xps_image_t *image);

int xps_parse_tiling_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *root,
	int (*func)(xps_context_t*, char*, xps_resource_t*, xps_item_t*, void*), void *user);

void xps_parse_matrix_transform(xps_context_t *ctx, xps_item_t *root, gs_matrix *matrix);
void xps_parse_render_transform(xps_context_t *ctx, char *text, gs_matrix *matrix);
void xps_parse_rectangle(xps_context_t *ctx, char *text, gs_rect *rect);
int xps_parse_abbreviated_geometry(xps_context_t *ctx, char *geom);
int xps_parse_path_geometry(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root, int stroking);

int xps_begin_opacity(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, char *opacity_att, xps_item_t *opacity_mask_tag);
int xps_end_opacity(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, char *opacity_att, xps_item_t *opacity_mask_tag);

int xps_parse_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);
int xps_parse_element(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node);

int xps_clip(xps_context_t *ctx, gs_rect *saved_bounds);
int xps_unclip(xps_context_t *ctx, gs_rect *saved_bounds);
int xps_fill(xps_context_t *ctx);
void xps_bounds_in_user_space(xps_context_t *ctx, gs_rect *user);

int xps_parse_color(xps_context_t *ctx, char *base_uri, char *hexstring, gs_color_space **csp, float *samples);
int xps_set_color(xps_context_t *ctx, gs_color_space *colorspace, float *samples);
int xps_parse_icc_profile(xps_context_t *ctx, gs_color_space **csp, byte *data, int length, int ncomp);

int xps_element_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *node);
int xps_resource_dictionary_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *node);
int xps_image_brush_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root);


/*
 * Static XML resources.
 */

struct xps_resource_s
{
    char *name;
    char *base_uri; /* only used in the head nodes */
    xps_item_t *base_xml; /* only used in the head nodes, to free the xml document */
    xps_item_t *data;
    xps_resource_t *next;
    xps_resource_t *parent; /* up to the previous dict in the stack */
};

xps_resource_t *xps_parse_remote_resource_dictionary(xps_context_t *ctx, char *base_uri, char *name);
xps_resource_t *xps_parse_resource_dictionary(xps_context_t *ctx, char *base_uri, xps_item_t *root);
void xps_free_resource_dictionary(xps_context_t *ctx, xps_resource_t *dict);
int xps_resolve_resource_reference(xps_context_t *ctx, xps_resource_t *dict, char **attp, xps_item_t **tagp, char **urip);

void xps_debug_resource_dictionary(xps_resource_t *dict);
