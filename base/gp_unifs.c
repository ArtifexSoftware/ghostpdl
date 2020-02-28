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


/* "Unix-like" file system platform routines for Ghostscript */

/* prevent gp.h from defining fopen */
#define fopen fopen

#include "stdio_.h"             /* for FILENAME_MAX */
#include "memory_.h"
#include "string_.h"
#include "gx.h"

#include "gp.h"
#include "gpmisc.h"
#include "gsstruct.h"
#include "gsutil.h"             /* for string_match */
#include "stat_.h"
#include "dirent_.h"
#include "unistd_.h"
#include <stdlib.h>             /* for mkstemp/mktemp */

#if !defined(HAVE_FSEEKO)
#define ftello ftell
#define fseeko fseek
#define ftello64 ftell
#define fseeko64 fseek
#endif

/* Provide a definition of the maximum path length in case the system
 * headers don't define it. This should be gp_file_name_sizeof from
 * gp.h once that value is properly sent in a system-dependent way.
 * HP-UX 11i 11.11 incorrectly defines FILENAME_MAX as 14.
 */
#ifdef FILENAME_MAX
#  if FILENAME_MAX < 80  /* arbitrary */
#    undef FILENAME_MAX
#  endif
#endif
#ifndef FILENAME_MAX
#  define FILENAME_MAX 1024
#endif

/* Library routines not declared in a standard header */
extern char *mktemp(char *);

/* ------ File naming and accessing ------ */

/* Define the default scratch file name prefix. */
const char gp_scratch_file_name_prefix[] = "gs_";

/* Define the name of the null output file. */
const char gp_null_file_name[] = "/dev/null";

/* Define the name that designates the current directory. */
const char gp_current_directory_name[] = ".";

/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */
FILE *
gp_open_scratch_file_impl(const gs_memory_t *mem,
                          const char        *prefix,
                                char         fname[gp_file_name_sizeof],
                          const char        *mode,
                                int          remove)
{       /* The -8 is for XXXXXX plus a possible final / and -. */
#ifdef GS_NO_FILESYSTEM
    return NULL;
#else
    int prefix_length = strlen(prefix);
    int len = gp_file_name_sizeof - prefix_length - 8;
    FILE *fp;

    if (gp_file_name_is_absolute(prefix, prefix_length))
        *fname = 0;
    else if (gp_gettmpdir(fname, &len) != 0)
        strcpy(fname, "/tmp/");
    else {
        if (strlen(fname) != 0 && fname[strlen(fname) - 1] != '/')
            strcat(fname, "/");
    }
    if (strlen(fname) + prefix_length + 8 >= gp_file_name_sizeof)
        return 0;               /* file name too long */
    strcat(fname, prefix);
    /* Prevent trailing X's in path from being converted by mktemp. */
    if (*fname != 0 && fname[strlen(fname) - 1] == 'X')
        strcat(fname, "-");
    strcat(fname, "XXXXXX");

#ifdef HAVE_MKSTEMP
    {
        int file;
        char ofname[gp_file_name_sizeof];

        /* save the old filename template in case mkstemp fails */
        memcpy(ofname, fname, gp_file_name_sizeof);
#  ifdef HAVE_MKSTEMP64
        file = mkstemp64(fname);
#  else
        file = mkstemp(fname);
#  endif
        if (file < -1) {
            emprintf1(mem, "**** Could not open temporary file %s\n", ofname);
            return NULL;
        }
#  if defined(O_LARGEFILE) && defined(__hpux)
        fcntl(file, F_SETFD, fcntl(file, F_GETFD) | O_LARGEFILE);
#  else
        /* Fixme : what to do with b64 and 32-bit mkstemp? Unimplemented. */
#  endif

        fp = fdopen(file, mode);
        if (fp == NULL) {
	    close(file);
	}
    }
#else
    /* Coverity thinks that any use of mktemp() is insecure. But if we
    reach here then there is no mkstemp() alternative available, so there's
    not much we can do. Haven't been able to disable this - e.g. '//
    coverity[SECURE_TEMP]' doesn't have any affect. */
    mktemp(fname);
    fp = gp_fopentemp(fname, mode);
#endif
    if (fp == NULL)
        emprintf1(mem, "**** Could not open temporary file %s\n", fname);

    if (remove)
        unlink(fname);

    return fp;
#endif
}

