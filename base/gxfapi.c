/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


#include "memory_.h"

#include "gsmemory.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxpath.h"
#include "gxfcache.h"
#include "gxchrout.h"
#include "gximask.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gzstate.h"
#include "gxfcid.h"
#include "gxchar.h"             /* for st_gs_show_enum and MAX_CCACHE_TEMP_BITMAP_BITS */
#include "gdebug.h"
#include "gsimage.h"
#include "gsbittab.h"
#include "gzpath.h"
#include "gxdevsop.h"

#include "gxfapi.h"

#define FAPI_ROUND(v) (v >= 0 ? v + 0.5 : v - 0.5)
#define FAPI_ROUND_TO_FRACINT(v) ((fracint)FAPI_ROUND(v))

extern_gs_get_fapi_server_inits();

/* FIXME */
static int
gs_fapi_renderer_retcode(gs_memory_t *mem, gs_fapi_server *I,
                         gs_fapi_retcode rc)
{
    if (rc == 0)
        return 0;
    if (gs_debug_c('1')) {
        emprintf2(mem,
                  "Error: Font Renderer Plugin ( %s ) return code = %d\n",
                  I->ig.d->subtype, rc);
    }
    return rc < 0 ? rc : gs_error_invalidfont;
}

typedef struct gs_fapi_outline_handler_s
{
    gs_fapi_server *fserver;
    struct gx_path_s *path;
    fixed x0;
    fixed y0;
    bool close_path;
    bool need_close;            /* This stuff fixes unclosed paths being rendered with UFST */
} gs_fapi_outline_handler;

static inline int64_t
import_shift(int64_t x, int64_t n)
{
    return (n > 0 ? (x << n) : (x >> -n));
}

static inline int
export_shift(int x, int n)
{
    return (n > 0 ? (x >> n) : (x << -n));
}

static inline int
fapi_round(double x)
{
    return (int)(x + 0.5);
}

static int
add_closepath(gs_fapi_path *I)
{
    gs_fapi_outline_handler *olh = (gs_fapi_outline_handler *) I->olh;

    if (olh->need_close == true) {
        olh->need_close = false;
        I->gs_error = gx_path_close_subpath_notes(olh->path, 0);
    }
    return (I->gs_error);
}

static int
add_move(gs_fapi_path *I, int64_t x, int64_t y)
{
    gs_fapi_outline_handler *olh = (gs_fapi_outline_handler *) I->olh;

    x = import_shift(x, I->shift);
    y = -import_shift(y, I->shift);
    if (olh->fserver->transform_outline) {
        gs_point pt;
        I->gs_error = gs_distance_transform((double)fixed2float((float)x), (double)fixed2float((float)y), &olh->fserver->outline_mat, &pt);
        if (I->gs_error < 0)
            return I->gs_error;
        x = float2fixed(pt.x);
        y = float2fixed(pt.y);
    }
    x += olh->x0;
    y += olh->y0;

    if (x > (int64_t) max_coord_fixed || x < (int64_t) min_coord_fixed
     || y > (int64_t) max_coord_fixed || y < (int64_t) min_coord_fixed) {
         I->gs_error = gs_error_undefinedresult;
    }
    else {

        if (olh->need_close && olh->close_path)
            if ((I->gs_error = add_closepath(I)) < 0)
                return (I->gs_error);
        olh->need_close = false;

/*        dprintf2("%f %f moveto\n", fixed2float(x), fixed2float(y)); */
        I->gs_error = gx_path_add_point(olh->path, (fixed) x, (fixed) y);
    }
    return (I->gs_error);
}

static int
add_line(gs_fapi_path *I, int64_t x, int64_t y)
{
    gs_fapi_outline_handler *olh = (gs_fapi_outline_handler *) I->olh;

    x = import_shift(x, I->shift);
    y = -import_shift(y, I->shift);
    if (olh->fserver->transform_outline) {
        gs_point pt;
        I->gs_error = gs_distance_transform((double)fixed2float((float)x), (double)fixed2float((float)y), &olh->fserver->outline_mat, &pt);
        if (I->gs_error < 0)
            return I->gs_error;
        x = float2fixed(pt.x);
        y = float2fixed(pt.y);
    }
    x += olh->x0;
    y += olh->y0;

    if (x > (int64_t) max_coord_fixed || x < (int64_t) min_coord_fixed
     || y > (int64_t) max_coord_fixed || y < (int64_t) min_coord_fixed) {
         I->gs_error = gs_error_undefinedresult;
    }
    else {
        olh->need_close = true;

/*        dprintf2("%f %f lineto\n", fixed2float(x), fixed2float(y)); */
        I->gs_error = gx_path_add_line_notes(olh->path, (fixed) x, (fixed) y, 0);
    }
    return (I->gs_error);
}

static int
add_curve(gs_fapi_path *I, int64_t x0, int64_t y0, int64_t x1, int64_t y1,
          int64_t x2, int64_t y2)
{
    gs_fapi_outline_handler *olh = (gs_fapi_outline_handler *) I->olh;

    x0 = import_shift(x0, I->shift);
    y0 = -import_shift(y0, I->shift);
    x1 = import_shift(x1, I->shift);
    y1 = -import_shift(y1, I->shift);
    x2 = import_shift(x2, I->shift);
    y2 = -import_shift(y2, I->shift);

    if (olh->fserver->transform_outline) {
        gs_point pt;
        I->gs_error = gs_distance_transform((double)fixed2float((float)x0), (double)fixed2float((float)y0), &olh->fserver->outline_mat, &pt);
        if (I->gs_error < 0)
            return I->gs_error;
        x0 = float2fixed(pt.x);
        y0 = float2fixed(pt.y);
        I->gs_error = gs_distance_transform((double)fixed2float((float)x1), (double)fixed2float((float)y1), &olh->fserver->outline_mat, &pt);
        if (I->gs_error < 0)
            return I->gs_error;
        x1 = float2fixed(pt.x);
        y1 = float2fixed(pt.y);
        I->gs_error = gs_distance_transform((double)fixed2float((float)x2), (double)fixed2float((float)y2), &olh->fserver->outline_mat, &pt);
        if (I->gs_error < 0)
            return I->gs_error;
        x2 = float2fixed(pt.x);
        y2 = float2fixed(pt.y);
    }
    x0 += olh->x0;
    y0 += olh->y0;
    x1 += olh->x0;
    y1 += olh->y0;
    x2 += olh->x0;
    y2 += olh->y0;

    if (x0 > (int64_t) max_coord_fixed || x0 < (int64_t) min_coord_fixed
     || y0 > (int64_t) max_coord_fixed || y0 < (int64_t) min_coord_fixed
     || x1 > (int64_t) max_coord_fixed || x1 < (int64_t) min_coord_fixed
     || y1 > (int64_t) max_coord_fixed || y1 < (int64_t) min_coord_fixed
     || x2 > (int64_t) max_coord_fixed || x2 < (int64_t) min_coord_fixed
     || y2 > (int64_t) max_coord_fixed || y2 < (int64_t) min_coord_fixed)
    {
        I->gs_error = gs_error_undefinedresult;
    }
    else {
        olh->need_close = true;

/*        dprintf6("%f %f %f %f %f %f curveto\n", fixed2float(x0), fixed2float(y0), fixed2float(x1), fixed2float(y1), fixed2float(x2), fixed2float(y2));*/
        I->gs_error = gx_path_add_curve_notes(olh->path, (fixed) x0, (fixed) y0, (fixed) x1, (fixed) y1, (fixed) x2, (fixed) y2, 0);
    }
    return (I->gs_error);
}

static gs_fapi_path path_interface_stub =
    { NULL, 0, 0, add_move, add_line, add_curve, add_closepath };

int
gs_fapi_get_metrics_count(gs_fapi_font *ff)
{
    if (!ff->is_type1 && ff->is_cid) {
        gs_font_cid2 *pfcid = (gs_font_cid2 *) ff->client_font_data;

        return (pfcid->cidata.MetricsCount);
    }
    return 0;
}

/*
 * Lifted from psi/zchar.c
 * Return true if we only need the width from the rasterizer
 * and can short-circuit the full rendering of the character,
 * false if we need the actual character bits.  This is only safe if
 * we know the character is well-behaved, i.e., is not defined by an
 * arbitrary PostScript procedure.
 */
static inline bool
fapi_gs_char_show_width_only(const gs_text_enum_t *penum)
{
    if (!gs_text_is_width_only(penum))
        return false;
    switch (penum->orig_font->FontType) {
        case ft_encrypted:
        case ft_encrypted2:
        case ft_CID_encrypted:
        case ft_CID_TrueType:
        case ft_CID_bitmap:
        case ft_TrueType:
            return true;
        default:
            return false;
    }
}



/* If we're rendering an uncached glyph, we need to know
 * whether we're filling it with a pattern, and whether
 * transparency is involved - if so, we have to produce
 * a path outline, and not a bitmap.
 */
static inline bool
using_transparency_pattern(gs_gstate *pgs)
{
    gx_device *dev = gs_currentdevice_inline(pgs);

    return ((!gs_color_writes_pure(pgs)) && dev_proc(dev, dev_spec_op)(dev, gxdso_supports_pattern_transparency, NULL, 0));
}

