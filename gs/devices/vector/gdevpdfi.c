/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Image handling for PDF-writing driver */
#include "memory_.h"
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsdevice.h"
#include "gsflip.h"
#include "gsstate.h"
#include "gscolor2.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"		/* for data stream */
#include "gxcspace.h"
#include "gximage3.h"
#include "gximag3x.h"
#include "gsiparm4.h"
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "gxcolor2.h"
#include "gxhldevc.h"
#include "gxdevsop.h"
#include "gsicc_manage.h"
#include "gsform1.h"

/* Forward references */
static image_enum_proc_plane_data(pdf_image_plane_data);
static image_enum_proc_end_image(pdf_image_end_image);
static image_enum_proc_end_image(pdf_image_end_image_object);
static image_enum_proc_end_image(pdf_image_end_image_object2);
static image_enum_proc_end_image(pdf_image_end_image_cvd);
static IMAGE3_MAKE_MID_PROC(pdf_image3_make_mid);
static IMAGE3_MAKE_MCDE_PROC(pdf_image3_make_mcde);
static IMAGE3X_MAKE_MID_PROC(pdf_image3x_make_mid);
static IMAGE3X_MAKE_MCDE_PROC(pdf_image3x_make_mcde);

static const gx_image_enum_procs_t pdf_image_enum_procs = {
    pdf_image_plane_data,
    pdf_image_end_image
};
static const gx_image_enum_procs_t pdf_image_object_enum_procs = {
    pdf_image_plane_data,
    pdf_image_end_image_object
};
static const gx_image_enum_procs_t pdf_image_object_enum_procs2 = {
    pdf_image_plane_data,
    pdf_image_end_image_object2
};
static const gx_image_enum_procs_t pdf_image_cvd_enum_procs = {
    gx_image1_plane_data,
    pdf_image_end_image_cvd,
    gx_image1_flush
};

/* ---------------- Driver procedures ---------------- */

/* Define the structure for keeping track of progress through an image. */
typedef struct pdf_image_enum_s {
    gx_image_enum_common;
    int width;
    int bits_per_pixel;		/* bits per pixel (per plane) */
    int rows_left;
    pdf_image_writer writer;
    gs_matrix mat;
} pdf_image_enum;
gs_private_st_composite(st_pdf_image_enum, pdf_image_enum, "pdf_image_enum",
  pdf_image_enum_enum_ptrs, pdf_image_enum_reloc_ptrs);
/* GC procedures */
static ENUM_PTRS_WITH(pdf_image_enum_enum_ptrs, pdf_image_enum *pie)
    if (index < pdf_image_writer_max_ptrs) {
        gs_ptr_type_t ret =
            ENUM_USING(st_pdf_image_writer, &pie->writer, sizeof(pie->writer),
                       index);

        if (ret == 0)		/* don't stop early */
            ENUM_RETURN(0);
        return ret;
    }
    return ENUM_USING_PREFIX(st_gx_image_enum_common,
                             pdf_image_writer_max_ptrs);
ENUM_PTRS_END
static RELOC_PTRS_WITH(pdf_image_enum_reloc_ptrs, pdf_image_enum *pie)
{
    RELOC_USING(st_pdf_image_writer, &pie->writer, sizeof(pie->writer));
    RELOC_USING(st_gx_image_enum_common, vptr, size);
}
RELOC_PTRS_END

/*
 * Test whether we can write an image in-line.  This is always true,
 * because we only support PDF 1.2 and later.
 */
static bool
can_write_image_in_line(const gx_device_pdf *pdev, const gs_image_t *pim)
{
    return true;
}

/*
 * Convert a Type 4 image to a Type 1 masked image if possible.
 * Type 1 masked images are more compact, and are supported in all PDF
 * versions, whereas general masked images require PDF 1.3 or higher.
 * Also, Acrobat 5 for Windows has a bug that causes an error for images
 * with a color-key mask, at least for 1-bit-deep images using an Indexed
 * color space.
 */
static int
color_is_black_or_white(gx_device *dev, const gx_drawing_color *pdcolor)
{
    return (!color_is_pure(pdcolor) ? -1 :
            gx_dc_pure_color(pdcolor) == gx_device_black(dev) ? 0 :
            gx_dc_pure_color(pdcolor) == gx_device_white(dev) ? 1 : -1);
}
static int
pdf_convert_image4_to_image1(gx_device_pdf *pdev,
                             const gs_imager_state *pis,
                             const gx_drawing_color *pbcolor,
                             const gs_image4_t *pim4, gs_image_t *pim1,
                             gx_drawing_color *pdcolor)
{
    if (pim4->BitsPerComponent == 1 &&
        pim4->ColorSpace->type->num_components(pim4->ColorSpace) == 1 &&
        (pim4->MaskColor_is_range ?
         pim4->MaskColor[0] | pim4->MaskColor[1] :
         pim4->MaskColor[0]) <= 1
        ) {
        gx_device *const dev = (gx_device *)pdev;
        const gs_color_space *pcs = pim4->ColorSpace;
        bool write_1s = !pim4->MaskColor[0];
        gs_client_color cc;
        int code;

        /*
         * Prepare the drawing color.  (pdf_prepare_imagemask will set it.)
         * This is the other color in the image (the one that isn't the
         * mask key), taking Decode into account.
         */

        cc.paint.values[0] = pim4->Decode[(int)write_1s];
        cc.pattern = 0;
        code = pcs->type->remap_color(&cc, pcs, pdcolor, pis, dev,
                                      gs_color_select_texture);
        if (code < 0)
            return code;

        /*
         * The PDF imaging model doesn't support RasterOp.  We can convert a
         * Type 4 image to a Type 1 imagemask only if the effective RasterOp
         * passes through the source color unchanged.  "Effective" means we
         * take into account CombineWithColor, and whether the source and/or
         * texture are black, white, or neither.
         */
        {
            gs_logical_operation_t lop = pis->log_op;
            int black_or_white = color_is_black_or_white(dev, pdcolor);

            switch (black_or_white) {
            case 0: lop = lop_know_S_0(lop); break;
            case 1: lop = lop_know_S_1(lop); break;
            default: DO_NOTHING;
            }
            if (pim4->CombineWithColor)
                switch (color_is_black_or_white(dev, pbcolor)) {
                case 0: lop = lop_know_T_0(lop); break;
                case 1: lop = lop_know_T_1(lop); break;
                default: DO_NOTHING;
                }
            else
                lop = lop_know_T_0(lop);
            switch (lop_rop(lop)) {
            case rop3_0:
                if (black_or_white != 0)
                    return -1;
                break;
            case rop3_1:
                if (black_or_white != 1)
                    return -1;
                break;
            case rop3_S:
                break;
            default:
                return -1;
            }
            if ((lop & lop_S_transparent) && black_or_white == 1)
                return -1;
        }

        /* All conditions are met.  Convert to a masked image. */

        gs_image_t_init_mask_adjust(pim1, write_1s, false);
#define COPY_ELEMENT(e) pim1->e = pim4->e
        COPY_ELEMENT(ImageMatrix);
        COPY_ELEMENT(Width);
        COPY_ELEMENT(Height);
        pim1->BitsPerComponent = 1;
        /* not Decode */
        COPY_ELEMENT(Interpolate);
        pim1->format = gs_image_format_chunky; /* BPC = 1, doesn't matter */
#undef COPY_ELEMENT
        return 0;
    }
    return -1;			/* arbitrary <0 */
}

static int
pdf_begin_image_data_decoded(gx_device_pdf *pdev, int num_components, const gs_range_t *pranges, int i,
                             gs_pixel_image_t *pi, cos_value_t *cs_value, pdf_image_enum *pie)
{

    if (pranges) {
        /* Rescale the Decode values for the image data. */
        const gs_range_t *pr = pranges;
        float *decode = pi->Decode;
        int j;

        for (j = 0; j < num_components; ++j, ++pr, decode += 2) {
            double vmin = decode[0], vmax = decode[1];
            double base = pr->rmin, factor = pr->rmax - base;

            decode[1] = (vmax - vmin) / factor + (vmin - base);
            decode[0] = vmin - base;
        }
    }
    return pdf_begin_image_data(pdev, &pie->writer, pi, cs_value, i);
}

static int
make_device_color_space(gx_device_pdf *pdev,
                        gs_color_space_index output_cspace_index,
                        gs_color_space **ppcs)
{
    gs_color_space *cs;
    gs_memory_t *mem = pdev->v_memory;

    switch (output_cspace_index) {
        case gs_color_space_index_DeviceGray:
            cs = gs_cspace_new_DeviceGray(mem);
            break;
        case gs_color_space_index_DeviceRGB:
            cs = gs_cspace_new_DeviceRGB(mem);
            break;
        case gs_color_space_index_DeviceCMYK:
            cs = gs_cspace_new_DeviceCMYK(mem);
            break;
        default:
            /* Notify the user and terminate.
               Don't emit rangecheck becuause it would fall back
               to a default implementation (rasterisation).
             */
            emprintf(mem, "Unsupported ProcessColorModel");
            return_error(gs_error_undefined);
    }
    if (cs == NULL)
        return_error(gs_error_VMerror);
    *ppcs = cs;
    return 0;
}

static bool
check_image_color_space(gs_pixel_image_t * pim, gs_color_space_index index)
{
    if (pim->ColorSpace->type->index == index)
        return true;
    if (pim->ColorSpace->type->index == gs_color_space_index_Indexed)
        if (pim->ColorSpace->base_space->type->index == index)
            return true;
    return false;
}

/*
 * Start processing an image.  This procedure takes extra arguments because
 * it has to do something slightly different for the parts of an ImageType 3
 * image.
 */
typedef enum {
    PDF_IMAGE_DEFAULT,
    PDF_IMAGE_TYPE3_MASK,	/* no in-line, don't render */
    PDF_IMAGE_TYPE3_DATA	/* no in-line */
} pdf_typed_image_context_t;

/*
 * We define this union because psdf_setup_image_filters may alter the
 * gs_pixel_image_t part, but pdf_begin_image_data must also have access
 * to the type-specific parameters.
 */
typedef union image_union_s {
    gs_pixel_image_t pixel;	/* we may change some components */
    gs_image1_t type1;
    gs_image3_t type3;
    gs_image3x_t type3x;
    gs_image4_t type4;
} image_union_t;

static int pdf_begin_typed_image(gx_device_pdf *pdev,
    const gs_imager_state * pis, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect * prect,
    const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
    gs_memory_t * mem, gx_image_enum_common_t ** pinfo,
    pdf_typed_image_context_t context);

