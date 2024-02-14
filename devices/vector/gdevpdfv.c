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


/* Color value writing for pdfwrite driver */
#include "math_.h"
#include "string_.h"
#include "gx.h"
#include "gscindex.h"
#include "gserrors.h"
#include "gsiparm3.h"		/* for pattern colors */
#include "gsmatrix.h"		/* for gspcolor.h */
#include "gscoord.h"		/* for gs_currentmatrix, requires gsmatrix.h */
#include "gsptype2.h"
#include "gxcolor2.h"		/* for gxpcolor.h */
#include "gxdcolor.h"		/* for gxpcolor.h */
#include "gxpcolor.h"		/* for pattern device color types */
#include "gxshade.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"

/* Import the PatternType 2 Pattern device color type. */
extern const gx_device_color_type_t gx_dc_pattern2;

/*
 * Define the scaling and range of values written for mesh shadings.
 * BitsPerCoordinate is always 24; BitsPerComponent (for colors) is
 * always 16.
 */
#define ENCODE_VALUE(v, emax, vmin, vmax)\
  ( ((v) - (vmin)) * ((double)(emax) / ((vmax) - (vmin))) )
#define PDFA_MIN_MESH_COORDINATE (-0x800000 / 256.0)
#define PDFA_MAX_MESH_COORDINATE ( 0x7fffff / 256.0)
#define PDFA_ENCODE_MESH_COORDINATE(v)\
  ENCODE_VALUE(v, 0xffffff, PDFA_MIN_MESH_COORDINATE, PDFA_MAX_MESH_COORDINATE)
#define MIN_MESH_COORDINATE (-0x800000 )
#define MAX_MESH_COORDINATE ( 0x7fffff )
#define ENCODE_MESH_COORDINATE(v)\
  ENCODE_VALUE(v, 0xffffff, MIN_MESH_COORDINATE, MAX_MESH_COORDINATE)

#define MIN_MESH_COLOR_INDEX 0
#define MAX_MESH_COLOR_INDEX 0xffff
#define ENCODE_MESH_COLOR_INDEX(v) ((v) + MIN_MESH_COLOR_INDEX)

#define ENCODE_MESH_COMPONENT(v, vmin, vmax)\
  ENCODE_VALUE(v, 0xffff, vmin, vmax)

/* ---------------- Utilities ---------------- */

/* Write a matrix parameter. */
static int
cos_dict_put_matrix(gx_device_pdf *pdev, cos_dict_t *pscd, const char *key, const gs_matrix *pmat)
{
    float matrix[6];

    matrix[0] = pmat->xx;
    matrix[1] = pmat->xy;
    matrix[2] = pmat->yx;
    matrix[3] = pmat->yy;
    matrix[4] = pmat->tx;
    matrix[5] = pmat->ty;
    return cos_dict_put_c_key_floats(pdev, pscd, key, matrix, 6);
}

/* ---------------- PatternType 1 colors ---------------- */

/*
 * Create a Pattern resource referencing an image (currently only an XObject).
 * p_tile is NULL for uncolored patterns or the NULL pattern.
 * m_tile is NULL for colored patterns that fill their bounding box,
 * including the NULL pattern.
 ****** WE DON'T HANDLE NULL PATTERNS YET ******
 */
static uint
tile_size(const gx_strip_bitmap *tile, int depth)
{
    return (tile->rep_width * depth + 7) / 8 * tile->rep_height;
}
static bool
tile_size_ok(const gx_device_pdf *pdev, const gx_color_tile *p_tile,
             const gx_color_tile *m_tile)
{
    /*
     * Acrobat Reader can't handle image Patterns with more than
     * 64K of data.  :-(
     */
    uint p_size =
        (p_tile == 0 ? 0 : tile_size(&p_tile->tbits, p_tile->depth));
    uint m_size =
        (m_tile == 0 ? 0 : tile_size(&m_tile->tmask, 1));
    /* The image limit only applies to Acrobat versions less than 5
     * (PDF 1.4).
     */
    if (pdev->CompatibilityLevel < 1.4)
        return (max(p_size, m_size) <= 65500);
    else
        return 1;
}

