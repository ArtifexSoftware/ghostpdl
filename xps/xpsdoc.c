#include "ghostxps.h"

#include <expat.h>

#define REL_START_PART "http://schemas.microsoft.com/xps/2005/06/fixedrepresentation"

#define CT_FIXDOC "application/vnd.ms-package.xps-fixeddocument+xml"
#define CT_FIXDOCSEQ "application/vnd.ms-package.xps-fixeddocumentsequence+xml"
#define CT_FIXPAGE "application/vnd.ms-package.xps-fixedpage+xml"

static void
xps_absolute_path(char *output, char *pwd, char *path)
{
    if (path[0] == '/')
    {
	strcpy(output, path);
    }
    else
    {
	strcpy(output, pwd);
	strcat(output, "/");
	strcat(output, path);
    }
    xps_clean_path(output);
}

/*
 * Content types are stored in two separate binary trees.
 * One lists Override entries, which map a part name to a type.
 * The other lists Default entries, which map a file extension to a type.
 */

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

    dprintf2("adding type map from %s to %s\n", name, type);

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

static char *
xps_find_override(xps_type_map_t *node, char *partname)
{
    if (node)
    {
	int cmp = strcmp(node->name, partname);
	if (cmp < 0)
	    return xps_find_override(node->left, partname);
	if (cmp == 0)
	    return node->type;
	if (cmp > 0)
	    return xps_find_override(node->right, partname);
    }
    return NULL;
}

static char *
xps_find_default(xps_type_map_t *node, char *extension)
{
    if (node)
    {
	int cmp = strcmp(node->name, extension);
	if (cmp < 0)
	    return xps_find_default(node->left, extension);
	if (cmp == 0)
	    return node->type;
	if (cmp > 0)
	    return xps_find_default(node->right, extension);
    }
    return NULL;
}

char *
xps_get_content_type(xps_context_t *ctx, char *partname)
{
    char *extension;
    char *type;

    dprintf1("getting type for part '%s'\n", partname);

    type = xps_find_override(ctx->overrides, partname);
    if (type)
	return type;

    extension = strrchr(partname, '.');
    if (extension)
	extension ++;

    type = xps_find_default(ctx->defaults, extension);
    if (type)
	return type;

    return NULL;
}

/*
 * Relationships are stored in a linked list hooked into the part which
 * is the source of the relation.
 */

void xps_debug_parts(xps_context_t *ctx)
{
    xps_part_t *part = ctx->first_part;
    xps_relation_t *rel;
    while (part)
    {
	dprintf2("part '%s' size=%d\n", part->name, part->size);
	for (rel = part->relations; rel; rel = rel->next)
	    dprintf2("     target=%s type=%s\n", rel->target, rel->type);
	part = part->next;
    }
}

int
xps_add_relation(xps_context_t *ctx, char *source, char *target, char *type)
{
    xps_relation_t *node;
    xps_part_t *part;

    /* dprintf3("Relation source=%s target=%s type=%s\n", source, target, type); */

    part = xps_find_part(ctx, source);

    /* No existing part found. Create a blank part and hook it in. */
    if (part == NULL)
    {
	part = xps_new_part(ctx, source, 0);
	if (!part)
	    return gs_rethrow(-1, "cannot create new part");
    }

    /* Check for duplicates. */
    for (node = part->relations; node; node = node->next)
	if (!strcmp(node->target, target))
	    return 0;

    node = xps_alloc(ctx, sizeof(xps_relation_t));
    if (!node)
	return gs_rethrow(-1, "cannot create new relation");

    node->target = xps_strdup(ctx, target);
    node->type = xps_strdup(ctx, type);
    if (!node->target || !node->type)
    {
	if (node->target) xps_free(ctx, node->target);
	if (node->type) xps_free(ctx, node->type);
	xps_free(ctx, node);
	return gs_rethrow(-1, "cannot copy relation strings");
    }

    node->next = part->relations;
    part->relations = node;

    return 0;
}

