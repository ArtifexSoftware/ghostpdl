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

#ifdef _WIN32
/* Stop windows builds complaining about sprintf being insecure. */
#define _CRT_SECURE_NO_WARNINGS
/* Ensure the dll import works correctly. */
#define _WINDOWS_
#endif

/* We can't have pointers displayed in the test output, as that will
 * change the output between runs. The following definition hides
 * them. */
#ifdef CLUSTER
#define HIDE_POINTERS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>

#ifndef GHOSTPDL
#define GHOSTPDL 1
#endif

#if GHOSTPDL
#include "pcl/pl/plapi.h"   /* GSAPI - gpdf version */
#else
#include "psi/iapi.h"       /* GSAPI - ghostscript version */
#endif
#include "devices/gdevdsp.h"

/* In order to get consistent printing of pointers, we can't just
 * use %p, as this includes the 0x on some platforms, and not on
 * others. We therefore use a bit of #ifdeffery to get us a
 * consistent result. */
#if defined(_WIN64) || defined(_WIN32)
 #define FMT_PTR "I64x"
 #define PTR_CAST (__int64)(size_t)(intptr_t)
 #define FMT_Z "I64d"
 #define Z_CAST (__int64)
#else
 #define FMT_PTR "llx"
 #define PTR_CAST (long long)(size_t)(intptr_t)
 #define FMT_Z "lld"
 #define Z_CAST (long long)
#endif

#define INSTANCE_HANDLE ((void *)1234)
#define SANITY_CHECK_VALUE 0x12345678

#define SANITY_CHECK(ts) assert(ts->sanity_check_value == SANITY_CHECK_VALUE)

/* All the state for a given test is contained within the following
 * structure. */
typedef struct {
    /* This value should always be set to SANITY_CHECK_VALUE. It
     * allows us to check we have a valid (or at least plausible)
     * teststate_t pointer by checking its value. */
    int sanity_check_value;

    int use_clist;
    int legacy;

    int w;
    int h;
    int r;
    int pr;
    int bh;
    int y;
    int lines_requested;
    int format;
    int align;

    int n;
    void *mem;
    FILE *file;
    const char *fname;
} teststate_t;

#ifdef HIDE_POINTERS
void *hide_pointer(void *p)
{
    return (p == NULL) ? NULL : (void *)1;
}

#define PTR(p) hide_pointer(p)

#else

#define PTR(p) p

#endif


/*--------------------------------------------------------------------*/
/* First off, we have a set of functions that cope with dumping lines
 * of rendered data to file. We use pnm formats for their simplicity. */

/* This function opens the file, named appropriately, and writes the
 * header. */
static FILE *save_header(teststate_t *ts)
{
    char text[32];
    const char *suffix;
    const char *align_str;

    /* Only output the header once. */
    if (ts->file != NULL)
         return ts->file;

    switch (ts->n)
    {
        case 1:
            suffix = "pgm";
            break;
        case 3:
            suffix = "ppm";
            break;
        case 4:
        default:
            suffix = "pam";
            break;
    }

    switch (ts->format & DISPLAY_ROW_ALIGN_MASK) {
        default:
        case DISPLAY_ROW_ALIGN_DEFAULT:
            align_str = "";
            break;
        case DISPLAY_ROW_ALIGN_4:
            align_str = "_4";
            break;
        case DISPLAY_ROW_ALIGN_8:
            align_str = "_8";
            break;
        case DISPLAY_ROW_ALIGN_16:
            align_str = "_16";
            break;
        case DISPLAY_ROW_ALIGN_32:
            align_str = "_32";
            break;
        case DISPLAY_ROW_ALIGN_64:
            align_str = "_64";
            break;
    }

    sprintf(text, "%s%s%s%s.%s",
            ts->fname,
            ts->use_clist ? "_c" : "",
            ts->legacy ? "_l" : "",
            align_str,
            suffix);
    printf("Outputting %s\n", text);
    ts->file = fopen(text, "wb");
    if (ts->file == NULL) {
        fprintf(stderr, "Fatal error: couldn't open %s for writing.\n", text);
        exit(1);
    }

    switch (ts->n)
    {
        case 1:
            fprintf(ts->file,
                    "P5\n%d %d\n255\n",
                    ts->w, ts->h);
            break;
        case 3:
            fprintf(ts->file,
                    "P6\n%d %d\n255\n",
                    ts->w, ts->h);
            break;
        case 4:
        default:
            fprintf(ts->file,
                    "P7\nWIDTH %d\nHEIGHT %d\nDEPTH %d\nMAXVAL 255\n"
                    "%s"
                    "ENDHDR\n",
                    ts->w, ts->h, ts->n, ts->n == 4 ? "TUPLTYPE CMYK\n" : "");
            break;
    }

    /* If we're getting the lines in BOTTOMFIRST order, then pad out
     * the file to the required length. We'll gradually backtrack
     * through the file filling it in with real data as it arrives. */
    if (ts->format && DISPLAY_BOTTOMFIRST) {
        static const char blank[256] = { 0 };
        int n = ts->w * ts->h * ts->n;
        while (n > 0) {
            int i = n;
            if (i > sizeof(blank))
                i = sizeof(blank);
            fwrite(blank, 1, i, ts->file);
            n -= i;
        }
    }

    return ts->file;
}