static inline bool
recreate_multiple_master(gs_font_base *pbfont)
{
    bool r = false;
    gs_fapi_server *I = pbfont->FAPI;
    bool changed = false;

    if (I && I->face.font_id != gs_no_id &&
        (pbfont->FontType == ft_encrypted
        || pbfont->FontType == ft_encrypted2)) {
        gs_font_type1 *pfont1 = (gs_font_type1 *) pbfont;
        if (pfont1->data.WeightVector.count != 0
            && I->face.WeightVector.count != pfont1->data.WeightVector.count) {
            changed = true;
        }
        else if (pfont1->data.WeightVector.count != 0) {
            changed = (memcmp(I->face.WeightVector.values, pfont1->data.WeightVector.values,
                             pfont1->data.WeightVector.count * sizeof(pfont1->data.WeightVector.values[0])) != 0);
        }

        if (changed) {
            r = (I->set_mm_weight_vector(I, &I->ff, pfont1->data.WeightVector.values, pfont1->data.WeightVector.count) == gs_error_invalidaccess);
            I->face.WeightVector.count = pfont1->data.WeightVector.count;
            memcpy(I->face.WeightVector.values, pfont1->data.WeightVector.values,
                   pfont1->data.WeightVector.count * sizeof(pfont1->data.WeightVector.values[0]));
        }
    }
    return r;
}

static bool
produce_outline_char(gs_show_enum *penum_s,
                     gs_font_base *pbfont, int abits,
                     gs_log2_scale_point *log2_scale)
{
    gs_gstate *pgs = (gs_gstate *) penum_s->pgs;

    log2_scale->x = 0;
    log2_scale->y = 0;

    /* Checking both gx_compute_text_oversampling() result, and abits (below) may seem redundant,
     * and hopefully it will be soon, but for now, gx_compute_text_oversampling() could opt to
     * "oversample" sufficiently small glyphs (fwiw, I don't think gx_compute_text_oversampling is
     * working as intended in that respect), regardless of the device's anti-alias setting.
     * This was an old, partial solution for dropouts in small glyphs.
     */
    gx_compute_text_oversampling(penum_s, (gs_font *) pbfont, abits,
                                 log2_scale);

    return (pgs->in_charpath || pbfont->PaintType != 0 ||
            (pgs->in_cachedevice != CACHE_DEVICE_CACHING
             && using_transparency_pattern((gs_gstate *) penum_s->pgs))
            || (pgs->in_cachedevice != CACHE_DEVICE_CACHING
                && (log2_scale->x > 0 || log2_scale->y > 0))
            || (pgs->in_cachedevice != CACHE_DEVICE_CACHING && abits > 1));
}

static inline void
gs_fapi_release_typeface(gs_fapi_server *I, void **server_font_data)
{
    I->release_typeface(I, *server_font_data);
    I->face.font_id = gs_no_id;
    if (I->ff.server_font_data == *server_font_data)
        I->ff.server_font_data = 0;
    *server_font_data = 0;
}

static int
notify_remove_font(void *proc_data, void *event_data)
{                               /* gs_font_finalize passes event_data == NULL, so check it here. */
    if (event_data == NULL) {
        gs_font_base *pbfont = proc_data;
        gs_fapi_server *I = pbfont->FAPI;

        if (pbfont->FAPI_font_data != 0) {
            gs_fapi_release_typeface(I, &pbfont->FAPI_font_data);
        }
    }
    return 0;
}

int
gs_fapi_prepare_font(gs_font *pfont, gs_fapi_server *I, int subfont, const char *font_file_path,
                     gs_string *full_font_buf, const char *xlatmap, const char **decodingID)
{                               /* Returns 1 iff BBox is set. */
    /* Cleans up server's font data if failed. */

    /* A renderer may need to access the top level font's data of
     * a CIDFontType 0 (FontType 9) font while preparing its subfonts,
     * and/or perform a completion action with the top level font after
     * its descendants are prepared. Therefore with such fonts
     * we first call get_scaled_font(..., FAPI_TOPLEVEL_BEGIN), then
     * get_scaled_font(..., i) with eash descendant font index i,
     * and then get_scaled_font(..., FAPI_TOPLEVEL_COMPLETE).
     * For other fonts we don't call with 'i'.
     *
     * Actually server's data for top level CIDFontTYpe 0 non-disk fonts should not be important,
     * because with non-disk fonts FAPI_do_char never deals with the top-level font,
     * but does with its descendants individually.
     * Therefore a recommendation for the renderer is don't build any special
     * data for the top-level non-disk font of CIDFontType 0, but return immediately
     * with success code and NULL data pointer.
     *
     * get_scaled_font may be called for same font at second time,
     * so the renderen must check whether the data is already built.
     */
    gs_memory_t *mem = pfont->memory;
    gs_font_base *pbfont = (gs_font_base *)pfont;
    int code, bbox_set = 0;
    int BBox[4], scale;
    int units[2];
    double size;
    gs_fapi_font_scale font_scale =
        { {1, 0, 0, 1, 0, 0}, {0, 0}, {1, 1}, true };

    scale = 1 << I->frac_shift;
    size = 1 / hypot(pbfont->FontMatrix.xx, pbfont->FontMatrix.xy);
    /* I believe this is just to ensure minimal rounding problems with scalers that
       scale the FontBBox values with the font scale.
     */
    if (size < 1000)
        size = 1000;

    font_scale.matrix[0] = font_scale.matrix[3] = (int)(size * scale + 0.5);

    font_scale.HWResolution[0] = (fracint) (72 * scale);
    font_scale.HWResolution[1] = (fracint) (72 * scale);

    /* The interpreter specific parts of the gs_fapi_font should
     * be assinged by the caller - now do the generic parts.
     */
    I->ff.subfont = subfont;
    I->ff.font_file_path = font_file_path;
    I->ff.is_type1 = FAPI_ISTYPE1GLYPHDATA(pbfont);
    I->ff.is_vertical = (pbfont->WMode != 0);
    I->ff.memory = mem;
    I->ff.client_ctx_p = I->client_ctx_p;
    I->ff.client_font_data = pbfont;
    I->ff.client_font_data2 = pbfont;
    I->ff.server_font_data = pbfont->FAPI_font_data;    /* Possibly pass it from zFAPIpassfont. */
    if (full_font_buf) {
        I->ff.full_font_buf = (char *)full_font_buf->data;
        I->ff.full_font_buf_len = full_font_buf->size;
    }
    else {
        I->ff.full_font_buf = NULL;
        I->ff.full_font_buf_len = 0;
    }
    I->ff.is_cid = FAPI_ISCIDFONT(pbfont);
    I->ff.is_outline_font = pbfont->PaintType != 0;

    if (!I->ff.is_mtx_skipped)
        I->ff.is_mtx_skipped = (gs_fapi_get_metrics_count(&I->ff) != 0);

    if ((code = gs_fapi_renderer_retcode(mem, I, I->get_scaled_font(I, &I->ff,
                                                                    (const
                                                                     gs_fapi_font_scale
                                                                     *)
                                                                    &font_scale, xlatmap, gs_fapi_toplevel_begin)))
        < 0)
        return code;
    pbfont->FAPI_font_data = I->ff.server_font_data;    /* Save it back to GS font. */

    /* We only want to "refine" the FontBBox for fonts where we allow FAPI to be
       treated as a "black box", handing over the entire font to the FAPI server.
       That is, fonts where we give the server either the file, or a buffer with
       the entire font description in it.
     */
    if (I->ff.server_font_data != 0
        && (font_file_path != NULL || full_font_buf != NULL)) {
        if ((code =
             gs_fapi_renderer_retcode(mem, I,
                                      I->get_font_bbox(I, &I->ff,
                                                       BBox, units))) < 0) {
            gs_fapi_release_typeface(I, &pbfont->FAPI_font_data);
            return code;
        }
        /* Refine FontBBox : */
        pbfont->FontBBox.p.x = ((double)BBox[0] / units[0]);
        pbfont->FontBBox.p.y = ((double)BBox[1] / units[1]);
        pbfont->FontBBox.q.x = ((double)BBox[2] / units[0]);
        pbfont->FontBBox.q.y = ((double)BBox[3] / units[1]);

        bbox_set = 1;
    }

    if (xlatmap != NULL && pbfont->FAPI_font_data != NULL) {
        if ((code =
             gs_fapi_renderer_retcode(mem, I,
                                      I->get_decodingID(I, &I->ff,
                                                        decodingID))) < 0) {
            gs_fapi_release_typeface(I, &pbfont->FAPI_font_data);
            return code;
        }
    }

    /* Prepare descendant fonts : */
    if (font_file_path == NULL && I->ff.is_type1 && I->ff.is_cid) {     /* Renderers should expect same condition. */
        gs_font_cid0 *pfcid = (gs_font_cid0 *) pbfont;
        gs_font_type1 **FDArray = pfcid->cidata.FDArray;
        int i, n = pfcid->cidata.FDArray_size;

        I->ff.is_type1 = true;
        I->ff.is_vertical = false;      /* A subfont may be shared with another fonts. */
        I->ff.memory = mem;
        I->ff.client_ctx_p = I->client_ctx_p;
        for (i = 0; i < n; i++) {
            gs_font_type1 *pbfont1 = FDArray[i];
            int BBox_temp[4];
            int units_temp[2];

            pbfont1->FontBBox = pbfont->FontBBox;       /* Inherit FontBBox from the type 9 font. */

            I->ff.client_font_data = pbfont1;
            pbfont1->FAPI = pbfont->FAPI;
            I->ff.client_font_data2 = pbfont1;
            I->ff.server_font_data = pbfont1->FAPI_font_data;
            I->ff.is_cid = true;
            I->ff.is_outline_font = pbfont1->PaintType != 0;
            if (!I->ff.is_mtx_skipped)
                I->ff.is_mtx_skipped = (gs_fapi_get_metrics_count(&I->ff) != 0);
            I->ff.subfont = 0;
            if ((code =
                 gs_fapi_renderer_retcode(mem, I,
                                          I->get_scaled_font(I, &I->ff,
                                                             (const
                                                              gs_fapi_font_scale
                                                              *)&font_scale,
                                                             NULL, i))) < 0) {
                break;
            }

            pbfont1->FAPI_font_data = I->ff.server_font_data;   /* Save it back to GS font. */
            /* Try to do something with the descendant font to ensure that it's working : */
            if ((code =
                 gs_fapi_renderer_retcode(mem, I,
                                          I->get_font_bbox(I, &I->ff,
                                                           BBox_temp, units_temp))) < 0) {
                break;
            }
            code = gs_notify_register(&pbfont1->notify_list, notify_remove_font, pbfont1);
            if (code < 0) {
                emprintf(mem,
                         "Ignoring gs_notify_register() failure for FAPI font.....");
            }
        }
        if (i == n) {
            code =
                gs_fapi_renderer_retcode(mem, I,
                                         I->get_scaled_font(I, &I->ff,
                                                            (const
                                                             gs_fapi_font_scale
                                                             *)&font_scale,
                                                            NULL,
                                                            gs_fapi_toplevel_complete));
            if (code >= 0)
                return bbox_set;        /* Full success. */
        }
        /* Fail, release server's font data : */
        for (i = 0; i < n; i++) {
            gs_font_type1 *pbfont1 = FDArray[i];

            if (pbfont1->FAPI_font_data != NULL)
                gs_fapi_release_typeface(I, &pbfont1->FAPI_font_data);
        }

        if (pbfont->FAPI_font_data != NULL) {
            gs_fapi_release_typeface(I, &pbfont->FAPI_font_data);
        }
        return_error(gs_error_invalidfont);
    }

    /* This was an "else", but could elicit a warning from static analysis tools
     * about the potential for a non-void function without a return value.
     */
    code = gs_fapi_renderer_retcode(mem, I, I->get_scaled_font(I, &I->ff,
                                                               (const
                                                                gs_fapi_font_scale
                                                                *)&font_scale,
                                                               xlatmap,
                                                               gs_fapi_toplevel_complete));
    if (code < 0) {
        gs_fapi_release_typeface(I, &pbfont->FAPI_font_data);
        return code;
    }
    code =
        gs_notify_register(&pbfont->notify_list, notify_remove_font, pbfont);
    if (code < 0) {
        gs_fapi_release_typeface(I, &pbfont->FAPI_font_data);
        return code;
    }

    return bbox_set;
}


