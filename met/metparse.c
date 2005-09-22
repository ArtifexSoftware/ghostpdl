/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$*/

/* metro parser module */

#include <expat.h>
#include "metparse.h"
#include "zipstate.h"
#include "assert.h"
#include "gserrors.h"
#include "gsmemory.h"

/* have expat use the gs memory manager. */

/* NB global - needed for expat callbacks */
gs_memory_t *gs_mem_ptr = NULL;

/* memory callbacks */
private void *
met_expat_alloc(size_t size)
{
    return gs_alloc_bytes(gs_mem_ptr, size, "met_expat_alloc");
}

private void
met_expat_free(void *ptr)
{
    return gs_free_object(gs_mem_ptr, ptr, "met_expat_free");
}

private void *
met_expat_realloc(void *ptr, size_t size)
{
    return gs_resize_object(gs_mem_ptr, ptr, size, "met_expat_free");
}

/* start and end callbacks NB DOC */
private void
met_start(void *data, const char *el, const char **attr)
{
    int i;
    met_parser_state_t *st = data;
    gs_memory_t *mem = st->memory;

    for (i = 0; i < st->depth; i++)
        dprintf(mem, "  ");

    dprintf1(mem, "%s", el);

    for (i = 0; attr[i]; i += 2) {
        dprintf2(mem, " %s='%s'", attr[i], attr[i + 1]);
    }

    dprintf(mem, "\n");
    st->depth++;

}

private void
met_end(void *data, const char *el)
{
    met_parser_state_t *st = data;
    st->depth--;
}

/* allocate the parser, and set the global memory pointer see above */
met_parser_state_t *
met_process_alloc(gs_memory_t *memory)
{
    /* memory procedures */
    const XML_Memory_Handling_Suite memprocs = {
        met_expat_alloc, met_expat_realloc, met_expat_free
    };
        
    /* NB should have a structure descriptor */
    met_parser_state_t *stp = gs_alloc_bytes(memory,
                                             sizeof(met_parser_state_t),
                                             "met_process_alloc");
    XML_Parser p; 
    /* NB set the static mem ptr used by expat callbacks */
    gs_mem_ptr = memory;

    if (!stp)
        return NULL;

    p = XML_ParserCreate_MM(NULL /* encoding */,
                            &memprocs,
                            NULL /* name space separator */);
    if (!p)
        return NULL;

    stp->memory = memory;
    stp->parser = p;
    stp->depth = 0;
    stp->next_read_dir = -1;
    /* set up the start end callbacks */
    XML_SetElementHandler(p, met_start, met_end);
    XML_SetUserData(p, stp);
    return stp;
}

/* free the parser and corresponding expat parser */
void
met_process_release(met_parser_state_t *st)
{
    gs_free_object(st->memory, st, "met_process_release");
    XML_ParserFree(st->parser);
}


void zip_set_readdir(void *vpzip, int num, char *str)
{
    zip_state_t *pzip = (zip_state_t *) vpzip;

    if (num < pzip->num_files && num >= 0)
	pzip->read_dir = num;
}

int zip_get_numfiles(void *vpzip)
{
   zip_state_t *pzip = (zip_state_t *) vpzip;
    
   return pzip->num_files;
}
void
zip_get_filename(void *vpzip, int i, char **name)
{
    zip_state_t *pzip = (zip_state_t *) vpzip;
    
    if (i >= pzip->num_files || i < 0)
	i = pzip->read_dir;
    *name = pzip->dir[i]->name;
}


static byte g_bigbag[80000000];
static long g_offset = 0;

int
save_bigbag_process(met_parser_state_t *st, met_state_t *mets, void *pzip, 
		    stream_cursor_read *pr)
{
    if (1) {

	memcpy(&g_bigbag[g_offset], pr->ptr, pr->limit - pr->ptr);
	g_offset += pr->limit - pr->ptr;
	pr->ptr += pr->limit - pr->ptr;
    }
}

int
met_process(met_parser_state_t *st, met_state_t *mets, void *pzip, 
	    stream_cursor_read *pr)
{
    if (0) {

	memcpy(&g_bigbag[g_offset], pr->ptr, pr->limit - pr->ptr);
	g_offset += pr->limit - pr->ptr;
	pr->ptr += pr->limit - pr->ptr;

	// 
    } else {
	const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	uint avail = rlimit - p;
	XML_Parser parser = st->parser;
	
	if (XML_Parse(parser, p + 1, avail, 0 /* done? */) == XML_STATUS_ERROR) {
	    dprintf2(st->memory, "Parse error at line %d:\n%s\n",
		     XML_GetCurrentLineNumber(parser),
		     XML_ErrorString(XML_GetErrorCode(parser)));
	    return -1;
	}
	/* stefan foo: how is this getting updated ??? */
	pr->ptr += avail;
    }
    return 0;
}

