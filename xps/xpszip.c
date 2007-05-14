#include "ghostxps.h"

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_DATA_DESC_SIG 0x08074b50

static inline unsigned int
scan4(byte *buf)
{
    unsigned int a = buf[0];
    unsigned int b = buf[1];
    unsigned int c = buf[2];
    unsigned int d = buf[3];
    return a | (b << 8) | (c << 16) | (d << 24);
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
    part->capacity = 0;
    part->complete = 0;
    part->data = NULL;
    part->relations = NULL;
    part->relations_complete = 0;
    part->next = NULL;

    part->font = NULL;
    part->image = NULL;

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

    /* add it to the list of parts */
    if (!ctx->first_part)
    {
	ctx->first_part = part;
    }
    else
    {
	part->next = ctx->first_part;
	ctx->first_part = part;
    }

    return part;
}

void
xps_free_part(xps_context_t *ctx, xps_part_t *part)
{
    if (part->name) xps_free(ctx, part->name);
    if (part->data) xps_free(ctx, part->data);
    xps_free_relations(ctx, part->relations);
    xps_free(ctx, part);
}

xps_part_t *
xps_find_part(xps_context_t *ctx, char *name)
{
    xps_part_t *part;
    for (part = ctx->first_part; part; part = part->next)
	if (!strcmp(part->name, name))
	    return part;
    return NULL;
}

static int
xps_prepare_part(xps_context_t *ctx)
{
    xps_part_t *part;
    int piece;
    int last_piece;
    int code;

    dprintf1("unzipping part '%s'\n", ctx->zip_file_name);

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

    /* dprintf3("  to part '%s' (piece=%d) (last=%d)\n", ctx->zip_file_name, piece, last_piece); */

    part = xps_find_part(ctx, ctx->zip_file_name);
    if (!part)
    {
	part = xps_new_part(ctx, ctx->zip_file_name, ctx->zip_uncompressed_size);
	if (!part)
	    return gs_rethrow(code, "cannot create part buffer");
	ctx->last_part = part; /* make it the current part */
    }
    else
    {
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
    int code;

    if (ctx->zip_method == 8)
    {
	/* dputs("inflate data\n"); */

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

	/* dputs("stored data of unknown size\n"); */

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

	    if (part->size > 16)
	    {
		if (scan4(part->data + part->size - 16) == ZIP_DATA_DESC_SIG)
		{
		    unsigned int crc32 = scan4(part->data + part->size - 12);
		    unsigned int csize = scan4(part->data + part->size - 8);
		    unsigned int usize = scan4(part->data + part->size - 4);

		    if (csize == usize && usize == part->size - 16)
		    {
			if (crc32 == xps_crc32(0, part->data, part->size - 16))
			{
			    part->size -= 16;
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

	/* dprintf1("stored data of known size: %d\n", ctx->zip_uncompressed_size); */

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

    while (1)
    {
	switch (ctx->zip_state)
	{
	case -1:
	    /* at the end, or error condition. toss data. */
	    /* dputs("at end of zip (or in fail mode)\n"); */
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
		    dputs("local file signature\n");
		}
		else if (signature == ZIP_DATA_DESC_SIG)
		{
		    dputs("data desc signature\n");
		    (void) read4(ctx, buf); /* crc32 */
		    (void) read4(ctx, buf); /* csize */
		    (void) read4(ctx, buf); /* usize */
		}
		else
		{
		    ctx->zip_state = -1;

		    if (signature == 0x02014b50) /* central directory */
			return 0;
		    if (signature == 0x05054b50) /* digital signature */
			return 0;
		    if (signature == 0x06054b50) /* end of central directory */
			return 0;

		    /* some other unknown part. */
		    dprintf1("unknown signature 0x%x\n", signature);
		    return 0;
		}
	    }
	    while (signature != ZIP_LOCAL_FILE_SIG);

	    ctx->zip_state ++;

	case 1: /* local file header */
	    {
		if (buf->limit - buf->ptr < 26)
		    return 0;

		(void) read2(ctx, buf); /* version */
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
	    if (buf->limit - buf->ptr < ctx->zip_name_length)
		return 0;
	    if (ctx->zip_name_length >= sizeof(ctx->zip_file_name) + 2)
		return gs_throw(-1, "part name too long");
	    ctx->zip_file_name[0] = '/';
	    readall(ctx, buf, (byte*)ctx->zip_file_name + 1, ctx->zip_name_length);
	    ctx->zip_file_name[ctx->zip_name_length + 1] = 0;
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

	    if (0 && ctx->zip_general & 8)
	    {
		dputs("data descriptor by flag\n");
		if (buf->limit - buf->ptr < 16)
		    return 0;
		signature = read4(ctx, buf); /* crc32 ... or signature */
		if (signature == ZIP_DATA_DESC_SIG)
		    read4(ctx, buf); /* crc32 */
		read4(ctx, buf); /* csize */
		read4(ctx, buf); /* usize */
	    }

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

