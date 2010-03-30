/* Copyright (C) 2001-2008 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

  This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Generate source data for the %rom% IODevice */
/*
 * For reasons of convenience and footprint reduction, the various postscript
 * source files, resources and fonts required by Ghostscript can be compiled
 * into the executable.
 *
 * This file takes a set of directories, and creates a compressed filesystem
 * image that can be compiled into the executable as static data and accessed
 * through the %rom% iodevice prefix
 *
 */
/*
 *	Usage: mkromfs [-o outputfile] [options ...] path path ...
 *
 *	    mkromfs no longer performs implicit enumeration of paths, so if a
 *	    path is a directory to be enumerated trailing '/*' must be present.
 *
 *	    options and paths can be interspersed and are processed in order
 *      
 *	    options:
 *		-o outputfile	default: obj/gsromfs.c if this option present, must be first.
 *		-P prefix	use prefix to find path. prefix not included in %rom%
 *		-X path		exclude the path/file from further processing.
 *			  Note:	The tail of any path encountered will be tested so .svn on the
 *				-X list will exclude that path in all subsequent paths enumerated.
 *
 *              -d romprefix    directory in %rom% file system (a prefix string on filename)
 *		-c		compression on
 *		-b		compression off (binary).
 *		-g initfile gconfig_h 
 *				special handling to read the 'gs_init.ps' file (from
 *				the current -P prefix path), and read the gconfig.h for
 *				psfile_ entries and combines them into a whitespace
 *				optimized and no comments form and writes this 'gs_init.ps'
 *				to the current -d destination path. This is a space and
 *				startup performance improvement, so also this should be
 *				BEFORE other stuff in the %rom% list of files (currently
 *				we do a sequential search in the %rom% directory).
 *
 *				For performance reasons, it is best to turn off compression
 *				for the init file. Less frequently accessed files, if they
 *				are large should still be compressed.
 *
 */ 

#include "stdpre.h"
#include "stdint_.h"
#include "time_.h"
#include "gsiorom.h"
#include "gsmemret.h" /* for gs_memory_type_ptr_t */
#include "gsmalloc.h"
#include "gsstype.h"
#include "gp.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>

#include <zlib.h>

/*
 * The rom file system is an array of pointers to nodes, terminated by a NULL
 */
/*
 * in memory structure of each node is:
 *
 *	length_of_uncompressed_file		[31-bit big-endian]
 *						high bit is compression flag 
 *	data_block_struct[]
 *	padded_file_name (char *)	includes as least one terminating <nul>
 *	padded_data_blocks
 */
/*
 *	data_block_struct:
 *	    data_length			(not including pad)	[32-bit big-endian]
 *	    data_block_offset		(start of each block)	[32-bit big-endian]
 */

typedef struct romfs_inode_s {
    unsigned long length;		/* blocks is (length+ROMFS_BLOCKSIZE-1)/ROMFS_BLOCKSIZE */
    char *name;				/* nul terminated */
    unsigned long *data_lengths;	/* this could be short if ROMFS_BLOCKSIZE */
					/* is < 64k, but the cost is small to use int */
    unsigned char **data;
} romfs_inode;

typedef struct Xlist_element_s {
	void *next;
	char *path;
    } Xlist_element;

byte *minimal_alloc_bytes(gs_memory_t * mem, uint size, client_name_t cname);
byte *minimal_alloc_byte_array(gs_memory_t * mem, uint num_elements,
			     uint elt_size, client_name_t cname);
void *minimal_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
	       client_name_t cname);
void minimal_free_object(gs_memory_t * mem, void * data, client_name_t cname);
void minimal_free_string(gs_memory_t * mem, byte * data, uint nbytes, client_name_t cname);

/*******************************************************************************
 * The following is a REALLY minimal gs_memory_t for use by the gp_ functions
 *******************************************************************************/
byte *
minimal_alloc_bytes(gs_memory_t * mem, uint size, client_name_t cname)
{
    return malloc(size);
}

byte *
minimal_alloc_byte_array(gs_memory_t * mem, uint num_elements,
			     uint elt_size, client_name_t cname)
{
    return malloc(num_elements * elt_size);
}

void *
minimal_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
	       client_name_t cname)
{
    return malloc(pstype->ssize);
}

void
minimal_free_object(gs_memory_t * mem, void * data, client_name_t cname)
{
    free(data);
    return;
}

void
minimal_free_string(gs_memory_t * mem, byte * data, uint nbytes, client_name_t cname)
{
    free(data);
    return;
}

void basic_enum_ptrs(void);
void basic_reloc_ptrs(void);

