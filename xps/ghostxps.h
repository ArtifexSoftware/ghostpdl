#include <stdlib.h>

#include "memory_.h"
#include "gsgc.h"
#include "gstypes.h"
#include "gsstate.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "gsdevice.h"
#include "scommon.h"
#include "gserror.h"
#include "gserrors.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsimage.h"
#include "gscspace.h"

#include "zlib.h"

/*
 * Context and memory.
 */

typedef struct xps_context_s xps_context_t;

#define xps_alloc(ctx, size) \
    ((void*)gs_alloc_bytes(ctx->memory, size, __FUNCTION__));
#define xps_realloc(ctx, ptr, size) \
    gs_resize_object(ctx->memory, ptr, size, __FUNCTION__);
#define xps_strdup(ctx, str) \
    xps_strdup_imp(ctx, str, __FUNCTION__);
#define xps_free(ctx, ptr) \
    gs_free_object(ctx->memory, ptr, __FUNCTION__);

char *xps_strdup_imp(xps_context_t *ctx, const char *str, const char *function);
char *xps_clean_path(char *name);
void xps_absolute_path(char *output, char *pwd, char *path);

/* end of page device callback foo */
int xps_show_page(xps_context_t *ctx, int num_copies, int flush);

/*
 * Packages, parts and relations.
 */

typedef struct xps_part_s xps_part_t;
typedef struct xps_type_map_s xps_type_map_t;
typedef struct xps_relation_s xps_relation_t;
typedef struct xps_document_s xps_document_t;
typedef struct xps_page_s xps_page_t;

int xps_process_data(xps_context_t *ctx, stream_cursor_read *buf);
int xps_process_part(xps_context_t *ctx, xps_part_t *part);

struct xps_type_map_s
{
    char *name;
    char *type;
    xps_type_map_t *left;
    xps_type_map_t *right;
};

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

struct xps_context_s
{
    void *instance;
    gs_memory_t *memory;
    gs_state *pgs;

    xps_part_t *first_part;
    xps_part_t *last_part;

    xps_type_map_t *defaults;
    xps_type_map_t *overrides;

    char *start_part; /* fixed document sequence */
    xps_document_t *first_fixdoc; /* first fixed document */
    xps_document_t *last_fixdoc; /* last fixed document */

    xps_page_t *first_page; /* first page of document */
    xps_page_t *last_page; /* last page of document */
    xps_page_t *next_page; /* next page to process when its resources are completed */

    unsigned int zip_state;
    unsigned int zip_general;
    unsigned int zip_method;
    unsigned int zip_name_length;
    unsigned int zip_extra_length;
    unsigned int zip_compressed_size;
    unsigned int zip_uncompressed_size;
    z_stream zip_stream;
    char zip_file_name[2048];

    char pwd[1024]; /* directory name of xml part being processed */
    char *state; /* temporary state for various processing */
};

struct xps_part_s
{
    char *name;
    int size;
    int capacity;
    int complete;
    char *data;
    xps_relation_t *relations;
    int relations_complete; /* is corresponding .rels part finished? */
    xps_part_t *next;
};

xps_part_t *xps_new_part(xps_context_t *ctx, char *name, int capacity);
xps_part_t *xps_find_part(xps_context_t *ctx, char *name);
void xps_free_part(xps_context_t *ctx, xps_part_t *part);

int xps_add_relation(xps_context_t *ctx, char *source, char *target, char *type);

void xps_free_type_map(xps_context_t *ctx, xps_type_map_t *node);
void xps_free_relations(xps_context_t *ctx, xps_relation_t *node);
void xps_free_fixed_pages(xps_context_t *ctx);
void xps_free_fixed_documents(xps_context_t *ctx);

/*
 * Various resources.
 */

/* type for the information derived directly from the raster file format */

enum { XPS_GRAY, XPS_GRAY_A, XPS_RGB, XPS_RGB_A, XPS_CMYK, XPS_CMYK_A };

typedef struct xps_image_s xps_image_t;

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
};

int xps_decode_jpeg(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);
int xps_decode_png(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);
int xps_decode_tiff(gs_memory_t *mem, byte *rbuf, int rlen, xps_image_t *image);

/*
 * XML and content.
 */

typedef struct xps_item_s xps_item_t;

xps_item_t * xps_parse_xml(xps_context_t *ctx, char *buf, int len);
xps_item_t * xps_next(xps_item_t *item);
xps_item_t * xps_down(xps_item_t *item);
void xps_free_item(xps_context_t *ctx, xps_item_t *item);
char * xps_tag(xps_item_t *item);
char * xps_att(xps_item_t *item, const char *att);

int xps_parse_fixed_page(xps_context_t *ctx, xps_part_t *part);
int xps_parse_canvas(xps_context_t *ctx, xps_item_t *node);
int xps_parse_path(xps_context_t *ctx, xps_item_t *node);
int xps_parse_glyphs(xps_context_t *ctx, xps_item_t *node);
int xps_parse_image_brush(xps_context_t *ctx, xps_item_t *node);
int xps_parse_visual_brush(xps_context_t *ctx, xps_item_t *node);
int xps_parse_linear_gradient_brush(xps_context_t *ctx, xps_item_t *node);
int xps_parse_radial_gradient_brush(xps_context_t *ctx, xps_item_t *node);

void xps_parse_matrix_transform(xps_context_t *ctx, xps_item_t *root, gs_matrix *matrix);
void xps_parse_render_transform(xps_context_t *ctx, char *text, gs_matrix *matrix);
void xps_parse_color(xps_context_t *ctx, char *hexstring, float *argb);
void xps_parse_rectangle(xps_context_t *ctx, char *text, gs_rect *rect);
int xps_parse_abbreviated_geometry(xps_context_t *ctx, char *geom);

