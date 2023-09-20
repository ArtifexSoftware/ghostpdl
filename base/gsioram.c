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

/* %ram% file device implementation */

/*
 *  This file implements a simple ram-based file system.
 *
 * The fs has no subdirs, only a root directory. Files are stored as a
 * resizable array of pointers to blocks.  Timestamps are not implemented
 * for performance reasons.
 *
 * The implementation is in two parts - a ramfs interface that works
 * mostly like the unix file system calls, and a layer to hook this up
 * to ghostscript.
 *
 * Macros define an upper limit on the number of blocks a ram device
 * can take up, and (in ramfs.c) the size of each block.
 *
 * Routines for the gs stream interface were graciously stolen from
 * sfxstdio.c et al.
 */

#include "string_.h"
#include "unistd_.h"
#include "gx.h"
#include "gserrors.h"
#include "gp.h"
#include "gscdefs.h"
#include "gsparam.h"
#include "gsstruct.h"
#include "gxiodev.h"
#include "gsutil.h"
#include "stream.h"
#include "ramfs.h"

/* Function prototypes */
static iodev_proc_init(iodev_ram_init);
static iodev_proc_finit(iodev_ram_finit);
static iodev_proc_open_file(ram_open_file);
static iodev_proc_delete_file(ram_delete);
static iodev_proc_rename_file(ram_rename);
static iodev_proc_file_status(ram_status);
static iodev_proc_enumerate_files(ram_enumerate_init);
static iodev_proc_enumerate_next(ram_enumerate_next);
static iodev_proc_enumerate_close(ram_enumerate_close);
static iodev_proc_get_params(ram_get_params);
static void ram_finalize(const gs_memory_t *memory, void * vptr);

const gx_io_device gs_iodev_ram = {
    "%ram%", "FileSystem", {
        iodev_ram_init, iodev_ram_finit, iodev_no_open_device,
        ram_open_file, iodev_no_fopen, iodev_no_fclose,
        ram_delete, ram_rename, ram_status,
        ram_enumerate_init, ram_enumerate_next, ram_enumerate_close,
        ram_get_params, iodev_no_put_params
    },
    NULL,
    NULL
};

typedef struct ramfs_state_s {
    gs_memory_t *memory;
    ramfs* fs;
} ramfs_state;

#define GETRAMFS(state) (((ramfs_state*)(state))->fs)

gs_private_st_simple_final(st_ramfs_state, struct ramfs_state_s, "ramfs_state", ram_finalize);

typedef struct gsram_enum_s {
    char *pattern;
    ramfs_enum* e;
    gs_memory_t *memory;
} gsram_enum;

gs_private_st_ptrs3(st_gsram_enum, struct gsram_enum_s, "gsram_enum",
    gsram_enum_enum_ptrs, gsram_enum_reloc_ptrs, pattern, e, memory);

/* could make this runtime configurable later.  It doesn't allocate
   all the blocks in one go so it's not critical */
#define MAXBLOCKS 2000000

#define DEFAULT_BUFFER_SIZE 2048

/* stream stuff */

static int
s_ram_available(stream *, gs_offset_t *),
 s_ram_read_seek(stream *, gs_offset_t),
 s_ram_read_close(stream *),
 s_ram_read_process(stream_state *, stream_cursor_read *,
     stream_cursor_write *, bool);
static int
s_ram_write_seek(stream *, gs_offset_t),
 s_ram_write_flush(stream *),
 s_ram_write_close(stream *),
 s_ram_write_process(stream_state *, stream_cursor_read *,
     stream_cursor_write *, bool);
static int
s_ram_switch(stream *, bool);

static int
ramfs_errno_to_code(int error_number) {
    switch (error_number) {
    case RAMFS_NOTFOUND:
        return_error(gs_error_undefinedfilename);
    case RAMFS_NOACCESS:
        return_error(gs_error_invalidfileaccess);
    case RAMFS_NOMEM:
        return_error(gs_error_VMerror);
    case RAMFS_BADRANGE:
        return_error(gs_error_rangecheck);
    /* just in case */
    default:
        return_error(gs_error_ioerror);
    }
}

