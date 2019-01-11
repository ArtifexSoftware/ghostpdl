/* Copyright (C) 2001-2018 Artifex Software, Inc.
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


/* XPS interpreter - zip container parsing */

#include "ghostxps.h"

static int isfile(char *path)
{
    FILE *file = gp_fopen(path, "rb");
    if (file)
    {
        fclose(file);
        return 1;
    }
    return 0;
}

static inline int getshort(FILE *file)
{
    int a = xps_getc(file);
    int b = xps_getc(file);
    return a | (b << 8);
}

static inline int getlong(FILE *file)
{
    int a = xps_getc(file);
    int b = xps_getc(file);
    int c = xps_getc(file);
    int d = xps_getc(file);
    return a | (b << 8) | (c << 16) | (d << 24);
}

static void *
xps_zip_alloc_items(xps_context_t *ctx, int items, int size)
{
    void *item = xps_alloc(ctx, items * size);
    if (!item) {
        gs_throw(gs_error_VMerror, "out of memory: item.\n");
        return NULL;
    }
    return item;
}

static void
xps_zip_free(xps_context_t *ctx, void *ptr)
{
    xps_free(ctx, ptr);
}

static int
xps_compare_entries(const void *a0, const void *b0)
{
    xps_entry_t *a = (xps_entry_t*) a0;
    xps_entry_t *b = (xps_entry_t*) b0;
    return xps_strcasecmp(a->name, b->name);
}

static xps_entry_t *
xps_find_zip_entry(xps_context_t *ctx, const char *name)
{
    int l = 0;
    int r = ctx->zip_count - 1;
    while (l <= r)
    {
        int m = (l + r) >> 1;
        int c = xps_strcasecmp(name, ctx->zip_table[m].name);
        if (c < 0)
            r = m - 1;
        else if (c > 0)
            l = m + 1;
        else
            return &ctx->zip_table[m];
    }
    return NULL;
}

/*
 * Inflate the data in a zip entry.
 */

static int
xps_read_zip_entry(xps_context_t *ctx, xps_entry_t *ent, unsigned char *outbuf)
{
    z_stream stream;
    unsigned char *inbuf;
    int sig;
    int version, general, method;
    int namelength, extralength;
    int code;

    if_debug1m('|', ctx->memory, "zip: inflating entry '%s'\n", ent->name);

    if (xps_fseek(ctx->file, ent->offset, 0) < 0)
        return gs_throw1(-1, "seek to offset %d failed.", ent->offset);

    sig = getlong(ctx->file);
    if (sig != ZIP_LOCAL_FILE_SIG)
        return gs_throw1(-1, "wrong zip local file signature (0x%x)", sig);

    version = getshort(ctx->file);
    (void)version; /* Avoid unused variable compiler warning */
    general = getshort(ctx->file);
    if (general & ZIP_ENCRYPTED_FLAG)
        return gs_throw(-1, "zip file content is encrypted");
    method = getshort(ctx->file);
    (void) getshort(ctx->file); /* file time */
    (void) getshort(ctx->file); /* file date */
    (void) getlong(ctx->file); /* crc-32 */
    (void) getlong(ctx->file); /* csize */
    (void) getlong(ctx->file); /* usize */
    namelength = getshort(ctx->file);
    extralength = getshort(ctx->file);

    if (namelength < 0 || namelength > 65535)
        return gs_rethrow(gs_error_ioerror, "Illegal namelength (can't happen).\n");
    if (extralength < 0 || extralength > 65535)
        return gs_rethrow(gs_error_ioerror, "Illegal extralength (can't happen).\n");

    if (xps_fseek(ctx->file, namelength + extralength, 1) != 0)
        return gs_throw1(gs_error_ioerror, "xps_fseek to %d failed.\n", namelength + extralength);

    if (method == 0)
    {
        code = xps_fread(outbuf, 1, ent->usize, ctx->file);
        if (code != ent->usize)
            return gs_throw1(gs_error_ioerror, "Failed to read %d bytes", ent->usize);
    }
    else if (method == 8)
    {
        inbuf = xps_alloc(ctx, ent->csize);
        if (!inbuf) {
            return gs_rethrow(gs_error_VMerror, "out of memory.\n");
        }

        code = xps_fread(inbuf, 1, ent->csize, ctx->file);
        if (code != ent->csize) {
            xps_free(ctx, inbuf);
            return gs_throw1(gs_error_ioerror, "Failed to read %d bytes", ent->csize);
        }

        memset(&stream, 0, sizeof(z_stream));
        stream.zalloc = (alloc_func) xps_zip_alloc_items;
        stream.zfree = (free_func) xps_zip_free;
        stream.opaque = ctx;
        stream.next_in = inbuf;
        stream.avail_in = ent->csize;
        stream.next_out = outbuf;
        stream.avail_out = ent->usize;

        code = inflateInit2(&stream, -15);
        if (code != Z_OK)
        {
            xps_free(ctx, inbuf);
            return gs_throw1(-1, "zlib inflateInit2 error: %s", stream.msg);
        }
        code = inflate(&stream, Z_FINISH);
        if (code != Z_STREAM_END)
        {
            inflateEnd(&stream);
            xps_free(ctx, inbuf);
            return gs_throw1(-1, "zlib inflate error: %s", stream.msg);
        }
        code = inflateEnd(&stream);
        if (code != Z_OK)
        {
            xps_free(ctx, inbuf);
            return gs_throw1(-1, "zlib inflateEnd error: %s", stream.msg);
        }

        xps_free(ctx, inbuf);

        /* If the stream has less data than advertised, then zero the remainder. */
        if (stream.avail_out > 0)
        {
            gs_warn("truncated zipfile entry; possibly corrupt data");
            memset(stream.next_out, 0, stream.avail_out);
        }
    }
    else
    {
        return gs_throw1(-1, "unknown compression method (%d)", method);
    }

    return gs_okay;
}

