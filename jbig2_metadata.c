/*
    jbig2dec
    
    Copyright (c) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_metadata.c,v 1.1 2003/03/05 02:44:43 giles Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stdlib.h>
#include <string.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_metadata.h"

/* metadata key,value list object */
Jbig2Metadata *jbig2_metadata_new(Jbig2Ctx *ctx, Jbig2Encoding encoding)
{
    Jbig2Metadata *md = jbig2_alloc(ctx->allocator, sizeof(Jbig2Metadata));
    
    if (md != NULL) {
        md->encoding = encoding;
        md->entries = 0;
        md->max_entries = 4;
        md->keys = jbig2_alloc(ctx->allocator, md->max_entries*sizeof(char*));
        md->values = jbig2_alloc(ctx->allocator, md->max_entries*sizeof(char*));
        if (md->keys == NULL || md->values == NULL) {
            jbig2_metadata_free(ctx, md);
            md = NULL;
        }
    }
    return md;
}

void jbig2_metadata_free(Jbig2Ctx *ctx, Jbig2Metadata *md)
{
    if (md->keys) jbig2_free(ctx->allocator, md->keys);
    if (md->values) jbig2_free(ctx->allocator, md->values);
    jbig2_free(ctx->allocator, md);
}

static char *jbig2_strndup(Jbig2Ctx *ctx, const char *c, const int len)
{
    char *s = jbig2_alloc(ctx->allocator, len*sizeof(char));
    if (s == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
            "unable to duplicate comment string");
    } else {
        memcpy(s, c, len);
    }
    return s;
}

int jbig2_metadata_add(Jbig2Ctx *ctx, Jbig2Metadata *md,
                        const char *key, const int key_length,
                        const char *value, const int value_length)
{
    char **keys, **values;
    
    /* grow the array if necessary */
    if (md->entries == md->max_entries) {
        md->max_entries >>= 2;
        keys = jbig2_realloc(ctx->allocator, md->keys, md->max_entries);
        values = jbig2_realloc(ctx->allocator, md->values, md->max_entries);
        if (keys == NULL || values == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
                "unable to resize metadata structure");
            return -1;
        }
        md->keys = keys;
        md->values = values;
    }
    
    /* copy the passed key,value pair */
    md->keys[md->entries] = jbig2_strndup(ctx, key, key_length);
    md->values[md->entries] = jbig2_strndup(ctx, value, value_length);
    md->entries++;
    
    return 0;
}


/* decode an ascii comment segment 7.4.15.1 */
int jbig2_parse_comment_ascii(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const uint8_t *segment_data)
{
    char *s = (char *)segment_data + 4;
    char *end = (char *)segment_data + segment->data_length;
    Jbig2Metadata *comment;
    char *key, *value;
    int key_length, value_length;
    
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "ASCII comment data");
        
    comment = jbig2_metadata_new(ctx, JBIG2_ENCODING_ASCII);
    if (comment == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "unable to allocate comment structure");
        return -1;
    }
    /* loop over the segment data pulling out the key,value pairs */
    while(*s && s < end) {
        key_length = strlen(s) + 1;
        key = s; s += key_length;
        if (s >= end) goto too_short;
        value_length = strlen(s) + 1;
        value = s; s += value_length;
        if (s >= end) goto too_short;
        jbig2_metadata_add(ctx, comment, key, key_length, value, value_length);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
            "'%s'\t'%s'", key, value);
    }
    
    /* TODO: associate with ctx, page, or referred-to segment(s) */
    segment->result = comment;
    
    return 0;
    
too_short:
    jbig2_metadata_free(ctx, comment);
    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unexpected end of comment segment");
}

/* decode a UCS-16 comment segement 7.4.15.2 */
int jbig2_parse_comment_unicode(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const uint8_t *segment_data)
{
    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unhandled unicode comment segment");
}
