/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

/* Device for creating docx files. */

#include "memory_.h"
#include "string_.h"
#include "gp.h"			/* for gp_file_name_sizeof */
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gxfont.h"
#include "gxfont0.h"
#include "gstext.h"
#include "gxfcid.h"
#include "gxgstate.h"
#include "gxpath.h"
#include "gsagl.h"
#include "gxdevsop.h"
#include "gzpath.h"
#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently First/Last Page and obejct filter */
#include "gxchar.h"

#include "doc_common.h"

#include "../../extract/include/extract.h"

#include <errno.h>

#define outf(format, ...)
#define outfx(format, ...)

#ifdef __unix__
#if 0

/* Enable logging using outf() macro/function. */
#include <unistd.h>

static void (outf)(const char* file, int line, const char* function, const char* format, ...)
{
    static char buffer[2048];
    size_t prefix;
    va_list va;
    snprintf(buffer, sizeof(buffer), "%s:%i:%s(): ", file, line, function);
    prefix = strlen(buffer);
    va_start(va, format);
    vsnprintf(buffer + prefix, sizeof(buffer) - 1 - prefix, format, va);
    va_end(va);
    size_t l = strlen(buffer);
    buffer[l] = '\n';
    buffer[l+1] = 0;
    write(2, buffer, l+1);
}

#undef outf
#define outf(format, ...)  outf(__FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

#endif
#endif



extern single_glyph_list_t SingleGlyphList[];
extern double_glyph_list_t DoubleGlyphList[];
extern treble_glyph_list_t TrebleGlyphList[];
extern quad_glyph_list_t QuadGlyphList[];

/* Structure to record the Unicode characters, the total width of the text
 * recorded, and various useful attributes such as the font, size, colour
 * rendering mode, writing mode etc. These are stored as a series of x-ordered
 * entries in a list, using the starting x co-ordinate.
 */
typedef struct docx_list_entry_t {
    struct text_list_entry_s *previous;
    struct text_list_entry_s *next;
    gs_matrix matrix;		/* Tm et al */
    char *FontName;
    int wmode;			/* WMode of font */
    double size;
} docx_list_entry_t;

/* The custom sub-classed device structure */
typedef struct {
    gx_device_common;
    char fname[gp_file_name_sizeof];	/* OutputFile */
    gp_file *file;
    int TextFormat;
    extract_alloc_t *alloc;
    extract_t *extract;
    int file_per_page;
    float x;    /* Used to maintain x pos as we iterate through a span. */
} gx_device_docxwrite_t;


/* Device procedures */
static dev_proc_open_device(docxwrite_open_device);
static dev_proc_close_device(docxwrite_close_device);
static dev_proc_output_page(docxwrite_output_page);
static dev_proc_fill_rectangle(docxwrite_fill_rectangle);
static dev_proc_get_params(docxwrite_get_params);
static dev_proc_put_params(docxwrite_put_params);
static dev_proc_fill_path(docxwrite_fill_path);
static dev_proc_stroke_path(docxwrite_stroke_path);
static dev_proc_text_begin(docxwrite_text_begin);
static dev_proc_strip_copy_rop2(docxwrite_strip_copy_rop2);
static dev_proc_dev_spec_op(docxwrite_dev_spec_op);


/* The device prototype */
#define X_DPI 72
#define Y_DPI 72

/* Define the text enumerator. */
typedef struct {
    gs_text_enum_common;
    gs_text_enum_t *pte_fallback;
    double d1_width;
    bool d1_width_set;
    bool charproc_accum;
    bool cdevproc_callout;
    double cdevproc_result[10];
    docx_list_entry_t *text_state;
} docxw_text_enum_t;
#define private_st_textw_text_enum()\
  extern_st(st_gs_text_enum);\
  gs_private_st_suffix_add0(st_textw_text_enum, docxw_text_enum_t,\
    "docxw_text_enum_t", textw_text_enum_enum_ptrs, textw_text_enum_reloc_ptrs,\
    st_gs_text_enum)

private_st_textw_text_enum();

static void
docxwrite_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, docxwrite_open_device);
    set_dev_proc(dev, output_page, docxwrite_output_page);
    set_dev_proc(dev, close_device, docxwrite_close_device);
    set_dev_proc(dev, fill_rectangle, docxwrite_fill_rectangle);
    set_dev_proc(dev, get_params, docxwrite_get_params);
    set_dev_proc(dev, put_params, docxwrite_put_params);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, fill_path, docxwrite_fill_path);
    set_dev_proc(dev, stroke_path, docxwrite_stroke_path);
    set_dev_proc(dev, strip_copy_rop2, docxwrite_strip_copy_rop2);
    set_dev_proc(dev, composite, gx_null_composite);
    set_dev_proc(dev, text_begin, docxwrite_text_begin);
    set_dev_proc(dev, dev_spec_op, docxwrite_dev_spec_op);
}

const gx_device_docxwrite_t gs_docxwrite_device =
{
    /* Define the device as 8-bit gray scale to avoid computing halftones. */
    std_device_dci_body(gx_device_docxwrite_t,
                        docxwrite_initialize_device_procs,
                        "docxwrite",
                        DEFAULT_WIDTH_10THS * X_DPI / 10,
                        DEFAULT_HEIGHT_10THS * Y_DPI / 10,
                        X_DPI, Y_DPI,
                        1, 8, 255, 0, 256, 1),
    { 0 },                      /* procs */
    { 0 },			/* Page Data */
    0,				/* Output FILE * */
    0,				/* TextFormat */
    NULL,           /* alloc */
    NULL,			/* extract */
    0,              /* file_per_page */
    0.0             /* x */
};


