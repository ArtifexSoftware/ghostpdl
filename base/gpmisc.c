/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Miscellaneous support for platform facilities */

#include "errno_.h"
#include "stat_.h"
#include "unistd_.h"
#include "fcntl_.h"
#include "stdio_.h"
#include "memory_.h"
#include "string_.h"
#include "gp.h"
#include "gpgetenv.h"
#include "gpmisc.h"
#include "gserrors.h"

/*
 * Get the name of the directory for temporary files, if any.  Currently
 * this checks the TMPDIR and TEMP environment variables, in that order.
 * The return value and the setting of *ptr and *plen are as for gp_getenv.
 */
int
gp_gettmpdir(char *ptr, int *plen)
{
    int max_len = *plen;
    int code = gp_getenv("TMPDIR", ptr, plen);

    if (code != 1)
        return code;
    *plen = max_len;
    return gp_getenv("TEMP", ptr, plen);
}

/*
 * Open a temporary file, using O_EXCL and S_I*USR to prevent race
 * conditions and symlink attacks.
 */
FILE *
gp_fopentemp(const char *fname, const char *mode)
{
    int flags = O_EXCL;
    /* Scan the mode to construct the flags. */
    const char *p = mode;
    int fildes;
    FILE *file;

#if defined (O_LARGEFILE)
    /* It works for Linux/gcc. */
    flags |= O_LARGEFILE;
#else
    /* fixme : Not sure what to do. Unimplemented. */
    /* MSVC has no O_LARGEFILE, but MSVC build never calls this function. */
#endif
    while (*p)
        switch (*p++) {
        case 'a':
            flags |= O_CREAT | O_APPEND;
            break;
        case 'r':
            flags |= O_RDONLY;
            break;
        case 'w':
            flags |= O_CREAT | O_WRONLY | O_TRUNC;
            break;
#ifdef O_BINARY
            /* Watcom C insists on this non-ANSI flag being set. */
        case 'b':
            flags |= O_BINARY;
            break;
#endif
        case '+':
            flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
            break;
        default:		/* e.g., 'b' */
            break;
        }
    fildes = open(fname, flags, S_IRUSR | S_IWUSR);
    if (fildes < 0)
        return 0;
    /*
     * The DEC VMS C compiler incorrectly defines the second argument of
     * fdopen as (char *), rather than following the POSIX.1 standard,
     * which defines it as (const char *).  Patch this here.
     */
    file = fdopen(fildes, (char *)mode); /* still really const */
    if (file == 0)
        close(fildes);
    return file;
}

/* Append a string to buffer. */
static inline bool
append(char **bp, const char *bpe, const char **ip, uint len)
{
    if (bpe - *bp < len)
        return false;
    memcpy(*bp, *ip, len);
    *bp += len;
    *ip += len;
    return true;
}

/* Search a separator forward. */
static inline uint
search_separator(const char **ip, const char *ipe, const char *item, int direction)
{   uint slen = 0;
    for (slen = 0; (*ip - ipe) * direction < 0; (*ip) += direction)
        if((slen = gs_file_name_check_separator(*ip, ipe - *ip, item)) != 0)
            break;
    return slen;
}

/*
 * Combine a file name with a prefix.
 * Concatenates two paths and reduce parent references and current
 * directory references from the concatenation when possible.
 * The trailing zero byte is being added.
 *
 * Returns "gp_combine_success" if OK and sets '*blen' to the length of
 * the combined string. If the combined string is too small for the buffer
 * length passed in (as defined by the initial value of *blen), then the
 * "gp_combine_small_buffer" code is returned.
 *
 * Also tolerates/skips leading IODevice specifiers such as %os% or %rom%
 * When there is a leading '%' in the 'fname' no other processing is done.
 *
 * Examples :
 *	"/gs/lib" + "../Resource/CMap/H" --> "/gs/Resource/CMap/H"
 *	"C:/gs/lib" + "../Resource/CMap/H" --> "C:/gs/Resource/CMap/H"
 *	"hard disk:gs:lib" + "::Resource:CMap:H" -->
 *		"hard disk:gs:Resource:CMap:H"
 *	"DUA1:[GHOSTSCRIPT.LIB" + "-.RESOURCE.CMAP]H" -->
 *		"DUA1:[GHOSTSCRIPT.RESOURCE.CMAP]H"
 *      "\\server\share/a/b///c/../d.e/./" + "../x.e/././/../../y.z/v.v" -->
 *		"\\server\share/a/y.z/v.v"
 *	"%rom%lib/" + "gs_init.ps" --> "%rom%lib/gs_init.ps
 *	"" + "%rom%lib/gs_init.ps" --> "%rom%lib/gs_init.ps"
 */
