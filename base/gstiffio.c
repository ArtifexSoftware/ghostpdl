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


/* TIFF hooks and stubs */

#include "stdint_.h"   /* for tiff.h */
#include "stdio_.h"
#include "time_.h"
#include "malloc_.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gdevprn.h"

#include "scommon.h"
#include "stream.h"
#include "strmio.h"

#include "gstiffio.h"


#define TIFF_PRINT_BUF_LENGTH 1024
static const char tifs_msg_truncated[] = "\n*** Previous line has been truncated.\n";

/* place to hold the data for our libtiff i/o hooks */
typedef struct tifs_io_private_t
{
    gp_file *f;
    gx_device_printer *pdev;
} tifs_io_private;

/* libtiff i/o hooks */
static int
gs_tifsDummyMapProc(thandle_t fd, void** pbase, toff_t* psize)
{
    (void) fd;
    (void) pbase;
    (void) psize;
    return (0);
}

static void
gs_tifsDummyUnmapProc(thandle_t fd, void* base, toff_t size)
{
    (void) fd;
    (void) base;
    (void) size;
}

static size_t
gs_tifsReadProc(thandle_t fd, void* buf, size_t size)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    size_t size_io = (size_t) size;

    if ((size_t) size_io != size) {
        return (size_t) -1;
    }
    return((size_t) gp_fread (buf, 1, size_io, tiffio->f));
}

static size_t
gs_tifsWriteProc(thandle_t fd, void* buf, size_t size)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    size_t size_io = (size_t) size;
    size_t written;

    if ((size_t) size_io != size) {
        return (size_t) -1;
    }
    written = (size_t) gp_fwrite (buf, 1, size_io, tiffio->f);
    return written;
}

static uint64_t
gs_tifsSeekProc(thandle_t fd, uint64_t off, int whence)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    gs_offset_t off_io = (gs_offset_t) off;

    if ((uint64_t) off_io != off) {
        return (uint64_t) -1; /* this is really gross */
    }
    if (gp_fseek(tiffio->f , (gs_offset_t)off_io, whence) < 0) {
        return (uint64_t) -1;
    }
    return (gp_ftell(tiffio->f));
}

static int
gs_tifsCloseProc(thandle_t fd)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    gx_device_printer *pdev = tiffio->pdev;
    int code = gp_fclose(tiffio->f);
    
    gs_free(pdev->memory, tiffio, sizeof(tifs_io_private), 1, "gs_tifsCloseProc");

    return code;
}

static uint64_t
gs_tifsSizeProc(thandle_t fd)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    uint64_t length;
    gs_offset_t curpos = gp_ftell(tiffio->f);

    if (curpos < 0) {
        return(0);
    }
    
    if (gp_fseek(tiffio->f, (gs_offset_t)0, SEEK_END) < 0) {
        return(0);
    }
    length = (uint64_t)gp_ftell(tiffio->f);
    
    if (gp_fseek(tiffio->f, curpos, SEEK_SET) < 0) {
        return(0);
    }
    return length;
}

TIFF *
tiff_from_filep(gx_device_printer *dev,  const char *name, gp_file *filep, int big_endian, bool usebigtiff)
{
    char mode[5] = "w";
    int modelen = 1;
    TIFF *t;
    tifs_io_private *tiffio;

    if (big_endian)
        mode[modelen++] = 'b';
    else
        mode[modelen++] = 'l';

    if (usebigtiff)
    /* this should never happen for libtiff < 4.0 - see tiff_put_some_params() */
        mode[modelen++] = '8';

    mode[modelen] = (char)0;
    
    tiffio = (tifs_io_private *)gs_malloc(dev->memory, sizeof(tifs_io_private), 1, "tiff_from_filep");
    if (!tiffio) {
        return NULL;
    }
    tiffio->f = filep;
    tiffio->pdev = dev;

    t = TIFFClientOpen(name, mode,
        (thandle_t) tiffio, (TIFFReadWriteProc)gs_tifsReadProc,
        (TIFFReadWriteProc)gs_tifsWriteProc, (TIFFSeekProc)gs_tifsSeekProc,
        gs_tifsCloseProc, (TIFFSizeProc)gs_tifsSizeProc, gs_tifsDummyMapProc,
        gs_tifsDummyUnmapProc);

    return t;
}