/*
 * Read the central directory in a zip file.
 */

static int
xps_read_zip_dir(xps_context_t *ctx, int start_offset)
{
    int sig;
    int offset, count, read;
    int namesize, metasize, commentsize;
    int i;

    if (xps_fseek(ctx->file, start_offset, 0) != 0)
        return gs_throw1(gs_error_ioerror, "xps_fseek to %d failed.", start_offset);

    sig = getlong(ctx->file);
    if (sig != ZIP_END_OF_CENTRAL_DIRECTORY_SIG)
        return gs_throw1(-1, "wrong zip end of central directory signature (0x%x)", sig);

    (void) getshort(ctx->file); /* this disk */
    (void) getshort(ctx->file); /* start disk */
    (void) getshort(ctx->file); /* entries in this disk */
    count = getshort(ctx->file); /* entries in central directory disk */
    (void) getlong(ctx->file); /* size of central directory */
    offset = getlong(ctx->file); /* offset to central directory */

    if (count < 0 || count > 65535)
        return gs_rethrow(gs_error_rangecheck, "invalid number of entries in central directory disk (can't happen)");

    ctx->zip_count = count;
    ctx->zip_table = xps_alloc(ctx, sizeof(xps_entry_t) * count);
    if (!ctx->zip_table)
        return gs_rethrow(gs_error_VMerror, "cannot allocate zip entry table");

    memset(ctx->zip_table, 0, sizeof(xps_entry_t) * count);

    if (xps_fseek(ctx->file, offset, 0) != 0)
        return gs_throw1(gs_error_ioerror, "xps_fseek to offset %d failed", offset);

    for (i = 0; i < count; i++)
    {
        sig = getlong(ctx->file);
        if (sig != ZIP_CENTRAL_DIRECTORY_SIG)
            return gs_throw1(-1, "wrong zip central directory signature (0x%x)", sig);

        (void) getshort(ctx->file); /* version made by */
        (void) getshort(ctx->file); /* version to extract */
        (void) getshort(ctx->file); /* general */
        (void) getshort(ctx->file); /* method */
        (void) getshort(ctx->file); /* last mod file time */
        (void) getshort(ctx->file); /* last mod file date */
        (void) getlong(ctx->file); /* crc-32 */
        ctx->zip_table[i].csize = getlong(ctx->file);
        ctx->zip_table[i].usize = getlong(ctx->file);
        namesize = getshort(ctx->file);
        if (namesize < 0 || namesize > 65535)
            return gs_rethrow(gs_error_rangecheck, "illegal namesize (can't happen)");
        metasize = getshort(ctx->file);
        commentsize = getshort(ctx->file);
        (void) getshort(ctx->file); /* disk number start */
        (void) getshort(ctx->file); /* int file atts */
        (void) getlong(ctx->file); /* ext file atts */
        ctx->zip_table[i].offset = getlong(ctx->file);

	if (ctx->zip_table[i].csize < 0 || ctx->zip_table[i].usize < 0)
            return gs_throw(gs_error_ioerror, "cannot read zip entries larger than 2GB");

        ctx->zip_table[i].name = xps_alloc(ctx, namesize + 1);
        if (!ctx->zip_table[i].name)
            return gs_rethrow(gs_error_VMerror, "cannot allocate zip entry name");

        read = xps_fread(ctx->zip_table[i].name, 1, namesize, ctx->file);
        if (read != namesize)
            return gs_throw1(gs_error_ioerror, "failed to read %d bytes", namesize);

        ctx->zip_table[i].name[namesize] = 0;

        if (xps_fseek(ctx->file, metasize, 1) != 0)
            return gs_throw1(gs_error_ioerror, "xps_fseek to offset %d failed", metasize);
        if (xps_fseek(ctx->file, commentsize, 1) != 0)
            return gs_throw1(gs_error_ioerror, "xps_fseek to offset %d failed", commentsize);
    }

    qsort(ctx->zip_table, count, sizeof(xps_entry_t), xps_compare_entries);

    for (i = 0; i < ctx->zip_count; i++)
    {
        if_debug3m('|', ctx->memory, "zip entry '%s' csize=%d usize=%d\n",
                   ctx->zip_table[i].name,
                   ctx->zip_table[i].csize,
                   ctx->zip_table[i].usize);
    }

    return gs_okay;
}