/*
 * <DocumentReference> -- fixdocseq
 * <PageContent> -- fixdoc
 *
 * TODO: We should really look at the root StartPart relationship
 * for the FixedDocumentSequence and follow the DocumentReferences
 * therein for the page sequence. For now, we'll cheat and read
 * any PageContent references in the order they are in the file.
 */

void xps_debug_fixdocseq(xps_context_t *ctx)
{
    xps_document_t *fixdoc = ctx->first_fixdoc;
    xps_page_t *page = ctx->first_page;

    if (ctx->start_part)
	dprintf1("start part %s\n", ctx->start_part);

    while (fixdoc)
    {
	dprintf1("fixdoc %s\n", fixdoc->name);
	fixdoc = fixdoc->next;
    }

    while (page)
    {
	dprintf3("page %s w=%d h=%d\n", page->name, page->width, page->height);
	page = page->next;
    }
}

int
xps_add_fixed_document(xps_context_t *ctx, char *name)
{
    xps_document_t *fixdoc;

    /* Check for duplicates first */
    for (fixdoc = ctx->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
	if (!strcmp(fixdoc->name, name))
	    return 0;

    dprintf1("adding fixdoc %s\n", name);

    fixdoc = xps_alloc(ctx, sizeof(xps_document_t));
    if (!fixdoc)
	return -1;

    fixdoc->name = xps_strdup(ctx, name);
    if (!fixdoc->name)
    {
	xps_free(ctx, fixdoc);
	return -1;
    }

    fixdoc->next = NULL;

    if (!ctx->first_fixdoc)
    {
	ctx->first_fixdoc = fixdoc;
	ctx->last_fixdoc = fixdoc;
    }
    else
    {
	ctx->last_fixdoc->next = fixdoc;
	ctx->last_fixdoc = fixdoc;
    }

    return 0;
}

int
xps_add_fixed_page(xps_context_t *ctx, char *name, int width, int height)
{
    xps_page_t *page;

    /* Check for duplicates first */
    for (page = ctx->first_page; page; page = page->next)
	if (!strcmp(page->name, name))
	    return 0;

    dprintf1("adding page %s\n", name);

    page = xps_alloc(ctx, sizeof(xps_page_t));
    if (!page)
	return -1;

    page->name = xps_strdup(ctx, name);
    if (!page->name)
    {
	xps_free(ctx, page);
	return -1;
    }

    page->width = width;
    page->height = height;
    page->next = NULL;

    if (!ctx->first_page)
    {
	ctx->first_page = page;
	ctx->last_page = page;
    }
    else
    {
	ctx->last_page->next = page;
	ctx->last_page = page;
    }

    /* first page, or we processed all the previous pages */
    if (ctx->next_page == NULL)
	ctx->next_page = page;

    return 0;
}

/*
 * Parse the metadata [Content_Types.xml] and _rels/XXX.rels parts.
 * These should be parsed eagerly as they are interleaved, so the
 * parsing needs to be able to cope with incomplete xml.
 *
 * We re-parse the part every time a new part of the piece comes in.
 * The binary trees in xps_context_t make sure that the information
 * is not duplicated (since the entries are often parsed many times).
 *
 * We hook up unique expat handlers for this, and ignore any expat
 * errors that occur.
 */

static void
xps_handle_metadata(void *zp, char *name, char **atts)
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
	char srcbuf[1024];
	char tgtbuf[1024];
	char dirbuf[1024];
	char *target = NULL;
	char *type = NULL;
	char *p;

	for (i = 0; atts[i]; i += 2)
	{
	    if (!strcmp(atts[i], "Target"))
		target = atts[i + 1];
	    if (!strcmp(atts[i], "Type"))
		type = atts[i + 1];
	}

	if (target && type)
	{
	    strcpy(srcbuf, ctx->last_part->name);
	    p = strstr(srcbuf, "_rels/");
	    if (p)
	    {
		*p = 0;
		strcat(srcbuf, p + 6);
	    }
	    p = strstr(srcbuf, ".rels");
	    if (p)
	    {
		*p = 0;
	    }

	    strcpy(dirbuf, srcbuf);
	    p = strrchr(dirbuf, '/');
	    if (p)
		p[1] = 0;

	    xps_absolute_path(tgtbuf, dirbuf, target);

	    xps_add_relation(ctx, srcbuf, tgtbuf, type);
	}
    }

    if (!strcmp(name, "DocumentReference"))
    {
	char *source = NULL;
	char srcbuf[1024];

	for (i = 0; atts[i]; i += 2)
	{
	    if (!strcmp(atts[i], "Source"))
		source = atts[i + 1];
	}

	if (source)
	{
	    xps_absolute_path(srcbuf, ctx->pwd, source);
	    xps_add_fixed_document(ctx, srcbuf);
	}
    }

    if (!strcmp(name, "PageContent"))
    {
	char *source = NULL;
	char srcbuf[1024];
	int width = 0;
	int height = 0;

	for (i = 0; atts[i]; i += 2)
	{
	    if (!strcmp(atts[i], "Source"))
		source = atts[i + 1];
	    if (!strcmp(atts[i], "Width"))
		width = atoi(atts[i + 1]);
	    if (!strcmp(atts[i], "Height"))
		height = atoi(atts[i + 1]);
	}

	if (source)
	{
	    xps_absolute_path(srcbuf, ctx->pwd, source);
	    xps_add_fixed_page(ctx, srcbuf, width, height);
	}
    }
}

