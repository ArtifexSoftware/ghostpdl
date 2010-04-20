/* Copyright (C) 2006-2010 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
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

    strcpy(name, att + 16);
    s = strrchr(name, '}');
    if (s)
        *s = 0;

    return xps_find_resource(ctx, dict, name, urip);
}

int
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
    return 0;
}

xps_resource_t *
xps_parse_remote_resource_dictionary(xps_context_t *ctx, char *base_uri, char *source_att)
{
    char part_name[1024];
    char part_uri[1024];
    xps_resource_t *dict;
    xps_part_t *part;
    xps_item_t *xml;
    char *s;

    /* External resource dictionaries MUST NOT reference other resource dictionaries */
    xps_absolute_path(part_name, base_uri, source_att);
    part = xps_read_part(ctx, part_name);
    if (!part)
    {
        gs_throw1(-1, "cannot find remote resource part '%s'", part_name);
        return NULL;
    }

    xml = xps_parse_xml(ctx, part->data, part->size);
    if (!xml)
    {
        gs_rethrow(-1, "cannot parse xml");
        return NULL;
    }

    if (strcmp(xps_tag(xml), "ResourceDictionary"))
    {
        gs_throw1(-1, "expected ResourceDictionary element (found %s)", xps_tag(xml));
        return NULL;
    }

    strcpy(part_uri, part_name);
    s = strrchr(part_uri, '/');
    if (s)
        s[1] = 0;

    dict = xps_parse_resource_dictionary(ctx, part_uri, xml);
    if (!dict)
    {
        gs_rethrow1(-1, "cannot parse remote resource dictionary %s", part_uri);
        return NULL;
    }

    dict->base_xml = xml; /* pass on ownership */

    xps_release_part(ctx, part);

    return dict;
}

xps_resource_t *
xps_parse_resource_dictionary(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_resource_t *head;
    xps_resource_t *entry;
    xps_item_t *node;
    char *source;
    char *key;

    source = xps_att(root, "Source");
    if (source)
        return xps_parse_remote_resource_dictionary(ctx, base_uri, source);

    head = NULL;

    for (node = xps_down(root); node; node = xps_next(node))
    {
        /* Usually "x:Key"; we have already processed and stripped namespace */
        key = xps_att(node, "Key");
        if (key)
        {
            entry = xps_alloc(ctx, sizeof(xps_resource_t));
            if (!entry)
            {
                gs_throw(-1, "cannot allocate resource entry");
                return head;
            }
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
        head->base_uri = xps_alloc(ctx, strlen(base_uri) + 1);
        strcpy(head->base_uri, base_uri);
    }

    return head;
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
xps_debug_resource_dictionary(xps_resource_t *dict)
{
    while (dict)
    {
        if (dict->base_uri)
            dprintf1("URI = '%s'\n", dict->base_uri);
        dprintf2("KEY = '%s' VAL = %p\n", dict->name, dict->data);
        if (dict->parent)
        {
            dputs("PARENT = {\n");
            xps_debug_resource_dictionary(dict->parent);
            dputs("}\n");
        }
        dict = dict->next;
    }
}