void basic_enum_ptrs() {
    printf("basic_enum_ptrs is only called by a GC. Abort.\n");
    exit(1);
}

void basic_reloc_ptrs() {
    printf("basic_reloc_ptrs is only called by a GC. Abort.\n");
    exit(1);
}


const gs_malloc_memory_t minimal_memory = {
    (gs_memory_t *)&minimal_memory, /* stable */
    { minimal_alloc_bytes, /* alloc_bytes_immovable */
      NULL, /* resize_object */
      minimal_free_object, /* free_object */
      NULL, /* stable */
      NULL, /* status */
      NULL, /* free_all */
      NULL, /* consolidate_free */
      minimal_alloc_bytes, /* alloc_bytes */ 
      minimal_alloc_struct, /* alloc_struct */
      minimal_alloc_struct, /* alloc_struct_immovable */
      minimal_alloc_byte_array, /* alloc_byte_array */
      minimal_alloc_byte_array, /* alloc_byte_array_immovable */
      NULL, /* alloc_struct_array */
      NULL, /* alloc_struct_array_immovable */
      NULL, /* object_size */
      NULL, /* object_type */
      minimal_alloc_bytes, /* alloc_string */
      minimal_alloc_bytes, /* alloc_string_immovable */
      NULL, /* resize_string */
      minimal_free_string, /* free_string */
      NULL, /* register_root */ 
      NULL, /* unregister_root */
      NULL /* enable_free */ 
    },    
    NULL, /* gs_lib_ctx */
    NULL, /* head */
    NULL, /* non_gc_memory */
    0, /* allocated */
    0, /* limit */ 
    0, /* used */ 
    0  /* max used */
};

void put_uint32(FILE *out, const unsigned int q);
void put_bytes_padded(FILE *out, unsigned char *p, unsigned int len);
void inode_clear(romfs_inode* node);
void inode_write(FILE *out, romfs_inode *node, int compression, int inode_count, int*totlen);
void process_path(char *path, const char *os_prefix, const char *rom_prefix,
		  Xlist_element *Xlist_head,
		  int compression, int *inode_count, int *totlen, FILE *out);
int process_initfile(char *initfile, char *gconfig_h, const char *os_prefix,
		     const char *rom_prefix,
		     int compression, int *inode_count, int *totlen, FILE *out);
FILE *prefix_open(const char *os_prefix, const char *inname);
void prefix_add(const char *prefix, const char *filename, char *prefixed_path);

/* put 4 byte integer, big endian */
void put_uint32(FILE *out, const unsigned int q)
{
#if ARCH_IS_BIG_ENDIAN
    fprintf (out, "0x%08x,", q);
#else
    fprintf (out, "0x%02x%02x%02x%02x,", q & 0xff, (q>>8) & 0xff, (q>>16) & 0xff, (q>>24) & 0xff);
#endif
}

/* write string as 4 character chunks, padded to 4 byte words. */
void put_bytes_padded(FILE *out, unsigned char *p, unsigned int len)
{
    int i, j=0;
    union {
	uint32_t w;
	struct {
	    unsigned char c1;
	    unsigned char c2;
	    unsigned char c3;
	    unsigned char c4;
	} c;
    } w2c;

    for (i=0; i<(len/4); i++) {
	j = i*4;
	w2c.c.c1 = p[j++];
	w2c.c.c2 = p[j++];
	w2c.c.c3 = p[j++];
	w2c.c.c4 = p[j++];
	fprintf(out, "0x%08x,", w2c.w);
	if ((i & 7) == 7)
	    fprintf(out, "\n\t");
    }
    w2c.w = 0;
    switch (len - j) {
      case 3:
        w2c.c.c3 = p[j+2];
      case 2:
        w2c.c.c2 = p[j+1];
      case 1:
        w2c.c.c1 = p[j];
	fprintf(out, "0x%08x,", w2c.w);
      default: ;
    }
    fprintf(out, "\n\t");
}

/* clear the internal memory of an inode */
void inode_clear(romfs_inode* node)
{
    int i, blocks = (node->length+ROMFS_BLOCKSIZE-1)/ROMFS_BLOCKSIZE;
    
    if (node) {
        if (node->data) {
            for (i = 0; i < blocks; i++) {
                if (node->data[i]) free(node->data[i]);
            }
            free(node->data);
        }
        if (node->data_lengths) free(node->data_lengths);
    }
}

