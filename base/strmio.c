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


/* Interface for streams that mimic stdio functions (fopen, fread, fseek, ftell) */

#include "malloc_.h"
#include "memory_.h"
#include "gxiodev.h"
#include "gdebug.h"
#include "gsfname.h"
#include "gslibctx.h"
#include "gsmemret.h" /* for gs_memory_type_ptr_t */
#include "gsmalloc.h"
#include "gsstype.h"
#include "stream.h"
#include "strmio.h"
#include "gserrors.h"

/*
 * Open a stream using a filename that can include a PS style IODevice prefix
 * If iodev_default is the '%os' device, then the file will be on the host
 * file system transparently to the caller. The "%os%" prefix can be used
 * to explicilty access the host file system.
 */
stream *
sfopen(const char *path, const char *mode, gs_memory_t *mem)
{
    gs_parsed_file_name_t pfn;
    stream *s;
    iodev_proc_open_file((*open_file));

    int code = gs_parse_file_name(&pfn, path, strlen(path), mem);
    if (code < 0) {
#       define EMSG     "sfopen: gs_parse_file_name failed.\n"
        errwrite(mem, EMSG, strlen(EMSG));
#       undef EMSG
        return NULL;
    }
    if (pfn.fname == NULL) {    /* just a device */
#       define EMSG     "sfopen: not allowed with %device only.\n"
        errwrite(mem, EMSG, strlen(EMSG));
#       undef EMSG
        return NULL;
    }
    if (pfn.iodev == NULL)
        pfn.iodev = iodev_default(mem);
    open_file = pfn.iodev->procs.open_file;
    if (open_file == 0)
        code = file_open_stream(pfn.fname, pfn.len, mode, 2048, &s,
                                pfn.iodev, pfn.iodev->procs.gp_fopen, mem);
    else
        code = open_file(pfn.iodev, pfn.fname, pfn.len, mode, &s, mem);
    if (code < 0)
        return NULL;
    s->position = 0;
    code = ssetfilename(s, (const byte *)path, strlen(path));
    if (code < 0) {
        /* Only error is gs_error_VMerror */
        sclose(s);
        gs_free_object(s->memory, s, "sfopen: allocation error");
#       define EMSG     "sfopen: allocation error setting path name into stream.\n"
        errwrite(mem, EMSG, strlen(EMSG));
#       undef EMSG
        return NULL;
    }
    return s;
}

/*
 * Read a number of bytes from a stream, returning number read. Return count
 * will be less than count if EOF or error. Return count is number of elements.
 */
int
sfread(void *ptr, size_t size, size_t count, stream *s)
{
    int code;
    uint nread;

    code = sgets(s, ptr, size*count, &nread);
    if (code < 0)
        return code;
    return nread*size;
}

/*
 * Read a byte from a stream
 */
int
sfgetc(stream *s)
{
    int code = sgetc(s);

    return code >= 0 ? code : EOF;
}

/*
 * Seek to a position in the stream. Returns the 0, or -1 if error
 */
int
sfseek(stream *s, gs_offset_t offset, int whence)
{
    gs_offset_t newpos = offset;

    if (whence == SEEK_CUR)
        newpos += stell(s);
    if (whence == SEEK_END) {
        gs_offset_t endpos;

        if (savailable(s, &endpos) < 0)
            return -1;
        newpos = endpos - offset;
    }
    if (s_can_seek(s) || newpos == stell(s)) {
        return sseek(s, newpos);
    }
    return -1;          /* fail */
}

/*
 * Position to the beginning of the file
 */
int
srewind(stream *s)
{
    return sfseek(s, 0, SEEK_SET);
}

/*
 * Return the current position in the stream or -1 if error.
 */
long
sftell(stream *s)
{
    return stell(s);
}

/*
 * Return the EOF status, or 0 if not at EOF.
 */
int
sfeof(stream *s)
{
    return (s->end_status == EOFC) ? -1 : 0;
}

/*
 * Return the error status, or 0 if no error
 */
int
sferror(stream *s)
{
    return (s->end_status == ERRC) ? -1 : 0;
}

int
sfclose(stream *s)
{
    /* no need to flush since these are 'read' only */
    gs_memory_t *mem;

    if (s == NULL)
        return 0;
    mem = s->memory;
    sclose(s);
    gs_free_object(mem, s, "sfclose(stream)");
    return 0;
}

/* And now, a special version of the stdin iodev that can
 * read via the gsapi callout. This is lifted and modified
 * from gsiodev.c. Maybe at some point in the future we
 * can unify the two. */

#define STDIN_BUF_SIZE 1024
/* Read from stdin into the buffer. */
/* If interactive, only read one character. */
static int
s_stdin_read_process(stream_state * st, stream_cursor_read * ignore_pr,
                     stream_cursor_write * pw, bool last)
{
    int wcount = (int)(pw->limit - pw->ptr);
    int count;
    gs_memory_t *mem = st->memory;
    gs_lib_ctx_core_t *core = mem->gs_lib_ctx->core;

    if (wcount <= 0)
        return 0;

    /* do the callout */
    if (core->stdin_fn)
        count = (*core->stdin_fn)
            (core->std_caller_handle, (char *)pw->ptr + 1,
             core->stdin_is_interactive ? 1 : wcount);
    else
        count = gp_stdin_read((char *)pw->ptr + 1, wcount,
                      core->stdin_is_interactive,
                      core->fstdin);

    pw->ptr += (count < 0) ? 0 : count;
    return ((count < 0) ? ERRC : (count == 0) ? EOFC : count);
}

int
gs_get_callout_stdin(stream **ps, gs_memory_t *mem)
{
    stream *s;
    byte *buf;
    static const stream_procs p = {
        s_std_noavailable, s_std_noseek, s_std_read_reset,
        s_std_read_flush, file_close_file, s_stdin_read_process
    };

    s = file_alloc_stream(mem, "gs_get_callout_stdin(stream)");

    /* We want stdin to read only one character at a time, */
    /* but it must have a substantial buffer, in case it is used */
    /* by a stream that requires more than one input byte */
    /* to make progress. */
    buf = gs_alloc_bytes(mem, STDIN_BUF_SIZE, "gs_get_callout_stdin(buffer)");
    if (s == 0 || buf == 0)
        return_error(gs_error_VMerror);

    s_std_init(s, buf, STDIN_BUF_SIZE, &p, s_mode_read);
    s->file = 0;
    s->file_modes = s->modes;
    s->file_offset = 0;
    s->file_limit = S_FILE_LIMIT_MAX;
    s->save_close = s_std_null;
    *ps = s;
    return 0;
}
