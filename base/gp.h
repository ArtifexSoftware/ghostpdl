/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Interface to platform-specific routines */
/* Requires gsmemory.h */

#ifndef gp_INCLUDED
#  define gp_INCLUDED

#include "stat_.h"
#include "gstypes.h"
#include "gscdefs.h"		/* for gs_serialnumber */
/*
 * This file defines the interface to ***ALL*** platform-specific routines,
 * with the exception of the thread/synchronization interface (gpsync.h)
 * and getenv (gpgetenv.h).  The implementations are in gp_*.c files
 * specific to each platform.  We try very hard to keep this list short!
 */
/*
 * gp_getenv is declared in a separate file because a few places need it
 * and don't want to include any of the other gs definitions.
 */
#include "gpgetenv.h"
/*
 * The prototype for gp_readline is in srdline.h, since it is shared with
 * stream.h.
 */
#include "srdline.h"

/*
 * int64_t is used in the 64 bits file access.
 */
#include "stdint_.h"

#include "memory_.h"

/* ------ Initialization/termination ------ */

/*
 * This routine is called early in the initialization.
 * It should do as little as possible.  In particular, it should not
 * do things like open display connections: that is the responsibility
 * of the display device driver.
 */
void gp_init(void);

/*
 * This routine is called just before the program exits (normally or
 * abnormally).  It too should do as little as possible.
 */
void gp_exit(int exit_status, int code);

/*
 * Exit the program.  Normally this just calls the `exit' library procedure,
 * but it does something different on a few platforms.
 */
void gp_do_exit(int exit_status);

/* ------ Miscellaneous ------ */

/*
 * Get the string corresponding to an OS error number.
 * If no string is available, return NULL.  The caller may assume
 * the string is allocated statically and permanently.
 */
const char *gp_strerror(int);

/*
 * Get the system default paper size, which is usually letter for
 * countries using the imperial system, and a4 for countries
 * using the metric system.
 *
 * If there is no default paper size, set *ptr = 0 (if *plen > 0),
 * set *plen = 1, and return 1.
 *
 * If there is a default paper size and the length len of the value
 * (not counting the terminating \0) is less than *plen,
 * copy the value to ptr, set *plen = len + 1, and return 0.
 *
 * If there is a default paper size and len >= *plen, set *plen = len + 1,
 * don't store anything at ptr, and return -1.
 *
 * Note that *plen is the size of the buffer, not the length of the string:
 * because of the terminating \0, the maximum string length is 1 less than
 * the size of the buffer.
 *
 * The use of ptr and plen as described above are the same as gp_getenv.
 */
int gp_defaultpapersize(char *ptr, int *plen);

/*
 * Return a serialnumber. Clients that want to can modify the appropriate
 * gp_***.c file(s) for their platform and Digital Rights Management (DRM)
 * of choice. Default handlers for common platforms use info from the OS
 * and unsupported or old platforms simply return GS_SERIALNUMBER.
 */
int gp_serialnumber(void);

/* ------ Date and time ------ */

/*
 * Read the current time (in seconds since an implementation-defined epoch)
 * into ptm[0], and fraction (in nanoseconds) into ptm[1].
 */
void gp_get_realtime(long *ptm);

/*
 * Read the current user CPU time (in seconds) into ptm[0],
 * and fraction (in nanoseconds) into ptm[1].
 */
void gp_get_usertime(long *ptm);

/* ------ Reading lines from stdin ------ */

/*
 * These routines are intended to provide an abstract interface to GNU
 * readline or to other packages that offer enhanced line-reading
 * capability.
 */

/*
 * Allocate a state structure for line reading.  This is called once at
 * initialization.  *preadline_data is an opaque pointer that is passed
 * back to gp_readline and gp_readline_finit.
 */
int gp_readline_init(void **preadline_data, gs_memory_t *mem);

/*
 * See srdline.h for the definition of sreadline_proc.
 */
