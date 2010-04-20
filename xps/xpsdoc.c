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

/* XPS interpreter - document parsing */

#include "ghostxps.h"

#include <expat.h>

/*
 * The part table stores both incomplete (interleaved) and completed parts.
 * In feed mode the completed parts are buffered until they can be safely freed.
 * In feed mode, the parts may also be completely empty since they are used to
 * store relationships between parts that may not yet have been encountered.
 */

void xps_debug_parts(xps_context_t *ctx)
{
    xps_part_t *part = ctx->part_list;
    xps_relation_t *rel;
    while (part)
    {
        dprintf2("part '%s' size=%d\n", part->name, part->size);
        for (rel = part->relations; rel; rel = rel->next)
            dprintf2("     target=%s type=%s\n", rel->target, rel->type);
        part = part->next;
    }
}

xps_part_t *
xps_new_part(xps_context_t *ctx, char *name, int capacity)
{
    xps_part_t *part;

    part = xps_alloc(ctx, sizeof(xps_part_t));
    if (!part)
        return NULL;

    part->name = NULL;
    part->size = 0;
    part->interleave = 0;
    part->capacity = 0;
    part->complete = 0;
    part->data = NULL;
    part->relations = NULL;
    part->relations_complete = 0;

    part->font = NULL;
    part->icc = NULL;

    part->deobfuscated = 0;

    part->name = xps_strdup(ctx, name);
    if (!part->name)
    {
        xps_free(ctx, part);
        return NULL;
    }

    if (capacity == 0)
        capacity = 1024;

    part->size = 0;
    part->capacity = capacity;
    part->data = xps_alloc(ctx, part->capacity);
    if (!part->data)
    {
        xps_free(ctx, part->name);
        xps_free(ctx, part);
        return NULL;
    }

    part->next = NULL;

    /* add it to the list of parts */
    part->next = ctx->part_list;
    ctx->part_list = part;

    /* add it to the hash table of parts */
    xps_hash_insert(ctx, ctx->part_table, part->name, part);

    return part;
}

void
xps_free_part_data(xps_context_t *ctx, xps_part_t *part)
{
    if (part->data)
        xps_free(ctx, part->data);
    part->data = NULL;
    part->size = 0;
    part->capacity = 0;
    part->complete = 0;
    part->deobfuscated = 0;
}

void
xps_release_part(xps_context_t *ctx, xps_part_t *part)
{
    /* since fonts and colorspaces need to live for the duration of
       the job there's no point in freeing those parts */
    if (part->font || part->icc)
        return;

    /* never free the part data if we're in feed mode,
       since we may need it later */
    if (ctx->file)
        xps_free_part_data(ctx, part);
}

void
xps_free_part(xps_context_t *ctx, xps_part_t *part)
{
    xps_free_part_data(ctx, part);

    if (part->font)
        xps_free_font(ctx, part->font);

    if (part->name)
        xps_free(ctx, part->name);
    part->name = NULL;

    xps_free_relations(ctx, part->relations);
    xps_free(ctx, part);
}

/*
 * Lookup a part in the part table. It may be
 * unloaded, partially loaded, or loaded.
 */

xps_part_t *
xps_find_part(xps_context_t *ctx, char *name)
{
    return xps_hash_lookup(ctx->part_table, name);
}

/*
 * Find and ensure that the contents of the part have been loaded.
 * Will return NULL if used on on an incomplete or unloaded part in feed mode.
 */

xps_part_t *
xps_read_part(xps_context_t *ctx, char *name)
{
    xps_part_t *part;
    part = xps_hash_lookup(ctx->part_table, name);
    if (ctx->file)
    {
        if (!part)
            part = xps_read_zip_part(ctx, name);
        if (part && !part->complete)
            part = xps_read_zip_part(ctx, name);
        return part;
    }
    return part;
}

/*
 * Relationships are stored in a linked list hooked into the part which
 * is the source of the relation.
 */

void
xps_part_name_from_relation_part_name(char *output, char *name)
{
    char *p, *q;
    strcpy(output, name);
    p = strstr(output, "_rels/");
    q = strstr(name, "_rels/");
    if (p)
    {
        *p = 0;
        strcat(output, q + 6);
    }
    p = strstr(output, ".rels");
    if (p)
    {
        *p = 0;
    }
}

int
xps_add_relation(xps_context_t *ctx, char *source, char *target, char *type)
{
    xps_relation_t *node;
    xps_part_t *part;

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

    if (xps_doc_trace)
    {
        dprintf2("  relation %s -> %s\n", source, target);
    }

    return 0;
}

void
xps_free_relations(xps_context_t *ctx, xps_relation_t *node)
{
    xps_relation_t *next;
    while (node)
    {
        next = node->next;
        xps_free(ctx, node->target);
        xps_free(ctx, node->type);
        xps_free(ctx, node);
        node = next;
    }
}