static int
pdf_begin_typed_image_impl(gx_device_pdf *pdev, const gs_imager_state * pis,
                      const gs_matrix *pmat, const gs_image_common_t *pic,
                      const gs_int_rect * prect,
                      const gx_drawing_color * pdcolor,
                      const gx_clip_path * pcpath, gs_memory_t * mem,
                      gx_image_enum_common_t ** pinfo,
                      pdf_typed_image_context_t context,
                      image_union_t *image)
{
    cos_dict_t *pnamed = 0;
    const gs_pixel_image_t *pim;
    int code, i;
    pdf_image_enum *pie;
    gs_image_format_t format;
    const gs_color_space *pcs;
    cos_value_t cs_value;
    int num_components;
    bool is_mask = false, in_line = false;
    gs_int_rect rect;
    int width, height;
    const gs_range_t *pranges = 0;
    const pdf_color_space_names_t *names;
    bool convert_to_process_colors = false;
    gs_color_space *pcs_device = NULL;
    gs_color_space *pcs_orig = NULL;
    int BPC_orig = 1, BPC_device = 8;
    pdf_lcvd_t *cvd = NULL;
    bool force_lossless = false;

    /*
     * Pop the image name from the NI stack.  We must do this, to keep the
     * stack in sync, even if it turns out we can't handle the image.
     */
    {
        cos_value_t ni_value;

        if (cos_array_unadd(pdev->NI_stack, &ni_value) >= 0)
            pnamed = (cos_dict_t *)ni_value.contents.object;
    }

    /* An initialization for pdf_end_and_do_image :
       We need to delay adding the "Mask" entry into a type 3 image dictionary
       until the mask is completed due to equal image merging. */
    pdev->image_mask_id = gs_no_id;

    /* Check for the image types we can handle. */
    switch (pic->type->index) {
    case 1: {
        const gs_image_t *pim1 = (const gs_image_t *)pic;

        if (pim1->Alpha != gs_image_alpha_none)
            goto nyi;
        is_mask = pim1->ImageMask;
        if (is_mask) {
            /* If parameters are invalid, use the default implementation. */
            if (!(gx_dc_is_pattern1_color(pdcolor)))
                if (pim1->BitsPerComponent != 1 ||
                    !((pim1->Decode[0] == 0.0 && pim1->Decode[1] == 1.0) ||
                      (pim1->Decode[0] == 1.0 && pim1->Decode[1] == 0.0))
                    )
                    goto nyi;
        }
        /* If image is not type 3X and we can write in-line then make it so */

         in_line = context == PDF_IMAGE_DEFAULT &&
            can_write_image_in_line(pdev, pim1);

        image[0].type1 = *pim1;
        break;
    }
    case 3: {
        const gs_image3_t *pim3 = (const gs_image3_t *)pic;
        gs_image3_t pim3a;
        const gs_image_common_t *pic1 = pic;
        gs_matrix m, mi;
        const gs_matrix *pmat1 = pmat;

        pdev->image_mask_is_SMask = false;
        if (pdev->CompatibilityLevel < 1.2)
            goto nyi;
        if (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
                       prect->q.x == pim3->Width &&
                       prect->q.y == pim3->Height))
            goto nyi;
        if (pdev->CompatibilityLevel < 1.3 && !pdev->PatternImagemask) {
            if (pdf_must_put_clip_path(pdev, pcpath))
                code = pdf_unclip(pdev);
            else
                code = pdf_open_page(pdev, PDF_IN_STREAM);
            if (code < 0)
                return code;
            code = pdf_put_clip_path(pdev, pcpath);
            if (code < 0)
                return code;
            gs_make_identity(&m);
            pmat1 = &m;
            m.tx = floor(pis->ctm.tx + 0.5); /* Round the origin against the image size distorsions */
            m.ty = floor(pis->ctm.ty + 0.5);
            pim3a = *pim3;
            gs_matrix_invert(&pim3a.ImageMatrix, &mi);
            gs_make_identity(&pim3a.ImageMatrix);
            if (pim3a.Width < pim3a.MaskDict.Width && pim3a.Width > 0) {
                int sx = (pim3a.MaskDict.Width + pim3a.Width - 1) / pim3a.Width;

                gs_matrix_scale(&mi, 1.0 / sx, 1, &mi);
                gs_matrix_scale(&pim3a.ImageMatrix, 1.0 / sx, 1, &pim3a.ImageMatrix);
            }
            if (pim3a.Height < pim3a.MaskDict.Height && pim3a.Height > 0) {
                int sy = (pim3a.MaskDict.Height + pim3a.Height - 1) / pim3a.Height;

                gs_matrix_scale(&mi, 1, 1.0 / sy, &mi);
                gs_matrix_scale(&pim3a.ImageMatrix, 1, 1.0 / sy, &pim3a.ImageMatrix);
            }
            gs_matrix_multiply(&mi, &pim3a.MaskDict.ImageMatrix, &pim3a.MaskDict.ImageMatrix);
            pic1 = (gs_image_common_t *)&pim3a;
            /* Setting pdev->converting_image_matrix to communicate with pdf_image3_make_mcde. */
            gs_matrix_multiply(&mi, &ctm_only(pis), &pdev->converting_image_matrix);
        }
        /*
         * We handle ImageType 3 images in a completely different way:
         * the default implementation sets up the enumerator.
         */
        return gx_begin_image3_generic((gx_device *)pdev, pis, pmat1, pic1,
                                       prect, pdcolor, pcpath, mem,
                                       pdf_image3_make_mid,
                                       pdf_image3_make_mcde, pinfo);
    }
    case IMAGE3X_IMAGETYPE: {
        /* See ImageType3 above for more information. */
        const gs_image3x_t *pim3x = (const gs_image3x_t *)pic;

        if (pdev->CompatibilityLevel < 1.4)
            goto nyi;
        if (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
                       prect->q.x == pim3x->Width &&
                       prect->q.y == pim3x->Height))
            goto nyi;
        pdev->image_mask_is_SMask = true;
        return gx_begin_image3x_generic((gx_device *)pdev, pis, pmat, pic,
                                        prect, pdcolor, pcpath, mem,
                                        pdf_image3x_make_mid,
                                        pdf_image3x_make_mcde, pinfo);
    }
    case 4: {
        /* Try to convert the image to a plain masked image. */
        gx_drawing_color icolor;

        pdev->image_mask_is_SMask = false;
        if (pdf_convert_image4_to_image1(pdev, pis, pdcolor,
                                         (const gs_image4_t *)pic,
                                         &image[0].type1, &icolor) >= 0) {
            gs_state *pgs = (gs_state *)gx_hld_get_gstate_ptr(pis);

            if (pgs == NULL)
                return_error(gs_error_unregistered); /* Must not happen. */

            /* Undo the pop of the NI stack if necessary. */
            if (pnamed)
                cos_array_add_object(pdev->NI_stack, COS_OBJECT(pnamed));
            /* HACK: temporary patch the color space, to allow
               pdf_prepare_imagemask to write the right color for the imagemask. */
            code = gs_gsave(pgs);
            if (code < 0)
                return code;
            /* {csrc}: const cast warning */
            code = gs_setcolorspace(pgs, ((const gs_image4_t *)pic)->ColorSpace);
            if (code < 0)
                return code;
            code = pdf_begin_typed_image(pdev, pis, pmat,
                                         (gs_image_common_t *)&image[0].type1,
                                         prect, &icolor, pcpath, mem,
                                         pinfo, context);
            if (code < 0)
                return code;
            return gs_grestore(pgs);
        }
        /* No luck.  Masked images require PDF 1.3 or higher. */
        if (pdev->CompatibilityLevel < 1.2 || pdev->params.ColorConversionStrategy != ccs_LeaveColorUnchanged)
            goto nyi;
        if (pdev->CompatibilityLevel < 1.3 && !pdev->PatternImagemask) {
            gs_matrix m, m1, mi;
            gs_image4_t pi4 = *(const gs_image4_t *)pic;

            if (pdf_must_put_clip_path(pdev, pcpath))
                code = pdf_unclip(pdev);
            else
                code = pdf_open_page(pdev, PDF_IN_STREAM);
            if (code < 0)
                return code;
            code = pdf_put_clip_path(pdev, pcpath);
            if (code < 0)
                return code;
            gs_make_identity(&m1);
            gs_matrix_invert(&pic->ImageMatrix, &mi);
            gs_matrix_multiply(&mi, &ctm_only(pis), &m);
            code = pdf_setup_masked_image_converter(pdev, mem, &m, &cvd,
                                 true, 0, 0, pi4.Width, pi4.Height, false);
            if (code < 0)
                return code;
            cvd->mdev.is_open = true; /* fixme: same as above. */
            cvd->mask->is_open = true; /* fixme: same as above. */
            cvd->mask_is_empty = false;
            code = (*dev_proc(cvd->mask, fill_rectangle))((gx_device *)cvd->mask,
                        0, 0, cvd->mask->width, cvd->mask->height, (gx_color_index)0);
            if (code < 0)
                return code;
            gx_device_retain((gx_device *)cvd, true);
            gx_device_retain((gx_device *)cvd->mask, true);
            gs_make_identity(&pi4.ImageMatrix);
            code = gx_default_begin_typed_image((gx_device *)cvd,
                pis, &m1, (gs_image_common_t *)&pi4, prect, pdcolor, NULL, mem, pinfo);
            if (code < 0)
                return code;
            (*pinfo)->procs = &pdf_image_cvd_enum_procs;
            return 0;
        }
        image[0].type4 = *(const gs_image4_t *)pic;
        break;
    }
    default:
        goto nyi;
    }
    pim = (const gs_pixel_image_t *)pic;
    format = pim->format;
    switch (format) {
    case gs_image_format_chunky:
    case gs_image_format_component_planar:
        break;
    default:
        goto nyi;
    }
    /* AR5 on Windows doesn't support 0-size images. Skipping. */
    if (pim->Width == 0 || pim->Height == 0)
        goto nyi;
    /* PDF doesn't support images with more than 8 bits per component. */
    switch (pim->BitsPerComponent) {
        case 1:
        case 2:
        case 4:
        case 8:
            break;
        case 12:
        case 16:
            goto nyi;
        default:
            return_error(gs_error_rangecheck);
    }
    pcs = pim->ColorSpace;
    num_components = (is_mask ? 1 : gs_color_space_num_components(pcs));

    if (pdf_must_put_clip_path(pdev, pcpath))
        code = pdf_unclip(pdev);
    else
        code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;
    if (context == PDF_IMAGE_TYPE3_MASK) {
        /*
         * The soft mask for an ImageType 3x image uses a DevicePixel
         * color space, which pdf_color_space() can't handle.  Patch it
         * to DeviceGray here.
         */
        /* {csrc} make sure this gets freed */
        pcs = gs_cspace_new_DeviceGray(pdev->memory);
    } else if (is_mask)
        code = pdf_prepare_imagemask(pdev, pis, pdcolor);
    else
        code = pdf_prepare_image(pdev, pis);
    if (code < 0)
        goto nyi;
    if (prect)
        rect = *prect;
    else {
        rect.p.x = rect.p.y = 0;
        rect.q.x = pim->Width, rect.q.y = pim->Height;
    }
    pie = gs_alloc_struct(mem, pdf_image_enum, &st_pdf_image_enum,
                        "pdf_begin_image");
    if (pie == 0)
        return_error(gs_error_VMerror);
    memset(pie, 0, sizeof(*pie)); /* cleanup entirely for GC to work in all cases. */
    *pinfo = (gx_image_enum_common_t *) pie;
    gx_image_enum_common_init(*pinfo, (const gs_data_image_t *) pim,
                    ((pdev->CompatibilityLevel >= 1.3) ?
                            (context == PDF_IMAGE_TYPE3_MASK ?
                            &pdf_image_object_enum_procs :
                            &pdf_image_enum_procs) :
                            context == PDF_IMAGE_TYPE3_MASK ?
                            &pdf_image_object_enum_procs :
                            context == PDF_IMAGE_TYPE3_DATA ?
                            &pdf_image_object_enum_procs2 :
                            &pdf_image_enum_procs),
                        (gx_device *)pdev, num_components, format);
    pie->memory = mem;
    width = rect.q.x - rect.p.x;
    pie->width = width;
    height = rect.q.y - rect.p.y;
    pie->bits_per_pixel =
        pim->BitsPerComponent * num_components / pie->num_planes;
    pie->rows_left = height;
    if (pnamed != 0) /* Don't in-line the image if it is named. */
        in_line = false;
    else {
        double nbytes = (double)(((ulong) pie->width * pie->bits_per_pixel + 7) >> 3) *
            pie->num_planes * pie->rows_left;

        in_line &= (nbytes < pdev->MaxInlineImageSize);
    }
    if (rect.p.x != 0 || rect.p.y != 0 ||
        rect.q.x != pim->Width || rect.q.y != pim->Height ||
        (is_mask && pim->CombineWithColor)
        /* Color space setup used to be done here: see SRZB comment below. */
        ) {
        gs_free_object(mem, pie, "pdf_begin_image");
        goto nyi;
    }
    if (pmat == 0)
        pmat = &ctm_only(pis);
    {
        gs_matrix mat;
        gs_matrix bmat;
        int code;

        pdf_make_bitmap_matrix(&bmat, -rect.p.x, -rect.p.y,
                               pim->Width, pim->Height, height);
        if ((code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
            (code = gs_matrix_multiply(&bmat, &mat, &mat)) < 0 ||
            (code = gs_matrix_multiply(&mat, pmat, &pie->mat)) < 0
            ) {
            gs_free_object(mem, pie, "pdf_begin_image");
            return code;
        }
        /* AR3,AR4 show no image when CTM is singular; AR5 reports an error */
        if (pie->mat.xx * pie->mat.yy == pie->mat.xy * pie->mat.yx) {
            gs_free_object(mem, pie, "pdf_begin_image");
            goto nyi;
        }
    }
    code = pdf_put_clip_path(pdev, pcpath);
    if (code < 0)
        return code;
    pdf_image_writer_init(&pie->writer);
    pie->writer.alt_writer_count = (in_line ||
                                    (pim->Width <= 64 && pim->Height <= 64)
                                    ? 1 : 2);
    if ((image[0].pixel.ColorSpace != NULL &&
        image[0].pixel.ColorSpace->type->index == gs_color_space_index_Indexed
        && pdev->params.ColorImage.DownsampleType != ds_Subsample) ||
        pdev->transfer_not_identity)
        force_lossless = true;

    image[1] = image[0];
    names = (in_line ? &pdf_color_space_names_short : &pdf_color_space_names);
    if (!is_mask) {
        if (psdf_is_converting_image_to_RGB((gx_device_psdf *)pdev, pis, pim)) {
            /* psdf_setup_image_filters may change the color space
             * (in case of pdev->params.ConvertCMYKImagesToRGB == true).
             * Account it here.
             */
            cos_c_string_value(&cs_value, names->DeviceRGB);
        } else {
            /* A minor hack to deal with CIELAB images. */
            if (pcs->cmm_icc_profile_data != NULL &&
                pcs->cmm_icc_profile_data->islab) {
                    gscms_set_icc_range((cmm_profile_t **)&(pcs->cmm_icc_profile_data));
            }
            code = pdf_convert_ICC(pdev, pcs, &cs_value, names);
            if (code == 0) {
                code = pdf_color_space_named(pdev, &cs_value, &pranges, pcs, names,
                                         in_line, NULL, 0);
            }
            if (pcs->cmm_icc_profile_data != NULL &&
                pcs->cmm_icc_profile_data->islab) {
                gsicc_setrange_lab(pcs->cmm_icc_profile_data);
            }
            if (code < 0)
                convert_to_process_colors = true;
        }
    }
    if (image[0].pixel.ColorSpace != NULL && !(context == PDF_IMAGE_TYPE3_MASK)) {
        /* Not an imagemask. Also not a SMask, we carefully made a Gray space
         * for SMasks above, do not destroy it now!
         */
        if ((pdev->params.ColorConversionStrategy == ccs_Gray &&
             !check_image_color_space(&image[0].pixel, gs_color_space_index_DeviceGray)) ||
            (pdev->params.ColorConversionStrategy == ccs_sRGB &&
             !psdf_is_converting_image_to_RGB((const gx_device_psdf *)pdev, pis, &image[0].pixel) &&
             !check_image_color_space(&image[0].pixel, gs_color_space_index_DeviceGray) &&
             !check_image_color_space(&image[0].pixel, gs_color_space_index_DeviceRGB)) ||
            (pdev->params.ColorConversionStrategy == ccs_CMYK &&
             !check_image_color_space(&image[0].pixel, gs_color_space_index_DeviceGray) &&
             !check_image_color_space(&image[0].pixel, gs_color_space_index_DeviceCMYK))) {
            /* fixme : as a rudiment of old code,
                the case psdf_is_converting_image_to_RGB
                is handled with the 'cmyk_to_rgb' branch
                in psdf_setup_image_filters. */
            if ((pdev->params.ColorConversionStrategy == ccs_CMYK &&
                 strcmp(pdev->color_info.cm_name, "DeviceCMYK")) ||
                (pdev->params.ColorConversionStrategy == ccs_sRGB &&
                 strcmp(pdev->color_info.cm_name, "DeviceRGB")) ||
                (pdev->params.ColorConversionStrategy == ccs_Gray &&
                 strcmp(pdev->color_info.cm_name, "DeviceGray"))) {
                emprintf(pdev->memory, "ColorConversionStrategy isn't "
                         "compatible to ProcessColorModel.");
                return_error(gs_error_rangecheck);
            }
            convert_to_process_colors = true;
        }
    }
    if (convert_to_process_colors) {
        const char *sname;

        switch (pdev->pcm_color_info_index) {
            case gs_color_space_index_DeviceGray: sname = names->DeviceGray; break;
            case gs_color_space_index_DeviceRGB:  sname = names->DeviceRGB;  break;
            case gs_color_space_index_DeviceCMYK: sname = names->DeviceCMYK; break;
            default:
                emprintf(pdev->memory, "Unsupported ProcessColorModel.");
                return_error(gs_error_undefined);
        }
        cos_c_string_value(&cs_value, sname);
        pcs_orig = image[0].pixel.ColorSpace;
        BPC_orig = image[0].pixel.BitsPerComponent;
        code = make_device_color_space(pdev, pdev->pcm_color_info_index, &pcs_device);
        if (code < 0)
            goto fail;
        image[0].pixel.ColorSpace = pcs_device;
        image[0].pixel.BitsPerComponent = BPC_device = 8;
    }
    pdev->ParamCompatibilityLevel = pdev->CompatibilityLevel;

    /* This is rather hacky. the gs_image_pixel_t union has copies of the image
     * information, including the ColorSpace, but it does not increment the
     * reference counts of the counted objects (teh ColorSpace) when it makes
     * copies. However, if psdf_setup_image_filters() should change the colour
     * space into DeviceCMYK because it converts the image, it will decrement
     * the reference count of the original space. There is at least one place
     * where it is called with the original space, so we can't simply remove
     * the decrement. Handling this properly would entail incrementing the
     * reference count when we make the copy, and correctly decrementing it
     * in all the error conditions. Its easier to isolate the changes to
     * this piece of code.
     */
    rc_increment_cs(image[0].pixel.ColorSpace);
    code = pdf_begin_write_image(pdev, &pie->writer, gs_no_id, width,
                    height, pnamed, in_line);
    if (code < 0)
        goto fail;
    if (pie->writer.alt_writer_count == 1)
        code = psdf_setup_lossless_filters((gx_device_psdf *) pdev,
                                             &pie->writer.binary[0],
                                             &image[0].pixel, in_line);
    else {
        if (force_lossless) {
            /*
             * Some regrettable PostScript code (such as LanguageLevel 1 output
             * from Microsoft's PSCRIPT.DLL driver) misuses the transfer
             * function to accomplish the equivalent of indexed color.
             * Downsampling (well, only averaging) or JPEG compression are not
             * compatible with this.  Play it safe by using only lossless
             * filters if the transfer function(s) is/are other than the
             * identity and by setting the downsample type to Subsample..
             */
            int saved_downsample = pdev->params.ColorImage.DownsampleType;

            pdev->params.ColorImage.DownsampleType = ds_Subsample;
            code = psdf_setup_image_filters((gx_device_psdf *) pdev,
                                          &pie->writer.binary[0], &image[0].pixel,
                                          pmat, pis, true, in_line);
            pdev->params.ColorImage.DownsampleType = saved_downsample;
        } else {
            if (!convert_to_process_colors)
            code = psdf_setup_image_filters((gx_device_psdf *) pdev,
                                          &pie->writer.binary[0], &image[0].pixel,
                                          pmat, pis, true, in_line);
        }
    }
    if (code < 0) {
        if (image[0].pixel.ColorSpace == pim->ColorSpace)
            rc_decrement_only_cs(pim->ColorSpace, "psdf_setup_image_filters");
        goto fail;
    }

    if (convert_to_process_colors) {
        image[0].pixel.ColorSpace = pcs_orig;
        image[0].pixel.BitsPerComponent = BPC_orig;
        code = psdf_setup_image_colors_filter(&pie->writer.binary[0],
                    (gx_device_psdf *)pdev, &image[0].pixel, pis);
        if (code < 0)
            goto fail;
        image[0].pixel.ColorSpace = pcs_device;
        image[0].pixel.BitsPerComponent = BPC_device;
    }

    /* See the comment above about reference counting of the colour space */
    rc_increment_cs(image[1].pixel.ColorSpace);
    if (pie->writer.alt_writer_count > 1) {
        code = pdf_make_alt_stream(pdev, &pie->writer.binary[1]);
        if (code)
            goto fail;
        if (convert_to_process_colors) {
            image[1].pixel.ColorSpace = pcs_device;
            image[1].pixel.BitsPerComponent = BPC_device;
        }
        code = psdf_setup_image_filters((gx_device_psdf *) pdev,
                                  &pie->writer.binary[1], &image[1].pixel,
                                  pmat, pis, force_lossless, in_line);
        if (code == gs_error_rangecheck) {

            for (i=1;i < pie->writer.alt_writer_count; i++) {
                stream *s = pie->writer.binary[i].strm;
                cos_stream_t *pcos = cos_stream_from_pipeline(pie->writer.binary[i].strm);
                s_close_filters(&s, NULL);
                gs_free_object(pdev->pdf_memory, s, "compressed image stream");
                pcos->cos_procs->release((cos_object_t *)pcos, "pdf_begin_typed_image_impl");
                gs_free_object(pdev->pdf_memory, pcos, "compressed image cos_stream");
            }
            /* setup_image_compression rejected the alternative compression. */
            pie->writer.alt_writer_count = 1;
            memset(pie->writer.binary + 1, 0, sizeof(pie->writer.binary[1]));
            memset(pie->writer.binary + 2, 0, sizeof(pie->writer.binary[1]));
        } else if (code) {
            if (image[1].pixel.ColorSpace == pim->ColorSpace)
                rc_decrement_only_cs(pim->ColorSpace, "psdf_setup_image_filters");
            goto fail;
        }
        else if (convert_to_process_colors) {
            int Width = image[1].pixel.Width;
            int Height = image[1].pixel.Height;
            image[1].pixel.ColorSpace = pcs_orig;
            image[1].pixel.BitsPerComponent = BPC_orig;
            image[1].pixel.Width = image[0].pixel.Width;
            image[1].pixel.Height = image[0].pixel.Height;
            code = psdf_setup_image_colors_filter(&pie->writer.binary[1],
                    (gx_device_psdf *)pdev, &image[1].pixel, pis);
            if (code < 0) {
                if (image[1].pixel.ColorSpace == pim->ColorSpace)
                    rc_decrement_only_cs(pim->ColorSpace, "psdf_setup_image_filters");
                goto fail;
            }
            image[1].pixel.Width = Width;
            image[1].pixel.Height = Height;
            image[1].pixel.ColorSpace = pcs_device;
            image[1].pixel.BitsPerComponent = BPC_device;
        }
    }
    if (image[1].pixel.ColorSpace == pim->ColorSpace)
        rc_decrement_only_cs(pim->ColorSpace, "psdf_setup_image_filters");

    for (i = 0; i < pie->writer.alt_writer_count; i++) {
        code = pdf_begin_image_data_decoded(pdev, num_components, pranges, i,
                             &image[i].pixel, &cs_value, pie);
        if (code < 0)
            goto fail;
    }
    if (pie->writer.alt_writer_count == 2) {
        psdf_setup_compression_chooser(&pie->writer.binary[2],
             (gx_device_psdf *)pdev, pim->Width, pim->Height,
             num_components, pim->BitsPerComponent);
        pie->writer.alt_writer_count = 3;
    }
    if (pic->type->index == 4 && pdev->CompatibilityLevel < 1.3) {
        int i;

        /* Create a stream for writing the mask. */
        i = pie->writer.alt_writer_count;
        gs_image_t_init_mask_adjust((gs_image_t *)&image[i].type1, true, false);
        image[i].type1.Width = image[0].pixel.Width;
        image[i].type1.Height = image[0].pixel.Height;
        /* Won't use image[2]. */
        code = pdf_begin_write_image(pdev, &pie->writer, gs_no_id, width,
                    height, NULL, false);
        if (code)
            goto fail;
        code = psdf_setup_image_filters((gx_device_psdf *) pdev,
                                  &pie->writer.binary[i], &image[i].pixel,
                                  pmat, pis, force_lossless, in_line);
        if (code < 0)
            goto fail;
        psdf_setup_image_to_mask_filter(&pie->writer.binary[i],
             (gx_device_psdf *)pdev, pim->Width, pim->Height,
             num_components, pim->BitsPerComponent, image[i].type4.MaskColor);
        code = pdf_begin_image_data_decoded(pdev, num_components, pranges, i,
                             &image[i].pixel, &cs_value, pie);
        if (code < 0)
            goto fail;
        ++pie->writer.alt_writer_count;
        /* Note : Possible values for alt_writer_count are 1,2,3, 4.
           1 means no alternative streams.
           2 means the main image stream and a mask stream while converting
                   an Image Type 4.
           3 means the main image stream, alternative image compression stream,
                   and the compression chooser.
           4 meams 3 and a mask stream while convertingh an Image Type 4.
         */
    }
    return 0;
 fail:
    /****** SHOULD FREE STRUCTURES AND CLEAN UP HERE ******/
    /* Fall back to the default implementation. */
 nyi:
    return gx_default_begin_typed_image
        ((gx_device *)pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
        pinfo);
}

