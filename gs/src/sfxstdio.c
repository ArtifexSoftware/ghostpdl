/* Copyright (C) 2001-2006 Artifex Software, Inc.
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
/* File stream implementation using stdio */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "unistd_.h"
#include "gsmemory.h"
#include "gdebug.h"
#include "gpcheck.h"
#include "gp.h"
#include "gserror.h"
#include "gserrors.h"
#include "stream.h"
#include "strimpl.h"

/* Forward references for file stream procedures */
private int
    s_file_available(stream *, long *),
    s_file_read_seek(stream *, long),
    s_file_read_close(stream *),
    s_file_read_process(stream_state *, stream_cursor_read *,
			stream_cursor_write *, bool);
private int
    s_file_write_seek(stream *, long),
    s_file_write_flush(stream *),
    s_file_write_close(stream *),
    s_file_write_process(stream_state *, stream_cursor_read *,
			 stream_cursor_write *, bool);
private int
    s_file_switch(stream *, bool);

/* ------ File reading ------ */

#define DEFAULT_BUFFER_SIZE 2048
const uint file_default_buffer_size = DEFAULT_BUFFER_SIZE;

/* Prepare a stream with a file name. */
/* Return 0 if successful, error code if not. */
/* On a successful return, the C file name is in the stream buffer. */
/* If fname==0, set up stream, and buffer. */
int
file_prepare_stream(const char *fname, uint len, const char *file_access, 
		 uint buffer_size, stream ** ps, char fmode[4], gs_memory_t *mem)
{
    byte *buffer;
    register stream *s;

    /* Open the file, always in binary mode. */
    strcpy(fmode, file_access);
    strcat(fmode, gp_fmode_binary_suffix);
    if (buffer_size == 0)
	buffer_size = file_default_buffer_size;
    if (len >= buffer_size)    /* we copy the file name into the buffer */
	return_error(gs_error_limitcheck);
    /* Allocate the stream first, since it persists */
    /* even after the file has been closed. */
    s = file_alloc_stream(mem, "file_prepare_stream");
    if (s == 0)
	return_error(gs_error_VMerror);
    /* Allocate the buffer. */
    buffer = gs_alloc_bytes(mem, buffer_size, "file_prepare_stream(buffer)");
    if (buffer == 0)
	return_error(gs_error_VMerror);
    if (fname != 0) {
	memcpy(buffer, fname, len);
	buffer[len] = 0;	/* terminate string */
    } else
	buffer[0] = 0;	/* safety */
    s->cbuf = buffer;
    s->bsize = s->cbsize = buffer_size;
    *ps = s;
    return 0;
}

/*
 * Set up a file stream on an OS file.  The caller has allocated the
 * stream and buffer.
 */
void
file_init_stream(stream *s, FILE *file, const char *fmode, byte *buffer,
		 uint buffer_size)
{
    switch (fmode[0]) {
    case 'a':
	sappend_file(s, file, buffer, buffer_size);
	break;
    case 'r':
	/* Defeat buffering for terminals. */
	{
	    struct stat rstat;

	    fstat(fileno(file), &rstat);
	    sread_file(s, file, buffer,
		       (S_ISCHR(rstat.st_mode) ? 1 : buffer_size));
	}
	break;
    case 'w':
	swrite_file(s, file, buffer, buffer_size);
    }
    if (fmode[1] == '+')
	s->file_modes |= s_mode_read | s_mode_write;
    s->save_close = s->procs.close;
    s->procs.close = file_close_file;
}

/* Open a file stream, optionally on an OS file. */
/* Return 0 if successful, error code if not. */
/* On a successful return, the C file name is in the stream buffer. */
/* If fname==0, set up the file entry, stream, and buffer, */
/* but don't open an OS file or initialize the stream. */
int
file_open_stream(const char *fname, uint len, const char *file_access,
		 uint buffer_size, stream ** ps, gx_io_device *iodev,
		 iodev_proc_fopen_t fopen_proc, gs_memory_t *mem)
{
    int code;
    FILE *file;
    char fmode[4];  /* r/w/a, [+], [b], null */

    if (!iodev)
	iodev = iodev_default;
    code = file_prepare_stream(fname, len, file_access, buffer_size, ps, fmode, mem);
    if (code < 0)
	return code;
    if (fname == 0)
	return 0;
    if (fname[0] == 0)		/* fopen_proc gets NUL terminated string, not len */
	return 0;		/* so this is the same as len == 0, so return NULL */
    code = (*fopen_proc)(iodev, (char *)(*ps)->cbuf, fmode, &file,
			 (char *)(*ps)->cbuf, (*ps)->bsize);
    if (code < 0)
	return code;
    file_init_stream(*ps, file, fmode, (*ps)->cbuf, (*ps)->bsize);
    return 0;
}

