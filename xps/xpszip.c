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

/* XPS interpreter - zip container parsing */

#include "ghostxps.h"

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_DATA_DESC_SIG 0x08074b50
#define ZIP_CENTRAL_DIRECTORY_SIG 0x02014b50

int xps_zip_trace = 0;

static inline unsigned int
scan4(byte *buf)
{
    unsigned int a = buf[0];
    unsigned int b = buf[1];
    unsigned int c = buf[2];
    unsigned int d = buf[3];
    return a | (b << 8) | (c << 16) | (d << 24);
}

static inline unsigned int
scan8(byte *buf)
{
    return scan4(buf); /* skip high bytes */
}

static inline int
read1(xps_context_t *ctx, stream_cursor_read *buf)
{
    if (buf->ptr >= buf->limit)
	return -1;
    buf->ptr++;
    return *buf->ptr;
}

static inline int
read2(xps_context_t *ctx, stream_cursor_read *buf)
{
    int a = read1(ctx, buf);
    int b = read1(ctx, buf);
    return a | (b << 8);
}

static inline int
read4(xps_context_t *ctx, stream_cursor_read *buf)
{
    int a = read1(ctx, buf);
    int b = read1(ctx, buf);
    int c = read1(ctx, buf);
    int d = read1(ctx, buf);
    return a | (b << 8) | (c << 16) | (d << 24);
}

static inline int
read8(xps_context_t *ctx, stream_cursor_read *buf)
{
    int a = read4(ctx, buf);
    (void) read4(ctx, buf); /* skip high bytes */
    return a;
}

static inline void
readall(xps_context_t *ctx, stream_cursor_read *buf, byte *str, int n)
{
    while (n--)
	*str++ = read1(ctx, buf);
}

static void *
xps_zip_alloc_items(xps_context_t *ctx, int items, int size)
{
    return xps_alloc(ctx, items * size);
}

static void
xps_zip_free(xps_context_t *ctx, void *ptr)
{
    xps_free(ctx, ptr);
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
    part->image = NULL;
    part->icc = NULL;
    part->xml = NULL;

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
    part->next = ctx->first_part;
    ctx->first_part = part;

    /* add it to the hash table of parts */
    xps_hash_insert(ctx, ctx->part_table, part->name, part);

    return part;
}

void
xps_free_part_caches(xps_context_t *ctx, xps_part_t *part)
{
#if 0
    /* Can't free fonts because pdfwrite needs them alive */
    if (part->font)
    {
	xps_free_font(ctx, part->font);
	part->font = NULL;
    }

    if (part->icc)
    {
	xps_free_colorspace(ctx, part->icc);
	part->icc = NULL;
    }
#endif

    if (part->image)
    {
	xps_free_image(ctx, part->image);
	part->image = NULL;
    }

    if (part->xml)
    {
	xps_free_item(ctx, part->xml);
	part->xml = NULL;
    }
}

void
xps_free_part(xps_context_t *ctx, xps_part_t *part)
{
    xps_free_part_caches(ctx, part);

    /* Nu-uh, can't free fonts because pdfwrite needs them alive */
    if (part->font)
	return;

    if (part->name) xps_free(ctx, part->name);
    if (part->data) xps_free(ctx, part->data);

    part->name = NULL;
    part->data = NULL;

    xps_free_relations(ctx, part->relations);
    xps_free(ctx, part);
}

xps_part_t *
xps_find_part(xps_context_t *ctx, char *name)
{
    return xps_hash_lookup(ctx->part_table, name);
}

static int
xps_prepare_part(xps_context_t *ctx)
{
    xps_part_t *part;
    int piece;
    int last_piece;
    int code;

    if (strstr(ctx->zip_file_name, ".piece"))
    {
	piece = 1;
	if (strstr(ctx->zip_file_name, ".last.piece"))
	    last_piece = 1;
	else
	    last_piece = 0;
    }
    else
    {
	piece = 0;
	last_piece = 1;
    }

    if (piece)
    {
	char *p = strrchr(ctx->zip_file_name, '/');
	if (p)
	    *p = 0;
    }

    part = xps_find_part(ctx, ctx->zip_file_name);
    if (!part)
    {
	part = xps_new_part(ctx, ctx->zip_file_name, ctx->zip_uncompressed_size);
	if (!part)
	    return gs_rethrow(-1, "cannot create part buffer");

	ctx->last_part = part; /* make it the current part */
    }
    else
    {
	part->interleave = part->size; /* save where this interleaved part starts for checksums */

	if (ctx->zip_uncompressed_size != 0)
	{
	    part->capacity = part->size + ctx->zip_uncompressed_size; /* grow to exact size */
	    part->data = xps_realloc(ctx, part->data, part->capacity);
	    if (!part->data)
		return gs_throw(-1, "cannot extend part buffer");
	}

	ctx->last_part = part;
    }

    ctx->last_part->complete = last_piece;

    /* init decompression */
    if (ctx->zip_method == 8)
    {
	memset(&ctx->zip_stream, 0, sizeof(z_stream));
	ctx->zip_stream.zalloc = (alloc_func)xps_zip_alloc_items;
	ctx->zip_stream.zfree = (free_func)xps_zip_free;
	ctx->zip_stream.opaque = ctx;

	code = inflateInit2(&ctx->zip_stream, -15);
	if (code != Z_OK)
	    return gs_throw1(-1, "cannot inflateInit2(): %d", code);
	return 0;
    }
    else if (ctx->zip_method == 0)
    {
	return 0;
    }
    else
    {
	return gs_throw(-1, "unknown compression method");
    }
}

