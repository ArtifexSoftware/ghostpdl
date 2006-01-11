#include "assert.h"
#include "gserrors.h"
#include "memory_.h"
#include "zipparse.h"

#include "pltop.h"  /* NB hack to enable disable zip/xml parsing */

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
zip_new_block(zip_state_t *pzip, zip_part_t *part)
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
    zip_part_t *part = (zip_part_t *)gs_alloc_bytes(pzip->memory, size_of(zip_part_t), 
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

private int 
zip_read_part(zip_state_t *pzip, stream_cursor_read *pr)
{
    unsigned long id;
    unsigned short shortInt;
    unsigned long longInt;
    int code = 0;
    zip_part_t *part =0;
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
		return -1;  // memory error
	    memcpy(part->name, pr->ptr +1, part->namesize);
	    part->name[part->namesize] = 0;
	    // Perhaps -ZI spew
	    // dprintf1(pzip->memory, "zip file name:%s\n", part->name );
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
	/* coding error */
	return -1;
    }
    
    return code;
}


private int 
zip_init_write_stream(zip_state_t *pzip, zip_part_t *part)
{
    if (part->zs == NULL) {
	part->zs = gs_alloc_bytes(pzip->memory, size_of(z_stream), "zip z_stream");
	if (part->zs == NULL)
	    return -1;
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
    zip_part_t *part = pzip->parts[pzip->read_part];
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
	    return code;
	wptr = &part->tail->data[part->tail->writeoffset];
	wlen = ZIP_BLOCK_SIZE;
    }
    else 
	wlen = ((ZIP_BLOCK_SIZE - 1) - part->tail->writeoffset) + 1;

    zip_init_write_stream(pzip, part);
	
    if (part->comp_method == 0) {
	int left = part->csize - part->csaved;
	if (left == 0)\
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
		// NB: need to test the end of part/end of file/no csize case. 
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
    zip_part_t *rpart = NULL;

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
    zip_part_t *rpart = NULL;
    zip_part_t *wpart = NULL;
    int last_len = pr->limit - pr->ptr;

    while (pr->limit - pr->ptr > 4 && code == 0) {

	switch (pzip->read_state) {
	case 0: /* local file header and skip of end of zip file records */
	    if ((code = zip_read_part(pzip, pr)) != 0)
		return code;
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
zip_end_job(met_state_t *mets, zip_state_t *pzip)
{
    int i;

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
	    zip_part_free_all(pzip->parts[i]);

	    // need destructor here.
	    gs_free_object(pzip->memory, pzip->parts[i]->name, "zip_part_free struct");
	    gs_free_object(pzip->memory, pzip->parts[i]->buf, "zip_part_free struct");
	    gs_free_object(pzip->memory, pzip->parts[i]->zs, "zip_part_free struct");
	    gs_free_object(pzip->memory, pzip->parts[i], "zip_part_free struct");
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