static int
outline_char(gs_memory_t *mem, gs_fapi_server *I, int import_shift_v,
             gs_show_enum *penum_s, struct gx_path_s *path, bool close_path)
{
    gs_fapi_path path_interface = path_interface_stub;
    gs_fapi_outline_handler olh;
    int code = 0;
    gs_gstate *pgs = penum_s->pgs;
    struct gx_path_s path1;

    (void)gx_path_init_local(&path1, mem);

    olh.fserver = I;
    olh.path = &path1;
    olh.x0 = pgs->ctm.tx_fixed - float2fixed(penum_s->fapi_glyph_shift.x);
    olh.y0 = pgs->ctm.ty_fixed - float2fixed(penum_s->fapi_glyph_shift.y);
    olh.close_path = close_path;
    olh.need_close = false;
    path_interface.olh = &olh;
    path_interface.shift = import_shift_v;
    if ((code =
         gs_fapi_renderer_retcode(mem, I,
                                  I->get_char_outline(I,
                                                      &path_interface))) < 0
        || path_interface.gs_error != 0) {
        if (path_interface.gs_error != 0) {
            code = path_interface.gs_error;
            goto done;
        }
        else {
            goto done;
        }
    }
    if (olh.need_close && olh.close_path)
        if ((code = add_closepath(&path_interface)) < 0)
            goto done;
    code = gx_path_copy(&path1, path);
done:
    code = code >= 0 || code == gs_error_undefinedresult ? 0 : code;
    gx_path_free(&path1, "outline_char");
    return code;
}

static void
compute_em_scale(const gs_font_base *pbfont, gs_fapi_metrics *metrics,
                 double FontMatrix_div, double *em_scale_x,
                 double *em_scale_y)
{                               /* optimize : move this stuff to FAPI_refine_font */
    gs_matrix mat;
    gs_matrix *m = &mat;
    int rounding_x, rounding_y; /* Striking out the 'float' representation error in FontMatrix. */
    double sx, sy;
    gs_fapi_server *I = pbfont->FAPI;

#if 1
    I->get_fontmatrix(I, m);
#else
    /* Temporary: replace with a FAPI call to check *if* the library needs a replacement matrix */
    memset(m, 0x00, sizeof(gs_matrix));
    m->xx = m->yy = 1.0;
#endif

    if (m->xx == 0 && m->xy == 0 && m->yx == 0 && m->yy == 0)
        m = &pbfont->base->FontMatrix;
    sx = hypot(m->xx, m->xy) * metrics->em_x / FontMatrix_div;
    sy = hypot(m->yx, m->yy) * metrics->em_y / FontMatrix_div;
    rounding_x = (int)(0x00800000 / sx);
    rounding_y = (int)(0x00800000 / sy);
    *em_scale_x = (int)(sx * rounding_x + 0.5) / (double)rounding_x;
    *em_scale_y = (int)(sy * rounding_y + 0.5) / (double)rounding_y;
}

static int
fapi_copy_mono(gx_device *dev1, gs_fapi_raster *rast, int dx, int dy)
{
    int line_step = bitmap_raster(rast->width), code;

    if (rast->line_step >= line_step) {
        return dev_proc(dev1, copy_mono) (dev1, rast->p, 0, rast->line_step,
                                          0, dx, dy, rast->width,
                                          rast->height, 0, 1);
    }
    else {                      /* bitmap data needs to be aligned, make the aligned copy : */
        byte *p = gs_alloc_byte_array(dev1->memory, rast->height, line_step,
                                      "fapi_copy_mono");
        byte *q = p, *r = rast->p, *pe;

        if (p == NULL)
            return_error(gs_error_VMerror);
        pe = p + rast->height * line_step;
        for (; q < pe; q += line_step, r += rast->line_step) {
            memcpy(q, r, rast->line_step);
            memset(q + rast->line_step, 0, line_step - rast->line_step);
        }
        code =
            dev_proc(dev1, copy_mono) (dev1, p, 0, line_step, 0, dx, dy,
                                       rast->width, rast->height, 0, 1);
        gs_free_object(dev1->memory, p, "fapi_copy_mono");
        return code;
    }
}

/*
 * For PCL/PXL pseudo-bolding, we have to "smear" a bitmap horizontally and
 * vertically by ORing together a rectangle of bits below and to the left of
 * each output bit.  We do this separately for horizontal and vertical
 * smearing.  Eventually, we will replace the current procedure, which takes
 * time proportional to W * H * (N + log2(N)), with one that is only
 * proportional to N (but takes W * N additional buffer space).
 */

/* Allocate the line buffer for bolding.  We need 2 + bold scan lines. */
static byte *
alloc_bold_lines(gs_memory_t *mem, uint width, int bold, client_name_t cname)
{       return gs_alloc_byte_array(mem, 2 + bold, bitmap_raster(width + bold),
                                   cname);
}

/* Merge one (aligned) scan line into another, for vertical smearing. */
static void
bits_merge(byte *dest, const byte *src, uint nbytes)
{       long *dp = (long *)dest;
        const long *sp = (const long *)src;
        uint n = (nbytes + sizeof(long) - 1) >> ARCH_LOG2_SIZEOF_LONG;

        for ( ; n >= 4; sp += 4, dp += 4, n -= 4 )
          dp[0] |= sp[0], dp[1] |= sp[1], dp[2] |= sp[2], dp[3] |= sp[3];
        for ( ; n; ++sp, ++dp, --n )
          *dp |= *sp;
}

/* Smear a scan line horizontally.  Note that the output is wider than */
/* the input by the amount of bolding (smear_width). */
static void
bits_smear_horizontally(byte *dest, const byte *src, uint width,
  uint smear_width)
{       uint bits_on = 0;
        const byte *sp = src;
        uint sbyte = *sp;
        byte *dp = dest;
        uint dbyte = sbyte;
        uint sdmask = 0x80;
        const byte *zp = src;
        uint zmask = 0x80;
        uint i = 0;

        /* Process the first smear_width bits. */
        { uint stop = min(smear_width, width);

          for ( ; i < stop; ++i ) {
            if ( sbyte & sdmask )
              bits_on++;
            else if ( bits_on )
              dbyte |= sdmask;
            if ( (sdmask >>= 1) == 0 )
              sdmask = 0x80, *dp++ = dbyte, dbyte = sbyte = *++sp;
          }
        }

        /* Process all but the last smear_width bits. */
        { for ( ; i < width; ++i ) {
            if ( sbyte & sdmask )
              bits_on++;
            else if ( bits_on )
              dbyte |= sdmask;
            if ( *zp & zmask )
              --bits_on;
            if ( (sdmask >>= 1) == 0 ) {
              sdmask = 0x80;
              *dp++ = dbyte;
on:           switch ( (dbyte = sbyte = *++sp) ) {
                case 0xff:
                  if ( width - i <= 8 )
                    break;
                  *dp++ = 0xff;
                  bits_on += 8 -
                    byte_count_bits[(*zp & (zmask - 1)) + (zp[1] & -(int)zmask)];
                  ++zp;
                  i += 8;
                  goto on;
                case 0:
                  if ( bits_on || width - i <= 8 )
                    break;
                  *dp++ = 0;
                  /* We know there can't be any bits to be zeroed, */
                  /* because bits_on can't go negative. */
                  ++zp;
                  i += 8;
                  goto on;
                default:
                  ;
              }
            }
            if ( (zmask >>= 1) == 0 )
              zmask = 0x80, ++zp;
          }
        }

        /* Process the last smear_width bits. */
        /****** WRONG IF width < smear_width ******/
        { uint stop = width + smear_width;

          for ( ; i < stop; ++i ) {
            if ( bits_on )
              dbyte |= sdmask;
            if ( (sdmask >>= 1) == 0 )
              sdmask = 0x80, *dp++ = dbyte, dbyte = 0;
            if ( *zp & zmask )
              --bits_on;
            if ( (zmask >>= 1) == 0 )
              zmask = 0x80, ++zp;
          }
        }

        if ( sdmask != 0x80 )
          *dp = dbyte;
}