/* return -1 = fail, 0 = need more data, 1 = finished */
static int
xps_read_part(xps_context_t *ctx, stream_cursor_read *buf)
{
    xps_part_t *part = ctx->last_part;
    unsigned int crc32;
    unsigned int csize;
    unsigned int usize;
    int code;
    int sixteen;

    if (ctx->zip_method == 8)
    {
	if (part->size >= part->capacity)
	{
	    /* dprintf2("growing buffer (%d/%d)\n", part->size, part->capacity); */
	    part->capacity += 8192;
	    part->data = xps_realloc(ctx, part->data, part->capacity);
	    if (!part->data)
		return gs_throw(-1, "out of memory");
	}

	ctx->zip_stream.next_in = buf->ptr + 1;
	ctx->zip_stream.avail_in = buf->limit - buf->ptr;
	ctx->zip_stream.next_out = part->data + part->size;
	ctx->zip_stream.avail_out = part->capacity - part->size;

	code = inflate(&ctx->zip_stream, Z_NO_FLUSH);
	buf->ptr = ctx->zip_stream.next_in - 1;
	part->size = part->capacity - ctx->zip_stream.avail_out;

	if (code == Z_STREAM_END)
	{
	    inflateEnd(&ctx->zip_stream);
	    return 1;
	}
	else if (code == Z_OK || code == Z_BUF_ERROR)
	{
	    return 0;
	}
	else
	{
	    inflateEnd(&ctx->zip_stream);
	    return gs_throw2(-1, "inflate returned %d (%s)", code,
		    ctx->zip_stream.msg ? ctx->zip_stream.msg : "no error message");
	}
    }

    if (ctx->zip_method == 0 && (ctx->zip_general & 8))
    {
	/* A stored part with an unknown size. This is a brain damaged file
	 * allowed by a brain damaged spec and written by a brain damaged company.
	 */

	sixteen = ctx->zip_version < 45 ? 16 : 24;

	while (1)
	{
	    if (part->size >= part->capacity)
	    {
		/* dprintf2("growing buffer (%d/%d)\n", part->size, part->capacity); */
		part->capacity += 8192;
		part->data = xps_realloc(ctx, part->data, part->capacity);
		if (!part->data)
		    return gs_throw(-1, "out of memory");
	    }

	    /* A workaround to handle this case is to look for signatures.
	     * Check the crc32 just to be on the safe side.
	     */

	    if (part->size >= sixteen)
	    {
		if (scan4(part->data + part->size - sixteen) == ZIP_DATA_DESC_SIG)
		{
		    if (ctx->zip_version < 45)
		    {
			crc32 = scan4(part->data + part->size - 12);
			csize = scan4(part->data + part->size - 8);
			usize = scan4(part->data + part->size - 4);
		    }
		    else
		    {
			crc32 = scan4(part->data + part->size - 20);
			csize = scan4(part->data + part->size - 16);
			usize = scan4(part->data + part->size - 8);
		    }

		    if (csize == usize && usize == part->size - part->interleave - sixteen)
		    {
			if (crc32 == xps_crc32(0, part->data + part->interleave, part->size - part->interleave - sixteen))
			{
			    part->size -= sixteen;
			    return 1;
			}
		    }
		}
	    }

	    code = read1(ctx, buf);
	    if (code < 0)
		return 0;

	    part->data[part->size++] = code;
	}
    }

    else
    {
	/* For stored parts we usually know the size,
	 * and then capacity is set to the actual size of the data.
	 */

	if (ctx->zip_uncompressed_size == 0)
	    return 1;

	while (part->size < part->capacity)
	{
	    int c = read1(ctx, buf);
	    if (c < 0)
		return 0;
	    part->data[part->size++] = c;
	}

	return 1;
    }
}