/* Write out the next h lines from the buffer. */
static void save_lines(teststate_t *ts, int h)
{
    int i, j, k;
    const char *m = ts->mem;
    int w = ts->w;
    int n = ts->n;
    int r = ts->r;
    int pr = ts->pr;
    int wn = w*n;
    /* Make sure we've put a header on the file. */
    FILE *file = save_header(ts);

    /* If the data is being given in "little endian" format then
     * reorder it as we write it out. This is required to cope with
     * the fact that windows bitmaps prefer BGR over RGB.
     *
     * If we are getting data BOTTOMFIRST, then we will already be
     * positioned at the end of the file. Backtrack through the file
     * as we go so that the lines appear in a sane order.
     */
    if (ts->format & (DISPLAY_PLANAR | DISPLAY_PLANAR_INTERLEAVED)) {
        /* Planar */
        if (ts->format & DISPLAY_LITTLEENDIAN) {
            for (i = 0; i < h; i++) {
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
                for (j = 0; j < w; j++) {
                    for (k = n; k > 0;)
                        fputc(m[--k * pr], file);
                    m++;
                }
                m += r - w;
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
            }
        } else {
            for (i = 0; i < h; i++) {
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
                for (j = 0; j < w; j++) {
                    for (k = 0; k < n; k++)
                        fputc(m[k * pr], file);
                    m++;
                }
                m += r - w;
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
            }
        }
    } else {
        /* Chunky */
        if (ts->format & DISPLAY_LITTLEENDIAN) {
            for (i = 0; i < h; i++) {
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
                for (j = 0; j < w; j++) {
                    for (k = n; k > 0;)
                        fputc(m[--k], file);
                    m += n;
                }
                m += r - wn;
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
            }
        } else {
            for (i = 0; i < h; i++) {
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
                fwrite(m, 1, wn, file);
                m += r;
                if (ts->format & DISPLAY_BOTTOMFIRST)
                    fseek(file, -wn, SEEK_CUR);
            }
        }
    }
}

/* Finish writing out. */
static void save_end(teststate_t *ts)
{
    fclose(ts->file);
    ts->file = NULL;
}

/*--------------------------------------------------------------------*/
/* Next we have the implementations of the callback functions. */

static int
open(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("open\n");

    return 0;
}

static int
preclose(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("preclose\n");

    return 0;
}

static int
close(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("close\n");

    return 0;
}