/* write out and inode and its file data */
void
inode_write(FILE *out, romfs_inode *node, int compression, int inode_count, int *totlen)
{
    int i, offset;
    int blocks = (node->length+ROMFS_BLOCKSIZE-1)/ROMFS_BLOCKSIZE;
    int namelen = strlen(node->name) + 1;	/* include terminating <nul> */
    int clen = 0;			/* compressed length */
    
    /* write the node header */
    fprintf(out,"    static uint32_t node_%d[] = {\n\t", inode_count);
    /* 4 byte file length + compression flag in high bit */
    put_uint32(out, node->length | (compression ? 0x80000000 : 0));
    fprintf(out, "\t/* compression_flag_bit + file length */\n\t");
    
    /* write out data block structures */
    offset = 4 + (8*blocks) + ((namelen+3) & ~3);
    *totlen += offset;			/* add in the header size */
    for (i = 0; i < blocks; i++) {
	put_uint32(out, node->data_lengths[i]);
	put_uint32(out, offset);
	offset += (node->data_lengths[i]+3) & ~3;
	fprintf(out, "\t/* data_block_length, offset to data_block */\n\t");
    }
    /* write file name (path) padded to 4 byte multiple */
    fprintf(out, "\t/* file name '%s' */\n\t", node->name);
    put_bytes_padded(out, (unsigned char *)node->name, namelen);
    
    /* write out data */
    for (i = 0; i < blocks; i++) {
	put_bytes_padded(out, node->data[i], node->data_lengths[i]);
	clen += node->data_lengths[i];
	*totlen += (node->data_lengths[i]+3) & ~3;	/* padded block length */
    }
    fprintf(out, "\t0 };\t/* end-of-node */\n");

    printf("node '%s' len=%ld", node->name, node->length);
    printf(" %ld blocks", blocks);
    if (compression) {
	printf(", compressed size=%ld", clen);
    }
    printf("\n");
}

void
prefix_add(const char *prefix, const char *filename, char *prefixed_path)
{
    prefixed_path[0] = 0;	/* empty string */
    strcat(prefixed_path, prefix);
    strcat(prefixed_path, filename);
#if defined(__WIN32__) || defined(__OS2__)
    /* On Windows, the paths may (will) have '\' instead of '/' so we translate them */
    {
	int i;

	for (i=0; i<strlen(prefixed_path); i++)
	    if (prefixed_path[i] == '\\')
		prefixed_path[i] = '/';
    }
#endif
}

/* This relies on the gp_enumerate_* which should not return directories, nor	*/
/* should it recurse into directories (unlike Adobe's implementation)		*/
/* paths are checked to see if they are an ordinary file or a path		*/
void process_path(char *path, const char *os_prefix, const char *rom_prefix, Xlist_element *Xlist_head,
		int compression, int *inode_count, int *totlen, FILE *out)
{
    int namelen, excluded, save_count=*inode_count;
    Xlist_element *Xlist_scan;
    char *prefixed_path;
    char *found_path, *rom_filename;
    file_enum *pfenum;
    int ret, block, blocks;
    romfs_inode *node;
    unsigned char *ubuf, *cbuf;
    unsigned long ulen, clen;
    FILE *in;

    prefixed_path = malloc(1024);
    found_path = malloc(1024);
    rom_filename = malloc(1024);
    ubuf = malloc(ROMFS_BLOCKSIZE);
    cbuf = malloc(ROMFS_CBUFSIZE);
    if (ubuf == NULL || cbuf == NULL || prefixed_path == NULL ||
		found_path == NULL || rom_filename == NULL) {
	printf("malloc fail in process_path\n");
	exit(1);
    }
    prefix_add(os_prefix, path, prefixed_path);
    prefix_add(rom_prefix, "", rom_filename);
    strcpy(rom_filename, rom_prefix);

    /* check for the file on the Xlist */
    pfenum = gp_enumerate_files_init(prefixed_path, strlen(prefixed_path),
		    	(gs_memory_t *)&minimal_memory);
    if (pfenum == NULL) {
	printf("gp_enumerate_files_init failed.\n");
	exit(1);
    }
    while ((namelen=gp_enumerate_files_next(pfenum, found_path, 1024)) >= 0) {
	excluded = 0;
	found_path[namelen] = 0;		/* terminate the string */
	/* check to see if the tail of the path we found matches one on the exclusion list */
	for (Xlist_scan = Xlist_head; Xlist_scan != NULL; Xlist_scan = Xlist_scan->next) {
	    if (strlen(found_path) >= strlen(Xlist_scan->path)) {
		if (strcmp(Xlist_scan->path, found_path+strlen(found_path)-strlen(Xlist_scan->path)) == 0) {
		    excluded = 1;
		    break;
		}
	    }
	}
	if (excluded)
	    continue;

	/* process a file */
	node = calloc(1, sizeof(romfs_inode));
	/* get info for this file */
	in = fopen(found_path, "rb");
	if (in == NULL) {
	    printf("unable to open file for processing: %s\n", found_path);
	    continue;
	}
	/* rom_filename + strlen(rom_prefix) is first char after the new prefix we want to add */
	/* found_path + strlen(os_prefix) is the file name after the -P prefix */
	rom_filename[strlen(rom_prefix)] = 0;		/* truncate afater prefix */
	strcat(rom_filename, found_path + strlen(os_prefix));
	node->name = rom_filename;	/* without -P prefix, with -d rom_prefix */
	fseek(in, 0, SEEK_END);
	node->length = ftell(in);
	blocks = (node->length+ROMFS_BLOCKSIZE-1) / ROMFS_BLOCKSIZE + 1;
	node->data_lengths = calloc(blocks, sizeof(*node->data_lengths));
	node->data = calloc(blocks, sizeof(*node->data));
	fclose(in);
	in = fopen(found_path, "rb");
	block = 0;
	while (!feof(in)) {
	    ulen = fread(ubuf, 1, ROMFS_BLOCKSIZE, in);
	    if (!ulen) break;
	    clen = ROMFS_CBUFSIZE;
	    if (compression) {
		/* compress data here */
		ret = compress(cbuf, &clen, ubuf, ulen);
		if (ret != Z_OK) {
		    printf("error compressing data block!\n");
		    exit(1);
		}
	    } else {
		memcpy(cbuf, ubuf, ulen);
		clen = ulen;
	    }
	    node->data_lengths[block] = clen; 
	    node->data[block] = malloc(clen);
	    memcpy(node->data[block], cbuf, clen);
	    block++;
	}
	fclose(in);
	/* write out data for this file */
	inode_write(out, node, compression, *inode_count, totlen);
	/* clean up */
	inode_clear(node);
	free(node);
	(*inode_count)++;
    }
    free(cbuf);
    free(ubuf);
    free(found_path);
    free(prefixed_path);
    if (save_count == *inode_count) {
	printf("warning: no files found from path '%s%s'\n", os_prefix, path);
    }
}