static int
old_pdf_begin_typed_image(gx_device_pdf *pdev, const gs_imager_state * pis,
                      const gs_matrix *pmat, const gs_image_common_t *pic,
                      const gs_int_rect * prect,
                      const gx_drawing_color * pdcolor,
                      const gx_clip_path * pcpath, gs_memory_t * mem,
                      gx_image_enum_common_t ** pinfo,
                      pdf_typed_image_context_t context)
{
  int code;
  image_union_t *image = (image_union_t *)gs_malloc(mem->non_gc_memory, 4,
                       sizeof(image_union_t), "pdf_begin_typed_image(image)");
  if (image == 0)
      return_error(gs_error_VMerror);
  code = pdf_begin_typed_image_impl(pdev, pis, pmat, pic, prect,
         pdcolor, pcpath, mem, pinfo, context, image);
  gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
  return code;
}

static int setup_type1_image(gx_device_pdf *pdev, const gs_image_common_t *pic,
                             const gx_drawing_color * pdcolor, image_union_t *image,
                             pdf_typed_image_context_t context)
{
    const gs_image_t *pim1 = (const gs_image_t *)pic;

    if (pim1->Alpha != gs_image_alpha_none)
        return -1;
    if (pim1->ImageMask) {
        /* If parameters are invalid, use the fallback implementation. */
        if (!(gx_dc_is_pattern1_color(pdcolor)))
            if (pim1->BitsPerComponent != 1 ||
                !((pim1->Decode[0] == 0.0 && pim1->Decode[1] == 1.0) ||
                  (pim1->Decode[0] == 1.0 && pim1->Decode[1] == 0.0))
                )
                return -1;
    }
    image[0].type1 = *pim1;
    /* If we can write in-line then make it so */
    return (context == PDF_IMAGE_DEFAULT &&
        can_write_image_in_line(pdev, pim1));
}