static int
presize(void *handle, void *device,
        int width, int height, int raster, unsigned int format)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("presize: w=%d h=%d r=%d f=%x\n",
           width, height, raster, format);

    ts->w = width;
    ts->h = height;
    ts->r = raster;
    ts->bh = 0;
    ts->y = 0;
    ts->format = format;

    if (ts->format & DISPLAY_COLORS_GRAY)
        ts->n = 1;
    if (ts->format & DISPLAY_COLORS_RGB)
        ts->n = 3;
    if (ts->format & DISPLAY_COLORS_CMYK)
        ts->n = 4;
    if (ts->format & DISPLAY_COLORS_SEPARATION)
        ts->n = 0;
    if ((ts->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
        return -1; /* Haven't written code for that! */

    return 0;
}

static int
size(void *handle, void *device, int width, int height,
     int raster, unsigned int format, unsigned char *pimage)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("size: w=%d h=%d r=%d f=%x m=%p\n",
           width, height, raster, format, PTR(pimage));
    ts->w = width;
    ts->h = height;
    ts->r = raster;
    ts->format = format;
    ts->mem = pimage;

    if (ts->format & DISPLAY_PLANAR)
        ts->pr = ts->r * height;
    /* When running with spots, n is not known yet. */
    if (ts->n != 0 && ts->format & DISPLAY_PLANAR_INTERLEAVED)
        ts->pr = ts->r / ts->n;

    return 0;
}

static int
sync(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("sync\n");

    return 0;
}

static int
page(void *handle, void *device, int copies, int flush)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("page: c=%d f=%d\n", copies, flush);

    save_lines(ts, ts->h);
    save_end(ts);

    return 0;
}

static int
update(void *handle, void *device, int x, int y, int w, int h)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    /* This print statement just makes too much noise :) */
    /* printf("update: x=%d y=%d w=%d h=%d\n", x, y, w, h); */

    return 0;
}

void *
aligned_malloc(size_t size, int alignment)
{
    char *ret = malloc(size + alignment*2);
    int boost;

    if (ret == NULL)
        return ret;

    boost = alignment - (((intptr_t)ret) & (alignment-1));
    memset(ret, boost, boost);

    return ret + boost;
}

void aligned_free(void *ptr)
{
    char *p = ptr;

    if (ptr == NULL)
        return;

    p -= p[-1];
    free(p);
}

static void *
memalloc(void *handle, void *device, size_t size)
{
    void *ret = NULL;
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);

    if (ts->use_clist) {
        printf("memalloc: asked for %"FMT_Z" but requesting clist\n", Z_CAST size);
        return 0;
    }

    ret = aligned_malloc(size, 64);
    printf("memalloc: %"FMT_Z" -> %p\n", Z_CAST size, PTR(ret));

    return ret;
}

static int
memfree(void *handle, void *device, void *mem)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("memfree: %p\n", PTR(mem));
    aligned_free(mem);

    return 0;
}

static int
separation(void *handle, void *device,
           int component, const char *component_name,
           unsigned short c, unsigned short m,
           unsigned short y, unsigned short k)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("separation: %d %s (%x,%x,%x,%x)\n",
           component, component_name ? component_name : "<NULL>",
           c, m, y, k);
    ts->n++;

    /* Update the plane_raster as n has changed. */
    if (ts->format & DISPLAY_PLANAR_INTERLEAVED)
        ts->pr = ts->r / ts->n;

    return 0;
}

static int
adjust_band_height(void *handle, void *device, int bandheight)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("adjust_band_height: %d -> ", bandheight);

    if (bandheight > ts->h / 4)
        bandheight = ts->h / 4;

    printf("%d\n", bandheight);

    ts->bh = bandheight;

    return bandheight;
}