/* Open a file with the given name, as a stream of uninterpreted bytes. */
FILE *
gp_fopen_impl(gs_memory_t *mem, const char *fname, const char *mode)
{
#if defined(HAVE_FILE64)
    return fopen64(fname, mode);
#else
    return fopen(fname, mode);
#endif
}

int gp_stat_impl(const gs_memory_t *mem, const char *path, struct stat *buf)
{
    return stat(path, buf);
}

int gp_can_share_fdesc(void)
{
#if defined(HAVE_PREAD_PWRITE) && HAVE_PREAD_PWRITE == 1
    return 1;
#else
    return 0;	/* can't share FILE * descriptors w/o pread due to seek..read..seek */
#endif
}

FILE *gp_fdup_impl(FILE *f, const char *mode)
{
#ifdef GS_NO_FILESYSTEM
    return NULL;
#else
    int fd = fileno(f);
    if (fd < 0)
        return NULL;
    fd = dup(fd);
    if (fd < 0)
        return NULL;
    return fdopen(fd, mode);
#endif
}

int gp_pread_impl(char *buf, size_t count, gs_offset_t offset, FILE *f)
{
#ifdef GS_NO_FILESYSTEM
    return 0;
#elif defined(HAVE_PREAD_PWRITE) && HAVE_PREAD_PWRITE == 1
    return pread(fileno(f), buf, count, offset);
#else
    int c;
    int64_t os, curroff = gp_ftell_impl(f);
    if (curroff < 0) return curroff;

    os = gp_fseek_impl(f, offset, 0);
    if (os < 0) return os;

    c = fread(buf, 1, count, f);
    if (c < 0) return c;

    os = gp_fseek_impl(f, curroff, 0);
    if (os < 0) return os;

    return c;
#endif
}

int gp_pwrite_impl(const char *buf, size_t count, gs_offset_t offset, FILE *f)
{
#ifdef GS_NO_FILESYSTEM
    return 0;
#elif defined(HAVE_PREAD_PWRITE) && HAVE_PREAD_PWRITE == 1
    return pwrite(fileno(f), buf, count, offset);
#else
    int c;
    int64_t os, curroff = gp_ftell_impl(f);
    if (curroff < 0) return curroff;

    os = gp_fseek_impl(f, offset, 0);
    if (os < 0) return os;

    c = fwrite(buf, 1, count, f);
    if (c < 0) return c;

    os = gp_fseek_impl(f, curroff, 0);
    if (os < 0) return os;

    return c;
#endif
}

/* Set a file into binary or text mode. */
int
gp_setmode_binary_impl(FILE * pfile, bool mode)
{
    return 0;                   /* Noop under Unix */
}

/* ------ File enumeration ------ */

/* Thanks to Fritz Elfert (Fritz_Elfert@wue.maus.de) for */
/* the original version of the following code, and Richard Mlynarik */
/* (mly@adoc.xerox.com) for an improved version. */

#ifdef GS_NO_FILESYSTEM
struct file_enum_s {
    int dummy;
};

static file_enum dummy_enum;
#else
typedef struct dirstack_s dirstack;
struct dirstack_s {
    dirstack *next;
    DIR *entry;
};

gs_private_st_ptrs1(st_dirstack, dirstack, "dirstack",
                    dirstack_enum_ptrs, dirstack_reloc_ptrs, next);

