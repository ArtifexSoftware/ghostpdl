/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


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
 *	    path is a directory to be enumerated trailing '<slash><star>' must be present.
 *	    gcc complains about comments inside comments so we use '<slash>=/' and '<star>=*' here.
 *	    Note: one should avoid bare "<slash><star>" which may be expanded by the shell
 *	    to "/bin /boot /dev ...". e.g. use "<slash><star>.ttf" or "<slash><star>.ps" instead of "<slash><star>".
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
 *		-C		postscript 'compaction' on
 *		-B		postscript 'compaction' off
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
 *              -s num_files    split the output into <num_files> subfiles.
 *
 */

/* prevent gp.h redefining fopen */
#define fopen fopen
/* prevent gp.h redefining sprintf */
#define sprintf sprintf

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

int gs_log_error(int err, const char *file, int line);
/* Prototype to placate compiler */
int spgetcc(stream *s, bool b);
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
    unsigned long disc_length;		/* length of file on disc */
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

typedef struct {
    int num_splits;
    int max_splits;
    unsigned long *sizes;
    char *outname;
    char *outname_formatted;
} split_data;

#define PATH_STR_LEN 1024

#ifndef ARCH_IS_BIG_ENDIAN
#define ARCH_IS_BIG_ENDIAN 0
#endif
/* This gives what we're running on,
 * whilst ARCH_IS_BIG_ENDIAN gives what we're
 * building for.
 */
static inline int isbigendian(void)
{
    union {
        int i;
        char c[sizeof(int)];
    } u = {1};

    return u.c[0] != 1;
}

/* The gp_local_arg_encoding_get_codepoint() function presnet in (some) OS-specific
 * files (gp_unix.c, gp_win32.c) now uses streams, so it won't compile unless there
 * is an implementation of spgetcc(). We don't actually use the function for mkromfs
 * so we just need to make sure it compiles.
 */
int spgetcc(stream *s, bool b)
{
    (void) s; /* avoid 'unused' warning from compilers */
    (void) b;

    return 0;
}

/* mkromfs doesn't use gp_stat, but it does link gp_misc.c which includes
   call to gp_stat_impl(). Rather than major build upheaval for something not
   used, just define a dummy here for Windows.
 */
#ifdef _WIN32
int gp_stat_impl(const gs_memory_t *mem, const char *path, struct _stat64 *buf)
{
    (void)mem;
    (void)path;
    (void)buf;
    return 0;
}
#ifdef MEMENTO
#undef malloc
#undef free
void* Memento_malloc(size_t s)
{
    return malloc(s);
}
void Memento_free(void* blk)
{
    free(blk);
}
#endif
#endif

int
gs_add_control_path_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, int flags)
{
    return 0;
}

int
gs_remove_control_path_flags(const gs_memory_t *mem, gs_path_control_t type, const char *path, int flags)
{
    return 0;
}

/*******************************************************************************
 * The following are non-redirected printing functions to avoid the need for
 * these included from gsmisc.c (unix gp_ functions, among others, use if_debug).
 *******************************************************************************/
#include <stdarg.h>
#define PRINTF_BUF_LENGTH 1024

static const char msg_truncated[] = "\n*** Previous line has been truncated.\n";

int outprintf(const gs_memory_t *mem, const char *fmt, ...)
{
    int count;
    char buf[PRINTF_BUF_LENGTH];
    va_list args;

    va_start(args, fmt);
    count = vsnprintf(buf, sizeof(buf), fmt, args);
    if (count < 0 || count >= sizeof(buf))  { /* MSVC || C99 */
        fwrite(buf, 1, sizeof(buf) - 1, stdout);
        fwrite(msg_truncated, 1, sizeof(msg_truncated) - 1, stdout);
    } else {
        fwrite(buf, 1, count, stdout);
    }
    va_end(args);
    return count;
}

int errprintf(const gs_memory_t *mem, const char *fmt, ...)
{
    int count;
    char buf[PRINTF_BUF_LENGTH];
    va_list args;

    va_start(args, fmt);
    count = vsnprintf(buf, sizeof(buf), fmt, args);
    if (count < 0 || count >= sizeof(buf))  { /* MSVC || C99 */
        fwrite(buf, 1, sizeof(buf) - 1, stderr);
        fwrite(msg_truncated, 1, sizeof(msg_truncated) - 1, stderr);
    } else {
        fwrite(buf, 1, count, stderr);
    }
    va_end(args);
    return count;
}

int errprintf_nomem(const char *fmt, ...)
{
    int count;
    char buf[PRINTF_BUF_LENGTH];
    va_list args;

    va_start(args, fmt);
    count = vsnprintf(buf, sizeof(buf), fmt, args);
    if (count < 0 || count >= sizeof(buf))  { /* MSVC || C99 */
        fwrite(buf, 1, sizeof(buf) - 1, stderr);
        fwrite(msg_truncated, 1, sizeof(msg_truncated) - 1, stderr);
    } else {
        fwrite(buf, 1, count, stderr);
    }
    va_end(args);
    return count;
}

#if __LINE__                    /* compiler provides it */
void
lprintf_file_and_line(const char *file, int line)
{
    errprintf_nomem("%s(%d): ", file, line);
}
#else
void
lprintf_file_only(FILE * f, const char *file)
{
    errprintf_nomem("%s(?): ", file);
}
#endif

gs_memory_t *gp_get_debug_mem_ptr(void)
{
    return NULL;
}

void
eprintf_program_ident(const char *program_name,
                      long revision_number)
{
    gs_memory_t *mem = gp_get_debug_mem_ptr();

    if (mem == NULL)
        return;
    if (program_name) {
        errprintf(mem, (revision_number ? "%s " : "%s"), program_name);
        if (revision_number) {
            int fpart = revision_number % 100;

            errprintf(mem, "%d.%02d", (int)(revision_number / 100), fpart);
        }
        errprintf(mem, ": ");
    }
}

void
emprintf_program_ident(const gs_memory_t *mem,
                       const char *program_name,
                       long revision_number)
{
    if (program_name) {
        epfm(mem, (revision_number ? "%s " : "%s"), program_name);
        if (revision_number) {
            int fpart = revision_number % 100;

            epfm(mem, "%d.%02d", (int)(revision_number / 100), fpart);
        }
        epfm(mem, ": ");
    }
}

int
gs_log_error(int err, const char *file, int line)
{
    if (file == NULL)
        errprintf(NULL, "Returning error %d.\n", err);
    else
        errprintf(NULL, "%s(%d): Returning error %d.\n",
                 (const char *)file, line, err);
    return err;
}

/*******************************************************************************
 * The following is a REALLY minimal gs_memory_t for use by the gp_ functions
 *******************************************************************************/

byte *minimal_alloc_bytes(gs_memory_t * mem, size_t size, client_name_t cname);
byte *minimal_alloc_byte_array(gs_memory_t * mem, size_t num_elements,
                               size_t elt_size, client_name_t cname);
void *minimal_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
                           client_name_t cname);
void minimal_free_object(gs_memory_t * mem, void * data, client_name_t cname);
void minimal_free_string(gs_memory_t * mem, byte * data, size_t nbytes, client_name_t cname);

byte *
minimal_alloc_bytes(gs_memory_t * mem, size_t size, client_name_t cname)
{
    return malloc(size);
}

byte *
minimal_alloc_byte_array(gs_memory_t * mem, size_t num_elements,
                         size_t elt_size, client_name_t cname)
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
minimal_free_string(gs_memory_t * mem, byte * data, size_t nbytes, client_name_t cname)
{
    free(data);
    return;
}

/* Minimal thread functions (needed by gs_next_ids */
int gp_monitor_enter(gp_monitor *);
int gp_monitor_leave(gp_monitor *);

int
gp_monitor_enter(gp_monitor * mon)
{
    return 0;
}

