#include "ghostxps.h"


char *
xps_strdup_imp(xps_context_t *ctx, const char *str, const char *cname)
{
    char *cpy = NULL;
    if (str)
	cpy = gs_alloc_bytes(ctx->memory, strlen(str) + 1, cname);
    if (cpy)
	strcpy(cpy, str);
    return cpy;
}