static const int frac_pixel_shift = 4;

/* NOTE: fapi_image_uncached_glyph() doesn't check various paramters: it assumes fapi_finish_render_aux()
 * has done so: if it gets called from another function, the function must either do all the parameter
 * validation, or fapi_image_uncached_glyph() must be changed to include the validation.
 */
static int
fapi_image_uncached_glyph(gs_font *pfont, gs_gstate *pgs, gs_show_enum *penum,
                          gs_fapi_raster *rast, const int import_shift_v)
{
    gx_device *dev = penum->dev;
    gs_gstate *penum_pgs = (gs_gstate *) penum->pgs;
    int code;
    const gx_clip_path *pcpath = pgs->clip_path;
    const gx_drawing_color *pdcolor = gs_currentdevicecolor_inline(penum->pgs);
    int rast_orig_x = rast->orig_x;
    int rast_orig_y = -rast->orig_y;
    gs_font_base *pbfont = (gs_font_base *)pfont;
    gs_fapi_server *I = pbfont->FAPI;

    extern_st(st_gs_show_enum);

    byte *r = rast->p;
    byte *src, *dst;
    int h, padbytes, cpbytes, dstr = bitmap_raster(rast->width);
    int sstr = rast->line_step;

    double dx = penum_pgs->ctm.tx + (double)rast_orig_x / (1 << frac_pixel_shift) + 0.5;
    double dy = penum_pgs->ctm.ty + (double)rast_orig_y / (1 << frac_pixel_shift) + 0.5;

    /* we can only safely use the gx_image_fill_masked() "shortcut" if we're drawing
     * a "simple" colour, rather than a pattern.
     */
    if (gs_color_writes_pure(penum_pgs) && I->ff.embolden == 0.0) {
        if (dstr != sstr) {

            /* If the stride of the bitmap we've got doesn't match what the rest
             * of the Ghostscript world expects, make one that does.
             * Ghostscript aligns bitmap raster memory in a platform specific
             * manner, so see gxbitmap.h for details.
             *
             * Ideally the padding bytes wouldn't matter, but currently the
             * clist code ends up compressing it using bitmap compression. To
             * ensure consistency across runs (and to get the best possible
             * compression ratios) we therefore set such bytes to zero. It would
             * be nicer if this was fixed in future.
             */
            r = gs_alloc_bytes(penum->memory, (size_t)dstr * rast->height,
                               "fapi_finish_render_aux");
            if (!r) {
                return_error(gs_error_VMerror);
            }

            cpbytes = sstr < dstr ? sstr : dstr;
            padbytes = dstr - cpbytes;
            h = rast->height;
            src = rast->p;
            dst = r;
            if (padbytes > 0) {
                while (h-- > 0) {
                    memcpy(dst, src, cpbytes);
                    memset(dst + cpbytes, 0, padbytes);
                    src += sstr;
                    dst += dstr;
                }
            }
            else {
                while (h-- > 0) {
                    memcpy(dst, src, cpbytes);
                    src += sstr;
                    dst += dstr;
                }
            }
        }

        if (gs_object_type(penum->memory, penum) == &st_gs_show_enum) {
            dx += penum->fapi_glyph_shift.x;
            dy += penum->fapi_glyph_shift.y;
        }
        /* Processing an image object operation, but this may be for a text object */
        ensure_tag_is_set(pgs, pgs->device, GS_TEXT_TAG);	/* NB: may unset_dev_color */
        code = gx_set_dev_color(pgs);
        if (code != 0)
            return code;
        code = gs_gstate_color_load(pgs);
        if (code < 0)
            return code;

        code = gx_image_fill_masked(dev, r, 0, dstr, gx_no_bitmap_id,
                                    (int)dx, (int)dy,
                                    rast->width, rast->height,
                                    pdcolor, 1, rop3_default, pcpath);
        if (rast->p != r) {
            gs_free_object(penum->memory, r, "fapi_finish_render_aux");
        }
    }
    else {
        gs_memory_t *mem = penum->memory->non_gc_memory;
        gs_image_enum *pie;
        gs_image_t image;
        int iy, nbytes;
        uint used;
        int code1;
        int w, h;
        int x = (int)dx;
        int y = (int)dy;
        uint bold = 0;
        byte *bold_lines = NULL;
        byte *line = NULL;
        int ascent = 0;

        pie = gs_image_enum_alloc(mem, "image_char(image_enum)");
        if (!pie) {
            return_error(gs_error_VMerror);
        }

        w = rast->width;
        h = rast->height;
        if (I->ff.embolden != 0.0) {
            bold = (uint)(2 * h * I->ff.embolden + 0.5);
            ascent += bold;
            bold_lines = alloc_bold_lines(pgs->memory, w, bold, "fapi_image_uncached_glyph(bold_line)");
            if (bold_lines == NULL) {
                code = gs_note_error(gs_error_VMerror);
                return(code);
            }
            line = gs_alloc_byte_array(pgs->memory, 1, bitmap_raster(w + bold) + 1, "fapi_copy_mono");
            if (line == 0) {
                gs_free_object(pgs->memory, bold_lines, "fapi_image_uncached_glyph(bold_lines)");
                code = gs_note_error(gs_error_VMerror);
                return(code);
            }
        }

        /* Make a matrix that will place the image */
        /* at (x,y) with no transformation. */
        gs_image_t_init_mask(&image, true);
        gs_make_translation((double) - x, (double) (- y + ascent), &image.ImageMatrix);
        gs_matrix_multiply(&ctm_only(penum_pgs), &image.ImageMatrix, &image.ImageMatrix);
        image.Width = w + bold;
        image.Height = h + bold;
        image.adjust = false;
        code = gs_image_init(pie, &image, false, true, penum_pgs);
        nbytes = (rast->width + 7) >> 3;

        switch (code) {
            case 1:            /* empty image */
                code = 0;
            default:
                break;
            case 0:
                if (bold == 0) {
                    for (iy = 0; iy < h && code >= 0; iy++, r += sstr) {
                        code = gs_image_next(pie, r, nbytes, &used);
                    }
                }
                else {
                    uint dest_raster = bitmap_raster(image.Width);
                    uint dest_bytes = (image.Width + 7) >> 3;
                    int n1 = bold + 1;
#define merged_line(i) (bold_lines + ((i) % n1 + 1) * dest_raster)

                    for ( y = 0; y < image.Height; y++ ) {
                        int y0 = (y < bold ? 0 : y - bold);
                        int y1 = min(y + 1, rast->height);
                        int kmask;
                        bool first = true;

                        if ( y < rast->height ) {
                            memcpy(line, r + y * rast->line_step, rast->line_step);
                            memset(line + rast->line_step, 0x00, (dest_raster + 1) -  rast->line_step);

                            bits_smear_horizontally(merged_line(y), line, rast->width, bold);
                            /* Now re-establish the invariant -- see below. */
                            kmask = 1;

                            for ( ; (y & kmask) == kmask && y - kmask >= y0;
                                  kmask = (kmask << 1) + 1) {

                                bits_merge(merged_line(y - kmask), merged_line(y - (kmask >> 1)), dest_bytes);
                            }
                        }

                       /*
                        * As of this point in the loop, we maintain the following
                        * invariant to cache partial merges of groups of lines: for
                        * each Y, y0 <= Y < y1, let K be the maximum k such that Y
                        * mod 2^k = 0 and Y + 2^k < y1; then merged_line(Y) holds
                        * the union of horizontally smeared source lines Y through
                        * Y + 2^k - 1.  The idea behind this is similar to the idea
                        * of quicksort.
                        */

                        /* Now construct the output line. */
                        first = true;

                        for ( iy = y1 - 1; iy >= y0; --iy ) {
                          kmask = 1;

                          while ( (iy & kmask) == kmask && iy - kmask >= y0 )
                            iy -= kmask, kmask <<= 1;
                          if ( first ) {
                            memcpy(bold_lines, merged_line(iy), dest_bytes);
                            first = false;
                          }
                          else
                            bits_merge(bold_lines, merged_line(iy), dest_bytes);
                        }
                        code = gs_image_next(pie, bold_lines, dest_bytes, &used);
                    }
                }
#undef merged_line
        }

        if (bold_lines)
            gs_free_object(pgs->memory, bold_lines, "fapi_image_uncached_glyph(bold_lines)");

        if (line)
            gs_free_object(pgs->memory, line, "fapi_image_uncached_glyph(line)");

        code1 = gs_image_cleanup_and_free_enum(pie, penum_pgs);
        if (code >= 0 && code1 < 0)
            code = code1;
    }
    return (code);
}