static void
sread_ram(register stream * s, ramhandle * file, byte * buf, uint len),
 swrite_ram(register stream * s, ramhandle * file, byte * buf, uint len),
 sappend_ram(register stream * s, ramhandle * file, byte * buf, uint len);

static int
ram_open_file(gx_io_device * iodev, const char *fname, uint len,
    const char *file_access, stream ** ps, gs_memory_t * mem)
{
    int code = 0;
    ramhandle * file;
    char fmode[4];  /* r/w/a, [+], [b], null */
    int openmode=RAMFS_READ;
    ramfs * fs;
    char * namestr = NULL;

    /* Is there a more efficient way to do this? */
    namestr = (char *)gs_alloc_bytes(mem, len + 1, "temporary filename string");
    if(!namestr)
        return_error(gs_error_VMerror);
    strncpy(namestr,fname,len);
    namestr[len] = 0;

    /* Make sure iodev is valid, and we have initialised the RAM file system (state != NULL) */
    if (iodev == NULL || iodev->state == NULL) {/*iodev = iodev_default;*/
        gs_free_object(mem, namestr, "free temporary filename string");
        return gs_note_error(gs_error_invalidaccess);
    }
    fs = GETRAMFS(iodev->state);
    code = file_prepare_stream(fname, len, file_access, DEFAULT_BUFFER_SIZE,
    ps, fmode, mem
    );
    if (code < 0) goto error;
    if (fname == 0) {
        gs_free_object(mem, namestr, "free temporary filename string");
        return 0;
    }

    switch (fmode[0]) {
        case 'a':
          openmode = RAMFS_WRITE | RAMFS_APPEND;
          break;
        case 'r':
          openmode = RAMFS_READ;
          if (fmode[1] == '+')
            openmode |= RAMFS_WRITE;
          break;
        case 'w':
          openmode |= RAMFS_WRITE | RAMFS_TRUNC | RAMFS_CREATE;
          if (fmode[1] == '+')
             openmode |= RAMFS_READ;
    }

    /* For now, we cheat here in the same way that sfxstdio.c et al cheat -
       append mode is faked by opening in write mode and seeking to EOF just
       once. This is different from unix semantics, which seeks atomically
       before each write, and is actually useful as a distinct mode. */
    /* if (fmode[0] == 'a') openmode |= RAMFS_APPEND; */

    file = ramfs_open(mem, fs,namestr,openmode);
    if(!file) { code = ramfs_errno_to_code(ramfs_error(fs)); goto error; }

    switch (fmode[0]) {
    case 'a':
    sappend_ram(*ps, file, (*ps)->cbuf, (*ps)->bsize);
    break;
    case 'r':
    sread_ram(*ps, file, (*ps)->cbuf, (*ps)->bsize);
    break;
    case 'w':
    swrite_ram(*ps, file, (*ps)->cbuf, (*ps)->bsize);
    }
    if (fmode[1] == '+') {
      (*ps)->modes = (*ps)->file_modes |= s_mode_read | s_mode_write;
    }
    (*ps)->save_close = (*ps)->procs.close;
    (*ps)->procs.close = file_close_file;
 error:
    gs_free_object(mem, namestr, "free temporary filename string");
    /* XXX free stream stuff? */
    return code;
}

/* Initialize a stream for reading an OS file. */
static void
sread_ram(register stream * s, ramhandle * file, byte * buf, uint len)
{
    static const stream_procs p = {
    s_ram_available, s_ram_read_seek, s_std_read_reset,
    s_std_read_flush, s_ram_read_close, s_ram_read_process,
    s_ram_switch
    };

    s_std_init(s, buf, len, &p,s_mode_read + s_mode_seek);
    s->file = (gp_file *)file;
    s->file_modes = s->modes;
    s->file_offset = 0;
    ramfile_seek(file, 0, RAMFS_SEEK_END);
    s->file_limit = ramfile_tell(file);
    ramfile_seek(file, 0, RAMFS_SEEK_SET);
}

