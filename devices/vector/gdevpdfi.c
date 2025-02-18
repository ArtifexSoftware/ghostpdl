/* Copyright (C) 2001-2025 Artifex Software, Inc.
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
#include "gxpath.h"
#include "gxcdevn.h"

extern_st(st_gs_gstate);

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
    gs_color_space_index initial_colorspace;
    int JPEG_PassThrough;
    int JPX_PassThrough;
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
                             const gs_gstate *pgs,
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
        code = pcs->type->remap_color(&cc, pcs, pdcolor, pgs, dev,
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
            gs_logical_operation_t lop = pgs->log_op;
            int black_or_white = color_is_black_or_white(dev, pdcolor);

            lop = lop_sanitize(lop);

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
    const gs_gstate * pgs, const gs_matrix *pmat,
    const gs_image_common_t *pic, const gs_int_rect * prect,
    const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
    gs_memory_t * mem, gx_image_enum_common_t ** pinfo,
    pdf_typed_image_context_t context);

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

static int setup_type3_image(gx_device_pdf *pdev, const gs_gstate * pgs,
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
        code = pdf_check_soft_mask(pdev, (gs_gstate *)pgs);
        if (code < 0)
            return code;
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
        m.tx = floor(pgs->ctm.tx + 0.5); /* Round the origin against the image size distorsions */
        m.ty = floor(pgs->ctm.ty + 0.5);
        pim3a = *pim3;
        code = gs_matrix_invert(&pim3a.ImageMatrix, &mi);
        if (code < 0)
            return code;
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
        pim3a.imagematrices_are_untrustworthy = 1;
        pic1 = (gs_image_common_t *)&pim3a;
        /* Setting pdev->converting_image_matrix to communicate with pdf_image3_make_mcde. */
        gs_matrix_multiply(&mi, &ctm_only(pgs), &pdev->converting_image_matrix);
    }
    /*
     * We handle ImageType 3 images in a completely different way:
     * the default implementation sets up the enumerator.
     */
    return gx_begin_image3_generic((gx_device *)pdev, pgs, pmat1, pic1,
                                   prect, pdcolor, pcpath, mem,
                                   pdf_image3_make_mid,
                                   pdf_image3_make_mcde, pinfo);
}

static int convert_type4_image(gx_device_pdf *pdev, const gs_gstate * pgs,
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
    int code = 1;

    pdev->image_mask_is_SMask = false;
    if (pdf_convert_image4_to_image1(pdev, pgs, pdcolor,
                                     (const gs_image4_t *)pic,
                                     &image[0].type1, &icolor) >= 0) {
        if (pgs == NULL)
            return_error(gs_error_unregistered); /* Must not happen. */

        /* Undo the pop of the NI stack if necessary. */
        if (pnamed)
            cos_array_add_object(pdev->NI_stack, COS_OBJECT(pnamed));
        /* HACK: temporary patch the color space, to allow
           pdf_prepare_imagemask to write the right color for the imagemask. */
        code = gs_gsave((gs_gstate *)pgs);
        if (code < 0)
            return code;
        /* {csrc}: const cast warning */
        code = gs_setcolorspace((gs_gstate *)pgs, ((const gs_image4_t *)pic)->ColorSpace);
        if (code < 0)
            return code;
        code = pdf_begin_typed_image(pdev, pgs, pmat,
                                     (gs_image_common_t *)&image[0].type1,
                                     prect, &icolor, pcpath, mem,
                                     pinfo, context);
        if (code < 0)
            return code;
        code = gs_grestore((gs_gstate *)pgs);
        /* To account for the above pdf_begin_typed_image() being with a gsave/grestore */
        (*pinfo)->pgs_level = pgs->level;
    }
    return code;
}

static int convert_type4_to_masked_image(gx_device_pdf *pdev, const gs_gstate * pgs,
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

        code = pdf_check_soft_mask(pdev, (gs_gstate *)pgs);
        if (code < 0)
            return code;
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
        code = gs_matrix_invert(&pic->ImageMatrix, &mi);
        if (code < 0)
            return code;
        gs_matrix_multiply(&mi, &ctm_only(pgs), &m);
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
            pgs, &m1, (gs_image_common_t *)&pi4, prect, pdcolor, NULL, mem, pinfo);
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

