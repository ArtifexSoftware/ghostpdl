/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Recursive descent XML parsing API on top of Expat. */

#include "ghostsvg.h"

#include <expat.h>

#define XMLBUFLEN 4096

static void on_open_tag(void *zp, const char *name, const char **atts)
{
    svg_context_t *ctx = zp;
    svg_item_t *item;
    svg_item_t *tail;
    int namelen;
    int attslen;
    int textlen;
    char *p;
    int i;

    if (ctx->error)
        return;

    /* count size to alloc */

    namelen = strlen(name) + 1; /* zero terminated */
    attslen = sizeof(char*); /* with space for sentinel */
    textlen = 0;
    for (i = 0; atts[i]; i++)
    {
        attslen += sizeof(char*);
        textlen += strlen(atts[i]) + 1;
    }

    item = svg_alloc(ctx, sizeof(svg_item_t) + attslen + namelen + textlen);
    if (!item)
    {
        ctx->error = "out of memory";
    }

    /* copy strings to new memory */

    item->atts = (char**) (((char*)item) + sizeof(svg_item_t));
    item->name = ((char*)item) + sizeof(svg_item_t) + attslen;
    p = ((char*)item) + sizeof(svg_item_t) + attslen + namelen;

    strcpy(item->name, name);
    for (i = 0; atts[i]; i++)
    {
        item->atts[i] = p;
        strcpy(item->atts[i], atts[i]);
        p += strlen(p) + 1;
    }

    item->atts[i] = 0;

    /* link item into tree */

    item->up = ctx->head;
    item->down = NULL;
    item->next = NULL;

    if (!ctx->head)
    {
        ctx->root = item;
        ctx->head = item;
        return;
    }

    if (!ctx->head->down)
    {
        ctx->head->down = item;
        ctx->head = item;
        return;
    }

    tail = ctx->head->down;
    while (tail->next)
        tail = tail->next;
    tail->next = item;
    ctx->head = item;
}

static void on_close_tag(void *zp, const char *name)
{
    svg_context_t *ctx = zp;

    if (ctx->error)
        return;

    if (ctx->head)
        ctx->head = ctx->head->up;
}

static inline int is_svg_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void on_text(void *zp, const char *buf, int len)
{
    svg_context_t *ctx = zp;
    const char *atts[3];
    int i;

    if (ctx->error)
        return;

    for (i = 0; i < len; i++)
    {
        if (!is_svg_space(buf[i]))
        {
            char *tmp = svg_alloc(ctx, len + 1);
            if (!tmp)
            {
                ctx->error = "out of memory";
                return;
            }

            atts[0] = "";
            atts[1] = tmp;
            atts[2] = NULL;

            memcpy(tmp, buf, len);
            tmp[len] = 0;
            on_open_tag(zp, "", atts);
            on_close_tag(zp, "");
            svg_free(ctx, tmp);
            return;
        }
    }
}

int
svg_open_xml_parser(svg_context_t *ctx)
{
    XML_Parser xp;

    /* xp = XML_ParserCreateNS(NULL, ns); */
    xp = XML_ParserCreate(NULL);
    if (!xp)
        return gs_throw(-1, "xml error: could not create expat parser");

    XML_SetUserData(xp, ctx);
    XML_SetParamEntityParsing(xp, XML_PARAM_ENTITY_PARSING_NEVER);
    XML_SetStartElementHandler(xp, on_open_tag);
    XML_SetEndElementHandler(xp, on_close_tag);
    XML_SetCharacterDataHandler(xp, on_text);

    ctx->root = NULL;
    ctx->head = NULL;
    ctx->error = NULL;
    ctx->parser = xp;

    return 0;
}

int
svg_feed_xml_parser(svg_context_t *ctx, const char *buf, int len)
{
    XML_Parser xp = ctx->parser;
    int code = XML_Parse(xp, buf, len, 0);
    if (code == 0)
    {
        ctx->root = NULL;
        ctx->head = NULL;
        ctx->error = NULL;
        ctx->parser = NULL;
        gs_throw3(-1, "xml error: %s (line %d, column %d)",
                XML_ErrorString(XML_GetErrorCode(xp)),
                XML_GetCurrentLineNumber(xp),
                XML_GetCurrentColumnNumber(xp));
        XML_ParserFree(xp);
        return -1;
    }
    return 0;
}

svg_item_t *
svg_close_xml_parser(svg_context_t *ctx)
{
    XML_Parser xp = ctx->parser;
    svg_item_t *root = ctx->root;

    int code = XML_Parse(xp, "", 0, 1); /* finish parsing */
    if (code == 0)
    {
        gs_throw3(-1, "xml error: %s (line %d, column %d)",
                XML_ErrorString(XML_GetErrorCode(xp)),
                XML_GetCurrentLineNumber(xp),
                XML_GetCurrentColumnNumber(xp));
        root = NULL;
    }

    ctx->root = NULL;
    ctx->head = NULL;
    ctx->error = NULL;
    ctx->parser = NULL;

    XML_ParserFree(xp);

    return root;
}

svg_item_t *
svg_next(svg_item_t *item)
{
    return item->next;
}

svg_item_t *
svg_down(svg_item_t *item)
{
    return item->down;
}

char *
svg_tag(svg_item_t *item)
{
    return item->name;
}

char *
svg_att(svg_item_t *item, const char *att)
{
    int i;
    for (i = 0; item->atts[i]; i += 2)
        if (!strcmp(item->atts[i], att))
            return item->atts[i + 1];
    return NULL;
}

void
svg_free_item(svg_context_t *ctx, svg_item_t *item)
{
    svg_item_t *next;
    while (item)
    {
        next = item->next;
        if (item->down)
            svg_free_item(ctx, item->down);
        svg_free(ctx, item);
        item = next;
    }
}

static void indent(int n)
{
    while (n--)
        printf("  ");
}

static void
svg_debug_item_imp(svg_item_t *item, int level, int loop)
{
    int i;

    while (item)
    {
        indent(level);

        if (strlen(item->name) == 0)
            printf("%s\n", item->atts[1]);
        else
        {
            printf("<%s", item->name);

            for (i = 0; item->atts[i]; i += 2)
                printf(" %s=\"%s\"", item->atts[i], item->atts[i+1]);

            if (item->down)
            {
                printf(">\n");
                svg_debug_item_imp(item->down, level + 1, 1);
                indent(level);
                printf("</%s>\n", item->name);
            }
            else
                printf(" />\n");
        }

        item = item->next;

        if (!loop)
            return;
    }
}

void
svg_debug_item(svg_item_t *item, int level)
{
    svg_debug_item_imp(item, level, 0);
}