int gp_readline(stream *s_in, stream *s_out, void *readline_data,
                gs_const_string *prompt, gs_string *buf,
                gs_memory_t *bufmem, uint *pcount, bool *pin_eol,
                bool (*is_stdin)(const stream *));

/*
 * Free a readline state.
 */
void gp_readline_finit(void *readline_data);

/* ------ File access functions ------ */

typedef struct {
    int          (*close)(gp_file *);
    int          (*getc)(gp_file *);
    int          (*putc)(gp_file *, int);
    int          (*read)(gp_file *, size_t size, unsigned int count, void *buf);
    int          (*write)(gp_file *, size_t size, unsigned int count, const void *buf);
    int          (*seek)(gp_file *, gs_offset_t offset, int whence);
    gs_offset_t  (*tell)(gp_file *);
    int          (*eof)(gp_file *);
    gp_file     *(*dup)(gp_file *, const char *mode);
    int          (*seekable)(gp_file *);
    int          (*pread)(gp_file *, size_t count, gs_offset_t offset, void *buf);
    int          (*pwrite)(gp_file *, size_t count, gs_offset_t offset, const void *buf);
    int          (*is_char_buffered)(gp_file *file);
    void         (*fflush)(gp_file *file);
    int          (*ferror)(gp_file *file);
    FILE        *(*get_file)(gp_file *file);
    void         (*clearerr)(gp_file *file);
    gp_file     *(*reopen)(gp_file *f, const char *fname, const char *mode);
} gp_file_ops_t;

struct gp_file_s {
    gp_file_ops_t ops;

    gs_memory_t *memory;
    char        *buffer;
    int          buffer_size;
};

/* Implementers of gp_file instances will need to provide
 * implementations of the above function pointers, together with
 * one (or more) functions to create gp_file instances. We
 * provide the following convenience functions to aid in this:
 */

/* Allocate a structure based on a gp_file, initialise it with the
 * given prototype, and zero the rest of it. Returns NULL on failure
 * to allocate. */
gp_file *gp_file_alloc(gs_memory_t *mem, const gp_file_ops_t *prototype, size_t size, const char *cname);

/* Called automatically by gp_fclose. May be needed for implementers to
 * clear up allocations if errors occur while opening files. */
void gp_file_dealloc(gp_file *file);

/* Called to allocate a gp_file based on a FILE * (yet to be
 * opened). Returns NULL on failure to allocate. */
gp_file *gp_file_FILE_alloc(const gs_memory_t *mem);

/* Called to set a newly opened FILE * into a gp_file previously
 * allocated by gp_file_FILE_alloc. If f is NULL, then the
 * allocation is taken to have failed, file is freed, and the
 * function returns 1. Otherwise, f is recorded within file and
 * the function returns 0. */
int gp_file_FILE_set(gp_file *file, FILE *f, int (*close)(FILE *));


/* The actual API that people call to use gp_files: */

static inline int
gp_fgetc(gp_file *f) {
    return (f->ops.getc)(f);
}

static inline int
gp_fputc(int c, gp_file *f) {
    return (f->ops.putc)(f, c);
}

static inline int
gp_fread(void *buf, size_t size, size_t count, gp_file *f) {
    return (f->ops.read)(f, size, count, buf);
}

static inline int
gp_fwrite(const void *buf, size_t size, size_t count, gp_file *f) {
    return (f->ops.write)(f, size, count, buf);
}

static inline int
gp_fseek(gp_file *f, gs_offset_t offset, int whence) {
    if (f->ops.seek == NULL)
        return -1;
    return (f->ops.seek)(f, offset, whence);
}

static inline gs_offset_t
gp_ftell(gp_file *f) {
    if (f->ops.tell == NULL)
        return -1;
    return (f->ops.tell)(f);
}

static inline int
gp_feof(gp_file *f) {
    return f->ops.eof(f);
}