/*
 * The FixedDocumentSequence and FixedDocument parts determine
 * which parts correspond to actual pages, and the page order.
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

static int
xps_add_fixed_document(xps_context_t *ctx, char *name)
{
    xps_document_t *fixdoc;

    /* Check for duplicates first */
    for (fixdoc = ctx->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
        if (!strcmp(fixdoc->name, name))
            return 0;

    if (xps_doc_trace)
        dprintf1("doc: adding fixdoc %s\n", name);

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

void
xps_free_fixed_documents(xps_context_t *ctx)
{
    xps_document_t *node = ctx->first_fixdoc;
    while (node)
    {
        xps_document_t *next = node->next;
        xps_free(ctx, node->name);
        xps_free(ctx, node);
        node = next;
    }
    ctx->first_fixdoc = NULL;
    ctx->last_fixdoc = NULL;
}

static int
xps_add_fixed_page(xps_context_t *ctx, char *name, int width, int height)
{
    xps_page_t *page;

    /* Check for duplicates first */
    for (page = ctx->first_page; page; page = page->next)
        if (!strcmp(page->name, name))
            return 0;

    if (xps_doc_trace)
        dprintf1("doc: adding page %s\n", name);

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

void
xps_free_fixed_pages(xps_context_t *ctx)
{
    xps_page_t *node = ctx->first_page;
    while (node)
    {
        xps_page_t *next = node->next;
        xps_free(ctx, node->name);
        xps_free(ctx, node);
        node = next;
    }
    ctx->first_page = NULL;
    ctx->last_page = NULL;
    ctx->next_page = NULL;
}

/*
 * Parse the metadata document structure and _rels/XXX.rels parts.
 * These should be parsed eagerly as they are interleaved, so the
 * parsing needs to be able to cope with incomplete xml.
 *
 * We re-parse the part every time a new part of the piece comes in.
 * The binary trees in xps_context_t make sure that the information
 * is not duplicated (since the entries are often parsed many times).
 *
 * We hook up unique expat handlers for this, and ignore any expat
 * errors that occur.
 *
 * The seekable mode only parses the document structure parts,
 * and ignores all other metadata.
 */

static void
xps_parse_metadata_imp(void *zp, char *name, char **atts)
{
    xps_context_t *ctx = zp;
    int i;

    if (!strcmp(name, "Relationship"))
    {
        char realpart[1024];
        char tgtbuf[1024];
        char *target = NULL;
        char *type = NULL;

        for (i = 0; atts[i]; i += 2)
        {
            if (!strcmp(atts[i], "Target"))
                target = atts[i + 1];
            if (!strcmp(atts[i], "Type"))
                type = atts[i + 1];
        }

        if (target && type)
        {
            xps_part_name_from_relation_part_name(realpart, ctx->part_uri);
            xps_absolute_path(tgtbuf, ctx->base_uri, target);
            xps_add_relation(ctx, realpart, tgtbuf, type);
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
            xps_absolute_path(srcbuf, ctx->base_uri, source);
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
            xps_absolute_path(srcbuf, ctx->base_uri, source);
            xps_add_fixed_page(ctx, srcbuf, width, height);
        }
    }
}

int
xps_parse_metadata(xps_context_t *ctx, xps_part_t *part)
{
    XML_Parser xp;
    char buf[1024];
    char *s;

    /* Save directory name part */
    strcpy(buf, part->name);
    s = strrchr(buf, '/');
    if (s)
        s[0] = 0;

    /* _rels parts are voodoo: their URI references are from
     * the part they are associated with, not the actual _rels
     * part being parsed.
     */
    s = strstr(buf, "/_rels");
    if (s)
        *s = 0;

    ctx->base_uri = buf;
    ctx->part_uri = part->name;

    xp = XML_ParserCreate(NULL);
    if (!xp)
        return -1;

    XML_SetUserData(xp, ctx);
    XML_SetParamEntityParsing(xp, XML_PARAM_ENTITY_PARSING_NEVER);
    XML_SetStartElementHandler(xp, (XML_StartElementHandler)xps_parse_metadata_imp);

    (void) XML_Parse(xp, (char*)part->data, part->size, 1);

    XML_ParserFree(xp);

    ctx->base_uri = NULL;
    ctx->part_uri = NULL;

    return 0;
}

/*
 * Parse a FixedPage part and infer the required relationships. The
 * relationship parts are often placed at the end of the file, so we don't want
 * to rely on them. This function gets called if a FixedPage part is
 * encountered and its relationship part has not been parsed yet.
 *
 *   <Glyphs FontUri=... >
 *   <ImageBrush ImageSource=... >
 *   <ResourceDictionary Source=... >
 */

static void
xps_trim_url(char *path)
{
    char *p = strrchr(path, '#');
    if (p)
        *p = 0;
}

static void
xps_parse_color_relation(xps_context_t *ctx, char *string)
{
    char path[1024];
    char buf[1024];
    char *sp, *ep;

    /* "ContextColor /Resources/Foo.icc 1,0.3,0.5,1.0" */

    strcpy(buf, string);
    sp = strchr(buf, ' ');
    if (sp)
    {
        sp ++;
        ep = strchr(sp, ' ');
        if (ep)
        {
            *ep = 0;
            xps_absolute_path(path, ctx->base_uri, sp);
            xps_trim_url(path);
            xps_add_relation(ctx, ctx->part_uri, path, REL_REQUIRED_RESOURCE);
        }
    }
}

static void
xps_parse_image_relation(xps_context_t *ctx, char *string)
{
    char path[1024];
    char buf[1024];
    char *sp, *ep;

    /* "{ColorConvertedBitmap /Resources/Image.tiff /Resources/Profile.icc}" */

    if (strstr(string, "{ColorConvertedBitmap") == string)
    {
        strcpy(buf, string);
        sp = strchr(buf, ' ');
        if (sp)
        {
            sp ++;
            ep = strchr(sp, ' ');
            if (ep)
            {
                *ep = 0;
                xps_absolute_path(path, ctx->base_uri, sp);
                xps_trim_url(path);
                xps_add_relation(ctx, ctx->part_uri, path, REL_REQUIRED_RESOURCE);

                sp = ep + 1;
                ep = strchr(sp, '}');
                if (ep)
                {
                    *ep = 0;
                    xps_absolute_path(path, ctx->base_uri, sp);
                    xps_trim_url(path);
                    xps_add_relation(ctx, ctx->part_uri, path, REL_REQUIRED_RESOURCE);
                }
            }
        }
    }
    else
    {
        xps_absolute_path(path, ctx->base_uri, string);
        xps_trim_url(path);
        xps_add_relation(ctx, ctx->part_uri, path, REL_REQUIRED_RESOURCE);
    }
}

static void
xps_parse_content_relations_imp(void *zp, char *ns_name, char **atts)
{
    xps_context_t *ctx = zp;
    char path[1024];
    char *name;
    int i;

    name = strchr(ns_name, ' ');
    if (name)
        name ++;
    else
        name = ns_name;

    if (!strcmp(name, "Glyphs"))
    {
        for (i = 0; atts[i]; i += 2)
        {
            if (!strcmp(atts[i], "FontUri"))
            {
                xps_absolute_path(path, ctx->base_uri, atts[i+1]);
                xps_trim_url(path);
                xps_add_relation(ctx, ctx->part_uri, path, REL_REQUIRED_RESOURCE);
            }
        }
    }

    if (!strcmp(name, "ImageBrush"))
    {
        for (i = 0; atts[i]; i += 2)
            if (!strcmp(atts[i], "ImageSource"))
                xps_parse_image_relation(ctx, atts[i + 1]);
    }

    if (!strcmp(name, "ResourceDictionary"))
    {
        for (i = 0; atts[i]; i += 2)
        {
            if (!strcmp(atts[i], "Source"))
            {
                xps_absolute_path(path, ctx->base_uri, atts[i+1]);
                xps_trim_url(path);
                xps_add_relation(ctx, ctx->part_uri, path, REL_REQUIRED_RESOURCE_RECURSIVE);
            }
        }
    }

    if (!strcmp(name, "SolidColorBrush") || !strcmp(name, "GradientStop"))
    {
        for (i = 0; atts[i]; i += 2)
            if (!strcmp(atts[i], "Color"))
                xps_parse_color_relation(ctx, atts[i + 1]);
    }

    if (!strcmp(name, "Glyphs") || !strcmp(name, "Path"))
    {
        for (i = 0; atts[i]; i += 2)
            if (!strcmp(atts[i], "Fill") || !strcmp(atts[i], "Stroke"))
                xps_parse_color_relation(ctx, atts[i + 1]);
    }
}

int
xps_parse_content_relations(xps_context_t *ctx, xps_part_t *part)
{
    XML_Parser xp;
    char buf[1024];
    char *s;

    /* Set current directory for resolving relative path names */
    strcpy(buf, part->name);
    s = strrchr(buf, '/');
    if (s)
        s[0] = 0;

    ctx->part_uri = part->name;
    ctx->base_uri = buf;

    if (xps_doc_trace)
        dprintf1("doc: parsing relations from content (%s)\n", part->name);

    xp = XML_ParserCreateNS(NULL, ' ');
    if (!xp)
        return -1;

    XML_SetUserData(xp, ctx);
    XML_SetParamEntityParsing(xp, XML_PARAM_ENTITY_PARSING_NEVER);
    XML_SetStartElementHandler(xp, (XML_StartElementHandler)xps_parse_content_relations_imp);

    (void) XML_Parse(xp, (char*)part->data, part->size, 1);

    XML_ParserFree(xp);

    ctx->part_uri = NULL;
    ctx->base_uri = NULL;

    return 0;
}