static int
rectangle_request(void *handle, void *device,
                  void **memory, int *ox, int *oy,
                  int *raster, int *plane_raster,
                  int *x, int *y, int *w, int *h)
{
    teststate_t *ts = (teststate_t *)handle;
    size_t size;

    SANITY_CHECK(ts);
    printf("rectangle_request:");

    if (ts->mem) {
        /* Rectangle returned */
        save_lines(ts, ts->lines_requested);
        aligned_free(ts->mem);
        ts->mem = NULL;
        ts->y += ts->lines_requested;
        ts->lines_requested = 0;
    }

    if (ts->y >= ts->h)
    {
        /* All banded out */
        printf("Finished!\n");
        *ox = 0;
        *oy = 0;
        *raster = 0;
        *plane_raster = 0;
        *x = 0;
        *y = 0;
        *w = 0;
        *h = 0;
        *memory = NULL;
        save_end(ts);
        return 0;
    }
    *ox = 0;
    *oy = ts->y;
    *x = 0;
    *y = ts->y;
    *w = ts->w;
    *h = ts->bh;
    if (ts->y + ts->bh > ts->h)
        *h = ts->h - ts->y;
    ts->lines_requested = *h;
    switch (ts->format & (DISPLAY_PLANAR | DISPLAY_PLANAR_INTERLEAVED)) {
        case DISPLAY_CHUNKY:
            ts->r = (ts->w * ts->n + ts->align-1) & ~(ts->align-1);
            ts->pr = 0;
            size = ts->r * *h;
            break;
        case DISPLAY_PLANAR:
            ts->r = (ts->w + ts->align-1) & ~(ts->align-1);
            ts->pr = ts->r * *h;
            size = ts->pr * ts->n;
            break;
        case DISPLAY_PLANAR_INTERLEAVED:
            ts->pr = (ts->w + ts->align-1) & ~(ts->align-1);
            ts->r = ts->pr * ts->n;
            size = ts->r * *h;
            break;
    }
    *raster = ts->r;
    *plane_raster = ts->pr;
    ts->mem = aligned_malloc(size, 64);
    *memory = ts->mem;

    printf("x=%d y=%d w=%d h=%d mem=%p\n", *x, *y, *w, *h, PTR(*memory));
    if (ts->mem == NULL)
        return -1;

    return 0;
}

/*--------------------------------------------------------------------*/
/* All those callback functions live in a display_callback structure
 * that we return to the main code. This can be done using the modern
 * "callout" method, or by using the legacy (deprecated) direct
 * registration method. We strongly prefer the callout method as it
 * avoids the need to pass a pointer using -sDisplayHandle. */
static display_callback callbacks =
{
    sizeof(callbacks),
    DISPLAY_VERSION_MAJOR,
    DISPLAY_VERSION_MINOR,
    open,
    preclose,
    close,
    presize,
    size,
    sync,
    page,
    update,
    memalloc,
    memfree,
    separation,
    adjust_band_height,
    rectangle_request
};

/*--------------------------------------------------------------------*/
/* This is our callout handler. It handles callouts from devices within
 * Ghostscript. It only handles a single callout, from the display
 * device, to return the callback handler and callback handle. */
static int
callout(void *instance,
        void *callout_handle,
        const char *device_name,
        int id,
        int size,
        void *data)
{
    teststate_t *ts = (teststate_t *)callout_handle;

    SANITY_CHECK(ts);

    /* We are only interested in callouts from the display device. */
    if (strcmp(device_name, "display"))
        return -1;

    if (id == DISPLAY_CALLOUT_GET_CALLBACK)
    {
        /* Fill in the supplied block with the details of our callback
         * handler, and the handle to use. In this instance, the handle
         * is the pointer to our test structure. */
        gs_display_get_callback_t *cb = (gs_display_get_callback_t *)data;
        cb->callback = &callbacks;
        cb->caller_handle = ts;
        return 0;
    }
    return -1;
}

