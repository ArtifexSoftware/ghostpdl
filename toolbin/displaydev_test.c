#ifdef _WIN32
/* Stop windows builds complaining about sprintf being insecure. */
#define _CRT_SECURE_NO_WARNINGS
/* Ensure the dll import works correctly. */
#define _WINDOWS_
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <limits.h>

//#include "pcl/pl/plapi.h"   /* GSAPI - gpdf version */
#include "psi/iapi.h"       /* GSAPI - ghostscript version */
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
           width, height, raster, format, pimage);
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

    ret = malloc(size);
    printf("memalloc: %"FMT_Z" -> %p\n", Z_CAST size, ret);

    return ret;
}

static int
memfree(void *handle, void *device, void *mem)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    printf("memfree: %p\n", mem);
    free(mem);

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
    printf("adjust_band_height: %d - >", bandheight);

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
        free(ts->mem);
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
    ts->mem = malloc(size);
    *memory = ts->mem;

    printf("x=%d y=%d w=%d h=%d mem=%p\n", *x, *y, *w, *h, *memory);
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
static int do_test(const char *title, int format,
                   int use_clist, int legacy,
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
    char *argv[10];

    argv[argc++] = "gs";
    argv[argc++] = "-sDEVICE=display";
    argv[argc++] = "-dNOPAUSE";
    argv[argc++] = format_arg;
    if (legacy)
        argv[argc++] = handle_arg;
    if (format & DISPLAY_COLORS_SEPARATION)
        argv[argc++] = "examples/spots.ps";
    else
        argv[argc++] = "examples/tiger.eps";

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
        teststate.align = 8;

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

    return (code < 0);
}

static int test(const char *title, int format, const char *fname)
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
                code = do_test(title, form, use_clist, legacy, fname);
                if (code < 0)
                    return code;
            }
        }
    }
    return code;
}

int main(int argc, char *argv[])
{
    int code = 0;

#define RUNTEST(STR, FMT, FILE)\
    if (code >= 0) code = test(STR, FMT, FILE)

    /* Run a variety of tests. */
    RUNTEST("Chunky Windows Gray", 0x030802, "ddtest0");
    RUNTEST("Chunky Windows RGB",  0x030804, "ddtest1");
    /* Display device does no support "little endian" CMYK */
    RUNTEST("Chunky Windows CMYK", 0x020808, "ddtest2");

    RUNTEST("Planar Windows Gray", 0x830802, "ddtest3");
    RUNTEST("Planar Windows RGB",  0x830804, "ddtest4");
    RUNTEST("Planar Windows CMYK", 0x820808, "ddtest5");

    RUNTEST("Planar Interleaved Windows Gray", 0x1030802, "ddtest6");
    RUNTEST("Planar Interleaved Windows RGB",  0x1030804, "ddtest7");
    RUNTEST("Planar Interleaved Windows CMYK", 0x1020808, "ddtest8");

    RUNTEST("Chunky Spots", 0x0A0800, "ddtest9");
    RUNTEST("Planar Spots", 0x8A0800, "ddtest10");
    RUNTEST("Planar Interleaved Spots", 0x10A0800, "ddtest11");

    return 0;
}