static const gs_param_item_t docx_param_items[] = {
#define pi(key, type, memb) { key, type, offset_of(gx_device_docxwrite_t, memb) }
    pi("TextFormat", gs_param_type_int, TextFormat),
#undef pi
    gs_param_item_end
};

/* ---------------- Open/close/page ---------------- */


static void *s_extract_realloc_fn(void *state, void *ptr, size_t newsize)
{
    /* We have to go to some effort to behave like realloc() when we only have
    make gs_malloc() and gs_free() available. We store each allocated block's
    allocation size in the preceding size_t. */
    gs_memory_t *mem = state;
    size_t *oldbase = (ptr) ? (size_t*) ptr - 1 : NULL;
    size_t oldsize = (oldbase) ? *oldbase : 0;
    size_t *newbase = NULL;
    if (newsize) {
        /* malloc() or realloc() - allocate new buffer.  */
        newbase = gs_malloc(mem, sizeof(size_t) + newsize /*nelts*/, 1 /*esize*/, "extract");
        if (newbase) {
            *newbase = newsize;
            if (oldbase) {
                /* Realloc, so copy across. */
                size_t minsize = (newsize < oldsize) ? newsize : oldsize;
                memcpy(newbase+1, oldbase+1, minsize);
            }
        }
    }
    if (oldbase && (!newsize || newbase)) {
        /* free() or successful realloc() - free the old buffer. */
        gs_free(mem, oldbase, sizeof(size_t) + oldsize /*nelts*/, 1 /*esize*/, "extract");
    }
    return (newbase) ? newbase + 1 : NULL;
}

static int s_errno_to_gs(void)
{
    if (errno == EPERM) return gs_error_invalidaccess;
    if (errno == ENOENT) return gs_error_invalidfileaccess;
    if (errno == ESRCH) {}
    if (errno == EINTR) {}
    if (errno == EIO) return gs_error_ioerror;
    if (errno == ENXIO) {}

    return gs_error_unknownerror;
}

static int
docxwrite_open_device(gx_device * dev)
{
    gx_device_docxwrite_t * tdev = (gx_device_docxwrite_t *) dev;
    const char* fmt = NULL;
    gs_parsed_file_name_t parsed;
    int code = 0;

    gx_device_fill_in_procs(dev);
    if (tdev->fname[0] == 0)
        return_error(gs_error_undefinedfilename);

    tdev->file = NULL;
    dev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    set_linear_color_bits_mask_shift(dev);
    dev->interpolate_control = 0;

    tdev->alloc = NULL;
    tdev->extract = NULL;

    code = gx_parse_output_file_name(&parsed, &fmt, tdev->fname, strlen(tdev->fname), tdev->memory);
    if (code < 0) goto end;
    tdev->file_per_page = (fmt) ? 1 : 0;

    if (extract_alloc_create(s_extract_realloc_fn, tdev->memory, &tdev->alloc)) {
        code = s_errno_to_gs();
        goto end;
    }
    extract_alloc_exp_min(tdev->alloc, 64);

    if (extract_begin(tdev->alloc, extract_format_DOCX, &tdev->extract)) {
        code = s_errno_to_gs();
        goto end;
    }
    if (extract_page_begin(tdev->extract)) {
        code = s_errno_to_gs();
        goto end;
    }
    code = install_internal_subclass_devices((gx_device **)&dev, NULL);

    end:

    if (code < 0) {
        extract_alloc_destroy(&tdev->alloc);
        extract_end(&tdev->extract);
    }
    return code;
}

/* For use with an extract_buffer_t - writes directly to tdev->file. */
static int docxwrite_extract_buffer_write(void* handle, const void* source, size_t numbytes, size_t* o_actual)
{
    int n;
    gx_device_docxwrite_t *tdev = handle;
    n = gp_fwrite(source, 1 /*size*/, numbytes /*count*/, tdev->file);
    if (n < 0) return s_errno_to_gs();
    *o_actual = n;
    return 0;
}

/* Finishes processing for page.

If <write_file> is true, we write to file; e.g. set if we are writing a file
per page, or if we are being called from docxwrite_close_device(). */
static int
s_end_page(gx_device_docxwrite_t *tdev, int write_file)
{
    int code = 0;
    extract_buffer_t *buffer = NULL;

    if (!tdev->extract) {
        /* Nothing to do. */
        return 0;
    }
    if (extract_page_end(tdev->extract)) {
        code = s_errno_to_gs();
        goto end;
    }
    if (extract_process(tdev->extract, 0 /*spacing*/, 1 /*rotation*/, 1 /*images*/)) {
        code = s_errno_to_gs();
        goto end;
    }
    if (write_file) {
        code = gx_device_open_output_file((gx_device*) tdev, tdev->fname, true, false, &tdev->file);
        if (code) goto end;
        /* extract_write() can only write to an extract_buffer_t, so we create one that
        writes to tdev->file. */
        if (extract_buffer_open(
                tdev->alloc,
                tdev,
                NULL /*fn_read*/,
                docxwrite_extract_buffer_write,
                NULL /*cache*/,
                NULL /*fn_close*/,
                &buffer
                )) {
            code = s_errno_to_gs();
            goto end;
        }
        if (extract_write(tdev->extract, buffer)) {
            code = s_errno_to_gs();
            goto end;
        }
    }

    end:
    extract_buffer_close(&buffer);
    if (tdev->file) {
        gx_device_close_output_file((gx_device*) tdev, tdev->fname, tdev->file);
        tdev->file = NULL;
    }
    return code;
}