/* Allocate and return a file stream. */
/* Return 0 if the allocation failed. */
/* The stream is initialized to an invalid state, so the caller need not */
/* worry about cleaning up if a later step in opening the stream fails. */
stream *
file_alloc_stream(gs_memory_t * mem, client_name_t cname)
{
    stream *s;
    s = s_alloc(mem, cname);
    if (s == 0)
	return 0;
    s_init_ids(s);
    s->is_temp = 0;		/* not a temp stream */
    s->foreign = 0;
    /*
     * Disable the stream now (in case we can't open the file,
     * or a filter init procedure fails) so that `restore' won't
     * crash when it tries to close open files.
     */
    s_disable(s);
    s->prev = 0;
    s->next = 0;
    return s;
}

/* Close a file stream.  This replaces the close procedure in the stream */
/* for normal (OS) files and for filters. */
int
file_close_file(stream * s)
{
    stream *stemp = s->strm;
    gs_memory_t *mem;
    int code = file_close_disable(s);

    if (code)
	return code;
    /*
     * Check for temporary streams created for filters.
     * There may be more than one in the case of a procedure-based filter,
     * or if we created an intermediate stream to ensure
     * a large enough buffer.  Note that these streams may have been
     * allocated by file_alloc_stream, so we mustn't free them.
     */
    while (stemp != 0 && stemp->is_temp != 0) {
	stream *snext = stemp->strm;

	mem = stemp->memory;
	if (stemp->is_temp > 1)
	    gs_free_object(mem, stemp->cbuf,
			   "file_close(temp stream buffer)");
	s_disable(stemp);
	stemp = snext;
    }
    mem = s->memory;
    gs_free_object(mem, s->cbuf, "file_close(buffer)");
    if (s->close_strm && stemp != 0)
	return sclose(stemp);
    return 0;
}

/* Initialize a stream for reading an OS file. */
void
sread_file(register stream * s, FILE * file, byte * buf, uint len)
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
    int had_error = ferror(file);
    long curpos = ftell(file);
    bool seekable = (curpos != -1L && fseek(file, curpos, SEEK_SET) == 0);

    if (!had_error)
	clearerr(file);
    s_std_init(s, buf, len, &p,
	       (seekable ? s_mode_read + s_mode_seek : s_mode_read));
    if_debug1('s', "[s]read file=0x%lx\n", (ulong) file);
    s->file = file;
    s->file_modes = s->modes;
    s->file_offset = 0;
    s->file_limit = max_long;
}

/* Confine reading to a subfile.  This is primarily for reusable streams. */
int
sread_subfile(stream *s, long start, long length)
{
    if (s->file == 0 || s->modes != s_mode_read + s_mode_seek ||
	s->file_offset != 0 || s->file_limit != max_long ||
	((s->position < start || s->position > start + length) &&
	 sseek(s, start) < 0)
	)
	return ERRC;
    s->position -= start;
    s->file_offset = start;
    s->file_limit = length;
    return 0;
}

/* Procedures for reading from a file */
private int
s_file_available(register stream * s, long *pl)
{
    long max_avail = s->file_limit - stell(s);
    long buf_avail = sbufavailable(s);

    *pl = min(max_avail, buf_avail);
    if (sseekable(s)) {
	long pos, end;

	pos = ftell(s->file);
	if (fseek(s->file, 0L, SEEK_END))
	    return ERRC;
	end = ftell(s->file);
	if (fseek(s->file, pos, SEEK_SET))
	    return ERRC;
	buf_avail += end - pos;
	*pl = min(max_avail, buf_avail);
	if (*pl == 0)
	    *pl = -1;		/* EOF */
    } else {
	if (*pl == 0 && feof(s->file))
	    *pl = -1;		/* EOF */
    }
    return 0;
}
private int
s_file_read_seek(register stream * s, long pos)
{
    uint end = s->srlimit - s->cbuf + 1;
    long offset = pos - s->position;

    if (offset >= 0 && offset <= end) {  /* Staying within the same buffer */
	s->srptr = s->cbuf + offset - 1;
	return 0;
    }
    if (pos < 0 || pos > s->file_limit ||
	fseek(s->file, s->file_offset + pos, SEEK_SET) != 0
	)
	return ERRC;
    s->srptr = s->srlimit = s->cbuf - 1;
    s->end_status = 0;
    s->position = pos;
    return 0;
}
private int
s_file_read_close(stream * s)
{
    FILE *file = s->file;

    if (file != 0) {
	s->file = 0;
	return (fclose(file) ? ERRC : 0);
    }
    return 0;
}