int
xps_process_data(xps_context_t *ctx, stream_cursor_read *buf)
{
    unsigned int signature;
    int code;

    /* dprintf1("xps_process_data state=%d\n", ctx->zip_state); */

    if (getenv("XPS_ZIP_TRACE"))
	xps_zip_trace = 1;

    while (1)
    {
	switch (ctx->zip_state)
	{
	case -1:
	    /* at the end, or error condition. toss data. */
	    buf->ptr = buf->limit;
	    return 1;

	case 0: /* between parts looking for a signature */
	    do
	    {
		if (buf->limit - buf->ptr < 4 + 12)
		    return 0;

		signature = read4(ctx, buf);
		if (signature == ZIP_LOCAL_FILE_SIG)
		{
		    if (xps_zip_trace)
			dputs("zip: local file signature\n");
		}
		else if (signature == ZIP_DATA_DESC_SIG)
		{
		    if (xps_zip_trace)
			dputs("zip: data desc signature\n");
		    if (ctx->zip_version >= 45)
		    {
			(void) read4(ctx, buf); /* crc32 */
			(void) read8(ctx, buf); /* csize */
			(void) read8(ctx, buf); /* usize */
		    }
		    else
		    {
			(void) read4(ctx, buf); /* crc32 */
			(void) read4(ctx, buf); /* csize */
			(void) read4(ctx, buf); /* usize */
		    }
		}
		else if (signature == ZIP_CENTRAL_DIRECTORY_SIG)
		{
		    if (xps_zip_trace)
			dputs("zip: central directory signature\n");
		    ctx->zip_state = -1;
		    return 0;
		}
		else
		{
		    if (xps_zip_trace)
			dprintf1("zip: unknown signature 0x%x\n", signature);
		    ctx->zip_state = -1;
		    return 0;
		}
	    }
	    while (signature != ZIP_LOCAL_FILE_SIG);

	    ctx->zip_state ++;

	case 1: /* local file header */
	    {
		if (buf->limit - buf->ptr < 26)
		    return 0;

		ctx->zip_version = read2(ctx, buf);
		ctx->zip_general = read2(ctx, buf);
		ctx->zip_method = read2(ctx, buf);
		(void) read2(ctx, buf); /* file time */
		(void) read2(ctx, buf); /* file date */
		(void) read4(ctx, buf); /* crc32 */
		ctx->zip_compressed_size = read4(ctx, buf);
		ctx->zip_uncompressed_size = read4(ctx, buf);
		ctx->zip_name_length = read2(ctx, buf);
		ctx->zip_extra_length = read2(ctx, buf);
	    }
	    ctx->zip_state ++;

	case 2: /* file name */
	    {
		if (buf->limit - buf->ptr < ctx->zip_name_length)
		    return 0;
		if (ctx->zip_name_length >= sizeof(ctx->zip_file_name) + 2)
		    return gs_throw(-1, "part name too long");
		ctx->zip_file_name[0] = '/';
		readall(ctx, buf, (byte*)ctx->zip_file_name + 1, ctx->zip_name_length);
		ctx->zip_file_name[ctx->zip_name_length + 1] = 0;

		if (xps_zip_trace)
		    dprintf1("zip: entry %s\n", ctx->zip_file_name);
	    }
	    ctx->zip_state ++;

	case 3: /* extra field */

	    /* work around for the fixed size cursor buffer */
	    if (ctx->zip_extra_length > buf->limit - buf->ptr)
	    {
		ctx->zip_extra_length -= buf->limit - buf->ptr;
		buf->limit = buf->ptr;
	    }

	    if (buf->limit - buf->ptr < ctx->zip_extra_length)
		return 0;
	    buf->ptr += ctx->zip_extra_length;
	    ctx->zip_state ++;

	    /* prepare the correct part for receiving data */
	    code = xps_prepare_part(ctx);
	    if (code < 0)
		return gs_throw(-1, "cannot create part");

	case 4: /* file data */

	    while (ctx->zip_state == 4)
	    {
		code = xps_read_part(ctx, buf);
		if (code < 0)
		    return gs_throw(-1, "cannot read part");
		if (code == 0)
		    return 0;
		if (code == 1)
		    ctx->zip_state ++;
	    }

	case 5: /* end of part (data descriptor) */

	    ctx->zip_state = 0;

	    /* Process contents of part.
	     * This is the entrance to the real parser.
	     */
	    code = xps_process_part(ctx, ctx->last_part);
	    if (code < 0)
		return gs_rethrow(code, "cannot handle part");
	}
    }

    return 0;
}