static int
xps_find_and_read_zip_dir(xps_context_t *ctx)
{
    int filesize, back, maxback;
    int i, n;
    char buf[512];

    xps_fseek(ctx->file, 0, SEEK_END);
    filesize = xps_ftell(ctx->file);

    maxback = MIN(filesize, 0xFFFF + sizeof buf);
    back = MIN(maxback, sizeof buf);

    while (back < maxback)
    {
        xps_fseek(ctx->file, filesize - back, 0);

        n = xps_fread(buf, 1, sizeof buf, ctx->file);
        if (n < 0)
            return gs_throw(-1, "cannot read end of central directory");

        for (i = n - 4; i > 0; i--)
            if (!memcmp(buf + i, "PK\5\6", 4))
                return xps_read_zip_dir(ctx, filesize - back + i);

        back += sizeof buf - 4;
    }

    return gs_throw(-1, "cannot find end of central directory");
}

/*
 * Read and interleave split parts from a ZIP file.
 */

static xps_part_t *
xps_read_zip_part(xps_context_t *ctx, const char *partname)
{
    char buf[2048];
    xps_entry_t *ent;
    xps_part_t *part;
    int count, size, offset, i;
    int code = 0;
    const char *name;
    int seen_last = 0;

    name = partname;
    if (name[0] == '/')
        name ++;

    /* All in one piece */
    ent = xps_find_zip_entry(ctx, name);
    if (ent)
    {
        part = xps_new_part(ctx, partname, ent->usize);
        if (part != NULL)
            code = xps_read_zip_entry(ctx, ent, part->data);
        else
            return NULL;
        if (code < 0)
        {
            xps_free_part(ctx, part);
            gs_rethrow1(-1, "cannot read zip entry '%s'", name);
            return NULL;
        }
        return part;
    }

    /* Count the number of pieces and their total size */
    count = 0;
    size = 0;
    while (!seen_last)
    {
        gs_sprintf(buf, "%s/[%d].piece", name, count);
        ent = xps_find_zip_entry(ctx, buf);
        if (!ent)
        {
            gs_sprintf(buf, "%s/[%d].last.piece", name, count);
            ent = xps_find_zip_entry(ctx, buf);
            seen_last = !!ent;
        }
        if (!ent)
            break;
        count ++;
        size += ent->usize;
    }
    if (!seen_last)
    {
        gs_throw1(-1, "cannot find all pieces for part '%s'", partname);
        return NULL;
    }

    /* Inflate the pieces */
    if (count)
    {
        part = xps_new_part(ctx, partname, size);
        offset = 0;
        for (i = 0; i < count; i++)
        {
            if (i < count - 1)
                gs_sprintf(buf, "%s/[%d].piece", name, i);
            else
                gs_sprintf(buf, "%s/[%d].last.piece", name, i);
            ent = xps_find_zip_entry(ctx, buf);
            if (!ent)
                gs_warn("missing piece");
            else
            {
                code = xps_read_zip_entry(ctx, ent, part->data + offset);
                if (code < 0)
                {
                    xps_free_part(ctx, part);
                    gs_rethrow1(-1, "cannot read zip entry '%s'", buf);
                    return NULL;
                }
                offset += ent->usize;
            }
        }
        return part;
    }

    return NULL;
}

