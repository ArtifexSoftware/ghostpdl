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


/* File stream implementation using stdio */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "unistd_.h"
#include "gsmemory.h"
#include "gdebug.h"
#include "gpcheck.h"
#include "gp.h"
#include "gserrors.h"
#include "stream.h"
#include "strimpl.h"

/* Forward references for file stream procedures */
static int
    s_file_available(stream *, gs_offset_t *),
    s_file_read_seek(stream *, gs_offset_t),
    s_file_read_close(stream *),
    s_file_read_process(stream_state *, stream_cursor_read *,
                        stream_cursor_write *, bool);
static int
    s_file_write_seek(stream *, gs_offset_t),
    s_file_write_flush(stream *),
    s_file_write_close(stream *),
    s_file_write_process(stream_state *, stream_cursor_read *,
                         stream_cursor_write *, bool);
static int
    s_file_switch(stream *, bool);

/* ------ File reading ------ */

/* Initialize a stream for reading an OS file. */
void
sread_file(register stream * s, gp_file * file, byte * buf, uint len)
{
    static const stream_procs p = {
        s_file_available, s_file_read_seek, s_std_read_reset,
        s_std_read_flush, s_file_read_close, s_file_read_process,
        s_file_switch
    };
    /*
     * There is no really portable way to test seekability, but this should
     * work on most systems.  Note that if our probe sets the ferror bit for
     * the stream, we have to clear it again to avoid trouble later.
     */
    int had_error = gp_ferror(file);
    gs_offset_t curpos = gp_ftell(file);
    bool seekable = (curpos != -1L && gp_fseek(file, curpos, SEEK_SET) == 0);

    if (!had_error)
        gp_clearerr(file);
    s_std_init(s, buf, len, &p,
               (seekable ? s_mode_read + s_mode_seek : s_mode_read));
    if_debug1m('s', s->memory, "[s]read file="PRI_INTPTR"\n", (intptr_t)file);
    s->file = file;
    s->file_modes = s->modes;
    s->file_offset = 0;
    s->file_limit = (sizeof(gs_offset_t) > 4 ? max_int64_t : max_long);
}

/* Confine reading to a subfile.  This is primarily for reusable streams. */
int
sread_subfile(stream *s, gs_offset_t start, gs_offset_t length)
{
    if (s->file == 0 || s->modes != s_mode_read + s_mode_seek ||
        s->file_offset != 0 ||
        s->file_limit != S_FILE_LIMIT_MAX ||
        ((s->position < start || s->position > start + length) && sseek(s, start) < 0)
        )
        return ERRC;
    s->position -= start;
    s->file_offset = start;
    s->file_limit = length;
    return 0;
}

/* Procedures for reading from a file */
static int
s_file_available(register stream * s, gs_offset_t *pl)
{
    gs_offset_t max_avail = s->file_limit - stell(s);
    gs_offset_t buf_avail = sbufavailable(s);

    *pl = min(max_avail, buf_avail);
    if (sseekable(s)) {
        gs_offset_t pos, end;

        pos = gp_ftell(s->file);
        if (gp_fseek(s->file, 0, SEEK_END))
            return ERRC;
        end = gp_ftell(s->file);
        if (gp_fseek(s->file, pos, SEEK_SET))
            return ERRC;
        buf_avail += end - pos;
        *pl = min(max_avail, buf_avail);
        if (*pl == 0)
            *pl = -1;		/* EOF */
    } else {
        /* s->end_status == EOFC may indicate the stream is disabled
         * or that the underlying gp_file * has reached EOF.
         */
        if (*pl == 0 && (s->end_status == EOFC || gp_feof(s->file)))
            *pl = -1;		/* EOF */
    }
    return 0;
}
static int
s_file_read_seek(register stream * s, gs_offset_t pos)
{
    gs_offset_t end = s->cursor.r.limit - s->cbuf + 1;
    gs_offset_t offset = pos - s->position;

    if (offset >= 0 && offset <= end) {  /* Staying within the same buffer */
        s->cursor.r.ptr = s->cbuf + offset - 1;
        return 0;
    }
    if (pos < 0 || pos > s->file_limit || s->file == NULL ||
        gp_fseek(s->file, s->file_offset + pos, SEEK_SET) != 0
        )
        return ERRC;
    s->cursor.r.ptr = s->cursor.r.limit = s->cbuf - 1;
    s->end_status = 0;
    s->position = pos;
    return 0;
}
static int
s_file_read_close(stream * s)
{
    gp_file *file = s->file;

    if (file != 0) {
        s->file = 0;
        return (gp_fclose(file) ? ERRC : 0);
    }
    return 0;
}

/*
 * Process a buffer for a file reading stream.
 * This is the first stream in the pipeline, so pr is irrelevant.
 */