int
gp_monitor_leave(gp_monitor * mon)
{
    return 0;
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

int cmpstringp(const void *p1, const void *p2);
void put_uint32(FILE *out, const unsigned int q);
void put_bytes_padded(FILE *out, unsigned char *p, unsigned int len);
void inode_clear(romfs_inode* node);
void inode_write(FILE *out, romfs_inode *node, int compression, int inode_count, int*totlen, split_data *splits, int verbose);
void process_path(char *path, const char *os_prefix, const char *rom_prefix,
                  Xlist_element *Xlist_head, int compression,
                  int compaction, int *inode_count, int *totlen, FILE *out,
                  split_data *splits, int verbose);
FILE *prefix_open(const char *os_prefix, const char *inname, int verbose);
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
        if (isbigendian() != ARCH_IS_BIG_ENDIAN) {
            fprintf (out, "0x%02x%02x%02x%02x,", (w2c.w) & 0xff, (w2c.w>>8) & 0xff, (w2c.w>>16) & 0xff, (w2c.w >>24) & 0xff);
        }
        else {
            fprintf(out, "0x%08x,", w2c.w);
        }
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
        if (isbigendian() != ARCH_IS_BIG_ENDIAN) {
            fprintf (out, "0x%02x%02x%02x%02x,", (w2c.w) & 0xff, (w2c.w>>8) & 0xff, (w2c.w>>16) & 0xff, (w2c.w >>24) & 0xff);
        }
        else {
            fprintf(out, "0x%08x,", w2c.w);
        }
      default: ;
    }
    fprintf(out, "\n\t");
}

/* clear the internal memory of an inode */
void inode_clear(romfs_inode* node)
{
    int i, blocks;

    if (node) {
        blocks = (node->disc_length+ROMFS_BLOCKSIZE-1)/ROMFS_BLOCKSIZE;
        if (node->data) {
            for (i = 0; i < blocks; i++) {
                if (node->data[i]) free(node->data[i]);
            }
            free(node->data);
        }
        if (node->data_lengths) free(node->data_lengths);
    }
}

static unsigned long
do_inode_write(FILE *out, romfs_inode *node, int compression, int inode_count, int *totlen, int split, int verbose)
{
    int i, offset;
    int blocks = (node->length+ROMFS_BLOCKSIZE-1)/ROMFS_BLOCKSIZE;
    int namelen = strlen(node->name) + 1;	/* include terminating <nul> */
    int clen = 0;			/* compressed length */

    /* write the node header */
    fprintf(out,"    %s const uint32_t %snode_%d[] = {\n\t",
            split ? "" : "static",
            split ? "mkromfs_" : "",
            inode_count);
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

    if (verbose) {
        printf("node '%s' len=%ld", node->name, node->length);
        printf(" %d blocks", blocks);
        if (compression) {
            printf(", compressed size=%d", clen);
        }
        printf("\n");
    }
    if (compression)
        return clen;
    return node->length;
}

static void
prepare_splits(split_data *splits)
{
    if (splits->num_splits) {
        /* Limit splits->num_splits to something merely insane to avoid overflow. */
        if (splits->num_splits > (1<<(8*sizeof(int)-2))/sizeof(unsigned long))
            splits->num_splits = (1<<(8*sizeof(int)-2))/sizeof(unsigned long);
        /* Make sure we have a properly sized size array. */
        if (splits->num_splits > splits->max_splits) {
            unsigned long *sizes = realloc(splits->sizes, sizeof(unsigned long) * splits->num_splits);
            if (sizes == NULL) {
                fprintf(stderr, "Failed to allocate split data array\n");
                exit(1);
            }
            memset(&sizes[splits->max_splits], 0, sizeof(unsigned long) * (splits->num_splits - splits->max_splits));
            splits->sizes = sizes;
            splits->max_splits = splits->num_splits;
        }
    }
}

static void
start_file(FILE *out)
{
    fprintf(out,"\t/* Generated data for %%rom%% device, see mkromfs.c */\n");
#if ARCH_IS_BIG_ENDIAN
    fprintf(out,"\t/* this code assumes a big endian target platform */\n");
#else
    fprintf(out,"\t/* this code assumes a little endian target platform */\n");
#endif
    fprintf(out,"\n#include \"stdint_.h\"\n");
    fprintf(out,"\n#include \"time_.h\"\n\n");
}

