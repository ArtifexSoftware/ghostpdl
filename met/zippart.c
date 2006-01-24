/* Copyright (C) 2005, 2006 Artifex Software Inc.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/* $Id$ */

#include "gx.h"
#include "zipparse.h"
#include "assert.h"
#include "gserrors.h"
#include "gsmemory.h"


/* compile switch for debuging zip compression 
 * writes the exploded zip file to /tmp/XPS so you might want to create the directory
 */
static const int ziptestmode = 1;



/* NB: needed features:  unicode, esc characters,  directories aren't supported.*/
zip_part_t * 
find_zip_part_by_name(zip_state_t *pzip, char *name)
{
    int i;
    // NB: needs linked list parts interface
    
    for (i=0; i<5000 && i < pzip->num_files; i++) 
	if (pzip->parts[i]) 
	    if (name[0] == '/') {
		if (!strncmp(name+1, pzip->parts[i]->name, strlen(name) - 1))
		    return( pzip->parts[i] );
	    }
	    else
		if (!strncmp(name, pzip->parts[i]->name, strlen(name)))
		    return( pzip->parts[i] );
    
    return NULL;
}

/* move curr read block to next block updating read stream
 * 
 * do 
 *   read all avail limit is block size or end of data
 * while (!zip_read_blk_next_blk(rpart))
 */
int 
zip_read_blk_next_blk(zip_part_t *rpart)
{
    rpart->curr = rpart->curr->next;
    if (rpart->curr) {
	rpart->s.r.ptr = &rpart->curr->data[0] - 1;
	rpart->s.r.limit = &rpart->curr->data[rpart->curr->writeoffset-1];
    }
    else {
	rpart->curr = rpart->head;  /* let's not leave a bad pointer */
	return 	-1; 
    }
    return 0;
}


/* length of all the data in a zip file, or XPS part */
int 
zip_part_length(zip_part_t *rpart)
{
    int len = 0;
    zip_block_t *blk;

    blk = rpart->head;
    while (blk->next) {
	len += blk->writeoffset;
	if (blk->writeoffset != ZIP_BLOCK_SIZE) {
	    ;
	    dprintf2(NULL, 
		     "Wasted Space! zip_part_length != zip_part_read %ld, %d\n", 
		     blk->writeoffset, ZIP_BLOCK_SIZE);
	}
	    
	blk = blk->next;
    }
    len += blk->writeoffset;
    return len;
}

/* SEEK_SET, SEEK_CUR, SEEK_END are all supported.
 * readonly usage, so seek beyond EOF is ignored.
 */ 
int 
zip_part_seek(zip_part_t *rpart, long offset, int whence)
{
    zip_block_t *blk;

    if (whence == SEEK_END) {
	/* SEEK to end of file is legal, beyond isn't 
	 * so this will either move back from end of file or to EOF
	 */
	return zip_part_seek( rpart, zip_part_length(rpart) + offset, SEEK_SET );
    }
    else if (whence == SEEK_CUR) {  // sk foo untested.
	long extra = rpart->s.r.ptr - ( &blk->data[0] - 1); 
        blk = rpart->curr;
	offset += extra; 

	do { 
	    if ( offset >= ZIP_BLOCK_SIZE ) 
		offset -= ZIP_BLOCK_SIZE;
	    else 
		break;
	    blk = blk->next;
	} while (blk->next);
	rpart->curr = blk;
    }
    else if (whence == SEEK_SET) {  // sk foo tested.
        blk = rpart->head;
	do { 
	    if ( offset >= ZIP_BLOCK_SIZE ) 
		offset -= ZIP_BLOCK_SIZE;
	    else 
		break;
	    blk = blk->next;
	} while (blk->next) ;
	rpart->curr = blk;
    }
    else 
	return -1;

    rpart->s.r.limit = &rpart->curr->data[rpart->curr->writeoffset] - 1;
    if ( offset < rpart->curr->writeoffset )
	rpart->s.r.ptr = &rpart->curr->data[offset] - 1;
    else 
	rpart->s.r.ptr = &rpart->curr->data[rpart->curr->writeoffset] - 1;

    return 0;
}

/* copy rlen bytes from zip_part into dest 
 * return number of bytes copied.  
 * 
 * spans linked list of part data blocks.
 * only returns less for rlen on end of list data 
 * entire part is availiable, all pieces are concatinated 
 * prior to first read.
 */
int 
zip_part_read( byte *dest, int rlen, zip_part_t *rpart )
{
    int error = 0;
    int have = rpart->s.r.limit - rpart->s.r.ptr;
    int copied = 0;

    while (rlen > 0) {
	if ( have > 0 ) {
	    have = min(have, rlen);
	    memcpy( dest, rpart->s.r.ptr + 1, have );
	    rlen -= have;
	    copied += have;
	    dest += have;
	}
	if (rlen > 0) {
	    if (zip_read_blk_next_blk(rpart)) {
		break; // early end of data, client must check return count
	    }
	    have = rpart->s.r.limit - rpart->s.r.ptr;
	}
	else 
	    break;
    }
    return copied;
}