struct file_enum_s {
    DIR *dirp;                  /* pointer to current open directory   */
    char *pattern;              /* original pattern                    */
    char *work;                 /* current path                        */
    int worklen;                /* strlen (work)                       */
    dirstack *dstack;           /* directory stack                     */
    int patlen;
    int pathead;                /* how much of pattern to consider
                                 *  when listing files in current directory */
    bool first_time;
    gs_memory_t *memory;
};
gs_private_st_ptrs3(st_file_enum, struct file_enum_s, "file_enum",
          file_enum_enum_ptrs, file_enum_reloc_ptrs, pattern, work, dstack);

/* Private procedures */

/* Do a wild-card match. */
#ifdef DEBUG
static bool
wmatch(const byte * str, uint len, const byte * pstr, uint plen,
       const gs_memory_t *mem)
{
    bool match = string_match(str, len, pstr, plen, NULL);

    if (gs_debug_c('e')) {
        int i;
        dmlputs(mem, "[e]string_match(\"");
        for (i=0; i<len; i++)
            errprintf(mem, "%c", str[i]);
        dmputs(mem, "\", \"");
        for (i=0; i<plen; i++)
            errprintf(mem, "%c", pstr[i]);
        dmprintf1(mem, "\") = %s\n", (match ? "TRUE" : "false"));
    }
    return match;
}
#else
#define wmatch(S,L,PS,PL,M) string_match(S,L,PS,PL,NULL)
#endif

/* Search a string backward for a character. */
/* (This substitutes for strrchr, which some systems don't provide.) */
static char *
rchr(char *str, char ch, int len)
{
    register char *p = str + len;

    while (p > str)
        if (*--p == ch)
            return p;
    return 0;
}

/* Pop a directory from the enumeration stack. */
static bool
popdir(file_enum * pfen)
{
    dirstack *d = pfen->dstack;

    if (d == 0)
        return false;
    pfen->dirp = d->entry;
    pfen->dstack = d->next;
    gs_free_object(pfen->memory, d, "gp_enumerate_files(popdir)");
    return true;
}
#endif

/* Initialize an enumeration. */
file_enum *
gp_enumerate_files_init_impl(gs_memory_t * mem, const char *pat, uint patlen)
{
#ifdef GS_NO_FILESYSTEM
    return &dummy_enum;
#else
    file_enum *pfen;
    char *p;
    char *work;

    /* Reject attempts to enumerate paths longer than the */
    /* system-dependent limit. */
    if (patlen > FILENAME_MAX)
        return 0;

    /* Reject attempts to enumerate with a pattern containing zeroes. */
    {
        const char *p1;

        for (p1 = pat; p1 < pat + patlen; p1++)
            if (*p1 == 0)
                return 0;
    }
    /* >>> Should crunch strings of repeated "/"'s in pat to a single "/"
     * >>>  to match stupid unix filesystem "conventions" */

    pfen = gs_alloc_struct(mem, file_enum, &st_file_enum,
                           "gp_enumerate_files");
    if (pfen == 0)
        return 0;

    /* pattern and work could be allocated as strings, */
    /* but it's simpler for GC and freeing to allocate them as bytes. */

    pfen->memory = mem;
    pfen->dstack = 0;
    pfen->first_time = true;
    pfen->patlen = patlen;
    pfen->work = 0;
    pfen->pattern =
        (char *)gs_alloc_bytes(mem, patlen + 1,
                               "gp_enumerate_files(pattern)");
    if (pfen->pattern == 0)
        goto fail1;
    memcpy(pfen->pattern, pat, patlen);
    pfen->pattern[patlen] = 0;

    work = (char *)gs_alloc_bytes(mem, FILENAME_MAX + 1,
                                  "gp_enumerate_files(work)");
    if (work == 0)
        goto fail2;
    pfen->work = work;

    p = work;
    memcpy(p, pat, patlen);
    p += patlen;
    *p = 0;

    /* Remove directory specifications beyond the first wild card. */
    /* Some systems don't have strpbrk, so we code it open. */
    p = pfen->work;
    while (*p != '*' && *p != '?' && *p != 0)
        p++;
    while (*p != '/' && *p != 0)
        p++;
    if (*p == '/')
        *p = 0;
    /* Substring for first wildcard match */
    pfen->pathead = p - work;

    /* Select the next higher directory-level. */
    p = rchr(work, '/', p - work);
    if (!p) {                   /* No directory specification */
        work[0] = 0;
        pfen->worklen = 0;
    } else {
        if (p == work) {        /* Root directory -- don't turn "/" into "" */
            p++;
        }
        *p = 0;
        pfen->worklen = p - work;
    }

    return pfen;

fail2:
    gs_free_object(mem, pfen->pattern, "gp_enumerate_files(pattern)");
fail1:
    gs_free_object(mem, pfen, "gp_enumerate_files");
    return NULL;
#endif
}

