#include "ghostxps.h"

#include <expat.h>

/*
 * Parse incomplete Content_Type and relationship parts and add entries
 * to the type map lists and relation ship maps.
 */

static inline int
xps_compare_relation(xps_relation_t *a, xps_relation_t *b)
{
    int cmp = strcmp(a->source, b->source);
    if (cmp == 0)
	cmp = strcmp(a->target, b->target);
    return cmp;
}

void xps_debug_type_map(xps_context_t *ctx, char *label, xps_type_map_t *node)
{
    if (node)
    {
	if (node->left)
	    xps_debug_type_map(ctx, label, node->left);
	dprintf3("%s name=%s type=%s\n", label, node->name, node->type);
	if (node->right)
	    xps_debug_type_map(ctx, label, node->right);
    }
}

static xps_type_map_t *
xps_new_type_map(xps_context_t *ctx, char *name, char *type)
{
    xps_type_map_t *node;

    node = xps_alloc(ctx, sizeof(xps_type_map_t));
    if (!node)
	goto cleanup;

    node->name = xps_strdup(ctx, name);
    node->type = xps_strdup(ctx, type);
    node->left = NULL;
    node->right = NULL;

    if (!node->name)
	goto cleanup;
    if (!node->type)
	goto cleanup;

    return node;

cleanup:
    if (node)
    {
	if (node->name) xps_free(ctx, node->name);
	if (node->type) xps_free(ctx, node->type);
	xps_free(ctx, node);
    }
    return NULL;
}

static void
xps_add_type_map(xps_context_t *ctx, xps_type_map_t *node, char *name, char *type)
{
    int cmp = strcmp(node->name, name);
    if (cmp < 0)
    {
	if (node->left)
	    xps_add_type_map(ctx, node->left, name, type);
	else
	    node->left = xps_new_type_map(ctx, name, type);
    }
    else if (cmp > 0)
    {
	if (node->right)
	    xps_add_type_map(ctx, node->right, name, type);
	else
	    node->right = xps_new_type_map(ctx, name, type);
    }
    else
    {
	/* it's a duplicate so we don't do anything */
    }
}

static void
xps_add_override(xps_context_t *ctx, char *part_name, char *content_type)
{
    /* dprintf2("Override part=%s type=%s\n", part_name, content_type); */
    if (ctx->overrides == NULL)
	ctx->overrides = xps_new_type_map(ctx, part_name, content_type);
    else
	xps_add_type_map(ctx, ctx->overrides, part_name, content_type);
}

static void
xps_add_default(xps_context_t *ctx, char *extension, char *content_type)
{
    /* dprintf2("Default extension=%s type=%s\n", extension, content_type); */
    if (ctx->defaults == NULL)
	ctx->defaults = xps_new_type_map(ctx, extension, content_type);
    else
	xps_add_type_map(ctx, ctx->defaults, extension, content_type);
}

static void
xps_add_relation(xps_context_t *ctx, char *source, char *target, char *type)
{
    dprintf3("Relation source=%s target=%s type=%s\n", source, target, type);
}

static void
xps_handle_metadata(void *zp, const char *name, const char **atts)
{
    xps_context_t *ctx = zp;
    int i;

    if (!strcmp(name, "Default"))
    {
	char *extension = NULL;
	char *type = NULL;

	for (i = 0; atts[i]; i += 2)
	{
	    if (!strcmp(atts[i], "Extension"))
		extension = atts[i + 1];
	    if (!strcmp(atts[i], "ContentType"))
		type = atts[i + 1];
	}

	if (extension && type)
	    xps_add_default(ctx, extension, type);
    }

    if (!strcmp(name, "Override"))
    {
	char *partname = NULL;
	char *type = NULL;

	for (i = 0; atts[i]; i += 2)
	{
	    if (!strcmp(atts[i], "PartName"))
		partname = atts[i + 1];
	    if (!strcmp(atts[i], "ContentType"))
		type = atts[i + 1];
	}

	if (partname && type)
	    xps_add_override(ctx, partname, type);
    }

    if (!strcmp(name, "Relationship"))
    {
	char source[1024];
	char *target = NULL;
	char *type = NULL;

	strcpy(source, ctx->last->name);

	for (i = 0; atts[i]; i += 2)
	{
	    if (!strcmp(atts[i], "Target"))
		target = atts[i + 1];
	    if (!strcmp(atts[i], "Type"))
		type = atts[i + 1];
	}

	if (target && type)
	    xps_add_relation(ctx, source, target, type);
    }
}

static int
xps_process_metadata(xps_context_t *ctx, xps_part_t *part)
{
    XML_Parser xp;

    xp = XML_ParserCreate(NULL);
    if (!xp)
	return -1;

    XML_SetUserData(xp, ctx);
    XML_SetParamEntityParsing(xp, XML_PARAM_ENTITY_PARSING_NEVER);
    XML_SetStartElementHandler(xp, xps_handle_metadata);

    (void) XML_Parse(xp, part->data, part->size, 1);

    XML_ParserFree(xp);

    return 0;
}

int
xps_process_fpage(xps_context_t *ctx, xps_part_t *part)
{
    xps_parser_t *parser;
    parser = xps_new_parser(ctx, part->data, part->size);
    if (!parser)
	return gs_rethrow(-1, "could not create xml parser");

//    xps_debug_parser(parser);

    return 0;
}

int
xps_process_part(xps_context_t *ctx, xps_part_t *part)
{
    if (strstr(part->name, "[Content_Types].xml"))
    {
	xps_process_metadata(ctx, part);
    }

    if (strstr(part->name, "_rels/"))
    {
	xps_process_metadata(ctx, part);
    }

    if (part->complete)
    {
	dprintf1("complete part '%s'.\n", part->name);
	if (strstr(part->name, ".fpage"))
	{
	    dprintf("  a page!\n");
	    xps_process_fpage(ctx, part);
	}
    }
    else
    {
	dprintf1("incomplete piece '%s'.\n", part->name);
    }

}