static inline int
gp_fclose(gp_file *f) {
    int ret = (f->ops.close)(f);
    gp_file_dealloc(f);
    return ret;
}

static inline gp_file *
gp_fdup(gp_file *f, const char *mode) {
    if (f == NULL || f->ops.dup == NULL)
        return NULL;

    return (f->ops.dup)(f, mode);
}

static inline int
gp_fseekable(gp_file *f) {
    if (f == NULL || f->ops.seekable == NULL)
        return 0;
    return f->ops.seekable(f);
}

static inline int
gp_fpread(void *buf, size_t count, gs_offset_t offset, gp_file *f) {
    return (f->ops.pread)(f, count, offset, buf);
}

static inline int
gp_fpwrite(void *buf, size_t count, gs_offset_t offset, gp_file *f) {
    return (f->ops.pwrite)(f, count, offset, buf);
}

static inline int
gp_file_is_char_buffered(gp_file *f) {
    if (f->ops.is_char_buffered == NULL)
        return 0;
    return (f->ops.is_char_buffered)(f);
}

static inline void
gp_fflush(gp_file *f) {
    if (f->ops.fflush)
        (f->ops.fflush)(f);
}

static inline int
gp_ferror(gp_file *f) {
    return (f->ops.ferror)(f);
}

static inline FILE *
gp_get_file(gp_file *f) {
    if (f->ops.get_file == NULL)
        return NULL;
    return (f->ops.get_file)(f);
}

static inline void
gp_clearerr(gp_file *f) {
    if (f->ops.clearerr)
        (f->ops.clearerr)(f);
}

/* fname is always in utf8 format */
static inline gp_file *
gp_freopen(const char *fname, const char *mode, gp_file *f) {
    if (f->ops.reopen == NULL)
        return NULL;
    return (f->ops.reopen)(f, fname, mode);
}

static inline int
gp_fputs(const char *string, gp_file *f) {
    size_t len = strlen(string);
    return (f->ops.write)(f, 1, len, string);
}

static inline int
gp_rewind(gp_file *f) {
    return gp_fseek(f, 0, SEEK_SET);
}

char *gp_fgets(char *buffer, size_t n, gp_file *f);

int gp_fprintf(gp_file *f, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (__printf__, 2, 3)))
#endif
    ;

/* ------ Reading from stdin, unbuffered if possible ------ */

/* Read bytes from stdin, using unbuffered if possible.
 * Store up to len bytes in buf.
 * Returns number of bytes read, or 0 if EOF, or -ve if error.
 * If unbuffered is NOT possible, fetch 1 byte if interactive
 * is non-zero, or up to len bytes otherwise.
 * If unbuffered is possible, fetch at least 1 byte (unless error or EOF)
 * and any additional bytes that are available without blocking.
 */
int gp_stdin_read(char *buf, int len, int interactive, FILE *f);

/* ------ Screen management ------ */

/*
 * The following are only relevant for X Windows.
 */

/* Get the environment variable that specifies the display to use. */
const char *gp_getenv_display(void);

/* ------ File naming and accessing ------ */

/*
 * Define the maximum size of a file name returned by gp_open_scratch_file
 * or gp_open_printer.  (This should really be passed as an additional
 * parameter, but it would break too many clients to make this change now.)
 * Note that this is the size of the buffer, not the maximum number of
 * characters: the latter is one less, because of the terminating \0.
 *
 * This used to be 260, the same as the MAX_PATH value on Windows,
 * but although MAX_PATH still exists on Windows, it is no longer
 * the maximum length of a path - doh??
 * We now use 4k as a reasonable limit for most environments.
 */
#define gp_file_name_sizeof 4096

/* Define the character used for separating file names in a list. */
extern const char gp_file_name_list_separator;

/* Define the default scratch file name prefix. */
extern const char gp_scratch_file_name_prefix[];

/* Define the name of the null output file. */
extern const char gp_null_file_name[];