/* Procedures for reading from a file */
static int
s_ram_available(register stream * s, gs_offset_t *pl)
{
    long max_avail = s->file_limit - stell(s);

    *pl = max_avail;
    if(*pl == 0 && ramfile_eof((ramhandle*)s->file))
    *pl = -1;        /* EOF */
    return 0;
}

static int
s_ram_read_seek(register stream * s, gs_offset_t pos)
{
    uint end = s->cursor.r.limit - s->cbuf + 1;
    long offset = pos - s->position;

    if (offset >= 0 && offset <= end) {  /* Staying within the same buffer */
        s->cursor.r.ptr = s->cbuf + offset - 1;
        return 0;
    }

    if (pos < 0 || pos > s->file_limit || s->file == NULL ||
        ramfile_seek((ramhandle*)s->file, s->file_offset + pos, RAMFS_SEEK_SET) != 0)
        return ERRC;

    s->cursor.r.ptr = s->cursor.r.limit = s->cbuf - 1;
    s->end_status = 0;
    s->position = pos;
    return 0;
}
static int
s_ram_read_close(stream * s)
{
    ramhandle *file = (ramhandle*)s->file;

    if (file != 0) {
    s->file = 0;
    ramfile_close(file);
    }
    return 0;
}

/*
 * Process a buffer for a file reading stream.
 * This is the first stream in the pipeline, so pr is irrelevant.
 */
static int
s_ram_read_process(stream_state * st, stream_cursor_read * ignore_pr,
    stream_cursor_write * pw, bool last)
{
    stream *s = (stream *)st;    /* no separate state */
    ramhandle *file = (ramhandle*)s->file;
    uint max_count = pw->limit - pw->ptr;
    int status = 1;
    int count;

    if (s->file_limit < S_FILE_LIMIT_MAX) {
    long limit_count = s->file_offset + s->file_limit -
    ramfile_tell(file);

    if (max_count > limit_count)
        max_count = limit_count, status = EOFC;
    }
    count = ramfile_read(file,pw->ptr + 1, max_count);
    if (count < 0) return ERRC;
    pw->ptr += count;
    /*    process_interrupts(s->memory); */
    return ramfile_eof(file) ? EOFC : status;
}

/* ------ File writing ------ */

/* Initialize a stream for writing a file. */
static void
swrite_ram(register stream * s, ramhandle * file, byte * buf, uint len)
{
    static const stream_procs p = {
    s_std_noavailable, s_ram_write_seek, s_std_write_reset,
    s_ram_write_flush, s_ram_write_close, s_ram_write_process,
    s_ram_switch
    };

    s_std_init(s, buf, len, &p, s_mode_write + s_mode_seek);
    s->file = (gp_file *)file;
    s->file_modes = s->modes;
    s->file_offset = 0;        /* in case we switch to reading later */
    s->file_limit = S_FILE_LIMIT_MAX;
}

/* Initialize for appending to a file. */
static void
sappend_ram(register stream * s, ramhandle * file, byte * buf, uint len)
{
    swrite_ram(s, file, buf, len);
    s->modes = s_mode_write + s_mode_append;    /* no seek */
    s->file_modes = s->modes;
    ramfile_seek(file,0,RAMFS_SEEK_END);
    s->position = ramfile_tell(file);
}

/* Procedures for writing on a file */
static int
s_ram_write_seek(stream * s, gs_offset_t pos)
{
    /* We must flush the buffer to reposition. */
    int code = sflush(s);

    if (code < 0) return code;
    if (pos < 0 || ramfile_seek((ramhandle*)s->file, pos, RAMFS_SEEK_SET) != 0)
        return ERRC;
    s->position = pos;
    return 0;
}

static int
s_ram_write_flush(register stream * s)
{
    int result = s_process_write_buf(s, false);
    return result;
}

static int
s_ram_write_close(register stream * s)
{
    s_process_write_buf(s, true);
    return s_ram_read_close(s);
}

/*
 * Process a buffer for a file writing stream.
 * This is the last stream in the pipeline, so pw is irrelevant.
 */