static int setup_type3_image(gx_device_pdf *pdev, const gs_imager_state * pis,
                      const gs_matrix *pmat, const gs_image_common_t *pic,
                      const gs_int_rect * prect,
                      const gx_drawing_color * pdcolor,
                      const gx_clip_path * pcpath, gs_memory_t * mem,
                      gx_image_enum_common_t ** pinfo,
                      image_union_t *image)
{
    const gs_image3_t *pim3 = (const gs_image3_t *)pic;
    gs_image3_t pim3a;
    const gs_image_common_t *pic1 = pic;
    gs_matrix m, mi;
    const gs_matrix *pmat1 = pmat;
    int code;

    if (pdev->CompatibilityLevel < 1.3 && !pdev->PatternImagemask) {
        if (pdf_must_put_clip_path(pdev, pcpath))
            code = pdf_unclip(pdev);
        else
            code = pdf_open_page(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
        code = pdf_put_clip_path(pdev, pcpath);
        if (code < 0)
            return code;
        gs_make_identity(&m);
        pmat1 = &m;
        m.tx = floor(pis->ctm.tx + 0.5); /* Round the origin against the image size distorsions */
        m.ty = floor(pis->ctm.ty + 0.5);
        pim3a = *pim3;
        gs_matrix_invert(&pim3a.ImageMatrix, &mi);
        gs_make_identity(&pim3a.ImageMatrix);
        if (pim3a.Width < pim3a.MaskDict.Width && pim3a.Width > 0) {
            int sx = (pim3a.MaskDict.Width + pim3a.Width - 1) / pim3a.Width;

            gs_matrix_scale(&mi, 1.0 / sx, 1, &mi);
            gs_matrix_scale(&pim3a.ImageMatrix, 1.0 / sx, 1, &pim3a.ImageMatrix);
        }
        if (pim3a.Height < pim3a.MaskDict.Height && pim3a.Height > 0) {
            int sy = (pim3a.MaskDict.Height + pim3a.Height - 1) / pim3a.Height;

            gs_matrix_scale(&mi, 1, 1.0 / sy, &mi);
            gs_matrix_scale(&pim3a.ImageMatrix, 1, 1.0 / sy, &pim3a.ImageMatrix);
        }
        gs_matrix_multiply(&mi, &pim3a.MaskDict.ImageMatrix, &pim3a.MaskDict.ImageMatrix);
        pic1 = (gs_image_common_t *)&pim3a;
        /* Setting pdev->converting_image_matrix to communicate with pdf_image3_make_mcde. */
        gs_matrix_multiply(&mi, &ctm_only(pis), &pdev->converting_image_matrix);
    }
    /*
     * We handle ImageType 3 images in a completely different way:
     * the default implementation sets up the enumerator.
     */
    return gx_begin_image3_generic((gx_device *)pdev, pis, pmat1, pic1,
                                   prect, pdcolor, pcpath, mem,
                                   pdf_image3_make_mid,
                                   pdf_image3_make_mcde, pinfo);
}

static int convert_type4_image(gx_device_pdf *pdev, const gs_imager_state * pis,
                      const gs_matrix *pmat, const gs_image_common_t *pic,
                      const gs_int_rect * prect,
                      const gx_drawing_color * pdcolor,
                      const gx_clip_path * pcpath, gs_memory_t * mem,
                      gx_image_enum_common_t ** pinfo,
                      pdf_typed_image_context_t context, image_union_t *image,
                      cos_dict_t *pnamed)
{
    /* Try to convert the image to a plain masked image. */
    gx_drawing_color icolor;
    int code;

    pdev->image_mask_is_SMask = false;
    if (pdf_convert_image4_to_image1(pdev, pis, pdcolor,
                                     (const gs_image4_t *)pic,
                                     &image[0].type1, &icolor) >= 0) {
        gs_state *pgs = (gs_state *)gx_hld_get_gstate_ptr(pis);

        if (pgs == NULL)
            return_error(gs_error_unregistered); /* Must not happen. */

        /* Undo the pop of the NI stack if necessary. */
        if (pnamed)
            cos_array_add_object(pdev->NI_stack, COS_OBJECT(pnamed));
        /* HACK: temporary patch the color space, to allow
           pdf_prepare_imagemask to write the right color for the imagemask. */
        code = gs_gsave(pgs);
        if (code < 0)
            return code;
        /* {csrc}: const cast warning */
        code = gs_setcolorspace(pgs, ((const gs_image4_t *)pic)->ColorSpace);
        if (code < 0)
            return code;
        code = pdf_begin_typed_image(pdev, pis, pmat,
                                     (gs_image_common_t *)&image[0].type1,
                                     prect, &icolor, pcpath, mem,
                                     pinfo, context);
        if (code < 0)
            return code;
        return gs_grestore(pgs);
    }
    return 1;
}

static int convert_type4_to_masked_image(gx_device_pdf *pdev, const gs_imager_state * pis,
                      const gs_image_common_t *pic,
                      const gs_int_rect * prect,
                      const gx_drawing_color * pdcolor,
                      const gx_clip_path * pcpath, gs_memory_t * mem,
                      gx_image_enum_common_t ** pinfo)
{
        gs_matrix m, m1, mi;
        gs_image4_t pi4 = *(const gs_image4_t *)pic;
        int code;
        pdf_lcvd_t *cvd = NULL;

        if (pdf_must_put_clip_path(pdev, pcpath))
            code = pdf_unclip(pdev);
        else
            code = pdf_open_page(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
        code = pdf_put_clip_path(pdev, pcpath);
        if (code < 0)
            return code;
        gs_make_identity(&m1);
        gs_matrix_invert(&pic->ImageMatrix, &mi);
        gs_matrix_multiply(&mi, &ctm_only(pis), &m);
        code = pdf_setup_masked_image_converter(pdev, mem, &m, &cvd,
                             true, 0, 0, pi4.Width, pi4.Height, false);
        if (code < 0)
            return code;
        cvd->mdev.is_open = true; /* fixme: same as above. */
        cvd->mask->is_open = true; /* fixme: same as above. */
        cvd->mask_is_empty = false;
        code = (*dev_proc(cvd->mask, fill_rectangle))((gx_device *)cvd->mask,
                    0, 0, cvd->mask->width, cvd->mask->height, (gx_color_index)0);
        if (code < 0)
            return code;
        gx_device_retain((gx_device *)cvd, true);
        gx_device_retain((gx_device *)cvd->mask, true);
        gs_make_identity(&pi4.ImageMatrix);
        code = gx_default_begin_typed_image((gx_device *)cvd,
            pis, &m1, (gs_image_common_t *)&pi4, prect, pdcolor, NULL, mem, pinfo);
        if (code < 0)
            return code;
        (*pinfo)->procs = &pdf_image_cvd_enum_procs;
        return 0;
}

static int setup_image_process_colorspace(gx_device_pdf *pdev, image_union_t *image, gs_color_space **pcs_orig,
                                          const char *sname, cos_value_t *cs_value)
{
    int code;
    gs_color_space *pcs_device = NULL;

    cos_c_string_value(cs_value, sname);
    *pcs_orig = image->pixel.ColorSpace;
    code = make_device_color_space(pdev, pdev->pcm_color_info_index, &pcs_device);
    if (code < 0)
        return code;
    image->pixel.ColorSpace = pcs_device;
    return 0;
}

/* 0 = write unchanged
   1 = convert to process
   2 = write as ICC
   3 = convert base space (Separation)
   4 = convert base space (DeviceN)
 */
static int setup_image_colorspace(gx_device_pdf *pdev, image_union_t *image, const gs_color_space *pcs, gs_color_space **pcs_orig,
                                  const pdf_color_space_names_t *names, cos_value_t *cs_value)
{
    int code=0;
    gs_color_space_index csi;
    gs_color_space_index csi2;
    const gs_color_space *pcs2 = pcs;

    csi = csi2 = gs_color_space_get_index(pcs);
    if (csi == gs_color_space_index_ICC) {
        csi2 = gsicc_get_default_type(pcs->cmm_icc_profile_data);
    }
    /* Figure out what to do if we are outputting to really ancient versions of PDF */
    /* NB ps2write sets CompatibilityLevel to 1.2 so we cater for it here */
    if (pdev->CompatibilityLevel <= 1.2) {

        /* If we have an /Indexed space, we need to look at the base space */
        if (csi2 == gs_color_space_index_Indexed) {
            pcs2 = pcs->base_space;
            csi2 = gs_color_space_get_index(pcs2);
        }

        switch (csi2) {
            case gs_color_space_index_DeviceGray:
                if (pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged ||
                    pdev->params.ColorConversionStrategy == ccs_Gray) {
                    return 0;
                }
                else {
                    code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceGray, cs_value);
                    if (code < 0)
                        return code;
                    return 1;
                }
                break;
            case gs_color_space_index_DeviceRGB:
                if (pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged ||
                    pdev->params.ColorConversionStrategy == ccs_RGB)
                    return 0;
                else {
                    code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceRGB, cs_value);
                    if (code < 0)
                        return code;
                    return 1;
                }
                break;
            case gs_color_space_index_DeviceCMYK:
                if ((pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged ||
                    pdev->params.ColorConversionStrategy == ccs_CMYK) && !pdev->params.ConvertCMYKImagesToRGB)
                    return 0;
                else {
                    code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceCMYK, cs_value);
                    if (code < 0)
                        return code;
                    return 1;
                }
                break;
            case gs_color_space_index_CIEA:
            case gs_color_space_index_CIEABC:
            case gs_color_space_index_CIEDEF:
            case gs_color_space_index_CIEDEFG:
            case gs_color_space_index_Separation:
                if (pdev->ForOPDFRead) {
                    switch (pdev->params.ColorConversionStrategy) {
                        case ccs_ByObjectType:
                            /* Object type not implemented yet */
                        case ccs_UseDeviceDependentColor:
                            /* DeviceDependentColor deprecated */
                        case ccs_UseDeviceIndependentColorForImages:
                            /* If only correcting images, then leave unchanged */
                        case ccs_LeaveColorUnchanged:
                            if (csi2 == gs_color_space_index_Separation)
                                return 0;
                            /* Fall through and convert CIE to the device space */
                        default:
                            switch (pdev->pcm_color_info_index) {
                                case gs_color_space_index_DeviceGray:
                                    code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceGray, cs_value);
                                    break;
                                case gs_color_space_index_DeviceRGB:
                                    code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceRGB, cs_value);
                                    break;
                                case gs_color_space_index_DeviceCMYK:
                                    code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceCMYK, cs_value);
                                    break;
                                default:
                                    emprintf(pdev->memory, "Unsupported ProcessColorModel.");
                                    return_error(gs_error_undefined);
                            }
                            if (code < 0)
                                return code;
                            return 1;
                            break;
                    }
                }
                else
                    return 1;
                break;

            case gs_color_space_index_ICC:
                /* Note that if csi is ICC, check to see if this was one of
                   the default substitutes that we introduced for DeviceGray,
                   DeviceRGB or DeviceCMYK.  If it is, then just write
                   the default color.  Depending upon the flavor of PDF,
                   or other options, we may want to actually have all
                   the colors defined by ICC profiles and not do the following
                   substituion of the Device space. */
                csi2 = gsicc_get_default_type(pcs2->cmm_icc_profile_data);

                switch (csi2) {
                    case gs_color_space_index_DeviceGray:
                        if (pdev->params.ColorConversionStrategy == ccs_Gray ||
                            pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged)
                            return 0;
                        break;
                    case gs_color_space_index_DeviceRGB:
                        if (pdev->params.ColorConversionStrategy == ccs_RGB ||
                            pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged)
                            return 0;
                        break;
                    case gs_color_space_index_DeviceCMYK:
                        if (pdev->params.ColorConversionStrategy == ccs_CMYK ||
                            pdev->params.ColorConversionStrategy == ccs_LeaveColorUnchanged)
                            return 0;
                        break;
                    default:
                        break;
                }
                /* Fall through for non-handled cases */
            case gs_color_space_index_DeviceN:
            case gs_color_space_index_DevicePixel:
            case gs_color_space_index_Indexed:
                switch (pdev->pcm_color_info_index) {
                    case gs_color_space_index_DeviceGray:
                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceGray, cs_value);
                        break;
                    case gs_color_space_index_DeviceRGB:
                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceRGB, cs_value);
                        break;
                    case gs_color_space_index_DeviceCMYK:
                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceCMYK, cs_value);
                        break;
                    default:
                        emprintf(pdev->memory, "Unsupported ProcessColorModel.");
                        return_error(gs_error_undefined);
                }
                if (code < 0)
                    return code;
                return 1;
                break;
            default:
                return (gs_note_error(gs_error_rangecheck));
                break;
        }
    } else {
        switch(pdev->params.ColorConversionStrategy) {
            case ccs_ByObjectType:
                /* Object type not implemented yet */
            case ccs_UseDeviceDependentColor:
                /* DeviceDependentCOlor deprecated */
            case ccs_UseDeviceIndependentColorForImages:
                /* If only correcting images, then leave unchanged */
            case ccs_LeaveColorUnchanged:
                return 0;
                break;
            case ccs_sRGB:
            case ccs_UseDeviceIndependentColor:
                return 2;
                break;
            case ccs_CMYK:
                switch(csi2) {
                    case gs_color_space_index_DeviceGray:
                    case gs_color_space_index_DeviceCMYK:
                        return 0;
                        break;
                    case gs_color_space_index_Separation:
                        pcs2 = pcs;
                        while (pcs2->base_space)
                            pcs2 = pcs2->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        if (csi == gs_color_space_index_DeviceCMYK)
                            return 0;
                        else
                            return 3;
                        break;
                    case gs_color_space_index_DeviceN:
                        pcs2 = pcs;
                        while (pcs2->base_space)
                            pcs2 = pcs2->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        if (csi == gs_color_space_index_DeviceCMYK)
                            return 0;
                        else
                            return 4;
                        break;
                    case gs_color_space_index_Indexed:
                        pcs2 = pcs->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        switch(csi) {
                            case gs_color_space_index_DeviceGray:
                            case gs_color_space_index_DeviceCMYK:
                                return 0;
                                break;
                            case gs_color_space_index_Separation:
                                pcs2 = pcs;
                                while (pcs2->base_space)
                                    pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi == gs_color_space_index_DeviceCMYK)
                                    return 0;
                                else
                                    return 3;
                                break;
                            case gs_color_space_index_DeviceN:
                                pcs2 = pcs;
                                while (pcs2->base_space)
                                    pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi == gs_color_space_index_DeviceCMYK)
                                    return 0;
                                else
                                    return 4;
                                break;
                            default:
                                switch (pdev->pcm_color_info_index) {
                                    case gs_color_space_index_DeviceGray:
                                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceGray, cs_value);
                                        break;
                                    case gs_color_space_index_DeviceRGB:
                                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceRGB, cs_value);
                                        break;
                                    case gs_color_space_index_DeviceCMYK:
                                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceCMYK, cs_value);
                                        break;
                                    default:
                                        emprintf(pdev->memory, "Unsupported ProcessColorModel.");
                                        return_error(gs_error_undefined);
                                }
                                if (code < 0)
                                    return code;
                                return 1;
                                break;
                        }
                        break;
                    default:
                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceCMYK, cs_value);
                        if (code < 0)
                            return code;
                        return 1;
                        break;
                }
                break;
            case ccs_Gray:
                switch(csi2) {
                    case gs_color_space_index_DeviceGray:
                        return 0;
                        break;
                    case gs_color_space_index_Separation:
                        pcs2 = pcs;
                        while (pcs2->base_space)
                            pcs2 = pcs2->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        if (csi == gs_color_space_index_DeviceGray)
                            return 0;
                        else
                            return 3;
                        break;
                    case gs_color_space_index_DeviceN:
                        pcs2 = pcs;
                        while (pcs2->base_space)
                            pcs2 = pcs2->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        if (csi == gs_color_space_index_DeviceGray)
                            return 0;
                        else
                            return 4;
                        break;
                    case gs_color_space_index_Indexed:
                        pcs2 = pcs->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        switch(csi) {
                            case gs_color_space_index_DeviceGray:
                                return 0;
                                break;
                            case gs_color_space_index_Separation:
                                pcs2 = pcs;
                                while (pcs2->base_space)
                                    pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi == gs_color_space_index_DeviceGray)
                                    return 0;
                                else
                                    return 3;
                                break;
                            case gs_color_space_index_DeviceN:
                                pcs2 = pcs;
                                while (pcs2->base_space)
                                    pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi == gs_color_space_index_DeviceGray)
                                    return 0;
                                else
                                    return 4;
                                break;
                            default:
                                code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceGray, cs_value);
                                if (code < 0)
                                    return code;
                                return 1;
                                break;
                        }
                        break;
                    default:
                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceGray, cs_value);
                        if (code < 0)
                            return code;
                        return 1;
                        break;
                }
                break;
            case ccs_RGB:
                switch(csi2) {
                    case gs_color_space_index_DeviceGray:
                    case gs_color_space_index_DeviceRGB:
                        return 0;
                        break;
                    case gs_color_space_index_Separation:
                        pcs2 = pcs;
                        while (pcs2->base_space)
                            pcs2 = pcs2->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        if (csi == gs_color_space_index_DeviceRGB)
                            return 0;
                        else
                            return 3;
                        break;
                    case gs_color_space_index_DeviceN:
                        pcs2 = pcs;
                        while (pcs2->base_space)
                            pcs2 = pcs2->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        if (csi == gs_color_space_index_DeviceRGB)
                            return 0;
                        else
                            return 4;
                        break;
                    case gs_color_space_index_Indexed:
                        pcs2 = pcs->base_space;
                        csi = gs_color_space_get_index(pcs2);
                        if (csi == gs_color_space_index_ICC)
                            csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                        switch(csi) {
                            case gs_color_space_index_DeviceGray:
                            case gs_color_space_index_DeviceRGB:
                                return 0;
                                break;
                            case gs_color_space_index_Separation:
                                pcs2 = pcs;
                                while (pcs2->base_space)
                                    pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi == gs_color_space_index_DeviceRGB)
                                    return 0;
                                else
                                    return 3;
                                break;
                            case gs_color_space_index_DeviceN:
                                pcs2 = pcs;
                                while (pcs2->base_space)
                                    pcs2 = pcs2->base_space;
                                csi = gs_color_space_get_index(pcs2);
                                if (csi == gs_color_space_index_ICC)
                                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                                if (csi == gs_color_space_index_DeviceRGB)
                                    return 0;
                                else
                                    return 4;
                                break;
                            default:
                                code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceRGB, cs_value);
                                if (code < 0)
                                    return code;
                                return 1;
                                break;
                        }
                        break;
                    default:
                        code = setup_image_process_colorspace(pdev, image, pcs_orig, names->DeviceRGB, cs_value);
                        if (code < 0)
                            return code;
                        return 1;
                        break;
                }
                break;
            default:
                break;
        }
    }
    return 0;
}