static int
docxwrite_close_device(gx_device * dev)
{
    int code = 0;
    gx_device_docxwrite_t *const tdev = (gx_device_docxwrite_t *) dev;
    if (!tdev->file_per_page)
    {
        s_end_page(tdev, true /*write_file*/);
    }
    extract_end(&tdev->extract);
    extract_alloc_destroy(&tdev->alloc);
    return code;
}

static int
docxwrite_output_page(gx_device * dev, int num_copies, int flush)
{
    int code;
    gx_device_docxwrite_t *tdev = (gx_device_docxwrite_t *) dev;

    s_end_page(tdev, tdev->file_per_page /*write_file*/);

    if (tdev->file_per_page) {
        /* Create a new extract_t for the next page. */
        extract_end(&tdev->extract);
        if (extract_begin(tdev->alloc, extract_format_DOCX, &tdev->extract)) {
            code = s_errno_to_gs();
            goto end;
        }
    }
    if (extract_page_begin(tdev->extract)) {
        code = s_errno_to_gs();
        goto end;
    }

    code =  gx_default_output_page(dev, num_copies, flush);
    if (code < 0)
        goto end;

    end:
    return code;
}

/* ---------------- Low-level drawing ---------------- */

static int
docxwrite_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                    gx_color_index color)
{
    return 0;
}

/*static int
docxwrite_copy_alpha(gx_device * dev, const byte * data, int data_x,
                int raster, gx_bitmap_id id, int x, int y, int w, int h,
                gx_color_index color, int depth)
{
    return 0;
}

static int
docxwrite_copy_mono(gx_device * dev, const byte * data, int dx, int raster,
               gx_bitmap_id id, int x, int y, int w, int h,
               gx_color_index zero, gx_color_index one)
{
    return 0;
}
static int
docxwrite_copy_color(gx_device * dev, const byte * data,
                int data_x, int raster, gx_bitmap_id id,
                int x, int y, int width, int height)
{
    return 0;
}

static int
docxwrite_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
                          int px, int py)
{
    return 0;
}

static int
docxwrite_strip_copy_rop2(gx_device * dev,
                    const byte * sdata, int sourcex, uint sraster,
                    gx_bitmap_id id,
                    const gx_color_index * scolors,
                    const gx_strip_bitmap * textures,
                    const gx_color_index * tcolors,
                    int x, int y, int w, int h,
                    int phase_x, int phase_y, gs_logical_operation_t lop,
                    uint plane_height)
{
    return 0;
}*/

/* ---------------- Parameters ---------------- */

static int docxwrite_get_param(gx_device *dev, char *Param, void *list)
{
    gx_device_docxwrite_t *const tdev = (gx_device_docxwrite_t *) dev;
    gs_param_list * plist = (gs_param_list *)list;
    bool bool_T = true;

    if (strcmp(Param, "OutputFile") == 0) {
        gs_param_string ofns;

        ofns.data = (const byte *)tdev->fname,
        ofns.size = strlen(tdev->fname),
        ofns.persistent = false;
        return param_write_string(plist, "OutputFile", &ofns);
    }
    if (strcmp(Param, "WantsToUnicode") == 0) {
        return param_write_bool(plist, "WantsToUnicode", &bool_T);
    }
    if (strcmp(Param, "PreserveTrMode") == 0) {
        return param_write_bool(plist, "PreserveTrMode", &bool_T);
    }
    if (strcmp(Param, "HighLevelDevice") == 0) {
        return param_write_bool(plist, "HighLevelDevice", &bool_T);
    }
    return_error(gs_error_undefined);
}

static int
docxwrite_get_params(gx_device * dev, gs_param_list * plist)
{
    int code;
    bool bool_T = true;
    gs_param_string ofns;
    gx_device_docxwrite_t *const tdev = (gx_device_docxwrite_t *) dev;

    code = gx_default_get_params(dev, plist);
    if (code < 0)
        return code;

    ofns.data = (const byte *)tdev->fname,
    ofns.size = strlen(tdev->fname),
    ofns.persistent = false;
    code = param_write_string(plist, "OutputFile", &ofns);
    if (code < 0)
        return code;

    code = param_write_bool(plist, "WantsToUnicode", &bool_T);
    if (code < 0)
        return code;

    code = param_write_bool(plist, "PreserveTrMode", &bool_T);
    if (code < 0)
        return code;

    code = param_write_bool(plist, "HighLevelDevice", &bool_T);
    if (code < 0)
        return code;

   code = gs_param_write_items(plist, tdev, NULL, docx_param_items);
   return code;
}