int
gs_fapi_finish_render(gs_font *pfont, gs_gstate *pgs, gs_text_enum_t *penum, gs_fapi_server *I)
{
    gs_show_enum *penum_s = (gs_show_enum *) penum;
    gs_gstate *penum_pgs;
    gx_device *dev1;
    const int import_shift_v = _fixed_shift - 32;       /* we always 32.32 values for the outline interface now */
    gs_fapi_raster rast;
    int code;
    gs_memory_t *mem = pfont->memory;
    gs_font_base *pbfont = (gs_font_base *)pfont;

    if (penum == NULL)
        return_error(gs_error_undefined);

    penum_pgs = penum_s->pgs;

    dev1 = gs_currentdevice_inline(penum_pgs);  /* Possibly changed by zchar_set_cache. */

    /* Even for "non-marking" text operations (for example, stringwidth) we are expected
     * to have a glyph bitmap for the cache, if we're using the cache. For the
     * non-cacheing, non-marking cases, we must not draw the glyph.
     */
    if (pgs->in_charpath && !SHOW_IS(penum, TEXT_DO_NONE)) {
        gs_point pt;

        if ((code = gs_currentpoint(penum_pgs, &pt)) < 0)
            return code;

        if ((code =
             outline_char(mem, I, import_shift_v, penum_s, penum_pgs->path,
                          !pbfont->PaintType)) < 0) {
            return code;
        }

        if ((code =
             gx_path_add_char_path(penum_pgs->show_gstate->path,
                                   penum_pgs->path,
                                   penum_pgs->in_charpath)) < 0) {
            return code;
        }

    }
    else {
        int code;
        memset(&rast, 0x00, sizeof(rast));

        if ((code = I->get_char_raster(I, &rast)) < 0 && code != gs_error_unregistered)
            return code;

        if (!SHOW_IS(penum, TEXT_DO_NONE) && I->use_outline) {
            /* The server provides an outline instead the raster. */
            gs_point pt;

            /* This mimics code which is used below in the case where I->Use_outline is set.
             * This is usually caused by setting -dTextAlphaBits, and will result in 'undefinedresult'
             * errors. We want the code to work the same regardless off the setting of -dTextAlphaBits
             * and it seems the simplest way to provoke the same error is to do the same steps....
             * Note that we do not actually ever use the returned value.
             */
            if ((code = gs_currentpoint(penum_pgs, &pt)) < 0)
                return code;

            if ((code =
                 outline_char(mem, I, import_shift_v, penum_s,
                              penum_pgs->path, !pbfont->PaintType)) < 0)
                return code;
            if ((code =
                 gs_gstate_setflat((gs_gstate *) penum_pgs,
                                   gs_char_flatness(penum_pgs->show_gstate, 1.0))) < 0)
                return code;
            if (pbfont->PaintType) {
                float lw = gs_currentlinewidth(penum_pgs);

                gs_setlinewidth(penum_pgs, pbfont->StrokeWidth);
                code = gs_stroke(penum_pgs);
                gs_setlinewidth(penum_pgs, lw);
                if (code < 0)
                    return code;
            }
            else {
                gs_in_cache_device_t in_cachedevice =
                    penum_pgs->in_cachedevice;

                penum_pgs->in_cachedevice = CACHE_DEVICE_NOT_CACHING;

                penum_pgs->fill_adjust.x = penum_pgs->fill_adjust.y = 0;

                code = gs_fill(penum_pgs);

                penum_pgs->in_cachedevice = in_cachedevice;

                if (code < 0)
                    return code;
            }
            if ((code = gs_moveto(penum_pgs, pt.x, pt.y)) < 0)
                return code;
        }
        else {
            int rast_orig_x = rast.orig_x;
            int rast_orig_y = -rast.orig_y;
            gs_point pt;

            /* This mimics code which is used above in the case where I->Use_outline is set.
             * This is usually caused by setting -dTextAlphaBits, and will result in 'undefinedresult'
             * errors. We want the code to work the same regardless off the setting of -dTextAlphaBits
             * and it seems the simplest way to provoke the same error is to do the same steps....
             * Note that we do not actually ever use the returned value.
             */
            if ((code = gs_currentpoint(penum_pgs, &pt)) < 0)
                return code;

            if (penum_pgs->in_cachedevice == CACHE_DEVICE_CACHING) {    /* Using GS cache */
                /*  GS and renderer may transform coordinates few differently.
                   The best way is to make set_cache_device to take the renderer's bitmap metrics immediately,
                   but we need to account CDevProc, which may truncate the bitmap.
                   Perhaps GS overestimates the bitmap size,
                   so now we only add a compensating shift - the dx and dy.
                 */
                if (rast.width != 0) {
                    int shift_rd = _fixed_shift - frac_pixel_shift;
                    int rounding = 1 << (frac_pixel_shift - 1);
                    int dx =
                        arith_rshift_slow((penum_pgs->
                                           ctm.tx_fixed >> shift_rd) +
                                          rast_orig_x + rounding,
                                          frac_pixel_shift);
                    int dy =
                        arith_rshift_slow((penum_pgs->
                                           ctm.ty_fixed >> shift_rd) +
                                          rast_orig_y + rounding,
                                          frac_pixel_shift);

                    if (dx + rast.left_indent < 0
                        || dx + rast.left_indent + rast.black_width >
                        dev1->width) {
#ifdef DEBUG
                        if (gs_debug_c('m')) {
                            emprintf2(dev1->memory,
                                      "Warning : Cropping a FAPI glyph while caching : dx=%d,%d.\n",
                                      dx + rast.left_indent,
                                      dx + rast.left_indent +
                                      rast.black_width - dev1->width);
                        }
#endif
                        if (dx + rast.left_indent < 0)
                            dx -= dx + rast.left_indent;
                    }
                    if (dy + rast.top_indent < 0
                        || dy + rast.top_indent + rast.black_height >
                        dev1->height) {
#ifdef DEBUG
                        if (gs_debug_c('m')) {
                            emprintf2(dev1->memory,
                                      "Warning : Cropping a FAPI glyph while caching : dx=%d,%d.\n",
                                      dy + rast.top_indent,
                                      dy + rast.top_indent +
                                      rast.black_height - dev1->height);
                        }
#endif
                        if (dy + rast.top_indent < 0)
                            dy -= dy + rast.top_indent;
                    }
                    if ((code = fapi_copy_mono(dev1, &rast, dx, dy)) < 0)
                        return code;

                    if (penum_s->cc != NULL) {
                        penum_s->cc->offset.x +=
                            float2fixed(penum_s->fapi_glyph_shift.x);
                        penum_s->cc->offset.y +=
                            float2fixed(penum_s->fapi_glyph_shift.y);
                    }
                }
            }
            else if (!SHOW_IS(penum, TEXT_DO_NONE)) {   /* Not using GS cache */
                if ((code =
                     fapi_image_uncached_glyph(pfont, pgs, penum_s, &rast,
                                               import_shift_v)) < 0)
                    return code;
            }
        }
    }

    return 0;
}


#define GET_U16_MSB(p) (((uint)((p)[0]) << 8) + (p)[1])
#define GET_S16_MSB(p) (int)((GET_U16_MSB(p) ^ 0x8000) - 0x8000)

#define MTX_EQ(mtx1,mtx2) (mtx1->xx == mtx2->xx && mtx1->xy == mtx2->xy && \
                           mtx1->yx == mtx2->yx && mtx1->yy == mtx2->yy)