static int
pdf_pattern(gx_device_pdf *pdev, const gx_drawing_color *pdc,
            const gx_color_tile *p_tile, const gx_color_tile *m_tile,
            cos_stream_t *pcs_image, pdf_resource_t **ppres)
{
    pdf_resource_t *pres;
    int code = pdf_alloc_resource(pdev, resourcePattern, pdc->mask.id, ppres,
                                  0L);
    cos_stream_t *pcos;
    cos_dict_t *pcd;
    cos_dict_t *pcd_Resources = cos_dict_alloc(pdev, "pdf_pattern(Resources)");
    const gx_color_tile *tile = (p_tile ? p_tile : m_tile);
    const gx_strip_bitmap *btile = (p_tile ? &p_tile->tbits : &m_tile->tmask);
    bool mask = p_tile == 0;
    gs_point step;
    gs_matrix smat;

    if (code < 0)
        return code;
    if (!tile_size_ok(pdev, p_tile, m_tile))
        return_error(gs_error_limitcheck);
    /*
     * We currently can't handle Patterns whose X/Y step isn't parallel
     * to the coordinate axes.
     */
    if (is_xxyy(&tile->step_matrix))
        step.x = tile->step_matrix.xx, step.y = tile->step_matrix.yy;
    else if (is_xyyx(&tile->step_matrix))
        step.x = tile->step_matrix.yx, step.y = tile->step_matrix.xy;
    else
        return_error(gs_error_rangecheck);
    if (pcd_Resources == 0)
        return_error(gs_error_VMerror);
    gs_make_identity(&smat);
    smat.xx = btile->rep_width / (pdev->HWResolution[0] / 72.0);
    smat.yy = btile->rep_height / (pdev->HWResolution[1] / 72.0);
    smat.tx = tile->step_matrix.tx / (pdev->HWResolution[0] / 72.0);
    smat.ty = tile->step_matrix.ty / (pdev->HWResolution[1] / 72.0);
    pres = *ppres;
    {
        cos_dict_t *pcd_XObject = cos_dict_alloc(pdev, "pdf_pattern(XObject)");
        char key[MAX_REF_CHARS + 3];
        cos_value_t v;
        cos_object_t *object;

        if (pcd_XObject == 0)
            return_error(gs_error_VMerror);
        gs_snprintf(key, sizeof(key), "/R%ld", pcs_image->id);
        /* This is non-obvious code. Previously we would put the image object (pcs_image)
         * into the Resources dit. When we come to write out the Resources dict
         * that code writes a reference (index 0 R) using the ID from the object.
         * However that means we have two pointers to the XObject. One in the chain
         * of resoruces (which we need in order to write teh XObject) and one from
         * the pattern here. That seriously messes up memory handling. So instead
         * we now make a new object, and copy the id from the pcs_image. Since that's
         * all that the writing code will use, we cna avoid the double pointers.
         */
        object = cos_reference_alloc(pdev, "pdf_pattern(reference copy of XObject)");
        object->id = pcs_image->id;
        COS_OBJECT_VALUE(&v, object);
        if ((code = cos_dict_put(pcd_XObject, (byte *)key, strlen(key), &v)) < 0 ||
            (code = cos_dict_put_c_key_object(pcd_Resources, "/XObject",
                                              COS_OBJECT(pcd_XObject))) < 0
            )
            return code;
    }
    if (pdev->CompatibilityLevel <= 1.7) {
        if ((code = cos_dict_put_c_strings(pcd_Resources, "/ProcSet",
                                       (mask ? "[/PDF/ImageB]" :
                                        "[/PDF/ImageC]"))) < 0)
            return code;
    }
    cos_become(pres->object, cos_type_stream);
    pcos = (cos_stream_t *)pres->object;
    pcd = cos_stream_dict(pcos);
    if ((code = cos_dict_put_c_key_int(pcd, "/PatternType", 1)) < 0 ||
        (code = cos_dict_put_c_key_int(pcd, "/PaintType",
                                       (mask ? 2 : 1))) < 0 ||
        (code = cos_dict_put_c_key_int(pcd, "/TilingType",
                                       tile->tiling_type)) < 0 ||
        (code = cos_dict_put_c_key_object(pcd, "/Resources",
                                          COS_OBJECT(pcd_Resources))) < 0 ||
        (code = cos_dict_put_c_strings(pcd, "/BBox", "[0 0 1 1]")) < 0 ||
        (code = cos_dict_put_matrix(pdev, pcd, "/Matrix", &smat)) < 0 ||
        (code = cos_dict_put_c_key_real(pcd, "/XStep", step.x / btile->rep_width)) < 0 ||
        (code = cos_dict_put_c_key_real(pcd, "/YStep", step.y / btile->rep_height)) < 0
        ) {
        return code;
    }

    {
        char buf[MAX_REF_CHARS + 6 + 1]; /* +6 for /R# Do\n */

        gs_snprintf(buf, sizeof(buf), "/R%ld Do\n", pcs_image->id);
        cos_stream_add_bytes(pdev, pcos, (const byte *)buf, strlen(buf));
    }

    return 0;
}

/* Store pattern 1 parameters to cos dictionary. */
int
pdf_store_pattern1_params(gx_device_pdf *pdev, pdf_resource_t *pres,
                        gs_pattern1_instance_t *pinst)
{
    gs_pattern1_template_t *t = &pinst->templat;
    gs_matrix smat2 = ctm_only((gs_gstate *)pinst->saved), smat;
    double scale_x = pdev->HWResolution[0] / 72.0;
    double scale_y = pdev->HWResolution[1] / 72.0;
    cos_dict_t *pcd = cos_stream_dict((cos_stream_t *)pres->object);
    cos_dict_t *pcd_Resources = cos_dict_alloc(pdev, "pdf_pattern(Resources)");
    float bbox[4];
    int code;

    if (pcd == NULL || pcd_Resources == NULL)
        return_error(gs_error_VMerror);
    pdev->substream_Resources = pcd_Resources;
    bbox[0] = t->BBox.p.x;
    bbox[1] = t->BBox.p.y;
    bbox[2] = t->BBox.q.x;
    bbox[3] = t->BBox.q.y;
    if (pdev->accumulating_charproc) {
        /* Assume here we can only be installing a pattern while acumulating a
         * charproc if the font is a coloured type 3 font. In this case we will
         * have set the CTM to be the identity scaled by 100 (!). See gdevpdtt.c
         * install_PS_charproc_accumulator() for details.
         */
        gs_make_identity(&smat2);
    }
    /* The graphics library assumes a shifted origin to provide
       positive bitmap pixel indices. Compensate it now. */
    smat2.tx += pinst->step_matrix.tx;
    smat2.ty += pinst->step_matrix.ty;
    /*
     * In PDF, the Matrix is the transformation from the pattern space to
     * the *default* user coordinate space, not the current space.
     * NB. For a form the default space is the parent. This means that when a
     * form is nested inside a form, the default space is the space of the
     * first form, and therefore we do *not* remove the resolution scaling.
     */
    if ((pdev->FormDepth == 0 || (pdev->FormDepth > 0 && pdev->PatternsSinceForm > 0)) && !pdev->accumulating_charproc) {
        gs_matrix scaled;

        gs_make_scaling(1 / scale_x, 1 / scale_y, &scaled);
        gs_matrix_multiply(&smat2, &scaled, &smat);
    } else {
        smat = smat2;
    }
    if ((smat.xx == 0.0 && smat.yy == 0.0) && (smat.xy == 0.0 && smat.yx == 0.0))
        return_error(gs_error_undefinedresult);

    if (pdev->ForOPDFRead) {
        if (pdev->PatternDepth) {
            gs_matrix_multiply(&smat, &pdev->AccumulatedPatternMatrix, &smat2);
            gs_matrix_multiply(&pdev->AccumulatedPatternMatrix, &smat, &pdev->AccumulatedPatternMatrix);
            smat = smat2;
        } else {
            gs_make_identity(&pdev->AccumulatedPatternMatrix);
            gs_matrix_multiply(&pdev->AccumulatedPatternMatrix, &smat, &pdev->AccumulatedPatternMatrix);
        }
    }
    if (any_abs(smat.tx) < 0.0001)  /* Noise. */
        smat.tx = 0;
    if (any_abs(smat.ty) < 0.0001)
        smat.ty = 0;
    code = cos_dict_put_c_strings(pcd, "/Type", "/Pattern");
    if (code >= 0)
        code = cos_dict_put_c_key_int(pcd, "/PatternType", 1);
    if (code >= 0)
        code = cos_dict_put_c_key_int(pcd, "/PaintType", t->PaintType);
    if (code >= 0)
        code = cos_dict_put_c_key_int(pcd, "/TilingType", t->TilingType);
    if (code >= 0)
        code = cos_dict_put_c_key_floats(pdev, pcd, "/BBox", bbox, 4);
    if (code >= 0)
        code = cos_dict_put_matrix(pdev, pcd, "/Matrix", &smat);
    if (code >= 0)
        code = cos_dict_put_c_key_real(pcd, "/XStep", t->XStep);
    if (code >= 0)
        code = cos_dict_put_c_key_real(pcd, "/YStep", t->YStep);
    if (code >= 0)
        code = cos_dict_put_c_key_object(pcd, "/Resources", COS_OBJECT(pcd_Resources));
    pdev->skip_colors = (t->PaintType == 2);
    return code;
}