gp_file_name_combine_result
gp_file_name_combine_generic(const char *prefix, uint plen, const char *fname, uint flen,
                    bool no_sibling, char *buffer, uint *blen)
{
    /*
     * THIS CODE IS SHARED FOR MULTIPLE PLATFORMS.
     * PLEASE DON'T CHANGE IT FOR A SPECIFIC PLATFORM.
     * Change gp_file_name_combine instead.
     */
    char *bp = buffer, *bpe = buffer + *blen;
    const char *ip, *ipe;
    uint slen;
    uint infix_type = 0; /* 0=none, 1=current, 2=parent. */
    uint infix_len = 0;
    uint rlen = gp_file_name_root(fname, flen);
    /* We need a special handling of infixes only immediately after a drive. */

    if ( flen > 0 && fname[0] == '%') {
        /* IoDevice -- just return the fname as-is since this */
        /* function only handles the default file system */
        /* NOTE: %os% will subvert the normal processing of prefix and fname */
        ip = fname;
        *blen = flen;
        if (!append(&bp, bpe, &ip, flen))
            return gp_combine_small_buffer;
        return gp_combine_success;
    }
    if (rlen != 0) {
        /* 'fname' is absolute, ignore the prefix. */
        ip = fname;
        ipe = fname + flen;
    } else {
        /* Concatenate with prefix. */
        ip = prefix;
        ipe = prefix + plen;
        rlen = gp_file_name_root(prefix, plen);
    }
    if (!append(&bp, bpe, &ip, rlen))
        return gp_combine_small_buffer;
    for (;;) {
        const char *item = ip;
        uint ilen;

        slen = search_separator(&ip, ipe, item, 1);
        ilen = ip - item;
        if (ilen == 0 && !gp_file_name_is_empty_item_meanful()) {
            ip += slen;
            slen = 0;
        } else if (gp_file_name_is_current(item, ilen)) {
            /* Skip the reference to 'current', except the starting one.
             * We keep the starting 'current' for platforms, which
             * require relative paths to start with it.
             */
            if (bp == buffer) {
                if (!append(&bp, bpe, &item, ilen))
                    return gp_combine_small_buffer;
                infix_type = 1;
                infix_len = ilen;
            } else {
                ip += slen;
                slen = 0;
            }
        } else if (!gp_file_name_is_parent(item, ilen)) {
            if (!append(&bp, bpe, &item, ilen))
                return gp_combine_small_buffer;
            /* The 'item' pointer is now broken; it may be restored using 'ilen'. */
        } else if (bp == buffer + rlen + infix_len) {
            /* Input is a parent and the output only contains a root and an infix. */
            if (rlen != 0)
                return gp_combine_cant_handle;
            switch (infix_type) {
                case 1:
                    /* Replace the infix with parent. */
                    bp = buffer + rlen; /* Drop the old infix, if any. */
                    infix_len = 0;
                    /* Falls through. */
                case 0:
                    /* We have no infix, start with parent. */
                    if ((no_sibling && ipe == fname + flen && flen != 0) ||
                            !gp_file_name_is_parent_allowed())
                        return gp_combine_cant_handle;
                    /* Falls through. */
                case 2:
                    /* Append one more parent - falls through. */
                    DO_NOTHING;
            }
            if (!append(&bp, bpe, &item, ilen))
                return gp_combine_small_buffer;
            infix_type = 2;
            infix_len += ilen;
            /* Recompute the separator length. We cannot use the old slen on Mac OS. */
            slen = gs_file_name_check_separator(ip, ipe - ip, ip);
        } else {
            /* Input is a parent and the output continues after infix. */
            /* Unappend the last separator and the last item. */
            uint slen1 = gs_file_name_check_separator(bp, buffer + rlen - bp, bp); /* Backward search. */
            char *bie = bp - slen1;

            bp = bie;
            DISCARD(search_separator((const char **)&bp, buffer + rlen, bp, -1));
            /* The cast above quiets a gcc warning. We believe it's a bug in the compiler. */
            /* Skip the input with separator. We cannot use slen on Mac OS. */
            ip += gs_file_name_check_separator(ip, ipe - ip, ip);
            if (no_sibling) {
                const char *p = ip;

                DISCARD(search_separator(&p, ipe, ip, 1));
                if (p - ip != bie - bp || memcmp(ip, bp, p - ip))
                    return gp_combine_cant_handle;
            }
            slen = 0;
        }
        if (slen) {
            if (bp == buffer + rlen + infix_len)
                infix_len += slen;
            if (!append(&bp, bpe, &ip, slen))
                return gp_combine_small_buffer;
        }
        if (ip == ipe) {
            if (ipe == fname + flen) {
                /* All done.
                 * Note that the case (prefix + plen == fname && flen == 0)
                 * falls here without appending a separator.
                 */
                const char *zero="";

                if (bp == buffer) {
                    /* Must not return empty path. */
                    const char *current = gp_file_name_current();
                    int clen = strlen(current);

                    if (!append(&bp, bpe, &current, clen))
                        return gp_combine_small_buffer;
                }
                *blen = bp - buffer;
                if (!append(&bp, bpe, &zero, 1))
                    return gp_combine_small_buffer;
                return gp_combine_success;
            } else {
                /* ipe == prefix + plen */
                /* Switch to fname. */
                ip = fname;
                ipe = fname + flen;
                if (slen == 0) {
                    /* Insert a separator. */
                    const char *sep;

                    slen = search_separator(&ip, ipe, fname, 1);
                    sep = (slen != 0 ? gp_file_name_directory_separator()
                                    : gp_file_name_separator());
                    slen = strlen(sep);
                    if (bp == buffer + rlen + infix_len)
                        infix_len += slen;
                    if (!append(&bp, bpe, &sep, slen))
                        return gp_combine_small_buffer;
                    ip = fname; /* Switch to fname. */
                }
            }
        }
    }
}