/*--------------------------------------------------------------------*/
/* This is the function that actually runs a test. */
static int do_ddtest(const char *title, int format,
                   int use_clist, int legacy,
                   int ps,
                   const char *fname)
{
    int code;
    void *instance   = NULL;
    char *clist_str  = use_clist ? " (clist)"  : "";
    char *legacy_str = legacy    ? " (legacy)" : "";
    char *align_str = "";
    char format_arg[64];
    char handle_arg[64];

    /* Make the teststate a blank slate for us to work with. */
    teststate_t teststate = { SANITY_CHECK_VALUE };

    /* Construct the argc/argv to pass to ghostscript. */
    int argc = 0;
    char *argv[20];

    argv[argc++] = "gs";
    argv[argc++] = "-sDEVICE=display";
    argv[argc++] = "-dNOPAUSE";
    argv[argc++] = format_arg;
    if (legacy)
        argv[argc++] = handle_arg;
    if (format & DISPLAY_COLORS_SEPARATION) {
        /* Two different spots test files. The PS one
         * uses 4 process colors + 4 spots, the
         * PDF one uses 4 process colors + 3 spots.
         * Chunky mode can't use more than 8 in total,
         * and we want to test the behaviour of the
         * system with an 'odd' number of spots.
         */
        if (ps)
            argv[argc++] = "../../examples/spots.ps";
        else
            argv[argc++] = "../../examples/spots2.pdf";
    } else
        argv[argc++] = "../../examples/tiger.eps";

    sprintf(format_arg, "-dDisplayFormat=16#%x", format);
    sprintf(handle_arg, "-sDisplayHandle=16#%" FMT_PTR, PTR_CAST &teststate);

    /* Setup the details to control the test. */
    teststate.use_clist = use_clist;
    teststate.legacy = legacy;
    teststate.fname = fname;

    switch (format & DISPLAY_ROW_ALIGN_MASK) {
        default:
        case DISPLAY_ROW_ALIGN_DEFAULT:
            align_str = " (default alignment)";
            teststate.align = 0;
            break;
        case DISPLAY_ROW_ALIGN_4:
            align_str = " (align % 4)";
            teststate.align = 4;
            break;
        case DISPLAY_ROW_ALIGN_8:
            align_str = " (align % 8)";
            teststate.align = 8;
            break;
        case DISPLAY_ROW_ALIGN_16:
            align_str = " (align % 16)";
            teststate.align = 16;
            break;
        case DISPLAY_ROW_ALIGN_32:
            align_str = " (align % 32)";
            teststate.align = 32;
            break;
        case DISPLAY_ROW_ALIGN_64:
            align_str = " (align % 64)";
            teststate.align = 64;
            break;
    }
    /* Special case: alignments are always at least pointer sized. */
    if (teststate.align <= sizeof(void *))
        teststate.align = sizeof(void *);

    /* Print the test title. */
    printf("%s%s%s%s\n", title, clist_str, legacy_str, align_str);

    /* Create a GS instance. */
    code = gsapi_new_instance(&instance, INSTANCE_HANDLE);
    if (code < 0) {
        printf("Error %d in gsapi_new_instance\n", code);
        goto failearly;
    }

    if (legacy) {
        /* Directly pass in the callback structure. This relies on the
         * handle being passed using -sDisplayHandle above.  */
        code = gsapi_set_display_callback(instance, &callbacks);
        if (code < 0) {
            printf("Error %d in gsapi_set_display_callback\n", code);
            goto fail;
        }
    } else {
        /* Register our callout handler. This will pass the display
         * device the callback structure and handle when requested. */
        code = gsapi_register_callout(instance, callout, &teststate);
        if (code < 0) {
            printf("Error %d in gsapi_register_callout\n", code);
            goto fail;
        }
    }

    /* Run our test. */
    code = gsapi_init_with_args(instance, argc, argv);
    if ((format & DISPLAY_ROW_ALIGN_MASK) == DISPLAY_ROW_ALIGN_4 &&
        sizeof(void *) > 4) {
        if (code == -100) {
            printf("Got expected failure!\n");
            code = 0;
            goto fail;
        } else if (code == 0) {
            printf("Failed to get expected failure!\n");
            code = -1;
            goto fail;
        }
    }
    if (code < 0) {
        printf("Error %d in gsapi_init_with_args\n", code);
        goto fail;
    }

    /* Close the interpreter down (important, or we will leak!) */
    code = gsapi_exit(instance);
    if (code < 0) {
        printf("Error %d in gsapi_exit\n", code);
        goto fail;
    }

fail:
    /* Delete the gs instance. */
    gsapi_delete_instance(instance);

failearly:
    /* All done! */
    printf("%s%s%s%s %s\n", title, clist_str, legacy_str, align_str,
           (code < 0) ? "failed" : "complete");

    return code;
}

static int displaydev_test(const char *title, int format, int ps, const char *fname)
{
    int use_clist, legacy, align, code;

    code = 0;
    for (use_clist = 0; use_clist <= 1; use_clist++) {
        for (legacy = 0; legacy <= 1; legacy++) {
            for (align = 2; align <= 7; align++) {
                int form = format;
                if (align != 2) {
                    form |= align<<20;
                }
                code = do_ddtest(title, form, use_clist, legacy, ps, fname);
                if (code < 0)
                    return code;
            }
        }
    }
    return code;
}

