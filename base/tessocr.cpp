#include "tesseract/baseapi.h"
#include "tesseract/genericvector.h"
#include "tesseract/serialis.h"

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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#undef DEBUG_ALLOCS
#ifdef DEBUG_ALLOCS
#undef printf
static int event = 0;
#endif

/* Hackily define prototypes for alloc routines for leptonica. */
extern "C" void *leptonica_malloc(size_t blocksize);
extern "C" void *leptonica_calloc(size_t numelm, size_t elemsize);
extern "C" void *leptonica_realloc(void *ptr, size_t blocksize);
extern "C" void leptonica_free(void *ptr);

void *leptonica_malloc(size_t blocksize)
{
    void *ret = malloc(blocksize);
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_MALLOC %d -> %p\n", event++, (int)blocksize, ret);
    fflush(stdout);
#endif
    return ret;
}

void *leptonica_calloc(size_t numelm, size_t elemsize)
{
    void *ret = calloc(numelm, elemsize);
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_CALLOC %d,%d -> %p\n", event++, (int)numelm, (int)elemsize, ret);
    fflush(stdout);
#endif
    return ret;
}

void *leptonica_realloc(void *ptr, size_t blocksize)
{
    void *ret = realloc(ptr, blocksize);
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_REALLOC %p,%d -> %p\n", event++, ptr, (int)blocksize, ret);
    fflush(stdout);
#endif
    return ret;
}

void leptonica_free(void *ptr)
{
#ifdef DEBUG_ALLOCS
    printf("%d LEPTONICA_FREE %p\n", event++, ptr);
    fflush(stdout);
#endif
    free(ptr);
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

static gs_memory_t *leptonica_mem;

static void *my_leptonica_malloc(size_t size)
{
    void *ret = gs_alloc_bytes(leptonica_mem, size, "leptonica_malloc");
#ifdef DEBUG_ALLOCS
    printf("%d MY_LEPTONICA_MALLOC(%p) %d -> %p\n", event++, leptonica_mem, (int)size, ret);
    fflush(stdout);
#endif
    return ret;
}

static void my_leptonica_free(void *ptr)
{
#ifdef DEBUG_ALLOCS
    printf("%d MY_LEPTONICA_FREE(%p) %p\n", event++, leptonica_mem, ptr);
    fflush(stdout);
#endif
    gs_free_object(leptonica_mem, ptr, "leptonica_free");
}

static bool
load_file(const char* filename, GenericVector<char>* data) {
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
    data->resize_no_init(size);
    result = static_cast<long>(gp_fread(&(*data)[0], 1, size, fp)) == size;
  }
  gp_fclose(fp);

fail:
  (void)gs_remove_control_path(leptonica_mem, gs_permit_file_reading, filename);

  return result;
}

static bool
load_file_from_path(const char *path, const char *file, GenericVector<char> *out)
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
static char *tessdata_prefix = STRINGIFY(TESSDATA);

static bool
tess_file_reader(const char *fname, GenericVector<char> *out)
{
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
            out->resize_no_init(size);
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
    tesseract::TessBaseAPI *api;
    enum tesseract::OcrEngineMode mode;

    leptonica_mem = mem->non_gc_memory;
    setPixMemoryManager(my_leptonica_malloc, my_leptonica_free);
    api = new tesseract::TessBaseAPI();

    *state = NULL;

    if (api == NULL) {
        leptonica_mem = NULL;
        setPixMemoryManager(malloc, free);
        return_error(gs_error_VMerror);
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
            return_error(gs_error_rangecheck);
    }

    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (api->Init(NULL, 0, /* data, data_size */
                  language,
                  mode,
                  NULL, 0, /* configs, configs_size */
                  NULL, NULL, /* vars_vec */
                  false, /* set_only_non_debug_params */
                  &tess_file_reader)) {
        delete api;
        leptonica_mem = NULL;
        setPixMemoryManager(malloc, free);
        return_error(gs_error_unknownerror);
    }

    *state = (void *)api;

    return 0;
}