/*
 * Reduces parent references and current directory references when possible.
 * The trailing zero byte is being added.
 *
 * Examples :
 *	"/gs/lib/../Resource/CMap/H" --> "/gs/Resource/CMap/H"
 *	"C:/gs/lib/../Resource/CMap/H" --> "C:/gs/Resource/CMap/H"
 *	"hard disk:gs:lib::Resource:CMap:H" -->
 *		"hard disk:gs:Resource:CMap:H"
 *	"DUA1:[GHOSTSCRIPT.LIB.-.RESOURCE.CMAP]H" -->
 *		"DUA1:[GHOSTSCRIPT.RESOURCE.CMAP]H"
 *      "\\server\share/a/b///c/../d.e/./../x.e/././/../../y.z/v.v" -->
 *		"\\server\share/a/y.z/v.v"
 *
 */
gp_file_name_combine_result
gp_file_name_reduce(const char *fname, uint flen, char *buffer, uint *blen)
{
    return gp_file_name_combine(fname, flen, fname + flen, 0, false, buffer, blen);
}

/*
 * Answers whether a file name is absolute (starts from a root).
 */
bool
gp_file_name_is_absolute(const char *fname, uint flen)
{
    return (gp_file_name_root(fname, flen) > 0);
}

/*
 * Returns length of all starting parent references.
 */
static uint
gp_file_name_prefix(const char *fname, uint flen,
                bool (*test)(const char *fname, uint flen))
{
    uint plen = gp_file_name_root(fname, flen), slen;
    const char *ip, *ipe;
    const char *item = fname; /* plen == flen could cause an indeterminizm. */

    if (plen > 0)
        return 0;
    ip = fname + plen;
    ipe = fname + flen;
    for (; ip < ipe; ) {
        item = ip;
        slen = search_separator(&ip, ipe, item, 1);
        if (!(*test)(item, ip - item))
            break;
        ip += slen;
    }
    return item - fname;
}

/*
 * Returns length of all starting parent references.
 */
uint
gp_file_name_parents(const char *fname, uint flen)
{
    return gp_file_name_prefix(fname, flen, gp_file_name_is_parent);
}

/*
 * Returns length of all starting cwd references.
 */
uint
gp_file_name_cwds(const char *fname, uint flen)
{
    return gp_file_name_prefix(fname, flen, gp_file_name_is_current);
}

static int
generic_pread(gp_file *f, size_t count, gs_offset_t offset, void *buf)
{
    int c;
    int64_t os, curroff = gp_ftell(f);
    if (curroff < 0) return curroff;

    os = gp_fseek(f, offset, 0);
    if (os < 0) return os;

    c = gp_fread(buf, 1, count, f);
    if (c < 0) return c;

    os = gp_fseek(f, curroff, 0);
    if (os < 0) return os;

    return c;
}

static int
generic_pwrite(gp_file *f, size_t count, gs_offset_t offset, const void *buf)
{
    int c;
    int64_t os, curroff = gp_ftell(f);
    if (curroff < 0) return curroff;

    os = gp_fseek(f, offset, 0);
    if (os < 0) return os;

    c = gp_fwrite(buf, 1, count, f);
    if (c < 0) return c;

    os = gp_fseek(f, curroff, 0);
    if (os < 0) return os;

    return c;
}

gp_file *gp_file_alloc(const gs_memory_t *mem, const gp_file_ops_t *prototype, size_t size, const char *cname)
{
    gp_file *file = (gp_file *)gs_alloc_bytes(mem->thread_safe_memory, size, cname ? cname : "gp_file");
    if (file == NULL)
        return NULL;

    if (prototype)
        file->ops = *prototype;
    if (file->ops.pread == NULL)
        file->ops.pread = generic_pread;
    if (file->ops.pwrite == NULL)
        file->ops.pwrite = generic_pwrite;
    if (size > sizeof(*prototype))
        memset(((char *)file)+sizeof(*prototype),
               0,
               size - sizeof(*prototype));
    file->memory = mem->thread_safe_memory;

    return file;
}