static int
new_pdf_begin_typed_image(gx_device_pdf *pdev, const gs_imager_state * pis,
                      const gs_matrix *pmat, const gs_image_common_t *pic,
                      const gs_int_rect * prect,
                      const gx_drawing_color * pdcolor,
                      const gx_clip_path * pcpath, gs_memory_t * mem,
                      gx_image_enum_common_t ** pinfo,
                      pdf_typed_image_context_t context)
{
    int code, i;
    unsigned int use_fallback  = 0, in_line = 0, is_mask = 0,
        force_lossless = 0, convert_to_process_colors = 0, reduce_bits = 0;
    int width, height;
    cos_dict_t *pnamed = 0;
    image_union_t *image;
    const gs_pixel_image_t *pim;
    gs_int_rect rect;
    gs_image_format_t format;
    const gs_color_space *pcs;
    int num_components;
    pdf_image_enum *pie;
    const pdf_color_space_names_t *names;
    gs_color_space *pcs_orig = NULL;
    gs_color_space *pcs_device = NULL;
    cos_value_t cs_value;
    const gs_range_t *pranges = 0;

    image = (image_union_t *)gs_malloc(mem->non_gc_memory, 4,
                       sizeof(image_union_t), "pdf_begin_typed_image(image)");
    if (image == 0)
        return_error(gs_error_VMerror);

    /*
     * Pop the image name from the NI stack.  We must do this, to keep the
     * stack in sync, even if it turns out we can't handle the image.
     */
    {
        cos_value_t ni_value;

        if (cos_array_unadd(pdev->NI_stack, &ni_value) >= 0)
            pnamed = (cos_dict_t *)ni_value.contents.object;
    }

    /* An initialization for pdf_end_and_do_image :
       We need to delay adding the "Mask" entry into a type 3 image dictionary
       until the mask is completed due to equal image merging. */
    pdev->image_mask_id = gs_no_id;

    /* Check for the image types we can handle. */
    switch (pic->type->index) {
    case 1:
        is_mask = ((const gs_image_t *)pic)->ImageMask;
        code = setup_type1_image(pdev, pic, pdcolor, image, context);
        if (code < 0) {
            use_fallback = 1;
        }
        else
            in_line = code;
        break;

    case 3:
        pdev->image_mask_is_SMask = false;
        if (pdev->CompatibilityLevel < 1.2 ||
            (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
                   prect->q.x == ((const gs_image3_t *)pic)->Width &&
                   prect->q.y == ((const gs_image3_t *)pic)->Height))) {
            gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
            return (gx_default_begin_typed_image((gx_device *)pdev, pis, pmat, pic, prect, pdcolor,
                pcpath, mem, pinfo));
        }
        gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                          "pdf_begin_typed_image(image)");
        return (setup_type3_image(pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem, pinfo, image));
        break;

    case IMAGE3X_IMAGETYPE:
        gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
        if (pdev->CompatibilityLevel < 1.4 ||
            (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
                       prect->q.x == ((const gs_image3x_t *)pic)->Width &&
                       prect->q.y == ((const gs_image3x_t *)pic)->Height))) {
            return (gx_default_begin_typed_image((gx_device *)pdev, pis, pmat, pic, prect, pdcolor,
                pcpath, mem, pinfo));
        }
        pdev->image_mask_is_SMask = true;
        return gx_begin_image3x_generic((gx_device *)pdev, pis, pmat, pic,
                                        prect, pdcolor, pcpath, mem,
                                        pdf_image3x_make_mid,
                                        pdf_image3x_make_mcde, pinfo);
        break;

    case 4:
        code = convert_type4_image(pdev, pis, pmat, pic, prect, pdcolor,
                      pcpath, mem, pinfo, context, image, pnamed);
        if (code < 0) {
            use_fallback = 1;
        }
        if (code == 0) {
            gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                                  "pdf_begin_typed_image(image)");
            return code;
        }
        /* No luck.  Masked images require PDF 1.3 or higher. */
        if (pdev->CompatibilityLevel < 1.2) {
            use_fallback = 1;
        }
        if (pdev->CompatibilityLevel < 1.3 && !pdev->PatternImagemask) {
            gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                                  "pdf_begin_typed_image(image)");
            return (convert_type4_to_masked_image(pdev, pis, pic, prect, pdcolor,
                      pcpath, mem,pinfo));
        }
        image[0].type4 = *(const gs_image4_t *)pic;
        break;

    default:
        use_fallback = 1;
        break;
    }

    pim = (const gs_pixel_image_t *)pic;
    format = pim->format;
    switch (format) {
    case gs_image_format_chunky:
    case gs_image_format_component_planar:
        break;
    default:
        use_fallback = 1;
    }
    /* AR5 on Windows doesn't support 0-size images. Skipping. */
    if (pim->Width == 0 || pim->Height == 0)
        use_fallback = 1;
    /* PDF doesn't support images with more than 8 bits per component. */
    switch (pim->BitsPerComponent) {
        case 1:
        case 2:
        case 4:
        case 8:
            break;
        case 12:
        case 16:
            use_fallback = 1;
//            reduce_bits = pim->BitsPerComponent;
            break;
        default:
            gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
            return_error(gs_error_rangecheck);
    }
    if (prect)
        rect = *prect;
    else {
        rect.p.x = rect.p.y = 0;
        rect.q.x = pim->Width, rect.q.y = pim->Height;
    }
    if (rect.p.x != 0 || rect.p.y != 0 ||
        rect.q.x != pim->Width || rect.q.y != pim->Height ||
        (is_mask && pim->CombineWithColor))
        use_fallback = 1;

    if (use_fallback) {
        gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
        return gx_default_begin_typed_image
            ((gx_device *)pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
            pinfo);
    }

    pcs = pim->ColorSpace;
    num_components = (is_mask ? 1 : gs_color_space_num_components(pcs));

    if (pdf_must_put_clip_path(pdev, pcpath))
        code = pdf_unclip(pdev);
    else
        code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0) {
        gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
        return gx_default_begin_typed_image
            ((gx_device *)pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
            pinfo);
    }

    if (context == PDF_IMAGE_TYPE3_MASK) {
        /*
         * The soft mask for an ImageType 3x image uses a DevicePixel
         * color space, which pdf_color_space() can't handle.  Patch it
         * to DeviceGray here.
         */
        /* {csrc} make sure this gets freed */
        pcs = gs_cspace_new_DeviceGray(pdev->memory);
    } else if (is_mask)
        code = pdf_prepare_imagemask(pdev, pis, pdcolor);
    else
        code = pdf_prepare_image(pdev, pis);
    if (code < 0) {
        gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
        return gx_default_begin_typed_image
            ((gx_device *)pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
            pinfo);
    }

    pie = gs_alloc_struct(mem, pdf_image_enum, &st_pdf_image_enum,
                        "pdf_begin_image");
    if (pie == 0) {
        gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
        return_error(gs_error_VMerror);
    }
    memset(pie, 0, sizeof(*pie)); /* cleanup entirely for GC to work in all cases. */
    *pinfo = (gx_image_enum_common_t *) pie;
    gx_image_enum_common_init(*pinfo, (const gs_data_image_t *) pim,
                    ((pdev->CompatibilityLevel >= 1.3) ?
                            (context == PDF_IMAGE_TYPE3_MASK ?
                            &pdf_image_object_enum_procs :
                            &pdf_image_enum_procs) :
                            context == PDF_IMAGE_TYPE3_MASK ?
                            &pdf_image_object_enum_procs :
                            context == PDF_IMAGE_TYPE3_DATA ?
                            &pdf_image_object_enum_procs2 :
                            &pdf_image_enum_procs),
                        (gx_device *)pdev, num_components, format);
    pie->memory = mem;
    width = rect.q.x - rect.p.x;
    pie->width = width;
    height = rect.q.y - rect.p.y;
    pie->bits_per_pixel =
        (reduce_bits ? reduce_bits : pim->BitsPerComponent) * num_components / pie->num_planes;
    pie->rows_left = height;
    if (pnamed != 0) /* Don't in-line the image if it is named. */
        in_line = false;
    else {
        double nbytes = (double)(((ulong) pie->width * pie->bits_per_pixel + 7) >> 3) *
            pie->num_planes * pie->rows_left;

        in_line &= (nbytes < pdev->MaxInlineImageSize);
    }

    if (pmat == 0)
        pmat = &ctm_only(pis);
    {
        gs_matrix mat;
        gs_matrix bmat;
        int code;

        pdf_make_bitmap_matrix(&bmat, -rect.p.x, -rect.p.y,
                               pim->Width, pim->Height, height);
        if ((code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
            (code = gs_matrix_multiply(&bmat, &mat, &mat)) < 0 ||
            (code = gs_matrix_multiply(&mat, pmat, &pie->mat)) < 0
            ) {
            gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
            gs_free_object(mem, pie, "pdf_begin_image");
            return code;
        }
        /* AR3,AR4 show no image when CTM is singular; AR5 reports an error */
        if (pie->mat.xx * pie->mat.yy == pie->mat.xy * pie->mat.yx)
            goto fail_and_fallback;
    }

    code = pdf_put_clip_path(pdev, pcpath);
    if (code < 0) {
        gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                          "pdf_begin_typed_image(image)");
        gs_free_object(mem, pie, "pdf_begin_image");
        return code;
    }
    pdf_image_writer_init(&pie->writer);
    /* Note : Possible values for alt_writer_count are 1,2,3,4.
       1 means no alternative streams.
       2 means the main image stream and a mask stream while converting
               an Image Type 4.
       3 means the main image stream, alternative image compression stream,
               and the compression chooser.
       4 meams 3 and a mask stream while convertingh an Image Type 4.
     */
    pie->writer.alt_writer_count = (in_line ||
                                    (pim->Width <= 64 && pim->Height <= 64)
                                    ? 1 : 2);
    if ((image[0].pixel.ColorSpace != NULL &&
        image[0].pixel.ColorSpace->type->index == gs_color_space_index_Indexed
        && pdev->params.ColorImage.DownsampleType != ds_Subsample) ||
        pdev->transfer_not_identity)
        force_lossless = true;

    names = (in_line ? &pdf_color_space_names_short : &pdf_color_space_names);

    /* We don't want to change the colour space of a mask, or an SMask (both of which are Gray) */
    if (!is_mask) {
        if (image[0].pixel.ColorSpace != NULL && !(context == PDF_IMAGE_TYPE3_MASK))
            convert_to_process_colors = setup_image_colorspace(pdev, &image[0], pcs, &pcs_orig, names, &cs_value);
        if (convert_to_process_colors == 4) {
            code = convert_DeviceN_alternate(pdev, pis, pcs, NULL, NULL, NULL, NULL, &cs_value, in_line);
            if (code < 0)
                goto fail_and_fallback;
        }
        if (convert_to_process_colors == 3) {
            code = convert_separation_alternate(pdev, pis, pcs, NULL, NULL, NULL, NULL, &cs_value, in_line);
            if (code < 0)
                goto fail_and_fallback;
        }
        if (convert_to_process_colors == 1) {
            code = make_device_color_space(pdev, pdev->pcm_color_info_index, &pcs_device);
            if (code < 0)
                goto fail_and_fallback;
            image[0].pixel.ColorSpace = pcs_device;
            code = pdf_color_space_named(pdev, &cs_value, &pranges, pcs_device, names,
                                     in_line, NULL, 0);
            if (code < 0)
                goto fail_and_fallback;
        } else {
            convert_to_process_colors = 0;
            code = pdf_color_space_named(pdev, &cs_value, &pranges, pcs, names,
                                     in_line, NULL, 0);
            if (code < 0)
                goto fail_and_fallback;
        }
    }

    image[1] = image[0];

    pdev->ParamCompatibilityLevel = pdev->CompatibilityLevel;

    code = pdf_begin_write_image(pdev, &pie->writer, gs_no_id, width,
                    height, pnamed, in_line);
    if (code < 0)
        goto fail_and_fallback;

    /* Code below here deals with setting up the multiple data stream writing.
     * We can have up to 4 stream writers, which we keep in an array. We must
     * always have at least one which writes the uncompressed stream. If we
     * are writing compressed streams, we have one for the compressed stream
     * and one for the compression chooser.
     * For type 4 images being converted (for old versions of PDF or for ps2write)
     * we need an additional stream to write a mask, which masks the real
     * image.
     * For colour conversion we will place an additional filter in front of all
     * the streams which does the conversion.
     */
    if (in_line) {
        code = new_setup_lossless_filters((gx_device_psdf *) pdev,
                                             &pie->writer.binary[0],
                                             &image[0].pixel, in_line, convert_to_process_colors);
    } else {
        if (force_lossless) {
            /*
             * Some regrettable PostScript code (such as LanguageLevel 1 output
             * from Microsoft's PSCRIPT.DLL driver) misuses the transfer
             * function to accomplish the equivalent of indexed color.
             * Downsampling (well, only averaging) or JPEG compression are not
             * compatible with this.  Play it safe by using only lossless
             * filters if the transfer function(s) is/are other than the
             * identity and by setting the downsample type to Subsample..
             */
            int saved_downsample = pdev->params.ColorImage.DownsampleType;

            pdev->params.ColorImage.DownsampleType = ds_Subsample;
            code = new_setup_image_filters((gx_device_psdf *) pdev,
                                          &pie->writer.binary[0], &image[0].pixel,
                                          pmat, pis, true, in_line, convert_to_process_colors);
            pdev->params.ColorImage.DownsampleType = saved_downsample;
        } else {
            code = new_setup_image_filters((gx_device_psdf *) pdev,
                                          &pie->writer.binary[0], &image[0].pixel,
                                          pmat, pis, true, in_line, convert_to_process_colors);
        }
    }

    if (code < 0)
        goto fail_and_fallback;

    if (convert_to_process_colors) {
        image[0].pixel.ColorSpace = pcs_orig;
        code = psdf_setup_image_colors_filter(&pie->writer.binary[0],
                    (gx_device_psdf *)pdev, &image[0].pixel, pis);
        if (code < 0)
            goto fail_and_fallback;
        image[0].pixel.ColorSpace = pcs_device;
    }

    if (reduce_bits) {
        code = new_resize_input(&pie->writer.binary[0], pim->Width, gs_color_space_num_components(pim->ColorSpace), reduce_bits, 8);
        if (code < 0)
            goto fail_and_fallback;
    }

    if (pie->writer.alt_writer_count > 1) {
        code = pdf_make_alt_stream(pdev, &pie->writer.binary[1]);
        if (code) {
            goto fail_and_fallback;
        }
        code = new_setup_image_filters((gx_device_psdf *) pdev,
                                  &pie->writer.binary[1], &image[1].pixel,
                                  pmat, pis, force_lossless, in_line, convert_to_process_colors);
        if (code == gs_error_rangecheck) {

            for (i=1;i < pie->writer.alt_writer_count; i++) {
                stream *s = pie->writer.binary[i].strm;
                cos_stream_t *pcos = cos_stream_from_pipeline(pie->writer.binary[i].strm);
                s_close_filters(&s, NULL);
                gs_free_object(pdev->pdf_memory, s, "compressed image stream");
                pcos->cos_procs->release((cos_object_t *)pcos, "pdf_begin_typed_image_impl");
                gs_free_object(pdev->pdf_memory, pcos, "compressed image cos_stream");
            }
            /* setup_image_compression rejected the alternative compression. */
            pie->writer.alt_writer_count = 1;
            memset(pie->writer.binary + 1, 0, sizeof(pie->writer.binary[1]));
            memset(pie->writer.binary + 2, 0, sizeof(pie->writer.binary[1]));
        } else if (code) {
            goto fail_and_fallback;
        } else if (convert_to_process_colors) {
            image[1].pixel.ColorSpace = pcs_orig;
            code = psdf_setup_image_colors_filter(&pie->writer.binary[1],
                    (gx_device_psdf *)pdev, &image[1].pixel, pis);
            if (code < 0) {
                goto fail_and_fallback;
            }
            image[1].pixel.ColorSpace = pcs_device;
        }
        if (reduce_bits) {
            code = new_resize_input(&pie->writer.binary[0], pim->Width, gs_color_space_num_components(pim->ColorSpace), reduce_bits, 8);
            if (code < 0)
                goto fail_and_fallback;
        }
    }

    for (i = 0; i < pie->writer.alt_writer_count; i++) {
        code = pdf_begin_image_data_decoded(pdev, num_components, pranges, i,
                             &image[i].pixel, &cs_value, pie);
        if (code < 0)
            goto fail_and_fallback;
    }
    if (pie->writer.alt_writer_count == 2) {
        psdf_setup_compression_chooser(&pie->writer.binary[2],
             (gx_device_psdf *)pdev, pim->Width, pim->Height,
             num_components, pim->BitsPerComponent);
        pie->writer.alt_writer_count = 3;
    }
    if (pic->type->index == 4 && pdev->CompatibilityLevel < 1.3) {
        int i;

        /* Create a stream for writing the mask. */
        i = pie->writer.alt_writer_count;
        gs_image_t_init_mask_adjust((gs_image_t *)&image[i].type1, true, false);
        image[i].type1.Width = image[0].pixel.Width;
        image[i].type1.Height = image[0].pixel.Height;
        /* Won't use image[2]. */
        code = pdf_begin_write_image(pdev, &pie->writer, gs_no_id, width,
                    height, NULL, false);
        if (code)
            goto fail_and_fallback;
        code = psdf_setup_image_filters((gx_device_psdf *) pdev,
                                  &pie->writer.binary[i], &image[i].pixel,
                                  pmat, pis, force_lossless, in_line);
        if (code < 0)
            goto fail_and_fallback;
        psdf_setup_image_to_mask_filter(&pie->writer.binary[i],
             (gx_device_psdf *)pdev, pim->Width, pim->Height,
             num_components, pim->BitsPerComponent, image[i].type4.MaskColor);
        code = pdf_begin_image_data_decoded(pdev, num_components, pranges, i,
                             &image[i].pixel, &cs_value, pie);
        if (code < 0)
            goto fail_and_fallback;
        ++pie->writer.alt_writer_count;
    }

    gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                              "pdf_begin_typed_image(image)");
    return 0;