/*
 * Utility for merging all the Ghostscript initialization files (gs_*.ps)
 * into a single file.
 *
 * The following special constructs are recognized in the input files:
 *	%% Replace[%| ]<#lines> (<psfile>)
 *	%% Replace[%| ]<#lines> INITFILES
 * '%' after Replace means that the file to be included intact.
 * If the first non-comment, non-blank line in a file ends with the
 * LanguageLevel 2 constructs << or <~, its section of the merged file
 * will begin with
 *	currentobjectformat 1 setobjectformat
 * and end with
 *	setobjectformat
 * and if any line then ends with with <, the following ASCIIHex string
 * will be converted to a binary token.
 */
/* Forward references */
void merge_to_ps(const char *os_prefix, const char *inname, FILE * in, FILE * config);
int write_init(char *);
bool rl(FILE * in, char *str, int len);
void wsc(const byte *str, int len);
void ws(const byte *str, int len);
void wl(const char *str);
char *doit(char *line, bool intact);
void hex_string_to_binary(FILE *in);
void flush_buf(char *buf);
void mergefile(const char *os_prefix, const char *inname, FILE * in, FILE * config,
	       bool intact);
void flush_line_buf(int len);

typedef struct in_block_s in_block_t;
struct in_block_s {
    struct in_block_s *next;
    unsigned char data[ROMFS_BLOCKSIZE];
};


#define LINE_SIZE 128

/* Globals used for gs_init processing */
char linebuf[LINE_SIZE * 2];		/* make it plenty long to avoid overflow */
in_block_t *in_block_head = NULL;
in_block_t *in_block_tail = NULL;
unsigned char *curr_block_p, *curr_block_end;