/* We implement put_params to ensure that we keep the important */
/* device parameters up to date, and to prevent an /undefined error */
static int
docxwrite_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_docxwrite_t *tdev = (gx_device_docxwrite_t *) dev;
    int ecode = 0;
    int code, old_TextFormat = tdev->TextFormat;
    const char *param_name;
    gs_param_string ofs;
    bool dummy, open = dev->is_open;

    switch (code = param_read_string(plist, (param_name = "OutputFile"), &ofs)) {
        case 0:
            if (dev->LockSafetyParams &&
                    bytes_compare(ofs.data, ofs.size,
                        (const byte *)tdev->fname, strlen(tdev->fname))) {
                ecode = gs_note_error(gs_error_invalidaccess);
                goto ofe;
            }
            if (ofs.size >= gp_file_name_sizeof)
                ecode = gs_error_limitcheck;
            else
                break;
            goto ofe;
        default:
            ecode = code;
          ofe:param_signal_error(plist, param_name, ecode);
        /* fall through */
        case 1:
            ofs.data = 0;
            break;
    }

    if (ecode < 0)
        return ecode;

    code = param_read_int(plist, "TextFormat", &tdev->TextFormat);
    if (code < 0)
        return code;

    code = param_read_bool(plist, "WantsToUnicode", &dummy);
    if (code < 0)
        return code;

    code = param_read_bool(plist, "HighLevelDevice", &dummy);
    if (code < 0)
        return code;

    code = param_read_bool(plist, "PreserveTrMode", &dummy);
    if (code < 0)
        return code;

    if (ofs.data != 0) {	/* Close the file if it's open. */
        if (0 && tdev->file != 0) {
            gp_fclose(tdev->file);
            tdev->file = 0;
            outf("have closed tdef->file. tdev=%p", tdev);
        }
        memcpy(tdev->fname, ofs.data, ofs.size);
        tdev->fname[ofs.size] = 0;
    }

    /* If we change media size then gs_default_put_params will close
     * the device if it is open. We don't want it to do that, so set
     * the device's 'is_open' flag to false, and reset it after we've
     * processed the params.
     */
    if (old_TextFormat == tdev->TextFormat && open)
        dev->is_open = false;

    code = gx_default_put_params(dev, plist);
    if (code < 0)
        return code;

    dev->is_open = open;

    dev->interpolate_control = 0;

    return 0;
}

/* ---------------- Polygon drawing ---------------- */

/*static int
docxwrite_fill_trapezoid(gx_device * dev,
                    const gs_fixed_edge * left, const gs_fixed_edge * right,
                    fixed ybot, fixed ytop, bool swap_axes,
                    const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    return 0;
}

static int
docxwrite_fill_parallelogram(gx_device * dev,
                        fixed px, fixed py, fixed ax, fixed ay,
                        fixed bx, fixed by, const gx_device_color * pdevc,
                        gs_logical_operation_t lop)
{
    return 0;
}

static int
docxwrite_fill_triangle(gx_device * dev,
                   fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
                   const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    return 0;
}

static int
docxwrite_draw_thin_line(gx_device * dev,
                    fixed fx0, fixed fy0, fixed fx1, fixed fy1,
                    const gx_device_color * pdevc, gs_logical_operation_t lop,
                    fixed adjustx, fixed adjusty)
{
    return 0;
}*/

/* ---------------- High-level drawing ---------------- */

static int
docxwrite_fill_path(gx_device * dev, const gs_gstate * pgs, gx_path * ppath,
               const gx_fill_params * params, const gx_device_color * pdevc,
               const gx_clip_path * pcpath)
{
        return 0;
}

static int
docxwrite_stroke_path(gx_device * dev, const gs_gstate * pgs, gx_path * ppath,
                 const gx_stroke_params * params,
                 const gx_drawing_color * pdevc, const gx_clip_path * pcpath)
{
    return 0;
}


/*
 * Compute the cached values in the text processing state from the text
 * parameters, current_font, and pgs->ctm.  Return either an error code (<
 * 0) or a mask of operation attributes that the caller must emulate.
 * Currently the only such attributes are TEXT_ADD_TO_ALL_WIDTHS and
 * TEXT_ADD_TO_SPACE_WIDTH.  Note that this procedure fills in all the
 * values in ppts->values, not just the ones that need to be set now.
 */
static int
transform_delta_inverse(const gs_point *pdelta, const gs_matrix *pmat,
                        gs_point *ppt)
{
    int code = gs_distance_transform_inverse(pdelta->x, pdelta->y, pmat, ppt);
    gs_point delta;

    if (code < 0)
        return code;
    if (ppt->y == 0)
        return 0;
    /* Check for numerical fuzz. */
    code = gs_distance_transform(ppt->x, 0.0, pmat, &delta);
    if (code < 0)
        return 0;                /* punt */
    if (fabs(delta.x - pdelta->x) < 0.01 && fabs(delta.y - pdelta->y) < 0.01) {
        /* Close enough to y == 0: device space error < 0.01 pixel. */
        ppt->y = 0;
    }
    return 0;
}

