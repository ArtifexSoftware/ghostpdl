/* Copyright (C) 2020-2024 Artifex Software, Inc.
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

/* This file is the veneer between GS and Leptonica/Tesseract.
 *
 * Leptonica's memory handling is intercepted via
 * leptonica_{malloc,free,calloc,realloc} (though the last of these
 * is not used) and forwarded to the GS memory handlers. Leptonica
 * only makes calls when we're calling it, hence we use a leptonica_mem
 * global to store the current memory pointer in. This will clearly not
 * play nicely with multi-threaded use of Ghostscript, but that seems
 * unlikely with OCR.
 *
 * Tesseract is trickier. For a start it uses new/delete/new[]/delete[]
 * rather than malloc free. That's OK, cos we can intercept this - see
 * the #ifdef TESSERACT_CUSTOM_ALLOCATOR section at the end of this file
 * for how.
 *
 * The larger problem is that on startup, there is lots of 'static init'
 * done in tesseract, which involves the system calling new. That happens
 * before we have even entered main, so it's impossible to use any other
 * allocator other than malloc.
 *
 * For now, we'll live with this; I believe that most of the 'bulk'
 * allocations (pixmaps etc) are done via Leptonica. If we have an integrator
 * that really needs to avoid malloc/free, then the section of code enclosed
 * in #ifdef TESSERACT_CUSTOM_ALLOCATOR at the end of this file can be used,
 * and tesseract_malloc/tesseract_free can be changed as required.
 */


#include "tesseract/baseapi.h"

#if defined(OCR_SHARED) && OCR_SHARED == 1
#if defined(TESSERACT_MAJOR_VERSION) && TESSERACT_MAJOR_VERSION <= 4
#define USE_TESS_5_API 0
#include "tesseract/genericvector.h"
#else
#define USE_TESS_5_API 1
#endif
#else
#define USE_TESS_5_API 1
#endif

