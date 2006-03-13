/* Copyright (C) 2001-2006 artofcode LLC.
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
/* %rom% IODevice implementation for a compressed in-memory filesystem */
 
/*
 * This file implements a special %rom% IODevice designed for embedded
 * use. It accesses a compressed filesytem image which may be stored
 * in literal ROM, or more commonly is just static data linked directly
 * into the executable. This can be used for storing postscript library
 * files, fonts, Resources or other data files that Ghostscript needs
 * to run.
 */

#include "std.h"
#include "stdint_.h"
#include "string_.h"
#include "gsiorom.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "gxiodev.h"
#include "stream.h"
#include "zlib.h"

/* device method prototypes */
private iodev_proc_init(romfs_init);
private iodev_proc_open_file(romfs_open_file);
private iodev_proc_enumerate_files(romfs_enumerate_files_init);
private iodev_proc_enumerate_next(romfs_enumerate_next);
private iodev_proc_enumerate_close(romfs_enumerate_close);
/* close is handled by stream closure */

/* device definition */
const gx_io_device gs_iodev_rom =
{
    "%rom%", "FileSystem",
    {romfs_init, iodev_no_open_device,
     romfs_open_file,
     iodev_no_fopen, iodev_no_fclose,
     iodev_no_delete_file, iodev_no_rename_file,
     iodev_no_file_status,
     romfs_enumerate_files_init, romfs_enumerate_next, romfs_enumerate_close, 
     iodev_no_get_params, iodev_no_put_params
    }
};

/* internal state for our device */
typedef struct romfs_state_s {
    int atblock;			/* for later when we decompress by blocks */
} romfs_state;

gs_private_st_simple(st_romfs_state, struct romfs_state_s, "romfs_state");

typedef struct romfs_file_enum_s {
    char *pattern;		/* pattern pointer    */
    int  list_index;		/* next node to visit */
    gs_memory_t *memory;	/* memory structure used */
} romfs_file_enum;

gs_private_st_ptrs1(st_romfs_file_enum, struct romfs_file_enum_s, "romfs_file_enum",
    romfs_file_enum_enum_ptrs, romfs_file_enum_reloc_ptrs, pattern);

private uint32_t get_u32_big_endian(const uint32_t *a);

private uint32_t
get_u32_big_endian(const uint32_t *a)
{
    uint32_t v;
    const unsigned char *c=(const unsigned char *)a;

    v = (c[0]<<24) | (c[1]<<16) | (c[2]<<8) | c[3];
    return v;
}

private int
romfs_init(gx_io_device *iodev, gs_memory_t *mem)
{
    romfs_state *state = gs_alloc_struct(mem, romfs_state, &st_romfs_state, 
                                         "romfs_init(state)");
    if (!state)
	return gs_error_VMerror;
    iodev->state = state;
    return 0;
}