static int
docx_update_text_state(docx_list_entry_t *ppts,
                      const docxw_text_enum_t *penum,
                      gs_font *ofont, const gs_matrix *pfmat)
{
    gx_device *const pdev = penum->dev;
    gs_font *font = penum->current_font;
    gs_fixed_point cpt;
    gs_matrix smat, tmat;
    float size;
    int mask = 0;
    int code = gx_path_current_point(gs_text_enum_path(penum), &cpt);

    if (code < 0)
        return code;

    size = txt_calculate_text_size(penum->pgs, ofont, pfmat, &smat, &tmat, penum->current_font, pdev);
    /* Check for spacing parameters we can handle, and transform them. */

    if (penum->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
        if (penum->current_font->WMode == 0) {
            gs_point pt;

            code = transform_delta_inverse(&penum->text.delta_all, &smat, &pt);
            if (code < 0 || pt.y != 0)
                mask |= TEXT_ADD_TO_ALL_WIDTHS;
        }
        else
            mask |= TEXT_ADD_TO_ALL_WIDTHS;
    }

    if (penum->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
        gs_point pt;

        code = transform_delta_inverse(&penum->text.delta_space, &smat, &pt);
        if (code < 0 || pt.y != 0 || penum->text.space.s_char != 32)
            mask |= TEXT_ADD_TO_SPACE_WIDTH;
    }
    /* Store the updated values. */

    tmat.xx /= size;
    tmat.xy /= size;
    tmat.yx /= size;
    tmat.yy /= size;
    tmat.tx += fixed2float(cpt.x);
    tmat.ty += fixed2float(cpt.y);

    ppts->size = size;
    ppts->matrix = tmat;
    ppts->FontName = (char *)gs_malloc(pdev->memory->stable_memory, 1,
        font->font_name.size + 1, "txtwrite alloc font name");
    if (!ppts->FontName)
        return gs_note_error(gs_error_VMerror);
    memcpy(ppts->FontName, font->font_name.chars, font->font_name.size + 1);

    if (font->PaintType == 2 && penum->pgs->text_rendering_mode == 0)
    {
        gs_gstate *pgs = penum->pgs;
        gs_font *font = penum->current_font;
        double scaled_width = font->StrokeWidth != 0 ? font->StrokeWidth : 0.001;
        double saved_width = pgs->line_params.half_width;
        /*
         * See stream_to_text in gdevpdfu.c re the computation of
         * the scaling value.
         */
        double scale = 72.0 / pdev->HWResolution[1];

        if (font->FontMatrix.yy != 0)
            scaled_width *= fabs(font->orig_FontMatrix.yy) * size * tmat.yy * scale;
        else
            scaled_width *= fabs(font->orig_FontMatrix.xy) * size * tmat.xy * scale;

        pgs->line_params.half_width = scaled_width / 2;
        if (code < 0)
            return code;

        pgs->line_params.half_width = saved_width;
    }
    return (code < 0 ? code : mask);
}

/* Simple routine to update the current point by the accumulated width of the
 * text.
 */
static int
docx_shift_text_currentpoint(docxw_text_enum_t *penum, gs_point *wpt)
{
    return gs_moveto_aux(penum->pgs, gx_current_path(penum->pgs),
                              fixed2float(penum->origin.x) + wpt->x,
                              fixed2float(penum->origin.y) + wpt->y);
}

static int font_final(gx_device_docxwrite_t *tdev, docxw_text_enum_t *penum)
{
    gs_text_enum_t* pte = (void*) penum;
    gs_font_base *font_base = (gs_font_base *)pte->current_font;
    int code;

    penum->d1_width = 0;
    penum->d1_width_set = false;
    if (font_base->FontBBox.p.x != font_base->FontBBox.q.x ||
        font_base->FontBBox.p.y != font_base->FontBBox.q.y) {
        gs_point p0, p1, p2, p3;
        gs_matrix m;

        m = ctm_only(pte->pgs);
        m.tx = m.ty = fixed2float(0);
        gs_matrix_multiply(&font_base->FontMatrix, &m, &m);
        gs_point_transform(font_base->FontBBox.p.x, font_base->FontBBox.p.y, &m, &p0);
        gs_point_transform(font_base->FontBBox.p.x, font_base->FontBBox.q.y, &m, &p1);
        gs_point_transform(font_base->FontBBox.q.x, font_base->FontBBox.p.y, &m, &p2);
        gs_point_transform(font_base->FontBBox.q.x, font_base->FontBBox.q.y, &m, &p3);
    }
    code = docx_shift_text_currentpoint(penum, &penum->returned.total_width);
    return code;
}


/* Routines to enumerate each glyph/character code in turn and sent to
 * extract_add_char().
 */

