/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Simple XML document object model on top of Expat. */

#include "ghostxps.h"

#include <expat.h>

#define XMLBUFLEN 4096

#define NS_XPS "http://schemas.microsoft.com/xps/2005/06"
#define NS_MC "http://schemas.openxmlformats.org/markup-compatibility/2006"
#define NS_OXPS "http://schemas.openxps.org/oxps/v1.0"
typedef struct xps_parser_s xps_parser_t;

struct xps_parser_s
{
    xps_context_t *ctx;
    xps_item_t *root;
    xps_item_t *head;
    const char *error;
    char *base; /* base of relative URIs */
};

struct xps_item_s
{
    char *name;
    char **atts;
    xps_item_t *up;
    xps_item_t *down;
    xps_item_t *tail;
    xps_item_t *next;
};

static const char *
skip_namespace(const char *s)
{
    const char *p = strchr(s, ' ');
    if (p)
        return p + 1;
    return s;
}

static void
on_open_tag(void *zp, const char *ns_name, const char **atts)
{
    xps_parser_t *parser = zp;
    xps_context_t *ctx = parser->ctx;
    xps_item_t *item;
    xps_item_t *tail;
    int namelen;
    int attslen;
    int textlen;
    const char *name;
    char *p;
    int i;

    if (parser->error)
        return;

    /* check namespace */

    name = NULL;

    p = strstr(ns_name, NS_XPS);
    if (p == ns_name)
    {
        name = strchr(ns_name, ' ') + 1;
    }

    p = strstr(ns_name, NS_MC);
    if (p == ns_name)
    {
        name = strchr(ns_name, ' ') + 1;
    }

    p = strstr(ns_name, NS_OXPS);
    if (p == ns_name)
    {
        name = strchr(ns_name, ' ') + 1;
    }

    if (!name)
    {
        dmprintf1(ctx->memory, "unknown namespace: %s\n", ns_name);
        name = ns_name;
    }

    /* count size to alloc */

    namelen = strlen(name) + 1; /* zero terminated */
    attslen = sizeof(char*); /* with space for sentinel */
    textlen = 0;
    for (i = 0; atts[i]; i++)
    {
        attslen += sizeof(char*);
        if ((i & 1) == 0)
            textlen += strlen(skip_namespace(atts[i])) + 1;
        else
            textlen += strlen(atts[i]) + 1;
    }

    item = xps_alloc(ctx, sizeof(xps_item_t) + attslen + namelen + textlen);
    if (!item) {
        parser->error = "out of memory";
        gs_throw(gs_error_VMerror, "out of memory.\n");
        return;
    }

    /* copy strings to new memory */

    item->atts = (char**) (((char*)item) + sizeof(xps_item_t));
    item->name = ((char*)item) + sizeof(xps_item_t) + attslen;
    p = ((char*)item) + sizeof(xps_item_t) + attslen + namelen;

    strcpy(item->name, name);
    for (i = 0; atts[i]; i++)
    {
        item->atts[i] = p;
        if ((i & 1) == 0)
            strcpy(item->atts[i], skip_namespace(atts[i]));
        else
            strcpy(item->atts[i], atts[i]);
        p += strlen(p) + 1;
    }

    item->atts[i] = 0;

    /* link item into tree */

    item->up = parser->head;
    item->down = NULL;
    item->next = NULL;

    if (!parser->head)
    {
        parser->root = item;
        parser->head = item;
        return;
    }

    if (!parser->head->down)
    {
        parser->head->down = item;
        parser->head->tail = item;
        parser->head = item;
        return;
    }

    tail = parser->head->tail;
    tail->next = item;
    parser->head->tail = item;
    parser->head = item;
}

static void
on_close_tag(void *zp, const char *name)
{
    xps_parser_t *parser = zp;

    if (parser->error)
        return;

    if (parser->head)
        parser->head = parser->head->up;
}