/* This is test code to test the read stream interface */

private int 
zip_page_test( zip_state_t *pzip, zip_part_t *rpart ) 
{
    static int called_counter = 0;
    int error;

    called_counter++;

    if (0) {  /* test small reads of the whole part */
	byte buf[100];
	if ((error = zip_part_seek(rpart, 0, 0)) != 0)
	    return mt_rethrow(error, "seek set 0 error");
	uint len = zip_part_length(rpart);
	while (len) {
	    len -= zip_part_read(&buf[0], 100, rpart);
	}
	zip_part_free_all(rpart);
    }

    if (1) {
	char buf[4];
	if ((error = zip_part_seek(rpart, 0, 0)) != 0)
	    return mt_rethrow(error, "seek set 0 error");
	int size = zip_part_read(&buf, 4, rpart);
	if ( size != 4 )
	    dprintf(rpart->parent->memory, "bad len\n");
    }
    
    if (1) {
	char fname[256];
	FILE *fp = 0;
	char *ptr = rpart->name;
	int i = 0;
	int index = -1;
	

	mkdir("/tmp/XPS", 0777);		

	strcpy(fname, "/tmp/XPS/");
	for (i = strlen(fname); *ptr != 0; i++, ptr++) 
	    if (*ptr == '/')
	         fname[i] = '-';
	    else 
		fname[i] = *ptr;
	fname[i]=0;

	dprintf1(rpart->parent->memory, "Zip File Name %s\n", fname);

	if ((fp = fopen(fname, "w")) == NULL)
	    return mt_throw1(-1, "couldn't open %s", fname);
	
	if ((error = zip_part_seek(rpart, 0, 0)) != 0)
	    return mt_rethrow(error, "seek failed");
	do {
	    do {
	        uint avail = rpart->s.r.limit - rpart->s.r.ptr;
		fwrite(rpart->s.r.ptr + 1, 1, avail, fp);
		rpart->s.r.ptr += avail;
	    }
	    while (rpart->s.r.ptr + 1 < rpart->s.r.limit); 
	}
	while ( 0 == zip_read_blk_next_blk(rpart) );
	fclose(fp);
    }

}

int 
zip_page( met_parser_state_t *st, met_state_t *mets, zip_state_t *pzip, zip_part_t *rpart ) 
{
    static int page = 0;
    static int start_page = 0;  /* hack to skip a few pages */

    long len = 0;
    int error = 0;
    char *p = &rpart->name[strlen(rpart->name) - 4];

    if ( gs_debug_c('i') ) 
	dprintf1(NULL, "End of part %s\n", rpart->name );
    
    /* NB: its not cool to string compare to find FixedPages to parse thats 
     * what rels are for...
     */

    if ( !strncmp(rpart->name, "FixedPage_", 10) && 
	 strncmp(p, "rels", 4 )) { /* feed met_process a Page */

	++page;

	if ( page < start_page )
	    return 0;

	if ( ziptestmode ) {
	    zip_page_test(pzip, rpart);
	}

	if (0 < zip_part_length(rpart)) {
	    error = zip_part_seek(rpart, 0, 0);
	    if (error) return error;
	    do {
		do {
		    error = met_process(st, mets, pzip, &rpart->s.r);
		    if (error) 
			return mt_rethrow(error, "met_process");
		}
		while (rpart->s.r.ptr + 1 < rpart->s.r.limit); 
	    }
	    while ( 0 == zip_read_blk_next_blk(rpart) );
	}
	else
	    dprintf1(pzip->memory, "Zero length part %s\n", rpart->name );
	/* auto deletion of FixedPage after parsing */
	//if ( pzip->inline_mode == false )
	//    zip_part_free_all( rpart );
	// can't delete 
    }
    else if ( 1 && ziptestmode ) {
	//zip_page_test(pzip, rpart);
    }

    return error;
}


int 
zip_part_free_all( zip_part_t *part ) 
{
    zip_block_t *next = part->head;
    zip_block_t *this = part->head;
    zip_state_t *pzip = part->parent;

    while (next) {
	this = next;
	next = next->next;

	if (pzip->free_blk_list_len < PZIP_FREE_BLK_MAX_LEN) {
	    this->next = pzip->free_list;
	    pzip->free_list = this;
	    pzip->free_blk_list_len++;
	    }
	else
	    gs_free_object(pzip->memory, this, "zip_part_free block");
    } 
    part->head = 0;
    part->curr = 0;
    part->tail = 0;
}
