#include "memory_.h"
#include "gsgc.h"
#include "scommon.h"
#include "gsstate.h"
#include "gserror.h"
#include "gserrors.h"
#include "gspaint.h"

#include "zlib.h"

typedef struct xps_context_s xps_context_t;
typedef struct xps_zip_part_s xps_zip_part_t;

struct xps_context_s
{
    void *instance;
    gs_memory_t *memory;
    gs_state *pgs;

    xps_zip_part_t *root;
    xps_zip_part_t *last;

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

void *xps_alloc(xps_context_t *ctx, int size);
void *xps_realloc(xps_context_t *ctx, void *ptr, int size);
void xps_free(xps_context_t *ctx, void *ptr);

char *xps_strdup(xps_context_t *ctx, const char *str);

int xps_process_data(xps_context_t *ctx, stream_cursor_read *buf);