void gp_file_dealloc(gp_file *file)
{
    if (file == NULL)
        return;

    if (file->buffer)
        gs_free_object(file->memory, file->buffer, "gp_file");
    gs_free_object(file->memory, file, "gp_file");
}

int gp_fprintf(gp_file *f, const char *fmt, ...)
{
    va_list args;
    int n;

    if (f->buffer)
        goto mid;
    do {
        n = f->buffer_size * 2;
        if (n == 0)
            n = 256;
        gs_free_object(f->memory, f->buffer, "gp_file(buffer)");
        f->buffer = (char *)gs_alloc_bytes(f->memory, n, "gp_file(buffer)");
        if (f->buffer == NULL)
            return -1;
        f->buffer_size = n;
mid:
        va_start(args, fmt);
        n = vsnprintf(f->buffer, f->buffer_size, fmt, args);
        va_end(args);
    } while (n >= f->buffer_size);
    return (f->ops.write)(f, 1, n, f->buffer);
}
typedef struct {
    gp_file base;
    FILE *file;
    int (*close)(FILE *file);
} gp_file_FILE;

static int
gp_file_FILE_close(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return (file->close)(file->file);
}

static int
gp_file_FILE_getc(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return fgetc(file->file);
}

static int
gp_file_FILE_putc(gp_file *file_, int c)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return fputc(c, file->file);
}

static int
gp_file_FILE_read(gp_file *file_, size_t size, unsigned int count, void *buf)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return fread(buf, size, count, file->file);
}

static int
gp_file_FILE_write(gp_file *file_, size_t size, unsigned int count, const void *buf)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return fwrite(buf, size, count, file->file);
}

static int
gp_file_FILE_seek(gp_file *file_, gs_offset_t offset, int whence)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return gp_fseek_impl(file->file, offset, whence);
}

static gs_offset_t
gp_file_FILE_tell(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return gp_ftell_impl(file->file);
}

static int
gp_file_FILE_eof(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return feof(file->file);
}

static gp_file *
gp_file_FILE_dup(gp_file *file_, const char *mode)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;
    gp_file *file2 = gp_file_FILE_alloc(file->base.memory);

    if (gp_file_FILE_set(file2, gp_fdup_impl(file->file, mode), NULL))
        file2 = NULL;

    return file2;
}

static int
gp_file_FILE_seekable(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return gp_fseekable_impl(file->file);
}

static int
gp_file_FILE_pread(gp_file *file_, size_t count, gs_offset_t offset, void *buf)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return gp_pread_impl(buf, count, offset, file->file);
}

static int
gp_file_FILE_pwrite(gp_file *file_, size_t count, gs_offset_t offset, const void *buf)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return gp_pwrite_impl(buf, count, offset, file->file);
}

static int
gp_file_FILE_is_char_buffered(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;
    struct stat rstat;

    if (fstat(fileno(file->file), &rstat) != 0)
        return ERRC;
    return S_ISCHR(rstat.st_mode);
}

static void
gp_file_FILE_fflush(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    fflush(file->file);
}

static int
gp_file_FILE_ferror(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return ferror(file->file);
}

static FILE *
gp_file_FILE_get_file(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    return file->file;
}

static void
gp_file_FILE_clearerr(gp_file *file_)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    clearerr(file->file);
}

static gp_file *
gp_file_FILE_reopen(gp_file *file_, const char *fname, const char *mode)
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    file->file = freopen(fname, mode, file->file);
    if (file->file == NULL) {
        gp_file_dealloc(file_);
        return NULL;
    }
    return file_;
}

static const gp_file_ops_t gp_file_FILE_prototype =
{
    gp_file_FILE_close,
    gp_file_FILE_getc,
    gp_file_FILE_putc,
    gp_file_FILE_read,
    gp_file_FILE_write,
    gp_file_FILE_seek,
    gp_file_FILE_tell,
    gp_file_FILE_eof,
    gp_file_FILE_dup,
    gp_file_FILE_seekable,
    gp_file_FILE_pread,
    gp_file_FILE_pwrite,
    gp_file_FILE_is_char_buffered,
    gp_file_FILE_fflush,
    gp_file_FILE_ferror,
    gp_file_FILE_get_file,
    gp_file_FILE_clearerr,
    gp_file_FILE_reopen
};

gp_file *gp_file_FILE_alloc(const gs_memory_t *mem)
{
    return gp_file_alloc(mem->non_gc_memory,
                         &gp_file_FILE_prototype,
                         sizeof(gp_file_FILE),
                         "gp_file_FILE");
}

int gp_file_FILE_set(gp_file *file_, FILE *f, int (*close)(FILE *))
{
    gp_file_FILE *file = (gp_file_FILE *)file_;

    if (f == NULL) {
        gp_file_dealloc(file_);
        return 1;
    }

    file->file = f;
    file->close = close ? close : fclose;

    return 0;
}

