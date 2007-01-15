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
    part->resource = NULL;
    part->free = NULL;
    part->next = NULL;

    return part;
}

void
xps_free_part(xps_context_t *ctx, xps_part_t *part)
{
    if (part->name) xps_free(ctx, part->name);
    if (part->data) xps_free(ctx, part->data);
    if (part->resource && part->free)
	part->free(ctx, part->resource);
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
xps_alloc_items(xps_context_t *ctx, int items, int size)
{
    return xps_alloc(ctx, items * size);
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

    dprintf3("  to part '%s' (piece=%d) (last=%d)\n", ctx->zip_file_name, piece, last_piece);

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
	ctx->zip_stream.zalloc = (alloc_func)xps_alloc_items;
	ctx->zip_stream.zfree = (free_func)xps_free;
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
	dprintf2("growing buffer (%d/%d)\n", part->size, part->capacity);
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
	dprintf1("stored data of known size: %d\n", ctx->zip_uncompressed_size);
	int avail = buf->limit - buf->ptr;
	int remain = part->capacity - part->size;
	int count = avail;
	if (count > remain)
	    count = remain;
	dprintf1("  reading %d bytes\n", count);
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
		    dputs("local file signature\n");
		}
		else if (signature == ZIP_DATA_DESC_SIG)
		{
		    dputs("data desc signature\n");
		    unsigned int dd_crc32 = read4(ctx, buf);
		    unsigned int dd_csize = read4(ctx, buf);
		    unsigned int dd_usize = read4(ctx, buf);
		}
		else
		{
		    dprintf1("unknown signature %x\n", signature);
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
		dputs("data descriptor by flag\n");
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

#if 0

private inline int 
stream_has(stream_cursor_read *pr, unsigned int cnt)
{
    return( pr->limit - pr->ptr >= cnt );
}


private inline byte 
read_byte(stream_cursor_read *pr)
{
    pr->ptr++;
    return *pr->ptr;
}

private inline unsigned int 
read2(stream_cursor_read *pr, unsigned short *a)
{
    if (stream_has(pr, 2)) {
	*a =  read_byte(pr) | (read_byte(pr) << 8) ;
	// dprintf2(gs_mem_ptr, "read2 %0hd:%0hx\n", *a, *a);
    }
    else
	return eNeedData;
    return 0;
}

private inline unsigned long 
read4(stream_cursor_read *pr, unsigned long *a)
{
    if (stream_has(pr, 4)) {
	*a = read_byte(pr) |  
	    (read_byte(pr) << 8) |  
	    (read_byte(pr) << 16) |  
	    (read_byte(pr) << 24);
	// dprintf2(gs_mem_ptr, "read4 %0ld:%0lx\n", *a, *a);
    }
    else
	return eNeedData;
    return 0;
}


private int 
zip_new_block(zip_state_t *pzip, part_t *part)
{
    int code = 0;
    zip_block_t *blk;

    if (pzip->free_list) {
	blk = pzip->free_list;
	pzip->free_list = blk->next;
	blk->next = NULL;
	blk->writeoffset = 0;
    }
    else {
	blk = (zip_block_t *) gs_alloc_bytes(pzip->memory, 
					     sizeof(zip_block_t), 
					     "zip_new_block");	
	if (blk == NULL)
	    return gs_error_VMerror;
	blk->writeoffset = 0;
	blk->next = NULL;
    }
    assert(part->write);
    assert(part->write->writeoffset == ZIP_BLOCK_SIZE);

    memset(blk->data, 0xF0, ZIP_BLOCK_SIZE);

    if (part->tail)
	part->tail->next = blk;
    part->tail = blk;

    return code;
}

private int 
zip_init_part(zip_state_t *pzip)
{
    part_t *part = (part_t *)gs_alloc_bytes(pzip->memory, size_of(part_t), 
						    "zip_init_part");

    part->parent = pzip;  /* back pointer */
    pzip->num_files++;
    pzip->parts[pzip->num_files - 1] = part;
    pzip->part_read_state = 0;
    part->tail = part->head = part->csaved = part->metasize = part->namesize = 0;
    zip_new_block(pzip, part);
    part->head = part->curr = part->tail;
    part->keep_it = false;  /* read once by default */
    part->buf = NULL;
    part->zs = NULL;
    part->newfile = true;
    
    return 0;
}


/* Can return eNeedData, eExitLanguage, error, 0  
 */
private int 
zip_read_part(zip_state_t *pzip, stream_cursor_read *pr)
{
    unsigned long id;
    unsigned short shortInt;
    unsigned long longInt;
    int code = 0;
    part_t *part =0;
    int i;

    switch( pzip->part_read_state ) {
    case 0:
	if ((code = read4(pr, &id)) != 0)
	    return code;
        if (id == 0x04034b50) {
	    if ((code = zip_init_part(pzip)) != 0 )
		return code;
	    part = pzip->parts[pzip->num_files - 1];
	} else { // if (id == 0x02014b50) 
	    // end of archive 
	    pzip->part_read_state = 20;
	    return 0;
	}
	pzip->part_read_state++;

    case 1: /* version to extract*/
	if ((code = read2(pr, &shortInt)) != 0)
	    return code;
	pzip->part_read_state++;

    case 2: /* general*/
	part = pzip->parts[pzip->num_files - 1];
	if ((code = read2(pr, &part->gp_bitflag)) != 0)
	    return code;
	pzip->part_read_state++;
	 
    case 3: /* method */
	part = pzip->parts[pzip->num_files - 1];
	if ((code = read2(pr, &part->comp_method)) != 0)
	    return code;
	pzip->part_read_state++;
   
    case 4: /* last mod file time */
	if ((code = read2(pr, &shortInt)) != 0)
	    return code;
	pzip->part_read_state++;
	    
    case 5: /* last mod file date */
	if ((code = read2(pr, &shortInt)) != 0)
	    return code;
	pzip->part_read_state++;

    case 6: /* crc-32 */
	if ((code = read4(pr, &longInt)) != 0)
	    return code;
	pzip->part_read_state++;

    case 7: /* csize */
	part = pzip->parts[pzip->num_files - 1];
	if ((code = read4(pr, &part->csize)) != 0)
	    return code;
	pzip->part_read_state++;	
	
    case 8: /* usize */
	part = pzip->parts[pzip->num_files - 1];
	if ((code = read4(pr, &part->usize)) != 0)
	    return code;
	pzip->part_read_state++;

    case 9: /* namesize */
	part = pzip->parts[pzip->num_files - 1];
	if ((code = read2(pr, &part->namesize)) != 0)
	    return code;
	pzip->part_read_state++;

    case 10: /* metasize */
	part = pzip->parts[pzip->num_files - 1];
	if ((code = read2(pr, &part->metasize)) != 0)
	    return code;
	pzip->part_read_state++; 

    case 11:
	part = pzip->parts[pzip->num_files - 1];
	if (stream_has(pr, part->namesize)) {
	    part->name = gs_alloc_bytes(pzip->memory, part->namesize + 1, "pzip part name");
	    if (part->name == NULL) 
		return gs_throw(-1, "out of memory");
	    memcpy(part->name, pr->ptr +1, part->namesize);
	    part->name[part->namesize] = 0;
	    pr->ptr += part->namesize;
	    pzip->part_read_state++;
	} 
	else 
	    return eNeedData;
    case 12:
	part = pzip->parts[pzip->num_files - 1];
	if (stream_has(pr, part->metasize)) {
	    pr->ptr += part->metasize;
	    pzip->part_read_state++;
	}
	else if (part->metasize){
	    part->metasize -= pr->limit - pr->ptr;
	    pr->ptr = pr->limit;
	    return eNeedData;
	}
    case 13:    
	pzip->read_state = 1;  /* read file after this header */
	pzip->part_read_state = 0; /* next call looks for header */
	break;

    case 20:
	/* read extra foo after last file 
	   [archive decryption header] (EFS)
	   [archive extra data record] (EFS)
	   [central directory]
	   [zip64 end of central directory record]
	   [zip64 end of central directory locator] 
	   [end of central directory record]
	*/
	for ( i = pr->limit - pr->ptr; i > 4; --i, pr->ptr++ ) {
	    if ( pr->ptr[1] == 0x50 && 
		 pr->ptr[2] == 0x4b &&
		 pr->ptr[3] == 0x05 &&
		 pr->ptr[4] == 0x06 ) {
		pzip->part_read_state++;
		pr->ptr += 4;  
		break;
	    }
	}
	break;
    case 21:

	if (stream_has(pr, 18) == 0 )
	    return eNeedData;
	pr->ptr += 16;
	if (stream_has(pr, 3)) {
	    read2(pr, &part->skip_len);
	    pzip->part_read_state++;
	} else {
	    pr->ptr = pr->limit - 1;
	    pzip->part_read_state = 0;  /* reset for next zip file */
	    return -102; // e_ExitLanguage;
	}
    case 22:
	if (stream_has(pr, part->skip_len) == 0 )
	    pr->ptr += part->skip_len;


	pzip->part_read_state = 0;  /* reset for next zip file */

	return -102; // e_ExitLanguage;
    
    default:
	return gs_throw(-1, "coding error");
    }
    
    return code;
}


private int 
zip_init_write_stream(zip_state_t *pzip, part_t *part)
{
    if (part->zs == NULL) {
	part->zs = gs_alloc_bytes(pzip->memory, size_of(z_stream), "zip z_stream");
	if (part->zs == NULL)
	    return gs_throw(-1, "out of memory");
	part->zs->zalloc = 0;
	part->zs->zfree = 0;
	part->zs->opaque = 0;
    }
    return 0;
}

private int 
zip_decompress_data(zip_state_t *pzip, stream_cursor_read *pin )
{
    int code = 0;
    part_t *part = pzip->parts[pzip->read_part];
    z_stream *zs = 0;
    long rlen = pin->limit - pin->ptr - 1;
    long wlen = 0;
    byte *wptr = NULL;

    int rstart = pin->ptr;
    long len;
    int status;

    /* add to tail of part's block list */
    wptr = &part->tail->data[part->tail->writeoffset];

    if (ZIP_BLOCK_SIZE <= part->tail->writeoffset + 1) {
	if ((code = zip_new_block(pzip, part)))
	    return gs_throw(code, "zip_new_block");
	wptr = &part->tail->data[part->tail->writeoffset];
	wlen = ZIP_BLOCK_SIZE;
    }
    else 
	wlen = ((ZIP_BLOCK_SIZE - 1) - part->tail->writeoffset) + 1;

    zip_init_write_stream(pzip, part);
	
    if (part->comp_method == 0) {
	int left = part->csize - part->csaved;
	if (left == 0)
	    return eEndOfStream;
	rlen = min(left, min(rlen, wlen));
	memcpy(wptr, pin->ptr, rlen);
	part->tail->writeoffset += rlen;
	part->csaved += rlen;
	pin->ptr += rlen;    
	if (part->csize && part->csaved == part->csize) 
	    return eEndOfStream;
    }
    else {  /* 8 == flate */
	zs = part->zs;
	zs->next_in = pin->ptr + 1;
	zs->avail_in = rlen;
	zs->next_out = wptr;
	zs->avail_out = wlen;

	if (part->newfile) 	/* init for new file */
	    inflateInit2(part->zs, -15);  /* neg zip, 15bit window */

	status = inflate(zs, Z_NO_FLUSH);

	part->csaved += zs->next_in - pin->ptr - 1;  // update comressed read for zips with csize
	pin->ptr = zs->next_in - 1;
	part->tail->writeoffset += zs->next_out - wptr;

	switch (status) {
        case Z_OK:
	    if (pin->ptr + 1 < pin->limit)
		code = 0;
	    else
		code = pin->ptr > rstart ? 0 : 1;
	    part->newfile = false;

	    if (part->csize && part->csaved == part->csize) {
		/* recursive call
		 * NB: need to test the end of part/end of file/no csize case. */
		code = zip_decompress_data(pzip, pin);
	    }
	    break;

        case Z_STREAM_END:
	    part->newfile = true;
	    inflateEnd(zs);
	    code = eEndOfStream; // was: EOFC;
	    break;

        default:
	    code = ERRC;
	    gs_throw(code, "inflate error");
	}
    }
    return code;
}

/* returns zero on ok else error, swallows status returns 
 */
int 
zip_decomp_process(met_parser_state_t *st, met_state_t *mets, zip_state_t *pzip, 
		   stream_cursor_read *pr)
{
    int code = 0;
    part_t *rpart = NULL;

    /* update reading pointer 
     * NB: client should be able to choose a different file to read
     * currently just serially sends file after file.
     */

    if (pzip->read_part < 0 && pzip->num_files > 0)
	pzip->read_part = 0;
    if (pzip->read_part >= pzip->num_files )
	return code; // Nothing to do til we have some data.

    rpart = pzip->parts[pzip->read_part];

    /* decompress and send data to parser */
    code = zip_decompress_data(pzip, pr);
    if (code == eEndOfStream) { // good end of input stream 
	code = 0;	

	/* determine if the xps should be parsed and call the xml parser
	 */
	if ( pzip->inline_mode )
	    zip_page(st, mets, pzip, rpart);

	/* setup to read next part */
	pzip->read_part++;
	pzip->read_state = 2; 
    }
    else if (code == eWriteStall) { /* write stall on output */
	long len = rpart->s.r.limit - rpart->s.r.ptr;
	if ( len && rpart->buf -1 != rpart->s.r.ptr )
	    memmove(rpart->buf, rpart->s.r.ptr, len);
	rpart->s.r.ptr = rpart->buf -1;
	rpart->s.r.limit = rpart->buf + ( len - 1 );
	
	code = 0;  /* let it be read */
    }
    if (code == eNeedData) 
	code = 0;
    return code;
}


/* 
 * code == eNeedData is returned as 0 here.
 */
int
zip_process(met_parser_state_t *st, met_state_t *mets, zip_state_t *pzip, stream_cursor_read *pr)
{
    int code = 0;
    part_t *rpart = NULL;
    part_t *wpart = NULL;
    int last_len = pr->limit - pr->ptr;

    while (pr->limit - pr->ptr > 4 && code == 0) {

	switch (pzip->read_state) {
	case 0: /* local file header and skip of end of zip file records */
	    if ((code = zip_read_part(pzip, pr)) != 0) {
		return code;
	    }
	    /* 0 : more header to read 
	     * 1 : file to read
	     * 3 : end of file 
	     */
	    break;
	case 1: /* file data */	
	    /* We don't save compressed data since the 'standard' XPS files don't have lengths
	     */

	case 2: /* optional Data descriptor */
	    wpart = pzip->parts[pzip->num_files - 1];
	    if (wpart && wpart->csaved && wpart->csaved == wpart->csize ) {
		/* have count and fall through */

		if (wpart->gp_bitflag & 1 << 3) { 
		    /* Skip Data descriptor */
		    if (stream_has(pr, 12) == 0) {
			pzip->read_state = 2;
			return 0;
		    }
		    pr->ptr += 12; 
		}
		pzip->read_state = 0; 
		break;
	    }
	    else if (pzip->read_state == 2  && wpart->gp_bitflag & 1 << 3) {
		 /* else end of stream goto after decompress */ 
		if ( pr->ptr[1] == 0x50 &&
		     pr->ptr[2] == 0x4b &&
		     pr->ptr[3] == 0x07 &&
		     pr->ptr[4] == 0x08 ) {
		    /* check for split archive marker in the middle of the file
		     * spec says it must be first ?
		     */
		    if (stream_has(pr, 16) == 0) 
			return 0;
		    pr->ptr += 16; 
		}
		else {
		    if (stream_has(pr, 12) == 0) 
			return 0;
		    pr->ptr += 12; 
		}
		pzip->read_state = 0; 
		break;
	    }

	case 3:
    
	    if ((code = zip_decomp_process(st, mets, pzip, pr)) != 0 )
		return code;
	    break;

	case 4:
	default :
	    break;
	    //pzip->read_state = 0; 
	}
    }
    if (code == eNeedData) 
	return 0;
    return code;
}

private int
zip_initialize(zip_state_t *pzip)
{
    int i = 0;

    if (pzip->memory == 0)
	return -1;
    
#ifdef PARSE_XML_HACK
    pzip->zip_mode = false;
#else
    pzip->zip_mode = true;
#endif

    pzip->inline_mode = false;  /* false == buffer entire job in ram! */

    pzip->need_dir = false;
    pzip->needfiledata = false;
    pzip->read_state = 0;
    pzip->part_read_state = 0;
    pzip->read_part = -1;
    pzip->num_files = 0;
    for (i=0; i < 5000;  i++)
	pzip->parts[i] = 0;

    return 0;
}

private bool
is_font(char *name)
{
    int pos = strlen(name) - 4;
    if (name && pos > 0 )
	return strncasecmp("TTF", &name[pos], 3);
    else
	return false;
}

int
zip_end_job(met_parser_state_t *st, met_state_t *mets, zip_state_t *pzip)
{
    int i = 0; //pzip->firstPage;
    int code = 0;

    
    if (!pzip->inline_mode) {
	for (; i < 5000;  i++) {
	    if (pzip->parts[i]) {
		code = zip_page(st, mets, pzip, pzip->parts[i]);
		if (code) {
		    gs_rethrow(code, "delayed zip_page");
		    break;
		}
	    }
	}
    }

    for (i=0; i < 5000;  i++) {
	if (pzip->parts[i]) {
	    if ( is_font(pzip->parts[i]->name) ) {
		char buf[256];
		buf[0] = '/';
		buf[1] = 0;
		strncpy(&buf[1], pzip->parts[i]->name, strlen(pzip->parts[i]->name));
		buf[strlen(pzip->parts[i]->name) + 1] = 0;
		pl_dict_undef(&mets->font_dict, buf, strlen(buf));
	    }
	    part_free_all(pzip->parts[i]);

	    // need destructor here.
	    gs_free_object(pzip->memory, pzip->parts[i]->name, "part_free struct");
	    gs_free_object(pzip->memory, pzip->parts[i]->buf, "part_free struct");
	    gs_free_object(pzip->memory, pzip->parts[i]->zs, "part_free struct");
	    gs_free_object(pzip->memory, pzip->parts[i], "part_free struct");
	    pzip->parts[i] = 0;
	}
    }
    pzip->read_part = -1;

    return zip_initialize(pzip);
}

zip_state_t *
zip_init_instance(gs_memory_t *mem)
{
    zip_state_t *pzip;

    pzip = gs_alloc_bytes(mem, sizeof(zip_state_t), "zip_init_instance alloc zip_state_t");
    pzip->memory = mem;
    zip_initialize(pzip);
    pzip->free_blk_list_len = 0;
    pzip->free_list = NULL;


    return pzip;
}

#endif