int
process_initfile(char *initfile, char *gconfig_h, const char *os_prefix,
		 const char *rom_prefix, int compression, int *inode_count,
		 int *totlen, FILE *out)
{
    int ret, block, blocks;
    romfs_inode *node;
    unsigned char *ubuf, *cbuf;
    char *prefixed_path, *rom_filename;
    unsigned long clen;
    FILE *in;
    FILE *config;
    in_block_t *in_block = NULL;

    ubuf = malloc(ROMFS_BLOCKSIZE);
    cbuf = malloc(ROMFS_CBUFSIZE);
    prefixed_path = malloc(1024);
    rom_filename = malloc(1024);
    if (ubuf == NULL || cbuf == NULL || prefixed_path == NULL ||
	rom_filename == NULL) {
	printf("malloc fail in process_initfile\n");
	/* should free whichever buffers got allocated, but don't bother */
	return -1;
    }

    prefix_add(os_prefix, initfile, prefixed_path);
    prefix_add(rom_prefix, initfile, rom_filename);

    in = fopen(prefixed_path, "r");
    if (in == 0) {
	printf("cannot open initfile at: %s\n", prefixed_path);
	return -1;
    }
    config = fopen(gconfig_h, "r");
    if (config == 0) {
	printf("Cannot open gconfig file %s\n", gconfig_h);
	fclose(in);
	return -1;
    }
    memset(linebuf, 0, sizeof(linebuf));
    node = calloc(1, sizeof(romfs_inode));
    node->name = rom_filename;	/* without -P prefix, with -d rom_prefix */

    merge_to_ps(os_prefix, initfile, in, config);

    fclose(in);
    fclose(config);

/**********/
    node->length = 0;
    in_block = in_block_head;
    while (in_block != NULL) {
	node->length += 
	    in_block != in_block_tail ? ROMFS_BLOCKSIZE : curr_block_p - in_block->data;
	in_block = in_block->next;
    }

    blocks = (node->length+ROMFS_BLOCKSIZE-1) / ROMFS_BLOCKSIZE + 1;
    node->data_lengths = calloc(blocks, sizeof(*node->data_lengths));
    node->data = calloc(blocks, sizeof(*node->data));
    block = 0;

    in_block = in_block_head;
    while (in_block != NULL) {
        int block_len = 
		in_block != in_block_tail ? ROMFS_BLOCKSIZE : curr_block_p - in_block->data;

	clen = ROMFS_CBUFSIZE;
	if (compression) {
	    /* compress data here */
	    ret = compress(cbuf, &clen, in_block->data, block_len);
	    if (ret != Z_OK) {
		printf("error compressing data block!\n");
		exit(1);
	    }
	} else {
	    memcpy(cbuf, in_block->data, block_len);
	    clen = block_len;
	}
	node->data_lengths[block] = clen; 
	node->data[block] = malloc(clen);
	memcpy(node->data[block], cbuf, clen);
	block++;
	in_block = in_block->next;
    }

    /* write data for this file */
    inode_write(out, node, compression, *inode_count, totlen);
    /* clean up */
    inode_clear(node);
    free(node);
    (*inode_count)++;

    free(cbuf);
    free(ubuf);
    free(prefixed_path);
    free(rom_filename);
    return 0;
}

void
flush_line_buf(int len) {
    int remaining_len = len;
    int move_len;

    if (len > LINE_SIZE) {
	printf("*** warning, flush_line called with len (%d) > LINE_SIZE (%d)\n",
		len, LINE_SIZE);
	return;
    }
    /* check for empty list and allocate the first block if needed */
    if (in_block_tail == NULL) {
	in_block_head = in_block_tail = calloc(1, sizeof(in_block_t));
	in_block_tail->next = NULL;	/* calloc really already does this */
	curr_block_p = in_block_head->data;
	curr_block_end = curr_block_p + ROMFS_BLOCKSIZE;
    }
    /* move the data into the in_block buffer */
    do {
	move_len = min(remaining_len, curr_block_end - curr_block_p); 
        memcpy(curr_block_p, linebuf, move_len);
	curr_block_p += move_len;
	if (curr_block_p == curr_block_end) {
	    /* start a new data block appended to the list of blocks */
	    in_block_tail->next =  calloc(1, sizeof(in_block_t));
	    in_block_tail = in_block_tail->next;
	    in_block_tail->next = NULL;	/* calloc really already does this */
	    curr_block_p = in_block_tail->data;
	    curr_block_end = curr_block_p + ROMFS_BLOCKSIZE;
	}
	remaining_len = max(0, remaining_len - move_len);
    } while (remaining_len > 0);

    /* clear the line (to allow 'strlen' to work if the data is not binary */
    memset(linebuf, 0, sizeof(linebuf));
}

/* Read a line from the input. */
bool
rl(FILE * in, char *str, int len)
{
    /*
     * Unfortunately, we can't use fgets here, because the typical
     * implementation only recognizes the EOL convention of the current
     * platform.
     */
    int i = 0, c = getc(in);

    if (c < 0)
	return false;
    while (i < len - 1) {
	if (c < 0 || c == 10)		/* ^J, Unix EOL */
	    break;
	if (c == 13) {		/* ^M, Mac EOL */
	    c = getc(in);
	    if (c != 10 && c >= 0)	/* ^M^J, PC EOL */
		ungetc(c, in);
	    break;
	}
	str[i++] = c;
	c = getc(in);
    }
    str[i] = 0;
    return true;
}