private int
romfs_open_file(gx_io_device *iodev, const char *fname, uint namelen,
    const char *access, stream **ps, gs_memory_t *mem)
{
    extern const uint32_t *gs_romfs[];
    const uint32_t *node_scan = gs_romfs[0], *node = NULL;
    uint32_t filelen, blocks, decompress_len;
    int i, compression;
    char *filename;
    byte *buf;

    /* return an empty stream on error */
    *ps = NULL;

    /* scan the inodes to find the requested file */
    for (i=0; node_scan != 0; i++, node_scan = gs_romfs[i]) {
	filelen = get_u32_big_endian(node_scan) & 0x7fffffff;	/* ignore compression bit */
	blocks = (filelen+ROMFS_BLOCKSIZE-1)/ ROMFS_BLOCKSIZE;
	filename = (char *)(&(node_scan[1+(2*blocks)]));
	if ((namelen == strlen(filename)) && 
	    (strncmp(filename, fname, namelen) == 0)) {
	    node = node_scan;
	    break;
	}
    }
    /* inode points to the file (or NULL if not found */
    if (node == NULL)
	return_error(gs_error_undefinedfilename);

    /* return the uncompressed contents of the string */
    compression = ((get_u32_big_endian(node) & 0x80000000) != 0) ? 1 : 0;
    buf = gs_alloc_string(mem, filelen, "romfs buffer");
    if (buf == NULL) {
	if_debug0('s', "%rom%: could not allocate buffer\n");
	return_error(gs_error_VMerror);
    }
    /* deflate the file into the buffer */
    decompress_len = 0;
    for (i=0; i<blocks; i++) {
	unsigned long block_length = get_u32_big_endian(node+1+(2*i));
	unsigned const long block_offset = get_u32_big_endian(node+2+(2*i));
	unsigned const char *block_data = ((unsigned char *)node) + block_offset;
	int code;

	if (compression) {
	    unsigned long buflen = ROMFS_BLOCKSIZE;

	    /* Decompress the data into this block */
	    code = uncompress (buf+(i*ROMFS_BLOCKSIZE), &buflen,
				block_data, block_length);
	    decompress_len += buflen;
	} else {
	    /* not compressed -- just copy it */
	    memcpy(buf+(i*ROMFS_BLOCKSIZE), block_data, block_length);
	    decompress_len += block_length;
        }
    }
    if (decompress_len != filelen) {
	unsigned long dl=decompress_len, fl=filelen;

	eprintf2("romfs decompression length error. Was %ld should be %ld\n",
		dl, fl);
	return_error(gs_error_ioerror);
    }
    *ps = s_alloc(mem, "romfs");
    sread_string(*ps, buf, filelen);

    /* return success */
    return 0;
}

private file_enum *
romfs_enumerate_files_init(gx_io_device *iodev, const char *pat, uint patlen,
	     gs_memory_t *mem)
{
    romfs_file_enum *penum = gs_alloc_struct(mem, romfs_file_enum, &st_romfs_file_enum,
			   				"romfs_enumerate_files_init(file_enum)");
    if (penum == NULL)
        return NULL;
    memset(penum, 0, sizeof(romfs_file_enum));
    penum->pattern = (char *)gs_alloc_bytes(mem, patlen+1, "romfs_enumerate_file_init(pattern)");
    penum->list_index = 0;		/* start at first node */
    penum->memory = mem;
    if (penum->pattern == NULL) {
	romfs_enumerate_close((file_enum *) penum);
	return NULL;
    }
    memcpy(penum->pattern, pat, patlen);	/* Copy string to buffer */
    penum->pattern[patlen]=0;			/* Terminate string */

    return (file_enum *)penum;
}

private void
romfs_enumerate_close(file_enum *pfen)
{
    romfs_file_enum *penum = (romfs_file_enum *)pfen;
    gs_memory_t *mem = penum->memory;

    if (penum->pattern)
	gs_free_object(mem, penum->pattern, "romfs_enum_init(pattern)");
    gs_free_object(mem, penum, "romfs_enum_init(romfs_enum)");
}

private uint
romfs_enumerate_next(file_enum *pfen, char *ptr, uint maxlen)
{
    extern const uint32_t *gs_romfs[];
    romfs_file_enum *penum = (romfs_file_enum *)pfen;
    
    while (gs_romfs[penum->list_index] != 0) {
	const uint32_t *node = gs_romfs[penum->list_index];
	uint32_t filelen = get_u32_big_endian(node) & 0x7fffffff;	/* ignore compression bit */
	long blocks = (filelen+ROMFS_BLOCKSIZE-1)/ ROMFS_BLOCKSIZE;
	char *filename = (char *)(&(node[1+(2*blocks)]));

	penum->list_index++;		/* bump to next unconditionally */
	if (string_match((byte *)filename, strlen(filename),
			 (byte *)penum->pattern,
			 strlen(penum->pattern), 0)) {
	    if (strlen(filename) < maxlen) 
		memcpy(ptr, filename, strlen(filename));
	    return strlen(filename);	/* if > maxlen, caller will detect rangecheck */
	}
    }
    /* ran off end of list, close the enum */
    romfs_enumerate_close(pfen);
    return ~(uint)0;
}