static int
s_file_read_process(stream_state * st, stream_cursor_read * ignore_pr,
                    stream_cursor_write * pw, bool last)
{
    stream *s = (stream *)st;	/* no separate state */
    gp_file *file = s->file;
    gs_offset_t max_count = pw->limit - pw->ptr;
    int status = 1;
    int count;

    if (s->file_limit < S_FILE_LIMIT_MAX) {
        gs_offset_t limit_count = s->file_offset + s->file_limit - gp_ftell(file);

        if (max_count > limit_count)
            max_count = limit_count, status = EOFC;
    }
    count = gp_fread(pw->ptr + 1, 1, max_count, file);
    if (count < 0)
        count = 0;
    pw->ptr += count;
    process_interrupts(s->memory);
    return (gp_ferror(file) ? ERRC : gp_feof(file) ? EOFC : status);
}

/* ------ File writing ------ */

/* Initialize a stream for writing an OS file. */
void
swrite_file(register stream * s, gp_file * file, byte * buf, uint len)
{
    static const stream_procs p = {
        s_std_noavailable, s_file_write_seek, s_std_write_reset,
        s_file_write_flush, s_file_write_close, s_file_write_process,
        s_file_switch
    };

    s_std_init(s, buf, len, &p,
               (gp_get_file(file) == stdout ? s_mode_write : s_mode_write + s_mode_seek));
    if_debug1m('s', s->memory, "[s]write file="PRI_INTPTR"\n", (intptr_t) file);
    s->file = file;
    s->file_modes = s->modes;
    s->file_offset = 0;		/* in case we switch to reading later */
    s->file_limit = S_FILE_LIMIT_MAX;
}
/* Initialize for appending to an OS file. */
int
sappend_file(register stream * s, gp_file * file, byte * buf, uint len)
{
    swrite_file(s, file, buf, len);
    s->modes = s_mode_write + s_mode_append;	/* no seek */
    s->file_modes = s->modes;
    if (gp_fseek(file, 0L, SEEK_END) != 0)
        return ERRC;
    s->position = gp_ftell(file);
    return 0;
}
/* Procedures for writing on a file */
static int
s_file_write_seek(stream * s, gs_offset_t pos)
{
    /* We must flush the buffer to reposition. */
    int code = sflush(s);

    if (code < 0)
        return code;
    if (gp_fseek(s->file, pos, SEEK_SET) != 0)
        return ERRC;
    s->position = pos;
    return 0;
}
static int
s_file_write_flush(register stream * s)
{
    int result = s_process_write_buf(s, false);

    gp_fflush(s->file);
    return result;
}
static int
s_file_write_close(register stream * s)
{
    s_process_write_buf(s, true);
    return s_file_read_close(s);
}

/*
 * Process a buffer for a file writing stream.
 * This is the last stream in the pipeline, so pw is irrelevant.
 */
static int
s_file_write_process(stream_state * st, stream_cursor_read * pr,
                     stream_cursor_write * ignore_pw, bool last)
{
    uint count = pr->limit - pr->ptr;

    /*
     * The DEC C library on AXP architectures gives an error on
     * fwrite if the count is zero!
     */
    if (count != 0) {
        gp_file *file = ((stream *) st)->file;
        int written = gp_fwrite(pr->ptr + 1, 1, count, file);

        if (written < 0)
            written = 0;
        pr->ptr += written;
        process_interrupts(st->memory);
        return (gp_ferror(file) ? ERRC : 0);
    } else {
        process_interrupts(st->memory);
        return 0;
    }
}

/* ------ File switching ------ */

/* Switch a file stream to reading or writing. */
static int
s_file_switch(stream * s, bool writing)
{
    uint modes = s->file_modes;
    gp_file *file = s->file;
    gs_offset_t pos;

    if (writing) {
        if (!(s->file_modes & s_mode_write))
            return ERRC;
        pos = stell(s);
        if_debug2m('s', s->memory, "[s]switch 0x%"PRIx64" to write at %"PRId64"\n",
                   (uint64_t) s, (int64_t)pos);
        if (gp_fseek(file, pos, SEEK_SET) != 0)
            return ERRC;
        if (modes & s_mode_append) {
            if (sappend_file(s, file, s->cbuf, s->cbsize)!= 0)	/* sets position */
                return ERRC;
        } else {
            swrite_file(s, file, s->cbuf, s->cbsize);
            s->position = pos;
        }
        s->modes = modes;
    } else {
        if (!(s->file_modes & s_mode_read))
            return ERRC;
        pos = stell(s);
        if_debug2m('s', s->memory, "[s]switch 0x%"PRIu64" to read at %"PRId64"\n",
                   (uint64_t) s, (int64_t)pos);
        if (sflush(s) < 0)
            return ERRC;
        if (gp_fseek(file, 0L, SEEK_CUR) != 0)
            return ERRC;
        sread_file(s, file, s->cbuf, s->cbsize);
        s->modes |= modes & s_mode_append;	/* don't lose append info */
        s->position = pos;
    }
    s->file_modes = modes;
    return 0;
}