/* Write a string or a line on the output. */
void
wsc(const byte *str, int len)
{
    int n = 0;
    int i;

    if (len >= LINE_SIZE)
	exit(1);

    for (i = 0; i < len; ++i) {
	int c = str[i];

	sprintf(linebuf,
		(c < 32 || c >= 127 ? "%d," :
		 c == '\'' || c == '\\' ? "'\\%c'," : "'%c',"),
		c);
	flush_line_buf(strlen(linebuf));
	if (++n == 15) {
	    linebuf[0] = '\n';
	    flush_line_buf(1);
	    n = 0;
	}
    }
    if (n != 0) {
	flush_line_buf(strlen(linebuf));
	linebuf[0] = '\n';
	flush_line_buf(1);
    }
}
void
ws(const byte *str, int len)
{
    if (len >= LINE_SIZE)
	exit(1);

    memcpy(linebuf, str, len);
    flush_line_buf(len);
}

void
wl(const char *str)
{
    ws((const byte *)str, strlen(str));
    ws((const byte *)"\n", 1);
}

/*
 * Strip whitespace and comments from a line of PostScript code as possible.
 * Return a pointer to any string that remains, or NULL if none.
 * Note that this may store into the string.
 */
char *
doit(char *line, bool intact)
{
    char *str = line;
    char *from;
    char *to;
    int in_string = 0;

    if (intact)
	return str;
    while (*str == ' ' || *str == '\t')		/* strip leading whitespace */
	++str;
    if (*str == 0)		/* all whitespace */
	return NULL;
    if (!strncmp(str, "%END", 4))	/* keep these for .skipeof */
	return str;
    if (str[0] == '%')    /* comment line */
	return NULL;
    /*
     * Copy the string over itself removing:
     *  - All comments not within string literals;
     *  - Whitespace adjacent to '[' ']' '{' '}';
     *  - Whitespace before '/' '(' '<';
     *  - Whitespace after ')' '>'.
     */
    for (to = from = str; (*to = *from) != 0; ++from, ++to) {
	switch (*from) {
	    case '%':
		if (!in_string)
		    break;
		continue;
	    case ' ':
	    case '\t':
		if (to > str && !in_string && strchr(" \t>[]{})", to[-1]))
		    --to;
		continue;
	    case '(':
	    case '<':
	    case '/':
	    case '[':
	    case ']':
	    case '{':
	    case '}':
		if (to > str && !in_string && strchr(" \t", to[-1]))
		    *--to = *from;
                if (*from == '(')
                    ++in_string;
              	continue;
	    case ')':
		--in_string;
		continue;
	    case '\\':
		if (from[1] == '\\' || from[1] == '(' || from[1] == ')')
		    *++to = *++from;
		continue;
	    default:
		continue;
	}
	break;
    }
    /* Strip trailing whitespace. */
    while (to > str && (to[-1] == ' ' || to[-1] == '\t'))
	--to;
    *to = 0;
    return str;
}

/* Copy a hex string to the output as a binary token. */
void
hex_string_to_binary(FILE *in)
{
#define MAX_STR 0xffff	/* longest possible PostScript string token */
    byte *strbuf = (byte *)malloc(MAX_STR);
    byte *q = strbuf;
    int c;
    bool which = false;
    int len;
    byte prefix[3];

    if (strbuf == 0) {
	printf("Unable to allocate string token buffer.\n");
	exit(1);
    }
    while ((c = getc(in)) >= 0) {
	if (isxdigit(c)) {
	    c -= (isdigit(c) ? '0' : islower(c) ? 'a' : 'A');
	    if ((which = !which)) {
		if (q == strbuf + MAX_STR) {
		    printf("Can't handle string token > %d bytes.\n",
			    MAX_STR);
		    exit(1);
		}
		*q++ = c << 4;
	    } else
		q[-1] += c;
	} else if (isspace(c))
	    continue;
	else if (c == '>')
	    break;
	else
	    printf("Unknown character in ASCIIHex string: %c\n", c);
    }
    len = q - strbuf;
    if (len <= 255) {
	prefix[0] = 142;
	prefix[1] = (byte)len;
	ws(prefix, 2);
    } else {
	prefix[0] = 143;
	prefix[1] = (byte)(len >> 8);
	prefix[2] = (byte)len;
	ws(prefix, 3);
    }
    ws(strbuf, len);
    free((char *)strbuf);
#undef MAX_STR
}

