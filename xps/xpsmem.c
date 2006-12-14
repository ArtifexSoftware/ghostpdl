#include "ghostxps.h"

void *
xps_alloc(xps_context_t *ctx, int size)
{
    return (void*)gs_alloc_bytes(ctx->memory, size, "xps_alloc");
}

void *
xps_realloc(xps_context_t *ctx, void *ptr, int size)
{
    return gs_resize_object(ctx->memory, ptr, size, "xps_alloc");
}

void
xps_free(xps_context_t *ctx, void *ptr)
{
    gs_free_object(ctx->memory, ptr, "xps_alloc");
}

char *
xps_strdup(xps_context_t *ctx, const char *str)
{
    char *cpy = NULL;
    if (str)
	cpy = xps_alloc(ctx, strlen(str) + 1);
    if (cpy)
	strcpy(cpy, str);
    return cpy;
}