/* expat need explicit shutdown */
int
met_process_shutdown(met_parser_state_t *st)
{
    XML_Parser parser = st->parser;
    if (XML_Parse(parser, 0 /* data buffer */, 
                  0 /* buffer length */,
                  1 /* done? */ ) == XML_STATUS_ERROR) {
        dprintf2(st->memory, "Parse error at line %d:\n%s\n",
                 XML_GetCurrentLineNumber(parser),
                 XML_ErrorString(XML_GetErrorCode(parser)));
        return -1;
    }
    return 0;
}

inline int stream_has(stream_cursor_read *pr, unsigned int cnt)
{
    return( pr->limit - pr->ptr >= cnt );
}


inline byte read_byte(stream_cursor_read *pr)
{
    pr->ptr++;
    return *pr->ptr;
}

unsigned int read2(stream_cursor_read *pr, unsigned short *a)
{
    if (stream_has(pr, 2)) {
	*a =  read_byte(pr) | (read_byte(pr) << 8) ;
	// dprintf2(gs_mem_ptr, "read2 %0hd:%0hx\n", *a, *a);
    }
    else
	return eNeedData;
    return 0;
}

unsigned long read4(stream_cursor_read *pr, unsigned long *a)
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


int zip_init_dir(zip_state_t *pzip)
{
    zip_dir_t *dir = (zip_dir_t *)gs_alloc_bytes(pzip->memory, size_of(zip_dir_t), 
						 "zip_init_dir");
    pzip->num_files++;
    pzip->dir[pzip->num_files - 1] = dir;
    pzip->dir_read_state++;
    dir->tail = dir->head = dir->csaved = dir->metasize = dir->namesize = 0;
    zip_new_block(pzip, dir);
    dir->head = dir->tail;
    dir->keep_it = false;  /* read once by default */
    dir->buf = NULL;
    dir->newfile = true;
    
    return 0;
}