/* Set the ImageMatrix, Width, and Height for a Pattern image. */
static void
pdf_set_pattern_image(gs_data_image_t *pic, const gx_strip_bitmap *tile)
{
    pic->ImageMatrix.xx = (float)(pic->Width = tile->rep_width);
    pic->ImageMatrix.yy = (float)(pic->Height = tile->rep_height);
}

/* Write the mask for a Pattern (colored or uncolored). */
static int
pdf_put_pattern_mask(gx_device_pdf *pdev, const gx_color_tile *m_tile,
                     cos_stream_t **ppcs_mask)
{
    int w = m_tile->tmask.rep_width, h = m_tile->tmask.rep_height;
    gs_image1_t image;
    pdf_image_writer writer;
    int code;

    gs_image_t_init_mask_adjust(&image, true, false);
    pdf_set_pattern_image((gs_data_image_t *)&image, &m_tile->tmask);
    pdf_image_writer_init(&writer);
    if ((code = pdf_begin_write_image(pdev, &writer, gs_no_id, w, h, NULL, false)) < 0 ||
        (pdev->params.MonoImage.Encode &&
         (code = psdf_CFE_binary(&writer.binary[0], w, h, true)) < 0) ||
        (code = pdf_begin_image_data(pdev, &writer, (const gs_pixel_image_t *)&image, NULL, 0)) < 0
        )
        return code;
    /* Pattern masks are specified in device coordinates, so invert Y. */
    if ((code = pdf_copy_mask_bits(writer.binary[0].strm, m_tile->tmask.data + (h - 1) * m_tile->tmask.raster, 0, -m_tile->tmask.raster, w, h, 0)) < 0 ||
        (code = pdf_end_image_binary(pdev, &writer, h)) < 0 ||
        (code = pdf_end_write_image(pdev, &writer)) < 0
        )
        return code;
    *ppcs_mask = (cos_stream_t *)writer.pres->object;
    return 0;
}

/* Write an uncolored Pattern color. */
int
pdf_put_uncolored_pattern(gx_device_pdf *pdev, const gx_drawing_color *pdc,
                          const gs_color_space *pcs,
                          const psdf_set_color_commands_t *ppscc,
                          const gs_gstate * pgs, pdf_resource_t **ppres)
{
    const gx_color_tile *m_tile = pdc->mask.m_tile;
    gx_drawing_color dc_pure;

    if (!pgs->have_pattern_streams && m_tile == 0) {
        /*
         * If m_tile == 0, this uncolored Pattern is all 1's,
         * equivalent to a pure color.
         */
        *ppres = 0;
        set_nonclient_dev_color(&dc_pure, gx_dc_pure_color(pdc));
        return psdf_set_color((gx_device_vector *)pdev, &dc_pure, ppscc);
    } else {
        cos_value_t v;
        stream *s = pdev->strm;
        int code;
        cos_stream_t *pcs_image;
        static const psdf_set_color_commands_t no_scc = {0, 0, 0};

        if (!tile_size_ok(pdev, NULL, m_tile))
            return_error(gs_error_limitcheck);
        if (!pgs->have_pattern_streams) {
            if ((code = pdf_cs_Pattern_uncolored(pdev, &v)) < 0 ||
                (code = pdf_put_pattern_mask(pdev, m_tile, &pcs_image)) < 0 ||
                (code = pdf_pattern(pdev, pdc, NULL, m_tile, pcs_image, ppres)) < 0
                )
                return code;
        } else {
            code = pdf_cs_Pattern_uncolored_hl(pdev, pcs, &v, pgs);
            if (code < 0)
                return code;
            *ppres = pdf_find_resource_by_gs_id(pdev, resourcePattern, pdc->mask.id);
            if (*ppres == NULL)
                return_error(gs_error_undefined);

            *ppres = pdf_substitute_pattern(*ppres);
            if (!pdev->AR4_save_bug && pdev->CompatibilityLevel <= 1.3) {
                /* We reconnized AR4 behavior as reserving "q Q" stack elements
                 * on demand. It looks as processing a pattern stream
                 * with PaintType 1 AR4 replaces the topmost stack element
                 * instead allocating a new one, if it was not previousely allocated.
                 * AR 5 doesn't have this bug. Working around the AR4 bug here.
                 */
                stream_puts(pdev->strm, "q q Q Q\n");
                pdev->AR4_save_bug = true;
            }
            (*ppres)->where_used |= pdev->used_mask;
        }
        cos_value_write(&v, pdev);
        pprints1(s, " %s ", ppscc->setcolorspace);
        if (pgs->have_pattern_streams)
            return 0;
        set_nonclient_dev_color(&dc_pure, gx_dc_pure_color(pdc));
        return psdf_set_color((gx_device_vector *)pdev, &dc_pure, &no_scc);
    }
}