static int check_colorants_for_pdfx4(const gs_color_space *pcs)
{
    int comp, all_present = 1;
    char *ink, *Colorant;
    gs_device_n_colorant *colorant = NULL;

    if (pcs->params.device_n.colorants == NULL) {
        return 0;
    } else {
        for (comp = 0; comp < pcs->params.device_n.num_components;comp++){
            colorant = pcs->params.device_n.colorants;
            ink = pcs->params.device_n.names[comp];
            do {
                if (memcmp(colorant->colorant_name, ink, strlen(ink)) == 0)
                    break;
                colorant = colorant->next;
            }while(colorant);
            if (!colorant) {
                all_present = 0;
                break;
            }
        }
    }
    return all_present;
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
                    pdev->params.ColorConversionStrategy == ccs_RGB || pdev->params.ColorConversionStrategy == ccs_sRGB)
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
                    *pcs_orig = (gs_color_space *)pcs;
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
                        if (pdev->params.ColorConversionStrategy == ccs_RGB || pdev->params.ColorConversionStrategy == ccs_sRGB ||
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
        int strategy = pdev->params.ColorConversionStrategy;

        if (pdev->params.TransferFunctionInfo == tfi_Apply && pdev->transfer_not_identity && csi2 == gs_color_space_index_Indexed) {
            csi = gs_color_space_get_index(pcs->base_space);
            if (csi == gs_color_space_index_ICC) {
                csi = gsicc_get_default_type(pcs->base_space->cmm_icc_profile_data);
            }
            /* If its still not a base space, make it the ProcessCOlorModel of the device */
            if (csi > gs_color_space_index_DeviceCMYK)
                csi = pdev->pcm_color_info_index;
            switch(csi) {
                case gs_color_space_index_DeviceGray:
                    strategy = ccs_Gray;
                    break;
                case gs_color_space_index_DeviceRGB:
                    strategy = ccs_RGB;
                    break;
                case gs_color_space_index_DeviceCMYK:
                    strategy = ccs_CMYK;
                    break;
                default:
                    break;
            }
        }

        switch(strategy) {
            case ccs_ByObjectType:
                /* Object type not implemented yet */
            case ccs_UseDeviceIndependentColorForImages:
                /* If only correcting images, then leave unchanged */
            case ccs_LeaveColorUnchanged:
                return 0;
                break;
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
                        if (pdev->PDFX > 0) {
                            if (pdev->PDFX < 4) {
                                *pcs_orig = pcs;
                                return 1;
                            }
                            if (!check_colorants_for_pdfx4(pcs2)){
                                *pcs_orig = pcs;
                                return 1;
                            }
                        }
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
            case ccs_sRGB:
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

/* Basically, sets up the BBox for Eps2Write case */
static int
pdf_image_handle_eps(gx_device_pdf *pdev, const gs_gstate * pgs,
                     const gs_matrix *pmat, const gs_image_common_t *pic,
                     const gs_int_rect *prect,
                     const gx_clip_path * pcpath)
{
    int code = 0;
    gs_rect sbox, dbox, *Box;
    gs_point corners[4];
    gs_fixed_rect ibox;
    gs_matrix * pmat1 = (gs_matrix *)pmat;
    gs_matrix mat;

    if (!pdev->Eps2Write)
        return 0;

    if (!pdev->accumulating_charproc)
        Box = &pdev->BBox;
    else
        Box = &pdev->charproc_BBox;
    if (pmat1 == 0)
        pmat1 = (gs_matrix *)&ctm_only(pgs);
    if ((code = gs_matrix_invert(&pic->ImageMatrix, &mat)) < 0 ||
        (code = gs_matrix_multiply(&mat, pmat1, &mat)) < 0)
        goto exit;
    sbox.p.x = prect->p.x;
    sbox.p.y = prect->p.y;
    sbox.q.x = prect->q.x;
    sbox.q.y = prect->q.y;
    gs_bbox_transform_only(&sbox, &mat, corners);
    gs_points_bbox(corners, &dbox);
    ibox.p.x = float2fixed(dbox.p.x);
    ibox.p.y = float2fixed(dbox.p.y);
    ibox.q.x = float2fixed(dbox.q.x);
    ibox.q.y = float2fixed(dbox.q.y);
    if (pcpath != NULL &&
        !gx_cpath_includes_rectangle(pcpath, ibox.p.x, ibox.p.y,
                                     ibox.q.x, ibox.q.y)
        ) {
        /* Let the target do the drawing, but drive two triangles */
        /* through the clipping path to get an accurate bounding box. */
        gx_device_clip cdev;
        gx_drawing_color devc;

        fixed x0 = float2fixed(corners[0].x), y0 = float2fixed(corners[0].y);
        fixed bx2 = float2fixed(corners[2].x) - x0, by2 = float2fixed(corners[2].y) - y0;

        pdev->AccumulatingBBox++;
        gx_make_clip_device_on_stack(&cdev, pcpath, (gx_device *)pdev);
        set_nonclient_dev_color(&devc, gx_device_black((gx_device *)pdev));  /* any non-white color will do */
        gx_default_fill_triangle((gx_device *) & cdev, x0, y0,
                                 float2fixed(corners[1].x) - x0,
                                 float2fixed(corners[1].y) - y0,
                                 bx2, by2, &devc, lop_default);
        gx_default_fill_triangle((gx_device *) & cdev, x0, y0,
                                 float2fixed(corners[3].x) - x0,
                                 float2fixed(corners[3].y) - y0,
                                 bx2, by2, &devc, lop_default);
        pdev->AccumulatingBBox--;
    } else {
        /* Just use the bounding box. */
        float x0, y0, x1, y1;
        x0 = fixed2float(ibox.p.x) / (pdev->HWResolution[0] / 72.0);
        y0 = fixed2float(ibox.p.y) / (pdev->HWResolution[1] / 72.0);
        x1 = fixed2float(ibox.q.x) / (pdev->HWResolution[0] / 72.0);
        y1 = fixed2float(ibox.q.y) / (pdev->HWResolution[1] / 72.0);
        if (Box->p.x > x0)
            Box->p.x = x0;
        if (Box->p.y > y0)
            Box->p.y = y0;
        if (Box->q.x < x1)
            Box->q.x = x1;
        if (Box->q.y < y1)
            Box->q.y = y1;
    }
 exit:
    return code;
}

static int
pdf_begin_typed_image(gx_device_pdf *pdev, const gs_gstate * pgs,
                      const gs_matrix *pmat, const gs_image_common_t *pic,
                      const gs_int_rect * prect,
                      const gx_drawing_color * pdcolor,
                      const gx_clip_path * pcpath, gs_memory_t * mem,
                      gx_image_enum_common_t ** pinfo,
                      pdf_typed_image_context_t context)
{
    int code = 0, i;
    unsigned int use_fallback  = 0, in_line = 0, is_mask = 0,
        force_lossless = 0, convert_to_process_colors = 0;
    int width, height;
    cos_dict_t *pnamed = 0;
    image_union_t *image;
    const gs_pixel_image_t *pim;
    gs_int_rect rect;
    gs_image_format_t format;
    gs_color_space *pcs = NULL;
    int num_components;
    pdf_image_enum *pie = NULL;
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

    pim = (const gs_pixel_image_t *)pic;

    /* Check for the image types we can handle. */
    switch (pic->type->index) {
    case 1:
        is_mask = ((const gs_image_t *)pic)->ImageMask;
        code = setup_type1_image(pdev, pic, pdcolor, image, context);
        if (code < 0)
            use_fallback = 1;
        else
            in_line = code;
        break;

    case 3:
        /* Currently we can't handle images with masks, because we have two
         * image enumerators, and the JPEG passthrough is stored at the device
         * level, not the enumerator level. This means that when we skip the
         * image data (because its handled as JPEG) we also skip the mask data,
         * which is no use at all. FIXME: not sure how but if possible I
         * should fix this. Probably need to store the PassThrough in the
         * enumerator, and then store a pointer to the enumerator in the
         * device in place of the flag, so that when we get JPEG data supplied
         * we know where to send it. Of course, that won't work if we ever end
         * up in the situation where we have two JPEG sources at the same time.....
         * That can probably be handled with some judicious work in the DCTDecode
         * structure, to hold some reference to the particular stream that
         * should get the data. But lets get the simple code working first.
         */
        pdev->JPEG_PassThrough = 0;
        pdev->JPX_PassThrough = 0;
        pdev->image_mask_is_SMask = false;
        if (pdev->CompatibilityLevel < 1.2 ||
            (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
                   prect->q.x == ((const gs_image3_t *)pic)->Width &&
                   prect->q.y == ((const gs_image3_t *)pic)->Height))) {
            use_fallback = 1;
            goto exit;
        }
        code = setup_type3_image(pdev, pgs, pmat, pic, prect, pdcolor, pcpath, mem, pinfo, image);
        goto exit;

    case IMAGE3X_IMAGETYPE:
        {
            int64_t OC = pdev->PendingOC;

            pdev->JPEG_PassThrough = 0;
            pdev->JPX_PassThrough = 0;
            if (pdev->CompatibilityLevel < 1.4 ||
                (prect && !(prect->p.x == 0 && prect->p.y == 0 &&
                           prect->q.x == ((const gs_image3x_t *)pic)->Width &&
                           prect->q.y == ((const gs_image3x_t *)pic)->Height))) {
                use_fallback = 1;
                goto exit;
            }
            pdev->image_mask_is_SMask = true;

            pdev->PendingOC = 0;
            code = gx_begin_image3x_generic((gx_device *)pdev, pgs, pmat, pic,
                                            prect, pdcolor, pcpath, mem,
                                            pdf_image3x_make_mid,
                                            pdf_image3x_make_mcde, pinfo, OC);
            goto exit;
        }

    case 4:
        /* If we are colour converting then we may not be able to preserve the
         * type 4 image, if it has a /Mask entry which is a range of colours
         * (chroma-key masking). If its a stencil mask then we can just conver the
         * underlying image and leave the mask alone.
         */
        if (pdev->params.ColorConversionStrategy != ccs_LeaveColorUnchanged) {
            gs_color_space *pcs2;
            int csi = 0;
            bool fallback = false;
            gs_image4_t *pim4 = (gs_image4_t *)pic;

            /* If the /Mask is chroma-key rather than a stencil */
            if (pim4->MaskColor_is_range) {
                /* Find the colour space */
                pcs2 = pim->ColorSpace;
                csi = gs_color_space_get_index(pcs2);
                /* If its /Indexed, get the base space */
                if (csi == gs_color_space_index_Indexed) {
                    pcs2 = pim->ColorSpace->base_space;
                    csi = gs_color_space_get_index(pcs2);
                }
                if (csi == gs_color_space_index_ICC)
                    csi = gsicc_get_default_type(pcs2->cmm_icc_profile_data);
                /* If the base space matches the target for colour conversion
                 * then no conversion is needed, so we can preserve the type
                 * 4 image.
                 */
                switch(csi) {
                    case gs_color_space_index_DeviceGray:
                        if (pdev->params.ColorConversionStrategy != ccs_Gray)
                            fallback = true;
                        break;
                    case gs_color_space_index_DeviceRGB:
                        if (pdev->params.ColorConversionStrategy != ccs_RGB)
                            fallback = true;
                        break;
                    case gs_color_space_index_DeviceCMYK:
                        if (pdev->params.ColorConversionStrategy != ccs_CMYK)
                            fallback = true;
                        break;
                    default:
                        fallback = true;
                        break;
                }
                if (fallback == true && pdev->CompatibilityLevel > 1.2) {
                    /* We've arrived at the point where we have a chroma-keyed
                     * type 4 image, and the image needs to be converted to a
                     * different space. We can't do that, so fall back to a
                     * default implementation, create a clip path and apply it to
                     * the image.
                     */
                    pdev->JPEG_PassThrough = 0;
                    pdev->JPX_PassThrough = 0;
                    use_fallback = 0;
                    code = convert_type4_to_masked_image(pdev, pgs, pic, prect, pdcolor,
                                                         pcpath, mem,pinfo);
                    goto exit;
                }
                /* Note that we fall through to the original code, so if we are not
                 * producing at least PDF 1.2 (for image mask support) then we will
                 * fall back further filled to rectangles.
                 */
            }
        }
        pdev->JPEG_PassThrough = 0;
        pdev->JPX_PassThrough = 0;
        code = convert_type4_image(pdev, pgs, pmat, pic, prect, pdcolor,
                      pcpath, mem, pinfo, context, image, pnamed);
        if (code < 0)
            use_fallback = 1;
        if (code == 0)
            goto exit;
        /* No luck.  Masked images require PDF 1.3 or higher. */
        if (pdev->CompatibilityLevel < 1.2)
            use_fallback = 1;
        if (pdev->CompatibilityLevel < 1.3 && !pdev->PatternImagemask) {
            use_fallback = 0;
            code = convert_type4_to_masked_image(pdev, pgs, pic, prect, pdcolor,
                                                 pcpath, mem,pinfo);
            goto exit;
        }
        image[0].type4 = *(const gs_image4_t *)pic;
        break;

    default:
        use_fallback = 1;
        break;
    }

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
        case 12:
        case 16:
            break;
        default:
            code = gs_note_error(gs_error_rangecheck);
            goto exit;
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

    /* Handle BBox for Eps2Write, if applicable */
    code = pdf_image_handle_eps(pdev, pgs, pmat, pic, &rect, pcpath);
    if (code < 0) {
        use_fallback = 0;
        goto exit;
    }

    if (use_fallback)
        goto exit;

    pcs = pim->ColorSpace;
    rc_increment_cs(pcs);
    num_components = (is_mask ? 1 : gs_color_space_num_components(pcs));

    code = pdf_check_soft_mask(pdev, (gs_gstate *)pgs);
    if (code < 0)
        goto exit;
    if (pdf_must_put_clip_path(pdev, pcpath))
        code = pdf_unclip(pdev);
    else
        code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
        goto fail_and_fallback;

    if (context == PDF_IMAGE_TYPE3_MASK) {
        /*
         * The soft mask for an ImageType 3x image uses a DevicePixel
         * color space, which pdf_color_space() can't handle.  Patch it
         * to DeviceGray here.
         */
        /* {csrc} make sure this gets freed */
        rc_decrement(pcs, "pdf_begin_typed_image(pcs)");
        pcs = gs_cspace_new_DeviceGray(pdev->memory);
        if (pcs == NULL)
            code = gs_note_error(gs_error_VMerror);
    } else if (is_mask)
        code = pdf_prepare_imagemask(pdev, pgs, pdcolor);
    else
        code = pdf_prepare_image(pdev, pgs);
    if (code < 0)
        goto fail_and_fallback;

    pie = gs_alloc_struct(mem, pdf_image_enum, &st_pdf_image_enum,
                        "pdf_begin_image");
    if (pie == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto exit;
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
    pie->pgs = pgs;
    if (pgs != NULL)
        pie->pgs_level = pgs->level;
    width = rect.q.x - rect.p.x;
    pie->width = width;
    height = rect.q.y - rect.p.y;
    pie->bits_per_pixel =
        pim->BitsPerComponent * num_components / pie->num_planes;
    pie->rows_left = height;
    if (pnamed != 0 || pdev->PendingOC) /* Don't in-line the image if it is named. Or has Optional Content */
        in_line = false;
    else {
        int64_t nbytes = (int64_t)(((int64_t) pie->width * pie->bits_per_pixel + 7) >> 3) *
            pie->num_planes * pie->rows_left;

        in_line &= (nbytes < pdev->MaxInlineImageSize);
    }
    pie->initial_colorspace = pdev->pcm_color_info_index;

    if (pmat == 0)
        pmat = &ctm_only(pgs);
    {
        gs_matrix mat;
        gs_matrix bmat;
        int code;

        pdf_make_bitmap_matrix(&bmat, -rect.p.x, -rect.p.y,
                               pim->Width, pim->Height, height);
        if ((code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
            (code = gs_matrix_multiply(&bmat, &mat, &mat)) < 0 ||
            (code = gs_matrix_multiply(&mat, pmat, &pie->mat)) < 0
            )
            goto exit;
        /* AR3,AR4 show no image when CTM is singular; AR5 reports an error */
        if (pie->mat.xx * pie->mat.yy == pie->mat.xy * pie->mat.yx) {
            use_fallback = 1;
            goto fail_and_fallback;
        }
    }

    code = pdf_put_clip_path(pdev, pcpath);
    if (code < 0)
        goto exit;
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

    if ((image[0].pixel.ColorSpace != NULL && image[0].pixel.ColorSpace->type->index == gs_color_space_index_Indexed)
        || force_lossless)
        pie->writer.alt_writer_count = 1;

    if (image[0].pixel.ColorSpace != NULL && (image[0].pixel.ColorSpace->type->index != gs_color_space_index_DeviceGray &&
        image[0].pixel.ColorSpace->type->index != gs_color_space_index_DeviceRGB &&
        image[0].pixel.ColorSpace->type->index != gs_color_space_index_DeviceCMYK &&
        image[0].pixel.ColorSpace->type->index != gs_color_space_index_Indexed))
        names = &pdf_color_space_names;
    else
        names = (in_line ? &pdf_color_space_names_short : &pdf_color_space_names);

    /* We don't want to change the colour space of a mask, or an SMask (both of which are Gray) */
    if (!is_mask) {
#if 1
        if (image[0].pixel.ColorSpace != NULL && !(context == PDF_IMAGE_TYPE3_MASK))
           convert_to_process_colors = setup_image_colorspace(pdev, &image[0], pcs, &pcs_orig, names, &cs_value);
#else
        if (image[0].pixel.ColorSpace != NULL) {
            if (context != PDF_IMAGE_TYPE3_MASK)
                convert_to_process_colors = setup_image_colorspace(pdev, &image[0], pcs, &pcs_orig, names, &cs_value);
            else {
                if (pdev->PDFA != 0 && pdev->params.ColorConversionStrategy != ccs_Gray)
                {
                    /* A SMask *must* be in DeviceGray (PDF 1.7 reference, page 555), but if we're producing a PDF/A
                     * and not creating a Gray output then we can't write the SMask as DeviceGray!
                     */
                    emprintf(pdev->memory,
                         "\nDetected SMask which must be in DeviceGray, but we are not converting to DeviceGray, reverting to normal PDF output\n");
                    pdev->AbortPDFAX = true;
                    pdev->PDFA = 0;
                }
            }
        }
#endif

        if (pim->BitsPerComponent > 8 && convert_to_process_colors) {
            use_fallback = 1;
            goto fail_and_fallback;
        }
        if (convert_to_process_colors == 4) {
            if (pdev->PDFX == 1) {
                convert_to_process_colors = 1;
                code = 0;
            }
            else
                code = convert_DeviceN_alternate(pdev, pgs, pcs, NULL, NULL, NULL, NULL, &cs_value, in_line);
            if (code < 0)
                goto fail_and_fallback;
        }
        if (convert_to_process_colors == 3) {
            code = convert_separation_alternate(pdev, pgs, pcs, NULL, NULL, NULL, NULL, &cs_value, in_line);
            if (code < 0)
                goto fail_and_fallback;
        }
        if (convert_to_process_colors == 1) {
            code = make_device_color_space(pdev, pdev->pcm_color_info_index, &pcs_device);
            if (code < 0)
                goto fail_and_fallback;
            image[0].pixel.ColorSpace = pcs_device;
            image[0].pixel.BitsPerComponent = 8;
            code = pdf_color_space_named(pdev, pgs, &cs_value, &pranges, pcs_device, names,
                                     in_line, NULL, 0, false);
            if (code < 0)
                goto fail_and_fallback;
        } else {
            if (convert_to_process_colors == 2) {
                convert_to_process_colors = 0;
                code = pdf_color_space_named(pdev, pgs, &cs_value, &pranges, pcs, names,
                                     in_line, NULL, 0, true);
                if (code < 0)
                    goto fail_and_fallback;
            } else {
                convert_to_process_colors = 0;
                code = pdf_color_space_named(pdev, pgs, &cs_value, &pranges, pcs, names,
                                     in_line, NULL, 0, false);
                if (code < 0)
                    goto fail_and_fallback;
            }
        }
    }

    image[1] = image[0];

    pdev->ParamCompatibilityLevel = pdev->CompatibilityLevel;

    code = pdf_begin_write_image(pdev, &pie->writer, gs_no_id, width,
                    height, pnamed, in_line);
    if (code < 0)
        goto fail_and_fallback;

    if (pdev->CompatibilityLevel < 1.5)
        pdev->JPX_PassThrough = 0;

    if (!convert_to_process_colors)
    {
        gs_color_space_index csi;

        if (pdev->params.TransferFunctionInfo == tfi_Apply && pdev->transfer_not_identity && !is_mask) {
            pdev->JPEG_PassThrough = 0;
            pdev->JPX_PassThrough = 0;
            csi = gs_color_space_get_index(image[0].pixel.ColorSpace);
            if (csi == gs_color_space_index_Indexed) {
                csi = gs_color_space_get_index(image[0].pixel.ColorSpace->base_space);
                if (csi == gs_color_space_index_ICC) {
                    csi = gsicc_get_default_type(image[0].pixel.ColorSpace->base_space->cmm_icc_profile_data);
                }
            } else {
                if (csi == gs_color_space_index_ICC) {
                    csi = gsicc_get_default_type(image[0].pixel.ColorSpace->cmm_icc_profile_data);
                }
            }
            switch(csi) {
                case gs_color_space_index_DevicePixel:
                case gs_color_space_index_CIEA:
                    convert_to_process_colors = 1;
                    pdf_set_process_color_model(pdev, 0);
                    break;
                case gs_color_space_index_CIEDEF:
                case gs_color_space_index_CIEABC:
                case gs_color_space_index_DeviceGray:
                    convert_to_process_colors = 1;
                    pdf_set_process_color_model(pdev, 0);
                    break;
                case gs_color_space_index_DeviceRGB:
                    convert_to_process_colors = 1;
                    pdf_set_process_color_model(pdev, 1);
                    break;
                case gs_color_space_index_CIEDEFG:
                case gs_color_space_index_DeviceCMYK:
                    convert_to_process_colors = 1;
                    pdf_set_process_color_model(pdev, 2);
                    break;
                default:
                    break;
            }
            if (convert_to_process_colors == 1) {
                pcs_orig = image->pixel.ColorSpace;
                code = make_device_color_space(pdev, pdev->pcm_color_info_index, &pcs_device);
                if (code < 0)
                    goto fail_and_fallback;
                image[0].pixel.ColorSpace = pcs_device;
                code = pdf_color_space_named(pdev, pgs, &cs_value, &pranges, pcs_device, names,
                                         in_line, NULL, 0, false);
                if (code < 0)
                    goto fail_and_fallback;
            }
        }
    }
    /* If we are not preserving the colour space unchanged, then we can't pass through JPEG */
    else
        pdev->JPEG_PassThrough = pdev->JPX_PassThrough = 0;

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
        pdev->JPEG_PassThrough = 0;
        pdev->JPX_PassThrough = 0;
        code = new_setup_lossless_filters((gx_device_psdf *) pdev,
                                             &pie->writer.binary[0],
                                             &image[0].pixel, in_line, convert_to_process_colors, (gs_matrix *)pmat, (gs_gstate *)pgs);
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
                                          pmat, pgs, true, in_line, convert_to_process_colors);
            pdev->params.ColorImage.DownsampleType = saved_downsample;
        } else {
            code = new_setup_image_filters((gx_device_psdf *) pdev,
                                          &pie->writer.binary[0], &image[0].pixel,
                                          pmat, pgs, true, in_line, convert_to_process_colors);
        }
    }

    if (code < 0)
        goto fail_and_fallback;

    if (convert_to_process_colors) {
        image[0].pixel.ColorSpace = pcs_orig;
        image[0].pixel.BitsPerComponent = pim->BitsPerComponent;
        code = psdf_setup_image_colors_filter(&pie->writer.binary[0],
                                              (gx_device_psdf *)pdev, pim, &image[0].pixel, pgs);
        if (code < 0)
            goto fail_and_fallback;
        image[0].pixel.ColorSpace = pcs_device;
    }

    if (pdev->JPEG_PassThrough || pdev->JPX_PassThrough) {
/*        if (pie->writer.alt_writer_count > 1) {
            s_close_filters(&pie->writer.binary[0].strm, uncompressed);
            memset(pie->writer.binary + 1, 0, sizeof(pie->writer.binary[1]));
            memset(pie->writer.binary + 2, 0, sizeof(pie->writer.binary[1]));
        }*/
        pdev->PassThroughWriter = pie->writer.binary[0].strm;
        pie->writer.alt_writer_count = 1;
    }
    pie->JPEG_PassThrough = pdev->JPEG_PassThrough;
    pie->JPX_PassThrough = pdev->JPX_PassThrough;

    if (pie->writer.alt_writer_count > 1) {
        code = pdf_make_alt_stream(pdev, &pie->writer.binary[1]);
        if (code < 0)
            goto fail_and_fallback;
        code = new_setup_image_filters((gx_device_psdf *) pdev,
                                  &pie->writer.binary[1], &image[1].pixel,
                                  pmat, pgs, force_lossless, in_line, convert_to_process_colors);
        if (code == gs_error_rangecheck) {

            for (i=1;i < pie->writer.alt_writer_count; i++) {
                stream *s = pie->writer.binary[i].strm;
                cos_stream_t *pcos = cos_stream_from_pipeline(pie->writer.binary[i].strm);
                s_close_filters(&s, NULL);
                gs_free_object(pdev->pdf_memory, s, "compressed image stream");
                if (pcos == 0L) {
                    /* TODO: Seems like something should be freed here */
                    code = gs_note_error(gs_error_ioerror);
                    goto exit;
                }
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
            image[1].pixel.BitsPerComponent = pim->BitsPerComponent;
            code = psdf_setup_image_colors_filter(&pie->writer.binary[1],
                                              (gx_device_psdf *)pdev, pim, &image[1].pixel, pgs);
            if (code < 0)
                goto fail_and_fallback;
            image[1].pixel.ColorSpace = pcs_device;
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
        if (code < 0)
            goto fail_and_fallback;
        code = psdf_setup_image_filters((gx_device_psdf *) pdev,
                                  &pie->writer.binary[i], &image[i].pixel,
                                  pmat, pgs, force_lossless, in_line);
        if (code < 0)
            goto fail_and_fallback;
        /* Bug701972 -- added input_width arg here.  For this case, just passing in the same
         * width as before, so nothing changes.  This is an obscure case that isn't tested
         * on the cluster (note that it requires CompatibilityLevel < 1.3).
         */
        psdf_setup_image_to_mask_filter(&pie->writer.binary[i],
                                        (gx_device_psdf *)pdev, pim->Width, pim->Height, pim->Width,
                                        num_components, pim->BitsPerComponent, image[i].type4.MaskColor);
        code = pdf_begin_image_data_decoded(pdev, num_components, pranges, i,
                             &image[i].pixel, &cs_value, pie);
        if (code < 0)
            goto fail_and_fallback;
        ++pie->writer.alt_writer_count;
    }

    /* use_fallback = 0, so this will drop through the below labels, doing only the cleanup parts */
    code = 0;

    /* This label is used when code < 0 and we want to do the fallback code */
 fail_and_fallback:
    if (code != 0)
        use_fallback = 1;

    /* This label is used either when there's no error and we are just exiting normally
     * (possibly with use_fallback=1), or there is an error but we don't want to do
     * the fallback code.
     */
 exit:
    /* Free everything */
    rc_decrement(pcs, "pdf_begin_typed_image(pcs)");
    rc_decrement(pcs_device, "pdf_begin_typed_image(pcs_device)");
    gs_free(mem->non_gc_memory, image, 4, sizeof(image_union_t),
                                      "pdf_begin_typed_image(image)");

    /* Free pie only if there was an error or we are falling back */
    if (code < 0 || use_fallback) {
        if (pie)
            gs_free_object(mem, pie, "pdf_begin_typed_image(pie)");
        *pinfo = NULL;
    }
    /* Do the fallback */
    if (use_fallback) {
        pdev->JPEG_PassThrough = 0;
        pdev->JPX_PassThrough = 0;
        code = gx_default_begin_typed_image
            ((gx_device *)pdev, pgs, pmat, pic, prect, pdcolor, pcpath, mem, pinfo);
    }
    return code;
}

int
gdev_pdf_begin_typed_image(gx_device * dev, const gs_gstate * pgs,
                           const gs_matrix *pmat, const gs_image_common_t *pic,
                           const gs_int_rect * prect,
                           const gx_drawing_color * pdcolor,
                           const gx_clip_path * pcpath, gs_memory_t * mem,
                           gx_image_enum_common_t ** pinfo)
{
    return pdf_begin_typed_image((gx_device_pdf *)dev, pgs, pmat, pic, prect,
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
    uint ignore;
    int nplanes = pie->num_planes;
    int status = 0;
    uint bcount = (width_bits + 7) >> 3;

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

                if (count > block_bytes) {
                    flip_count = block_bytes;
                    flipped_count = block_bytes * nplanes;
                } else {
                    flip_count = count;
                    flipped_count =
                        (width_bits % (block_bytes * 8) * nplanes + 7) >> 3;
                    /* In case the width of the image is a precise multiple of our block size */
                    if (flipped_count == 0)
                        flipped_count = block_bytes * nplanes;
                }
                status = image_flip_planes(row, bit_planes, offset, flip_count,
                                           nplanes, pie->plane_depths[0]);
                if (status < 0)
                    break;
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

    if (info->pgs != NULL && info->pgs->level < info->pgs_level)
        return_error(gs_error_undefinedresult);

    if (pie->JPEG_PassThrough || pie->JPX_PassThrough) {
        pie->rows_left -= height;
        *rows_used = height;
        return !pie->rows_left;
    }

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
    gs_gstate s;
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
    inst.saved = (gs_gstate *)&s; /* HACK : will use s.ctm only. */
    inst.templat.PaintType = 1;
    inst.templat.TilingType = 1;
    inst.templat.BBox.p.x = inst.templat.BBox.p.y = 0;
    inst.templat.BBox.q.x = 1;
    inst.templat.BBox.q.y = 1;
    inst.templat.XStep = 2; /* Set 2 times bigger step against artifacts. */
    inst.templat.YStep = 2;

    {
        pattern_accum_param_s param;
        param.pinst = (void *)&inst;
        param.graphics_state = (void *)&s;
        param.pinst_id = inst.id;

        code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
            gxdso_pattern_start_accum, &param, sizeof(pattern_accum_param_s));
    }

    if (code >= 0)
        pprinti64d1(pdev->strm, "/R%"PRId64" Do\n", pdf_resource_id(pres1));
    pres = pdev->accumulating_substream_resource;
    if (code >= 0)
        code = pdf_add_resource(pdev, pdev->substream_Resources, "/XObject", pres1);
    if (code >= 0) {
        pattern_accum_param_s param;
        param.pinst = (void *)&inst;
        param.graphics_state = (void *)&s;
        param.pinst_id = inst.id;

        code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
            gxdso_pattern_finish_accum, &param, id);
    }
    if (code >= 0)
        code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
            gxdso_pattern_load, &id, sizeof(gs_id));
    if (code >= 0) {
        stream_puts(pdev->strm, "q ");
        code = pdf_cs_Pattern_colored(pdev, &v);
    }
    if (code >= 0) {
        cos_value_write(&v, pdev);
        pprinti64d1(pdev->strm, " cs /R%"PRId64" scn ", pdf_resource_id(pres));
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
    int code = 0;
    pdf_resource_t *pres = NULL;

    /* In order to identify duplicated images which use the same SMask we must
     * add the SMask entry to the image dictionary before we call pdf_end_write_image
     * because that will hash the dictionaries and streams. If we haven't already
     * added the SMask then the hash will not match any existing image which uses an SMask.
     */
    if (do_image == USE_AS_IMAGE) {
        if (pdev->image_mask_id != gs_no_id && piw->pres && piw->pres->object) {
            char buf[20];

            gs_snprintf(buf, sizeof(buf), "%ld 0 R", pdev->image_mask_id);
            code = cos_dict_put_string_copy((cos_dict_t *)piw->pres->object,
                    pdev->image_mask_is_SMask ? "/SMask" : "/Mask", buf);
            (*(piw->pres->object)).md5_valid = 0;
            if (code < 0)
                return code;
        }
    }

    code = pdf_end_write_image(pdev, piw);
    pres = piw->pres;

    switch (code) {
    default:
        return code;	/* error */
    case 1:
        code = 0;
        break;
    case 0:
        if (do_image == USE_AS_IMAGE) {
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
    int code = 0, ecode;

    if (pie->writer.pres)
        ((pdf_x_object_t *)pie->writer.pres)->data_height = data_height;
    else if (data_height > 0)
        pdf_put_image_matrix(pdev, &pie->mat, (double)data_height / height);
    if (data_height > 0) {
        if (pie->writer.pres) {
            code = pdf_complete_image_data(pdev, &pie->writer, data_height,
                        pie->width, pie->bits_per_pixel);
            if (code < 0)
                return code;
        }
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
    } else {
        code = pdf_end_image_binary(pdev, &pie->writer, data_height);
        code = pdf_end_abort_image(pdev, &pie->writer);
        pie->writer.alt_writer_count--; /* For GC. */
    }
    if (pie->initial_colorspace != pdev->pcm_color_info_index)
        pdf_set_process_color_model(pdev, pie->initial_colorspace);

    /* Clean up any outstanding streams before freeing the enumerator */
    while (pie->writer.alt_writer_count-- > 0) {
        ecode = psdf_end_binary(&(pie->writer.binary[pie->writer.alt_writer_count]));
        /* If we are skipping an image (because its clipped out or similar) then we
         * won't have written any data to it. Some filters (notably the DCTEncode filter)
         * throw an error (premature EOD) if we close the filter without writing any data to it.
         * So if we are skipping the image, ignore errors when closing the stream.
         * Unfortunately we don't set pie->skipping until after begin_typed_image()
         * or we could avoid a lot of setup....
         */
        if (ecode < 0 && code >= 0 && !pie->skipping) code  = ecode;
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
    int code = pdf_dump_converted_image(cvd->pdev, cvd, 0);
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
pdf_mid_begin_typed_image(gx_device * dev, const gs_gstate * pgs,
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
        (pdev, pgs, pmat, pic, prect, pdcolor, pcpath, mem, pinfo,
         PDF_IMAGE_TYPE3_MASK);
}

/* Implement the mask clip device. */
static int
pdf_image3_make_mcde(gx_device *dev, const gs_gstate *pgs,
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
        cvd->mdev.width += origin->x;
        cvd->mdev.height += origin->y;
        *pmcdev = (gx_device *)&cvd->mdev;
        code = gx_default_begin_typed_image
            ((gx_device *)&cvd->mdev, pgs, pmat, pic, prect, pdcolor, NULL, mem,
            pinfo);
        if (code < 0)
            return code;
    } else {
        code = pdf_make_mxd(pmcdev, midev, mem);
        if (code < 0)
            return code;
        code = pdf_begin_typed_image
            ((gx_device_pdf *)dev, pgs, pmat, pic, prect, pdcolor, pcpath, mem,
            pinfo, PDF_IMAGE_TYPE3_DATA);
        if (code < 0) {
            gx_device_set_target((gx_device_forward *)(*pmcdev), NULL);
            gs_closedevice((*pmcdev));
            gs_free_object(mem, (*pmcdev), "pdf_image3_make_mcde(*pmcdev)");
            *pmcdev = NULL;
            return code;
        }
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
pdf_image3x_make_mcde(gx_device *dev, const gs_gstate *pgs,
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
    gx_device_pdf *pdf_dev = (gx_device_pdf *)dev;

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

    if (pminfo[0] != NULL)
        pdf_dev->PendingOC = pminfo[0]->OC;
    else
        pdf_dev->PendingOC = 0;

    code = pdf_begin_typed_image
        ((gx_device_pdf *)dev, pgs, pmat, pic, prect, pdcolor, pcpath, mem,
         pinfo, PDF_IMAGE_TYPE3_DATA);
    pdf_dev->PendingOC = 0;
    if (code < 0) {
        rc_decrement(*pmcdev, "pdf_image3x_make_mcde");
        return code;
    }
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
        gx_device_pdf *pdev = (gx_device_pdf *)dev;
        int DoMatte = 0, num_components =
            gs_color_space_num_components(pim->ColorSpace);

        switch (pdev->params.ColorConversionStrategy) {
            case ccs_LeaveColorUnchanged:
                DoMatte = 1;
                break;
            case ccs_RGB:
            case ccs_sRGB:
                if (num_components == 3)
                    DoMatte = 1;
                else
                    DoMatte = 0;
                break;
            case ccs_CMYK:
                if (num_components == 4)
                    DoMatte = 1;
                else
                    DoMatte = 0;
                break;
            case ccs_Gray:
                if (num_components == 1)
                    DoMatte = 1;
                else
                    DoMatte = 0;
                break;
            case ccs_UseDeviceIndependentColor:
            case ccs_UseDeviceIndependentColorForImages:
            case ccs_ByObjectType:
            default:
                DoMatte = 0;
                break;
        }

        if (DoMatte) {
            code = cos_dict_put_c_key_floats((gx_device_pdf *)dev,
                                    (cos_dict_t *)pmie->writer.pres->object,
                                    "/Matte", pixm->Matte,
                                    num_components);
            if (code < 0)
                return code;
        }
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

static int reset_gstate_for_pattern(gx_device_pdf * pdev, gs_gstate *destination, gs_gstate *source)
{
    if (pdev->vg_initial_set) {
        destination->strokeconstantalpha = source->strokeconstantalpha;
        source->strokeconstantalpha = pdev->vg_initial.strokeconstantalpha;
        destination->fillconstantalpha = source->fillconstantalpha;
        source->fillconstantalpha = pdev->vg_initial.fillconstantalpha;
        if (destination->set_transfer.red != NULL)
            destination->set_transfer.red->id = (source->set_transfer.red != NULL ? source->set_transfer.red->id : 0);
        if (destination->set_transfer.green != NULL)
            destination->set_transfer.green->id = (source->set_transfer.green != NULL ? source->set_transfer.green->id : 0);
        if (destination->set_transfer.blue != NULL)
            destination->set_transfer.blue->id = (source->set_transfer.blue != NULL ? source->set_transfer.blue->id : 0);
        if (destination->set_transfer.gray != NULL)
            destination->set_transfer.gray->id = (source->set_transfer.gray != NULL ? source->set_transfer.gray->id : 0);
        if (source->set_transfer.red != NULL)
            source->set_transfer.red->id = pdev->vg_initial.transfer_ids[0];
        if (source->set_transfer.green != NULL)
            source->set_transfer.green->id = pdev->vg_initial.transfer_ids[1];
        if (source->set_transfer.blue != NULL)
            source->set_transfer.blue->id = pdev->vg_initial.transfer_ids[2];
        if (source->set_transfer.gray != NULL)
            source->set_transfer.gray->id = pdev->vg_initial.transfer_ids[3];
        destination->alphaisshape = source->alphaisshape;
        source->alphaisshape = pdev->vg_initial.alphaisshape;
        destination->blend_mode = source->blend_mode;
        source->blend_mode = pdev->vg_initial.blend_mode;
        if (destination->black_generation != NULL)
            destination->black_generation->id = (source->black_generation != NULL ? source->black_generation->id : 0);
        if (source->black_generation != NULL)
            source->black_generation->id = pdev->vg_initial.black_generation_id;
        if (destination->undercolor_removal != NULL)
            destination->undercolor_removal->id = (source->undercolor_removal != NULL ? source->undercolor_removal->id : 0);
        if (source->undercolor_removal != NULL)
            source->undercolor_removal->id = pdev->vg_initial.undercolor_removal_id;
        destination->overprint_mode = source->overprint_mode;
        source->overprint_mode = pdev->vg_initial.overprint_mode;
        destination->flatness = source->flatness;
        source->flatness = pdev->vg_initial.flatness;
        destination->smoothness = source->smoothness;
        source->smoothness = pdev->vg_initial.smoothness;
        destination->flatness = source->flatness;
        source->flatness = pdev->vg_initial.flatness;
        destination->text_knockout = source->text_knockout;
        source->text_knockout = pdev->vg_initial.text_knockout;
        destination->stroke_adjust = source->stroke_adjust;
        source->stroke_adjust = pdev->vg_initial.stroke_adjust;
        destination->line_params.half_width = source->line_params.half_width;
        source->line_params.half_width = pdev->vg_initial.line_params.half_width;
        destination->line_params.start_cap = source->line_params.start_cap;
        source->line_params.start_cap = pdev->vg_initial.line_params.start_cap;
        destination->line_params.end_cap = source->line_params.end_cap;
        source->line_params.end_cap = pdev->vg_initial.line_params.end_cap;
        destination->line_params.dash_cap = source->line_params.dash_cap;
        source->line_params.dash_cap = pdev->vg_initial.line_params.dash_cap;
        destination->line_params.join = source->line_params.join;
        source->line_params.join = pdev->vg_initial.line_params.join;
        destination->line_params.curve_join = source->line_params.curve_join;
        source->line_params.curve_join = pdev->vg_initial.line_params.curve_join;
        destination->line_params.miter_limit = source->line_params.miter_limit;
        source->line_params.miter_limit = pdev->vg_initial.line_params.miter_limit;
        destination->line_params.miter_check = source->line_params.miter_check;
        source->line_params.miter_check = pdev->vg_initial.line_params.miter_check;
        destination->line_params.dot_length = source->line_params.dot_length;
        source->line_params.dot_length = pdev->vg_initial.line_params.dot_length;
        destination->line_params.dot_length_absolute = source->line_params.dot_length_absolute;
        source->line_params.dot_length_absolute = pdev->vg_initial.line_params.dot_length_absolute;
        destination->line_params.dot_orientation = source->line_params.dot_orientation;
        source->line_params.dot_orientation = pdev->vg_initial.line_params.dot_orientation;
        if (destination->line_params.dash.pattern != NULL && destination->line_params.dash.pattern != source->line_params.dash.pattern)
            gs_free_object(destination->memory, destination->line_params.dash.pattern, "free dash assigned during pattern accumulation");
        memcpy(&destination->line_params.dash, &source->line_params.dash, sizeof(source->line_params.dash));
        memcpy(&source->line_params.dash, &pdev->vg_initial.line_params.dash, sizeof(source->line_params.dash));
    }
    return 0;
}

/*
   The device specific operations - just pattern management.
   See gxdevcli.h about return codes.
 */
int
gdev_pdf_dev_spec_op(gx_device *pdev1, int dev_spec_op, void *data, int size)
{
    gx_device_pdf *pdev = (gx_device_pdf *)pdev1;
    int code=0, force_CTM_change=0;
    pdf_resource_t *pres, *pres1;
    gx_bitmap_id id = (gx_bitmap_id)size;

    switch (dev_spec_op) {
        case gxdso_skip_icc_component_validation:
            return 1;
        case gxdso_supports_pattern_transparency:
            return 1;
        case gxdso_pattern_can_accum:
            return 1;
        case gxdso_pdf_form_name:
            if (pdev->PDFFormName) {
                gs_free_object(pdev->memory->non_gc_memory, pdev->PDFFormName, "free Name of Form for pdfmark");
            }
            pdev->PDFFormName = (char *)gs_alloc_bytes(pdev->memory->non_gc_memory, size + 1, "Name of Form for pdfmark");
            memset(pdev->PDFFormName, 0x00, size + 1);
            memcpy(pdev->PDFFormName, data, size);
            return 0;
        case gxdso_pdf_last_form_ID:
            {
            int *i = (int *)data;
            *i = pdev->LastFormID;
            }
            return 0;
        case gxdso_form_begin:
            if ((!pdev->ForOPDFRead || pdev->HighLevelForm == 0) && pdev->PatternDepth == 0) {
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
                if (!pdev->PDFFormName) {
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
                }

                /* start capturing the form stream */
                code = pdf_enter_substream(pdev, resourceXObject, id, &pres, false,
                        pdev->CompressStreams);
                if (code < 0)
                    return code;
                pcd = cos_stream_dict((cos_stream_t *)pres->object);
                pcd_Resources = cos_dict_alloc(pdev, "pdf_form(Resources)");
                if (pcd == NULL || pcd_Resources == NULL)
                    return_error(gs_error_VMerror);
                code = cos_dict_put_c_strings(pcd, "/Type", "/XObject");
                if (code >= 0)
                    code = cos_dict_put_c_strings(pcd, "/Subtype", "/Form");
                if (code >= 0)
                    code = cos_dict_put_c_strings(pcd, "/FormType", "1");
                if (code >= 0)
                    code = cos_dict_put_c_key_object(pcd, "/Resources", COS_OBJECT(pcd_Resources));

                if (pdev->PDFFormName) {
                    /* This is not (I think) required when executing PS forms, because the
                     * CTM is written out before we execute the Form. It *is* required for
                     * PDF Appearance Forms, because the Form is written directly from the
                     * outer context, not from the page, so we don't emit the CTM first.
                     * We want to alter the Form BBox to take any required rotation and scaling
                     * (from FitPage and /Rotate) into account so that the form appearance is
                     * properly scaled and rotated.
                     */
                    gs_rect bbox_out;
                    gs_matrix cmat, new_mat = tmplate->CTM;

                    /* We don't want anything left over from the page content stream, or other
                     * annotation appearances, to affect whether or not we emit any graphics
                     * state, so reset the state here to the defaults.
                     */
                    pdf_viewer_state_from_gs_gstate(pdev, tmplate->pgs, NULL);
                    /* For PDF Appearance streams at least, the Form BBox is modified by the
                     * Form Matrix.
                     */
                    code = gs_matrix_multiply(&tmplate->form_matrix, &tmplate->CTM, &cmat);
                    if (code < 0)
                        return code;
                    code = gs_bbox_transform(&tmplate->BBox, &cmat, &bbox_out);
                    if (code < 0)
                        return code;

                    /* Check the BBox is on the page. Modify it if it is not (this can happen
                     * if the MediaBox does not have bottom left at 0,0)
                     */
                    cmat.xx = cmat.yy = 1.0f;
                    cmat.xy = cmat.yx = cmat.tx = cmat.ty = 0.0f;
                    if(bbox_out.q.x - bbox_out.p.x > pdev->width) {
                        cmat.xx = pdev->width / (bbox_out.q.x - bbox_out.p.x);
                        bbox_out.q.x = bbox_out.p.x + ((bbox_out.q.x - bbox_out.p.x) * cmat.xx);
                        force_CTM_change = 1;
                    }
                    if(bbox_out.q.y - bbox_out.p.y > pdev->height) {
                        cmat.yy = pdev->height / (bbox_out.q.y - bbox_out.p.y);
                        bbox_out.q.y = bbox_out.p.y + ((bbox_out.q.y - bbox_out.p.y) * cmat.yy);
                        force_CTM_change = 1;
                    }

                    if (bbox_out.p.x < 0) {
                        cmat.tx = bbox_out.p.x * -1;
                        bbox_out.q.x += cmat.tx;
                        force_CTM_change = 1;
                    }
                    if (floor(bbox_out.q.x) > pdev->width) {
                        cmat.tx -= bbox_out.p.x;
                        bbox_out.q.x -= bbox_out.p.x;
                        bbox_out.p.x = 0;
                        force_CTM_change = 1;
                    }
                    if (bbox_out.p.y < 0) {
                        cmat.ty = bbox_out.p.y * -1;
                        bbox_out.q.y += cmat.ty;
                        force_CTM_change = 1;
                    }
                    if (floor(bbox_out.q.y) > pdev->height) {
                        cmat.ty += pdev->height - bbox_out.q.y;
                        force_CTM_change = 1;
                    }

                    if (force_CTM_change) {
                        code = gs_matrix_multiply(&tmplate->CTM, &cmat, &new_mat);
                        if (code < 0)
                            return code;
                        code = gs_matrix_multiply(&tmplate->form_matrix, &new_mat, &cmat);
                        if (code < 0)
                            return code;
                        code = gs_bbox_transform(&tmplate->BBox, &cmat, &bbox_out);
                        if (code < 0)
                            return code;
                        tmplate->CTM = cmat;
                    }
                    arry[0] = bbox_out.p.x;
                    arry[1] = bbox_out.p.y;
                    arry[2] = bbox_out.q.x;
                    arry[3] = bbox_out.q.y;
                    if (code >= 0)
                        code = cos_dict_put_c_key_floats(pdev, pcd, "/BBox", arry, 4);
                    if (code < 0)
                        return code;

                    /* Note that we will apply the CTM to the form, and the Form Matrix. To prevcent
                     * us applying the Matrix twice, we need to set it to the identity in the Form
                     * dictionary. I'm not sure why we don't need to do that for PostScript Forms.
                     */
                    arry[0] = arry[3] = 1.0f;
                    arry[1] = arry[2] = arry[4] = arry[5] = 0.0f;
                } else {
                    arry[0] = tmplate->BBox.p.x;
                    arry[1] = tmplate->BBox.p.y;
                    arry[2] = tmplate->BBox.q.x;
                    arry[3] = tmplate->BBox.q.y;
                    if (code >= 0)
                        code = cos_dict_put_c_key_floats(pdev, pcd, "/BBox", arry, 4);
                    if (code < 0)
                        return code;

                    arry[0] = tmplate->form_matrix.xx;
                    arry[1] = tmplate->form_matrix.xy;
                    arry[2] = tmplate->form_matrix.yx;
                    arry[3] = tmplate->form_matrix.yy;
                    arry[4] = tmplate->form_matrix.tx;
                    arry[5] = tmplate->form_matrix.ty;

                    pprintg2(pdev->strm, "%g 0 0 %g 0 0 cm\n",
                         72.0 / pdev->HWResolution[0], 72.0 / pdev->HWResolution[1]);
                }

                code = cos_dict_put_c_key_floats(pdev, pcd, "/Matrix", arry, 6);
                if (code < 0)
                    return code;

                /* We'll return this to the interpreter and have it set
                 * as the CTM, so that we remove the prior CTM before capturing the form.
                 * This is safe because forms are always run inside a gsave/grestore, so
                 * CTM will be put back for us.
                 */
                if (!pdev->PDFFormName) {
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
                } else {
                    /* For PDF Appearance streams (Forms) we *must* apply the
                     * CTM. This is because if the PDF has a non-zero Rotate key
                     * we bake that rotation into the CTM. If we didn't apply that
                     * then the annotation wouldn't get rotated :-(
                     */
                    pdev->substream_Resources = pcd_Resources;
                    pres->rid = id;
                    if (code >= 0)
                        pdev->HighLevelForm++;
                    return force_CTM_change;
                }
            }
            return code;
        case gxdso_form_end:
            /* This test must be the same as the one in gxdso_form_begin, above */
            if ((!pdev->ForOPDFRead || pdev->HighLevelForm == 1) && pdev->PatternDepth == 0) {
                if (pdev->CompatibilityLevel <= 1.7) {
                    code = pdf_add_procsets(pdev->substream_Resources, pdev->procsets);
                    if (code < 0)
                        return code;
                }
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
                pdev->LastFormID = pdf_resource_id(pres);
                pdev->HighLevelForm--;
                if (pdev->accumulating_substream_resource) {
                    code = pdf_add_resource(pdev, pdev->substream_Resources, "/XObject", pres);
                    if (code < 0)
                        return code;
                }
                if (pdev->PDFFormName) {
                    cos_value_t value;

                    code = cos_dict_put(pdev->local_named_objects, (const byte *)pdev->PDFFormName,
                        strlen(pdev->PDFFormName), cos_object_value(&value, pres->object));

                    if (code < 0)
                        return code;
                    pdf_drop_resource_from_chain(pdev, pres, resourceXObject);
                    pres->object = NULL;
                    gs_free_object(pdev->pdf_memory, pres, "free redundant resource");

                    gs_free_object(pdev->memory->non_gc_memory, pdev->PDFFormName, "free Name of Form for pdfmark");
                    pdev->PDFFormName = 0x00;
                } else {
                    pprinti64d1(pdev->strm, "/R%"PRId64" Do Q\n", pdf_resource_id(pres));
                }
            }
            return 0;
        case gxdso_get_form_ID:
            {
                int *ID = data;
                *ID = pdev->LastFormID;
            }
            return 0;
        case gxdso_repeat_form:
            {
                gs_form_template_t *tmplate = (gs_form_template_t *)data;

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
                pprintld1(pdev->strm, "/R%ld Do Q\n", tmplate->FormID);
                pres = pdf_find_resource_by_resource_id(pdev, resourceXObject, tmplate->FormID);
                if (pres == NULL)
                    return_error(gs_error_undefined);
                pres->where_used |= pdev->used_mask;
                if (pdev->accumulating_substream_resource) {
                    code = pdf_add_resource(pdev, pdev->substream_Resources, "/XObject", pres);
                    if (code < 0)
                        return code;
                }
            }
            return 0;
        case gxdso_pattern_start_accum:
            {
                pattern_accum_param_s *param = (pattern_accum_param_s *)data;
                gs_pattern1_instance_t *pinst = param->pinst;
                gs_gstate *pgs = param->graphics_state;

                code = pdf_check_soft_mask(pdev, (gs_gstate *)pgs);
                if (code < 0)
                    return code;
                if (pdev->context == PDF_IN_NONE) {
                    code = pdf_open_page(pdev, PDF_IN_STREAM);
                    if (code < 0)
                        return code;
                }
                code = pdf_prepare_fill_stroke(pdev, (gs_gstate *)pgs, false);
                if (code < 0)
                    return code;
                if (pdev->PatternDepth == 0 && pdev->initial_pattern_states != NULL) {
                    int pdepth = 0;

                    while (pdev->initial_pattern_states[pdepth] != 0x00) {
                        gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdepth], "Freeing dangling pattern state");
                        pdev->initial_pattern_states[pdepth] = NULL;
                        pdepth++;
                    }
                    gs_free_object(pdev->pdf_memory->non_gc_memory, pdev->initial_pattern_states, "Freeing dangling pattern state stack");
                }

                {
                    gs_gstate **new_states;
                    int pdepth;

                    new_states = (gs_gstate **)gs_alloc_bytes(pdev->pdf_memory->non_gc_memory, sizeof(gs_gstate *) * (pdev->PatternDepth + 2), "pattern initial graphics state stack");
                    memset(new_states, 0x00, sizeof(gs_gstate *) * (pdev->PatternDepth + 2));
                    for (pdepth = 0; pdepth < pdev->PatternDepth;pdepth++)
                        new_states[pdepth] = pdev->initial_pattern_states[pdepth];
                    gs_free_object(pdev->pdf_memory->non_gc_memory, pdev->initial_pattern_states, "Freeing old pattern state stack");
                    pdev->initial_pattern_states = new_states;
                }
                pdev->initial_pattern_states[pdev->PatternDepth] = (gs_gstate *)gs_alloc_struct(pdev->pdf_memory, gs_gstate, &st_gs_gstate, "gdev_pdf_dev_spec_op");
                if (pdev->initial_pattern_states[pdev->PatternDepth] == NULL)
                    return code;
                memset(pdev->initial_pattern_states[pdev->PatternDepth], 0x00, sizeof(gs_gstate));
                pdev->initial_pattern_states[pdev->PatternDepth]->memory = pdev->pdf_memory;

                reset_gstate_for_pattern(pdev, pdev->initial_pattern_states[pdev->PatternDepth], pgs);
                code = pdf_enter_substream(pdev, resourcePattern, pinst->id, &pres, false,
                        pdev->CompressStreams);
                if (code < 0) {
                    gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdev->PatternDepth], "Freeing dangling pattern state");
                    pdev->initial_pattern_states[pdev->PatternDepth] = NULL;
                    return code;
                }

                /* We have started a new substream, to avoid confusing the 'saved viewer state'
                 * (the stack of pdfwrite's saved copies of graophics states) we need to reset the
                 * soft_mask_id, which is the ID of the SMask we have already created in the pdfwrite
                 * output. The gsave/grestore round the spec_op to start and finish the pattern
                 * accumulator (see pattern_paint_prepare and pattern_paint_finish) will ensure that
                 * the ID is restored when we finish capturing the pattern.
                 */
                pdev->state.soft_mask_id = pgs->soft_mask_id;
                pres->rid = pinst->id;
                code = pdf_store_pattern1_params(pdev, pres, pinst);
                if (code < 0) {
                    gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdev->PatternDepth], "Freeing dangling pattern state");
                    pdev->initial_pattern_states[pdev->PatternDepth] = NULL;
                    return code;
                }
                /* Scale the coordinate system, because object handlers assume so. See none_to_stream. */
                pprintg2(pdev->strm, "%g 0 0 %g 0 0 cm\n",
                         72.0 / pdev->HWResolution[0], 72.0 / pdev->HWResolution[1]);
                pdev->PatternDepth++;
                pdev->PatternsSinceForm++;
            }
            return 1;
        case gxdso_pattern_finish_accum:
            {
                pattern_accum_param_s *param = (pattern_accum_param_s *)data;
                gs_gstate *pgs = param->graphics_state;

                if (pdev->CompatibilityLevel <= 1.7) {
                    if (pdev->substream_Resources == NULL) {
                        pdev->substream_Resources = cos_dict_alloc(pdev, "pdf_pattern(Resources)");
                        if (pdev->substream_Resources == NULL)
                            return_error(gs_error_VMerror);
                    }
                    code = pdf_add_procsets(pdev->substream_Resources, pdev->procsets);
                    if (code < 0) {
                        gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdev->PatternDepth], "Freeing dangling pattern state");
                        pdev->initial_pattern_states[pdev->PatternDepth] = NULL;
                        return code;
                    }
                }
                pres = pres1 = pdev->accumulating_substream_resource;
                code = pdf_exit_substream(pdev);
                if (code < 0) {
                    gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdev->PatternDepth], "Freeing dangling pattern state");
                    pdev->initial_pattern_states[pdev->PatternDepth] = NULL;
                    return code;
                }
                if (pdev->substituted_pattern_count > 300 &&
                        pdev->substituted_pattern_drop_page != pdev->next_page) { /* arbitrary */
                    pdf_drop_resources(pdev, resourcePattern, check_unsubstituted1);
                    pdev->substituted_pattern_count = 0;
                    pdev->substituted_pattern_drop_page = pdev->next_page;
                }
                code = pdf_find_same_resource(pdev, resourcePattern, &pres, check_unsubstituted2);
                if (code < 0) {
                    gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdev->PatternDepth], "Freeing dangling pattern state");
                    pdev->initial_pattern_states[pdev->PatternDepth] = NULL;
                    return code;
                }
                if (code > 0) {
                    pdf_pattern_t *ppat = (pdf_pattern_t *)pres1;

                    code = pdf_cancel_resource(pdev, pres1, resourcePattern);
                    if (code < 0) {
                        gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdev->PatternDepth], "Freeing dangling pattern state");
                        pdev->initial_pattern_states[pdev->PatternDepth] = NULL;
                        return code;
                    }
                    /* Do not remove pres1, because it keeps the substitution. */
                    ppat->substitute = (pdf_pattern_t *)pres;
                    pres->where_used |= pdev->used_mask;
                    pdev->substituted_pattern_count++;
                } else if (pres->object->id < 0)
                    pdf_reserve_object_id(pdev, pres, 0);
                reset_gstate_for_pattern(pdev, pgs, pdev->initial_pattern_states[pdev->PatternDepth - 1]);
                gs_free_object(pdev->pdf_memory, pdev->initial_pattern_states[pdev->PatternDepth - 1], "Freeing dangling pattern state");
                pdev->initial_pattern_states[pdev->PatternDepth - 1] = NULL;
                if (pdev->PatternDepth == 1) {
                    gs_free_object(pdev->pdf_memory->non_gc_memory, pdev->initial_pattern_states, "Freeing dangling pattern state");
                    pdev->initial_pattern_states = NULL;
                }

                pdev->PatternDepth--;
                pdev->PatternsSinceForm--;
            }
            return 1;
        case gxdso_pattern_load:
            pres = pdf_find_resource_by_gs_id(pdev, resourcePattern, *((gx_bitmap_id *)data));
            if (pres == 0)
                return 0;
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
        case gxdso_JPEG_passthrough_query:
            pdev->JPEG_PassThrough = pdev->params.PassThroughJPEGImages;
            return 1;
            break;
        case gxdso_JPEG_passthrough_begin:
            return 0;
            break;
        case gxdso_JPEG_passthrough_data:
            if (pdev->JPEG_PassThrough && pdev->PassThroughWriter)
            {
                uint ignore;
                if (sputs(pdev->PassThroughWriter,
                           data, size,
                           &ignore) < 0)
                           return_error(gs_error_ioerror);
            }
            return 0;
            break;
        case gxdso_JPEG_passthrough_end:
            pdev->JPEG_PassThrough = 0;
            pdev->PassThroughWriter = 0;
            return 0;
            break;
        case gxdso_JPX_passthrough_query:
            pdev->JPX_PassThrough = pdev->params.PassThroughJPXImages;
            return 1;
            break;
        case gxdso_JPX_passthrough_begin:
            return 0;
            break;
        case gxdso_JPX_passthrough_data:
            if (pdev->JPX_PassThrough && pdev->PassThroughWriter)
            {
                uint ignore;
                if (sputs(pdev->PassThroughWriter,
                           data, size,
                           &ignore) < 0)
                           return_error(gs_error_ioerror);
            }
            return 0;
            break;
        case gxdso_JPX_passthrough_end:
            pdev->JPX_PassThrough = 0;
            pdev->PassThroughWriter = 0;
            return 0;
            break;
        case gxdso_event_info:
            {
                dev_param_req_t *request = (dev_param_req_t *)data;
                if (memcmp(request->Param, "SubstitutedFont", 15) == 0 && (pdev->PDFA || pdev->PDFX)) {
                    switch (pdev->PDFACompatibilityPolicy) {
                        case 0:
                        case 1:
                            emprintf(pdev->memory,
                             "\n **** A font missing from the input PDF has been substituted with a different font.\n\tWidths may differ, reverting to normal PDF output!\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFX = 0;
                            pdev->PDFA = 0;
                            break;
                        case 2:
                            emprintf(pdev->memory,
                             "\n **** A font missing from the input PDF has been substituted with a different font.\n\tWidths may differ, aborting conversion!\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFX = 0;
                            pdev->PDFA = 0;
                            return gs_note_error(gs_error_unknownerror);
                            break;
                        default:
                            emprintf(pdev->memory,
                             "\n **** A font missing from the input PDF has been substituted with a different font.\n\tWidths may differ, unknown PDFACompatibilityPolicy, reverting to normal PDF output!\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFX = 0;
                            pdev->PDFA = 0;
                            break;
                    }
                }
                return 0;
            }
            break;
        case gxdso_in_smask_construction:
            return pdev->smask_construction;
        case gxdso_pending_optional_content:
            if (pdev->CompatibilityLevel < 1.4999) {
                if (pdev->PDFA) {
                    switch (pdev->PDFACompatibilityPolicy) {
                        case 0:
                            emprintf(pdev->memory,
                                     "Optional Content not valid in this version of PDF, reverting to normal PDF output\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFA = 0;
                            break;
                        case 1:
                            emprintf(pdev->memory,
                                     "Optional Content not valid in this version of PDF. Dropping feature to preserve PDF/A compatibility\n");
                            break;
                        case 2:
                            emprintf(pdev->memory,
                                     "Optional Content not valid in this version of PDF,  aborting conversion\n");
                            return_error (gs_error_typecheck);
                            break;
                        default:
                            emprintf(pdev->memory,
                                     "Optional Content not valid in this version of PDF, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFA = 0;
                            break;
                    }
                } else {
                    emprintf(pdev->memory,
                             "Optional Content not valid in this version of PDF. Dropping feature to preserve compatibility\n");
                }
            } else
            {
                int64_t *object = data;
                pdev->PendingOC = *object;
            }
            return 0;
            break;
        case gxdso_hilevel_text_clip:
            if (data == 0 && !pdev->accumulating_charproc) {
                /* We are exiting a text render mode 'clip' by grestoring back to
                 *  a time when the clip wasn't active.
                 * First, check if we have a clip (this should always be true).
                 */
                if (pdev->clipped_text_pending) {
                    /* Get back to the content stream. This will (amongst other things) flush
                     * any pending text.
                     */
                    code = pdf_open_page(pdev, PDF_IN_STREAM);
                    if (code < 0)
                        return code;
                    /* Reset the pending state */
                    pdev->clipped_text_pending = 0;
                    /* Restore to our saved state */

                    /* The saved state in this case is the dpeth of the saved gstate stack at the time we
                     * started the text clipping. Note; we cannot restore back past the 'bottom' of the
                     * stack, which is why we alter vgstack_bottom here, rather than just using the saved
                     * level in the loop below.
                     */
                    if (pdev->vgstack_bottom)
                        pdev->vgstack_bottom = pdev->saved_vgstack_depth_for_textclip;

                    while (pdev->vgstack_depth > pdev->vgstack_bottom) {
                        code = pdf_restore_viewer_state(pdev, pdev->strm);
                        if (code < 0)
                            return code;
                    }

                    pdev->vgstack_bottom = pdev->saved_vgstack_bottom_for_textclip;
                    pdf_reset_text(pdev);	/* because of Q */
                }
            } else {
                if (!pdev->accumulating_charproc) {
                    gs_gstate *pgs = (gs_gstate *)data;
                    /* We are starting text in a clip mode
                     * First make sure we aren't already in a clip mode (this should never be true)
                     */
                    if (!pdev->clipped_text_pending) {
                        /* Return to the content stream, this will (amongst other things) flush
                         * any pending text.
                         */
                        code = pdf_open_page(pdev, PDF_IN_STREAM);
                        if (code < 0)
                            return code;

                        if (pdf_must_put_clip_path(pdev, pgs->clip_path)) {
                           code = pdf_unclip(pdev);
                            if (code < 0)
                                return code;
                            code = pdf_put_clip_path(pdev, pgs->clip_path);
                            if (code < 0)
                                return code;
                        }

                        pdev->saved_vgstack_depth_for_textclip = pdev->vgstack_depth;

                        /* Save the current graphics state (or at least that bit which we track) so
                         * that we can put it back later.
                         */
                        code = pdf_save_viewer_state(pdev, pdev->strm);
                        if (code < 0)
                            return code;
                        pdev->clipped_text_pending = 1;

                        /* Save the current 'bottom' of the saved gstate stack, we need to
                         * restore back to this state when we exit the graphics state
                         * with a text rendering mode involving a clip.
                         */
                        pdev->saved_vgstack_bottom_for_textclip = pdev->vgstack_bottom;
                        /* And push the bottom of the stack up until it is where we are now.
                         * This is because clip paths, images, and possibly other constructs
                         * will emit a clip path if the 'depth - bottom' is not zero, to create
                         * a clip path. We want to make sure that it doesn't try to restore back
                         * to a point before we established the text clip.
                         */
                        pdev->vgstack_bottom = pdev->vgstack_depth;
                    }
                }
            }
            break;
        case gxdso_get_dev_param:
            {
                int code;
                dev_param_req_t *request = (dev_param_req_t *)data;
                code = gdev_pdf_get_param(pdev1, request->Param, request->list);
                if (code != gs_error_undefined)
                    return code;
            }
            /* Fall through */
    }
    return gx_default_dev_spec_op(pdev1, dev_spec_op, data, size);
}