/* Merge a file from input to output. */
void
flush_buf(char *buf)
{
    if (buf[0] != 0) {
	wl(buf);
	buf[0] = 0;
    }
}

FILE *
prefix_open(const char *os_prefix, const char *filename)
{
    char *prefixed_path;
    FILE *filep;

    prefixed_path = malloc(1024);
    if (prefixed_path == NULL) {
	printf("malloc problem in prefix_open\n");
	return NULL;
    }
    prefix_add(os_prefix, filename, prefixed_path);
    filep = fopen(prefixed_path, "rb");
    free(prefixed_path);
    return filep;
}

void
mergefile(const char *os_prefix, const char *inname, FILE * in, FILE * config,
	  bool intact)
{
    char line[LINE_SIZE + 1];
    char buf[LINE_SIZE + 1];
    char *str;
    int level = 1;
    bool first = true;

    buf[0] = 0;
    while (rl(in, line, LINE_SIZE)) {
	char psname[LINE_SIZE + 1];
	int nlines;

	if (!strncmp(line, "%% Replace", 10) &&
	    sscanf(line + 11, "%d %s", &nlines, psname) == 2
	    ) {
	    bool do_intact = (line[10] == '%');

	    flush_buf(buf);
	    while (nlines-- > 0)
		rl(in, line, LINE_SIZE);
	    if (psname[0] == '(') {
		FILE *ps;
		
		psname[strlen(psname) - 1] = 0;
		ps = prefix_open(os_prefix, psname + 1);
		if (ps == 0)
		    exit(1);
		mergefile(os_prefix, psname + 1, ps, config, intact || do_intact);
	    } else if (!strcmp(psname, "INITFILES")) {
		/*
		 * We don't want to bind config.h into geninit, so
		 * we parse it ourselves at execution time instead.
		 */
		rewind(config);
		while (rl(config, psname, LINE_SIZE))
		    if (!strncmp(psname, "psfile_(\"", 9)) {
			FILE *ps;
			char *quote = strchr(psname + 9, '"');

			*quote = 0;
			ps = prefix_open(os_prefix, psname + 9);
			if (ps == 0)
			    exit(1);
			mergefile(os_prefix, psname + 9, ps, config, false);
		    }
	    } else {
		printf("Unknown %%%% Replace %d %s\n",
			nlines, psname);
		exit(1);
	    }
	} else if (!strcmp(line, "currentfile closefile")) {
	    /* The rest of the file is debugging code, stop here. */
	    break;
	} else {
	    int len;

	    str = doit(line, intact);
	    if (str == 0)
		continue;
	    len = strlen(str);
	    if (first && len >= 2 && str[len - 2] == '<' &&
		(str[len - 1] == '<' || str[len - 1] == '~')
		) {
		wl("currentobjectformat 1 setobjectformat\n");
		level = 2;
	    }
	    if (level > 1 && len > 0 && str[len - 1] == '<' &&
		(len < 2 || str[len - 2] != '<')
		) {
		/* Convert a hex string to a binary token. */
		flush_buf(buf);
		str[len - 1] = 0;
		wl(str);
		hex_string_to_binary(in);
		continue;
	    }
	    if (buf[0] != '%' &&	/* i.e. not special retained comment */
		strlen(buf) + len < LINE_SIZE &&
		(strchr("(/[]{}", str[0]) ||
		 (buf[0] != 0 && strchr(")[]{}", buf[strlen(buf) - 1])))
		)
		strcat(buf, str);
	    else {
		flush_buf(buf);
		strcpy(buf, str);
	    }
	    first = false;
	}
    }
    flush_buf(buf);
    if (level > 1)
	wl("setobjectformat");
}

/* Merge and produce a PostScript file. */
void
merge_to_ps(const char *os_prefix, const char *inname, FILE * in, FILE * config)
{
    char line[LINE_SIZE + 1];

    while ((rl(in, line, LINE_SIZE), line[0])) {
	sprintf(linebuf, "%s", line );
	wl(linebuf);
    }
    mergefile(os_prefix, inname, in, config, false);
}