int
pdf_put_colored_pattern(gx_device_pdf *pdev, const gx_drawing_color *pdc,
                        const gs_color_space *pcs,
                        const psdf_set_color_commands_t *ppscc,
                        const gs_gstate * pgs, pdf_resource_t **ppres)
{
    const gx_color_tile *p_tile = pdc->colors.pattern.p_tile;
    gs_color_space *pcs_Device;
    cos_value_t cs_value;
    cos_value_t v;
    int code;
    gs_image1_t image;
    const gx_color_tile *m_tile = NULL;
    pdf_image_writer writer;
    int w = 0, h = 0;

    if (p_tile) {
        w = p_tile->tbits.rep_width;
        h = p_tile->tbits.rep_height;
    }

    if (!pgs->have_pattern_streams) {
        /*
         * NOTE: We assume here that the color space of the cached Pattern
         * is the same as the native color space of the device.  This will
         * have to change in the future!
         */
        /*
         * Check whether this colored pattern is actually a masked pure color,
         * by testing whether all the colored pixels have the same color.
         */
        m_tile = pdc->mask.m_tile;
        if (m_tile) {
            if (p_tile && !(p_tile->depth & 7) && p_tile->depth <= ARCH_SIZEOF_COLOR_INDEX * 8) {
                int depth_bytes = p_tile->depth >> 3;
                int width = p_tile->tbits.rep_width;
                int skip = p_tile->tbits.raster -
                    p_tile->tbits.rep_width * depth_bytes;
                const byte *bp;
                const byte *mp;
                int i, j, k;
                gx_color_index color = 0; /* init is arbitrary if not empty */
                bool first = true;

                for (i = 0, bp = p_tile->tbits.data, mp = p_tile->tmask.data;
                     i < p_tile->tbits.rep_height;
                     ++i, bp += skip, mp += p_tile->tmask.raster) {

                    for (j = 0; j < width; ++j) {
                        if (mp[j >> 3] & (0x80 >> (j & 7))) {
                            gx_color_index ci = 0;

                            for (k = 0; k < depth_bytes; ++k)
                                ci = (ci << 8) + *bp++;
                            if (first)
                                color = ci, first = false;
                            else if (ci != color)
                                goto not_pure;
                        } else
                            bp += depth_bytes;
                    }
                }
                {
                    /* Set the color, then handle as an uncolored pattern. */
                    gx_drawing_color dcolor;

                    dcolor = *pdc;
                    dcolor.colors.pure = color;
                    return pdf_put_uncolored_pattern(pdev, &dcolor, pcs, ppscc,
                                pgs, ppres);
                }
            not_pure:
                DO_NOTHING;		/* required by MSVC */
            }
            if (pdev->CompatibilityLevel < 1.3) {
                /* Masked images are only supported starting in PDF 1.3. */
                return_error(gs_error_rangecheck);
            }
        }
        /* Acrobat Reader has a size limit for image Patterns. */
        if (!tile_size_ok(pdev, p_tile, m_tile))
            return_error(gs_error_limitcheck);
    }
    code = pdf_cs_Pattern_colored(pdev, &v);
    if (code < 0)
        return code;
    pdf_cspace_init_Device(pdev->memory, &pcs_Device, pdev->color_info.num_components);
    /*
     * We don't have to worry about color space scaling: the color
     * space is always a Device space.
     */
    code = pdf_color_space_named(pdev, NULL, &cs_value, NULL, pcs_Device,
                           &pdf_color_space_names, true, NULL, 0, false);
    if (code < 0)
        return code;
    if (!pgs->have_pattern_streams) {
        cos_stream_t *pcs_mask = 0;
        cos_stream_t *pcs_image;

        gs_image_t_init_adjust(&image, pcs_Device, false);
        image.BitsPerComponent = 8;
        if (!p_tile)
            return_error(gs_error_unknownerror);

        pdf_set_pattern_image((gs_data_image_t *)&image, &p_tile->tbits);
        if (m_tile) {
            if ((code = pdf_put_pattern_mask(pdev, m_tile, &pcs_mask)) < 0)
                return code;
        }
        pdf_image_writer_init(&writer);
        pdev->ParamCompatibilityLevel = pdev->CompatibilityLevel;
        if ((code = pdf_begin_write_image(pdev, &writer, gs_no_id, w, h, NULL, false)) < 0 ||
            (code = psdf_setup_lossless_filters((gx_device_psdf *)pdev,
                                                &writer.binary[0],
                                                (gs_pixel_image_t *)&image, false)) < 0 ||
            (code = pdf_begin_image_data(pdev, &writer, (const gs_pixel_image_t *)&image, &cs_value, 0)) < 0
            )
            return code;
        /* Pattern masks are specified in device coordinates, so invert Y. */
        if ((code = pdf_copy_color_bits(writer.binary[0].strm, p_tile->tbits.data + (h - 1) * p_tile->tbits.raster, 0, -p_tile->tbits.raster, w, h, pdev->color_info.depth >> 3)) < 0 ||
            (code = pdf_end_image_binary(pdev, &writer, h)) < 0
            )
            return code;
        pcs_image = (cos_stream_t *)writer.pres->object;
        if ((pcs_mask != 0 &&
             (code = cos_dict_put_c_key_object(cos_stream_dict(pcs_image), "/Mask",
                                               COS_OBJECT(pcs_mask))) < 0) ||
            (code = pdf_end_write_image(pdev, &writer)) < 0
            )
            return code;
        pcs_image = (cos_stream_t *)writer.pres->object; /* pdf_end_write_image may change it. */
        code = pdf_pattern(pdev, pdc, p_tile, m_tile, pcs_image, ppres);
        if (code < 0)
            return code;
    } else {
        if (!p_tile)
            return_error(gs_error_unknownerror);
        *ppres = pdf_find_resource_by_gs_id(pdev, resourcePattern, p_tile->id);
        *ppres = pdf_substitute_pattern(*ppres);
        (*ppres)->where_used |= pdev->used_mask;
    }
    /* pcs_Device will leak (picked up by GC in PS) on error, but we'll
       tolerate that for now. */
    rc_decrement_cs(pcs_Device, "pdf_put_colored_pattern");
    cos_value_write(&v, pdev);
    pprints1(pdev->strm, " %s", ppscc->setcolorspace);
    return 0;
}

/* ---------------- PatternType 2 colors ---------------- */

