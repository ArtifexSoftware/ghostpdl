/*
    jbig2dec
    
    Copyright (c) 2003 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_metadata.h,v 1.2 2003/03/05 03:32:41 giles Exp $
*/


#ifndef _JBIG2_METADATA_H
#define _JBIG2_METADATA_H

/* metadata from extension segments */

/* these bits should be moved to jbig2.h for public access */
typedef enum {
    JBIG2_ENCODING_ASCII,
    JBIG2_ENCODING_UCS16
} Jbig2Encoding;

typedef struct _Jbig2Metadata Jbig2Metadata;

Jbig2Metadata *jbig2_metadata_new(Jbig2Ctx *ctx, Jbig2Encoding encoding);
void jbig2_metadata_free(Jbig2Ctx *ctx, Jbig2Metadata *md);
int jbig2_metadata_add(Jbig2Ctx *ctx, Jbig2Metadata *md,
                        const char *key, const int key_length,
                        const char *value, const int value_length);

struct _Jbig2Metadata {
    Jbig2Encoding encoding;
    char **keys, **values;
    int entries, max_entries;
};

/* these bits can go to jbig2_priv.h */
int jbig2_parse_comment_ascii(Jbig2Ctx *ctx, Jbig2Segment *segment,
                                const uint8_t *segment_data);
int jbig2_parse_comment_unicode(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const uint8_t *segment_data);

#endif /* _JBIG2_METADATA_H */
