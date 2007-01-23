#include "memory_.h"
#include "gsgc.h"
#include "scommon.h"
#include "gsstate.h"
#include "gserror.h"
#include "gserrors.h"
#include "gspaint.h"

#include "zlib.h"

typedef struct xps_context_s xps_context_t;
typedef struct xps_part_s xps_part_t;
typedef struct xps_type_map_s xps_type_map_t;
typedef struct xps_relation_s xps_relation_t;

#define xps_alloc(ctx, size) \
    ((void*)gs_alloc_bytes(ctx->memory, size, __FUNCTION__));
#define xps_realloc(ctx, ptr, size) \
    gs_resize_object(ctx->memory, ptr, size, __FUNCTION__);
#define xps_strdup(ctx, str) \
    xps_strdup_imp(ctx, str, __FUNCTION__);
#define xps_free(ctx, ptr) \
    gs_free_object(ctx->memory, ptr, __FUNCTION__);

int xps_process_data(xps_context_t *ctx, stream_cursor_read *buf);
int xps_process_part(xps_context_t *ctx, xps_part_t *part);

char *xps_strdup_imp(xps_context_t *ctx, const char *str, const char *function);
char *xps_clean_path(char *name);

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

struct xps_context_s
{
    void *instance;
    gs_memory_t *memory;
    gs_state *pgs;

    xps_part_t *root;
    xps_part_t *last;

    xps_type_map_t *defaults;
    xps_type_map_t *overrides;

    unsigned int zip_state;
    unsigned int zip_general;
    unsigned int zip_method;
    unsigned int zip_name_length;
    unsigned int zip_extra_length;
    unsigned int zip_compressed_size;
    unsigned int zip_uncompressed_size;
    z_stream zip_stream;
    char zip_file_name[2048];
};

struct xps_part_s
{
    char *name;
    int size;
    int capacity;
    int complete;
    char *data;
    xps_relation_t *relations;
    xps_part_t *next;
};

xps_part_t * xps_new_part(xps_context_t *ctx, char *name, int capacity);
void xps_free_part(xps_context_t *ctx, xps_part_t *part);
int xps_add_relation(xps_context_t *ctx, char *source, char *target, char *type);

typedef struct xps_parser_s xps_parser_t;
typedef struct xps_item_s xps_item_t;
typedef struct xps_mark_s xps_mark_t;

struct xps_mark_s
{
    /* this is opaque state to save/restore the parser location */
    xps_item_t *head;
    int downed;
    int nexted;
};

xps_parser_t * xps_new_parser(xps_context_t *ctx, char *buf, int len);
void xps_free_parser(xps_parser_t *parser);
xps_item_t * xps_next(xps_parser_t *parser);
void xps_down(xps_parser_t *parser);
void xps_up(xps_parser_t *parser);
char * xps_tag(xps_item_t *item);
char * xps_att(xps_item_t *item, char *att);
xps_mark_t xps_mark(xps_parser_t *parser);
void xps_goto(xps_parser_t *parser, xps_mark_t mark);