/* Write parameters common to all Shadings. */
static int
pdf_put_shading_common(gx_device_pdf *pdev, cos_dict_t *pscd, const gs_gstate * pgs, const gs_shading_t *psh,
                       bool shfill, const gs_range_t **ppranges)
{
    gs_shading_type_t type = ShadingType(psh);
    const gs_color_space *pcs = psh->params.ColorSpace;
    int code = cos_dict_put_c_key_int(pscd, "/ShadingType", (int)type);
    cos_value_t cs_value;

    if (code < 0 ||
        (psh->params.AntiAlias &&
         (code = cos_dict_put_c_strings(pscd, "/AntiAlias", "true")) < 0) ||
        (code = pdf_color_space_named(pdev, pgs, &cs_value, ppranges, pcs,
                                &pdf_color_space_names, false, NULL, 0, false)) < 0 ||
        (code = cos_dict_put_c_key(pscd, "/ColorSpace", &cs_value)) < 0
        )
        return code;
    if (psh->params.Background && !shfill) {
        /****** SCALE Background ******/
        code = cos_dict_put_c_key_floats(pdev, pscd, "/Background",
                                   psh->params.Background->paint.values,
                                   gs_color_space_num_components(pcs));
        if (code < 0)
            return code;
    }
    if (psh->params.have_BBox) {
        float bbox[4];

        bbox[0] = psh->params.BBox.p.x;
        bbox[1] = psh->params.BBox.p.y;
        bbox[2] = psh->params.BBox.q.x;
        bbox[3] = psh->params.BBox.q.y;
        code = cos_dict_put_c_key_floats(pdev, pscd, "/BBox", bbox, 4);
        if (code < 0)
            return code;
    }
    return 0;
}

/* Write an optional Function parameter. */
static int
pdf_put_shading_Function(gx_device_pdf *pdev, cos_dict_t *pscd, const gs_function_t *pfn,
                         const gs_range_t *pranges)
{
    int code = 0;

    if (pfn != 0) {
        cos_value_t fn_value;

        if ((code = pdf_function_scaled(pdev, pfn, pranges, &fn_value)) >= 0)
            code = cos_dict_put_c_key(pscd, "/Function", &fn_value);
    }
    return code;
}

/* Write a linear (Axial / Radial) Shading. */
static int
pdf_put_linear_shading(gx_device_pdf *pdev, cos_dict_t *pscd, const float *Coords,
                       int num_coords, const float *Domain /*[2]*/,
                       const gs_function_t *Function,
                       const bool *Extend /*[2]*/,
                       const gs_range_t *pranges)
{
    int code = cos_dict_put_c_key_floats(pdev, pscd, "/Coords", Coords, num_coords);

    if (code < 0 ||
        ((Domain[0] != 0 || Domain[1] != 1) &&
         (code = cos_dict_put_c_key_floats(pdev, pscd, "/Domain", Domain, 2)) < 0) ||
        (code = pdf_put_shading_Function(pdev, pscd, Function, pranges)) < 0
        )
        return code;
    if (Extend[0] | Extend[1]) {
        char extend_str[1 + 5 + 1 + 5 + 1 + 1]; /* [bool bool] */

        gs_snprintf(extend_str, sizeof(extend_str), "[%s %s]",
                (Extend[0] ? "true" : "false"),
                (Extend[1] ? "true" : "false"));
        code = cos_dict_put_c_key_string(pscd, "/Extend",
                                         (const byte *)extend_str,
                                         strlen(extend_str));
    }
    return code;
}

/* Write a scalar (non-mesh) Shading. */
/* (Single-use procedure for readability.) */
static int
pdf_put_scalar_shading(gx_device_pdf *pdev, cos_dict_t *pscd, const gs_shading_t *psh,
                       const gs_range_t *pranges)
{
    int code;

    switch (ShadingType(psh)) {
    case shading_type_Function_based: {
        const gs_shading_Fb_params_t *const params =
            (const gs_shading_Fb_params_t *)&psh->params;

        if ((code = cos_dict_put_c_key_floats(pdev, pscd, "/Domain", params->Domain, 4)) < 0 ||
            (code = pdf_put_shading_Function(pdev, pscd, params->Function, pranges)) < 0 ||
            (code = cos_dict_put_matrix(pdev, pscd, "/Matrix", &params->Matrix)) < 0
            )
            return code;
        return 0;
    }
    case shading_type_Axial: {
        const gs_shading_A_params_t *const params =
            (const gs_shading_A_params_t *)&psh->params;

        return pdf_put_linear_shading(pdev, pscd, params->Coords, 4,
                                      params->Domain, params->Function,
                                      params->Extend, pranges);
    }
    case shading_type_Radial: {
        const gs_shading_R_params_t *const params =
            (const gs_shading_R_params_t *)&psh->params;

        return pdf_put_linear_shading(pdev, pscd, params->Coords, 6,
                                      params->Domain, params->Function,
                                      params->Extend, pranges);
    }
    default:
        return_error(gs_error_rangecheck);
    }
}

/* Add a floating point range to an array. */
static int
pdf_array_add_real2(cos_array_t *pca, double lower, double upper)
{
    int code = cos_array_add_real(pca, lower);

    if (code >= 0)
        code = cos_array_add_real(pca, upper);
    return code;
}

/* Define a parameter structure for mesh data. */
typedef struct pdf_mesh_data_params_s {
    int num_points;
    int num_components;
    bool is_indexed;
    int rescale;            /* If the co-ordinates won't fit into crappy Acrobat values, scale them here and in the pattern Matrix */
    const float *Domain;	/* iff Function */
    const gs_range_t *ranges;
} pdf_mesh_data_params_t;

/* Put a clamped value into a data stream.  num_bytes < sizeof(int). */
static void
put_clamped(byte *p, double v, int num_bytes)
{
    int limit = 1 << (num_bytes * 8);
    int i, shift;

    if (v <= -limit)
        i = -limit + 1;
    else if (v >= limit)
        i = limit - 1;
    else
        i = (int)v;
    for (shift = (num_bytes - 1) * 8; shift >= 0; shift -= 8)
        *p++ = (byte)(i >> shift);
}
static inline void
put_clamped_coord(byte *p, double v, int num_bytes, const pdf_mesh_data_params_t *pmdp)
{
    if (pmdp->rescale != 1.0) {
        v = v / pmdp->rescale;
        put_clamped(p, PDFA_ENCODE_MESH_COORDINATE(v), num_bytes);
    } else
        put_clamped(p, ENCODE_MESH_COORDINATE(v), num_bytes);
}