static int
xps_process_metadata(xps_context_t *ctx, xps_part_t *part)
{
    XML_Parser xp;
    char *s;

    /* Save directory name part */
    strcpy(ctx->pwd, part->name);
    s = strrchr(ctx->pwd, '/');
    if (s)
	s[1] = 0;

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

/*
 * The document sequence bla bla bla.
 */

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
    xps_document_t *fixdoc;
    char *type;

    dprintf2("processing part '%s' (%s)\n", part->name, part->complete ? "final" : "piece");

    /*
     * These two are magic Open Packaging Convention names.
     */

    if (strstr(part->name, "[Content_Types].xml"))
    {
	xps_process_metadata(ctx, part);
    }

    if (strstr(part->name, "_rels/"))
    {
	xps_process_metadata(ctx, part);
    }

    /*
     * For the rest we need to track the relationships
     * and content-types given by the previous two types.
     *
     * We can't do anything until we have the relationship
     * for the start part.
     */

    if (!ctx->start_part)
    {
	xps_part_t *rootpart = xps_find_part(ctx, "/");
	if (rootpart)
	{
	    xps_relation_t *rel;
	    for (rel = rootpart->relations; rel; rel = rel->next)
	    {
		if (!strcmp(rel->type, REL_START_PART))
		{
		    ctx->start_part = rel->target;
		    dprintf1("milestone: we have a start part '%s'\n", ctx->start_part);
		}
	    }
	}
    }

    /*
     * Read the start part (which is a FixedDocumentSequence) if it
     * is the current part.
     */

    if (ctx->start_part)
    {
	if (!strcmp(part->name, ctx->start_part))
	{
	    xps_process_metadata(ctx, part);
	}
    }

    /*
     * Follow the FixedDocumentSequence and parse the
     * listed FixedDocuments that we have available.
     */

    for (fixdoc = ctx->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
    {
	xps_part_t *fixdocpart = xps_find_part(ctx, fixdoc->name);
	if (fixdocpart)
	{
	    xps_process_metadata(ctx, fixdocpart);
	    if (!fixdocpart->complete)
		break; /* incomplete fixdocpart, try parsing more later */
	}
    }

    /*
     * If we know which page part is next, check if we
     * have all the page dependencies. If everything is
     * ready: parse and render.
     */

    while (ctx->next_page)
    {
	xps_part_t *pagepart = xps_find_part(ctx, ctx->next_page->name);
	if (pagepart && pagepart->complete)
	{
	    /* TODO: Check required resources */
	    dprintf1("PROCESSING PAGE '%s'.\n", pagepart->name);
	    ctx->next_page = ctx->next_page->next;
	}
	else
	    break;
    }

    return 0;
}