static int
runstring_test(const char *dev, char *outfile, ...)
{
    int code;
    void *instance   = NULL;
    char devtext[64];
    va_list args;
    char *infile;
    int error_code;

    /* Construct the argc/argv to pass to ghostscript. */
    int argc = 0;
    char *argv[10];

    sprintf(devtext, "-sDEVICE=%s", dev);
    argv[argc++] = "gpdl";
    argv[argc++] = devtext;
    argv[argc++] = "-o";
    argv[argc++] = outfile;

    /* Create a GS instance. */
    code = gsapi_new_instance(&instance, INSTANCE_HANDLE);
    if (code < 0) {
        printf("Error %d in gsapi_new_instance\n", code);
        goto failearly;
    }

    /* Run our test. */
    code = gsapi_init_with_args(instance, argc, argv);
    if (code < 0) {
        printf("Error %d in gsapi_init_with_args\n", code);
        goto fail;
    }

    va_start(args, outfile);
    while ((infile = va_arg(args, char *)) != NULL) {
        printf("Feeding %s via runstring\n", infile);
        code = gsapi_run_string_begin(instance, 0, &error_code);
        if (code < 0) {
            printf("Error %d in gsapi_run_string_begin\n", code);
            goto fail;
        }

        {
            FILE *file = fopen(infile, "rb");
            char block[1024];
            unsigned int len;
            if (file == NULL) {
                printf("Error: Failed to open %s for reading\n", infile);
                code = -1;
                goto fail;
            }
            while (!feof(file)) {
                len = (unsigned int)fread(block, 1, 1024, file);

                code = gsapi_run_string_continue(instance, block, len,
                                                 0, &error_code);
                if (code < 0) {
                    printf("Error %d in gsapi_run_string_continue\n", code);
                    goto fail;
                }
            }
        }

        code = gsapi_run_string_end(instance, 0, &error_code);
        if (code < 0) {
            printf("Error %d in gsapi_run_string_end\n", code);
            goto fail;
        }
    }
    va_end(args);

    /* Close the interpreter down (important, or we will leak!) */
    code = gsapi_exit(instance);
    if (code < 0) {
        printf("Error %d in gsapi_exit\n", code);
        goto fail;
    }

fail:
    /* Delete the gs instance. */
    gsapi_delete_instance(instance);

failearly:

    return code;
}

char *types[] = {
    "null",
    "bool",
    "int",
    "float",
    "name",
    "string",
    "long",
    "i64",
    "size_t",
    "parsed"
};

static int
list_params(void *instance)
{
    void *iter = NULL;
    const char *key;
    gs_set_param_type type;
    char buffer[1024];
    int code;

    while ((code = gsapi_enumerate_params(instance, &iter, &key, &type)) == 0) {
        printf("Key=%s, type=%s: ", key, type >= 0 && type <= 9 ? types[type] : "invalid");
        code = gsapi_get_param(instance, key, NULL, gs_spt_parsed);
        if (code < 0)
            break;
        if (code > sizeof(buffer)) {
            printf("<overly long value>\n");
            continue;
        }
        code = gsapi_get_param(instance, key, buffer, gs_spt_parsed);
        if (code < 0)
            break;
        printf("%s\n", buffer);
    }
    return code;
}