char *gp_fgets(char *buffer, size_t n, gp_file *f)
{
    int c = EOF;
    char *b = buffer;
    while (n > 1) {
        c = gp_fgetc(f);
	if (c == 0)
            break;
	*b++ = c;
	n--;
    }
    if (c == EOF && b == buffer)
        return NULL;
    if (gp_ferror(f))
        return NULL;
    if (n > 0)
        *b++ = 0;
    return buffer;
}

gp_file *
gp_fopen(const gs_memory_t *mem, const char *fname, const char *mode)
{
    gp_file *file = NULL;
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;
    gs_fs_list_t *fs = ctx->core->fs;

    if (gp_validate_path(mem, fname, mode) != 0)
        return NULL;

    for (fs = ctx->core->fs; fs != NULL; fs = fs->next)
    {
        int code = 0;
        if (fs->fs.open_file)
            code = fs->fs.open_file(mem, fs->secret, fname, mode, &file);
        if (code < 0)
            return NULL;
        if (file != NULL)
            break;
    }

    return file;
}

gp_file *
gp_open_printer(const gs_memory_t *mem,
                      char         fname[gp_file_name_sizeof],
                      int          binary_mode)
{
    gp_file *file = NULL;
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;
    gs_fs_list_t *fs = ctx->core->fs;

    if (gp_validate_path(mem, fname, binary_mode ? "wb" : "w") != 0)
        return NULL;

    for (fs = ctx->core->fs; fs != NULL; fs = fs->next)
    {
        int code = 0;
        if (fs->fs.open_printer)
            code = fs->fs.open_printer(mem, fs->secret, fname, binary_mode, &file);
        if (code < 0)
            return NULL;
        if (file != NULL)
            break;
    }

    return file;
}

static gp_file *
do_open_scratch_file(const gs_memory_t *mem,
                     const char        *prefix,
                     char              *fname,
                     const char        *mode,
                     int                rm)
{
    gp_file *file = NULL;
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;
    gs_fs_list_t *fs = ctx->core->fs;
    int code = 0;

    /* If the prefix is absolute, then we must check it's a permissible
     * path. If not, we're OK. */
    if (gp_file_name_is_absolute(prefix, strlen(prefix)) &&
        gp_validate_path(mem, prefix, mode) != 0)
            return NULL;

    for (fs = ctx->core->fs; fs != NULL; fs = fs->next)
    {
        if (fs->fs.open_scratch)
            code = fs->fs.open_scratch(mem, fs->secret, prefix, fname, mode, rm, &file);
        if (code < 0)
            return NULL;
        if (file != NULL)
            break;
    }

    if (file == NULL) {
        /* The file failed to open. Don't add it to the list. */
    } else if (rm) {
        /* This file has already been deleted by the underlying system.
         * We don't need to add it to the lists as it will never be
         * deleted manually, nor do we need to tidy it up on closedown. */
    } else {
         /* This file was not requested to be deleted. We add it to the
          * list so that it will either be deleted by any future call to
          * zdeletefile, OR on closedown. */
         /* Add the scratch file name to the lists. We can't do this any
          * earlier as we didn't know the name until now! Unfortunately
          * that makes cleanup harder. */
        code = gs_add_control_path_flags(mem, gs_permit_file_control, fname,
                                         gs_path_control_flag_is_scratch_file);
        if (code >= 0)
            code = gs_add_control_path_flags(mem, gs_permit_file_reading, fname,
                                             gs_path_control_flag_is_scratch_file);
        if (code >= 0)
            code = gs_add_control_path_flags(mem, gs_permit_file_writing, fname,
                                         gs_path_control_flag_is_scratch_file);

        if (code < 0) {
            gp_fclose(file);
            file = NULL;
            /* Call directly through to the unlink implementation. We know
             * we're 'permitted' to do this, but we might not be on all the
             * required permit lists because of the failure. The only bad
             * thing here, is that we're deleting an fname that might not
             * have come from the filing system itself. */
            if (fname && fname[0])
                gp_unlink_impl(ctx->memory, fname);
            (void)gs_remove_control_path_flags(mem, gs_permit_file_control, fname,
                                               gs_path_control_flag_is_scratch_file);
            (void)gs_remove_control_path_flags(mem, gs_permit_file_reading, fname,
                                               gs_path_control_flag_is_scratch_file);
            (void)gs_remove_control_path_flags(mem, gs_permit_file_writing, fname,
                                               gs_path_control_flag_is_scratch_file);
        }
    }

    return file;
}

gp_file *
gp_open_scratch_file(const gs_memory_t *mem,
                     const char        *prefix,
                     char              fname[gp_file_name_sizeof],
                     const char        *mode)
{
    return do_open_scratch_file(mem, prefix, fname, mode, 0);
}

gp_file *
gp_open_scratch_file_rm(const gs_memory_t *mem,
                        const char        *prefix,
                        char              fname[gp_file_name_sizeof],
                        const char        *mode)
{
    return do_open_scratch_file(mem, prefix, fname, mode, 1);
}