int
gs_fapi_do_char(gs_font *pfont, gs_gstate *pgs, gs_text_enum_t *penum, char *font_file_path,
                bool bBuildGlyph, gs_string *charstring, gs_string *glyphname,
                gs_char chr, gs_glyph index, int subfont)
{                               /* Stack : <font> <code|name> --> - */
    gs_show_enum *penum_s = (gs_show_enum *) penum;
    gx_device *dev = gs_currentdevice_inline(pgs);

    /*
       fixme: the following code needs to optimize with a maintainence of scaled font objects
       in graphics library and in interpreter. Now we suppose that the renderer
       uses font cache, so redundant font opening isn't a big expense.
     */
    gs_fapi_char_ref cr =
        { 0, {0}, 0, false, NULL, 0, 0, 0, 0, 0, gs_fapi_metrics_notdef };
    gs_font_base *pbfont = (gs_font_base *)pfont;
    const gs_matrix *ctm = &ctm_only(pgs);
    int scale;
    gs_fapi_metrics metrics;
    gs_fapi_server *I = pbfont->FAPI;

    gs_string enc_char_name_string;
    bool bCID = (FAPI_ISCIDFONT(pbfont) || charstring != NULL);
    bool bIsType1GlyphData = FAPI_ISTYPE1GLYPHDATA(pbfont);
    gs_log2_scale_point log2_scale = { 0, 0 };
    int alpha_bits = (*dev_proc(dev, get_alpha_bits)) (dev, go_text);
    double FontMatrix_div = 1;
    bool bVertical = (gs_rootfont(pgs)->WMode != 0), bVertical0 = bVertical;
    double *sbwp, sbw[4] = { 0, 0, 0, 0 };
    double em_scale_x, em_scale_y;
    gs_rect char_bbox;
    int code;
    bool imagenow = false;
    bool align_to_pixels = gs_currentaligntopixels(pbfont->dir);
    gs_memory_t *mem = pfont->memory;
    enum
    {
        SBW_DONE,
        SBW_SCALE,
        SBW_FROM_RENDERER
    } sbw_state = SBW_SCALE;

    if ( index == GS_NO_GLYPH )
        return 0;

    I->use_outline = false;
    I->transform_outline = false;

    if (penum == 0)
        return_error(gs_error_undefined);

    I->use_outline =
        produce_outline_char(penum_s, pbfont, alpha_bits, &log2_scale);

    if (I->use_outline) {
        I->max_bitmap = 0;
    }
    else {
        /* FIX ME: It can be a very bad thing, right now, for the FAPI code to decide unilaterally to
         * produce an outline, when the rest of GS expects a bitmap, so we give ourselves a
         * 50% leeway on the maximum cache bitmap, just to be sure. Or the same maximum bitmap size
         * used in gxchar.c
         */
        I->max_bitmap =
            pbfont->dir->ccache.upper + (pbfont->dir->ccache.upper >> 1) <
            MAX_CCACHE_TEMP_BITMAP_BITS ? pbfont->dir->ccache.upper +
            (pbfont->dir->ccache.upper >> 1) : MAX_CCACHE_TEMP_BITMAP_BITS;
    }

    if (pbfont->memory->gs_lib_ctx->font_dir != NULL)
        I->grid_fit = pbfont->memory->gs_lib_ctx->font_dir->grid_fit_tt;
    else
        I->grid_fit = pbfont->dir->grid_fit_tt;

    /* Compute the scale : */
    if (!SHOW_IS(penum, TEXT_DO_NONE) && !I->use_outline) {
        gs_currentcharmatrix(pgs, NULL, 1);     /* make char_tm valid */
        penum_s->fapi_log2_scale = log2_scale;
    }
    else {
        log2_scale.x = 0;
        log2_scale.y = 0;
    }

    /* Prepare font data
     * This needs done here (earlier than it used to be) because FAPI/UFST has conflicting
     * requirements in what I->get_fontmatrix() returns based on font type, so it needs to
     * find the font type.
     */

    I->ff.memory = pbfont->memory;
    I->ff.subfont = subfont;
    I->ff.font_file_path = font_file_path;
    I->ff.client_font_data = pbfont;
    I->ff.client_font_data2 = pbfont;
    I->ff.server_font_data = pbfont->FAPI_font_data;
    I->ff.is_type1 = bIsType1GlyphData;
    I->ff.is_cid = bCID;
    I->ff.is_outline_font = pbfont->PaintType != 0;
    I->ff.metrics_only = fapi_gs_char_show_width_only(penum);

    if (!I->ff.is_mtx_skipped)
        I->ff.is_mtx_skipped = (gs_fapi_get_metrics_count(&I->ff) != 0);

    I->ff.is_vertical = bVertical;
    I->ff.client_ctx_p = I->client_ctx_p;

    if (recreate_multiple_master(pbfont)) {
        if ((void *)pbfont->base == (void *)pbfont) {
           gs_fapi_release_typeface(I, &pbfont->FAPI_font_data);
        }
        else {
            pbfont->FAPI_font_data = NULL;
            pbfont->FAPI->face.font_id = gs_no_id;
        }
    }

   scale = 1 << I->frac_shift;
retry_oversampling:
    if (I->face.font_id != pbfont->id ||
        !MTX_EQ((&I->face.ctm), ctm) ||
        I->face.log2_scale.x != log2_scale.x ||
        I->face.log2_scale.y != log2_scale.y ||
        I->face.align_to_pixels != align_to_pixels ||
        I->face.HWResolution[0] != dev->HWResolution[0] ||
        I->face.HWResolution[1] != dev->HWResolution[1]
        ) {
        gs_fapi_font_scale font_scale = { {1, 0, 0, 1, 0, 0}
        , {0, 0}
        , {1, 1}
        , true
        };

        gs_matrix lctm, scale_mat, scale_ctm;
        I->face.font_id = pbfont->id;
        I->face.log2_scale = log2_scale;
        I->face.align_to_pixels = align_to_pixels;
        I->face.HWResolution[0] = dev->HWResolution[0];
        I->face.HWResolution[1] = dev->HWResolution[1];

        font_scale.subpixels[0] = 1 << log2_scale.x;
        font_scale.subpixels[1] = 1 << log2_scale.y;
        font_scale.align_to_pixels = align_to_pixels;

        /* We apply the entire transform to the glyph (that is ctm x FontMatrix)
         * at render time.
         */
        lctm = *ctm;
retry_scaling:
        I->face.ctm = lctm;
        memset(&scale_ctm, 0x00, sizeof(gs_matrix));
        scale_ctm.xx = dev->HWResolution[0] / 72;
        scale_ctm.yy = dev->HWResolution[1] / 72;

        if ((code = gs_matrix_invert((const gs_matrix *)&scale_ctm, &scale_ctm)) < 0)
            return code;

        if ((code = gs_matrix_multiply(&lctm, &scale_ctm, &scale_mat)) < 0)  /* scale_mat ==  CTM - resolution scaling */
            return code;

        if ((code = I->get_fontmatrix(I, &scale_ctm)) < 0)
            return code;

        if ((code = gs_matrix_invert((const gs_matrix *)&scale_ctm, &scale_ctm)) < 0)
            return code;

        if ((code = gs_matrix_multiply(&scale_mat, &scale_ctm, &scale_mat)) < 0)  /* scale_mat ==  CTM - resolution scaling - FontMatrix scaling */
            return code;

        if (((int64_t)(scale_mat.xx * FontMatrix_div * scale + 0.5)) != ((int32_t)(scale_mat.xx * FontMatrix_div * scale + 0.5))
        ||  ((int64_t)(scale_mat.xy * FontMatrix_div * scale + 0.5)) != ((int32_t)(scale_mat.xy * FontMatrix_div * scale + 0.5))
        ||  ((int64_t)(scale_mat.yx * FontMatrix_div * scale + 0.5)) != ((int32_t)(scale_mat.yx * FontMatrix_div * scale + 0.5))
        ||  ((int64_t)(scale_mat.yy * FontMatrix_div * scale + 0.5)) != ((int32_t)(scale_mat.yy * FontMatrix_div * scale + 0.5))) {
            /* Overflow
               If the scaling is large enough to overflow the 16.16 representation, we forcibly produce an outline
               unscaled except an arbitrary "midrange" scale (chosen to avoid under/overflow issues). And
               then scale the points as we create the Ghostscript path outline_char().
               If the glyph is this large, we're really not worried about hinting or dropout detection etc.
             */

            memset(&lctm, 0x00, sizeof(gs_matrix));
            lctm.xx = 256.0;
            lctm.yy = 256.0;
            I->transform_outline = true;
            I->use_outline = true;
            if ((code = gs_matrix_invert((const gs_matrix *)&lctm, &scale_ctm)) < 0)
                 return code;
            if ((code = gs_matrix_multiply(ctm, &scale_ctm, &scale_mat)) < 0)  /* scale_mat ==  CTM - resolution scaling */
                return code;

            I->outline_mat = scale_mat;
            goto retry_scaling;
        }
        else {
            font_scale.matrix[0] =  FAPI_ROUND_TO_FRACINT(scale_mat.xx * FontMatrix_div * scale);
            font_scale.matrix[1] = -FAPI_ROUND_TO_FRACINT(scale_mat.xy * FontMatrix_div * scale);
            font_scale.matrix[2] =  FAPI_ROUND_TO_FRACINT(scale_mat.yx * FontMatrix_div * scale);
            font_scale.matrix[3] = -FAPI_ROUND_TO_FRACINT(scale_mat.yy * FontMatrix_div * scale);
            font_scale.matrix[4] =  FAPI_ROUND_TO_FRACINT(scale_mat.tx * FontMatrix_div * scale);
            font_scale.matrix[5] =  FAPI_ROUND_TO_FRACINT(scale_mat.ty * FontMatrix_div * scale);
        }

        /* Note: the ctm mapping here is upside down. */
        font_scale.HWResolution[0] =
            (fracint) ((double)dev->HWResolution[0] *
                       font_scale.subpixels[0] * scale);
        font_scale.HWResolution[1] =
            (fracint) ((double)dev->HWResolution[1] *
                       font_scale.subpixels[1] * scale);


        if ((hypot((double)font_scale.matrix[0], (double)font_scale.matrix[2])
             == 0.0
             || hypot((double)font_scale.matrix[1],
                      (double)font_scale.matrix[3]) == 0.0)) {

            /* If the matrix is degenerate, force a scale to 1 unit. */
            memset(&font_scale.matrix, 0x00, sizeof(font_scale.matrix));
            if (!font_scale.matrix[0])
                font_scale.matrix[0] = 1;
            if (!font_scale.matrix[3])
                font_scale.matrix[3] = 1;
        }

        if ((code =
             gs_fapi_renderer_retcode(mem, I,
                                      I->get_scaled_font(I, &I->ff,
                                                         &font_scale, NULL,
                                                         gs_fapi_toplevel_prepared))) < 0) {
            return code;
        }
        pbfont->FAPI_font_data = I->ff.server_font_data;    /* Save it back to GS font. */
    }

    cr.char_codes_count = 1;
    cr.char_codes[0] = index;
    cr.client_char_code = chr;
    cr.is_glyph_index = true;
    enc_char_name_string.data = NULL;
    enc_char_name_string.size = 0;

    if (I->ff.get_glyphname_or_cid) {
        if ((code =
             I->ff.get_glyphname_or_cid(penum, pbfont, charstring, glyphname, index,
                                        &enc_char_name_string, font_file_path,
                                        &cr, bCID)) < 0)
            return (code);
    }

    /* Compute the metrics replacement : */
    if (bCID && !bIsType1GlyphData) {
        gs_font_cid2 *pfcid = (gs_font_cid2 *) pbfont;
        int MetricsCount = pfcid->cidata.MetricsCount;

        if (MetricsCount > 0) {
            const byte *data_ptr;
            int l = I->ff.get_glyphdirectory_data(&(I->ff), cr.char_codes[0],
                                                  &data_ptr);

            /* We do not need to apply the unitsPerEm scale here because
             * these values are read directly from the glyph data.
             */
            if (MetricsCount == 2 && l >= 4) {
                if (!bVertical0) {
                    cr.sb_x = GET_S16_MSB(data_ptr + 2) * scale;
                    cr.aw_x = GET_U16_MSB(data_ptr + 0) * scale;
                    cr.metrics_type = gs_fapi_metrics_replace;
                }
            }
            else if (l >= 8) {
                cr.sb_y = GET_S16_MSB(data_ptr + 2) * scale;
                cr.aw_y = GET_U16_MSB(data_ptr + 0) * scale;
                cr.sb_x = GET_S16_MSB(data_ptr + 6) * scale;
                cr.aw_x = GET_U16_MSB(data_ptr + 4) * scale;
                cr.metrics_type = gs_fapi_metrics_replace;
            }
        }
    }
    /* Metrics in GS are scaled to a 1.0x1.0 square, as that's what Postscript
     * fonts expect. But for Truetype we need the "native" units,
     */
    em_scale_x = 1.0;
    if (pfont->FontType == ft_TrueType) {
        gs_font_type42 *pfont42 = (gs_font_type42 *) pfont;
        em_scale_x = pfont42->data.unitsPerEm;
    }

    if (cr.metrics_type != gs_fapi_metrics_replace && bVertical) {
        double pwv[4];

        code =
            I->ff.fapi_get_metrics(&I->ff, &enc_char_name_string, index, pwv,
                                   bVertical);
        if (code < 0)
            return code;
        if (code == 0 /* metricsNone */ ) {
            if (bCID && (!bIsType1GlyphData && font_file_path)) {
                cr.sb_x = (fracint)(fapi_round( (sbw[2] / 2)         * scale) * em_scale_x);
                cr.sb_y = (fracint)(fapi_round( pbfont->FontBBox.q.y * scale) * em_scale_x);
                cr.aw_y = (fracint)(fapi_round(-pbfont->FontBBox.q.x * scale) * em_scale_x);    /* Sic ! */
                cr.metrics_scale = 1;
                cr.metrics_type = gs_fapi_metrics_replace;
                sbw[0] = sbw[2] / 2;
                sbw[1] = pbfont->FontBBox.q.y;
                sbw[2] = 0;
                sbw[3] = -pbfont->FontBBox.q.x; /* Sic ! */
                sbw_state = SBW_DONE;
            }
            else {
                bVertical = false;
            }
        }
        else {
            cr.sb_x = (fracint)(fapi_round(pwv[2] * scale) * em_scale_x);
            cr.sb_y = (fracint)(fapi_round(pwv[3] * scale) * em_scale_x);
            cr.aw_x = (fracint)(fapi_round(pwv[0] * scale) * em_scale_x);
            cr.aw_y = (fracint)(fapi_round(pwv[1] * scale) * em_scale_x);
            cr.metrics_scale = (bIsType1GlyphData ? 1000 : 1);
            cr.metrics_type = (code == 2 /* metricsSideBearingAndWidth */ ? gs_fapi_metrics_replace
                               : gs_fapi_metrics_replace_width);
            sbw[0] = pwv[2];
            sbw[1] = pwv[3];
            sbw[2] = pwv[0];
            sbw[3] = pwv[1];
            sbw_state = SBW_DONE;
        }
    }
    if (cr.metrics_type == gs_fapi_metrics_notdef && !bVertical) {
        code =
            I->ff.fapi_get_metrics(&I->ff, &enc_char_name_string, (int)index, sbw,
                                   bVertical);
        if (code < 0)
            return code;
        if (code == 0 /* metricsNone */ ) {
            sbw_state = SBW_FROM_RENDERER;
            if (pbfont->FontType == 2) {
                gs_font_type1 *pfont1 = (gs_font_type1 *) pbfont;

                cr.aw_x =
                    export_shift(pfont1->data.defaultWidthX,
                                 _fixed_shift - I->frac_shift);
                cr.metrics_scale = 1000;
                cr.metrics_type = gs_fapi_metrics_add;
            }
        }
        else {
            cr.sb_x = fapi_round(sbw[0] * scale * em_scale_x);
            cr.sb_y = fapi_round(sbw[1] * scale * em_scale_x);
            cr.aw_x = fapi_round(sbw[2] * scale * em_scale_x);
            cr.aw_y = fapi_round(sbw[3] * scale * em_scale_x);
            cr.metrics_scale = (bIsType1GlyphData ? 1000 : 1);
            cr.metrics_type = (code == 2 /* metricsSideBearingAndWidth */ ? gs_fapi_metrics_replace
                               : gs_fapi_metrics_replace_width);
            sbw_state = SBW_DONE;
        }
    }
    memset(&metrics, 0x00, sizeof(metrics));
    /* Take metrics from font : */
    if (I->ff.metrics_only) {
        code = I->get_char_outline_metrics(I, &I->ff, &cr, &metrics);
    }
    else if (I->use_outline) {
        code = I->get_char_outline_metrics(I, &I->ff, &cr, &metrics);
    }
    else {
        code = I->get_char_raster_metrics(I, &I->ff, &cr, &metrics);
        /* A VMerror could be a real out of memory, or the glyph being too big for a bitmap
         * so it's worth retrying as an outline glyph
         */
        if (code == gs_error_VMerror) {
            I->use_outline = true;
            goto retry_oversampling;
        }
        if (code == gs_error_limitcheck) {
            if (log2_scale.x > 0 || log2_scale.y > 0) {
                penum_s->fapi_log2_scale.x = log2_scale.x =
                    penum_s->fapi_log2_scale.y = log2_scale.y = 0;
                I->release_char_data(I);
                goto retry_oversampling;
            }
            code = I->get_char_outline_metrics(I, &I->ff, &cr, &metrics);
        }
    }

    /* This handles the situation where a charstring has been replaced with a PS procedure.
     * against the rules, but not *that* rare.
     * It's also something that GS does internally to simulate font styles.
     */
    if (code > 0) {
        return (gs_error_unregistered);
    }

    if ((code = gs_fapi_renderer_retcode(mem, I, code)) < 0)
        return code;

    compute_em_scale(pbfont, &metrics, FontMatrix_div, &em_scale_x,
                     &em_scale_y);
    char_bbox.p.x = metrics.bbox_x0 / em_scale_x;
    char_bbox.p.y = metrics.bbox_y0 / em_scale_y;
    char_bbox.q.x = metrics.bbox_x1 / em_scale_x;
    char_bbox.q.y = metrics.bbox_y1 / em_scale_y;

    /* We must use the FontBBox, but it seems some buggy fonts have glyphs which extend outside the
     * FontBBox, so we have to do this....
     */
    if (pbfont->FontType != ft_MicroType && !bCID
        && pbfont->FontBBox.q.x > pbfont->FontBBox.p.x
        && pbfont->FontBBox.q.y > pbfont->FontBBox.p.y) {
        char_bbox.p.x = min(char_bbox.p.x, pbfont->FontBBox.p.x);
        char_bbox.p.y = min(char_bbox.p.y, pbfont->FontBBox.p.y);
        char_bbox.q.x = max(char_bbox.q.x, pbfont->FontBBox.q.x);
        char_bbox.q.y = max(char_bbox.q.y, pbfont->FontBBox.q.y);
    }

    if (pbfont->PaintType != 0) {
        float w = pbfont->StrokeWidth / 2;

        char_bbox.p.x -= w;
        char_bbox.p.y -= w;
        char_bbox.q.x += w;
        char_bbox.q.y += w;
    }
    penum_s->fapi_glyph_shift.x = penum_s->fapi_glyph_shift.y = 0;
    if (sbw_state == SBW_FROM_RENDERER) {
        int can_replace_metrics;

        if ((code =
             gs_fapi_renderer_retcode(mem, I,
                                      I->can_replace_metrics(I, &I->ff, &cr,
                                                             &can_replace_metrics)))
            < 0)
            return code;

        sbw[2] = metrics.escapement / em_scale_x;
        sbw[3] = metrics.v_escapement / em_scale_y;
        if (pbfont->FontType == 2 && !can_replace_metrics) {
            gs_font_type1 *pfont1 = (gs_font_type1 *) pbfont;

            sbw[2] += fixed2float(pfont1->data.nominalWidthX);
        }
    }
    else if (sbw_state == SBW_SCALE) {
        sbw[0] = (double)cr.sb_x / scale / em_scale_x;
        sbw[1] = (double)cr.sb_y / scale / em_scale_y;
        sbw[2] = (double)cr.aw_x / scale / em_scale_x;
        sbw[3] = (double)cr.aw_y / scale / em_scale_y;
    }

    /* Setup cache and render : */
    if (cr.metrics_type == gs_fapi_metrics_replace) {
        /*
         * Here we don't take care of replaced advance width
         * because gs_text_setcachedevice handles it.
         */
        int can_replace_metrics;

        if ((code =
             gs_fapi_renderer_retcode(mem, I,
                                      I->can_replace_metrics(I, &I->ff, &cr,
                                                             &can_replace_metrics)))
            < 0)
            return code;
        if (!can_replace_metrics) {
            /*
             * The renderer should replace the lsb, but it can't.
             * To work around we compute a displacement in integral pixels
             * and later shift the bitmap to it. The raster will be inprecise
             * with non-integral pixels shift.
             */
            char_bbox.q.x -= char_bbox.p.x;
            char_bbox.p.x = 0;
            gs_distance_transform((metrics.bbox_x0 / em_scale_x - sbw[0]),
                                  0, ctm, &penum_s->fapi_glyph_shift);
            penum_s->fapi_glyph_shift.x *= 1 << log2_scale.x;
            penum_s->fapi_glyph_shift.y *= 1 << log2_scale.y;
        }
    }

    /*
     * We assume that if bMetricsFromGlyphDirectory is true,
     * the font does not specify Metrics[2] and/or CDevProc
     * If someday we meet a font contradicting this assumption,
     * zchar_set_cache to be improved with additional flag,
     * to ignore Metrics[2] and CDevProc.
     *
     * Note that for best quality the result of CDevProc
     * to be passed to I->get_char_raster_metrics, because
     * both raster and metrics depend on replaced lsb.
     * Perhaps in many cases the metrics from font is
     * used as an argument for CDevProc. Only way to resolve
     * is to call I->get_char_raster_metrics twice (before
     * and after CDevProc), or better to split it into
     * smaller functions. Unfortunately UFST cannot retrieve metrics
     * quickly and separately from raster. Only way to resolve is
     * to devide the replaced lsb into 2 parts, which correspond to
     * integral and fractinal pixels, then pass the fractional shift
     * to renderer and apply the integer shift after it.
     *
     * Besides that, we are not sure what to do if a font
     * contains both Metrics[2] and CDevProc. Should
     * CDevProc to be applied to Metrics[2] or to the metrics
     * from glyph code ? Currently we keep a compatibility
     * to the native GS font renderer without a deep analyzis.
     */

    /* Don't allow caching if we're only returning the metrics */
    if (I->ff.metrics_only) {
        pgs->in_cachedevice = CACHE_DEVICE_NOT_CACHING;
    }

    if (pgs->in_cachedevice == CACHE_DEVICE_CACHING) {
        sbwp = sbw;
    }
    else {
        /* Very occasionally, if we don't do this, setcachedevice2
         * will decide we are cacheing, when we're not, and this
         * causes problems when we get to show_update().
         */
        sbwp = NULL;

        if (I->use_outline) {
            /* HACK!!
             * The decision about whether to cache has already been
             * we need to prevent it being made again....
             */
            pgs->in_cachedevice = CACHE_DEVICE_NOT_CACHING;
        }
    }

    if (bCID) {
        code =
            I->ff.fapi_set_cache(penum, pbfont, &enc_char_name_string, index + GS_MIN_CID_GLYPH,
                                 sbw + 2, &char_bbox, sbwp, &imagenow);
    }
    else {
        code =
            I->ff.fapi_set_cache(penum, pbfont, &enc_char_name_string, index,
                                 sbw + 2, &char_bbox, sbwp, &imagenow);
    }

    /* If we can render the glyph now, do so.
     * we may not be able to in the PS world if there's a CDevProc in the font
     * in which case gs_fapi_finish_render() will be called from the PS
     * "function" zfapi_finish_render() which has been pushed onto the
     * stack.
     */
    if (code >= 0 && imagenow == true) {
        code = gs_fapi_finish_render(pfont, pgs, penum, I);
        I->release_char_data(I);
    }

    if (code != 0) {
        if (code < 0) {
            /* An error */
            I->release_char_data(I);
        }
        else {
            /* Callout to CDevProc, zsetcachedevice2, zfapi_finish_render. */
        }
    }

    return code;
}