/* Define the name that designates the current directory. */
extern const char gp_current_directory_name[];

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
/* This is always either "" or "b". */
extern const char* gp_fmode_binary_suffix;

/* Define the file modes for binary reading or writing. */
/* (This is just a convenience: they are "r" or "w" + the suffix.) */
extern const char gp_fmode_rb[];
extern const char gp_fmode_wb[];

/**
 * gp_open_scratch_file: Create a scratch file.
 * @mem: Memory pointer
 * @prefix: Name prefix.
 * @fname: Where to store filename of newly created file (utf8).
 * @mode: File access mode (in fopen syntax).
 *
 * Creates a scratch (temporary) file in the filesystem. The exact
 * location and name of the file is platform dependent, but in general
 * uses @prefix as a prefix. If @prefix is not absolute, then choose
 * an appropriate system directory, usually as determined from
 * gp_gettmpdir(), followed by a path as returned from a system call.
 *
 * Return value: Opened file object, or NULL on error.
 **/
gp_file *gp_open_scratch_file(const gs_memory_t *mem,
                              const char        *prefix,
                                    char         fname[gp_file_name_sizeof],
                              const char        *mode);

/* Open a file with the given (utf8) name, as a stream of uninterpreted
 * bytes. */
gp_file *gp_fopen(const gs_memory_t *mem, const char *fname, const char *mode);

/* Create a self-deleting scratch file (utf8) */
gp_file *gp_open_scratch_file_rm(const gs_memory_t *mem,
                                 const char        *prefix,
                                       char         fname[gp_file_name_sizeof],
                                 const char        *mode);

/* gp_stat is defined in stat_.h rather than here due to macro problems */

typedef enum {
    gp_combine_small_buffer = -1,
    gp_combine_cant_handle = 0,
    gp_combine_success = 1
} gp_file_name_combine_result;

/*
 * Combine a file name with a prefix.
 * Concatenates two paths and reduce parten references and current
 * directory references from the concatenation when possible.
 * The trailing zero byte is being added.
 * Various platforms may share this code.
 */
gp_file_name_combine_result gp_file_name_combine(const char *prefix, uint plen,
            const char *fname, uint flen, bool no_sibling, char *buffer, uint *blen);

/* -------------- Helpers for gp_file_name_combine_generic ------------- */
/* Platforms, which do not call gp_file_name_combine_generic, */
/* must stub the helpers against linkage problems. */

/* Return length of root prefix of the file name, or zero. */
/*	unix:   length("/")	    */
/*	Win:    length("c:/") or length("//computername/cd:/")  */
/*	mac:	length("volume:")    */
/*	VMS:	length("device:[root.]["	    */
uint gp_file_name_root(const char *fname, uint len);

/* Check whether a part of file name starts (ends) with a separator. */
/* Must return the length of the separator.*/
/* If the 'len' is negative, must search in backward direction. */
/*	unix:   '/'	    */
/*	Win:    '/' or '\'  */
/*	mac:	':' except "::"	    */
/*	VMS:	smart - see the implementation   */
uint gs_file_name_check_separator(const char *fname, int len, const char *item);

/* Check whether a part of file name is a parent reference. */
/*	unix, Win:  equal to ".."	*/
/*	mac:	equal to ":"		*/
/*	VMS:	equal to "."		*/
bool gp_file_name_is_parent(const char *fname, uint len);

/* Check if a part of file name is a current directory reference. */
/*	unix, Win:  equal to "."	*/
/*	mac:	equal to ""		*/
/*	VMS:	equal to ""		*/
bool gp_file_name_is_current(const char *fname, uint len);

/* Returns a string for referencing the current directory. */
/*	unix, Win:  "."	    */
/*	mac:	":"	    */
/*	VMS:	""          */
const char *gp_file_name_current(void);