fail_and_fallback:
    gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                      "pdf_begin_typed_image(image)");
    gs_free_object(mem, pie, "pdf_begin_image");
    return gx_default_begin_typed_image
        ((gx_device *)pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
        pinfo);
}

static int pdf_begin_typed_image(gx_device_pdf *pdev,
    const gs_imager_state * pis, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect * prect,
    const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
    gs_memory_t * mem, gx_image_enum_common_t ** pinfo,
    pdf_typed_image_context_t context)
{
    if (!pdev->UseOldColor) {
    return new_pdf_begin_typed_image(pdev, pis, pmat, pic, prect,
                                 pdcolor, pcpath, mem, pinfo,
                                 context);
    } else {
    return old_pdf_begin_typed_image(pdev, pis, pmat, pic, prect,
                                 pdcolor, pcpath, mem, pinfo,
                                 context);
    }
}

int
gdev_pdf_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
                           const gs_matrix *pmat, const gs_image_common_t *pic,
                           const gs_int_rect * prect,
                           const gx_drawing_color * pdcolor,
                           const gx_clip_path * pcpath, gs_memory_t * mem,
                           gx_image_enum_common_t ** pinfo)
{
    return pdf_begin_typed_image((gx_device_pdf *)dev, pis, pmat, pic, prect,
                                 pdcolor, pcpath, mem, pinfo,
                                 PDF_IMAGE_DEFAULT);
}

/* ---------------- All images ---------------- */

/* Process the next piece of an image. */
static int
pdf_image_plane_data_alt(gx_image_enum_common_t * info,
                     const gx_image_plane_t * planes, int height,
                     int *rows_used, int alt_writer_index)
{
    pdf_image_enum *pie = (pdf_image_enum *) info;
    int h = height;
    int y;
    /****** DOESN'T HANDLE IMAGES WITH VARYING WIDTH PER PLANE ******/
    uint width_bits = pie->width * pie->plane_depths[0];
    /****** DOESN'T HANDLE NON-ZERO data_x CORRECTLY ******/
    uint bcount = (width_bits + 7) >> 3;
    uint ignore;
    int nplanes = pie->num_planes;
    int status = 0;

    if (h > pie->rows_left)
        h = pie->rows_left;
    for (y = 0; y < h; ++y) {
        if (nplanes > 1) {
            /*
             * We flip images in blocks, and each block except the last one
             * must contain an integral number of pixels.  The easiest way
             * to meet this condition is for all blocks except the last to
             * be a multiple of 3 source bytes (guaranteeing an integral
             * number of 1/2/4/8/12-bit samples), i.e., 3*nplanes flipped
             * bytes.  This requires a buffer of at least
             * 3*GS_IMAGE_MAX_COMPONENTS bytes.
             */
            int pi;
            uint count = bcount;
            uint offset = 0;
#define ROW_BYTES max(200 /*arbitrary*/, 3 * GS_IMAGE_MAX_COMPONENTS)
            const byte *bit_planes[GS_IMAGE_MAX_COMPONENTS];
            int block_bytes = ROW_BYTES / (3 * nplanes) * 3;
            byte row[ROW_BYTES];

            for (pi = 0; pi < nplanes; ++pi)
                bit_planes[pi] = planes[pi].data + planes[pi].raster * y;
            while (count) {
                uint flip_count;
                uint flipped_count;

                if (count >= block_bytes) {
                    flip_count = block_bytes;
                    flipped_count = block_bytes * nplanes;
                } else {
                    flip_count = count;
                    flipped_count =
                        (width_bits % (block_bytes * 8) * nplanes + 7) >> 3;
                }
                image_flip_planes(row, bit_planes, offset, flip_count,
                                  nplanes, pie->plane_depths[0]);
                status = sputs(pie->writer.binary[alt_writer_index].strm, row,
                               flipped_count, &ignore);
                if (status < 0)
                    break;
                offset += flip_count;
                count -= flip_count;
            }
        } else {
            status = sputs(pie->writer.binary[alt_writer_index].strm,
                           planes[0].data + planes[0].raster * y, bcount,
                           &ignore);
        }
        if (status < 0)
            break;
    }
    *rows_used = h;
    if (status < 0)
        return_error(gs_error_ioerror);
    return !pie->rows_left;
#undef ROW_BYTES
}