static int
docxwrite_process_cmap_text(gx_device_docxwrite_t *tdev, gs_text_enum_t *pte)
{
    docxw_text_enum_t *const penum = (docxw_text_enum_t *)pte;
    unsigned int rcode = 0;
    gs_text_enum_t scan = *(gs_text_enum_t *)pte;
    int i;

    /* Composite font using a CMap */

    for (i=0; ; ++i) {
        const char* prevFontName;

        gs_glyph glyph;
        int font_code, code;
        gs_font *subfont;
        gs_char chr;
        txt_glyph_widths_t widths;
        gs_matrix m3;
        gs_point wanted;	/* user space */
        gs_point dpt = {0,0};

        font_code = scan.orig_font->procs.next_char_glyph
                (&scan, &chr, &glyph);

        subfont = scan.fstack.items[scan.fstack.depth].font;

        if (font_code == 2) {
            /* end of string */
            return 0;
        }
        if (font_code != 0 && font_code != 1) {
            /* error */
            return font_code;
        }

        if (font_code == 1) {
            /* font change */
            if (i) abort();
        }

        code = txt_glyph_widths(subfont, scan.orig_font->WMode, glyph, (gs_font *)subfont, &widths,
                penum->cdevproc_callout ? penum->cdevproc_result : NULL);
        if (code == TEXT_PROCESS_CDEVPROC) {
            penum->cdevproc_callout = true;
            pte->returned.current_glyph = glyph;
            scan.returned.current_glyph = glyph;
            pte->current_font = subfont;
            scan.current_font = subfont;
            rcode = TEXT_PROCESS_CDEVPROC;
            break;
        }
        else {
            penum->cdevproc_callout = false;
            pte->index = scan.index;
        }
        code = gs_matrix_multiply(&subfont->FontMatrix, &pte->orig_font->FontMatrix, &m3);
        if (code < 0) {
            outf("returning code=%i", code);
            return code;
        }
        prevFontName = penum->text_state->FontName;
        code = docx_update_text_state(penum->text_state, (docxw_text_enum_t *)pte, subfont, &m3);
        if (code < 0) {
            outf("returning code=%i", code);
            return code;
        }
        if (!prevFontName && penum->text_state->FontName) {

            tdev->x = fixed2float(penum->origin.x) - penum->text_state->matrix.tx;

            if (extract_span_begin(
                    tdev->extract,
                    penum->text_state->FontName,
                    0 /*font_bold*/,
                    0 /*font_italic*/,
                    penum->text_state->wmode,
                    penum->text_state->matrix.xx,
                    penum->text_state->matrix.xy,
                    penum->text_state->matrix.yx,
                    penum->text_state->matrix.yy,
                    penum->text_state->matrix.tx,
                    penum->text_state->matrix.ty,
                    penum->text_state->size,
                    0.0f,
                    0.0f,
                    penum->text_state->size,
                    0.0f,
                    0.0f
                    )) {
                return s_errno_to_gs();
            }
        }
        txt_char_widths_to_uts(pte->orig_font, &widths); /* convert design->text space */
        gs_distance_transform(widths.real_width.xy.x * penum->text_state->size,
                  widths.real_width.xy.y * penum->text_state->size,
                  &penum->text_state->matrix, &wanted);
        pte->returned.total_width.x += wanted.x;
        pte->returned.total_width.y += wanted.y;

        {
            unsigned short buffer[4];
            float glyph_width = widths.real_width.xy.x * penum->text_state->size;

            if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
                gs_point tpt;

                gs_distance_transform(pte->text.delta_all.x, pte->text.delta_all.y,
                          &ctm_only(pte->pgs), &tpt);
                dpt.x += tpt.x;
                dpt.y += tpt.y;
            }
            if (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH && chr == pte->text.space.s_char) {
                gs_point tpt;

                gs_distance_transform(pte->text.delta_space.x, pte->text.delta_space.y,
                          &ctm_only(pte->pgs), &tpt);
                dpt.x += tpt.x;
                dpt.y += tpt.y;
            }
            pte->returned.total_width.x += dpt.x;
            pte->returned.total_width.y += dpt.y;

            glyph_width += dpt.x;

            /* We can get up to 4 Unicode points per glyph, and a glyph can be
             * be represented by as little as one byte. So we make a very large
             * temporary buffer to hold the Unicode string as we assemble it. When
             * we copy it to the text fragment we will allocate only as many bytes
             * as are required to hold the actual nukmber of Unicode values we
             * decoded, and this temporary buffer will be discarded.
             */

            txt_get_unicode(penum->dev, (gs_font *)pte->orig_font, glyph, chr, &buffer[0]);

            if (extract_add_char(
                    tdev->extract,
                    tdev->x,
                    fixed2float(penum->origin.y) - penum->text_state->matrix.ty,
                    buffer[0] /*ucs*/,
                    glyph_width / penum->text_state->size /*adv*/,
                    0 /*autosplit*/
                    )) {
                return s_errno_to_gs();
            }
        }

        tdev->x += widths.real_width.xy.x * penum->text_state->size;

        if (rcode || pte->index >= pte->text.size)
            break;
    }
    if (!rcode) rcode = font_final(tdev, penum);
    return rcode;
}