int zip_read_dir(zip_state_t *pzip, stream_cursor_read *pr)
{
    unsigned long id;
    short shortInt;
    long longInt;
    int code = 0;
    zip_dir_t *dir =0;
    int i;

    switch( pzip->dir_read_state ) {
    case 0:
	if ((code = read4(pr, &id)) != 0)
	    return code;
        if (id == 0x04034b50) {
	    if ((code = zip_init_dir(pzip)) != 0 )
		return code;
	    dir = pzip->dir[pzip->num_files - 1];
	} else { // if (id == 0x02014b50) 
	    // end of archive or foobar
	    dprintf(pzip->memory, "foobar");
	    pzip->dir_read_state = 20;
	    return 0;
	}
	
    case 1: /* version made by */
	//if ((code = read2(pr, &shortInt)) != 0)
	//    return 0;
	//dprintf2(pzip->memory, "dir id: %hx%hd\n\n", shortInt, shortInt);
	pzip->dir_read_state++;

    case 2: /* version to extract*/
	if ((code = read2(pr, &shortInt)) != 0)
	    return code;
	pzip->dir_read_state++;

    case 3: /* general*/
	dir = pzip->dir[pzip->num_files - 1];
	if ((code = read2(pr, &dir->gp_bitflag)) != 0)
	    return code;
	pzip->dir_read_state++;
	 
    case 4: /* method */
	dir = pzip->dir[pzip->num_files - 1];
	if ((code = read2(pr, &dir->comp_method)) != 0)
	    return code;
	pzip->dir_read_state++;
   
    case 5: /* last mod file time */
	if ((code = read2(pr, &shortInt)) != 0)
	    return code;
	pzip->dir_read_state++;
	    
    case 6: /* last mod file date */
	if ((code = read2(pr, &shortInt)) != 0)
	    return code;
	pzip->dir_read_state++;

    case 7: /* crc-32 */
	if ((code = read4(pr, &shortInt)) != 0)
	    return code;
	pzip->dir_read_state++;

    case 8: /* csize */
	dir = pzip->dir[pzip->num_files - 1];
	if ((code = read4(pr, &dir->csize)) != 0)
	    return code;
	pzip->dir_read_state++;	
	
    case 9: /* usize */
	dir = pzip->dir[pzip->num_files - 1];
	if ((code = read4(pr, &dir->usize)) != 0)
	    return code;
	pzip->dir_read_state++;

    case 10: /* namesize */
	dir = pzip->dir[pzip->num_files - 1];
	if ((code = read2(pr, &dir->namesize)) != 0)
	    return code;
	pzip->dir_read_state++;

    case 11: /* metasize */
	dir = pzip->dir[pzip->num_files - 1];
	if ((code = read2(pr, &dir->metasize)) != 0)
	    return code;
	pzip->dir_read_state++; 

    case 12:
	dir = pzip->dir[pzip->num_files - 1];
	if (stream_has(pr, dir->namesize)) {
	    dir->name = gs_alloc_bytes(pzip->memory, dir->namesize + 1, "pzip dir name");
	    if (dir->name == NULL) 
		return -1;  // memory error
	    memcpy(dir->name, pr->ptr +1, dir->namesize);
	    dir->name[dir->namesize] = 0;
	    dprintf1(pzip->memory, "zip file name:%s\n", dir->name );
	    pr->ptr += dir->namesize;
	    pzip->dir_read_state++;
	} 
	else 
	    return eNeedData;
    case 13:
	dir = pzip->dir[pzip->num_files - 1];
	if (stream_has(pr, dir->metasize)) {
	    pr->ptr += dir->metasize;
	    pzip->dir_read_state++;
	}
	else if (dir->metasize){
	    dir->metasize -= pr->limit - pr->ptr;
	    pr->ptr = pr->limit;
	    return eNeedData;
	}
    case 14:    
	pzip->read_state = 1;  /* read file after this header */
	pzip->dir_read_state = 0; /* next call looks for header */
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
		pzip->dir_read_state++;
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
	    read2(pr, &dir->skip_len);
	    pzip->dir_read_state++;
	} else {
	    pr->ptr == pr->limit - 1;
	    pzip->dir_read_state = 0;  /* reset for next zip file */
	    return -102; // e_ExitLanguage;
	}
    case 22:
	if (stream_has(pr, dir->skip_len) == 0 )
	    pr->ptr += dir->skip_len;


	pzip->dir_read_state = 0;  /* reset for next zip file */
	return -102; // e_ExitLanguage;
    
    default:
	/* coding error */
	return -1;
    }
    
    return code;
}

int zip_new_block(zip_state_t *pzip, zip_dir_t *dir)
{
    int code = 0;
    zip_block_t *blk;

    if (pzip->free_list) {
	blk = pzip->free_list;
	pzip->free_list = blk->next;
	blk->next = NULL;
	blk->readoffset = blk->writeoffset = 0;
    }
    else {
	blk = (zip_block_t *) gs_alloc_bytes(pzip->memory, 
					     sizeof(zip_block_t), 
					     "zip_new_block");	
	if (blk == NULL)
	    return gs_error_VMerror;
	blk->readoffset = blk->writeoffset = 0;
	blk->next = NULL;
    }
    assert(dir->write);
    assert(dir->write->writeoffset == ZIP_BLOCK_SIZE);

    if (dir->tail)
	dir->tail->next = blk;
    dir->tail = blk;

    return code;
}

int zip_save_data(zip_state_t *pzip, stream_cursor_read *pr)
{
    int code = 0;
    zip_dir_t *dir = pzip->dir[pzip->num_files - 1];
    long wlen = ZIP_BLOCK_SIZE - dir->tail->writeoffset;
    long slen = pr->limit - pr->ptr;
    long cleft = max(0, dir->csize - dir->csaved);

    if (slen == 0)
	return code;

    if (wlen == 0) {
	if ((code = zip_new_block(pzip, dir)) != 0) 
	    return code;
	wlen = ZIP_BLOCK_SIZE - dir->tail->writeoffset;
    }
    if (cleft == 0 && dir->comp_method == 8)
	cleft = slen;
    //  zero for length
    if (cleft) {
	byte *wptr = &dir->tail->data[dir->tail->writeoffset];
	wlen = min(wlen, slen);
	wlen = min(wlen, cleft);
	memcpy(wptr, pr->ptr + 1, wlen);
	pr->ptr += wlen;
	dir->tail->writeoffset += wlen;
	dir->csaved += wlen;
    }
    return code;
}