int
gp_stat(const gs_memory_t *mem, const char *path, struct stat *buf)
{
    if (gp_validate_path(mem, path, "r") != 0) {
        return -1;
    }

    return gp_stat_impl(mem, path, buf);
}

file_enum *
gp_enumerate_files_init(gs_memory_t *mem, const char *pat, uint patlen)
{
    return gp_enumerate_files_init_impl(mem, pat, patlen);
}

uint
gp_enumerate_files_next(gs_memory_t *mem, file_enum * pfen, char *ptr, uint maxlen)
{
    uint code = 0;

    while (code == 0) {
        code = gp_enumerate_files_next_impl(mem, pfen, ptr, maxlen);
        if (code == ~0) break;
        if (code > 0 && ptr != NULL) {
            if (gp_validate_path_len(mem, ptr, code, "r") != 0)
                code = 0;
        }
    }
    return code;
}
void
gp_enumerate_files_close(gs_memory_t *mem, file_enum * pfen)
{
    gp_enumerate_files_close_impl(mem, pfen);
}

/* Path validation: (FIXME: Move this somewhere better)
 *
 * The only wildcard we accept is '*'.
 *
 * A '*' at the end of the path means "in this directory,
 * or any subdirectory". Anywhere else it means "a sequence of
 * characters not including a director separator".
 *
 * A sequence of multiple '*'s is equivalent to a single one.
 *
 * Matching on '*' is simplistic; the matching sequence will end
 * as soon as we meet an instance of a character that follows
 * the '*' in a pattern. i.e. "foo*bar" will fail to match "fooabbar"
 * as the '*' will be held to match just 'a'.
 *
 * There is no way of specifying a literal '*'; if you find yourself
 * wanting to do this, slap yourself until you come to your senses.
 *
 * Due to the difficulties of using both * and / in writing C comments,
 * I shall use \ as the directory separator in the examples below, but
 * in practice it means "the directory separator for the current
 * platform".
 *
 * Pattern           Match example
 *  *                 any file, in any directory at all.
 *  foo\bar           a file, foo\bar.
 *  foo\bar\          any file within foo\bar\, but no subdirectories.
 *  foo\bar\*         any file within foo\bar\ or any subdirectory thereof.
 *  foo\*\bar         any file 'bar' within any single subdirectory of foo
 *                    (i.e. foo\baz\bar, but not foo\baz\whoop\bar)
 *  foo\out*.tif      e.g. foo\out1.tif
 *  foo\out*.*.tif*   e.g. foo\out1.(Red).tif
 */

static int
validate(const gs_memory_t *mem,
         const char        *path,
         gs_path_control_t  type)
{
    gs_lib_ctx_core_t *core = mem->gs_lib_ctx->core;
    gs_path_control_set_t *control;
    unsigned int i, n;

    switch (type) {
        case gs_permit_file_reading:
            control = &core->permit_reading;
            break;
        case gs_permit_file_writing:
            control = &core->permit_writing;
            break;
        case gs_permit_file_control:
            control = &core->permit_control;
            break;
        default:
            return gs_error_unknownerror;
    }

    n = control->num;
    for (i = 0; i < n; i++) {
        const char *a = path;
        const char *b = control->entry[i].path;
        while (1) {
            if (*a == 0) {
                if (*b == 0)
                    /* PATH=abc pattern=abc */
                    goto found; /* Bingo! */
                else
                    /* PATH=abc pattern=abcd */
                    break; /* No match */
            } else if (*b == '*') {
                if (b[1] == '*') {
                    /* Multiple '*'s are taken to mean the
                     * output from a printf. */
                    b++;
                    while (b[1] == '*')
                        b++;
                    /* Skip over the permissible matching chars */
                    while (*a &&
                           ((*a == ' ' || *a == '-' || *a == '+' ||
                             (*a >= '0' && *a <= '9') ||
                             (*a >= 'a' && *a <= 'f') ||
                             (*a >= 'A' && *a <= 'F'))))
                            a++;
                    if (b[1] == 0 && *a == 0)
                        /* PATH=abc<%d> pattern=abc** */
                        goto found;
                    if (*a == 0)
                        break; /* No match */
                } else {
                    if (b[1] == 0)
                        /* PATH=abc???? pattern=abc* */
                        goto found;
                    /* Skip over anything except NUL, directory
                     * separator, and the next char to match. */
                    while (*a && !gs_file_name_check_separator(a, 1, a) && *a != b[1])
                        a++;
                    if (*a == 0 || *a == gs_file_name_check_separator(a, 1, a))
                        break; /* No match */
                }
                /* Continue matching */
                a--; /* Subtract 1 as the loop will increment it again later */
            } else if (*b == 0) {
                if (b != control->entry[i].path &&
                    gs_file_name_check_separator(b, -1, b)) {
                    const char *a2 = a;
                    const char *aend = path + strlen(path);
                    while (aend != a2 && !gs_file_name_check_separator(a2, 1, a2))
                      a2++;
                    /* If the control path ends in a directory separator and we've scanned
                       to the end of the candidate path with no further directory separators
                       found, we have a match
                     */
                    if (aend == a2)
                      /* PATH=abc/? pattern=abc/ */
                      goto found; /* Bingo! */
                 }
                /* PATH=abcd pattern=abc */
                break; /* No match */
            } else if (gs_file_name_check_separator(a, 1, a) == 1
                        && gs_file_name_check_separator(b, 1, b) == 1) {
                /* On Windows we can get random combinations of "/" and "\" as directory
                 * separators, and we want "C:\" to match C:/" hence using the pair of
                 * gs_file_name_check_separator() calls above */
                 /* Annoyingly, we can also end up with a combination of explicitly escaped
                  * '\' characters, and not escaped. So we also need "C:\\" to match "C:\"
                  * and "C:/" - hence we need to check for, and skip over the
                  * the extra '\' character - I'm reticent to change the upstream code that
                  * adds the explicit escape, because that could have unforeseen side effects
                  * elsewhere. */
                 if (*(a + 1) != 0 && gs_file_name_check_separator(a + 1, 1, a + 1) == 1)
                     a++;
                 if (*(b + 1) != 0 && gs_file_name_check_separator(b + 1, 1, b + 1) == 1)
                     b++;
            } else if (*a != *b) {
                break;
            }
            a++, b++;
        }
    }
    return gs_error_invalidfileaccess;

found:
    return control->entry[i].flags;
}