static int
pdf_image_plane_data(gx_image_enum_common_t * info,
                     const gx_image_plane_t * planes, int height,
                     int *rows_used)
{
    pdf_image_enum *pie = (pdf_image_enum *) info;
    int i;
    for (i = 0; i < pie->writer.alt_writer_count; i++) {
        int code = pdf_image_plane_data_alt(info, planes, height, rows_used, i);
        if (code)
            return code;
    }
    pie->rows_left -= *rows_used;
    if (pie->writer.alt_writer_count > 2)
        pdf_choose_compression(&pie->writer, false);
    return !pie->rows_left;
}

static int
use_image_as_pattern(gx_device_pdf *pdev, pdf_resource_t *pres1,
                     const gs_matrix *pmat, gs_id id)
{   /* See also dump_image in gdevpdfd.c . */
    gs_imager_state s;
    gs_pattern1_instance_t inst;
    cos_value_t v;
    const pdf_resource_t *pres;
    int code;

    memset(&s, 0, sizeof(s));
    s.ctm.xx = pmat->xx;
    s.ctm.xy = pmat->xy;
    s.ctm.yx = pmat->yx;
    s.ctm.yy = pmat->yy;
    s.ctm.tx = pmat->tx;
    s.ctm.ty = pmat->ty;
    memset(&inst, 0, sizeof(inst));
    inst.saved = (gs_state *)&s; /* HACK : will use s.ctm only. */
    inst.templat.PaintType = 1;
    inst.templat.TilingType = 1;
    inst.templat.BBox.p.x = inst.templat.BBox.p.y = 0;
    inst.templat.BBox.q.x = 1;
    inst.templat.BBox.q.y = 1;
    inst.templat.XStep = 2; /* Set 2 times bigger step against artifacts. */
    inst.templat.YStep = 2;
    code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
        gxdso_pattern_start_accum, &inst, id);
    if (code >= 0)
        pprintld1(pdev->strm, "/R%ld Do\n", pdf_resource_id(pres1));
    pres = pdev->accumulating_substream_resource;
    if (code >= 0)
        code = pdf_add_resource(pdev, pdev->substream_Resources, "/XObject", pres1);
    if (code >= 0)
        code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
            gxdso_pattern_finish_accum, &inst, id);
    if (code >= 0)
        code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
            gxdso_pattern_load, &inst, id);
    if (code >= 0) {
        stream_puts(pdev->strm, "q ");
        code = pdf_cs_Pattern_colored(pdev, &v);
    }
    if (code >= 0) {
        cos_value_write(&v, pdev);
        pprintld1(pdev->strm, " cs /R%ld scn ", pdf_resource_id(pres));
    }
    if (code >= 0) {
        /* The image offset weas broken in gx_begin_image3_generic,
           (see 'origin' in there).
           As a temporary hack use the offset of the image.
           fixme : This isn't generally correct,
           because the mask may be "transpozed" against the image. */
        gs_matrix m = pdev->converting_image_matrix;

        m.tx = pmat->tx;
        m.ty = pmat->ty;
        code = pdf_do_image_by_id(pdev, pdev->image_mask_scale,
             &m, true, pdev->image_mask_id);
        stream_puts(pdev->strm, "Q\n");
    }
    return code;
}

typedef enum {
    USE_AS_MASK,
    USE_AS_IMAGE,
    USE_AS_PATTERN
} pdf_image_usage_t;

/* Close PDF image and do it. */
static int
pdf_end_and_do_image(gx_device_pdf *pdev, pdf_image_writer *piw,
                     const gs_matrix *mat, gs_id ps_bitmap_id, pdf_image_usage_t do_image)
{
    int code = pdf_end_write_image(pdev, piw);
    pdf_resource_t *pres = piw->pres;

    switch (code) {
    default:
        return code;	/* error */
    case 1:
        code = 0;
        break;
    case 0:
        if (do_image == USE_AS_IMAGE) {
            if (pdev->image_mask_id != gs_no_id) {
                char buf[20];

                gs_sprintf(buf, "%ld 0 R", pdev->image_mask_id);
                code = cos_dict_put_string_copy((cos_dict_t *)pres->object,
                        pdev->image_mask_is_SMask ? "/SMask" : "/Mask", buf);
                if (code < 0)
                    return code;
            }
            if (pdev->image_mask_skip)
                code = 0;
            else
                code = pdf_do_image(pdev, pres, mat, true);
        } else if (do_image == USE_AS_MASK) {
            /* Provide data for pdf_do_image_by_id, which will be called through
                use_image_as_pattern during the next call to this function.
                See pdf_do_image about the meaning of 'scale'. */
            const pdf_x_object_t *const pxo = (const pdf_x_object_t *)pres;

            pdev->image_mask_scale = (double)pxo->data_height / pxo->height;
            pdev->image_mask_id = pdf_resource_id(pres);
            pdev->converting_image_matrix = *mat;
        } else if (do_image == USE_AS_PATTERN)
            code = use_image_as_pattern(pdev, pres, mat, ps_bitmap_id);
    }
    return code;
}

/* Clean up by releasing the buffers. */
static int
pdf_image_end_image_data(gx_image_enum_common_t * info, bool draw_last,
                         pdf_image_usage_t do_image)
{
    gx_device_pdf *pdev = (gx_device_pdf *)info->dev;
    pdf_image_enum *pie = (pdf_image_enum *)info;
    int height = pie->writer.height;
    int data_height = height - pie->rows_left;
    int code = 0;

    if (pie->writer.pres)
        ((pdf_x_object_t *)pie->writer.pres)->data_height = data_height;
    else if (data_height > 0)
        pdf_put_image_matrix(pdev, &pie->mat, (double)data_height / height);
    if (data_height > 0) {
        code = pdf_complete_image_data(pdev, &pie->writer, data_height,
                        pie->width, pie->bits_per_pixel);
        if (code < 0)
            return code;
        code = pdf_end_image_binary(pdev, &pie->writer, data_height);
        /* The call above possibly decreases pie->writer.alt_writer_count in 2. */
        if (code < 0)
            return code;
        if (pie->writer.alt_writer_count == 2) {
            /* We're converting a type 4 image into an imagemask with a pattern color. */
            /* Since the type 3 image writes the mask first, do so here. */
            pdf_image_writer writer = pie->writer;

            writer.binary[0] = pie->writer.binary[1];
            writer.pres = pie->writer.pres_mask;
            writer.alt_writer_count = 1;
            memset(&pie->writer.binary[1], 0, sizeof(pie->writer.binary[1]));
            pie->writer.alt_writer_count--; /* For GC. */
            pie->writer.pres_mask = 0; /* For GC. */
            code = pdf_end_image_binary(pdev, &writer, data_height);
            if (code < 0)
                return code;
            code = pdf_end_and_do_image(pdev, &writer, &pie->mat, info->id, USE_AS_MASK);
            if (code < 0)
                return code;
            code = pdf_end_and_do_image(pdev, &pie->writer, &pie->mat, info->id, USE_AS_PATTERN);
        } else
            code = pdf_end_and_do_image(pdev, &pie->writer, &pie->mat, info->id, do_image);
        pie->writer.alt_writer_count--; /* For GC. */
    }
    gx_image_free_enum(&info);
    return code;
}

/* End a normal image, drawing it. */
static int
pdf_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    return pdf_image_end_image_data(info, draw_last, USE_AS_IMAGE);
}

/* End an image converted with pdf_lcvd_t. */
static int
pdf_image_end_image_cvd(gx_image_enum_common_t * info, bool draw_last)
{   pdf_lcvd_t *cvd = (pdf_lcvd_t *)info->dev;
    int code = pdf_dump_converted_image(cvd->pdev, cvd);
    int code1 = gx_image1_end_image(info, draw_last);
    int code2 = gs_closedevice((gx_device *)cvd->mask);
    int code3 = gs_closedevice((gx_device *)cvd);

    gs_free_object(cvd->mask->memory, (gx_device *)cvd->mask, "pdf_image_end_image_cvd");
    gs_free_object(cvd->mdev.memory, (gx_device *)cvd, "pdf_image_end_image_cvd");
    return code < 0 ? code : code1 < 0 ? code1 : code2 < 0 ? code2 : code3;
}
/* ---------------- Type 3/3x images ---------------- */

/*
 * For both types of masked images, we create temporary dummy (null) devices
 * that forward the begin_typed_image call to the implementation above.
 */
static int
pdf_make_mxd(gx_device **pmxdev, gx_device *tdev, gs_memory_t *mem)
{
    gx_device *fdev;
    int code = gs_copydevice(&fdev, (const gx_device *)&gs_null_device, mem);

    if (code < 0)
        return code;
    gx_device_set_target((gx_device_forward *)fdev, tdev);
    *pmxdev = fdev;
    return 0;
}

/* End the mask of an ImageType 3 image, not drawing it. */
static int
pdf_image_end_image_object(gx_image_enum_common_t * info, bool draw_last)
{
    return pdf_image_end_image_data(info, draw_last, USE_AS_MASK);
}
/* End the data of an ImageType 3 image, converting it into pattern. */
static int
pdf_image_end_image_object2(gx_image_enum_common_t * info, bool draw_last)
{
    return pdf_image_end_image_data(info, draw_last, USE_AS_PATTERN);
}

/* ---------------- Type 3 images ---------------- */

/* Implement the mask image device. */
static dev_proc_begin_typed_image(pdf_mid_begin_typed_image);
static int
pdf_image3_make_mid(gx_device **pmidev, gx_device *dev, int width, int height,
                    gs_memory_t *mem)
{
    gx_device_pdf *pdev = (gx_device_pdf *)dev;

    if (pdev->CompatibilityLevel < 1.3 && !pdev->PatternImagemask) {
        gs_matrix m;
        pdf_lcvd_t *cvd = NULL;
        int code;

        gs_make_identity(&m);
        code = pdf_setup_masked_image_converter(pdev, mem, &m, &cvd,
                                        true, 0, 0, width, height, true);
        if (code < 0)
            return code;
        cvd->mask->target = (gx_device *)cvd; /* Temporary, just to communicate with
                                         pdf_image3_make_mcde. The latter will reset it. */
        cvd->mask_is_empty = false;
        *pmidev = (gx_device *)cvd->mask;
        return 0;
    } else {
        int code = pdf_make_mxd(pmidev, dev, mem);

        if (code < 0)
            return code;
        set_dev_proc(*pmidev, begin_typed_image, pdf_mid_begin_typed_image);
        return 0;
    }
}
static int
pdf_mid_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
                          const gs_matrix *pmat, const gs_image_common_t *pic,
                          const gs_int_rect * prect,
                          const gx_drawing_color * pdcolor,
                          const gx_clip_path * pcpath, gs_memory_t * mem,
                          gx_image_enum_common_t ** pinfo)
{
    /* The target of the null device is the pdfwrite device. */
    gx_device_pdf *const pdev = (gx_device_pdf *)
        ((gx_device_null *)dev)->target;
    return pdf_begin_typed_image
        (pdev, pis, pmat, pic, prect, pdcolor, pcpath, mem, pinfo,
         PDF_IMAGE_TYPE3_MASK);
}

/* Implement the mask clip device. */
static int
pdf_image3_make_mcde(gx_device *dev, const gs_imager_state *pis,
                     const gs_matrix *pmat, const gs_image_common_t *pic,
                     const gs_int_rect *prect, const gx_drawing_color *pdcolor,
                     const gx_clip_path *pcpath, gs_memory_t *mem,
                     gx_image_enum_common_t **pinfo,
                     gx_device **pmcdev, gx_device *midev,
                     gx_image_enum_common_t *pminfo,
                     const gs_int_point *origin)
{
    int code;
    gx_device_pdf *pdev = (gx_device_pdf *)dev;

    if (pdev->CompatibilityLevel < 1.3 && !pdev->PatternImagemask) {
        /* pdf_image3_make_mid must set midev with a pdf_lcvd_t instance.*/
        pdf_lcvd_t *cvd = (pdf_lcvd_t *)((gx_device_memory *)midev)->target;

        ((gx_device_memory *)midev)->target = NULL;
        cvd->m = pdev->converting_image_matrix;
        cvd->mdev.mapped_x = origin->x;
        cvd->mdev.mapped_y = origin->y;
        *pmcdev = (gx_device *)&cvd->mdev;
        code = gx_default_begin_typed_image
            ((gx_device *)&cvd->mdev, pis, pmat, pic, prect, pdcolor, NULL, mem,
            pinfo);
        if (code < 0)
            return code;
    } else {
        code = pdf_make_mxd(pmcdev, midev, mem);
        if (code < 0)
            return code;
        code = pdf_begin_typed_image
            ((gx_device_pdf *)dev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
            pinfo, PDF_IMAGE_TYPE3_DATA);
        if (code < 0)
            return code;
    }
    /* Due to equal image merging, we delay the adding of the "Mask" entry into
       a type 3 image dictionary until the mask is completed.
       Will do in pdf_end_and_do_image.*/
    return 0;
}