/*
 * Read and interleave split parts from files in the directory.
 */

static xps_part_t *
xps_read_dir_part(xps_context_t *ctx, const char *name)
{
    char buf[2048];
    xps_part_t *part;
    FILE *file;
    int count, size, offset, i, n;

    gs_strlcpy(buf, ctx->directory, sizeof buf);
    gs_strlcat(buf, name, sizeof buf);

    /* All in one piece */
    file = gp_fopen(buf, "rb");
    if (file)
    {
        if (xps_fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return NULL;
        }

        size = xps_ftell(file);
        if (size < 0) {
            fclose(file);
            return NULL;
        }

        if (xps_fseek(file, 0, SEEK_SET) != 0) {
            fclose(file);
            return NULL;
        }

        part = xps_new_part(ctx, name, size);
        count = xps_fread(part->data, 1, size, file);
        fclose(file);
        if (count == size)
            return part;
        else
            return NULL;
    }

    /* Count the number of pieces and their total size */
    count = 0;
    size = 0;
    while (1)
    {
        gs_sprintf(buf, "%s%s/[%d].piece", ctx->directory, name, count);
        file = gp_fopen(buf, "rb");
        if (!file)
        {
            gs_sprintf(buf, "%s%s/[%d].last.piece", ctx->directory, name, count);
            file = gp_fopen(buf, "rb");
        }
        if (!file)
            break;
        count ++;
        xps_fseek(file, 0, SEEK_END);
        size += xps_ftell(file);
        fclose(file);
    }

    /* Inflate the pieces */
    if (count)
    {
        part = xps_new_part(ctx, name, size);
        offset = 0;
        for (i = 0; i < count; i++)
        {
            if (i < count - 1)
                gs_sprintf(buf, "%s%s/[%d].piece", ctx->directory, name, i);
            else
                gs_sprintf(buf, "%s%s/[%d].last.piece", ctx->directory, name, i);
            file = gp_fopen(buf, "rb");
            n = xps_fread(part->data + offset, 1, size - offset, file);
            offset += n;
            fclose(file);
        }
        return part;
    }

    return NULL;
}

xps_part_t *
xps_read_part(xps_context_t *ctx, const char *partname)
{
    if (ctx->directory)
        return xps_read_dir_part(ctx, partname);
    return xps_read_zip_part(ctx, partname);
}

/*
 * Read and process the XPS document.
 */

static int
xps_read_and_process_metadata_part(xps_context_t *ctx, const char *name)
{
    xps_part_t *part;
    int code;

    part = xps_read_part(ctx, name);
    if (!part)
        return gs_rethrow1(-1, "cannot read zip part '%s'", name);

    code = xps_parse_metadata(ctx, part);
    if (code)
    {
        xps_free_part(ctx, part);
        return gs_rethrow1(code, "cannot process metadata part '%s'", name);
    }

    xps_free_part(ctx, part);

    return gs_okay;
}

static int
xps_read_and_process_page_part(xps_context_t *ctx, char *name)
{
    xps_part_t *part;
    int code;

    part = xps_read_part(ctx, name);
    if (!part)
        return gs_rethrow1(-1, "cannot read zip part '%s'", name);

    code = xps_parse_fixed_page(ctx, part);
    if (code)
    {
        xps_free_part(ctx, part);
        return gs_rethrow1(code, "cannot parse fixed page part '%s'", name);
    }

    xps_free_part(ctx, part);

    return gs_okay;
}

/*
 * Called by xpstop.c
 */