static void
gs_tifsWarningHandlerEx(thandle_t client_data, const char* module, const char* fmt, va_list ap)
{
    tifs_io_private *tiffio = (tifs_io_private *)client_data;
    gx_device_printer *pdev = tiffio->pdev;    
    int count;
    char buf[TIFF_PRINT_BUF_LENGTH];

    count = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (count >= sizeof(buf) || count < 0)  { /* C99 || MSVC */
        dmlprintf1(pdev->memory, "%s", buf);
        dmlprintf1(pdev->memory, "%s\n", tifs_msg_truncated);
    } else {
        dmlprintf1(pdev->memory, "%s\n", buf);
    }
}

static void
gs_tifsErrorHandlerEx(thandle_t client_data, const char* module, const char* fmt, va_list ap)
{
    tifs_io_private *tiffio = (tifs_io_private *)client_data;
    gx_device_printer *pdev = tiffio->pdev;    
    const char *max_size_error = "Maximum TIFF file size exceeded";
    int count;
    char buf[TIFF_PRINT_BUF_LENGTH];

    count = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (count >= sizeof(buf) || count < 0)  { /* C99 || MSVC */
        dmlprintf1(pdev->memory, "%s\n", buf);
        dmlprintf1(pdev->memory, "%s", tifs_msg_truncated);
    } else {
        dmlprintf1(pdev->memory, "%s\n", buf);
    }

#if (TIFFLIB_VERSION >= 20111221)
    if (!strncmp(fmt, max_size_error, strlen(max_size_error))) {
        dmlprintf(pdev->memory, "Use -dUseBigTIFF(=true) for BigTIFF output\n");
    }
#endif
}

void tiff_set_handlers (void)
{
    /* Bad things happen if we set custom error/warning
     * handlers, and multiple callers are using the shared
     * libtiff - our handlers may be triggered in the
     * context of the other caller(s) meaning it's not
     * our client_data being passed in.
     */
    (void)TIFFSetErrorHandler(NULL);
    (void)TIFFSetWarningHandler(NULL);
#if SHARE_LIBTIFF == 0
    (void)TIFFSetErrorHandlerExt(gs_tifsErrorHandlerEx);
    (void)TIFFSetWarningHandlerExt(gs_tifsWarningHandlerEx);
#endif
}

#if SHARE_LIBTIFF == 0
TIFF*
TIFFFdOpen(int fd, const char* name, const char* mode)
{
    (void)fd;
    (void)name;
    (void)mode;

    return(NULL);
}

TIFF*
TIFFOpen(const char* name, const char* mode)
{
    (void)name;
    (void)mode;
    
    return(NULL);
}

void*
_TIFFmalloc(tmsize_t s)
{
    return (malloc((size_t) s));
}

void* _TIFFcalloc(tmsize_t nmemb, tmsize_t siz)
{
    void *m = NULL;
    if( nmemb != 0 && siz != 0 ) {
        m = malloc((size_t)(nmemb * siz));
    }
    if (m)
        memset(m, 0x00, (size_t)(nmemb * siz));
    return m;
}

void
_TIFFfree(void* p)
{
    free(p);
}

void*
_TIFFrealloc(void* p, tmsize_t s)
{
    return (realloc(p, (size_t) s));
}

void
_TIFFmemset(void* p, int v, tmsize_t c)
{
    memset(p, v, (size_t) c);
}

void
_TIFFmemcpy(void* d, const void* s, tmsize_t c)
{
    memcpy(d, s, (size_t) c);
}

int
_TIFFmemcmp(const void* p1, const void* p2, tmsize_t c)
{
    return (memcmp(p1, p2, (size_t) c));
}

/* We supply our own warning/error handlers when we invoke libtiff */
TIFFErrorHandler _TIFFwarningHandler = NULL;
TIFFErrorHandler _TIFFerrorHandler = NULL;

#endif /*  SHARE_LIBTIFF == 0 */