extern "C"
{

#include "allheaders.h"
#include "stdpre.h"
#include "tessocr.h"
#include "gserrors.h"
#include "gp.h"
#include "gssprintf.h"
#include "gxiodev.h"
#include "stream.h"
#include <climits>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#undef DEBUG_ALLOCS
#ifdef DEBUG_ALLOCS
#undef printf
static int event = 0;
#endif

#ifndef FUTURE_DEVELOPMENT
#define FUTURE_DEVELOPMENT 0
#endif

/* Hackily define prototypes for alloc routines for leptonica. */
extern "C" void *leptonica_malloc(size_t blocksize);
extern "C" void *leptonica_calloc(size_t numelm, size_t elemsize);
extern "C" void *leptonica_realloc(void *ptr, size_t blocksize);
extern "C" void leptonica_free(void *ptr);

typedef struct
{
    gs_memory_t *mem;
    tesseract::TessBaseAPI *api;
} wrapped_api;


static gs_memory_t *leptonica_mem;

void *leptonica_malloc(size_t size)
{
    void *ret = gs_alloc_bytes(leptonica_mem, size, "leptonica_malloc");
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_MALLOC(%p) %d -> %p\n", event++, leptonica_mem, (int)size, ret);
    fflush(stdout);
#endif
    return ret;
}

void leptonica_free(void *ptr)
{
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_FREE(%p) %p\n", event++, leptonica_mem, ptr);
    fflush(stdout);
#endif
    gs_free_object(leptonica_mem, ptr, "leptonica_free");
}

void *leptonica_calloc(size_t numelm, size_t elemsize)
{
    void *ret = leptonica_malloc(numelm * elemsize);

    if (ret)
        memset(ret, 0, numelm * elemsize);
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_CALLOC %d,%d -> %p\n", event++, (int)numelm, (int)elemsize, ret);
    fflush(stdout);
#endif
    return ret;
}

void *leptonica_realloc(void *ptr, size_t blocksize)
{
    /* Never called in our usage. */
    return NULL;
}

/* Convert from gs format bitmaps to leptonica format bitmaps. */
static int convert2pix(l_uint32 *data, int w, int h, int raster)
{
    int x;
    int w4 = ((w+3)>>2)-1;
    int extra = raster - w >= 4;
    l_uint32 mask = ~(0xFFFFFFFF<<((w&3)*8));

    for (; h > 0; h--) {
        l_uint32 v;
        for (x = w4; x > 0; x--) {
            v = *data;
            *data++ = (v>>24) | ((v & 0xff0000)>>8) | ((v & 0xff00)<<8) | (v<<24);
        }
        v = *data;
        *data++ = (v>>24) | ((v & 0xff0000)>>8) | ((v & 0xff00)<<8) | (v<<24) | mask;
        if (extra)
            *data++ = 0xFFFFFFFF;
    }

    return w + extra*4;
}

#if USE_TESS_5_API
static bool
load_file(const char* filename, std::vector<char> *data)
{
#else
static bool
load_file(const STRING& fname, GenericVector<char> *data)
{
  const char *filename = (const char *)fname.string();
#endif
  bool result = false;
  gp_file *fp;
  int code;
  int size;

  code = gs_add_control_path(leptonica_mem, gs_permit_file_reading, filename);
  if (code < 0)
    return false;

  fp = gp_fopen(leptonica_mem, filename, "rb");
  if (fp == NULL)
    goto fail;

  gp_fseek(fp, 0, SEEK_END);
  size = (int)gp_ftell(fp);
  gp_fseek(fp, 0, SEEK_SET);
  // Trying to open a directory on Linux sets size to LONG_MAX. Catch it here.
  if (size > 0 && size < LONG_MAX) {
    // reserve an extra byte in case caller wants to append a '\0' character
    data->reserve(size + 1);
#if USE_TESS_5_API
    data->resize(size);
#else
    data->resize_no_init(size);
#endif
    result = static_cast<long>(gp_fread(&(*data)[0], 1, size, fp)) == size;
  }
  gp_fclose(fp);

fail:
  (void)gs_remove_control_path(leptonica_mem, gs_permit_file_reading, filename);

  return result;
}

#if USE_TESS_5_API
static bool
load_file_from_path(const char *path, const char *file, std::vector<char> *out)
#else
static bool
load_file_from_path(const char *path, const char *file, GenericVector<char>  *out)
#endif
{
    const char *sep = gp_file_name_directory_separator();
    size_t seplen = strlen(sep);
    size_t bufsize = strlen(path) + seplen + strlen(file) + 1;
    const char *s, *e;
    bool ret = 0;
    char *buf = (char *)gs_alloc_bytes(leptonica_mem, bufsize, "load_file_from_path");
    if (buf == NULL)
        return 0;

    s = path;
    do {
        e = path;
        while (*e && *e != gp_file_name_list_separator)
            e++;
        memcpy(buf, s, e-s);
        memcpy(&buf[e-s], sep, seplen);
        strcpy(&buf[e-s+seplen], file);
        ret = load_file(buf, out);
        if (ret)
            break;
        s = e;
        while (*s == gp_file_name_list_separator)
            s++;
    } while (*s != 0);

    gs_free_object(leptonica_mem, buf, "load_file_from_path");

    return ret;
}

#ifndef TESSDATA
#define TESSDATA tessdata
#endif
#define STRINGIFY2(S) #S
#define STRINGIFY(S) STRINGIFY2(S)
static char *tessdata_prefix = (char *)STRINGIFY(TESSDATA);

#if USE_TESS_5_API
static bool
tess_file_reader(const char *fname, std::vector<char> *out)
{
#else
static bool
tess_file_reader(const STRING& fname_str, GenericVector<char>*out)
{
    const char *fname = fname_str.string();
#endif
    const char *file = fname;
    const char *s;
    char text[PATH_MAX];
    int code = 0;
    bool found;
    stream *ps;
    gx_io_device *iodev;

    /* fname, as supplied to us by Tesseract has TESSDATA_PREFIX prepended
     * to it. Check that first. */
    found = load_file(fname, out);
    if (found)
            return found;

    /* Find file, fname with any prefix removed, and use that in
     * the rest of the searches. */
    for (s = fname; *s; s++)
        if (*s == '\\' || *s == '/')
            file = s+1;

    /* Next look in romfs in the tessdata directory. */
    iodev = gs_findiodevice(leptonica_mem, (const byte *)"%rom", 4);
    gs_snprintf(text, sizeof(text), "tessdata/%s", file);
    if (iodev) {
        long size;
        long i;
        byte *copy;
        /* We cannot call iodev->procs.file_status here to get the
         * length, because C and C++ differ in their definition of
         * stat on linux. */
        size = (long)romfs_file_len(leptonica_mem, text);
        if (size >= 0) {
            out->reserve(size + 1);
#if USE_TESS_5_API
            out->resize(size);
#else
            out->resize_no_init(size);
#endif
            code = iodev->procs.open_file(iodev, text, strlen(text), "rb", &ps, leptonica_mem);
            if (code < 0)
                return code;
            copy = (byte *)&(*out)[0];
            i = 0;
            while (i < size) {
                long a, n = size - i;
                s_process_read_buf(ps);
                a = sbufavailable(ps);
                if (n > a)
                    n = a;
                memcpy(copy+i, sbufptr(ps), a);
                i += a;
                sbufskip(ps, a);
            }
            sclose(ps);
            gs_free_object(leptonica_mem, ps, "stream(tess_file_reader)");
            return true;
        }
    }

    /* Fall back to gp_file access under our configured tessdata path. */
    found = load_file_from_path(tessdata_prefix, file, out);
    if (found)
        return found;

    /* If all else fails, look in the current directory. */
    return load_file(file, out);
}

int
ocr_init_api(gs_memory_t *mem, const char *language, int engine, void **state)
{
    enum tesseract::OcrEngineMode mode;
    wrapped_api *wrapped;
    int code = 0;

    if (mem->non_gc_memory != mem) {
        dlprintf("ocr_init_api must not be called with gc controlled memory!\n");
        return_error(gs_error_unknownerror);
    }

    wrapped = (wrapped_api *)(void *)gs_alloc_bytes(mem, sizeof(*wrapped), "ocr_init_api");
    if (wrapped == NULL)
        return gs_error_VMerror;

    leptonica_mem = mem;
    setPixMemoryManager(leptonica_malloc, leptonica_free);

    wrapped->mem = mem;
    wrapped->api = new tesseract::TessBaseAPI();

    *state = NULL;

    if (wrapped->api == NULL) {
        code = gs_error_VMerror;
        goto fail;
    }

    if (language == NULL || language[0] == 0) {
        language = "eng";
    }

    switch (engine)
    {
        case OCR_ENGINE_DEFAULT:
            mode = tesseract::OcrEngineMode::OEM_DEFAULT;
            break;
        case OCR_ENGINE_LSTM:
            mode = tesseract::OcrEngineMode::OEM_LSTM_ONLY;
            break;
        case OCR_ENGINE_LEGACY:
            mode = tesseract::OcrEngineMode::OEM_TESSERACT_ONLY;
            break;
        case OCR_ENGINE_BOTH:
            mode = tesseract::OcrEngineMode::OEM_TESSERACT_LSTM_COMBINED;
            break;
        default:
            code = gs_error_rangecheck;
            goto fail;
    }

    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (wrapped->api->Init(NULL, 0, /* data, data_size */
                           language,
                           mode,
                           NULL, 0, /* configs, configs_size */
                           NULL, NULL, /* vars_vec */
                           false, /* set_only_non_debug_params */
                           (tesseract::FileReader)&tess_file_reader)) {
        code = gs_error_unknownerror;
        goto fail;
    }

    *state = (void *)wrapped;

    return 0;
fail:
    if (wrapped->api) {
        delete wrapped->api;
    }
    leptonica_mem = NULL;
    setPixMemoryManager(malloc, free);
    gs_free_object(wrapped->mem, wrapped, "ocr_init_api");
    return_error(code);
}

void
ocr_fin_api(gs_memory_t *mem, void *api_)
{
    wrapped_api *wrapped = (wrapped_api *)api_;

    if (wrapped == NULL)
        return;

    if (wrapped->api) {
        wrapped->api->End();
        delete wrapped->api;
    }
    gs_free_object(wrapped->mem, wrapped, "ocr_fin_api");
    leptonica_mem = NULL;
    setPixMemoryManager(malloc, free);
}

static Pix *
ocr_set_image(tesseract::TessBaseAPI *api,
              int w, int h, void *data, int xres, int yres)
{
    Pix *image = pixCreateHeader(w, h, 8);

    if (image == NULL)
        return NULL;
    pixSetData(image, (l_uint32 *)data);
    pixSetPadBits(image, 1);
    pixSetXRes(image, xres);
    pixSetYRes(image, yres);
    api->SetImage(image);
    //pixWrite("test.pnm", image, IFF_PNM);

    return image;
}

static void
ocr_clear_image(Pix *image)
{
    pixSetData(image, NULL);
    pixDestroy(&image);
}

static int
do_ocr_image(wrapped_api *wrapped,
             int w, int h, int bpp, int raster,
             int xres, int yres, void *data, int restore,
             int hocr, int pagecount,
             char **out)
{
    char *outText;
    Pix *image;

    *out = NULL;

    if (bpp == 8)
        w = convert2pix((l_uint32 *)data, w, h, raster);

    image = ocr_set_image(wrapped->api, w, h, data, xres, yres);
    if (image == NULL) {
        if (restore && bpp == 8)
            convert2pix((l_uint32 *)data, w, h, raster);
        return_error(gs_error_VMerror);
    }

    // Get OCR result
    //pixWrite("test.pnm", image, IFF_PNM);
    if (hocr) {
        wrapped->api->SetVariable("hocr_font_info", "true");
        wrapped->api->SetVariable("hocr_char_boxes", "true");
        outText = wrapped->api->GetHOCRText(pagecount);
    }
    else
        outText = wrapped->api->GetUTF8Text();

    ocr_clear_image(image);

    /* Convert the image back. */
    if (restore && bpp == 8)
        w = convert2pix((l_uint32 *)data, w, h, raster);

    // Copy the results into a gs controlled block.
    if (outText)
    {
        size_t len = strlen(outText)+1;
        *out = (char *)(void *)gs_alloc_bytes(wrapped->mem, len, "ocr_to_utf8");
        if (*out)
            memcpy(*out, outText, len);
    }

    delete [] outText;

    return 0;
}

int ocr_image_to_hocr(void *api,
                      int w, int h, int bpp, int raster,
                      int xres, int yres, void *data, int restore,
                      int pagecount, char **out)
{
        return do_ocr_image((wrapped_api *)api,
                            w, h, bpp, raster, xres, yres, data,
                            restore, 1, pagecount, out);
}

int ocr_image_to_utf8(void *api,
                      int w, int h, int bpp, int raster,
                      int xres, int yres, void *data, int restore,
                      char **out)
{
        return do_ocr_image((wrapped_api *)api,
                            w, h, bpp, raster, xres, yres, data,
                            restore, 0, 0, out);
}

int
ocr_recognise(void *api_, int w, int h, void *data,
              int xres, int yres,
              int (*callback)(void *, const char *, const int *, const int *, const int *, int),
              void *arg)
{
    wrapped_api *wrapped = (wrapped_api *)api_;
    Pix *image;
    int code;
    int word_bbox[4];
    int char_bbox[4];
    int line_bbox[4];
    bool bold, italic, underlined, monospace, serif, smallcaps;
    int pointsize, font_id;
    const char* font_name;

    if (wrapped == NULL || wrapped->api == NULL)
        return 0;

    image = ocr_set_image(wrapped->api, w, h, data, xres, yres);
    if (image == NULL)
        return_error(gs_error_VMerror);

    code = wrapped->api->Recognize(NULL);
    if (code >= 0) {
        /* Bingo! */
        tesseract::ResultIterator *res_it = wrapped->api->GetIterator();

        while (!res_it->Empty(tesseract::RIL_BLOCK)) {
            if (res_it->Empty(tesseract::RIL_WORD)) {
                res_it->Next(tesseract::RIL_WORD);
                continue;
            }

            res_it->BoundingBox(tesseract::RIL_TEXTLINE,
                                line_bbox, line_bbox+1,
                                line_bbox+2, line_bbox+3);
            res_it->BoundingBox(tesseract::RIL_WORD,
                                word_bbox, word_bbox+1,
                                word_bbox+2, word_bbox+3);
            font_name = res_it->WordFontAttributes(&bold,
                                                   &italic,
                                                   &underlined,
                                                   &monospace,
                                                   &serif,
                                                   &smallcaps,
                                                   &pointsize,
                                                   &font_id);
            (void)font_name;
            do {
                const char *graph = res_it->GetUTF8Text(tesseract::RIL_SYMBOL);
                if (graph && graph[0] != 0) {
                    res_it->BoundingBox(tesseract::RIL_SYMBOL,
                                        char_bbox, char_bbox+1,
                                        char_bbox+2, char_bbox+3);
                    code = callback(arg, graph, line_bbox, word_bbox, char_bbox, pointsize);
                    if (code < 0)
                    {
                        delete res_it;
                        return code;
                    }
                }
                res_it->Next(tesseract::RIL_SYMBOL);
             } while (!res_it->Empty(tesseract::RIL_BLOCK) &&
                      !res_it->IsAtBeginningOf(tesseract::RIL_WORD));
        }
        delete res_it;
        code = code;
    }

    ocr_clear_image(image);

    return code;
}

static Pix *
ocr_set_bitmap(wrapped_api *wrapped,
               int w, int h,
               const unsigned char *data, int data_x, int raster,
               int xres, int yres)
{
    /* Tesseract prefers a border around things, so we add an 8 pixel
     * border all around. */
#define BORDER_SIZE 8
    size_t r = ((size_t)w+BORDER_SIZE*2+3)&~3;
    Pix *image = pixCreateHeader(r, h+BORDER_SIZE*2, 8);
    unsigned char *pdata, *d;
    const unsigned char *s;
    int x, y;

    if (image == NULL)
        return NULL;

    pdata = gs_alloc_bytes(wrapped->mem, r * (h+BORDER_SIZE*2), "ocr_set_bitmap");
    if (pdata == NULL) {
        pixDestroy(&image);
        return NULL;
    }
    pixSetData(image, (l_uint32 *)pdata);
    pixSetPadBits(image, 1);
    pixSetXRes(image, xres);
    pixSetYRes(image, yres);

    s = &data[data_x>>3] + raster*(h-1);
    d = pdata;
    memset(d, 255, r * (h+BORDER_SIZE*2));
    d += r*BORDER_SIZE + BORDER_SIZE;
    for (y = 0; y < h; y++) {
        int b = 128>>(data_x & 7);
        for (x = 0; x < w; x++) {
            if (s[x>>3] & b)
                d[x^3] = 0;
            else
                d[x^3] = 255;
            b >>= 1;
            if (b == 0)
                b = 128;
        }
        s -= raster;
        d += r;
    }

    wrapped->api->SetImage(image);
//    pixWrite("test.pnm", image, IFF_PNM);

    return image;
}

static void
ocr_clear_bitmap(wrapped_api *wrapped, Pix *image)
{
    gs_free_object(wrapped->mem, pixGetData(image), "ocr_clear_bitmap");
    pixSetData(image, NULL);
    pixDestroy(&image);
}

int ocr_bitmap_to_unicodes(void *state,
                          const void *data, int data_x,
                          int w, int h, int raster,
                          int xres, int yres, int *unicode, int *char_count)
{
    wrapped_api *wrapped = (wrapped_api *)state;
    Pix *image;
    int code, max_chars = *char_count, count = 0;

    if (wrapped == NULL || wrapped->api == NULL)
        return 0;

    image = ocr_set_bitmap(wrapped, w, h, (const unsigned char *)data,
                           data_x, raster, xres, yres);
    if (image == NULL)
        return_error(gs_error_VMerror);

    code = wrapped->api->Recognize(NULL);
    if (code >= 0) {
        /* Bingo! */
        tesseract::ResultIterator *res_it = wrapped->api->GetIterator();

        while (!res_it->Empty(tesseract::RIL_BLOCK)) {
            if (res_it->Empty(tesseract::RIL_WORD)) {
                res_it->Next(tesseract::RIL_WORD);
                continue;
            }

            do {
#if FUTURE_DEVELOPMENT
                int word_bbox[4];
                int char_bbox[4];
                int line_bbox[4];
#endif

                const unsigned char *graph = (unsigned char *)res_it->GetUTF8Text(tesseract::RIL_SYMBOL);
                if (graph && graph[0] != 0) {
                    /* Quick and nasty conversion from UTF8 to unicode. */
                    if (graph[0] < 0x80)
                        unicode[count] = graph[0];
                    else {
                        unicode[count] = graph[1] & 0x3f;
                        if (graph[0] < 0xE0)
                            unicode[count] += (graph[0] & 0x1f)<<6;
                        else {
                            unicode[count] = (graph[2] & 0x3f) | (*unicode << 6);
                            if (graph[0] < 0xF0) {
                                unicode[count] += (graph[0] & 0x0F)<<6;
                            } else {
                                unicode[count] = (graph[3] & 0x3f) | (*unicode<<6);
                                unicode[count] += (graph[0] & 0x7);
                            }
                        }
                    }
                    count++;
#if FUTURE_DEVELOPMENT
                    res_it->BoundingBox(tesseract::RIL_TEXTLINE,
                        line_bbox,line_bbox + 1,
                        line_bbox + 2,line_bbox + 3);
                    res_it->BoundingBox(tesseract::RIL_WORD,
                        word_bbox,word_bbox + 1,
                        word_bbox + 2,word_bbox + 3);
                    res_it->BoundingBox(tesseract::RIL_SYMBOL,
                        char_bbox,char_bbox + 1,
                        char_bbox + 2,char_bbox + 3);
#endif
                }
                res_it->Next(tesseract::RIL_SYMBOL);
             } while (!res_it->Empty(tesseract::RIL_BLOCK) &&
                      !res_it->IsAtBeginningOf(tesseract::RIL_WORD) && count < max_chars);
        }
        delete res_it;
        code = code;
    }

    ocr_clear_bitmap(wrapped, image);
    *char_count = count;

    return code;
}

};

/* The following code is disabled by default. If enabled, nothing
 * should change, except integrators can tweak tesseract_malloc
 * and tesseract_free as required to avoid calling the normal
 * system malloc/free. */
#ifdef TESSERACT_CUSTOM_ALLOCATOR

static void *tesseract_malloc(size_t blocksize)
{
    void *ret;

    ret = malloc(blocksize);
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_MALLOC %d -> %p\n", event++, (int)blocksize, ret);
    fflush(stdout);
#endif
    return ret;
}

static void tesseract_free(void *ptr)
{
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_FREE %p\n", event++, ptr);
    fflush(stdout);
#endif
    free(ptr);
}

/* Currently tesseract is the only C++ lib we have.
 * We may need to revisit this if this changes.
 */
void *operator new(size_t size)
{
    return tesseract_malloc(size);
}

void operator_delete(void *ptr)
{
    tesseract_free(ptr);
}

void *operator new[](size_t size)
{
    return tesseract_malloc(size);
}

void operator delete[](void *ptr)
{
    tesseract_free(ptr);
}

#endif