static int
s_ram_write_process(stream_state * st, stream_cursor_read * pr,
    stream_cursor_write * ignore_pw, bool last)
{
    uint count = pr->limit - pr->ptr;

    ramhandle *file = (ramhandle*)((stream *) st)->file;
    int written = ramfile_write(file,pr->ptr + 1, count);

    if (written < 0) return ERRC;
    pr->ptr += written;
    return 0;
}

static int
s_ram_switch(stream * s, bool writing)
{
    uint modes = s->file_modes;
    ramhandle *file = (ramhandle*)s->file;
    long pos;

    if (writing) {
    if (!(s->file_modes & s_mode_write)) return ERRC;
    pos = stell(s);
    ramfile_seek(file, pos, RAMFS_SEEK_SET);
    if (modes & s_mode_append) {
        sappend_ram(s, file, s->cbuf, s->cbsize);    /* sets position */
    } else {
        swrite_ram(s, file, s->cbuf, s->cbsize);
        s->position = pos;
    }
    s->modes = modes;
    } else {
    if (!(s->file_modes & s_mode_read)) return ERRC;
    pos = stell(s);
    if (sflush(s) < 0) return ERRC;
    sread_ram(s, file, s->cbuf, s->cbsize);
    s->modes |= modes & s_mode_append;    /* don't lose append info */
    s->position = pos;
    }
    s->file_modes = modes;
    return 0;
}


/* gx_io_device stuff */

static int
iodev_ram_init(gx_io_device * iodev, gs_memory_t * mem)
{
    ramfs* fs = ramfs_new(mem, MAXBLOCKS);
    ramfs_state* state = gs_alloc_struct(mem, ramfs_state, &st_ramfs_state,
    "ramfs_init(state)"
    );
    if (fs && state) {
    state->fs = fs;
    state->memory = mem;
    iodev->state = state;
    return 0;
    }
    if(fs) ramfs_destroy(mem, fs);
    if(state) gs_free_object(mem,state,"iodev_ram_init(state)");
    return_error(gs_error_VMerror);
}

static void
iodev_ram_finit(gx_io_device * iodev, gs_memory_t * mem)
{
    ramfs_state *state = (ramfs_state *)iodev->state;
    if (state != NULL)
    {
        iodev->state = NULL;
        gs_free_object(state->memory, state, "iodev_ram_finit");
    }
    return;
}

static void
ram_finalize(const gs_memory_t *memory, void * vptr)
{
    ramfs* fs = GETRAMFS((ramfs_state*)vptr);
    ramfs_destroy((gs_memory_t *)memory, fs);
    GETRAMFS((ramfs_state*)vptr) = NULL;
}

static int
ram_delete(gx_io_device * iodev, const char *fname)
{
    ramfs* fs;

    if (iodev->state == NULL)
        return_error(gs_error_ioerror);
    else
        fs = GETRAMFS(iodev->state);

    if(ramfs_unlink(fs,fname)!=0) {
    return_error(ramfs_errno_to_code(ramfs_error(fs)));
    }
    return 0;
}

static int
ram_rename(gx_io_device * iodev, const char *from, const char *to)
{
    ramfs* fs;

    if (iodev->state == NULL)
        return_error(gs_error_ioerror);
    else
        fs = GETRAMFS(iodev->state);

    if(ramfs_rename(fs,from,to)!=0) {
    return_error(ramfs_errno_to_code(ramfs_error(fs)));
    }
    return 0;
}

static int
ram_status(gx_io_device * iodev, const char *fname, struct stat *pstat)
{
    ramhandle * f;
    ramfs* fs;

    if (iodev->state == NULL)
        return_error(gs_error_ioerror);
    else
        fs = GETRAMFS(iodev->state);

    f = ramfs_open(((ramfs_state *)iodev->state)->memory, fs,fname,RAMFS_READ);
    if(!f) return_error(ramfs_errno_to_code(ramfs_error(fs)));

    memset(pstat, 0, sizeof(*pstat));
    pstat->st_size = ramfile_size(f);
    /* The Windows definition of struct stat doesn't include a st_blocks member
    pstat->st_blocks = (pstat->st_size+blocksize-1)/blocksize;*/
    /* XXX set mtime & ctime */
    ramfile_close(f);
    return 0;
}

