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

void *xps_alloc(xps_context_t *ctx, int size);
void *xps_realloc(xps_context_t *ctx, void *ptr, int size);
void xps_free(xps_context_t *ctx, void *ptr);
char *xps_strdup(xps_context_t *ctx, const char *str);

int xps_process_data(xps_context_t *ctx, stream_cursor_read *buf);
int xps_process_part(xps_context_t *ctx, xps_part_t *part);

struct xps_type_map_s
{
    char *name;
    char *type;
    xps_type_map_t *next;
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
    byte *data;
    xps_relation_t *rels;
    xps_part_t *next;
};

struct xps_relation_s
{
    char *source;
    char *type;
    char *target;
    xps_relation_t *next;
};

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