#define ZIP_DECOMPRESS_BUFFER_SIZE  4096

int zip_init_write_stream(zip_state_t *pzip, zip_dir_t *dir)
{
    int code = 0;
    
    if (0 /* && setup external stream */ ) {

	// jpeg stream or font stream might save to disk or font dictionary etc 
    }
    else if (dir->buf == NULL) {
	dir->buf = gs_alloc_bytes(pzip->memory, ZIP_DECOMPRESS_BUFFER_SIZE, "zip decompress buffer");
	dir->s.r.ptr = dir->buf - 1;
	dir->s.r.limit = dir->buf -1;
	dir->s.w.limit = dir->buf + ZIP_DECOMPRESS_BUFFER_SIZE -1;
	dir->zs = gs_alloc_bytes(pzip->memory, size_of(z_stream), "zip z_stream");
	if (dir->zs == NULL)
	    return -1;
	dir->zs->zalloc = 0;
	dir->zs->zfree = 0;
	dir->zs->opaque = 0;
    }
    return code;
}

/* NB: assumes we only read a compressed file once
 */
int zip_free_readblk(zip_state_t *pzip, zip_dir_t *dir)
{
    
    if (!dir->keep_it && dir->head->readoffset == ZIP_BLOCK_SIZE - 1) {
	zip_block_t *tmp = dir->head;
	dir->head = tmp->next;
	tmp->next = pzip->free_list;
	pzip->free_list = tmp;
    }
}

int zip_decompress_data(zip_state_t *pzip, stream_cursor_read *pin )
{
    int code = 0;
    zip_dir_t *dir = pzip->dir[pzip->read_dir];
    z_stream *zs = 0;
    long rlen = pin->limit - pin->ptr - 1;
    /* alias into write stream */
    stream_cursor_read *pr = NULL;
    stream_cursor_write *pw = NULL;

    int rstart = pin->ptr;
    long len;
    int status;

    if ((code = zip_init_write_stream(pzip, dir)) != 0)
	return code;


    pr = &dir->s.r;
    pw = &dir->s.w; 
    if (pw->ptr == pw->limit)
	return eWriteStall;  // write stall on output

    if (dir->comp_method == 0) {
	rlen = min(rlen, pw->limit - pw->ptr);
	memcpy(pr->ptr + 1, pin->ptr, rlen);
	pw->ptr += rlen;
	pin->ptr += rlen;
    }
    else {  /* 8 == flate */
	zs = dir->zs;
	zs->next_in = pin->ptr + 1;
	zs->avail_in = pin->limit - pin->ptr - 1;
	zs->next_out = pw->ptr + 1;
	zs->avail_out = pw->limit - pw->ptr;

	if (dir->newfile) 	/* init for new file */
	    inflateInit2(dir->zs, -8);

	status = inflate(zs, Z_NO_FLUSH);
	pin->ptr = zs->next_in - 1;
	pw->ptr = zs->next_out - 1;
    
	switch (status) {
        case Z_OK:
	    code = (pw->ptr == pw->limit ? 1 : pin->ptr > rstart ? 0 : 1);
	    dir->newfile = false;
	    break;

        case Z_STREAM_END:
	    dir->newfile = true;
	    inflateEnd(zs);
	    code = eEndOfStream; // foo was: EOFC;
	    break;

        default:
	    code = ERRC;
	}
    }
    zip_free_readblk(pzip, dir);
    return code;
}

int met_set_initial_read_dir(met_parser_state_t *st, met_state_t *mets, 
			     zip_state_t *pzip)
{
    int code = 0;
    // hard coded foo
    st->next_read_dir = 0;
    return code;
}


bool zip_is_image( zip_dir_t *dir )
{
    char *str = dir->name + strlen(dir->name) - 6; 
    if (strncmp(str, "1.xaml", 6) == 0)
	return true; // return false;
    return true;
}

int zip_save_image( zip_state_t *pzip, zip_dir_t *dir )
{
    long rlen = dir->s.r.limit - dir->s.r.ptr;
    long wlen = 0;
    stream_cursor_read *pr = &dir->s.r;
    int code = 0;

    while ( rlen ) {
	wlen = ZIP_BLOCK_SIZE - dir->tail->writeoffset;
	if (wlen == 0) {
	    if ((code = zip_new_block(pzip, dir)) != 0) 
		return code;
	    wlen = ZIP_BLOCK_SIZE - dir->tail->writeoffset;
	}
	wlen = min( rlen, wlen );
	
	memcpy(&dir->tail->data[dir->tail->writeoffset], pr->ptr + 1, wlen);
	pr->ptr += wlen;
	dir->tail->writeoffset += wlen;
	rlen -= wlen;
    }
}