static int
param_test(const char *dev, char *outfile)
{
    int code, len;
    void *instance   = NULL;
    char devtext[64];
    char buffer[4096];

    /* Construct the argc/argv to pass to ghostscript. */
    int argc = 0;
    char *argv[10];
    int i;

    sprintf(devtext, "-sDEVICE=%s", dev);
    argv[argc++] = "gpdl";
    argv[argc++] = devtext;
    argv[argc++] = "-o";
    argv[argc++] = outfile;

    /* Create a GS instance. */
    code = gsapi_new_instance(&instance, INSTANCE_HANDLE);
    if (code < 0) {
        printf("Error %d in gsapi_new_instance\n", code);
        goto failearly;
    }

    code = gsapi_set_param(instance, "Foo", "0", gs_spt_parsed);
    if (code < 0) {
        printf("Got error from early param setting.\n");
        goto fail;
    }

    /* List the params: */
    code = list_params(instance);
    if (code < 0) {
        printf("Error %d while listing params\n", code);
        goto fail;
    }

    /* Run our test. */
    code = gsapi_init_with_args(instance, argc, argv);
    if (code < 0) {
        printf("Error %d in gsapi_init_with_args\n", code);
        goto fail;
    }

    code = gsapi_set_param(instance, "Bar", "1", gs_spt_parsed | gs_spt_more_to_come);
    if (code < 0) {
        printf("Error %d in gsapi_set_param\n", code);
        goto fail;
    }

    code = gsapi_set_param(instance, "Baz", "<</Test[0 1 2.3]/Charm(>>)/Vixen<01234567>/Scented/Ephemeral>>", gs_spt_parsed);
    if (code < 0) {
        printf("Error %d in gsapi_set_param\n", code);
        goto fail;
    }

    /* This should fail, as /Baz is not an expected param. */
    code = gsapi_get_param(instance, "Baz", buffer, gs_spt_parsed);
    if (code == -21) {
        printf("Got expected error gsapi_get_param\n");
    } else  {
        printf("Error %d in gsapi_get_param\n", code);
        goto fail;
    }

    i = 32;
    code = gsapi_set_param(instance, "foo", (void *)&i, gs_spt_int);
    if (code < 0) {
        printf("Error %d in gsapi_set_param\n", code);
        goto fail;
    }

    code = gsapi_get_param(instance, "foo", (void *)&i, gs_spt_int);
    if (code == -21)
        printf("Got expected error gsapi_get_param\n");
    else if (code < 0) {
        printf("Error %d in gsapi_set_param\n", code);
        goto fail;
    }

    code = gsapi_set_param(instance, "GrayImageDict", "<</QFactor 0.1 /Blend 0/HSamples [1 1 1 1] /VSamples [ 1 1 1 1 ] /Foo[/A/B/C/D/E] /Bar (123) /Baz <0123> /Sp#20ce /D#7fl>>", gs_spt_parsed);
    if (code < 0) {
        printf("Error %d in gsapi_set_param\n", code);
        goto fail;
    }

    code = gsapi_get_param(instance, "GrayImageDict", NULL, gs_spt_parsed);
    if (code < 0) {
        printf("Error %d in gsapi_get_param\n", code);
        goto fail;
    }
    len = code;
    buffer[len-1] = 98;
    buffer[len] = 99;
    code = gsapi_get_param(instance, "GrayImageDict", buffer, gs_spt_parsed);
    if (code < 0) {
        printf("Error %d in gsapi_get_param\n", code);
        goto fail;
    }
    if (buffer[len] != 99 || buffer[len-1] != 0) {
        printf("Bad buffer return");
        goto fail;
    }
    if (strcmp(buffer, "<</Sp#20ce/D#7Fl/Baz<0123>/Bar(123)/Foo[/A/B/C/D/E]/VSamples[1 1 1 1]/HSamples[1 1 1 1]/Blend 0/QFactor 0.1>>")) {
        printf("Bad value return");
        goto fail;
    }

    /* List the params: */
    code = list_params(instance);
    if (code < 0) {
        printf("Error %d in while listing params\n", code);
        goto fail;
    }

    /* Close the interpreter down (important, or we will leak!) */
    code = gsapi_exit(instance);
    if (code < 0) {
        printf("Error %d in gsapi_exit\n", code);
        goto fail;
    }

fail:
    /* Delete the gs instance. */
    gsapi_delete_instance(instance);

failearly:

    return code;
}