void
ocr_fin_api(gs_memory_t *mem, void *api_)
{
    tesseract::TessBaseAPI *api = (tesseract::TessBaseAPI *)api_;

    if (api == NULL)
        return;

    api->End();
    delete api;
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
do_ocr_image(gs_memory_t *mem,
             int w, int h, int bpp, int raster,
             int xres, int yres, void *data, int restore,
             int hocr, int pagecount,
             const char *language, int engine, char **out)
{
    char *outText;
    tesseract::TessBaseAPI *api;
    int code;
    Pix *image;

    *out = NULL;

    if (language == NULL || *language == 0)
        language = "eng";
    code = ocr_init_api(mem, language, engine, (void **)&api);
    if (code < 0)
        return code;

    if (bpp == 8)
        w = convert2pix((l_uint32 *)data, w, h, raster);

    image = ocr_set_image(api, w, h, data, xres, yres);
    if (image == NULL) {
        if (restore && bpp == 8)
            convert2pix((l_uint32 *)data, w, h, raster);
        ocr_fin_api(mem, api);
        return_error(gs_error_VMerror);
    }

    // Get OCR result
    //pixWrite("test.pnm", image, IFF_PNM);
    if (hocr) {
        api->SetVariable("hocr_font_info", "true");
        api->SetVariable("hocr_char_boxes", "true");
        outText = api->GetHOCRText(pagecount);
    }
    else
        outText = api->GetUTF8Text();

    ocr_clear_image(image);

    /* Convert the image back. */
    if (restore && bpp == 8)
        w = convert2pix((l_uint32 *)data, w, h, raster);

    // Copy the results into a gs controlled block.
    if (outText)
    {
        size_t len = strlen(outText)+1;
        *out = (char *)(void *)gs_alloc_bytes(mem, len, "ocr_to_utf8");
        if (*out)
            memcpy(*out, outText, len);
    }

    delete [] outText;

    // Destroy used object and release memory
    ocr_fin_api(mem, api);

    return 0;
}

int ocr_image_to_hocr(gs_memory_t *mem,
                      int w, int h, int bpp, int raster,
                      int xres, int yres, void *data, int restore,
                      int pagecount, const char *language,
                      int engine, char **out)
{
        return do_ocr_image(mem, w, h, bpp, raster, xres, yres, data,
                            restore, 1, pagecount, language, engine, out);
}

int ocr_image_to_utf8(gs_memory_t *mem,
                      int w, int h, int bpp, int raster,
                      int xres, int yres, void *data, int restore,
                      const char *language, int engine, char **out)
{
        return do_ocr_image(mem, w, h, bpp, raster, xres, yres, data,
                            restore, 0, 0, language, engine, out);
}

int
ocr_recognise(void *api_, int w, int h, void *data,
              int xres, int yres,
              int (*callback)(void *, const char *, const int *, const int *, const int *, int),
              void *arg)
{
    tesseract::TessBaseAPI *api = (tesseract::TessBaseAPI *)api_;
    Pix *image;
    int code;
    int word_bbox[4];
    int char_bbox[4];
    int line_bbox[4];
    bool bold, italic, underlined, monospace, serif, smallcaps;
    int pointsize, font_id;
    const char* font_name;

    if (api == NULL)
        return 0;

    image = ocr_set_image(api, w, h, data, xres, yres);
    if (image == NULL)
        return_error(gs_error_VMerror);

    code = api->Recognize(NULL);
    if (code >= 0) {
        /* Bingo! */
        tesseract::ResultIterator *res_it = api->GetIterator();

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

};

/* Currently tesseract is the only C++ lib we have.
 * We may need to revisit this if this changes.
 */
void *operator new(size_t size)
{
    return leptonica_malloc(size);
}

void operator_delete(void *ptr)
{
    leptonica_free(ptr);
}

void *operator new[](size_t size)
{
    return leptonica_malloc(size);
}

void operator delete[](void *ptr)
{
    leptonica_free(ptr);
}