/* Returns a string for separating a file name item. */
/*	unix, Win:  "/"	    */
/*	mac:	":"	    */
/*	VMS:	"]"	    */
const char *gp_file_name_separator(void);

/* Returns a string for separating a directory item. */
/*	unix, Win:  "/"	    */
/*	mac:	":"	    */
/*	VMS:	"."	    */
const char *gp_file_name_directory_separator(void);

/* Returns a string for representing a parent directory reference. */
/*	unix, Win:  ".."    */
/*	mac:	":"	    */
/*	VMS:	"."	    */
const char *gp_file_name_parent(void);

/* Answer whether the platform allows parent refenences. */
/*	unix, Win, Mac: yes */
/*	VMS:	no.         */
bool gp_file_name_is_parent_allowed(void);

/* Answer whether an empty item is meanful in file names on the platform. */
/*	unix, Win:  no	    */
/*	mac:	yes	    */
/*	VMS:	yes         */
bool gp_file_name_is_empty_item_meanful(void);

/* Read a 'resource' stored in a special database indexed by a 32 bit  */
/* 'type' and 16 bit 'id' in an extended attribute of a file. The is   */
/* primarily for accessing fonts on MacOS, which classically used this */
/* format. Presumedly a 'nop' on systems that don't support Apple HFS. */
int gp_read_macresource(byte *buf, const char *fname,
                                     const uint type, const ushort id);

/* Returns true when the character can be used in the file name. */
bool gp_file_name_good_char(unsigned char c);

/* ------ Printer accessing ------ */

/*
 * Open a connection to a printer.  A null file name means use the standard
 * printer connected to the machine, if any.  Note that this procedure is
 * called only if the original file name (normally the value of the
 * OutputFile device parameter) was an ordinary file (not stdout, a pipe, or
 * other %filedevice%file name): stdout is handled specially, and other
 * values of filedevice are handled by calling the fopen procedure
 * associated with that kind of "file".
 *
 * Note that if the file name is null (0-length) and a default printer is
 * available, the file name may be replaced with the name of a scratch file
 * for spooling.  If the file name is null and no default printer is
 * available, this procedure returns 0.
 */
gp_file *gp_open_printer(const gs_memory_t *mem,
                               char         fname[gp_file_name_sizeof],
                               int          binary_mode);

/*
 * Close the connection to the printer.  Note that this is only called
 * if the original file name was an ordinary file (not stdout, a pipe,
 * or other %filedevice%file name): stdout is handled specially, and other
 * values of filedevice are handled by calling the fclose procedure
 * associated with that kind of "file".
 */
void gp_close_printer(      gp_file *pfile,
                      const char    *fname);

/* ------ File enumeration ------ */

typedef struct file_enum_s file_enum;

/*
 * Begin an enumeration.  pat is a C string that may contain *s or ?s.
 * The implementor should copy the string to a safe place.
 * If the operating system doesn't support correct, arbitrarily placed
 * *s and ?s, the implementation should modify the string so that it
 * will return a conservative superset of the request, and then use
 * the string_match procedure to select the desired subset.  E.g., if the
 * OS doesn't implement ? (single-character wild card), any consecutive
 * string of ?s should be interpreted as *.  Note that \ can appear in
 * the pattern also, as a quoting character.
 */
file_enum *gp_enumerate_files_init(gs_memory_t *memory, const char *pat, uint patlen);

/*
 * Return the next file name in the enumeration.  The client passes in
 * a scratch string and a max length.  If the name of the next file fits,
 * the procedure returns the length.  If it doesn't fit, the procedure
 * returns max length +1.  If there are no more files, the procedure
 * returns -1.
 */
uint gp_enumerate_files_next(gs_memory_t *memory, file_enum * pfen, char *ptr, uint maxlen);

/*
 * Clean up a file enumeration.  This is only called to abandon
 * an enumeration partway through: ...next should do it if there are
 * no more files to enumerate.  This should deallocate the file_enum
 * structure and any subsidiary structures, strings, buffers, etc.
 */
