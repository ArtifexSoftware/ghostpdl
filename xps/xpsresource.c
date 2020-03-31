/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* XPS interpreter - resource functions */

#include "ghostxps.h"

static xps_item_t *
xps_find_resource(xps_context_t *ctx, xps_resource_t *dict, char *name, char **urip)
{
    xps_resource_t *head, *node;
    for (head = dict; head; head = head->parent)
    {
        for (node = head; node; node = node->next)
        {
            if (!strcmp(node->name, name))
            {
                if (urip && head->base_uri)
                    *urip = head->base_uri;
                return node->data;
            }
        }
    }
    return NULL;
}

static xps_item_t *
xps_parse_resource_reference(xps_context_t *ctx, xps_resource_t *dict, char *att, char **urip)
{
    char name[1024];
    char *s;

    if (strstr(att, "{StaticResource ") != att)
        return NULL;

    gs_strlcpy(name, att + 16, sizeof name);
    s = strrchr(name, '}');
    if (s)
        *s = 0;

    return xps_find_resource(ctx, dict, name, urip);
}

void
xps_resolve_resource_reference(xps_context_t *ctx, xps_resource_t *dict,
        char **attp, xps_item_t **tagp, char **urip)
{
    if (*attp)
    {
        xps_item_t *rsrc = xps_parse_resource_reference(ctx, dict, *attp, urip);
        if (rsrc)
        {
            *attp = NULL;
            *tagp = rsrc;
        }
    }
}

static int
xps_parse_remote_resource_dictionary(xps_context_t *ctx, xps_resource_t **dictp, char *base_uri, char *source_att)
{
    char part_name[1024];
    char part_uri[1024];
    xps_resource_t *dict = *dictp;
    xps_part_t *part;
    xps_item_t *xml;
    char *s;
    int code;

    /* External resource dictionaries MUST NOT reference other resource dictionaries */
    xps_absolute_path(part_name, base_uri, source_att, sizeof part_name);
    part = xps_read_part(ctx, part_name);
    if (!part)
    {
        return gs_throw1(-1, "cannot find remote resource part '%s'", part_name);
    }

    xml = xps_parse_xml(ctx, part->data, part->size);
    if (!xml)
    {
        xps_free_part(ctx, part);
        return gs_rethrow(-1, "cannot parse xml");
    }

    if (strcmp(xps_tag(xml), "ResourceDictionary"))
    {
        xps_free_item(ctx, xml);
        xps_free_part(ctx, part);
        return gs_throw1(-1, "expected ResourceDictionary element (found %s)", xps_tag(xml));
    }

    gs_strlcpy(part_uri, part_name, sizeof part_uri);
    s = strrchr(part_uri, '/');
    if (s)
        s[1] = 0;

    code = xps_parse_resource_dictionary(ctx, &dict, part_uri, xml);
    if (code)
    {
        xps_free_item(ctx, xml);
        xps_free_part(ctx, part);
        return gs_rethrow1(code, "cannot parse remote resource dictionary: %s", part_uri);
    }

    if (dict != NULL)
        dict->base_xml = xml; /* pass on ownership */
    else
        xps_free_item(ctx, xml);

    xps_free_part(ctx, part);

    *dictp = dict;
    return gs_okay;
}

int
xps_parse_resource_dictionary(xps_context_t *ctx, xps_resource_t **dictp, char *base_uri, xps_item_t *root)
{
    xps_resource_t *head;
    xps_resource_t *entry;
    xps_item_t *node;
    char *source;
    char *key;
    int code;

    if (*dictp)
    {
        gs_warn("multiple resource dictionaries; ignoring all but the first");
        return gs_okay;
    }

    source = xps_att(root, "Source");
    if (source)
    {
        code = xps_parse_remote_resource_dictionary(ctx, dictp, base_uri, source);
        if (code)
            return gs_rethrow(code, "cannot parse remote resource dictionary");
        return gs_okay;
    }

    head = NULL;

    for (node = xps_down(root); node; node = xps_next(node))
    {
        /* Usually "x:Key"; we have already processed and stripped namespace */
        key = xps_att(node, "Key");
        if (key)
        {
            entry = xps_alloc(ctx, sizeof(xps_resource_t));
            if (!entry)
                return gs_throw(gs_error_VMerror, "cannot allocate resource entry");
            entry->name = key;
            entry->base_uri = NULL;
            entry->base_xml = NULL;
            entry->data = node;
            entry->next = head;
            entry->parent = NULL;
            head = entry;
        }
    }

    if (head)
    {
        head->base_uri = xps_strdup(ctx, base_uri);
    }
    else
    {
        gs_warn("empty resource dictionary");
    }

    *dictp = head;
    return gs_okay;
}

void
xps_free_resource_dictionary(xps_context_t *ctx, xps_resource_t *dict)
{
    xps_resource_t *next;
    while (dict)
    {
        next = dict->next;
        if (dict->base_xml)
            xps_free_item(ctx, dict->base_xml);
        if (dict->base_uri)
            xps_free(ctx, dict->base_uri);
        xps_free(ctx, dict);
        dict = next;
    }
}

void
xps_debug_resource_dictionary(xps_context_t *ctx, xps_resource_t *dict)
{
    while (dict)
    {
        if (dict->base_uri)
            dmprintf1(ctx->memory, "URI = '%s'\n", dict->base_uri);
        dmprintf2(ctx->memory, "KEY = '%s' VAL = "PRI_INTPTR"\n",
                  dict->name, (intptr_t)dict->data);
        if (dict->parent)
        {
            dmputs(ctx->memory, "PARENT = {\n");
            xps_debug_resource_dictionary(ctx, dict->parent);
            dmputs(ctx->memory, "}\n");
        }
        dict = dict->next;
    }
}