/* Convert floating-point mesh data to packed binary. */
/* BitsPerFlag = 8, BitsPerCoordinate = 24, BitsPerComponent = 16, */
/* scaling is as defined below. */
static int
put_float_mesh_data(gx_device_pdf *pdev, cos_stream_t *pscs, shade_coord_stream_t *cs,
                    int flag, int num_comps, const pdf_mesh_data_params_t *pmdp)
{
    int num_points = pmdp->num_points;
    byte b[1 + (3 + 3) * 16];	/* flag + x + y or c */
    gs_fixed_point pts[16];
    const float *domain = pmdp->Domain;
    const gs_range_t *pranges = pmdp->ranges;
    int i, code;

    b[0] = (byte)flag;		/* may be -1 */
    if ((code = shade_next_coords(cs, pts, num_points)) < 0)
        return code;
    for (i = 0; i < num_points; ++i) {
        put_clamped_coord(b + 1 + i * 6, fixed2float(pts[i].x), 3, pmdp);
        put_clamped_coord(b + 4 + i * 6, fixed2float(pts[i].y), 3, pmdp);
    }
    if ((code = cos_stream_add_bytes(pdev, pscs, b + (flag < 0),
                                     (flag >= 0) + num_points * 6)) < 0)
        return code;
    for (i = 0; i < pmdp->num_components; ++i) {
        float c = 0;
        double v;

        code = cs->get_decoded(cs, 0, NULL, &c);
        if (code < 0)
            return code;

        if (pmdp->is_indexed)
            v = ENCODE_MESH_COLOR_INDEX(c);
        else {
            /*
             * We don't rescale stream data values, only the Decode ranges.
             * (We do have to rescale data values from an array, unless
             * they are the input parameter for a Function.)
             * This makes everything come out as it should.
             */
            double vmin, vmax;

            if (domain)
                vmin = domain[2 * i], vmax = domain[2 * i + 1];
            else
                vmin = 0.0, vmax = 1.0;
            if (pranges) {
                double base = pranges[i % num_comps].rmin, factor = pranges[i % num_comps].rmax - base;

                vmin = vmin * factor + base;
                vmax = vmax * factor + base;
            }
            v = ENCODE_MESH_COMPONENT(c, vmin, vmax);
        }
        put_clamped(b, v, 2);
        if ((code = cos_stream_add_bytes(pdev, pscs, b, 2)) < 0)
            return code;
    }
    return 0;
}