int
xps_process_file(xps_context_t *ctx, char *filename)
{
    char buf[2048];
    xps_document_t *doc;
    xps_page_t *page;
    int code;
    char *p;

    ctx->file = xps_fopen(filename, "rb");
    if (!ctx->file)
        return gs_throw1(-1, "cannot open file: '%s'", filename);

    if (strstr(filename, ".fpage"))
    {
        xps_part_t *part;
        int size;

        if_debug0m('|', ctx->memory, "zip: single page mode\n");
        gs_strlcpy(buf, filename, sizeof buf);
        while (1)
        {
            p = strrchr(buf, '/');
            if (!p)
                p = strrchr(buf, '\\');
            if (!p)
                break;
            gs_strlcpy(p, "/_rels/.rels", buf + sizeof buf - p);
            if_debug1m('|', ctx->memory, "zip: testing if '%s' exists\n", buf);
            if (isfile(buf))
            {
                *p = 0;
                ctx->directory = xps_strdup(ctx, buf);
                if_debug1m('|', ctx->memory, "zip: using '%s' as root directory\n", ctx->directory);
                break;
            }
            *p = 0;
        }
        if (!ctx->directory)
        {
            if_debug0m('|', ctx->memory, "zip: no /_rels/.rels found; assuming absolute paths\n");
            ctx->directory = xps_strdup(ctx, "");
        }

        if (xps_fseek(ctx->file, 0, SEEK_END) != 0) {
            code = gs_rethrow(gs_error_ioerror, "xps_fseek to file end failed");
            goto cleanup;
        }

        size = xps_ftell(ctx->file);
        if (size < 0) {
            code = gs_rethrow(gs_error_ioerror, "xps_ftell raised an error");
            goto cleanup;
        }

        if (xps_fseek(ctx->file, 0, SEEK_SET) != 0) {
            code = gs_rethrow(gs_error_ioerror, "xps_fseek to file begin failed");
            goto cleanup;
        }

        part = xps_new_part(ctx, filename, size);
        code = xps_fread(part->data, 1, size, ctx->file);
        if (code != size) {
            code = gs_rethrow1(gs_error_ioerror, "failed to read %d bytes", size);
            xps_free_part(ctx, part);
            goto cleanup;
        }

        code = xps_parse_fixed_page(ctx, part);
        if (code)
        {
            code = gs_rethrow1(code, "cannot parse fixed page part '%s'", part->name);
            xps_free_part(ctx, part);
            goto cleanup;
        }

        xps_free_part(ctx, part);
        code = gs_okay;
        goto cleanup;
    }

    if (strstr(filename, "/_rels/.rels") || strstr(filename, "\\_rels\\.rels"))
    {
        gs_strlcpy(buf, filename, sizeof buf);
        p = strstr(buf, "/_rels/.rels");
        if (!p)
            p = strstr(buf, "\\_rels\\.rels");
        *p = 0;
        ctx->directory = xps_strdup(ctx, buf);
        if_debug1m('|', ctx->memory, "zip: using '%s' as root directory\n", ctx->directory);
    }
    else
    {
        code = xps_find_and_read_zip_dir(ctx);
        if (code < 0)
        {
            code = gs_rethrow(code, "cannot read zip central directory");
            goto cleanup;
        }
    }

    code = xps_read_and_process_metadata_part(ctx, "/_rels/.rels");
    if (code)
    {
        code = gs_rethrow(code, "cannot process root relationship part");
        goto cleanup;
    }

    if (!ctx->start_part)
    {
        code = gs_rethrow(-1, "cannot find fixed document sequence start part");
        goto cleanup;
    }

    code = xps_read_and_process_metadata_part(ctx, ctx->start_part);
    if (code)
    {
        code = gs_rethrow(code, "cannot process FixedDocumentSequence part");
        goto cleanup;
    }

    for (doc = ctx->first_fixdoc; doc; doc = doc->next)
    {
        code = xps_read_and_process_metadata_part(ctx, doc->name);
        if (code)
        {
            code = gs_rethrow(code, "cannot process FixedDocument part");
            goto cleanup;
        }
    }

    for (page = ctx->first_page; page; page = page->next)
    {
        code = xps_read_and_process_page_part(ctx, page->name);
        if (code)
        {
            code = gs_rethrow(code, "cannot process FixedPage part");
            goto cleanup;
        }
    }

    code = gs_okay;

cleanup:
    if (ctx->directory)
        xps_free(ctx, ctx->directory);
    if (ctx->file)
        xps_fclose(ctx->file);
    return code;
}