/* write out an inode and its file data */
void
inode_write(FILE *out, romfs_inode *node, int compression, int inode_count, int *totlen, split_data *splits, int verbose)
{
    prepare_splits(splits);
    if (splits->max_splits) {
        /* Find the smallest bin to add this to. */
        FILE *out2;
        int which = 0;
        int i;
        for (i = 1; i < splits->max_splits; i++) {
            if (splits->sizes[which] > splits->sizes[i])
                which = i;
        }

        sprintf(splits->outname_formatted, splits->outname, which);
        if (splits->sizes[which] == 0) {
            out2 = fopen(splits->outname_formatted, "w");
            start_file(out2);
        } else {
            out2 = fopen(splits->outname_formatted, "a");
        }
        splits->sizes[which] += do_inode_write(out2, node, compression, inode_count, totlen, 1, verbose);
        fclose(out2);
    } else
        (void)do_inode_write(out, node, compression, inode_count, totlen, 0, verbose);
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

/* Simple ps compaction routines; strip comments, compact whitespace */
typedef enum {
    PSC_BufferIn = 0,
    PSC_InComment,
    PSC_InString,
    PSC_InHexString,
    PSC_BufferOut,
    PSC_BufferCopy,
} psc_state;

typedef int (psc_getc)(void *);
typedef void (psc_ungetc)(int, void *);
typedef int (psc_feof)(void *);

typedef struct {
    psc_state state;
    int inpos;
    int inmax;
    int outpos;
    int outend;
    int outmax;
    int buffercopy;
    int wasascii;
    unsigned char *bufferin;
    unsigned char *bufferout;
    psc_getc *pgetc;
    psc_ungetc *unpgetc;
    psc_feof *peof;
    void *file;
    int names;
    int binary;
    int noescape;
    int escaping;
    int paren;
    int firstnum;
} pscompstate;

static void pscompact_start(pscompstate *psc, psc_getc *myfgetc, psc_ungetc *myungetc, psc_feof *myfeof, void *myfile, int names, int binary, int firstnum)
{
    psc->state = PSC_BufferIn;
    psc->bufferin = malloc(80);
    psc->bufferout = malloc(80);
    if ((psc->bufferin == NULL) || (psc->bufferout == NULL)) {
        fprintf(stderr, "Malloc failed in ps compaction\n");
        exit(1);
    }
    psc->inmax = 80;
    psc->outmax = 80;
    psc->inpos = 0;
    psc->wasascii = 0;
    psc->pgetc = myfgetc;
    psc->unpgetc = myungetc;
    psc->peof = myfeof;
    psc->file = myfile;
    psc->names = names;
    psc->binary = binary;
    psc->noescape = 0;
    psc->escaping = 0;
    psc->paren = 0;
    psc->firstnum = firstnum;
}

static void pscompact_end(pscompstate *psc)
{
    free(psc->bufferin);
    free(psc->bufferout);
}

static void pscompact_copy(pscompstate *psc, int c, int n)
{
    psc->bufferout[0] = c;
    psc->outend = 1;
    psc->outpos = 0;
    psc->buffercopy = n;
    if (n == 0)
        psc->state = PSC_BufferOut;
    else
        psc->state = PSC_BufferCopy;
}

static void pscompact_copy2(pscompstate *psc, int c1, int c2, int n)
{
    psc->bufferout[0] = c1;
    psc->bufferout[1] = c2;
    psc->outend = 2;
    psc->outpos = 0;
    psc->buffercopy = n;
    if (n == 0)
        psc->state = PSC_BufferOut;
    else
        psc->state = PSC_BufferCopy;
}

static void pscompact_copy3(pscompstate *psc, int c1, int c2, int c3, int n)
{
    psc->bufferout[0] = c1;
    psc->bufferout[1] = c2;
    psc->bufferout[2] = c3;
    psc->outend = 3;
    psc->outpos = 0;
    psc->buffercopy = n;
    if (n == 0)
        psc->state = PSC_BufferOut;
    else
        psc->state = PSC_BufferCopy;
}

static void pscompact_buffer(pscompstate *psc, int c)
{
    if (psc->inpos == psc->inmax) {
        psc->inmax *= 2;
        psc->bufferin = realloc(psc->bufferin, psc->inmax * 2);
        if (psc->bufferin == NULL) {
            fprintf(stderr, "Realloc failed in pscompaction\n");
            exit(1);
        }
    }
    psc->bufferin[psc->inpos++] = c;
}

static void pscompact_bufferatstart(pscompstate *psc, int c)
{
    if (psc->inpos == psc->inmax) {
        psc->inmax *= 2;
        psc->bufferin = realloc(psc->bufferin, psc->inmax * 2);
        if (psc->bufferin == NULL) {
            fprintf(stderr, "Realloc failed in pscompaction\n");
            exit(1);
        }
    }
    memmove(psc->bufferin+1, psc->bufferin, psc->inpos);
    psc->inpos++;
    psc->bufferin[0] = c;
}

static void pscompact_copyinout(pscompstate *psc)
{
    if (psc->outmax < psc->inpos) {
        psc->outmax = psc->inmax;
        psc->bufferout = realloc(psc->bufferout, psc->outmax);
        if (psc->bufferout == NULL) {
            fprintf(stderr, "Realloc failed in pscompaction\n");
            exit(1);
        }
    }
    memcpy(psc->bufferout, psc->bufferin, psc->inpos);
    psc->outpos = 0;
    psc->outend = psc->inpos;
    psc->state = PSC_BufferOut;
    psc->inpos = 0;
}

static void pscompact_copyinout_bin(pscompstate *psc)
{
    pscompact_copyinout(psc);
    psc->noescape = 1;
}

static void pscompact_hex2ascii(pscompstate *psc)
{
    int i = 0;
    int o = 0;

    while (i < psc->inpos) {
        int v = 0;

        if ((psc->bufferin[i] >= '0') && (psc->bufferin[i] <= '9')) {
            v = (psc->bufferin[i] - '0')<<4;
        } else if ((psc->bufferin[i] >= 'a') && (psc->bufferin[i] <= 'f')) {
            v = (psc->bufferin[i] - 'a' + 10)<<4;
        } else if ((psc->bufferin[i] >= 'A') && (psc->bufferin[i] <= 'F')) {
            v = (psc->bufferin[i] - 'A' + 10)<<4;
        } else {
            fprintf(stderr, "Malformed hexstring in pscompaction!\n");
            exit(1);
        }
        i++;

        if (i == psc->inpos) {
            /* End of string */
        } else if ((psc->bufferin[i] >= '0') && (psc->bufferin[i] <= '9')) {
            v += psc->bufferin[i] - '0';
        } else if ((psc->bufferin[i] >= 'a') && (psc->bufferin[i] <= 'f')) {
            v += psc->bufferin[i] - 'a' + 10;
        } else if ((psc->bufferin[i] >= 'A') && (psc->bufferin[i] <= 'F')) {
            v += psc->bufferin[i] - 'A' + 10;
        } else {
            fprintf(stderr, "Malformed hexstring in pscompaction!\n");
            exit(1);
        }
        i++;
        psc->bufferin[o++] = v;
    }
    psc->inpos = o;
}

static const char *pscompact_names[] =
{
  "abs",
  "add",
  "aload",
  "anchorsearch",
  "and",
  "arc",
  "arcn",
  "arct",
  "arcto",
  "array",
  "ashow",
  "astore",
  "awidthshow",
  "begin",
  "bind",
  "bitshift",
  "ceiling",
  "charpath",
  "clear",
  "cleartomark",
  "clip",
  "clippath",
  "closepath",
  "concat",
  "concatmatrix",
  "copy",
  "count",
  "counttomark",
  "currentcmykcolor",
  "currentdash",
  "currentdict",
  "currentfile",
  "currentfont",
  "currentgray",
  "currentgstate",
  "currenthsbcolor",
  "currentlinecap",
  "currentlinejoin",
  "currentlinewidth",
  "currentmatrix",
  "currentpoint",
  "currentrgbcolor",
  "currentshared",
  "curveto",
  "cvi",
  "cvlit",
  "cvn",
  "cvr",
  "cvrs",
  "cvs",
  "cvx",
  "def",
  "defineusername",
  "dict",
  "div",
  "dtransform",
  "dup",
  "end",
  "eoclip",
  "eofill",
  "eoviewclip",
  "eq",
  "exch",
  "exec",
  "exit",
  "file",
  "fill",
  "findfont",
  "flattenpath",
  "floor",
  "flush",
  "flushfile",
  "for",
  "forall",
  "ge",
  "get",
  "getinterval",
  "grestore",
  "gsave",
  "gstate",
  "gt",
  "identmatrix",
  "idiv",
  "idtransform",
  "if",
  "ifelse",
  "image",
  "imagemask",
  "index",
  "ineofill",
  "infill",
  "initviewclip",
  "inueofill",
  "inufill",
  "invertmatrix",
  "itransform",
  "known",
  "le",
  "length",
  "lineto",
  "load",
  "loop",
  "lt",
  "makefont",
  "matrix",
  "maxlength",
  "mod",
  "moveto",
  "mul",
  "ne",
  "neg",
  "newpath",
  "not",
  "null",
  "or",
  "pathbbox",
  "pathforall",
  "pop",
  "print",
  "printobject",
  "put",
  "putinterval",
  "rcurveto",
  "read",
  "readhexstring",
  "readline",
  "readstring",
  "rectclip",
  "rectfill",
  "rectstroke",
  "rectviewclip",
  "repeat",
  "restore",
  "rlineto",
  "rmoveto",
  "roll",
  "rotate",
  "round",
  "save",
  "scale",
  "scalefont",
  "search",
  "selectfont",
  "setbbox",
  "setcachedevice",
  "setcachedevice2",
  "setcharwidth",
  "setcmykcolor",
  "setdash",
  "setfont",
  "setgray",
  "setgstate",
  "sethsbcolor",
  "setlinecap",
  "setlinejoin",
  "setlinewidth",
  "setmatrix",
  "setrgbcolor",
  "setshared",
  "shareddict",
  "show",
  "showpage",
  "stop",
  "stopped",
  "store",
  "string",
  "stringwidth",
  "stroke",
  "strokepath",
  "sub",
  "systemdict",
  "token",
  "transform",
  "translate",
  "truncate",
  "type",
  "uappend",
  "ucache",
  "ueofill",
  "ufill",
  "undef",
  "upath",
  "userdict",
  "ustroke",
  "viewclip",
  "viewclippath",
  "where",
  "widthshow",
  "write",
  "writehexstring",
  "writeobject",
  "writestring",
  "wtranslation",
  "xor",
  "xshow",
  "xyshow",
  "yshow",
  "FontDirectory",
  "SharedFontDirectory",
  "Courier%",
  "Courier-Bold",
  "Courier-BoldOblique",
  "Courier-Oblique",
  "Helvetica",
  "Helvetica-Bold",
  "Helvetica-BoldOblique",
  "Helvetica-Oblique",
  "Symbol",
  "Times-Bold",
  "Times-BoldItalic",
  "Times-Italic",
  "Times-Roman",
  "execuserobject",
  "currentcolor",
  "currentcolorspace",
  "currentglobal",
  "execform",
  "filter",
  "findresource",
  "globaldict",
  "makepattern",
  "setcolor",
  "setcolorspace",
  "setglobal",
  "setpagedevice",
  "setpattern"
};

static int pscompact_isname(pscompstate *psc, int *i)
{
    int off = 0;
    int n;

    if (psc->bufferin[0] == '/')
        off = 1;
    for (n = 0; n < sizeof(pscompact_names)/sizeof(char *); n++) {
        if (strncmp(pscompact_names[n], (const char *)&psc->bufferin[off], psc->inpos-off) == 0) {
            /* Match! */
            if (off)
                *i = -1-n;
            else
                *i = n;
            return 1;
        }
    }
    return 0;
}

static int pscompact_isint(pscompstate *psc, int *i)
{
    int pos = 0;

    if ((psc->bufferin[0] == '+') || (psc->bufferin[0] == '-')) {
        pos = 1;
    }
    if (pos >= psc->inpos)
        return 0;
    if ((psc->inpos > pos+3) &&
        (strncmp((const char *)&psc->bufferin[pos], "16#", 3) == 0)) {
        /* hex */
        int v = 0;
        pos += 3;
        while (pos < psc->inpos) {
            if ((psc->bufferin[pos] >= '0') && (psc->bufferin[pos] <= '9'))
                v = v*16 + psc->bufferin[pos] - '0';
            else if ((psc->bufferin[pos] >= 'a') && (psc->bufferin[pos] <= 'f'))
                v = v*16 + psc->bufferin[pos] - 'a' + 10;
            else if ((psc->bufferin[pos] >= 'A') && (psc->bufferin[pos] <= 'F'))
                v = v*16 + psc->bufferin[pos] - 'A' + 10;
            else
                return 0;
            pos++;
        }
        if (psc->bufferin[0] == '-')
            v = -v;
        *i = v;
        return 1;
    }

    do {
        if ((psc->bufferin[pos] < '0') || (psc->bufferin[pos] > '9'))
            return 0;
        pos++;
    } while (pos < psc->inpos);

    if (psc->inpos == psc->inmax) {
        psc->inmax *= 2;
        psc->bufferin = realloc(psc->bufferin, psc->inmax);
    }
    psc->bufferin[psc->inpos] = 0;
    *i = atoi((const char *)psc->bufferin);

    /* Check for 32bit overflow */
    if (psc->inpos > 9) {
        char *end;
        double d = strtod((const char *)psc->bufferin, &end);
        if (d != (double)(int)*i)
            return 0;
    }

    return 1;
}

static int pscompact_isfloat(pscompstate *psc, float *f)
{
    int pos = 0;
    int point = 0;

    if ((psc->bufferin[0] == '+') || (psc->bufferin[0] == '-')) {
        pos = 1;
    }
    if (pos >= psc->inpos)
        return 0;
    do {
        if ((psc->bufferin[pos] >= '0') && (psc->bufferin[pos] <= '9')) {
            /* Digits are OK */
        } else if ((psc->bufferin[pos] == '.') && (point == 0)) {
            /* as are points, but only the first one */
            point = 1;
        } else {
            /* Anything else is a failure */
            return 0;
        }
        pos++;
    } while (pos < psc->inpos);

    if (psc->inpos == psc->inmax) {
        psc->inmax *= 2;
        psc->bufferin = realloc(psc->bufferin, psc->inmax);
    }
    psc->bufferin[psc->inpos] = 0;
    *f = atof((const char *)psc->bufferin);
    return 1;
}

static unsigned long pscompact_getcompactedblock(pscompstate *psc, unsigned char *ubuf, unsigned long ulen)
{
    unsigned char *out;
    int c;

    if (ulen == 0)
        return 0;
    out = ubuf;
    do {
        switch (psc->state) {
            case PSC_BufferIn:
                c = psc->pgetc(psc->file);
                if (c <= 32) {
                    /* Whitespace (or EOF) */
                    if (psc->inpos == 0) {
                        /* Leading whitespace, just bin it */
                        break;
                    }
                } else if (c == '(') {
                    /* Start of a string */
                    if (psc->inpos == 0) {
                        /* Go into string state */
                        psc->state = PSC_InString;
                        psc->paren = 1;
                        break;
                    }
                } else if (c == '>') {
                    /* End of a dictionary */
                    if (psc->inpos == 0) {
                        /* Just output it (with no whitespace) */
                        *out++ = c;
                        break;
                    }
                } else if ((c == '{') || (c == '}') ||
                           (c == '[') || (c == ']')) {
                    /* Stand alone token bytes */
                    if (psc->inpos == 0) {
                        /* Process now and be done with it */
                        *out++ = c;
                        psc->wasascii = 0;
                        break;
                    }
                } else if ((c >= 128) && (c <= 131)) {
                    fprintf(stderr, "Can't compact files with binary object sequences in!");
                    exit(1);
                } else if ((c == 132) || (c == 133) || (c == 138) || (c == 139) || (c == 140)) {
                    /* 32 bit integers or reals */
                    if (psc->inpos == 0) {
                        pscompact_copy(psc, c, 4);
                        break;
                    }
                } else if ((c == 134) || (c == 135)) {
                    /* 16 bit integers */
                    if (psc->inpos == 0) {
                        pscompact_copy(psc, c, 2);
                        break;
                    }
                } else if ((c == 136) || (c == 141) || (c == 145) ||
                           (c == 146) || (c == 147) || (c == 148)) {
                    /* 8 bit integers or bools or pool name */
                    if (psc->inpos == 0) {
                        pscompact_copy(psc, c, 1);
                        break;
                    }
                } else if (c == 137) {
                    /* fixed point */
                    if (psc->inpos == 0) {
                        int r = psc->pgetc(psc->file);
                        if (r & 32) {
                            pscompact_copy2(psc, c, r, 2);
                        } else {
                            pscompact_copy2(psc, c, r, 4);
                        }
                        break;
                    }
                } else if (c == 142) {
                    /* short string */
                    if (psc->inpos == 0) {
                        int n = psc->pgetc(psc->file);
                        pscompact_copy2(psc, c, n, n);
                        break;
                    }
                } else if (c == 143) {
                    /* long string */
                    if (psc->inpos == 0) {
                        int n1 = psc->pgetc(psc->file);
                        int n2 = psc->pgetc(psc->file);
                        pscompact_copy3(psc, c, n1, n2, (n1<<8) + n2);
                        break;
                    }
                } else if (c == 144) {
                    /* long string */
                    if (psc->inpos == 0) {
                        int n1 = psc->pgetc(psc->file);
                        int n2 = psc->pgetc(psc->file);
                        pscompact_copy3(psc, c, n1, n2, n1 + (n2<<8));
                        break;
                    }
                } else if ((c >= 149) && (c <= 159)) {
                    fprintf(stderr, "Can't compact files with binary postscript byte %d in!", c);
                    exit(1);
                } else if (c == '%') {
                    if (psc->inpos == 0) {
                        psc->state = PSC_InComment;
                        break;
                    }
                } else if (c == '<') {
                    if (psc->inpos == 0) {
                        psc->state = PSC_InHexString;
                        break;
                    }
                } else if ((c == '/') &&
                           (psc->inpos > 0) &&
                           (psc->bufferin[psc->inpos-1] != '/')) {
                    /* We hit a / while not in a prefix of them - stop the
                     * buffering. */
                } else {
                    /* Stick c into the buffer and continue */
                    pscompact_buffer(psc, c);
                    break;
                }
                /* If we reach here, we have a complete buffer full. We need
                 * to write it (or something equivalent to it) out. */
                if (c > 32) {
                    /* Put c back into the file to process next time */
                    psc->unpgetc(c, psc->file);
                }
                if (psc->binary) {
                    int i;
                    float f;
                    /* Is it a number? */
                    if (pscompact_isint(psc, &i)) {
                        if (psc->firstnum) {
                            /* Don't alter the first number in a file */
                            psc->firstnum = 0;
                        } else if ((i >= -128) && (i <= 127)) {
                            /* Encode as a small integer */
                            psc->bufferout[0] = 136;
                            psc->bufferout[1] = i & 255;
                            psc->inpos = 0;
                            psc->outend = 2;
                            psc->wasascii = 0;
                            psc->noescape = 1;
                            psc->state = PSC_BufferOut;
                            break;
                        } else if ((i >= -0x8000) && (i <= 0x7FFF)) {
                            /* Encode as a 16 bit integer */
                            psc->bufferout[0] = 135;
                            psc->bufferout[1] = i & 255;
                            psc->bufferout[2] = (i>>8) & 255;
                            psc->inpos = 0;
                            psc->outpos = 0;
                            psc->outend = 3;
                            psc->wasascii = 0;
                            psc->noescape = 1;
                            psc->state = PSC_BufferOut;
                            break;
                        } else {
                            /* Encode as a 32 bit integer */
                            psc->bufferout[0] = 133;
                            psc->bufferout[1] = i & 255;
                            psc->bufferout[2] = (i>>8) & 255;
                            psc->bufferout[3] = (i>>16) & 255;
                            psc->bufferout[4] = (i>>24) & 255;
                            psc->inpos = 0;
                            psc->outpos = 0;
                            psc->outend = 5;
                            psc->wasascii = 0;
                            psc->noescape = 1;
                            psc->state = PSC_BufferOut;
                            break;
                        }
                    } else if ((sizeof(float) == 4) && pscompact_isfloat(psc, &f)) {
                        /* Encode as a 32 bit float */
                        union {
                            float f;
                            unsigned char c[4];
                        } fc;
                        fc.f = 1.0;
                        if ((fc.c[0] == 0) && (fc.c[1] == 0) &&
                            (fc.c[2] == 0x80) && (fc.c[3] == 0x3f)) {
                            fc.f = f;
                            psc->bufferout[0] = 139;
                            psc->bufferout[1] = (char)fc.c[0];
                            psc->bufferout[2] = (char)fc.c[1];
                            psc->bufferout[3] = (char)fc.c[2];
                            psc->bufferout[4] = (char)fc.c[3];
                            psc->inpos = 0;
                            psc->outpos = 0;
                            psc->outend = 5;
                            psc->wasascii = 0;
                            psc->noescape = 1;
                            psc->state = PSC_BufferOut;
                            break;
                        } else if ((fc.c[0] == 0x3f) && (fc.c[1] == 0x80) &&
                                   (fc.c[2] == 0) && (fc.c[3] == 0)) {
                            fc.f = f;
                            psc->bufferout[0] = 139;
                            psc->bufferout[1] = (char)fc.c[3];
                            psc->bufferout[2] = (char)fc.c[2];
                            psc->bufferout[3] = (char)fc.c[1];
                            psc->bufferout[4] = (char)fc.c[0];
                            psc->inpos = 0;
                            psc->outpos = 0;
                            psc->outend = 5;
                            psc->wasascii = 0;
                            psc->noescape = 1;
                            psc->state = PSC_BufferOut;
                            break;
                        }
                    }
                    if ((psc->inpos == 4) &&
                        (strncmp((const char *)psc->bufferin, "true", 4) == 0)) {
                        /* Encode as a 32 bit integer */
                        psc->bufferout[0] = 141;
                        psc->bufferout[1] = 1;
                        psc->inpos = 0;
                        psc->outpos = 0;
                        psc->outend = 2;
                        psc->wasascii = 0;
                        psc->noescape = 1;
                        psc->state = PSC_BufferOut;
                        break;
                    }
                    if ((psc->inpos == 5) &&
                        (strncmp((const char *)psc->bufferin, "false", 5) == 0)) {
                        /* Encode as a 32 bit integer */
                        psc->bufferout[0] = 141;
                        psc->bufferout[1] = 0;
                        psc->inpos = 0;
                        psc->outpos = 0;
                        psc->outend = 2;
                        psc->wasascii = 0;
                        psc->noescape = 1;
                        psc->state = PSC_BufferOut;
                        break;
                    }
                    if (psc->names && pscompact_isname(psc, &i)) {
                        /* Encode as a name lookup */
                        if (i >= 0) {
                            /* Executable */
                            psc->bufferout[0] = 146;
                            psc->bufferout[1] = i;
                        } else {
                            /* Literal */
                            psc->bufferout[0] = 145;
                            psc->bufferout[1] = 1-i;
                        }
                        psc->inpos = 0;
                        psc->outpos = 0;
                        psc->outend = 2;
                        psc->wasascii = 0;
                        psc->noescape = 1;
                        psc->state = PSC_BufferOut;
                        break;
                    }
                }
                if ((psc->wasascii) && (psc->bufferin[0]!='/'))
                    *out++ = 32;
                pscompact_copyinout(psc);
                psc->wasascii = 1;
                break;
            case PSC_BufferOut:
            {
                unsigned char c = psc->bufferout[psc->outpos++];
                if (psc->noescape) {
                    /* No change */
                } else if ((c == 10) && (psc->outpos < psc->outend)) {
                    if (!psc->escaping) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        c = 'n';
                        psc->escaping = 0;
                    }
                } else if (c == 9) {
                    if (!psc->escaping) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        c = 't';
                        psc->escaping = 0;
                    }
                } else if (c == 8) {
                    if (!psc->escaping) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        c = 'b';
                        psc->escaping = 0;
                    }
                } else if (c == 12) {
                    if (!psc->escaping) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        c = 'f';
                        psc->escaping = 0;
                    }
                } else if (c == 13) {
                    if (!psc->escaping) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        c = 'r';
                        psc->escaping = 0;
                    }
                } else if (c == '\\') {
                    if (!psc->escaping) {
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        psc->escaping = 0;
                    }
                } else if ((c == ')') && (psc->outpos < psc->outend)) {
                    if (!psc->escaping) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        c = ')';
                        psc->escaping = 0;
                    }
                } else if ((c == '(') && (psc->outpos > 1)) {
                    if (!psc->escaping) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else {
                        c = '(';
                        psc->escaping = 0;
                    }
                } else if (((c < 32) && (c != 10)) || (c >= 128)) {
                    if (psc->escaping == 0) {
                        c = '\\';
                        psc->outpos--;
                        psc->escaping = 1;
                    } else if (psc->escaping == 1) {
                        c = '0' + ((c >> 6)&3);
                        psc->outpos--;
                        psc->escaping = 2;
                    } else if (psc->escaping == 2) {
                        c = '0' + ((c >> 3)&7);
                        psc->outpos--;
                        psc->escaping = 3;
                    } else if (psc->escaping == 3) {
                        c = '0' + (c&7);
                        psc->escaping = 0;
                    }
                }
                *out++ = c;
                if (psc->outpos == psc->outend) {
                    psc->outpos = 0;
                    psc->outend = 0;
                    psc->noescape = 0;
                    psc->state = PSC_BufferIn;
                }
                break;
            }
            case PSC_BufferCopy:
                if (psc->outpos < psc->outend) {
                    *out++ = psc->bufferout[psc->outpos++];
                    break;
                }
                *out++ = psc->pgetc(psc->file);
                psc->buffercopy--;
                if (psc->buffercopy == 0) {
                    psc->outpos = 0;
                    psc->outend = 0;
                    psc->state = PSC_BufferIn;
                }
                break;
            case PSC_InString:
                c = psc->pgetc(psc->file);
                if ((c == ')') && (--psc->paren == 0)) {
                    psc->wasascii = 0;
                    if (psc->binary) {
                        /* Write out the string as binary */
                        if (psc->inpos < 256) {
                            pscompact_bufferatstart(psc, psc->inpos);
                            pscompact_bufferatstart(psc, 142);
                            pscompact_copyinout_bin(psc);
                            break;
                        } else if (psc->inpos < 65536) {
                            int count = psc->inpos;
                            pscompact_bufferatstart(psc, count>>8);
                            pscompact_bufferatstart(psc, count & 255);
                            pscompact_bufferatstart(psc, 144);
                            pscompact_copyinout_bin(psc);
                            break;
                        }
                    }
                    /* if all else fails, just write it out as an ascii
                     * string. */
                    pscompact_bufferatstart(psc, '(');
                    pscompact_buffer(psc, ')');
                    pscompact_copyinout(psc);
                    break;
                } else if (c == '\\') {
                    c = psc->pgetc(psc->file);
                    if (c == 10)
                        break;
                    else if (c == 'b')
                        c = 8;
                    else if (c == 't')
                        c = 9;
                    else if (c == 'n')
                        c = 10;
                    else if (c == 'f')
                        c = 12;
                    else if (c == 'r')
                        c = 13;
                    else if ((c >= '0') && (c <= '7')) {
                        int d;
                        c = (c - '0');
                        d = psc->pgetc(psc->file);
                        if ((d >= '0') && (d <= '7')) {
                            c = (c<<3) + (d-'0');
                            d = psc->pgetc(psc->file);
                            if ((d >= '0') && (d <= '7')) {
                                c = (c<<3) + (d-'0');
                            } else {
                                psc->unpgetc(d, psc->file);
                            }
                        } else {
                            psc->unpgetc(d, psc->file);
                        }
                        c &= 0xFF;
                    }
                } else if (c == '(') {
                    psc->paren++;
                }
                pscompact_buffer(psc, c);
                break;
            case PSC_InComment:
                /* Watch for an EOL, otherwise swallow */
                c = psc->pgetc(psc->file);
                if ((c == 13) || (c == 10)) {
                    if ((psc->inpos >= 3) &&
                        (strncmp((const char *)psc->bufferin, "END", 3) == 0)) {
                        /* Special comment to retain */
                        pscompact_bufferatstart(psc, '%');
                        pscompact_buffer(psc, 10);
                        pscompact_copyinout(psc);
                        break;
                    }
                    if ((psc->inpos >= 7) &&
                        (strncmp((const char *)psc->bufferin, "NAMESOK", 7) == 0)) {
                        psc->names = 1;
                        pscompact_bufferatstart(psc, '%');
                        pscompact_buffer(psc, 10);
                        pscompact_copyinout(psc);
                        break;
                    }
                    if ((psc->inpos >= 8) &&
                        (strncmp((const char *)psc->bufferin, "BINARYOK", 8) == 0)) {
                        psc->binary = 1;
                        pscompact_bufferatstart(psc, '%');
                        pscompact_buffer(psc, 10);
                        pscompact_copyinout(psc);
                        break;
                    }
                    /* Throw the buffered line away, and go back to buffering */
                    psc->inpos = 0;
                    psc->state = PSC_BufferIn;
                    break;
                }
                pscompact_buffer(psc, c);
                break;
            case PSC_InHexString:
                c = psc->pgetc(psc->file);
                if (c == '<') {
                    /* Dictionary */
                    pscompact_copy2(psc, '<', '<', 0);
                    break;
                } else if (c == '~') {
                    /* FIXME: ASCII85 encoded! */
                    fprintf(stderr, "ASCII85 encoded strings unsupported in pscompaction\n");
                    exit(1);
                } else if (c == '>') {
                    psc->wasascii = 0;
                    if (psc->binary) {
                        pscompact_hex2ascii(psc);
                        if (psc->inpos < 256) {
                            pscompact_bufferatstart(psc, psc->inpos);
                            pscompact_bufferatstart(psc, 142);
                            pscompact_copyinout_bin(psc);
                        } else if (psc->inpos < 65536) {
                            int count = psc->inpos;
                            pscompact_bufferatstart(psc, count>>8);
                            pscompact_bufferatstart(psc, count & 255);
                            pscompact_bufferatstart(psc, 144);
                            pscompact_copyinout_bin(psc);
                        } else {
                            fprintf(stderr, "HexString more than 64K in pscompaction\n");
                            exit(1);
                        }
                        break;
                    }
                    /* If all else fails, write it out as an ascii hexstring
                     * again */
                    pscompact_bufferatstart(psc, '<');
                    pscompact_buffer(psc, '>');
                    pscompact_copyinout(psc);
                    break;
                } else if (c <= 32) {
                    /* Swallow whitespace */
                    break;
                } else if (((c >= 'A') && (c <= 'Z')) ||
                           ((c >= 'a') && (c <= 'z')) ||
                           ((c >= '0') && (c <= '9'))) {
                    pscompact_buffer(psc, c);
                } else {
                    fprintf(stderr, "Unexpected char when parsing hexstring in pscompaction\n");
                    exit(1);
                }
                break;
        }
    } while ((out-ubuf != ulen) && (!psc->peof(psc->file)));
    return out-ubuf;
}