/* Enumerate the next file. */
uint
gp_enumerate_files_next_impl(gs_memory_t * mem, file_enum * pfen, char *ptr, uint maxlen)
{
#ifdef GS_NO_FILESYSTEM
    return ~(uint)0;
#else
    const dir_entry *de;
    char *work = pfen->work;
    int worklen = pfen->worklen;
    char *pattern = pfen->pattern;
    int pathead = pfen->pathead;
    int len;

    if (pfen->first_time) {
        pfen->dirp = ((worklen == 0) ? opendir(".") : opendir(work));
        if_debug1m('e', pfen->memory, "[e]file_enum:First-Open '%s'\n", work);
        pfen->first_time = false;
        if (pfen->dirp == 0) {  /* first opendir failed */
            gp_enumerate_files_close(mem, pfen);
            return ~(uint) 0;
        }
    }
  top:de = readdir(pfen->dirp);
    if (de == 0) {              /* No more entries in this directory */
        char *p;

        if_debug0m('e', pfen->memory, "[e]file_enum:Closedir\n");
        closedir(pfen->dirp);
        /* Back working directory and matching pattern up one level */
        p = rchr(work, '/', worklen);
        if (p != 0) {
            if (p == work)
                p++;
            *p = 0;
            worklen = p - work;
        } else
            worklen = 0;
        if (pathead != pfen->patlen) {
            p = rchr(pattern, '/', pathead);
            if (p != 0)
                pathead = p - pattern;
            else
                pathead = 0;
        }

        if (popdir(pfen)) {     /* Back up the directory tree. */
            if_debug1m('e', pfen->memory, "[e]file_enum:Dir popped '%s'\n", work);
            goto top;
        } else {
            if_debug0m('e', pfen->memory, "[e]file_enum:Dirstack empty\n");
            gp_enumerate_files_close(mem, pfen);
            return ~(uint) 0;
        }
    }
    /* Skip . and .. */
    len = strlen(de->d_name);
    if (len <= 2 && (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")))
        goto top;
    if (len + worklen + 1 > FILENAME_MAX)
        /* Should be an error, I suppose */
        goto top;
    if (worklen == 0) {         /* "Current" directory (evil un*x kludge) */
        memcpy(work, de->d_name, len + 1);
    } else if (worklen == 1 && work[0] == '/') {        /* Root directory */
        memcpy(work + 1, de->d_name, len + 1);
        len = len + 1;
    } else {
        work[worklen] = '/';
        memcpy(work + worklen + 1, de->d_name, len + 1);
        len = worklen + 1 + len;
    }

    /* Test for a match at this directory level */
    if (!wmatch((byte *) work, len, (byte *) pattern, pathead, pfen->memory))
        goto top;

    /* Perhaps descend into subdirectories */
    if (pathead < maxlen) {
        /* Using stat() to decide whether this item is a directory then opening
        using opendir(), results in Coverity complaining about races. Se
        instead we simple call opendir() immediately and look at whether it
        succeeded. */
        DIR *dp = opendir(work);
        if (!dp) {
            /* Not a directory. */
            goto winner;
        }

        if (pfen->patlen == pathead + 1) {      /* Listing "foo/?/" -- return this entry */
            closedir(dp);
            work[len++] = '/';
            goto winner;
        }

        /* >>> Should optimise the case in which the next level */
        /* >>> of directory has no wildcards. */
#ifdef DEBUG
        {
            char save_end = pattern[pathead];

            pattern[pathead] = 0;
            if_debug2m('e', pfen->memory, "[e]file_enum:fname='%s', p='%s'\n",
                       work, pattern);
            pattern[pathead] = save_end;
        }
#endif /* DEBUG */
        {                  /* Advance to the next directory-delimiter */
            /* in pattern */
            char *p;
            dirstack *d;

	    if (pattern[pathead] == 0) {
                pathead = pfen->patlen;
	    } else {
                for (p = pattern + pathead + 1;; p++) {
                    if (*p == 0) {  /* No more subdirectories to match */
                        pathead = pfen->patlen;
                        break;
                    } else if (*p == '/') {
                        pathead = p - pattern;
                        break;
                    }
                }
            }

            /* Push a directory onto the enumeration stack. */
            d = gs_alloc_struct(pfen->memory, dirstack,
                                &st_dirstack,
                                "gp_enumerate_files(pushdir)");
            if (d != 0) {
                d->next = pfen->dstack;
                d->entry = pfen->dirp;
                pfen->dstack = d;
            } else
                DO_NOTHING;     /* >>> gs_error_VMerror!!! */

            if_debug1m('e', pfen->memory, "[e]file_enum:Dir pushed '%s'\n",
                       work);
            worklen = len;
            pfen->dirp = dp;
            goto top;
        }
    }
  winner:
    /* We have a winner! */
    pfen->worklen = worklen;
    pfen->pathead = pathead;
    memcpy(ptr, work, len > maxlen ? maxlen : len);

    return len;
#endif
}

/* Clean up the file enumeration. */
void
gp_enumerate_files_close_impl(gs_memory_t * mem, file_enum * pfen)
{
#ifdef GS_NO_FILESYSTEM
    /* No cleanup necessary */
#else
    gs_memory_t *mem2 = pfen->memory;
    (void)mem;

    if_debug0m('e', mem2, "[e]file_enum:Cleanup\n");
    while (popdir(pfen))        /* clear directory stack */
        DO_NOTHING;
    gs_free_object(mem2, (byte *) pfen->work,
                   "gp_enumerate_close(work)");
    gs_free_object(mem2, (byte *) pfen->pattern,
                   "gp_enumerate_files_close(pattern)");
    gs_free_object(mem2, pfen, "gp_enumerate_files_close");
#endif
}

/* Test-cases:
   (../?*r*?/?*.ps) {==} 100 string filenameforall
   (../?*r*?/?*.ps*) {==} 100 string filenameforall
   (../?*r*?/) {==} 100 string filenameforall
   (/t*?/?*.ps) {==} 100 string filenameforall
 */

/* --------- 64 bit file access ----------- */

gs_offset_t gp_ftell_impl(FILE *strm)
{
#if defined(HAVE_FILE64)
    return ftello64(strm);
#else
    return ftello(strm);
#endif
}

int gp_fseek_impl(FILE *strm, gs_offset_t offset, int origin)
{
#if defined(HAVE_FILE64)
    return fseeko64(strm, offset, origin);
#else
    off_t offset1 = (off_t)offset;

    if (offset != offset1)
        return -1;
    return fseeko(strm, offset1, origin);
#endif
}

bool gp_fseekable_impl(FILE *f)
{
    struct stat s;
    int fno;

    fno = fileno(f);
    if (fno < 0)
        return(false);

    if (fstat(fno, &s) < 0)
        return(false);

    return((bool)S_ISREG(s.st_mode));
}