void gp_enumerate_files_close(gs_memory_t *memory, file_enum * pfen);

/* ------ Font enumeration ------ */

/* This is used to query the native os for a list of font names and
 * corresponding paths. The general idea is to save the hassle of
 * building a custom fontmap file
 */

/* allocate and initialize the iterator
   returns a pointer to its local state or NULL on failure */
void *gp_enumerate_fonts_init(gs_memory_t *mem);

/* get the next element in the font enumeration
   Takes a pointer to its local state and pointers in which to
   return C strings. The string 'name' is the font name, 'path'
   is the access path for the font resource. The returned strings
   are only safe to reference until until the next call.
   Returns 0 when no more fonts are available, a positive value
   on success, or negative value on error. */
int gp_enumerate_fonts_next(void *enum_state, char **fontname, char **path);

/* clean up and deallocate the iterator */
void gp_enumerate_fonts_free(void *enum_state);

/* ----- Platform specific file operations ----- */

/* The following functions are implemented by each platform, and
 * never called directly by devices or core code (only by the
 * gp_file functions themselves). */

FILE *gp_fopen_impl(gs_memory_t *mem, const char *fname, const char *mode);

FILE *gp_fdup_impl(FILE *f, const char *mode);

int gp_pread_impl(char *buf, size_t count, gs_offset_t offset, FILE *f);

int gp_pwrite_impl(const char *buf, size_t count, gs_offset_t offset, FILE *f);

gs_offset_t gp_ftell_impl(FILE *f);

int gp_fseek_impl(FILE *strm, gs_offset_t offset, int origin);

/* Create a second open gp_file on the basis of a given one */
FILE *gp_fdup_impl(FILE *f, const char *mode);

int gp_fseekable_impl(FILE *f);

/* Force given file into binary mode (no eol translations, etc) */
/* if 2nd param true, text mode if 2nd param false */
int gp_setmode_binary_impl(FILE * pfile, bool mode);

FILE *
gp_open_printer_impl(gs_memory_t *mem,
                     const char  *fname,
                     int         *binary_mode,
                     int          (**close)(FILE *));

/* Create a scratch file (utf8) (self-deleting if remove) */
FILE *gp_open_scratch_file_impl(const gs_memory_t *mem,
                                const char        *prefix,
                                      char         fname[gp_file_name_sizeof],
                                const char        *mode,
                                      int          remove);

/* Test whether this platform supports the sharing of file descriptors */
int gp_can_share_fdesc(void);

int gp_stat_impl(const gs_memory_t *mem, const char *path, struct stat *buf);

file_enum *gp_enumerate_files_init_impl(gs_memory_t *memory, const char *pat, uint patlen);

uint gp_enumerate_files_next_impl(gs_memory_t *memory, file_enum * pfen, char *ptr, uint maxlen);

void gp_enumerate_files_close_impl(gs_memory_t *memory, file_enum * pfen);

/* We don't define gp_fread_64, gp_fwrite_64,
   because (1) known platforms allow regular fread, fwrite
   to be applied to a file opened with O_LARGEFILE,
   fopen64, etc.; (2) Ghostscript code does not
   perform writing/reading a long (over 4gb) block
   in one operation.
 */

/* Some platforms (currently only windows) may supply a function to convert
 * characters from an encoded command line arg from the local encoding into
 * a unicode codepoint. Returns EOF for end of file (or string).
 */
int
gp_local_arg_encoding_get_codepoint(gp_file *file, const char **astr);

int
gp_xpsprint(char *filename, char *printername, int *result);

int
gp_validate_path_len(const gs_memory_t *mem,
                 const char        *path,
                 const uint         rlen,
                 const char        *mode);

int
gp_validate_path(const gs_memory_t *mem,
                 const char        *path,
                 const char        *mode);

#endif /* gp_INCLUDED */