int
gs_fapi_get_font_info(gs_font *pfont, gs_fapi_font_info item, int index,
                      void *data, int *data_len)
{
    int code = 0;
    gs_font_base *pbfont = (gs_font_base *) pfont;
    gs_fapi_server *I = pbfont->FAPI;

    code = I->get_font_info(I, &I->ff, item, index, data, data_len);
    return (code);
}

/* On finding a suitable FAPI instance, fapi_id will be set to point to the string of the instance name.
 * A specific FAPI instance can be requested with the "fapi_request" parameter, but if the requested
 * isn't found, or rejects the font, we're re-search the available instances to find one that can handle
 * the font. If only an exact match is suitable, it is up to the *caller* to enforce that.
 */
int
gs_fapi_passfont(gs_font *pfont, int subfont, char *font_file_path,
                 gs_string *full_font_buf, char *fapi_request, char *xlatmap,
                 char **fapi_id, gs_fapi_get_server_param_callback get_server_param_cb)
{
    gs_font_base *pbfont;
    int code = 0;
    gs_fapi_server *I, **list;
    bool free_params = false;
    gs_memory_t *mem = pfont->memory;
    const char *decodingID = NULL;
    bool do_restart = false;

    list = gs_fapi_get_server_list(mem);

    if (!list) /* Should never happen */
      return_error(gs_error_unregistered);

    (*fapi_id) = NULL;

    pbfont = (gs_font_base *) pfont;

    I = *list;

    if (fapi_request) {
        if (gs_debug_c('1'))
            dprintf1("Requested FAPI plugin: %s ", fapi_request);

        while ((I = *list) != NULL
               && strncmp(I->ig.d->subtype, fapi_request,
                          strlen(fapi_request)) != 0) {
            list++;
        }
        if (!I) {
            if (gs_debug_c('1'))
                dprintf("not found. Falling back to normal plugin search\n");
            list = (gs_fapi_server **) gs_fapi_get_server_list(mem);
            I = *list;
        }
        else {
            if (gs_debug_c('1'))
                dprintf("found.\n");
            do_restart = true;
        }
    }

    while (I) {
        char *server_param = NULL;
        int server_param_size = 0;

        (*get_server_param_cb) (I, (const char *)I->ig.d->subtype,
                                &server_param, &server_param_size);

        if (server_param == NULL && server_param_size > 0) {
            server_param =
                (char *)gs_alloc_bytes_immovable(mem->non_gc_memory,
                                         server_param_size,
                                         "gs_fapi_passfont server params");
            if (!server_param) {
                return_error(gs_error_VMerror);
            }
            free_params = true;
            (*get_server_param_cb) (I, (const char *)I->ig.d->subtype,
                                    &server_param, &server_param_size);
        }

        if ((code =
             gs_fapi_renderer_retcode(mem, I,
                                      I->ensure_open(I, server_param,
                                                     server_param_size))) < 0) {
            gs_free_object(mem->non_gc_memory, server_param,
                           "gs_fapi_passfont server params");
            return code;
        }

        if (free_params) {
            gs_free_object(mem->non_gc_memory, server_param,
                           "gs_fapi_passfont server params");
        }

        pbfont->FAPI = I;       /* we need the FAPI server during this stage */
        code =
            gs_fapi_prepare_font(pfont, I, subfont, font_file_path,
                                 full_font_buf, xlatmap, &decodingID);
        if (code >= 0) {
            (*fapi_id) = (char *)I->ig.d->subtype;
            return 0;
        }

        /* renderer failed, continue search */
        pbfont->FAPI = NULL;
        if (do_restart == true) {
            if (gs_debug_c('1'))
                dprintf1
                    ("Requested FAPI plugin %s failed, searching for alternative plugin\n",
                     I->ig.d->subtype);
            list = (gs_fapi_server **) gs_fapi_get_server_list(mem);
            do_restart = false;
        }
        else {
            I = *list;
            list++;
        }
    }
    return (code);
}