static int
res_change_test(const char *dev, char *outfile)
{
    int code, dummy;
    void *instance   = NULL;
    char devtext[64];

    /* Construct the argc/argv to pass to ghostscript. */
    int argc = 0;
    char *argv[10];

    sprintf(devtext, "-sDEVICE=%s", dev);
    argv[argc++] = "gpdl";
    argv[argc++] = devtext;
    argv[argc++] = "-o";
    argv[argc++] = outfile;
    argv[argc++] = "-r100";
    argv[argc++] = "../../examples/tiger.eps";

    /* Create a GS instance. */
    code = gsapi_new_instance(&instance, INSTANCE_HANDLE);
    if (code < 0) {
        printf("Error %d in gsapi_new_instance\n", code);
        goto failearly;
    }

    /* Run our test. */
    code = gsapi_init_with_args(instance, argc, argv);
    if (code < 0) {
        printf("Error %d in gsapi_init_with_args\n", code);
        goto fail;
    }

    code = gsapi_set_param(instance, "HWResolution", "[200 200]", gs_spt_parsed);
    if (code < 0) {
        printf("Error %d in gsapi_set_param\n", code);
        goto fail;
    }

    code = gsapi_run_file(instance, "../../examples/tiger.eps", 0, &dummy);
    if (code < 0) {
        printf("Error %d in gsapi_run_file\n", code);
        goto fail;
    }

    /* Close the interpreter down (important, or we will leak!) */
    code = gsapi_exit(instance);
    if (code < 0) {
        printf("Error %d in gsapi_exit\n", code);
        goto fail;
    }

fail:
    /* Delete the gs instance. */
    gsapi_delete_instance(instance);

failearly:

    return code;
}

int main(int argc, char *argv[])
{
    int code = 0;

#define RUNTEST(A)\
    if (code >= 0) code = (A)

    RUNTEST(param_test("pdfwrite", "apitest20.pdf"));
    RUNTEST(res_change_test("ppmraw", "apitest21_%d.ppm"));

#define RS(A)\
    RUNTEST(runstring_test A )

    RS(("pdfwrite", "apitest12.pdf", "../../examples/tiger.eps", NULL));
    RS(("pdfwrite", "apitest13.pdf", "../../examples/golfer.eps", NULL));
    RS(("pdfwrite", "apitest14.pdf", "../../examples/tiger.eps",
                                     "../../examples/golfer.eps",
                                     NULL));
#if GHOSTPDL
    RS(("pdfwrite", "apitest15.pdf", "../../pcl/examples/tiger.px3", NULL));
    RS(("pdfwrite", "apitest16.pdf", "../../xps/tools/tiger.xps", NULL));
    RS(("pdfwrite", "apitest17.pdf", "../../pcl/examples/tiger.px3",
                                     "../../examples/golfer.eps",
                                     "../../xps/tools/tiger.xps",
                                     NULL));
    RS(("pdfwrite", "apitest18.pdf", "../../zlib/zlib.3.pdf", NULL));
    RS(("pdfwrite", "apitest19.pdf", "../../pcl/examples/tiger.px3",
                                     "../../examples/tiger.eps",
                                     "../../examples/golfer.eps",
                                     "../../zlib/zlib.3.pdf",
                                     "../../xps/tools/tiger.xps",
                                     NULL));
#endif

#define DD(STR, FMT, PS, FILE)\
    RUNTEST(displaydev_test(STR, FMT, PS, FILE))

    /* Run a variety of tests for the display device. */
    DD("Chunky Windows Gray", 0x030802, 0, "apitest0");
    DD("Chunky Windows RGB",  0x030804, 0, "apitest1");
    /* Display device does not support "little endian" CMYK */
    DD("Chunky Windows CMYK", 0x020808, 0, "apitest2");

    DD("Planar Windows Gray", 0x830802, 0, "apitest3");
    DD("Planar Windows RGB",  0x830804, 0, "apitest4");
    DD("Planar Windows CMYK", 0x820808, 0, "apitest5");

    DD("Planar Interleaved Windows Gray", 0x1030802, 0, "apitest6");
    DD("Planar Interleaved Windows RGB",  0x1030804, 0, "apitest7");
    DD("Planar Interleaved Windows CMYK", 0x1020808, 0, "apitest8");

    DD("Chunky Spots (PS)", 0x0A0800, 1, "apitest9");
    DD("Planar Spots (PS)", 0x8A0800, 1, "apitest10");
    DD("Planar Interleaved Spots (PS)", 0x10A0800, 1, "apitest11");
    DD("Chunky Spots (PDF)", 0x0A0800, 0, "apitest12");
    DD("Planar Spots (PDF)", 0x8A0800, 0, "apitest13");
    DD("Planar Interleaved Spots", 0x10A0800, 0, "apitest14");

    return 0;
}