int
gp_validate_path_len(const gs_memory_t *mem,
                     const char        *path,
                     const uint         len,
                     const char        *mode)
{
    char *buffer, *bufferfull = NULL;
    uint rlen;
    int code = 0;
    const char *cdirstr = gp_file_name_current();
    int cdirstrl = strlen(cdirstr);
    const char *dirsepstr = gp_file_name_separator();
    int dirsepstrl = strlen(dirsepstr);
    int prefix_len = cdirstrl + dirsepstrl;

    /* mem->gs_lib_ctx can be NULL when we're called from mkromfs */
    /* If path == NULL, don't care */
    if (path == NULL || mem->gs_lib_ctx == NULL ||
        /* Can't use gs_is_path_control_active(mem) here because it fails to build with mkromfs */
        mem->gs_lib_ctx->core->path_control_active == 0)
        return 0;

    /* For current directory accesses, we need handle both a "bare" name,
     * and one with a cwd prefix (in Unix terms, both "myfile.ps" and
     * "./myfile.ps".
     *
     * So we check up front if it's absolute, then just use that.
     * If it includes cwd prefix, we try that, then remove the prefix
     * and try again.
     * If it doesn't include the cwd prefix, we try it, then add the
     * prefix and try again.
     * To facilitate that, we allocate a large enough buffer to take
     * the path *and* the prefix up front.
     */
    if (gp_file_name_is_absolute(path, len)) {
       /* Absolute path, we don't need anything extra */
       prefix_len = cdirstrl = dirsepstrl = 0;
    }
    else if (len > prefix_len && !memcmp(path, cdirstr, cdirstrl)
             && !memcmp(path + cdirstrl, dirsepstr, dirsepstrl)) {
          prefix_len = 0;
    }

    /* "%pipe%" do not follow the normal rules for path definitions, so we
       don't "reduce" them to avoid unexpected results
     */
    if (path[0] == '|' || (len > 5 && memcmp(path, "%pipe", 5) == 0)) {
        bufferfull = buffer = (char *)gs_alloc_bytes(mem->thread_safe_memory, len + 1, "gp_validate_path");
        if (buffer == NULL)
            return gs_error_VMerror;
        memcpy(buffer, path, len);
        buffer[len] = 0;
        rlen = len;
    }
    else {
        char *test = (char *)path, *test1;
        uint tlen = len, slen;
        gp_file_name_combine_result res;

        /* Look for any pipe (%pipe% or '|' specifications between path separators
         * Reject any path spec which has a %pipe% or '|' anywhere except at the start.
         */
        while (tlen > 0) {
            if (test[0] == '|' || (tlen > 5 && memcmp(test, "%pipe", 5) == 0)) {
                code = gs_note_error(gs_error_invalidfileaccess);
                goto exit;
            }
            test1 = test;
            slen = search_separator((const char **)&test, path + len, test1, 1);
            if(slen == 0)
                break;
            test += slen;
            tlen -= test - test1;
            if (test >= path + len)
                break;
        }

        rlen = len+1;
        /* There is a very, very small chance that gp_file_name_reduce() can return a longer
           string than simply rlen + prefix_len. The do/while loop is to cope with that.
         */
        do {
            bufferfull = (char *)gs_alloc_bytes(mem->thread_safe_memory, rlen + prefix_len, "gp_validate_path");
            if (bufferfull == NULL)
                return gs_error_VMerror;

            buffer = bufferfull + prefix_len;
            res = gp_file_name_reduce(path, (uint)len, buffer, &rlen);
            if (res == gp_combine_small_buffer) {
                rlen += 1;
                gs_free_object(mem->thread_safe_memory, bufferfull, "gp_validate_path");
                rlen += 1;
                continue;
            }
            if (res != gp_combine_success) {
                code = gs_note_error(gs_error_invalidfileaccess);
                goto exit;
            }
        } while (res != gp_combine_success);
        buffer[rlen] = 0;
    }
    while (1) {
        switch (mode[0])
        {
        case 'r': /* Read */
            code = validate(mem, buffer, gs_permit_file_reading);
            break;
        case 'w': /* Write */
            code = validate(mem, buffer, gs_permit_file_writing);
            break;
        case 'a': /* Append needs reading and writing */
            code = (validate(mem, buffer, gs_permit_file_reading) |
                    validate(mem, buffer, gs_permit_file_writing));
            break;
        case 'c': /* "Control" */
            code =  validate(mem, buffer, gs_permit_file_control);
            break;
        case 'd': /* "Delete" (special case of control) */
            code =  validate(mem, buffer, gs_permit_file_control);
            break;
        case 'f': /* "Rename from" */
            code = (validate(mem, buffer, gs_permit_file_writing) |
                    validate(mem, buffer, gs_permit_file_control));
            break;
        case 't': /* "Rename to" */
            code = (validate(mem, buffer, gs_permit_file_writing) |
                    validate(mem, buffer, gs_permit_file_control));
            break;
        default:
            errprintf(mem, "gp_validate_path: Unknown mode='%s'\n", mode);
            code = gs_note_error(gs_error_invalidfileaccess);
        }
        if (code < 0 && prefix_len > 0 && buffer > bufferfull) {
            uint newlen = rlen + cdirstrl + dirsepstrl;
            char *newbuffer;
            int code;

            buffer = bufferfull;
            memcpy(buffer, cdirstr, cdirstrl);
            memcpy(buffer + cdirstrl, dirsepstr, dirsepstrl);

            /* We've prepended a './' or similar for the current working directory. We need
             * to execute file_name_reduce on that, to eliminate any '../' or similar from
             * the (new) full path.
             */
            newbuffer = (char *)gs_alloc_bytes(mem->thread_safe_memory, newlen + 1, "gp_validate_path");
            if (newbuffer == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto exit;
            }

            memcpy(newbuffer, buffer, rlen + cdirstrl + dirsepstrl);
            newbuffer[newlen] = 0x00;

            code = gp_file_name_reduce(newbuffer, (uint)newlen, buffer, &newlen);
            gs_free_object(mem->thread_safe_memory, newbuffer, "gp_validate_path");
            if (code != gp_combine_success) {
                code = gs_note_error(gs_error_invalidfileaccess);
                goto exit;
            }

            continue;
        }
        else if (code < 0 && cdirstrl > 0 && prefix_len == 0 && buffer == bufferfull
            && memcmp(buffer, cdirstr, cdirstrl) && !memcmp(buffer + cdirstrl, dirsepstr, dirsepstrl)) {
            continue;
        }
        break;
    }
    if (code > 0 && (mode[0] == 'd' || mode[0] == 'f') &&
        (code & gs_path_control_flag_is_scratch_file) != 0) {
        (void)gs_remove_control_path_flags(mem, gs_permit_file_reading, buffer,
                                           gs_path_control_flag_is_scratch_file);
        (void)gs_remove_control_path_flags(mem, gs_permit_file_writing, buffer,
                                           gs_path_control_flag_is_scratch_file);
        (void)gs_remove_control_path_flags(mem, gs_permit_file_control, buffer,
                                           gs_path_control_flag_is_scratch_file);
    }

exit:
    gs_free_object(mem->thread_safe_memory, bufferfull, "gp_validate_path");
#ifdef EACCES
    if (code == gs_error_invalidfileaccess)
        errno = EACCES;
#endif

    return code < 0 ? code : 0;
}

int
gp_validate_path(const gs_memory_t *mem,
                 const char        *path,
                 const char        *mode)
{
    return gp_validate_path_len(mem, path, strlen(path), mode);
}

int
gp_unlink(gs_memory_t *mem, const char *fname)
{
    if (gp_validate_path(mem, fname, "d") != 0)
        return gs_error_invalidaccess;

    return gp_unlink_impl(mem, fname);
}

int
gp_rename(gs_memory_t *mem, const char *from, const char *to)
{
    /* Always check 'to' before 'from', in case 'from' is a tempfile,
     * and testing it might remove it from the list! */
    if (gp_validate_path(mem, to, "t") != 0)
        return gs_error_invalidaccess;
    if (gp_validate_path(mem, from, "f") != 0)
        return gs_error_invalidaccess;

    return gp_rename_impl(mem, from, to);
}