static file_enum *
ram_enumerate_init(gs_memory_t * mem, gx_io_device *iodev, const char *pat,
                   uint patlen)
{
    gsram_enum * penum = gs_alloc_struct(
    mem, gsram_enum, &st_gsram_enum,
    "ram_enumerate_files_init(file_enum)"
    );
    char *pattern = (char *)gs_alloc_bytes(
    mem, patlen+1, "ram_enumerate_file_init(pattern)"
    );

    ramfs_enum * e;

    if (iodev->state == NULL)
        return NULL;
    else
        e = ramfs_enum_new(GETRAMFS(iodev->state));

    if(penum && pattern && e) {
    memcpy(pattern, pat, patlen);
    pattern[patlen]=0;

    penum->memory = mem;
    penum->pattern = pattern;
    penum->e = e;
    return (file_enum *)penum;
    }
    if (penum) gs_free_object(mem,penum,"ramfs_enum_init(ramfs_enum)");
    if (pattern)
    gs_free_object(mem, pattern, "ramfs_enum_init(pattern)");
    if(e) ramfs_enum_end(e);
    return NULL;
}

static void
ram_enumerate_close(gs_memory_t * mem, file_enum *pfen)
{
    gsram_enum *penum = (gsram_enum *)pfen;
    gs_memory_t *mem2 = penum->memory;
    (void)mem;

    ramfs_enum_end(penum->e);
    gs_free_object(mem2, penum->pattern, "ramfs_enum_init(pattern)");
    gs_free_object(mem2, penum, "ramfs_enum_init(ramfs_enum)");
}

static uint
ram_enumerate_next(gs_memory_t * mem, file_enum *pfen, char *ptr, uint maxlen)
{
    gsram_enum *penum = (gsram_enum *)pfen;

    char * filename;
    while ((filename = ramfs_enum_next(penum->e))) {
    if (string_match((byte *)filename, strlen(filename),
        (byte *)penum->pattern,
        strlen(penum->pattern), 0)) {
        if (strlen(filename) < maxlen)
        memcpy(ptr, filename, strlen(filename));
        return strlen(filename);    /* if > maxlen, caller will detect rangecheck */
    }
    }
    /* ran off end of list, close the enum */
    ram_enumerate_close(mem, pfen);
    return ~(uint)0;
}

static int
ram_get_params(gx_io_device * iodev, gs_param_list * plist)
{
    int code;
    int i0 = 0, so = 1;
    bool btrue = true, bfalse = false;
    ramfs* fs;

    int BlockSize;
    long Free, LogicalSize;

    if (iodev->state == NULL)
        return_error(gs_error_ioerror);
    else
        fs = GETRAMFS(iodev->state);

    BlockSize = ramfs_blocksize(fs);
    LogicalSize = MAXBLOCKS;
    Free = ramfs_blocksfree(fs);

    if (
    (code = param_write_bool(plist, "HasNames",        &btrue)) < 0 ||
    (code = param_write_int (plist, "BlockSize",       &BlockSize)) < 0 ||
    (code = param_write_long(plist, "Free",            &Free)) < 0 ||
    (code = param_write_int (plist, "InitializeAction",&i0)) < 0 ||
    (code = param_write_bool(plist, "Mounted",         &btrue)) < 0 ||
    (code = param_write_bool(plist, "Removable",       &bfalse)) < 0 ||
    (code = param_write_bool(plist, "Searchable",      &btrue)) < 0 ||
    (code = param_write_int (plist, "SearchOrder",     &so)) < 0 ||
    (code = param_write_bool(plist, "Writeable",       &btrue)) < 0 ||
    (code = param_write_long(plist, "LogicalSize",     &LogicalSize)) < 0
    )
    return code;
    return 0;
}