/* ---------------- Type 3x images ---------------- */

/* Implement the mask image device. */
static int
pdf_image3x_make_mid(gx_device **pmidev, gx_device *dev, int width, int height,
                     int depth, gs_memory_t *mem)
{
    int code = pdf_make_mxd(pmidev, dev, mem);

    if (code < 0)
        return code;
    set_dev_proc(*pmidev, begin_typed_image, pdf_mid_begin_typed_image);
    return 0;
}

/* Implement the mask clip device. */
static int
pdf_image3x_make_mcde(gx_device *dev, const gs_imager_state *pis,
                      const gs_matrix *pmat, const gs_image_common_t *pic,
                      const gs_int_rect *prect,
                      const gx_drawing_color *pdcolor,
                      const gx_clip_path *pcpath, gs_memory_t *mem,
                      gx_image_enum_common_t **pinfo,
                      gx_device **pmcdev, gx_device *midev[2],
                      gx_image_enum_common_t *pminfo[2],
                      const gs_int_point origin[2],
                      const gs_image3x_t *pim)
{
    int code;
    pdf_image_enum *pmie;
    int i;
    const gs_image3x_mask_t *pixm;

    if (midev[0]) {
        if (midev[1])
            return_error(gs_error_rangecheck);
        i = 0, pixm = &pim->Opacity;
    } else if (midev[1])
        i = 1, pixm = &pim->Shape;
    else
        return_error(gs_error_rangecheck);
    code = pdf_make_mxd(pmcdev, midev[i], mem);
    if (code < 0)
        return code;
    code = pdf_begin_typed_image
        ((gx_device_pdf *)dev, pis, pmat, pic, prect, pdcolor, pcpath, mem,
         pinfo, PDF_IMAGE_TYPE3_DATA);
    if (code < 0)
        return code;
    if ((*pinfo)->procs != &pdf_image_enum_procs) {
        /* We couldn't handle the image.  Bail out. */
        gx_image_end(*pinfo, false);
        gs_free_object(mem, *pmcdev, "pdf_image3x_make_mcde");
        return_error(gs_error_rangecheck);
    }
    pmie = (pdf_image_enum *)pminfo[i];
    /*
     * Add the SMask entry to the image dictionary, and, if needed,
     * the Matte entry to the mask dictionary.
     */
    if (pixm->has_Matte) {
        int num_components =
            gs_color_space_num_components(pim->ColorSpace);

        code = cos_dict_put_c_key_floats(
                                (cos_dict_t *)pmie->writer.pres->object,
                                "/Matte", pixm->Matte,
                                num_components);
        if (code < 0)
            return code;
    }
/* Don't put SMask here because pmie->writer.pres->object may be substituted
 * after the image stream is accummulated. pdf_end_and_do_image will set
 * SMask with the right value. Bug 690345.
 */
    return 0;
}

pdf_resource_t *pdf_substitute_pattern(pdf_resource_t *pres)
{
    pdf_pattern_t *ppat = (pdf_pattern_t *)pres;

    return (pdf_resource_t *)(ppat->substitute != 0 ? ppat->substitute : ppat);
}

static int
check_unsubstituted2(gx_device_pdf * pdev, pdf_resource_t *pres0, pdf_resource_t *pres1)
{
    pdf_pattern_t *ppat0 = (pdf_pattern_t *)pres0;
    pdf_pattern_t *ppat1 = (pdf_pattern_t *)pres1;

    return (ppat0->substitute == NULL && ppat1->substitute == NULL);
}

static int
check_unsubstituted1(gx_device_pdf * pdev, pdf_resource_t *pres0)
{
    pdf_pattern_t *ppat = (pdf_pattern_t *)pres0;

    return ppat->substitute != NULL;
}

/*
   The device specific operations - just pattern management.
   See gxdevcli.h about return codes.
 */
int
gdev_pdf_dev_spec_op(gx_device *pdev1, int dev_spec_op, void *data, int size)
{
    gx_device_pdf *pdev = (gx_device_pdf *)pdev1;
    int code=0;
    pdf_resource_t *pres, *pres1;
    gx_bitmap_id id = (gx_bitmap_id)size;
    gs_pattern1_instance_t *pinst = (gs_pattern1_instance_t *)data;

    switch (dev_spec_op) {
        case gxdso_pattern_can_accum:
            return 1;
        case gxdso_form_begin:
            if (pdev->HighLevelForm == 0 && pdev->PatternDepth == 0) {
                gs_form_template_t *tmplate = (gs_form_template_t *)data;
                float arry[6];
                cos_dict_t *pcd = NULL, *pcd_Resources = NULL;

                /* Make sure the document and page stream are open */
                code = pdfwrite_pdf_open_document(pdev);
                if (code < 0)
                    return code;
                code = pdf_open_contents(pdev, PDF_IN_STREAM);
                if (code < 0)
                    return code;
                /* Put any extant clip out before we start the form */
                code = pdf_put_clip_path(pdev, tmplate->pcpath);
                if (code < 0)
                    return code;
                /* Set the CTM to be the one passed in from the interpreter,
                 * this allows us to spot forms even when translation/rotation takes place
                 * as we remove the CTN from the form stream before capture
                 */
                pprintg6(pdev->strm, "q %g %g %g %g %g %g cm\n", tmplate->CTM.xx, tmplate->CTM.xy,
                         tmplate->CTM.yx, tmplate->CTM.yy, tmplate->CTM.tx, tmplate->CTM.ty);

                /* star capturing the form stream */
                code = pdf_enter_substream(pdev, resourceXObject, id, &pres, false,
                        pdev->CompressFonts/* Have no better switch.*/);
                if (code < 0)
                    return code;
                pcd = cos_stream_dict((cos_stream_t *)pres->object);
                pcd_Resources = cos_dict_alloc(pdev, "pdf_pattern(Resources)");
                if (pcd == NULL || pcd_Resources == NULL)
                    return_error(gs_error_VMerror);
                code = cos_dict_put_c_strings(pcd, "/Type", "/XObject");
                if (code >= 0)
                    code = cos_dict_put_c_strings(pcd, "/Subtype", "/Form");
                if (code >= 0)
                    code = cos_dict_put_c_strings(pcd, "/FormType", "1");
                if (code >= 0)
                    code = cos_dict_put_c_key_object(pcd, "/Resources", COS_OBJECT(pcd_Resources));
                arry[0] = tmplate->BBox.p.x;
                arry[1] = tmplate->BBox.p.y;
                arry[2] = tmplate->BBox.q.x;
                arry[3] = tmplate->BBox.q.y;
                if (code >= 0)
                    code = cos_dict_put_c_key_floats(pcd, "/BBox", arry, 4);
                if (code < 0)
                    return code;

                arry[0] = tmplate->form_matrix.xx;
                arry[1] = tmplate->form_matrix.xy;
                arry[2] = tmplate->form_matrix.yx;
                arry[3] = tmplate->form_matrix.yy;
                arry[4] = tmplate->form_matrix.tx;
                arry[5] = tmplate->form_matrix.ty;
                code = cos_dict_put_c_key_floats(pcd, "/Matrix", arry, 6);
                pprintg2(pdev->strm, "%g 0 0 %g 0 0 cm\n",
                         72.0 / pdev->HWResolution[0], 72.0 / pdev->HWResolution[1]);

                /* We'll return this to the interpreter and have it set
                 * as the CTM, so that we remove the prior CTM before capturing the form.
                 * This is safe because forms are always run inside a gsave/grestore, so
                 * CTM will be put back for us.
                 */
                tmplate->CTM.xx = pdev->HWResolution[0] / 72;
                tmplate->CTM.xy = 0.0;
                tmplate->CTM.yx = 0.0;
                tmplate->CTM.yy = pdev->HWResolution[0] / 72;
                tmplate->CTM.tx = 0.0;
                tmplate->CTM.ty = 0.0;

                pdev->substream_Resources = pcd_Resources;
                pres->rid = id;
                if (code >= 0)
                    pdev->HighLevelForm++;
                return 1;
            }
            return code;
        case gxdso_form_end:
            /* This test must be the same as the one in gxdso_form_begin, above */
            if (pdev->HighLevelForm == 1 && pdev->PatternDepth == 0) {
                code = pdf_add_procsets(pdev->substream_Resources, pdev->procsets);
                if (code < 0)
                    return code;
                pres = pres1 = pdev->accumulating_substream_resource;
                code = pdf_exit_substream(pdev);
                if (code < 0)
                    return code;
                code = pdf_find_same_resource(pdev, resourceXObject, &pres, check_unsubstituted2);
                if (code < 0)
                    return code;
                if (code > 0) {
                    code = pdf_cancel_resource(pdev, pres1, resourceXObject);
                    if (code < 0)
                        return code;
                    pres->where_used |= pdev->used_mask;
                } else if (pres->object->id < 0)
                    pdf_reserve_object_id(pdev, pres, 0);
                pprintld1(pdev->strm, "/R%ld Do Q\n", pdf_resource_id(pres));
                pdev->HighLevelForm--;
            }
            return 0;
        case gxdso_pattern_start_accum:
            code = pdf_enter_substream(pdev, resourcePattern, id, &pres, false,
                    pdev->CompressFonts/* Have no better switch.*/);
            if (code < 0)
                return code;
            pres->rid = id;
            code = pdf_store_pattern1_params(pdev, pres, pinst);
            if (code < 0)
                return code;
            /* Scale the coordinate system, because object handlers assume so. See none_to_stream. */
            pprintg2(pdev->strm, "%g 0 0 %g 0 0 cm\n",
                     72.0 / pdev->HWResolution[0], 72.0 / pdev->HWResolution[1]);
            pdev->PatternDepth++;
            return 1;
        case gxdso_pattern_finish_accum:
            code = pdf_add_procsets(pdev->substream_Resources, pdev->procsets);
            if (code < 0)
                return code;
            pres = pres1 = pdev->accumulating_substream_resource;
            code = pdf_exit_substream(pdev);
            if (code < 0)
                return code;
            if (pdev->substituted_pattern_count > 300 &&
                    pdev->substituted_pattern_drop_page != pdev->next_page) { /* arbitrary */
                pdf_drop_resources(pdev, resourcePattern, check_unsubstituted1);
                pdev->substituted_pattern_count = 0;
                pdev->substituted_pattern_drop_page = pdev->next_page;
            }
            code = pdf_find_same_resource(pdev, resourcePattern, &pres, check_unsubstituted2);
            if (code < 0)
                return code;
            if (code > 0) {
                pdf_pattern_t *ppat = (pdf_pattern_t *)pres1;

                code = pdf_cancel_resource(pdev, pres1, resourcePattern);
                if (code < 0)
                    return code;
                /* Do not remove pres1, because it keeps the substitution. */
                ppat->substitute = (pdf_pattern_t *)pres;
                pres->where_used |= pdev->used_mask;
                pdev->substituted_pattern_count++;
            } else if (pres->object->id < 0)
                pdf_reserve_object_id(pdev, pres, 0);
            pdev->PatternDepth--;
            return 1;
        case gxdso_pattern_load:
            pres = pdf_find_resource_by_gs_id(pdev, resourcePattern, id);
            if (pres == 0)
                return gs_error_undefined;
            pres = pdf_substitute_pattern(pres);
            pres->where_used |= pdev->used_mask;
            code = pdf_add_resource(pdev, pdev->substream_Resources, "/Pattern", pres);
            if (code < 0)
                return code;
            return 1;
        case gxdso_pattern_shading_area:
            return 0;
        case gxdso_pattern_is_cpath_accum:
            return 0;
        case gxdso_pattern_shfill_doesnt_need_path:
            return 0; /* gdev_pdf_fill_path still does need a path. */
        case gxdso_pattern_handles_clip_path:
            /* This is important when the default implementation of
               of fill_path is called due to a failure in setcolor
               or so, for example when a shading is incorrect.
               The test case is the unfixed (buggy) Genoa test 446-01.ps .
               In this case pdfwrite converts the object into rectangles,
               and the clipping device has to be set up. */
            return 0;
        case gxdso_supports_hlcolor:
            /* This is used due to some aliasing between the rect_hl color
               filling used by pdfwrite vs. that used by the planar device
               which is actually a devn vs. the pattern type for pdfwrite.
               We use this to distingush between the two */
            return 1;
        case gxdso_needs_invariant_palette:
            /* Indicates that it is not permissible to change /Indexed colour space
             * palette entries after the colour space has been set.
             */
            return 1;
    }
    return gx_default_dev_spec_op(pdev1, dev_spec_op, data, size);
}