/* Write a mesh Shading. */
static int
pdf_put_mesh_shading(gx_device_pdf *pdev, cos_stream_t *pscs, const gs_shading_t *psh,
                     const gs_range_t *pranges, int *rescale)
{
    cos_dict_t *const pscd = cos_stream_dict(pscs);
    gs_color_space *pcs = psh->params.ColorSpace;
    const gs_shading_mesh_params_t *const pmp =
        (const gs_shading_mesh_params_t *)&psh->params;
    int code, code1;
    int bits_per_coordinate, bits_per_component, bits_per_flag;
    int num_comp;
    bool from_array = data_source_is_array(pmp->DataSource);
    pdf_mesh_data_params_t data_params;
    shade_coord_stream_t cs;
    gs_matrix_fixed ctm_ident;
    int flag;

    if (pmp->Function) {
        data_params.Domain = 0;
        num_comp = 1;
    } else {
        data_params.Domain = (pmp->Decode != 0 ? pmp->Decode + 4 : NULL);
        num_comp = gs_color_space_num_components(pcs);
    }
    data_params.ranges = pranges;
    data_params.rescale = 1;


    /* Write parameters common to all mesh Shadings. */
    shade_next_init(&cs, pmp, NULL);
    if (from_array) {
        cos_array_t *pca = cos_array_alloc(pdev, "pdf_put_mesh_shading");
        int i;

        if (pca == 0)
            return_error(gs_error_VMerror);
        for (i = 0; i < 2; ++i) {
            if (pdev->CompatibilityLevel < 1.5) {
                if ((code = pdf_array_add_real2(pca, PDFA_MIN_MESH_COORDINATE,
                                            PDFA_MAX_MESH_COORDINATE)) < 0)
                    return code;
            }
            else {
                if ((code = pdf_array_add_real2(pca, MIN_MESH_COORDINATE,
                                            MAX_MESH_COORDINATE)) < 0)
                    return code;
            }
        }
        data_params.is_indexed = false;
        if (gs_color_space_get_index(pcs) == gs_color_space_index_Indexed) {
            data_params.is_indexed = true;
            if ((code = pdf_array_add_real2(pca, MIN_MESH_COLOR_INDEX,
                                            MAX_MESH_COLOR_INDEX)) < 0)
                return code;
        } else {
            for (i = 0; i < num_comp; ++i) {
                double rmin, rmax;

                if (pmp->Function || pranges || data_params.Domain == 0) {
                    if (pmp->Function && pmp->Function->params.Domain != 0) {
                        rmin = pmp->Function->params.Domain[0], rmax = pmp->Function->params.Domain[1];
                    } else {
                        rmin = 0.0, rmax = 1.0;
                    }
                }
                else
                    rmin = data_params.Domain[2 * i],
                        rmax = data_params.Domain[2 * i + 1];
                if ((code =
                     pdf_array_add_real2(pca, rmin, rmax)) < 0)
                    return code;
            }
        }
        code = cos_dict_put_c_key_object(pscd, "/Decode", COS_OBJECT(pca));
        if (code < 0)
            return code;
        bits_per_coordinate = 24;
        bits_per_component = 16;
        bits_per_flag = 8;
        gs_make_identity((gs_matrix *)&ctm_ident);
        ctm_ident.tx_fixed = ctm_ident.ty_fixed = 0;
        ctm_ident.txy_fixed_valid = true;
        cs.pctm = &ctm_ident;
        if (pmp->Function)
            data_params.ranges = 0; /* don't scale function parameter */
    } else {
        /****** SCALE Decode ******/
        code = cos_dict_put_c_key_floats(pdev, pscd, "/Decode", pmp->Decode,
                                         4 + num_comp * 2);
        if (code >= 0)
            code = cos_stream_add_stream_contents(pdev, pscs, cs.s);
        bits_per_coordinate = pmp->BitsPerCoordinate;
        bits_per_component = pmp->BitsPerComponent;
        bits_per_flag = -1;
    }
    if (code < 0 ||
        (code = pdf_put_shading_Function(pdev, pscd, pmp->Function, pranges)) < 0 ||
        (code = cos_dict_put_c_key_int(pscd, "/BitsPerCoordinate",
                                       bits_per_coordinate)) < 0 ||
        (code = cos_dict_put_c_key_int(pscd, "/BitsPerComponent",
                                       bits_per_component)) < 0
        )
        return code;

    if (from_array && pdev->CompatibilityLevel < 1.5 ) {
        float min_x, max_x, min_y, max_y, z;
        int i = 0, j, num_points = 1, num_components = 1;
        float c = 0;
        float x, y;

        /*
         * Because of the Acrobat Reader limitation noted in gdevpdfx.h,
         * we must limit coordinate values to 16 bits. We no longer
         * attempt to support Acrobat 1.3 and below (which only permit 14 bits).
         */
        min_x = min_y = PDFA_MIN_MESH_COORDINATE;
        max_x = max_y = PDFA_MAX_MESH_COORDINATE;

        switch(ShadingType(psh)){
            case shading_type_Tensor_product_patch:
                    while ((flag = shade_next_flag(&cs, 0)) >= 0){
                        num_points = (flag == 0 ? 16 : 12);
                        num_components = num_comp * (flag == 0 ? 4 : 2);

                        for (j = 0; j < num_points;j++) {
                            code = cs.get_decoded(&cs, 0, NULL, &x);
                            if (code < 0)
                                break;
                            code = cs.get_decoded(&cs, 0, NULL, &y);
                            if (code < 0)
                                break;
                            for (i = 0; i < num_components; ++i) {
                                code = cs.get_decoded(&cs, 0, NULL, &c);
                                if (code < 0)
                                    break;
                            }
                            if (x < min_x)
                                min_x = x;
                            if (y < min_y)
                                min_y = y;
                            if (x > max_x)
                                max_x = x;
                            if (y > max_y)
                                max_y = y;
                        }
                    }
                break;
            case shading_type_Coons_patch:
                    while ((flag = shade_next_flag(&cs, 0)) >= 0){
                        num_points = (flag == 0 ? 12 : 8);
                        num_components = num_comp * (flag == 0 ? 4 : 2);

                        for (j = 0; j < num_points;j++) {
                            code = cs.get_decoded(&cs, 0, NULL, &x);
                            if (code < 0)
                                break;
                            code = cs.get_decoded(&cs, 0, NULL, &y);
                            if (code < 0)
                                break;
                            for (i = 0; i < num_components; ++i) {
                                code = cs.get_decoded(&cs, 0, NULL, &c);
                                if (code < 0)
                                    break;
                            }
                            if (x < min_x)
                                min_x = x;
                            if (y < min_y)
                                min_y = y;
                            if (x > max_x)
                                max_x = x;
                            if (y > max_y)
                                max_y = y;
                        }
                    }
                break;
            case shading_type_Free_form_Gouraud_triangle:
                {
                    while ((flag = shade_next_flag(&cs, 0)) >= 0){
                        code = cs.get_decoded(&cs, 0, NULL, &x);
                        if (code < 0)
                            break;
                        code = cs.get_decoded(&cs, 0, NULL, &y);
                        if (code < 0)
                            break;
                        for (i = 0; i < num_comp; ++i) {
                            code = cs.get_decoded(&cs, 0, NULL, &c);
                            if (code < 0)
                                break;
                        }
                        if (x < min_x)
                            min_x = x;
                        if (y < min_y)
                            min_y = y;
                        if (x > max_x)
                            max_x = x;
                        if (y > max_y)
                            max_y = y;
                    }
                }
                break;
            case shading_type_Lattice_form_Gouraud_triangle:
                {
                    while (!seofp(cs.s)) {
                        code = cs.get_decoded(&cs, 0, NULL, &x);
                        if (code < 0)
                            break;
                        code = cs.get_decoded(&cs, 0, NULL, &y);
                        if (code < 0)
                            break;
                        for (i = 0; i < num_comp; ++i) {
                            code = cs.get_decoded(&cs, 0, NULL, &c);
                            if (code < 0)
                                break;
                        }
                        if (x < min_x)
                            min_x = x;
                        if (y < min_y)
                            min_y = y;
                        if (x > max_x)
                            max_x = x;
                        if (y > max_y)
                            max_y = y;
                    }
                }
                break;
            default:
                break;
        }
        s_init(&cs.ds, NULL);
        sread_string(&cs.ds, pmp->DataSource.data.str.data,
                     pmp->DataSource.data.str.size);
        cs.s = &cs.ds;

        *rescale = (int)ceil(min_x / PDFA_MIN_MESH_COORDINATE);
        z = ceil(min_y / PDFA_MIN_MESH_COORDINATE);
        if (z > *rescale)
            *rescale = (int)z;
        z = ceil(max_x / PDFA_MAX_MESH_COORDINATE);
        if (z > *rescale)
            *rescale = (int)z;
        z = ceil(max_y / PDFA_MAX_MESH_COORDINATE);
        if (z > *rescale)
            *rescale = (int)z;
        data_params.rescale = *rescale;
    }

    switch (ShadingType(psh)) {
    case shading_type_Free_form_Gouraud_triangle: {
        const gs_shading_FfGt_params_t *const params =
            (const gs_shading_FfGt_params_t *)pmp;

        data_params.num_points = 1;
        data_params.num_components = num_comp;
        if (from_array) {
            while ((flag = shade_next_flag(&cs, 0)) >= 0)
                if ((code = put_float_mesh_data(pdev, pscs, &cs, flag,
                                                num_comp, &data_params)) < 0)
                    return code;
            if (!seofp(cs.s))
                code = gs_note_error(gs_error_rangecheck);
        }
        if (bits_per_flag < 0)
            bits_per_flag = params->BitsPerFlag;
        break;
    }
    case shading_type_Lattice_form_Gouraud_triangle: {
        const gs_shading_LfGt_params_t *const params =
            (const gs_shading_LfGt_params_t *)pmp;

        data_params.num_points = 1;
        data_params.num_components = num_comp;
        if (from_array)
            while (!seofp(cs.s))
                if ((code = put_float_mesh_data(pdev, pscs, &cs, -1,
                                                num_comp, &data_params)) < 0)
                    return code;
        code = cos_dict_put_c_key_int(pscd, "/VerticesPerRow",
                                      params->VerticesPerRow);
        return code;
    }
    case shading_type_Coons_patch: {
        const gs_shading_Cp_params_t *const params =
            (const gs_shading_Cp_params_t *)pmp;

        if (from_array) {
            while ((flag = shade_next_flag(&cs, 0)) >= 0) {
                data_params.num_points = (flag == 0 ? 12 : 8);
                data_params.num_components = num_comp * (flag == 0 ? 4 : 2);
                if ((code = put_float_mesh_data(pdev, pscs, &cs, flag,
                                                num_comp, &data_params)) < 0)
                    return code;
            }
            if (!seofp(cs.s))
                code = gs_note_error(gs_error_rangecheck);
        }
        if (bits_per_flag < 0)
            bits_per_flag = params->BitsPerFlag;
        break;
    }
    case shading_type_Tensor_product_patch: {
        const gs_shading_Tpp_params_t *const params =
            (const gs_shading_Tpp_params_t *)pmp;

        if (from_array) {
            while ((flag = shade_next_flag(&cs, 0)) >= 0) {
                data_params.num_points = (flag == 0 ? 16 : 12);
                data_params.num_components = num_comp * (flag == 0 ? 4 : 2);
                if ((code = put_float_mesh_data(pdev, pscs, &cs, flag, num_comp,
                                                &data_params)) < 0)
                    return code;
            }
            if (!seofp(cs.s))
                code = gs_note_error(gs_error_rangecheck);
        }
        if (bits_per_flag < 0)
            bits_per_flag = params->BitsPerFlag;
        break;
    }
    default:
        return_error(gs_error_rangecheck);
    }
    code1 =  cos_dict_put_c_key_int(pscd, "/BitsPerFlag", bits_per_flag);
    if (code1 < 0)
        return code1;
    return code;
}