int zip_decomp_process(met_parser_state_t *st, met_state_t *mets, zip_state_t *pzip,stream_cursor_read *pr)
{
    int code = 0;
    zip_dir_t *rdir = NULL;
    

    /* update reading pointer 
     * NB: client should be able to choose a different file to read
     * currently just serially sends file after file.
     */
    //    if (pzip->read_dir < 0) {
	if (st->next_read_dir < 0)
	    return met_set_initial_read_dir(st, mets, pzip);
	else if (st->next_read_dir < pzip->num_files)
	    pzip->read_dir = st->next_read_dir;
	else 
	    return code; // Nothing to do till we have read the start of file.
	//    }
    rdir = pzip->dir[pzip->read_dir];

    /* decompress and send data to parser */
    code = zip_decompress_data(pzip, pr);
    if (code == eEndOfStream) { // good end of input stream 
	st->next_read_dir++;
	pzip->read_state = 2; 
	code = 0;
    }
    if (code == eWriteStall) { // write stall on output
	long len = rdir->s.r.limit - rdir->s.r.ptr;
	if ( len && rdir->buf -1 != rdir->s.r.ptr)
	    memmove(rdir->buf, rdir->s.r.ptr, len);
	rdir->s.r.ptr = rdir->buf -1;
	rdir->s.r.limit = rdir->buf + ( len - 1 );
	
	code = 0;  // let it be read
    }


    // save image
    if (zip_is_image( rdir ))
	zip_save_image( pzip, rdir );
    /*
    else if ( zip_is_font( rdir ) )
	zip_save_font(rdir);
    else if ( zip_is_rel( rdir ) )
	zip_save_rel( rdir );
    */
    else {
	// process pages
	// process documents

	/* NB: this hack removes non-xml file prefix */
	if ( strncmp(rdir->s.r.ptr + 1, "<", 4) == 0)
	    rdir->s.r.ptr += 3;
    
	if ((code = met_process(st, mets, pzip, &rdir->s.r)) != 0)
	    return code;
    }
    if (1) { // stream adjust
	long len = rdir->s.r.limit - rdir->s.r.ptr;
	if ( len )
	    memmove(rdir->buf, rdir->s.r.ptr, len);
	rdir->s.r.ptr = rdir->buf -1;
	rdir->s.r.limit = rdir->buf + ( len - 1 );
    }
    if (code == eNeedData)  // need data
	return 0;
    return code;
}


/* 
 * code == eNeedData is returned as 0 here.
 */
int
zip_process(met_parser_state_t *st, met_state_t *mets, zip_state_t *pzip, stream_cursor_read *pr)
{
    int code = 0;
    zip_dir_t *rdir = NULL;
    zip_dir_t *wdir = NULL;

    while (pr->limit - pr->ptr > 4 && code == 0) {

	switch (pzip->read_state) {
	case 0: /* local file header and skip of end of zip file records */
	    if ((code = zip_read_dir(pzip, pr)) != 0)
		return code;
	    /* 0 : more header to read 
	     * 1 : file to read
	     * 3 : end of file 
	     */
	    break;
	case 1: /* file data */	
	    // don't save compressed data
	    //if ((code = zip_save_data(pzip, pr)) != 0)
	    //return code;

	case 2: /* optional Data descriptor */
	    wdir = pzip->dir[pzip->num_files - 1];
	    if (wdir->csaved && wdir->csaved == wdir->csize ) {
		/* have count and fall through */

		if (wdir->gp_bitflag & 1 << 3) { 
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
	    else if (pzip->read_state == 2  && wdir->gp_bitflag & 1 << 3) {
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


zip_state_t *zip_init_instance(gs_memory_t *mem)
{
    zip_state_t *zip;

    zip = gs_alloc_bytes(mem, sizeof(zip_state_t), "zip_init alloc zip state");

    zip->memory = mem;
    zip->zip_mode = true;
    zip->read_state = 0;
    zip->dir_read_state = 0;
    zip->read_dir = -1;
    zip->num_files = 0;
    zip->free_list = NULL;

    return zip;
}