static int
docxwrite_process_plain_text(gx_device_docxwrite_t *tdev, gs_text_enum_t *pte)
{
    /* one byte regular font */
    docxw_text_enum_t *const penum = (docxw_text_enum_t *)pte;
    gs_font *font = pte->orig_font;
    const gs_glyph *gdata = NULL;
    gs_glyph glyph;
    gs_char ch = 0;
    int i, code;
    uint operation = pte->text.operation;
    txt_glyph_widths_t widths;
    gs_point wanted;	/* user space */

    for (i=pte->index;i<pte->text.size;i++) {
        const char* prevFontName;
        float span_delta_x;
        float glyph_width;
        unsigned short chr2[4];

        gs_point dpt = {0,0};
        if (operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES)) {
            ch = pte->text.data.bytes[pte->index];
        } else if (operation & (TEXT_FROM_CHARS | TEXT_FROM_SINGLE_CHAR)) {
            ch = pte->text.data.chars[pte->index];
        } else if (operation & (TEXT_FROM_GLYPHS | TEXT_FROM_SINGLE_GLYPH)) {
            if (operation & TEXT_FROM_GLYPHS) {
                gdata = pte->text.data.glyphs + (pte->index++ * sizeof (gs_glyph));
            } else {
                gdata = &pte->text.data.d_glyph;
            }
        }
        glyph = (gdata == NULL ? pte->orig_font->procs.encode_char(pte->orig_font, ch, GLYPH_SPACE_NAME)
                           : *gdata);

        code = txt_glyph_widths(font, font->WMode, glyph, (gs_font *)font, &widths, NULL);
        if (code < 0) {
            if (penum->d1_width_set) {
                widths.Width.w = widths.Width.xy.x = widths.real_width.w = widths.real_width.xy.x = penum->d1_width;
                penum->d1_width = 0;
                penum->d1_width_set = 0;
            }
            else
                return code;
        }

        prevFontName = penum->text_state->FontName;
        penum->cdevproc_callout = false;
        code = docx_update_text_state(penum->text_state, (docxw_text_enum_t *)pte, pte->orig_font, &font->FontMatrix);
        if (code < 0)
            return code;

        if (!prevFontName && penum->text_state->FontName) {

            tdev->x = fixed2float(penum->origin.x) - penum->text_state->matrix.tx;

            if (extract_span_begin(
                    tdev->extract,
                    penum->text_state->FontName,
                    0 /*font_bold*/,
                    0 /*font_italic*/,
                    penum->text_state->wmode,
                    penum->text_state->matrix.xx,
                    penum->text_state->matrix.xy,
                    penum->text_state->matrix.yx,
                    penum->text_state->matrix.yy,
                    penum->text_state->matrix.tx,
                    penum->text_state->matrix.ty,
                    penum->text_state->size,
                    0.0f,
                    0.0f,
                    penum->text_state->size,
                    0.0f,
                    0.0f
                    )) {
                return s_errno_to_gs();
            }
        }
        txt_char_widths_to_uts(pte->orig_font, &widths); /* convert design->text space */
        gs_distance_transform(widths.real_width.xy.x * penum->text_state->size,
                          widths.real_width.xy.y * penum->text_state->size,
                          &penum->text_state->matrix, &wanted);
        pte->returned.total_width.x += wanted.x;
        pte->returned.total_width.y += wanted.y;
        span_delta_x = widths.real_width.xy.x * penum->text_state->size;
        glyph_width = widths.real_width.xy.x * penum->text_state->size;

        if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
            gs_point tpt;

            gs_distance_transform(pte->text.delta_all.x, pte->text.delta_all.y,
                              &ctm_only(pte->pgs), &tpt);
            dpt.x += tpt.x;
            dpt.y += tpt.y;
        }
        if (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH && ch == pte->text.space.s_char) {
            gs_point tpt;

            gs_distance_transform(pte->text.delta_space.x, pte->text.delta_space.y,
                              &ctm_only(pte->pgs), &tpt);
            dpt.x += tpt.x;
            dpt.y += tpt.y;
        }
        pte->returned.total_width.x += dpt.x;
        pte->returned.total_width.y += dpt.y;

        span_delta_x += dpt.x;

        code = txt_get_unicode(penum->dev, (gs_font *)pte->orig_font, glyph, ch, &chr2[0]);
        /* If a single text code returned multiple Unicode values, then we need to set the
         * 'extra' code points' widths to 0.
         */

        if (extract_add_char(
                tdev->extract,
                tdev->x,
                fixed2float(penum->origin.y) - penum->text_state->matrix.ty,
                chr2[0] /*ucs*/,
                glyph_width / penum->text_state->size /*adv*/,
                0 /*autosplit*/
                )) {
            return s_errno_to_gs();
        }
        tdev->x += span_delta_x;
        pte->index++;
    }
    code = 0;
    if (!code) code = font_final(tdev, penum);

    return code;
}

/* This routine selects whether the text needs to be handled as regular glyphs
 * and character codes, or as CIDs, depending on the font type. This is required
 * because there are ways that regular text can be handled that aren't possible
 * with CIDFonts.
 */
static int
textw_text_process(gs_text_enum_t *pte)
{
    docxw_text_enum_t *const penum = (docxw_text_enum_t *)pte;
    gx_device_docxwrite_t *const tdev = (gx_device_docxwrite_t *) pte->dev;
    gs_font *font = pte->orig_font;
    int code = 0;
    gs_text_enum_t *pte_fallback;

    if (pte->text.size == 0)
        return 0;

    pte_fallback = penum->pte_fallback;
    if (pte_fallback) {
        code = gx_default_text_restore_state(pte_fallback);
        if (code < 0)
            return code;
        gs_text_release(NULL, pte_fallback, "docxwrite_text_process");
    }
    pte_fallback = penum->pte_fallback = NULL;

    {
        switch (font->FontType) {
        case ft_CID_encrypted:
        case ft_CID_TrueType:
        case ft_composite:
            code = docxwrite_process_cmap_text(tdev, pte);
            break;
        case ft_encrypted:
        case ft_encrypted2:
        case ft_TrueType:
        case ft_user_defined:
        case ft_PCL_user_defined:
        case ft_GL2_stick_user_defined:
        case ft_GL2_531:
            code = docxwrite_process_plain_text(tdev, pte);
            break;
        default:
            return_error(gs_error_rangecheck);
            break;
        }
        if (code != 0) {
            if (code == gs_error_unregistered) /* Debug purpose only. */
                return code;
            if (code == gs_error_VMerror)
                return code;
            if (code == gs_error_invalidfont) /* Bug 688370. */
                return code;
            /* Fall back to the default implementation. */
            code = gx_default_text_begin(pte->dev, pte->pgs, &pte->text, pte->current_font,
                                         pte->pcpath, &pte_fallback);
            if (code < 0)
                return code;
            penum->pte_fallback = pte_fallback;
            gs_text_enum_copy_dynamic(pte_fallback, pte, false);

            code = gs_text_process(pte_fallback);
            if (code != 0) {
                penum->returned.current_char = pte_fallback->returned.current_char;
                penum->returned.current_glyph = pte_fallback->returned.current_glyph;
                return code;
            }
            gs_text_release(NULL, pte_fallback, "docxwrite_text_process");
            penum->pte_fallback = 0;
        }
    }
    return code;
}

/* Begin processing text. */

