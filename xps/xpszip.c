#include "ghostxps.h"

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_DATA_DESC_SIG 0x08074b50

void xps_debug_parts(xps_context_t *ctx)
{
    xps_part_t *part = ctx->root;
    while (part)
    {
	dprintf2("part '%s' size=%d\n", part->name, part->size);
	part = part->next;
    }
}

xps_part_t *
xps_new_part(xps_context_t *ctx)
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
    part->next = NULL;

    return part;
}

void
xps_free_part(xps_context_t *ctx, xps_part_t *part)
{
    if (part->name) xps_free(ctx, part->name);
    if (part->data) xps_free(ctx, part->data);
    xps_free(ctx, part);
}

static inline int
read1(xps_context_t *ctx, stream_cursor_read *buf)
{
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

static xps_part_t *
xps_find_part(xps_context_t *ctx, char *name)
{
    xps_part_t *part;
    for (part = ctx->root; part; part = part->next)
	if (!strcmp(part->name, name))
	    return part;
    return NULL;
}

static int
xps_create_part(xps_context_t *ctx)
{
    xps_part_t *part;

    part = xps_new_part(ctx);
    if (!part)
	return gs_throw(-1, "cannot allocate part");

    part->name = xps_strdup(ctx, ctx->zip_file_name);
    if (!part->name)
	return gs_throw(-1, "cannot strdup part name");

    /* setup data buffer */
    if (ctx->zip_uncompressed_size == 0)
    {
	part->size = 0;
	part->capacity = 1024;
	part->data = xps_alloc(ctx, part->capacity);
    }
    else
    {
	part->size = 0;
	part->capacity = ctx->zip_uncompressed_size;
	part->data = xps_alloc(ctx, part->capacity);
    }
    if (!part->data)
	return gs_throw(-1, "cannot allocate part buffer");

    /* add it to the list of parts */
    if (!ctx->root)
    {
	ctx->root = part;
    }
    else
    {
	part->next = ctx->root;
	ctx->root = part;
    }

    /* make it the current */
    ctx->last = part;

    return 0;
}

static int
xps_prepare_part(xps_context_t *ctx)
{
    xps_part_t *part;
    int piece;
    int last_piece;
    int code;

    /* dprintf1("unzipping part '%s'\n", ctx->zip_file_name); */

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
	code = xps_create_part(ctx);
	if (code < 0)
	    return gs_rethrow(code, "cannot create part buffer");
    }
    else
    {
	if (ctx->zip_uncompressed_size != 0)
	{
	    part->capacity += ctx->zip_uncompressed_size;
	    part->data = xps_realloc(ctx, part->data, part->capacity);
	    if (!part->data)
		return gs_throw(-1, "cannot extend part buffer");
	}
	ctx->last = part;
    }

    ctx->last->complete = last_piece;

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
    xps_part_t *part = ctx->last;
    int code;

    if (part->size >= part->capacity)
    {
	/* dprintf2("growing buffer (%d/%d)\n", part->size, part->capacity); */
	part->capacity += 8192;
	part->data = xps_realloc(ctx, part->data, part->capacity);
	if (!part->data)
	    return gs_throw(-1, "out of memory");
    }

    if (ctx->zip_method == 8)
    {
	ctx->zip_stream.next_in = buf->ptr + 1;
	ctx->zip_stream.avail_in = buf->limit - buf->ptr;
	ctx->zip_stream.next_out = part->data + part->size;
	ctx->zip_stream.avail_out = part->capacity - part->size;
	code = inflate(&ctx->zip_stream, Z_NO_FLUSH);
	buf->ptr = ctx->zip_stream.next_in - 1;
	part->size = part->capacity - ctx->zip_stream.avail_out;

	if (code == Z_STREAM_END)
	{
	    return 1;
	}
	else if (code == Z_OK)
	{
	    return 0;
	}
	else
	    return gs_throw(-1, "inflate() error");
    }
    else
    {
	/* dprintf1("stored data of known size: %d\n", ctx->zip_uncompressed_size); */
	int avail = buf->limit - buf->ptr;
	int remain = part->capacity - part->size;
	int count = avail;
	if (count > remain)
	    count = remain;
	/* dprintf1("  reading %d bytes\n", count); */
	readall(ctx, buf, part->data + part->size, count);
	part->size += count;
	if (part->size == ctx->zip_uncompressed_size)
	    return 1;
	return 0;
    }
}

int
xps_process_data(xps_context_t *ctx, stream_cursor_read *buf)
{
    unsigned int signature;
    int code;

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
		    /* dputs("local file signature\n"); */
		}
		else if (signature == ZIP_DATA_DESC_SIG)
		{
		    /* dputs("data desc signature\n"); */
		    unsigned int dd_crc32 = read4(ctx, buf);
		    unsigned int dd_csize = read4(ctx, buf);
		    unsigned int dd_usize = read4(ctx, buf);
		}
		else
		{
		    /* dprintf1("unknown signature %x\n", signature); */
		    /* some other unknown part. we're done with the file. quit. */
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

		unsigned int version = read2(ctx, buf);
		unsigned int general = read2(ctx, buf);
		unsigned int method = read2(ctx, buf);
		unsigned int filetime = read2(ctx, buf);
		unsigned int filedate = read2(ctx, buf);
		unsigned int crc32 = read4(ctx, buf);
		unsigned int csize = read4(ctx, buf);
		unsigned int usize = read4(ctx, buf);
		unsigned int namelength = read2(ctx, buf);
		unsigned int extralength = read2(ctx, buf);

		ctx->zip_general = general;
		ctx->zip_method = method;
		ctx->zip_name_length = namelength;
		ctx->zip_extra_length = extralength;
		ctx->zip_compressed_size = csize;
		ctx->zip_uncompressed_size = usize;
	    }
	    ctx->zip_state ++;

	case 2: /* file name */
	    if (buf->limit - buf->ptr < ctx->zip_name_length)
		return 0;
	    if (ctx->zip_name_length >= sizeof(ctx->zip_file_name) + 1)
		return gs_throw(-1, "part name too long");
	    readall(ctx, buf, (byte*)ctx->zip_file_name, ctx->zip_name_length);
	    ctx->zip_file_name[ctx->zip_name_length] = 0;
	    ctx->zip_state ++;

	case 3: /* extra field */

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

	case 5: /* data descriptor */

	    if (ctx->zip_general & 4)
	    {
		/* dputs("data descriptor by flag\n"); */
		if (buf->limit - buf->ptr < 12)
		    return 0;
		unsigned int dd_crc32 = read4(ctx, buf);
		unsigned int dd_csize = read4(ctx, buf);
		unsigned int dd_usize = read4(ctx, buf);
	    }

	    ctx->zip_state = 0;

	    /* Process contents of part.
	     * This is the entrance to the real parser.
	     */
	    code = xps_process_part(ctx, ctx->last);
	    if (code < 0)
		return gs_rethrow(code, "cannot handle part");
	}
    }

    return 0;
}

