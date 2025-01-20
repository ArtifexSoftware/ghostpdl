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

/* prevent gp.h from defining fopen */
#define fopen fopen

#include "windows_.h"
#include "stdio_.h"
#include "gp.h"
#include "memory_.h"
#include "stat_.h"
#include "gserrors.h"

#include "gp_mswin.h"

/* Should be in io.h */
extern intptr_t _get_osfhandle(int fd);

/* Open a file with the given name, as a stream of uninterpreted bytes. */
FILE *
gp_fopen_impl(gs_memory_t *mem, const char *fname, const char *mode)
{
    int len = gp_utf8_to_uint16(NULL, fname);
    wchar_t *uni;
    wchar_t wmode[4];
    FILE *file;

    if (len <= 0)
        return NULL;

    uni = (wchar_t *)gs_alloc_bytes(mem, len*sizeof(wchar_t), "gp_fopen_impl");
    if (uni == NULL)
        return NULL;
    gp_utf8_to_uint16(uni, fname);

    len = gp_utf8_to_uint16(NULL, mode);
    if (len >= 0) {
        gp_utf8_to_uint16(wmode, mode);
        file = _wfopen(uni, wmode);
    }
    gs_free_object(mem, uni, "gs_fopen_impl");

    return file;
}

int
gp_unlink_impl(gs_memory_t *mem, const char *fname)
{
    int len = gp_utf8_to_uint16(NULL, fname);
    wchar_t *uni;
    int ret;

    if (len <= 0)
        return gs_error_unknownerror;

    uni = (wchar_t *)gs_alloc_bytes(mem, len*sizeof(wchar_t), "gp_unlink_impl");
    if (uni == NULL)
        return gs_error_VMerror;
    gp_utf8_to_uint16(uni, fname);
    ret = _wunlink(uni);
    gs_free_object(mem, uni, "gs_unlink_impl");

    return ret;
}

int
gp_rename_impl(gs_memory_t *mem, const char *from, const char *to)
{
    int lenf = gp_utf8_to_uint16(NULL, from);
    int lent = gp_utf8_to_uint16(NULL, to);
    wchar_t *unif, *unit;
    int ret;

    if (lenf <= 0 || lent <= 0)
        return gs_error_unknownerror;

    unif = (wchar_t *)gs_alloc_bytes(mem, lenf*sizeof(wchar_t), "gp_rename_impl");
    if (unif == NULL)
        return gs_error_VMerror;
    unit = (wchar_t *)gs_alloc_bytes(mem, lent*sizeof(wchar_t), "gp_rename_impl");
    if (unit == NULL) {
        gs_free_object(mem, unif, "gs_unlink_impl");
        return gs_error_VMerror;
    }
    gp_utf8_to_uint16(unif, from);
    gp_utf8_to_uint16(unit, to);
    ret = _wrename(unif, unit);
    gs_free_object(mem, unif, "gs_rename_impl");
    gs_free_object(mem, unit, "gs_rename_impl");

    return ret;
}

/* Create a second open FILE on the basis of a given one */
FILE *gp_fdup_impl(FILE *f, const char *mode)
{
    int fd = fileno(f);
    if (fd < 0)
        return NULL;

    fd = dup(fd);
    if (fd < 0)
        return NULL;

    return fdopen(fd, mode);
}

/* Read from a specified offset within a FILE into a buffer */
int gp_pread_impl(char *buf, size_t count, gs_offset_t offset, FILE *f)
{
    OVERLAPPED overlapped;
    DWORD ret;
    HANDLE hnd = (HANDLE)_get_osfhandle(fileno(f));

    if (hnd == INVALID_HANDLE_VALUE)
        return -1;

    memset(&overlapped, 0, sizeof(OVERLAPPED));
    overlapped.Offset = (DWORD)offset;
    overlapped.OffsetHigh = (DWORD)(offset >> 32);

    if (!ReadFile((HANDLE)hnd, buf, count, &ret, &overlapped))
        return -1;

    return ret;
}

/* Write to a specified offset within a FILE from a buffer */
int gp_pwrite_impl(const char *buf, size_t count, gs_offset_t offset, FILE *f)
{
    OVERLAPPED overlapped;
    DWORD ret;
    HANDLE hnd = (HANDLE)_get_osfhandle(fileno(f));

    if (hnd == INVALID_HANDLE_VALUE)
        return -1;

    memset(&overlapped, 0, sizeof(OVERLAPPED));
    overlapped.Offset = (DWORD)offset;
    overlapped.OffsetHigh = (DWORD)(offset >> 32);

    if (!WriteFile((HANDLE)hnd, buf, count, &ret, &overlapped))
        return -1;

    return ret;
}

/* --------- 64 bit file access ----------- */
/* MSVC versions before 8 doen't provide big files.
   MSVC 8 doesn't distinguish big and small files,
   but provide special positioning functions
   to access data behind 4GB.
   Currently we support 64 bits file access with MSVC only.
 */

#if defined(_MSC_VER) && _MSC_VER < 1400
    int64_t _ftelli64( FILE *);
    int _fseeki64( FILE *, int64_t, int);
#endif

gs_offset_t gp_ftell_impl(FILE *strm)
{
#if !defined(_MSC_VER)
    return ftell(strm);
#elif _MSC_VER < 1200
    return ftell(strm);
#else
    return (int64_t)_ftelli64(strm);
#endif
}

int gp_fseek_impl(FILE *strm, gs_offset_t offset, int origin)
{
#if !defined(_MSC_VER)
    return fseek(strm, offset, origin);
#elif _MSC_VER < 1200
    long offset1 = (long)offset;

    if (offset != offset1)
        return -1;
    return fseek(strm, offset1, origin);
#else
    return _fseeki64(strm, offset, origin);
#endif
}

bool gp_fseekable_impl(FILE *f)
{
    struct __stat64 s;
    int fno;

    fno = fileno(f);
    if (fno < 0)
        return(false);

    if (_fstat64(fno, &s) < 0)
        return(false);

    return((bool)S_ISREG(s.st_mode));
}