/* Define the auxiliary procedures for text processing. */
static int
textw_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    return gs_text_resync(pte, pfrom);
}
static bool
textw_text_is_width_only(const gs_text_enum_t *pte)
{
    return false;
}
static int
textw_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    return gs_text_current_width(pte, pwidth);
}
static int
textw_text_set_cache(gs_text_enum_t *pte, const double *pw,
                   gs_text_cache_control_t control)
{
    docxw_text_enum_t *const penum = (docxw_text_enum_t *)pte;

    switch (control) {
        case TEXT_SET_CHAR_WIDTH:
        case TEXT_SET_CACHE_DEVICE:
            if (penum->pte_fallback != NULL) {
                penum->d1_width = *pw;
                penum->d1_width_set = true;
                return 0;
            }
            return gs_text_set_cache(pte, pw, control);
        case TEXT_SET_CACHE_DEVICE2:
            if (penum->cdevproc_callout) {
                memcpy(penum->cdevproc_result, pw, sizeof(penum->cdevproc_result));
                return 0;
            }
            return gs_text_set_cache(pte, pw, control);
        default:
            return_error(gs_error_rangecheck);
    }
    return 0;
}

static int
textw_text_retry(gs_text_enum_t *pte)
{
    return gs_text_retry(pte);
}
static void
textw_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    docxw_text_enum_t *const penum = (docxw_text_enum_t *)pte;
    gx_device_docxwrite_t *const tdev = (gx_device_docxwrite_t *) pte->dev;

    /* If this is copied away when we complete the text enumeration succesfully, then
     * we set the pointer to NULL, if we get here with it non-NULL , then there was
     * an error.
     */
    if (penum->text_state)
        gs_free(tdev->memory, penum->text_state, 1, sizeof(penum->text_state), "txtwrite free text state");

    gs_text_release(NULL, pte, cname);
}

/* This is the list of methods for the text enumerator */
static const gs_text_enum_procs_t textw_text_procs = {
    textw_text_resync, textw_text_process,
    textw_text_is_width_only, textw_text_current_width,
    textw_text_set_cache, textw_text_retry,
    textw_text_release
};

/* This device method gets called by the interpreter to deal with text. It
 * must create a text enumerator, which contains the methods for dealing with
 * the text itself. The interpreter will use the enumerator methods to deal with
 * the text, it won't refer to the device methods again for this text.
 */
static int
docxwrite_text_begin(gx_device * dev, gs_gstate * pgs,
                const gs_text_params_t * text, gs_font * font,
                const gx_clip_path * pcpath,
                gs_text_enum_t ** ppenum)
{
    gx_device_docxwrite_t *const tdev = (gx_device_docxwrite_t *) dev;
    docxw_text_enum_t *penum;
    int code;
    gx_path *path = pgs->path;
    gs_memory_t * mem = pgs->memory;

    /* If this is a stringwidth, we must let the default graphics library code handle it
     * in case there is no current point (this can happen if this is the first operation
     * we get, the current font is a CIDFont, and its descendant is a substitute type 1
     * font). If there is no current point we throw an error in text_process and that
     * messes up all the font handling. The test below is copied from pdfwrite
     * (gdev_pdf_text_begin) and seems to do the job.
     */
    if ((!(text->operation & TEXT_DO_DRAW) && pgs->text_rendering_mode != 3)
                    || path == 0 || !path_position_valid(path))
            return gx_default_text_begin(dev, pgs, text, font,
                                         pcpath, ppenum);
    /* Allocate and initialize one of our text enumerators. */
    rc_alloc_struct_1(penum, docxw_text_enum_t, &st_textw_text_enum, mem,
                      return_error(gs_error_VMerror), "gdev_textw_text_begin");
    penum->rc.free = rc_free_text_enum;
    penum->charproc_accum = false;
    penum->cdevproc_callout = false;
    penum->returned.total_width.x = penum->returned.total_width.y = 0;
    penum->pte_fallback = NULL;
    penum->d1_width = 0;
    penum->d1_width_set = false;
    /* The enumerator's text_release method frees this memory */
    penum->text_state = (docx_list_entry_t *)gs_malloc(tdev->memory->stable_memory, 1,
            sizeof(docx_list_entry_t), "txtwrite alloc text state");
    if (!penum->text_state)
        return gs_note_error(gs_error_VMerror);
    memset(penum->text_state, 0x00, sizeof(docx_list_entry_t));

    code = gs_text_enum_init((gs_text_enum_t *)penum, &textw_text_procs,
                             dev, pgs, text, font, pcpath, mem);
    if (code < 0) {
        /* Belt and braces; I'm not certain this is required, but its safe */
        gs_free(tdev->memory, penum->text_state, 1, sizeof(docx_list_entry_t), "txtwrite free text state");
        penum->text_state = NULL;
        gs_free_object(mem, penum, "textwrite_text_begin");
        return code;
    }

    code = gx_path_current_point(gs_text_enum_path(penum), &penum->origin);
    if (code != 0)
       return code;

    *ppenum = (gs_text_enum_t *)penum;

    return 0;
}

static int
docxwrite_strip_copy_rop2(gx_device * dev,
                    const byte * sdata, int sourcex, uint sraster,
                    gx_bitmap_id id,
                    const gx_color_index * scolors,
                    const gx_strip_bitmap * textures,
                    const gx_color_index * tcolors,
                    int x, int y, int w, int h,
                    int phase_x, int phase_y, gs_logical_operation_t lop,
                    uint plane_height)
{
    return 0;
}

int
docxwrite_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    switch (dev_spec_op) {
        case gxdso_get_dev_param:
            {
                int code;
                dev_param_req_t *request = (dev_param_req_t *)data;
                code = docxwrite_get_param(pdev, request->Param, request->list);
                if (code != gs_error_undefined)
                    return code;
            }
    }
    return gx_default_dev_spec_op(pdev, dev_spec_op, data, size);
}