int
main(int argc, char *argv[])
{
    int i;
    int inode_count = 0, totlen = 0;
    FILE *out;
    const char *outfilename = "obj/gsromfs.c";
    const char *os_prefix = "";
    const char *rom_prefix = "";
    char *initfile, *gconfig_h;
    int atarg = 1;
    int compression = 1;			/* default to doing compression */
    Xlist_element *Xlist_scan, *Xlist_head = NULL;
    
    if (argc < 2) {
	printf("\n"
                "       Usage: mkromfs [-o outputfile] [options ...] paths\n"
                "           options and paths can be interspersed and are processed in order\n"
                "           options:\n"
                "               -o outputfile   default: obj/gsromfs.c if this option present, must be first.\n"
                "               -P prefix       use prefix to find path. prefix not included in %%rom%%\n"
                "               -X path         exclude the path from further processing.\n"
                "                         Note: The tail of any path encountered will be tested so .svn on the -X\n"
                "                               list will exclude that path in all subsequent paths enumerated.\n"
                "\n"
                "               -d romprefix    directory in %%rom file system (just a prefix string on filename)\n"
                "               -c              compression on\n"
                "               -b              compression off (binary).\n"
                "               -g initfile gconfig_h \n"
                "                       special handling to read the 'gs_init.ps' file (from\n"
                "                       the current -P prefix path), and read the gconfig.h for\n"
                "                       psfile_ entries and combines them into a whitespace\n"
                "                       optimized and no comments form and writes this 'gs_init.ps'\n"
                "                       to the current -d destination path. This is a space and\n"
                "                       startup performance improvement, so also this should be\n"
                "                       BEFORE other stuff in the %rom% list of files (currently\n"
                "                       we do a sequential search in the %rom% directory).\n"
                "\n"
                "                       For performance reasons, it is best to turn off compression\n"
                "                       for the init file. Less frequently accessed files, if they\n"
                "                       are large should still be compressed.\n"
                "\n"
	    );
	exit(1);
    }

    printf("compression will use %d byte blocksize (zlib output buffer %d bytes)\n",
        ROMFS_BLOCKSIZE, ROMFS_CBUFSIZE);
    
    if (argc > 3 && argv[1][0] == '-' && argv[1][1] == 'o') {
	/* process -o option for outputfile */
	outfilename = argv[2];
	atarg += 2;
    }
    printf("   writing romfs data to '%s'\n", outfilename);
    out = fopen(outfilename, "w");

    fprintf(out,"\t/* Generated data for %%rom%% device, see mkromfs.c */\n");
#if ARCH_IS_BIG_ENDIAN
    fprintf(out,"\t/* this code assumes a big endian target platform */\n");
#else
    fprintf(out,"\t/* this code assumes a little endian target platform */\n");
#endif
    fprintf(out,"\n#include \"stdint_.h\"\n");
    fprintf(out,"\n#include \"time_.h\"\n\n");
    fprintf(out,"    time_t gs_romfs_buildtime = %ld;\n\n", time(NULL));

    /* process the remaining arguments (options interspersed with paths) */
    for (; atarg < argc; atarg++) {  
	if (argv[atarg][0] == '-') {
	    /* process an option */
	    switch (argv[atarg][1]) {
	      case 'b':
	        compression = 0;
	        break;
	      case 'c':
	        compression = 1;
	        break;
	      case 'd':
		if (++atarg == argc) {
		    printf("   option %s missing required argument\n", argv[atarg-1]);
		    exit(1);
		}
		rom_prefix = argv[atarg];
	        break;
	      case 'g':
		if ((++atarg) + 1 == argc) {
		    printf("   option %s missing required arguments\n", argv[atarg-1]);
		    exit(1);
		}
		initfile = argv[atarg++];
		gconfig_h = argv[atarg];
		process_initfile(initfile, gconfig_h, os_prefix, rom_prefix, compression,
				&inode_count, &totlen, out);
	        break;
	      case 'P':
		if (++atarg == argc) {
		    printf("   option %s missing required argument\n", argv[atarg-1]);
		    exit(1);
		}
		os_prefix = argv[atarg];
	        break;
	      case 'X':
		if (++atarg == argc) {
		    printf("   option %s missing required argument\n", argv[atarg-1]);
		    exit(1);
		}
		Xlist_scan = malloc(sizeof(Xlist_element));
		if (Xlist_scan == NULL) {
		    exit(1);
		}
		Xlist_scan->next = Xlist_head;
		Xlist_head = Xlist_scan;
		Xlist_head->path = argv[atarg];
	        break;
	      default:
	        printf("  unknown option: %s \n", argv[atarg]);
	    }
	    continue;
	}
	/* process a path or file */
	process_path(argv[atarg], os_prefix, rom_prefix, Xlist_head,
		    compression, &inode_count, &totlen, out);

    }
    /* now write out the array of nodes */
    fprintf(out, "    uint32_t *gs_romfs[] = {\n");
    for (i=0; i<inode_count; i++)
	fprintf(out, "\tnode_%d,\n", i);
    fprintf(out, "\t0 };\n");
    
    fclose(out);

    printf("Total %%rom%% structure size is %d bytes.\n", totlen);
    
    return 0;
}