bool
gs_fapi_available(gs_memory_t *mem, char *server)
{
    bool retval = false;

    if (server) {
        gs_fapi_server *serv = NULL;

        retval = (gs_fapi_find_server(mem, server, &serv, NULL) >= 0);
    }
    else {
        retval = ((mem->gs_lib_ctx->fapi_servers) != NULL) && (*(mem->gs_lib_ctx->fapi_servers) != NULL);
    }
    return (retval);
}

void
gs_fapi_set_servers_client_data(gs_memory_t *mem, const gs_fapi_font *ff_proto, void *ctx_ptr)
{
    gs_fapi_server **servs = gs_fapi_get_server_list(mem);

    if (servs) {
        while (*servs) {
            (*servs)->client_ctx_p = ctx_ptr;
            if (ff_proto)
                (*servs)->ff = *ff_proto;
            servs++;
        }
    }
}

gs_fapi_server **
gs_fapi_get_server_list(gs_memory_t *mem)
{
    return (mem->gs_lib_ctx->fapi_servers);
}

int
gs_fapi_find_server(gs_memory_t *mem, const char *name, gs_fapi_server **server,
                    gs_fapi_get_server_param_callback get_server_param_cb)
{
    gs_fapi_server **servs = gs_fapi_get_server_list(mem);
    char *server_param = NULL;
    int server_param_size = 0;
    int code = 0;
    bool free_params = false;

    (*server) = NULL;

    while (servs && *servs && strcmp((char *)(*servs)->ig.d->subtype, (char *)name)) {
        servs++;
    }

    if (servs && *servs && get_server_param_cb) {
        (*get_server_param_cb) ((*servs), (char *) (*servs)->ig.d->subtype,
                                &server_param, &server_param_size);

        if (server_param == NULL && server_param_size > 0) {
            server_param =
                (char *)gs_alloc_bytes_immovable(mem->non_gc_memory,
                                         server_param_size,
                                         "gs_fapi_find_server server params");
            if (!server_param) {
                return_error(gs_error_VMerror);
            }
            free_params = true;
            (*get_server_param_cb) ((*servs),
                                    (const char *)(*servs)->ig.d->subtype,
                                    &server_param, &server_param_size);
        }

        code = gs_fapi_renderer_retcode(mem, (*servs),
                                 (*servs)->ensure_open((*servs),
                                                       server_param,
                                                       server_param_size));

        if (free_params) {
            gs_free_object(mem->non_gc_memory, server_param,
                           "gs_fapi_find_server: server_param");
        }

        (*server) = (*servs);
    }
    else {
        if (!servs || !(*servs)) {
            code = gs_error_invalidaccess;
        }
    }


    return (code);
}

int
gs_fapi_init(gs_memory_t *mem)
{
    int code = 0;
    int i, num_servers = 0;
    gs_fapi_server **servs = NULL;
    const gs_fapi_server_init_func *gs_fapi_server_inits =
        gs_get_fapi_server_inits();

    while (gs_fapi_server_inits[num_servers]) {
        num_servers++;
    }

    servs =
        (gs_fapi_server **) gs_alloc_bytes_immovable(mem->non_gc_memory,
                                                     (num_servers +
                                                      1) *
                                                     sizeof(gs_fapi_server *),
                                                     "gs_fapi_init");
    if (!servs) {
        return_error(gs_error_VMerror);
    }

    for (i = 0; i < num_servers; i++) {
        gs_fapi_server_init_func *f =
            (gs_fapi_server_init_func *) & (gs_fapi_server_inits[i]);

        code = (*f) (mem, &(servs[i]));
        if (code != 0) {
            break;
        }
        /* No point setting this, as in PS, the interpreter context might move
         * But set it to NULL, just for safety */
        servs[i]->client_ctx_p = NULL;
    }

    for (; i < num_servers + 1; i++) {
        servs[i] = NULL;
    }

    mem->gs_lib_ctx->fapi_servers = servs;

    return (code);
}

void
gs_fapi_finit(gs_memory_t *mem)
{
    gs_fapi_server **servs = mem->gs_lib_ctx->fapi_servers;

    while (servs && *servs) {
        ((*servs)->ig.d->finit) (servs);
        servs++;
    }
    gs_free_object(mem->non_gc_memory, mem->gs_lib_ctx->fapi_servers,
                   "gs_fapi_finit: mem->gs_lib_ctx->fapi_servers");
    mem->gs_lib_ctx->fapi_servers = NULL;
}
