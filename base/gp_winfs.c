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

/* prevent gp.h from defining fopen */
#define fopen fopen

#include "windows_.h"
#include "stdio_.h"
#include "gp.h"
#include "memory_.h"
#include "stat_.h"

#include "gp_mswin.h"

/* Open a file with the given name, as a stream of uninterpreted bytes. */
FILE *
gp_fopen_impl(gs_memory_t *mem, const char *fname, const char *mode)
{
    int len = utf8_to_wchar(NULL, fname);
    wchar_t *uni;
    wchar_t wmode[4];
    FILE *file;

    if (len <= 0)
        return NULL;

    uni = (wchar_t *)gs_alloc_bytes(mem, len*sizeof(wchar_t), "gp_fopen_impl");
    if (uni == NULL)
        return NULL;
    utf8_to_wchar(uni, fname);
    utf8_to_wchar(wmode, mode);
    file = _wfopen(uni, wmode);
    gs_free_object(mem, uni, "gs_fopen_impl");

    return file;
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