/* Write a PatternType 2 (shading pattern) color. */
int
pdf_put_pattern2(gx_device_pdf *pdev, const gs_gstate * pgs, const gx_drawing_color *pdc,
                 const psdf_set_color_commands_t *ppscc,
                 pdf_resource_t **ppres)
{
    const gs_pattern2_instance_t *pinst =
        (gs_pattern2_instance_t *)pdc->ccolor.pattern;
    const gs_shading_t *psh = pinst->templat.Shading;
    cos_value_t v;
    pdf_resource_t *pres;
    pdf_resource_t *psres;
    cos_dict_t *pcd;
    cos_object_t *psco;
    const gs_range_t *pranges;
    int code = pdf_cs_Pattern_colored(pdev, &v);
    int code1 = 0, rescale = 1;
    gs_matrix smat;
    gs_point dist;

    if (code < 0)
        return code;
    code = pdf_alloc_resource(pdev, resourcePattern, gs_no_id, ppres, -1);
    if (code < 0)
        return code;
    pres = *ppres;
    cos_become(pres->object, cos_type_dict);
    pcd = (cos_dict_t *)pres->object;
    code = pdf_alloc_resource(pdev, resourceShading, gs_no_id, &psres, -1);
    if (code < 0)
        return code;
    psco = psres->object;
    if (ShadingType(psh) >= 4) {
        /* Shading has an associated data stream. */
        cos_become(psco, cos_type_stream);
        code = pdf_put_shading_common(pdev, cos_stream_dict((cos_stream_t *)psco), pgs,
                                      psh, pinst->shfill, &pranges);
        if (code >= 0)
            code1 = pdf_put_mesh_shading(pdev, (cos_stream_t *)psco, psh, pranges, &rescale);
        else
            /* We won't use this shading, we fall back because we couldn't write it */
            psres->where_used = 0;
    } else {
        cos_become(psco, cos_type_dict);
        code = pdf_put_shading_common(pdev, (cos_dict_t *)psco, pgs, psh, pinst->shfill, &pranges);
        if (code >= 0)
            code1 = pdf_put_scalar_shading(pdev, (cos_dict_t *)psco, psh, pranges);
        else
            /* We won't use this shading, we fall back because we couldn't write it */
            psres->where_used = 0;
    }
    if (psres->where_used) {
        code = pdf_substitute_resource(pdev, &psres, resourceShading, NULL, false);
        if (code < 0)
            return code;
        psco = psres->object;
        psres->where_used |= pdev->used_mask;
    }
    /*
     * In PDF, the Matrix is the transformation from the pattern space to
     * the *default* user coordinate space, not the current space.
     * NB. For a form the default space is the parent. This means that when a
     * form is nested inside a form, the default space is the space of the
     * first form, and therefore we do *not* remove the resolution scaling.
     */
    gs_currentmatrix(pinst->saved, &smat);
    {
        double xscale = 1.0, yscale = 1.0;
        if (pdev->FormDepth == 0) {
            xscale = 72.0 / pdev->HWResolution[0];
            yscale = 72.0 / pdev->HWResolution[1];
        }

        if (rescale != 1) {
            xscale *= rescale;
            yscale *= rescale;
        }
        smat.xx *= xscale, smat.yx *= xscale, smat.tx *= xscale;
        smat.xy *= yscale, smat.yy *= yscale, smat.ty *= yscale;
    }

    /* Bug #697451, if we emit a PDF with a type 2 Pattern where the
     * Matrix is degenerate, Acrobat throws an error and aborts the
     * page content stream. Distiller refuses to embed the shfill,
     * it silently (!) ignores the problem. So here we test to see
     * if the CTM is degenerate, if it is, replace it with the
     * smallest Matrix we can.
     */
    code = gs_distance_transform_inverse(1, 1, &smat, &dist);
    if (code == gs_error_undefinedresult) {
        smat.xx = smat.yy = 0.00000001f;
        smat.xy = smat.yx = smat.tx = smat.ty = 0;
        code = 0;
    }

    if (code < 0 ||
        (code = cos_dict_put_c_key_int(pcd, "/PatternType", 2)) < 0 ||
        (code = cos_dict_put_c_key_object(pcd, "/Shading", psco)) < 0 ||
        (code = cos_dict_put_matrix(pdev, pcd, "/Matrix", &smat)) < 0
        /****** ExtGState ******/
        )
        return code;
    code = pdf_substitute_resource(pdev, &pres, resourcePattern, NULL, false);
    if (code < 0)
        return code;
    pres->where_used |= pdev->used_mask;
    *ppres = pres;

    cos_value_write(&v, pdev);
    pprints1(pdev->strm, " %s\n", ppscc->setcolorspace);
    return code1;
}

/*
    Include color space.
 */
int
gdev_pdf_include_color_space(gx_device *dev, gs_color_space *cspace, const byte *res_name, int name_length)
{
    gx_device_pdf * pdev = (gx_device_pdf *)dev;
    cos_value_t cs_value;

    return pdf_color_space_named(pdev, NULL, &cs_value, NULL, cspace,
                                &pdf_color_space_names, true, res_name, name_length, false);
}