int cmpstringp(const void *p1, const void *p2)
{
   /* The actual arguments to this function are "pointers to
      pointers to char", but strcmp(3) arguments are "pointers
      to char", hence the following cast plus dereference */

   return strcmp(* (char * const *) p1, * (char * const *) p2);
}

/* This relies on the gp_enumerate_* which should not return directories, nor	*/
/* should it recurse into directories (unlike Adobe's implementation)		*/
/* paths are checked to see if they are an ordinary file or a path		*/
void process_path(char *path, const char *os_prefix, const char *rom_prefix,
                  Xlist_element *Xlist_head, int compression,
                  int compaction, int *inode_count, int *totlen, FILE *out,
                  split_data *splits, int verbose)
{
    int i, namelen, excluded, save_count=*inode_count;
    Xlist_element *Xlist_scan;
    char *prefixed_path;
    char *found_path, *rom_filename;
    file_enum *pfenum;
    int ret, block, blocks;
    romfs_inode *node;
    unsigned char *ubuf, *cbuf;
    unsigned long ulen, clen;
    FILE *in;
    unsigned long psc_len;
    pscompstate psc = { 0 };
    unsigned long numfiles = 0;
    char **foundfiles = NULL, *temp;

    prefixed_path = malloc(PATH_STR_LEN);
    found_path = malloc(PATH_STR_LEN);
    rom_filename = malloc(PATH_STR_LEN);
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
    pfenum = gp_enumerate_files_init((gs_memory_t *)&minimal_memory, prefixed_path,
                         strlen(prefixed_path));
    if (pfenum == NULL) {
        printf("gp_enumerate_files_init failed.\n");
        exit(1);
    }
    while ((namelen=gp_enumerate_files_next((gs_memory_t *)&minimal_memory, pfenum, found_path, 1024)) >= 0) {
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

        numfiles++;
        temp = realloc(foundfiles, sizeof(char *) * numfiles);
        if (temp == NULL) {
            free(cbuf);
            free(ubuf);
            free(found_path);
            free(foundfiles);
            free(prefixed_path);
            free(rom_filename);
            printf("realloc failed in process_path.\n");
            exit(1);
        }
        foundfiles = (char **)temp;
        foundfiles[numfiles - 1] = strdup(found_path);
    }

    qsort(foundfiles, numfiles, sizeof(char *), cmpstringp);

    for (i = 0; i < numfiles; i++) {
        char *fpath = foundfiles[i];

        /* process a file */
        node = calloc(1, sizeof(romfs_inode));
        /* get info for this file */
        in = fopen(fpath, "rb");
        if (in == NULL) {
            printf("unable to open file for processing: %s\n", fpath);
            continue;
        }
        /* printf("compacting %s\n", fpath); */
        /* rom_filename + strlen(rom_prefix) is first char after the new prefix we want to add */
        /* fpath + strlen(os_prefix) is the file name after the -P prefix */
        rom_filename[strlen(rom_prefix)] = 0;		/* truncate afater prefix */
        strcat(rom_filename, fpath + strlen(os_prefix));
        node->name = rom_filename;	/* without -P prefix, with -d rom_prefix */
        fseek(in, 0, SEEK_END);
        node->disc_length = node->length = ftell(in);
        blocks = (node->length+ROMFS_BLOCKSIZE-1) / ROMFS_BLOCKSIZE + 1;
        node->data_lengths = calloc(blocks, sizeof(*node->data_lengths));
        node->data = calloc(blocks, sizeof(*node->data));
        fclose(in);
        in = fopen(fpath, "rb");
        ulen = strlen(fpath);
        block = 0;
        psc_len = 0;
        if (compaction)
            pscompact_start(&psc, (psc_getc*)&fgetc, (psc_ungetc*)&ungetc, (psc_feof*)&feof, in, 1, 0, 0);
        while (!feof(in)) {
            if (compaction)
                ulen = pscompact_getcompactedblock(&psc, ubuf, ROMFS_BLOCKSIZE);
            else
                ulen = fread(ubuf, 1, ROMFS_BLOCKSIZE, in);
            psc_len += ulen;
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
        if (compaction) {
            /* printf("%s: Compaction saved %d bytes (before compression)\n",
             *        fpath, node->length - psc_len); */
            pscompact_end(&psc);
            node->length = psc_len;
        }

        /* write out data for this file */
        inode_write(out, node, compression, *inode_count, totlen, splits, verbose);
        /* clean up */
        inode_clear(node);
        free(node);
        (*inode_count)++;
        free(fpath);
    }
    free(cbuf);
    free(ubuf);
    free(found_path);
    free(foundfiles);
    free(prefixed_path);
    free(rom_filename);
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
void merge_to_ps(const char *os_prefix, const char *inname, FILE * in, FILE * config, int verbose);
int write_init(char *);
bool rl(FILE * in, char *str, int len);
void wsc(const byte *str, int len);
void ws(const byte *str, int len);
void wl(const char *str);
char *doit(char *line, bool intact);
void hex_string_to_binary(FILE *in);
void flush_buf(char *buf);
void mergefile(const char *os_prefix, const char *inname, FILE * in, FILE * config,
               bool intact, int verbose);
void flush_line_buf(int len);

typedef struct in_block_s in_block_t;
struct in_block_s {
    struct in_block_s *next;
    unsigned char data[ROMFS_BLOCKSIZE];
};

#define LINE_SIZE 1024

/* Globals used for gs_init processing */
char linebuf[LINE_SIZE * 2];		/* make it plenty long to avoid overflow */
in_block_t *in_block_head = NULL;
in_block_t *in_block_tail = NULL;
unsigned char *curr_block_p, *curr_block_end;

typedef struct {
    in_block_t *block;
    int pos;
    int eof;
} in_block_file;

static int ib_getc(in_block_file *ibf) {
    if ((ibf->block == in_block_tail) &&
        (ibf->pos == curr_block_p - in_block_tail->data)) {
        ibf->eof = 1;
        return -1;
    }
    if (ibf->pos == ROMFS_BLOCKSIZE) {
        ibf->block = ibf->block->next;
        ibf->pos = 0;
    }
    return ibf->block->data[ibf->pos++];
}

static void ib_ungetc(int c, in_block_file *ibf)
{
    ibf->pos--;
}

static int ib_feof(in_block_file *ibf)
{
    return ibf->eof;
}

static int
process_initfile(char *initfile, char *gconfig_h, const char *os_prefix,
                 const char *rom_prefix, int compression, int *inode_count,
                 int *totlen, FILE *out, split_data *splits, int verbose)
{
    int ret, block, blocks;
    romfs_inode *node = NULL;
    unsigned char *ubuf = NULL, *cbuf = NULL;
    char *prefixed_path = NULL, *rom_filename = NULL;
    unsigned long clen;
    FILE *in;
    FILE *config;
    in_block_t *in_block = NULL;
    int compaction = 1;
    int code = 0;

    ubuf = malloc(ROMFS_BLOCKSIZE);
    cbuf = malloc(ROMFS_CBUFSIZE);
    prefixed_path = malloc(1024);
    rom_filename = malloc(1024);
    if (ubuf == NULL || cbuf == NULL || prefixed_path == NULL ||
        rom_filename == NULL) {
        printf("malloc fail in process_initfile\n");
        /* should free whichever buffers got allocated, but don't bother */
        code = -1;
        goto done;
    }

    prefix_add(os_prefix, initfile, prefixed_path);
    prefix_add(rom_prefix, initfile, rom_filename);

    in = fopen(prefixed_path, "r");
    if (in == 0) {
        printf("cannot open initfile at: %s\n", prefixed_path);
        code = -1;
        goto done;
    }
    config = fopen(gconfig_h, "r");
    if (config == 0) {
        printf("Cannot open gconfig file %s\n", gconfig_h);
        fclose(in);
        code = -1;
        goto done;
    }
    memset(linebuf, 0, sizeof(linebuf));
    node = calloc(1, sizeof(romfs_inode));
    node->name = rom_filename;	/* without -P prefix, with -d rom_prefix */

    merge_to_ps(os_prefix, initfile, in, config, verbose);

    fclose(in);
    fclose(config);

    if (compaction)
    {
        in_block_t *comp_block_head;
        in_block_t *comp_block;
        pscompstate psc = {0};
        in_block_file ibf;
        int ulen;

        ibf.block = in_block_head;
        ibf.pos = 0;
        ibf.eof = 0;

        comp_block = malloc(sizeof(*comp_block));
        comp_block_head = comp_block;
        pscompact_start(&psc, (psc_getc*)&ib_getc, (psc_ungetc*)&ib_ungetc, (psc_feof*)&ib_feof, &ibf, 0, 0, 1);
        do {
            ulen = pscompact_getcompactedblock(&psc, comp_block->data, ROMFS_BLOCKSIZE);
            comp_block->next = NULL;
            if (ulen == ROMFS_BLOCKSIZE) {
                comp_block->next = malloc(sizeof(*comp_block));
                comp_block = comp_block->next;
            }
        } while (ulen == ROMFS_BLOCKSIZE);
        pscompact_end(&psc);
        while (in_block_head != NULL) {
            in_block = in_block_head->next;
            free(in_block_head);
            in_block_head = in_block;
        }
        in_block_head = comp_block_head;
        in_block_tail = comp_block;
        curr_block_p = in_block_tail->data + ulen;
    }

    node->length = 0;
    in_block = in_block_head;
    while (in_block != NULL) {
        node->length +=
            in_block != in_block_tail ? ROMFS_BLOCKSIZE : curr_block_p - in_block->data;
        in_block = in_block->next;
    }
    node->disc_length = node->length;

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
                code = -1;
                goto done;
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
    inode_write(out, node, compression, *inode_count, totlen, splits, verbose);
    /* clean up */
    inode_clear(node);
    (*inode_count)++;
done:
    free(node);
    free(cbuf);
    free(ubuf);
    free(prefixed_path);
    free(rom_filename);
    return code;
}

void
flush_line_buf(int len) {
    int remaining_len = len;
    int move_len;
    int line_offset = 0;

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
        memcpy(curr_block_p, linebuf + line_offset, move_len);
        curr_block_p += move_len;
        line_offset += move_len;
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
prefix_open(const char *os_prefix, const char *filename, int verbose)
{
    char *prefixed_path;
    FILE *filep;

    prefixed_path = malloc(1024);
    if (prefixed_path == NULL) {
        printf("malloc problem in prefix_open\n");
        return NULL;
    }
    prefix_add(os_prefix, filename, prefixed_path);
    if (verbose) {
        printf("including: '%s'\n", prefixed_path);
    }
    filep = fopen(prefixed_path, "rb");
    free(prefixed_path);
    return filep;
}

void
mergefile(const char *os_prefix, const char *inname, FILE * in, FILE * config,
          bool intact, int verbose)
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
                ps = prefix_open(os_prefix, psname + 1, verbose);
                if (ps == 0) {
                    fprintf(stderr, "Failed to open '%s' - aborting\n", psname+1);
                    exit(1);
                }
                mergefile(os_prefix, psname + 1, ps, config, intact || do_intact, verbose);
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
                        if (quote == NULL)
                            exit(1);

                        *quote = 0;
                        ps = prefix_open(os_prefix, psname + 9, verbose);
                        if (ps == 0)
                            exit(1);
                        mergefile(os_prefix, psname + 9, ps, config, false, verbose);
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
merge_to_ps(const char *os_prefix, const char *inname, FILE * in, FILE * config, int verbose)
{
    char line[LINE_SIZE + 1];

    while ((rl(in, line, LINE_SIZE), line[0])) {
        sprintf(linebuf, "%s", line );
        wl(linebuf);
    }
    mergefile(os_prefix, inname, in, config, false, verbose);
}

static void
make_split_name(split_data *splits, const char *filename)
{
  const char *s = filename;
  const char *t = NULL;
  char *u;

  while (*s) {
    if (*s == '.')
      t = s;
    s++;
  }
  if (t == NULL)
    t = s;

  free(splits->outname);
  splits->outname = u = malloc(s-filename+4);
  if (u == NULL) {
    fprintf(stderr, "malloc failure while constructing split filename\n");
    exit(1);
  }
  memcpy(u, filename, t-filename);
  u[t-filename] = 'c';
  u[t-filename+1] = '%';
  u[t-filename+2] = 'd';
  if (s-t)
     memcpy(u+(t-filename)+3, t, s-t);
  u[s-filename+3] = 0;

  free(splits->outname_formatted);
  splits->outname_formatted = malloc(s-filename+4+32);
  if (splits->outname_formatted == NULL) {
    fprintf(stderr, "malloc failure while constructing split filename\n");
    exit(1);
  }
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
    int atarg = 1;
    int compression = 1;			/* default to doing compression */
    int compaction = 0;
    int verbose = 1;
    Xlist_element *Xlist_scan = NULL, *Xlist_head = NULL;
    char pa[PATH_STR_LEN];
    time_t buildtime = 0;
    char* env_source_date_epoch;
    split_data splits = { 0 } ;

    memset(pa, 0x00, PATH_STR_LEN);

    if (argc < 2) {
        printf("\n"
                "       Usage: mkromfs [-o outputfile] [options ...] paths\n"
                "           options and paths can be interspersed and are processed in order\n"
                "           options:\n"
                "               -o outputfile   default: obj/gsromfs.c if this option present, must be first.\n"
                "               -P prefix       use prefix to find path. prefix not included in %%rom%%\n"
                "               -q              reduce diagnostics\n"
                "               -X path         exclude the path from further processing.\n"
                "                         Note: The tail of any path encountered will be tested so .svn on the -X\n"
                "                               list will exclude that path in all subsequent paths enumerated.\n"
                "\n"
                "               -d romprefix    directory in %%rom file system (just a prefix string on filename)\n"
                "               -c              compression on\n"
                "               -b              compression off (binary).\n"
                "               -C              postscript 'compaction' on\n"
                "               -B              postscript 'compaction' off\n"
                "               -g initfile gconfig_h \n"
                "                       special handling to read the 'gs_init.ps' file (from\n"
                "                       the current -P prefix path), and read the gconfig.h for\n"
                "                       psfile_ entries and combines them into a whitespace\n"
                "                       optimized and no comments form and writes this 'gs_init.ps'\n"
                "                       to the current -d destination path. This is a space and\n"
                "                       startup performance improvement, so also this should be\n"
                "                       BEFORE other stuff in the %%rom%% list of files (currently\n"
                "                       we do a sequential search in the %%rom%% directory).\n"
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

    start_file(out);

    if ((env_source_date_epoch = getenv("SOURCE_DATE_EPOCH"))) {
        buildtime = strtoul(env_source_date_epoch, NULL, 10);
    }
    if (!buildtime)
        buildtime = time(NULL);
    fprintf(out,"    time_t gs_romfs_buildtime = %ld;\n\n", (long)buildtime);

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
              case 'B':
                compaction = 0;
                break;
              case 'C':
                compaction = 1;
                break;
              case 'q':
                verbose = 0;
                break;
              case 'd':
                if (++atarg == argc) {
                    printf("   option %s missing required argument\n", argv[atarg-1]);
                    exit(1);
                }
                rom_prefix = argv[atarg];
                break;
              case 's':
                if (++atarg == argc) {
                    printf("   option %s missing required argument\n", argv[atarg-1]);
                    exit(1);
                }
                splits.num_splits = atoi(argv[atarg]);
                if (splits.num_splits <= 0) {
                    printf("   Invalid number of files to split to: %s\n", argv[atarg]);
                    exit(1);
                }
                make_split_name(&splits, outfilename);
                break;
              case 'g':
                {
                    char initfile[PATH_STR_LEN] = {0};
                    char gconfig_h[PATH_STR_LEN] = {0};
                    if ((++atarg) + 1 == argc) {
                        printf("   option %s missing required arguments\n", argv[atarg-1]);
                        exit(1);
                    }
                    strncpy(initfile, argv[atarg], PATH_STR_LEN - 1);
                    atarg++;
                    strncpy(gconfig_h, argv[atarg], PATH_STR_LEN - 1);
                    process_initfile(initfile, gconfig_h, os_prefix, rom_prefix, compression,
                                    &inode_count, &totlen, out, &splits, verbose);
                }
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
        strncpy(pa, argv[atarg], PATH_STR_LEN - (strlen(os_prefix) < strlen(rom_prefix) ? strlen(rom_prefix) : strlen(os_prefix)));
        process_path(pa, os_prefix, rom_prefix, Xlist_head,
                     compression, compaction, &inode_count, &totlen, out, &splits, verbose);
    }

    /* Now allow for the (probably never happening) case where we are splitting, but haven't written anything to one of the files */
    prepare_splits(&splits);
    for (i = 0; i < splits.max_splits; i++) {
        if (splits.sizes[i] == 0) {
            FILE *out2;
            sprintf(splits.outname_formatted, splits.outname, i);
            out2 = fopen(splits.outname_formatted, "w");
            fprintf(out2, "const int mkromfs_dummy_chunk%d;\n", i);
            fclose(out2);
        }
    }

    if (splits.max_splits) {
        for (i=0; i<inode_count; i++)
            fprintf(out, "\t extern const uint32_t mkromfs_node_%d[];\n", i);
    }

    /* now write out the array of nodes */
    fprintf(out, "    const uint32_t *gs_romfs[] = {\n");
    for (i=0; i<inode_count; i++)
        fprintf(out, "\t%snode_%d,\n", splits.max_splits ? "mkromfs_" : "", i);
    fprintf(out, "\t0 };\n");
    fclose(out);
    while (Xlist_head) {
        Xlist_scan = Xlist_head->next;
        free(Xlist_head);
        Xlist_head = Xlist_scan;
    }
    printf("Total %%rom%% structure size is %d bytes.\n", totlen);
    if (splits.outname != NULL)
       free(splits.outname);
    if (splits.outname_formatted != NULL)
        free(splits.outname_formatted);
    if (splits.sizes != NULL)
        free(splits.sizes);
    return 0;
}