/*
 * Process a buffer for a file reading stream.
 * This is the first stream in the pipeline, so pr is irrelevant.
 */
private int
s_file_read_process(stream_state * st, stream_cursor_read * ignore_pr,
		    stream_cursor_write * pw, bool last)
{
    stream *s = (stream *)st;	/* no separate state */
    FILE *file = s->file;
    uint max_count = pw->limit - pw->ptr;
    int status = 1;
    int count;

    if (s->file_limit < max_long) {
	long limit_count = s->file_offset + s->file_limit - ftell(file);

	if (max_count > limit_count)
	    max_count = limit_count, status = EOFC;
    }
    count = fread(pw->ptr + 1, 1, max_count, file);
    if (count < 0)
	count = 0;
    pw->ptr += count;
    process_interrupts(s->memory);
    return (ferror(file) ? ERRC : feof(file) ? EOFC : status);
}

/* ------ File writing ------ */

/* Initialize a stream for writing an OS file. */
void
swrite_file(register stream * s, FILE * file, byte * buf, uint len)
{
    static const stream_procs p = {
	s_std_noavailable, s_file_write_seek, s_std_write_reset,
	s_file_write_flush, s_file_write_close, s_file_write_process,
	s_file_switch
    };

    s_std_init(s, buf, len, &p,
	       (file == stdout ? s_mode_write : s_mode_write + s_mode_seek));
    if_debug1('s', "[s]write file=0x%lx\n", (ulong) file);
    s->file = file;
    s->file_modes = s->modes;
    s->file_offset = 0;		/* in case we switch to reading later */
    s->file_limit = max_long;	/* ibid. */
}
/* Initialize for appending to an OS file. */
void
sappend_file(register stream * s, FILE * file, byte * buf, uint len)
{
    swrite_file(s, file, buf, len);
    s->modes = s_mode_write + s_mode_append;	/* no seek */
    s->file_modes = s->modes;
    fseek(file, 0L, SEEK_END);
    s->position = ftell(file);
}
/* Procedures for writing on a file */
private int
s_file_write_seek(stream * s, long pos)
{
    /* We must flush the buffer to reposition. */
    int code = sflush(s);

    if (code < 0)
	return code;
    if (fseek(s->file, pos, SEEK_SET) != 0)
	return ERRC;
    s->position = pos;
    return 0;
}
private int
s_file_write_flush(register stream * s)
{
    int result = s_process_write_buf(s, false);

    fflush(s->file);
    return result;
}
private int
s_file_write_close(register stream * s)
{
    s_process_write_buf(s, true);
    return s_file_read_close(s);
}

/*
 * Process a buffer for a file writing stream.
 * This is the last stream in the pipeline, so pw is irrelevant.
 */
private int
s_file_write_process(stream_state * st, stream_cursor_read * pr,
		     stream_cursor_write * ignore_pw, bool last)
{
    uint count = pr->limit - pr->ptr;

    /*
     * The DEC C library on AXP architectures gives an error on
     * fwrite if the count is zero!
     */
    if (count != 0) {
	FILE *file = ((stream *) st)->file;
	int written = fwrite(pr->ptr + 1, 1, count, file);

	if (written < 0)
	    written = 0;
	pr->ptr += written;
	process_interrupts(NULL);
	return (ferror(file) ? ERRC : 0);
    } else {
	process_interrupts(NULL);
	return 0;
    }
}

/* ------ File switching ------ */

/* Switch a file stream to reading or writing. */
private int
s_file_switch(stream * s, bool writing)
{
    uint modes = s->file_modes;
    FILE *file = s->file;
    long pos;

    if (writing) {
	if (!(s->file_modes & s_mode_write))
	    return ERRC;
	pos = stell(s);
	if_debug2('s', "[s]switch 0x%lx to write at %ld\n",
		  (ulong) s, pos);
	fseek(file, pos, SEEK_SET);
	if (modes & s_mode_append) {
	    sappend_file(s, file, s->cbuf, s->cbsize);	/* sets position */
	} else {
	    swrite_file(s, file, s->cbuf, s->cbsize);
	    s->position = pos;
	}
	s->modes = modes;
    } else {
	if (!(s->file_modes & s_mode_read))
	    return ERRC;
	pos = stell(s);
	if_debug2('s', "[s]switch 0x%lx to read at %ld\n",
		  (ulong) s, pos);
	if (sflush(s) < 0)
	    return ERRC;
	fseek(file, 0L, SEEK_CUR);	/* pacify C library */
	sread_file(s, file, s->cbuf, s->cbsize);
	s->modes |= modes & s_mode_append;	/* don't lose append info */
	s->position = pos;
    }
    s->file_modes = modes;
    return 0;
}