static inline int
is_xml_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void
on_text(void *zp, char *buf, int len)
{
    xps_parser_t *parser = zp;
    xps_context_t *ctx = parser->ctx;
    const char *atts[3];
    int i;

    if (parser->error)
        return;

    for (i = 0; i < len; i++)
    {
        if (!is_xml_space(buf[i]))
        {
            char *tmp = xps_alloc(ctx, len + 1);
            if (!tmp)
            {
                parser->error = "out of memory";
                gs_throw(gs_error_VMerror, "out of memory.\n");
                return;
            }

            atts[0] = "";
            atts[1] = tmp;
            atts[2] = NULL;

            memcpy(tmp, buf, len);
            tmp[len] = 0;
            on_open_tag(zp, "", atts);
            on_close_tag(zp, "");
            xps_free(ctx, tmp);
            return;
        }
    }
}

xps_item_t *
xps_parse_xml(xps_context_t *ctx, byte *buf, int len)
{
    xps_parser_t parser;
    XML_Parser xp;
    int code;

    parser.ctx = ctx;
    parser.root = NULL;
    parser.head = NULL;
    parser.error = NULL;

    xp = XML_ParserCreateNS(NULL, ' ');
    if (!xp)
    {
        gs_throw(-1, "xml error: could not create expat parser");
        return NULL;
    }

    XML_SetUserData(xp, &parser);
    XML_SetParamEntityParsing(xp, XML_PARAM_ENTITY_PARSING_NEVER);
    XML_SetStartElementHandler(xp, (XML_StartElementHandler)on_open_tag);
    XML_SetEndElementHandler(xp, (XML_EndElementHandler)on_close_tag);
    XML_SetCharacterDataHandler(xp, (XML_CharacterDataHandler)on_text);

    code = XML_Parse(xp, (char*)buf, len, 1);
    if (code == 0 || parser.error != NULL)
    {
        if (parser.root)
            xps_free_item(ctx, parser.root);
        if (XML_ErrorString(XML_GetErrorCode(xp)) != 0)
            emprintf1(parser.ctx->memory, "XML_Error: %s\n", XML_ErrorString(XML_GetErrorCode(xp)));
        XML_ParserFree(xp);
        gs_throw1(-1, "parser error: %s", parser.error);
        return NULL;
    }

    XML_ParserFree(xp);

    return parser.root;
}

xps_item_t *
xps_next(xps_item_t *item)
{
    return item->next;
}

xps_item_t *
xps_down(xps_item_t *item)
{
    return item->down;
}

char *
xps_tag(xps_item_t *item)
{
    return item->name;
}

char *
xps_att(xps_item_t *item, const char *att)
{
    int i;
    for (i = 0; item->atts[i]; i += 2)
        if (!strcmp(item->atts[i], att))
            return item->atts[i + 1];
    return NULL;
}

void
xps_detach_and_free_remainder(xps_context_t *ctx, xps_item_t *root, xps_item_t *item)
{
    if (item->up)
        item->up->down = NULL;

    xps_free_item(ctx, item->next);
    item->next = NULL;

    xps_free_item(ctx, root);
}

void
xps_free_item(xps_context_t *ctx, xps_item_t *item)
{
    xps_item_t *next;
    while (item)
    {
        next = item->next;
        if (item->down)
            xps_free_item(ctx, item->down);
        xps_free(ctx, item);
        item = next;
    }
}

static void indent(int n)
{
    while (n--)
        dlprintf("  ");
}

static void
xps_debug_item_imp(xps_item_t *item, int level, int loop)
{
    int i;

    while (item)
    {
        indent(level);

        if (strlen(item->name) == 0)
            dlprintf1("%s\n", item->atts[1]);
        else
        {
            dlprintf1("<%s", item->name);

            for (i = 0; item->atts[i]; i += 2)
                dlprintf2(" %s=\"%s\"", item->atts[i], item->atts[i+1]);

            if (item->down)
            {
                dlprintf(">\n");
                xps_debug_item_imp(item->down, level + 1, 1);
                indent(level);
                dlprintf1("</%s>\n", item->name);
            }
            else
                dlprintf(" />\n");
        }

        item = item->next;

        if (!loop)
            return;
    }
}

void
xps_debug_item(xps_item_t *item, int level)
{
    xps_debug_item_imp(item, level, 0);
}
