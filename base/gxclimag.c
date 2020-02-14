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


/* Higher-level image operations for band lists */
#include "math_.h"
#include "memory_.h"
#include "string_.h"		/* for strcmp */
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gscdefs.h"            /* for image type table */
#include "gxarith.h"
#include "gxcspace.h"
#include "gxpcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"           /* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gxfmap.h"
#include "gxiparam.h"
#include "gxpath.h"
#include "stream.h"
#include "strimpl.h"            /* for sisparam.h */
#include "sisparam.h"
#include "gxcomp.h"
#include "gsserial.h"
#include "gxdhtserial.h"
#include "gsptype1.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "gxdevsop.h"
#include "gscindex.h"
#include "gsicc_cms.h"
#include "gximdecode.h"

extern_gx_image_type_table();

/* Define whether we should use high-level images. */
/* (See below for additional restrictions.) */
static const bool USE_HL_IMAGES = true;

/* Forward references */
static int cmd_put_set_data_x(gx_device_clist_writer * cldev,
                               gx_clist_state * pcls, int data_x);
static bool check_rect_for_trivial_clip(
    const gx_clip_path *pcpath,  /* May be NULL, clip to evaluate */
    int px, int py, int qx, int qy  /* corners of box to test */
);

static bool
palette_has_color(const gs_color_space *pcs, const gs_pixel_image_t * const pim)
{
    gs_color_space *pbcs = pcs->base_space;
    gs_color_space_index base_type = gs_color_space_get_index(pbcs);
    bool ((*is_neutral)(void*, int));
    int bps = pim->BitsPerComponent;
    int num_entries = 1 << bps;
    int k;
    byte psrc[4];

    switch(base_type) {

    case gs_color_space_index_DeviceGray:
    case gs_color_space_index_CIEA:
        return false;
        break;

    case gs_color_space_index_DeviceRGB:
    case gs_color_space_index_CIEABC:
    case gs_color_space_index_CIEDEF:
        is_neutral = &gsicc_mcm_monitor_rgb;
        break;

    case gs_color_space_index_DeviceCMYK:
    case gs_color_space_index_CIEDEFG:
        is_neutral = &gsicc_mcm_monitor_cmyk;
        break;

     case gs_color_space_index_DevicePixel:
     case gs_color_space_index_DeviceN:
     case gs_color_space_index_Separation:
     case gs_color_space_index_Indexed:
     case gs_color_space_index_Pattern:
        return true;
        break;

     case gs_color_space_index_ICC:
        switch(pbcs->cmm_icc_profile_data->data_cs) {
        case gsRGB:
            is_neutral = &gsicc_mcm_monitor_rgb;
            break;

        case gsCMYK:
            is_neutral = &gsicc_mcm_monitor_cmyk;
            break;

        case gsCIELAB:
            is_neutral = &gsicc_mcm_monitor_lab;
            break;

        default:
            return true;
        }
        break;
     default:
        return true;
    }
    /* Now go through the palette with the check color function */
    for (k = 0; k < num_entries; k++) {
        (void)gs_cspace_indexed_lookup_bytes(pcs, (float) k, psrc); /* this always returns 0 */
        if (!is_neutral(psrc, 1)) {
            /* Has color end this now */
            return true;
        }
    }
    /* Must not have color */
    return false;
}


/* ------ Driver procedures ------ */

int
clist_fill_mask(gx_device * dev,
                const byte * data, int data_x, int raster, gx_bitmap_id id,
                int rx, int ry, int rwidth, int rheight,
                const gx_drawing_color * pdcolor, int depth,
                gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    const byte *orig_data = data;       /* for writing tile */
    int orig_data_x = data_x;   /* ditto */
    int orig_x = rx;            /* ditto */
    int orig_width = rwidth;    /* ditto */
    int orig_height = rheight;  /* ditto */
    int y0;
    byte copy_op =
        (depth > 1 ? cmd_op_copy_color_alpha :
         cmd_op_copy_mono_planes);  /* Plane not needed here */
    bool slow_rop =
        cmd_slow_rop(dev, lop_know_S_0(lop), pdcolor) ||
        cmd_slow_rop(dev, lop_know_S_1(lop), pdcolor);
    cmd_rects_enum_t re;

    /* If depth > 1, this call will be translated to a copy_alpha call. */
    /* if the target device can't perform copy_alpha, exit now. */
    if (depth > 1 && (cdev->disable_mask & clist_disable_copy_alpha) != 0)
        return_error(gs_error_unknownerror);

    crop_copy(cdev, data, data_x, raster, id, rx, ry, rwidth, rheight);
    if (rwidth <= 0 || rheight <= 0)
        return 0;
    y0 = ry;                    /* must do after fit_copy */

    /* If non-trivial clipping & complex clipping disabled, default */
    /* Also default for uncached bitmap or non-default lop; */
    /* We could handle more RasterOp cases here directly, but it */
    /* doesn't seem worth the trouble right now. */
    /* Lastly, the command list will translate calls with depth > 1 to */
    /* copy_alpha calls, so the device color must be pure */
    if (((cdev->disable_mask & clist_disable_complex_clip) &&
         !check_rect_for_trivial_clip(pcpath, rx, ry, rx + rwidth, ry + rheight)) ||
        gs_debug_c('`') || id == gx_no_bitmap_id || lop != lop_default ||
        (depth > 1 && !color_writes_pure(pdcolor, lop))
        )
  copy:
        return gx_default_fill_mask(dev, data, data_x, raster, id,
                                    rx, ry, rwidth, rheight, pdcolor, depth,
                                    lop, pcpath);

    if (cmd_check_clip_path(cdev, pcpath))
        cmd_clear_known(cdev, clip_path_known);
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    /* If needed, update the trans_bbox */
    if (cdev->pdf14_needed) {
        gs_int_rect bbox;

        bbox.p.x = rx;
        bbox.q.x = rx + rwidth - 1;
        bbox.p.y = ry;
        bbox.q.y = ry + rheight - 1;

        clist_update_trans_bbox(cdev, &bbox);
    }
    RECT_ENUM_INIT(re, ry, rheight);
    do {
        int code;
        ulong offset_temp;

        RECT_STEP_INIT(re);
        code = cmd_update_lop(cdev, re.pcls, lop);
        if (code < 0)
            return code;
        if (depth > 1 && !re.pcls->color_is_alpha) {
            byte *dp;

            code = set_cmd_put_op(&dp, cdev, re.pcls, cmd_opv_set_copy_alpha, 1);
            if (code < 0)
                return code;
            re.pcls->color_is_alpha = 1;
        }
        code = cmd_do_write_unknown(cdev, re.pcls, clip_path_known);
        if (code >= 0)
            code = cmd_do_enable_clip(cdev, re.pcls, pcpath != NULL);
        if (code < 0)
            return code;
        code = cmd_put_drawing_color(cdev, re.pcls, pdcolor, &re,
                                     devn_not_tile_fill);
        if (code == gs_error_unregistered)
            return code;
        if (depth > 1 && code >= 0)
            code = cmd_set_color1(cdev, re.pcls, pdcolor->colors.pure);
        if (code < 0)
            return code;
        re.pcls->color_usage.slow_rop |= slow_rop;
        /* Put it in the cache if possible. */
        if (!cls_has_tile_id(cdev, re.pcls, id, offset_temp)) {
            gx_strip_bitmap tile;

            tile.data = (byte *) orig_data;     /* actually const */
            tile.raster = raster;
            tile.size.x = tile.rep_width = orig_width;
            tile.size.y = tile.rep_height = orig_height;
            tile.rep_shift = tile.shift = 0;
            tile.id = id;
            tile.num_planes = 1;
            code = clist_change_bits(cdev, re.pcls, &tile, depth);
            if (code < 0) {
                /* Something went wrong; just copy the bits. */
                goto copy;
            }
        }
        {
            gx_cmd_rect rect;
            int rsize;
            byte op = copy_op + cmd_copy_use_tile;

            /* Output a command to copy the entire character. */
            /* It will be truncated properly per band. */
            rect.x = orig_x, rect.y = y0;
            rect.width = orig_width, rect.height = re.yend - y0;
            rsize = 1 + cmd_sizexy(rect);
            if (depth == 1) rsize = rsize + cmd_sizew(0);  /* need planar_height 0 setting */
            code = (orig_data_x ?
                    cmd_put_set_data_x(cdev, re.pcls, orig_data_x) : 0);
            if (code >= 0) {
                byte *dp;

                code = set_cmd_put_op(&dp, cdev, re.pcls, op, rsize);
                /*
                 * The following conditional is unnecessary: the two
                 * statements inside it should go outside the
                 * HANDLE_RECT.  They are here solely to pacify
                 * stupid compilers that don't understand that dp
                 * will always be set if control gets past the
                 * HANDLE_RECT.
                 */
                if (code >= 0) {
                    dp++;
                    if (depth == 1) {
                        cmd_putw(0, &dp);
                    }
                    cmd_putxy(rect, &dp);
                }
            }
            if (code < 0)
                return code;
            re.pcls->rect = rect;
        }
    } while ((re.y += re.height) < re.yend);
    return 0;
}

/* ------ Bitmap image driver procedures ------ */

/* Define the structure for keeping track of progress through an image. */
typedef struct clist_image_enum_s {
    gx_image_enum_common;
    /* Arguments of begin_image */
    gs_pixel_image_t image;     /* only uses Width, Height, Interpolate */
    gx_drawing_color dcolor;    /* only pure right now */
    gs_int_rect rect;
    const gs_gstate *pgs;
    const gx_clip_path *pcpath;
    /* Set at creation time */
    gs_image_format_t format;
    gs_int_point support;       /* extra source pixels for interpolation */
    int bits_per_plane;         /* bits per pixel per plane */
    gs_matrix matrix;           /* image space -> device space */
    bool uses_color;
    clist_color_space_t color_space;
    int ymin, ymax;
    gx_color_usage_t color_usage;
    /* begin_image command prepared & ready to output */
    /****** SIZE COMPUTATION IS WRONG, TIED TO gximage.c, gsmatrix.c ******/
    byte begin_image_command[3 +
                            /* Width, Height */
                            2 * cmd_sizew_max +
                            /* ImageMatrix */
                            1 + 6 * sizeof(float) +
                            /* Decode */
                            (GS_IMAGE_MAX_COMPONENTS + 3) / 4 +
                              GS_IMAGE_MAX_COMPONENTS * 2 * sizeof(float) +
                            /* MaskColors */
                            GS_IMAGE_MAX_COMPONENTS * cmd_sizew_max +
                            /* rect */
                            4 * cmd_sizew_max];
    int begin_image_command_length;
    /* Updated dynamically */
    int y;
    bool color_map_is_known;
    bool monitor_color;
    image_decode_t decode;
    byte *buffer;  /* needed for unpacking during monitoring */
} clist_image_enum;
gs_private_st_suffix_add4(st_clist_image_enum, clist_image_enum,
                          "clist_image_enum", clist_image_enum_enum_ptrs,
                          clist_image_enum_reloc_ptrs, st_gx_image_enum_common,
                          pgs, pcpath, color_space.space, buffer);

static image_enum_proc_plane_data(clist_image_plane_data);
static image_enum_proc_end_image(clist_image_end_image);
static const gx_image_enum_procs_t clist_image_enum_procs =
{
    clist_image_plane_data, clist_image_end_image
};

/* data_size is number of bytes per component, width is number of pixels in the row. */
static bool
row_has_color(byte *data_ptr, clist_image_enum *pie_c, int data_size, int width)
{
    clist_color_space_t pclcs = pie_c->color_space;
    bool ((*is_neutral)(void*, int));
    int step_size = data_size * pie_c->decode.spp;
    byte *ptr;
    bool is_mono;
    int k;

    if (pclcs.icc_info.is_lab) {
        is_neutral = &gsicc_mcm_monitor_lab;
    } else {
        switch(pclcs.icc_info.icc_num_components) {
        case 3:
            is_neutral = &gsicc_mcm_monitor_rgb;
            break;
        case 4:
            is_neutral = &gsicc_mcm_monitor_cmyk;
            break;
        default:
            return true;
        }
    }
    /* Now go through the raster line and determine if we have any color. */
    ptr = data_ptr;
    for (k = 0; k < width; k++) {
        is_mono = is_neutral(ptr, data_size);
        if (!is_mono) {
            return true;
        }
        ptr += step_size;
    }
    return false;
}

/* Forward declarations */
static bool image_band_box(gx_device * dev, const clist_image_enum * pie,
                            int y, int h, gs_int_rect * pbox);
static int begin_image_command(byte *buf, uint buf_size,
                                const gs_image_common_t *pic);
static int cmd_image_plane_data(gx_device_clist_writer * cldev,
                                 gx_clist_state * pcls,
                                 const gx_image_plane_t * planes,
                                 const gx_image_enum_common_t * pie,
                                 uint bytes_per_plane,
                                 const uint * offsets, int dx, int h);
static int cmd_image_plane_data_mon(gx_device_clist_writer * cldev,
                                 gx_clist_state * pcls,
                                 const gx_image_plane_t * planes,
                                 const gx_image_enum_common_t * pie,
                                 uint bytes_per_plane,
                                 const uint * offsets, int dx, int h,
                                 bool *found_color);
static uint clist_image_unknowns(gx_device *dev,
                                  const clist_image_enum *pie);
static int write_image_end_all(gx_device *dev,
                                const clist_image_enum *pie);

/*
 * Since currently we are limited to writing a single subrectangle of the
 * image for each band, images that are rotated by angles other than
 * multiples of 90 degrees may wind up writing many copies of the data.
 * Eventually we will fix this by breaking up the image into multiple
 * subrectangles, but for now, don't use the high-level approach if it would
 * cause the data to explode because of this.
 */
static bool
image_matrix_ok_to_band(const gs_matrix * pmat)
{
    double t;
    /* Detecting a downscale when it's really noscale upsets some
     * customers code, so we add a fudge factor in here. This may
     * cause us to allow the use of high level images for some downscales
     * that are *nearly* noscales, but our code will cope with that. */
    float one = (float)(1.0 - 1e-5);

    /* Don't band if the matrix is (nearly) singular. */
    if (fabs(pmat->xx * pmat->yy - pmat->xy * pmat->yx) < 0.001)
        return false;
    /* If it's portrait, then we encode it if not a downscale */
    if (is_xxyy(pmat))
        return (fabs(pmat->xx) >= one) && (fabs(pmat->yy) >= one);
    /* If it's landscape, then we encode it if not a downscale */
    if (is_xyyx(pmat))
        return (fabs(pmat->xy) >= one) && (fabs(pmat->yx) >= one);
    /* Skewed, so do more expensive downscale test */
    if ((pmat->xx * pmat->xx + pmat->xy * pmat->xy < one) ||
        (pmat->yx * pmat->yx + pmat->yy * pmat->yy < one))
        return false;
    /* Otherwise only encode it if it doesn't rotate too much */
    t = (fabs(pmat->xx) + fabs(pmat->yy)) /
        (fabs(pmat->xy) + fabs(pmat->yx));
    return (t < 0.2 || t > 5);
}

/* Start processing an image. */
int
clist_begin_typed_image(gx_device * dev, const gs_gstate * pgs,
                        const gs_matrix * pmat, const gs_image_common_t * pic,
                        const gs_int_rect * prect, const gx_drawing_color * pdcolor,
                        const gx_clip_path * pcpath, gs_memory_t * mem,
                        gx_image_enum_common_t ** pinfo)
{
    const gs_pixel_image_t * const pim = (const gs_pixel_image_t *)pic;
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    clist_image_enum *pie = 0;
    int base_index;
    bool indexed;
    bool masked = false;
    bool has_alpha = false;
    int num_components;
    int bits_per_pixel;
    bool uses_color;
    bool varying_depths = false;
    gs_matrix mat;
    gs_rect sbox, dbox;
    gs_image_format_t format;
    gx_color_usage_bits color_usage = 0;
    int code;
    bool mask_use_hl;
    clist_icc_color_t icc_zero_init = { 0 };
    cmm_profile_t *src_profile;
    cmm_srcgtag_profile_t *srcgtag_profile;
    gsicc_rendering_intents_t renderingintent = pgs->renderingintent;
    gsicc_blackptcomp_t blackptcomp = pgs->blackptcomp;
    gsicc_rendering_param_t stored_rendering_cond;
    gsicc_rendering_param_t dev_render_cond;
    gs_gstate *pgs_nonconst = (gs_gstate*) pgs;
    bool intent_changed = false;
    bool bp_changed = false;
    cmm_dev_profile_t *dev_profile = NULL;
    cmm_profile_t *gs_output_profile;
    bool is_planar_dev = dev->is_planar;
    bool render_is_valid;
    int csi;

    /* We can only handle a limited set of image types. */
    switch ((gs_debug_c('`') ? -1 : pic->type->index)) {
    case 1:
        masked = ((const gs_image1_t *)pim)->ImageMask;
        has_alpha = ((const gs_image1_t *)pim)->Alpha != 0;
        /* fall through */
    case 4:
        if (pmat == 0)
            break;
    default:
        goto use_default;
    }
    format = pim->format;
    /* See above for why we allocate the enumerator as immovable. */
    pie = gs_alloc_struct_immovable(mem, clist_image_enum,
                                    &st_clist_image_enum,
                                    "clist_begin_typed_image");
    if (pie == 0)
        return_error(gs_error_VMerror);
#ifdef PACIFY_VALGRIND
    /* The following memset is required to avoid a valgrind warning
     * in:
     *   gs -I./gs/lib -sOutputFile=out.pgm -dMaxBitmap=10000
     *      -sDEVICE=pgmraw -r300 -Z: -sDEFAULTPAPERSIZE=letter
     *      -dNOPAUSE -dBATCH -K2000000 -dClusterJob -dJOBSERVER
     *      tests_private/ps/ps3cet/11-14.PS
     * Setting the individual elements of the structure directly is
     * not enough, which leads me to believe that we are writing the
     * entire struct out, padding and all.
     */
    memset(&pie->color_space.icc_info, 0, sizeof(pie->color_space.icc_info));
#endif
    pie->memory = mem;
    pie->buffer = NULL;
    *pinfo = (gx_image_enum_common_t *) pie;
    /* num_planes and plane_depths[] are set later, */
    /* by gx_image_enum_common_init. */
    if (masked) {
        base_index = gs_color_space_index_DeviceGray;   /* arbitrary */
        indexed = false;
        num_components = 1;
        uses_color = true;
        /* cmd_put_drawing_color handles color_usage */
    } else {
        const gs_color_space *pcs = pim->ColorSpace;

        base_index = gs_color_space_get_index(pcs);
        if (base_index == gs_color_space_index_Indexed) {
            const gs_color_space *pbcs =
                gs_color_space_indexed_base_space(pcs);

            indexed = true;
            base_index = gs_color_space_get_index(pbcs);
            num_components = 1;
        } else {
            indexed = false;
            num_components = gs_color_space_num_components(pcs);
        }
        uses_color = pim->CombineWithColor &&
                    (rop3_uses_T(pgs->log_op) || rop3_uses_S(pgs->log_op));
    }
    code = gx_image_enum_common_init((gx_image_enum_common_t *) pie,
                                     (const gs_data_image_t *) pim,
                                     &clist_image_enum_procs, dev,
                                     num_components, format);
    {
        int i;

        for (i = 1; i < pie->num_planes; ++i)
            varying_depths |= pie->plane_depths[i] != pie->plane_depths[0];
    }

    /* Now, check to see if we can't handle this as a high level image. */
    if (code < 0)
        goto use_default;
    if (!USE_HL_IMAGES) /* Always use the default. */
        goto use_default;
    if (cdev->disable_mask & clist_disable_hl_image)
        goto use_default;
    if (cdev->image_enum_id != gs_no_id) /* Can't handle nested images */
        goto use_default;
    if (base_index > gs_color_space_index_DeviceCMYK &&
        base_index != gs_color_space_index_ICC)
        /****** Can only handle Gray, RGB, CMYK and ICC ******/
        goto use_default;
    if (has_alpha)
        /****** CAN'T HANDLE IMAGES WITH ALPHA YET ******/
        goto use_default;
    if (varying_depths)
        /****** CAN'T HANDLE IMAGES WITH IRREGULAR DEPTHS ******/
        goto use_default;
    if ((code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
        (code = gs_matrix_multiply(&mat, &ctm_only(pgs), &mat)) < 0 ||
        !(cdev->disable_mask & clist_disable_nonrect_hl_image ?
          (is_xxyy(&mat) || is_xyyx(&mat)) :
          image_matrix_ok_to_band(&mat)))
        goto use_default;

    mask_use_hl =
        masked && ( gx_dc_is_pattern1_color(pdcolor) || gx_dc_is_pure(pdcolor) );
    if (!mask_use_hl && uses_color && !gx_dc_is_pure(pdcolor) &&
             !gx_dc_is_pattern1_color_clist_based(pdcolor))
        /* Only add in masks that are pure or pattern or pattern trans types */
        goto use_default;

    /* We've passed the tests; code it as a high level image */
    {
        int bytes_per_plane, bytes_per_row;

        bits_per_pixel = pim->BitsPerComponent * num_components;
        pie->decode.bps = bits_per_pixel/num_components;
        pie->decode.spp = num_components;
        pie->image = *pim;
        pie->dcolor = *pdcolor;
        if (prect)
            pie->rect = *prect;
        else {
            pie->rect.p.x = 0, pie->rect.p.y = 0;
            pie->rect.q.x = pim->Width, pie->rect.q.y = pim->Height;
        }
        pie->pgs = pgs;
        pie->pcpath = pcpath;
        pie->buffer = NULL;
        pie->format = format;
        pie->bits_per_plane = bits_per_pixel / pie->num_planes;
        pie->matrix = mat;
        pie->uses_color = uses_color;
        if (masked) {
            pie->color_space.byte1 = 0;  /* arbitrary */
            pie->color_space.icc_info = icc_zero_init;
            pie->color_space.space = 0;
            pie->color_space.id = gs_no_id;
        } else {
            /* Check for presence of ICC profiles in standard Device Color Spaces
               This can happen if a default space was initialized. It should
               typically have assigned to it one of the default ICC profiles */
            if (indexed) {
                if (pim->ColorSpace->base_space->cmm_icc_profile_data) {
                    base_index = gs_color_space_index_ICC;
                }
            } else {
                if (pim->ColorSpace->cmm_icc_profile_data) {
                    base_index = gs_color_space_index_ICC;
                }
            }
            pie->color_space.byte1 = (base_index << 4) |
                (indexed ? (pim->ColorSpace->params.indexed.use_proc ? 12 : 8) : 0);
            pie->color_space.id =
                (pie->color_space.space = pim->ColorSpace)->id;
            /* Get the hash code of the ICC space */
            if ( base_index == gs_color_space_index_ICC ) {
                code = dev_proc(dev, get_profile)(dev,  &dev_profile);
                gsicc_extract_profile(dev->graphics_type_tag, dev_profile,
                                      &(gs_output_profile),
                                      (&(dev_render_cond)));
                if (!indexed) {
                    src_profile = pim->ColorSpace->cmm_icc_profile_data;
                } else {
                    src_profile =
                        pim->ColorSpace->base_space->cmm_icc_profile_data;
                }
                /* Initialize the rendering conditions to what we currently
                   have before we may blow them away with what is set in
                   the srcgtag information */
                stored_rendering_cond.graphics_type_tag = GS_IMAGE_TAG;
                stored_rendering_cond.override_icc =
                                dev_render_cond.override_icc;
                stored_rendering_cond.preserve_black =
                                dev_render_cond.preserve_black;
                stored_rendering_cond.cmm = gsCMM_DEFAULT;  /* Unless spec. below */
                /* We may need to do some substitions for the source profile */
                if (pgs->icc_manager->srcgtag_profile != NULL) {
                    srcgtag_profile = pgs->icc_manager->srcgtag_profile;
                    if (src_profile->data_cs == gsRGB) {
                        if (srcgtag_profile->rgb_profiles[gsSRC_IMAGPRO] != NULL) {
                            /* We only do this replacement depending upon the
                               ICC override setting for this object and the
                               original color space of this object */
                            csi = gsicc_get_default_type(src_profile);
                            if (srcgtag_profile->rgb_rend_cond[gsSRC_IMAGPRO].override_icc ||
                                csi == gs_color_space_index_DeviceRGB) {
                                src_profile =
                                    srcgtag_profile->rgb_profiles[gsSRC_IMAGPRO];
                                pgs_nonconst->renderingintent =
                                    srcgtag_profile->rgb_rend_cond[gsSRC_IMAGPRO].rendering_intent;
                                pgs_nonconst->blackptcomp =
                                    srcgtag_profile->rgb_rend_cond[gsSRC_IMAGPRO].black_point_comp;
                                stored_rendering_cond =
                                    srcgtag_profile->rgb_rend_cond[gsSRC_IMAGPRO];
                            }
                        } else {
                            /* A possible do not use CM case */
                            stored_rendering_cond.cmm =
                                srcgtag_profile->rgb_rend_cond[gsSRC_IMAGPRO].cmm;
                        }
                    } else if (src_profile->data_cs == gsCMYK) {
                        if (srcgtag_profile->cmyk_profiles[gsSRC_IMAGPRO] != NULL) {
                            csi = gsicc_get_default_type(src_profile);
                            if (srcgtag_profile->cmyk_rend_cond[gsSRC_IMAGPRO].override_icc ||
                                csi == gs_color_space_index_DeviceCMYK) {
                                src_profile =
                                    srcgtag_profile->cmyk_profiles[gsSRC_IMAGPRO];
                                pgs_nonconst->renderingintent =
                                    srcgtag_profile->cmyk_rend_cond[gsSRC_IMAGPRO].rendering_intent;
                                pgs_nonconst->blackptcomp =
                                    srcgtag_profile->cmyk_rend_cond[gsSRC_IMAGPRO].black_point_comp;
                                stored_rendering_cond =
                                    srcgtag_profile->cmyk_rend_cond[gsSRC_IMAGPRO];
                            }
                        } else {
                            /* A possible do not use CM case */
                            stored_rendering_cond.cmm =
                                srcgtag_profile->cmyk_rend_cond[gsSRC_IMAGPRO].cmm;
                        }
                    }
                }
                /* If the device RI is set and we are not  setting the RI from
                   the source structure, then override any RI specified in the
                   document by the RI specified in the device */
                if (!(pgs_nonconst->renderingintent & gsRI_OVERRIDE)) {  /* was set by source? */
                    /* No it was not.  See if we should override with the
                       device setting */
                    if (dev_render_cond.rendering_intent != gsRINOTSPECIFIED) {
                        pgs_nonconst->renderingintent =
                                        dev_render_cond.rendering_intent;
                        }
                }
                /* We have a similar issue to deal with with respect to the
                   black point.  */
                if (!(pgs_nonconst->blackptcomp & gsBP_OVERRIDE)) {
                    if (dev_render_cond.black_point_comp != gsBPNOTSPECIFIED) {
                        pgs_nonconst->blackptcomp =
                                            dev_render_cond.black_point_comp;
                    }
                }
                if (renderingintent != pgs_nonconst->renderingintent)
                    intent_changed = true;
                if (blackptcomp != pgs_nonconst->blackptcomp)
                    bp_changed = true;
                /* Set for the rendering param structure also */
                stored_rendering_cond.rendering_intent =
                                                pgs_nonconst->renderingintent;
                stored_rendering_cond.black_point_comp =
                                                pgs_nonconst->blackptcomp;
                stored_rendering_cond.graphics_type_tag = GS_IMAGE_TAG;
                if (!(src_profile->hash_is_valid)) {
                    int64_t hash;
                    gsicc_get_icc_buff_hash(src_profile->buffer, &hash,
                                            src_profile->buffer_size);
                    src_profile->hashcode = hash;
                    src_profile->hash_is_valid = true;
                }
                pie->color_space.icc_info.icc_hash = src_profile->hashcode;
                pie->color_space.icc_info.icc_num_components =
                    src_profile->num_comps;
                pie->color_space.icc_info.is_lab = src_profile->islab;
                pie->color_space.icc_info.default_match = src_profile->default_match;
                pie->color_space.icc_info.data_cs = src_profile->data_cs;
                src_profile->rend_cond = stored_rendering_cond;
                render_is_valid = src_profile->rend_is_valid;
                src_profile->rend_is_valid = true;
                clist_icc_addentry(cdev, src_profile->hashcode, src_profile);
                src_profile->rend_is_valid = render_is_valid;
            } else {
                pie->color_space.icc_info = icc_zero_init;
            }
        }
        pie->y = pie->rect.p.y;
        /* Image row has to fit in cmd writer's buffer */
        bytes_per_plane =
            (pim->Width * pie->bits_per_plane + 7) >> 3;
        bytes_per_row = bytes_per_plane * pie->num_planes;
        bytes_per_row = max(bytes_per_row, 1);
        if (cmd_largest_size + bytes_per_row > cdev->cend - cdev->cbuf)
            goto use_default;
    }
    if (pim->Interpolate) {
        pie->support.x = pie->support.y = MAX_ISCALE_SUPPORT + 1;
    } else {
        pie->support.x = pie->support.y = 0;
    }
    sbox.p.x = pie->rect.p.x - pie->support.x;
    sbox.p.y = pie->rect.p.y - pie->support.y;
    sbox.q.x = pie->rect.q.x + pie->support.x;
    sbox.q.y = pie->rect.q.y + pie->support.y;
    gs_bbox_transform(&sbox, &mat, &dbox);

    if (cdev->disable_mask & clist_disable_complex_clip)
        if (!check_rect_for_trivial_clip(pcpath,
                                (int)floor(dbox.p.x), (int)floor(dbox.p.y),
                                (int)ceil(dbox.q.x), (int)ceil(dbox.q.y)))
            goto use_default;

    /* If we are going out to a halftone device and the size of the stored
       image at device resolution and color space is going to be smaller,
       go ahead and do the default handler. This occurs only for planar
       devices where if we prerender we will end up doing the fast theshold
       halftone and going out as copy_planes commands into the clist.
       There is already a test above with regard to the posture so that
       we are only doing portrait or landscape cases if we are here.  Only
       question is penum->image_parent_type == gs_image_type1 */
    if (dev_profile == NULL) {
        gsicc_rendering_param_t temp_render_cond;
        code = dev_proc(dev, get_profile)(dev,  &dev_profile);
        if (code < 0)
            return code;
        gsicc_extract_profile(dev->graphics_type_tag, dev_profile,
                                              &(gs_output_profile),
                                              &(temp_render_cond));
    }
    /* Decide if we need to do any monitoring of the colors.  Note that multiple source
       (planes) is treated as color */
    pie->decode.unpack = NULL;
    if (dev_profile->pageneutralcolor && pie->color_space.icc_info.data_cs != gsGRAY) {
        /* If it is an index image, then check the pallete only */
        if (!indexed) {
            pie->monitor_color = true;
            /* Set up the unpacking proc for monitoring */
            get_unpack_proc((gx_image_enum_common_t*) pie, &(pie->decode), 
                             pim->format, pim->Decode);
            get_map(&(pie->decode), pim->format, pim->Decode);
            if (pie->decode.unpack == NULL) {
                /* If we cant unpack, then end monitoring now. Treat as has color */
                dev_profile->pageneutralcolor = false;
                code = gsicc_mcm_end_monitor(pgs->icc_link_cache, dev);
                if (code < 0)
                    return code;
            } else {
                /* We need to allocate the buffer for unpacking during monitoring.
                    This is mainly for the 12bit case */
                int bsize = ((pie->decode.bps > 8 ? (pim->Width) * 2 : pim->Width) + 15) * num_components;
                pie->buffer = gs_alloc_bytes(mem, bsize, "image buffer");
                if (pie->buffer == 0) {
                    gs_free_object(mem, pie, "clist_begin_typed_image");
                    *pinfo = NULL;
                    return_error(gs_error_VMerror);
                }
            }
        } else {
            pie->monitor_color = false;
            /* Check the Palette here */
            if (palette_has_color(pim->ColorSpace, pim)) {
                /* Has color.  We are done monitoring */
                dev_profile->pageneutralcolor = false;
                code = gsicc_mcm_end_monitor(pgs->icc_link_cache, dev);
                if (code < 0)
                    return code;
            }
        }
    } else {
        pie->monitor_color = false;
    }
    if (gx_device_must_halftone(dev) && pim->BitsPerComponent == 8 && !masked &&
        (dev->color_info.num_components == 1 || is_planar_dev) &&
        dev_profile->prebandthreshold) {
        int dev_width = (int)(ceil(dbox.q.x) - floor(dbox.p.x));
        int dev_height = (int)(ceil(dbox.q.y) - floor(dbox.p.y));

        int src_size = pim->Height *
                       bitmap_raster(pim->Width * pim->BitsPerComponent *
                                     num_components);
        int des_size = dev_height * bitmap_raster(dev_width *
                                                  dev->color_info.depth);
        if (src_size > des_size)
            goto use_default;
    }
    /* Create the begin_image command. */
    if ((pie->begin_image_command_length =
         begin_image_command(pie->begin_image_command,
                             sizeof(pie->begin_image_command), pic)) < 0)
        goto use_default;
    if (!masked) {
        /*
         * Calculate (conservatively) the set of colors that this image
         * might generate.  For single-component images we can sample
         * this. We generate all the possible colors now; otherwise,
         * we assume that any color might be generated.  It is possible
         * to do better than this, but we won't bother unless there's
         * evidence that it's worthwhile.
         */
        gx_color_usage_bits all = gx_color_usage_all(cdev);

        if (num_components > 1)
            color_usage = all;
        else {
            const gs_color_space *pcs = pim->ColorSpace;
            cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
            gs_client_color cc;
            gx_drawing_color dcolor;
            int i;
            int max_value = indexed ? pcs->params.indexed.hival : 1;

            for (i = 0; i <= max_value; ++i) {
                /* Enumerate the indexed colors, or just Black (DeviceGray = 0) */
                cc.paint.values[0] = (double)i;
                remap_color(&cc, pcs, &dcolor, pgs, dev,
                            gs_color_select_source);
                color_usage |= cmd_drawing_color_usage(cdev, &dcolor);
            }
        }
    }
    pie->color_usage.or = color_usage;
    pie->color_usage.slow_rop =
        cmd_slow_rop(dev, pgs->log_op, (uses_color ? pdcolor : NULL));
    pie->color_map_is_known = false;
    /*
     * Calculate a (slightly conservative) Y bounding interval for the image
     * in device space.
     */
    {
        int y0 = (int)floor(dbox.p.y - 0.51);   /* adjust + rounding slop */
        int y1 = (int)ceil(dbox.q.y + 0.51);    /* ditto */

        if (pcpath) {
            gs_fixed_rect obox;
            gx_cpath_outer_box(pcpath, &obox);
            pie->ymin = max(0, max(y0, fixed2int(obox.p.y)));
            pie->ymax = min(min(y1, fixed2int(obox.q.y)), dev->height);
        } else {
            pie->ymin = max(y0, 0);
            pie->ymax = min(y1, dev->height);
        }
    }

    /*
     * Make sure the CTM, color space, and clipping region (and, for
     * masked images or images with CombineWithColor, the current color)
     * are known at the time of the begin_image command.
     */
    cmd_clear_known(cdev, clist_image_unknowns(dev, pie) | begin_image_known);
    /* Because the rendering intent may be driven by the source color
       settings we may have needed to overide the intent.  Need to break the const
       on the pgs here for this and reset back */
    if (intent_changed)
        pgs_nonconst->renderingintent = renderingintent;
    if (bp_changed)
        pgs_nonconst->blackptcomp = blackptcomp;

    cdev->image_enum_id = pie->id;
    return 0;
    /*
     * We couldn't handle the image.  It is up to the caller to use the default
     * algorithms, which break the image up into rectangles or small pixmaps.
     * If we are doing the PDF14 transparency device then we want to make sure we do
     * NOT use the target device.  In this case we return -1.
     */
use_default:
    if (pie != NULL)
        gs_free_object(mem, pie->buffer, "clist_begin_typed_image");
    gs_free_object(mem, pie, "clist_begin_typed_image");
    *pinfo = NULL;

    if (pgs->has_transparency){
        return -1;
    } else {
        return gx_default_begin_typed_image(dev, pgs, pmat, pic, prect,
                                            pdcolor, pcpath, mem, pinfo);
    }
}

/* Error cleanup for clist_image_plane_data. */
static inline int
clist_image_plane_data_retry_cleanup(gx_device *dev, clist_image_enum *pie, int yh_used, int code)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;

    ++cdev->ignore_lo_mem_warnings;
    {
        code = write_image_end_all(dev, pie);
    }
    --cdev->ignore_lo_mem_warnings;
    /* Update sub-rect */
    if (!pie->image.Interpolate)
        pie->rect.p.y += yh_used;  /* interpolate & mem recovery currently incompat */
    return code;
}

/* Process the next piece of an image. */
static int
clist_image_plane_data(gx_image_enum_common_t * info,
                       const gx_image_plane_t * planes, int yh,
                       int *rows_used)
{
    gx_device *dev = info->dev;
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    clist_image_enum *pie = (clist_image_enum *) info;
    gs_rect sbox, dbox;
    int y_orig = pie->y;
    int yh_used = min(yh, pie->rect.q.y - y_orig);
    int y0, y1;
    int ry, rheight;
    int code;
    cmd_rects_enum_t re;
    bool found_color = false;

#ifdef DEBUG
    if (pie->id != cdev->image_enum_id) {
        lprintf2("end_image id = %lu != clist image id = %lu!\n",
                 (ulong) pie->id, (ulong) cdev->image_enum_id);
        *rows_used = 0;
        return_error(gs_error_Fatal);
    }
#endif
    /****** CAN'T HANDLE VARYING data_x VALUES YET ******/
    {
        int i;

        for (i = 1; i < info->num_planes; ++i)
            if (planes[i].data_x != planes[0].data_x) {
                *rows_used = 0;
                return_error(gs_error_rangecheck);
            }
    }
    sbox.p.x = pie->rect.p.x - pie->support.x;
    sbox.p.y = (y0 = y_orig) - pie->support.y;
    sbox.q.x = pie->rect.q.x + pie->support.x;
    sbox.q.y = (y1 = pie->y += yh_used) + pie->support.y;
    code = gs_bbox_transform(&sbox, &pie->matrix, &dbox);
    if (code < 0)
        return code;
    /*
     * In order to keep the band list consistent, we must write out
     * the image data in precisely those bands whose begin_image
     * Y range includes the respective image scan lines.  Because of
     * rounding, we must expand the dbox by a little extra, and then
     * use image_band_box to calculate the precise range for each band.
     * This is slow, but we don't see any faster way to do it in the
     * general case.
     */
    {
        int ry0 = (int)floor(dbox.p.y) - 2;
        int ry1 = (int)ceil(dbox.q.y) + 2;
        int band_height0 = cdev->page_band_height;

        /*
         * Make sure we don't go into any bands beyond the Y range
         * determined at begin_image time.
         */
        if (ry0 < pie->ymin)
            ry0 = pie->ymin;
        if (ry1 > pie->ymax)
            ry1 = pie->ymax;
        /*
         * If the image extends off the page in the Y direction,
         * we may have ry0 > ry1.  Check for this here.
         */
        if (ry0 >= ry1)
            goto done;
        /* Expand the range out to band boundaries. */
        ry = ry0 / band_height0 * band_height0;
        rheight = min(ROUND_UP(ry1, band_height0), dev->height) - ry;
    }

    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    /* If needed, update the trans_bbox */
    if (cdev->pdf14_needed) {
        gs_int_rect bbox;

        bbox.p.x = (int)floor(dbox.p.x);
        bbox.q.x = (int)ceil(dbox.q.x);
        bbox.p.y = pie->ymin;
        bbox.q.y = pie->ymax;

        clist_update_trans_bbox(cdev, &bbox);
    }
    /* Make sure clip_path for the cdev is not stale -- update from image_enum */
    cdev->clip_path = NULL;
    cmd_check_clip_path(cdev, pie->pcpath);

    RECT_ENUM_INIT(re, ry, rheight);
    do {
        gs_int_rect ibox;
        gs_int_rect entire_box;

        RECT_STEP_INIT(re);
        /*
         * Just transmit the subset of the data that intersects this band.
         * Note that y and height always define a complete band.
         */

        if (!image_band_box(dev, pie, re.y, re.height, &ibox))
            continue;
        /*
         * The transmitted subrectangle has to be computed at the time
         * we write the begin_image command; this in turn controls how
         * much of each scan line we write out.
         */
        {
            int band_ymax = min(re.band_end, pie->ymax);
            int band_ymin = max(re.band_end - re.band_height, pie->ymin);

            if (!image_band_box(dev, pie, band_ymin,
                                band_ymax - band_ymin, &entire_box))
                continue;
        }

        re.pcls->color_usage.or |= pie->color_usage.or;
        re.pcls->color_usage.slow_rop |= pie->color_usage.slow_rop;

        /* Write out begin_image & its preamble for this band */
        if (!(re.pcls->known & begin_image_known)) {
            gs_logical_operation_t lop = pie->pgs->log_op;
            byte *dp;
            byte *bp = pie->begin_image_command +
                pie->begin_image_command_length;
            uint len;
            byte image_op = cmd_opv_begin_image;

            /* Make sure the gs_gstate is up to date. */
            code = (pie->color_map_is_known ? 0 :
                    cmd_put_color_mapping(cdev, pie->pgs));
            pie->color_map_is_known = true;
            if (code >= 0) {
                uint want_known = ctm_known | clip_path_known |
                            op_bm_tk_known | opacity_alpha_known |
                            shape_alpha_known | alpha_known |
                            (pie->color_space.id == gs_no_id ? 0 :
                                                     color_space_known);

                code = cmd_do_write_unknown(cdev, re.pcls, want_known);
            }
            if (code >= 0)
                code = cmd_do_enable_clip(cdev, re.pcls, pie->pcpath != NULL);
            if (code >= 0)
                code = cmd_update_lop(cdev, re.pcls, lop);
            if (code < 0)
                return code;
            if (pie->uses_color) {
                /* We want to write the color taking into account the entire image so */
                /* we set re.rect_nbands from pie->ymin and pie->ymax so that we will */
                /* make the decision to write 'all_bands' the same for the whole image */
                /* This is slightly more efficient, and is required for patterns with */
                /* transparency that push the group at the begin_image step.          */
                re.rect_nbands = ((pie->ymax + re.band_height - 1) / re.band_height) -
                                 ((pie->ymin) / re.band_height);
                code = cmd_put_drawing_color(cdev, re.pcls, &pie->dcolor,
                                             &re, devn_not_tile_fill);
                if (code < 0)
                    return code;
            }
            if (entire_box.p.x != 0 || entire_box.p.y != 0 ||
                entire_box.q.x != pie->image.Width ||
                entire_box.q.y != pie->image.Height
                ) {
                image_op = cmd_opv_begin_image_rect;
                cmd_put2w(entire_box.p.x, entire_box.p.y, &bp);
                cmd_put2w(pie->image.Width - entire_box.q.x,
                          pie->image.Height - entire_box.q.y, &bp);
                }
            len = bp - pie->begin_image_command;
            code =
                set_cmd_put_op(&dp, cdev, re.pcls, image_op, 1 + len);
            if (code < 0)
                return code;
            memcpy(dp + 1, pie->begin_image_command, len);

            /* Mark band's begin_image as known */
            re.pcls->known |= begin_image_known;
        }

        /*
         * The data that we write out must use the X values set by
         * begin_image, which may cover a larger interval than the ones
         * actually needed for these particular scan lines if the image is
         * rotated.
         */
        {
            /*
             * image_band_box ensures that b{x,y}{0,1} fall within
             * pie->rect.
             */
            int bx0 = entire_box.p.x, bx1 = entire_box.q.x;
            int by0 = ibox.p.y, by1 = ibox.q.y;
            int bpp = pie->bits_per_plane;
            int num_planes = pie->num_planes;
            uint offsets[GS_IMAGE_MAX_COMPONENTS];
            int i, iy, ih, xskip, xoff, nrows;
            uint bytes_per_plane, bytes_per_row, rows_per_cmd;

            if (by0 < y0)
                by0 = y0;
            if (by1 > y1)
                by1 = y1;
            /*
             * Make sure we're skipping an integral number of pixels, by
             * truncating the initial X coordinate to the next lower
             * value that is an exact multiple of a byte.
             */
            xoff = bx0 - pie->rect.p.x;
            xskip = xoff & -(int)"\001\010\004\010\002\010\004\010"[bpp & 7];
            for (i = 0; i < num_planes; ++i)
                offsets[i] =
                    (by0 - y0) * planes[i].raster + ((xskip * bpp) >> 3);
            bytes_per_plane = ((bx1 - (pie->rect.p.x + xskip)) * bpp + 7) >> 3;
            bytes_per_row = bytes_per_plane * pie->num_planes;
            rows_per_cmd =
                (data_bits_size - cmd_largest_size) / max(bytes_per_row, 1);

            if (rows_per_cmd == 0) {
                /* The reader will have to buffer a row separately. */
                rows_per_cmd = 1;
            }
            if (pie->monitor_color) {
                for (iy = by0, ih = by1 - by0; ih > 0; iy += nrows, ih -= nrows) {
                    nrows = min(ih, rows_per_cmd);
                    if (!found_color) {
                        code = cmd_image_plane_data_mon(cdev, re.pcls, planes, info,
                                                    bytes_per_plane, offsets,
                                                    xoff - xskip, nrows,
                                                    &found_color);
                        if (found_color) {
                            /* Has color.  We are done monitoring */
                            cmm_dev_profile_t *dev_profile;
                            code = dev_proc(dev, get_profile)(dev,  &dev_profile);
                            dev_profile->pageneutralcolor = false;
                            code |= gsicc_mcm_end_monitor(pie->pgs->icc_link_cache, dev);
                            pie->monitor_color = false;
                        }
                    } else {
                        code = cmd_image_plane_data(cdev, re.pcls, planes, info,
                                                    bytes_per_plane, offsets,
                                                    xoff - xskip, nrows);
                    }
                    if (code < 0)
                        return code;
                    for (i = 0; i < num_planes; ++i)
                        offsets[i] += planes[i].raster * nrows;
                }
            } else {
                for (iy = by0, ih = by1 - by0; ih > 0; iy += nrows, ih -= nrows) {
                    nrows = min(ih, rows_per_cmd);
                    code = cmd_image_plane_data(cdev, re.pcls, planes, info,
                                                bytes_per_plane, offsets,
                                                xoff - xskip, nrows);
                    if (code < 0)
                        return code;
                    for (i = 0; i < num_planes; ++i)
                        offsets[i] += planes[i].raster * nrows;
                }
            }
        }
    } while ((re.y += re.height) < re.yend);
 done:
    *rows_used = pie->y - y_orig;
    return pie->y >= pie->rect.q.y;
}

/* Clean up by releasing the buffers. */
static int
clist_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    gx_device *dev = info->dev;
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    clist_image_enum *pie = (clist_image_enum *) info;
    int code;

#ifdef DEBUG
    if (pie->id != cdev->image_enum_id) {
        lprintf2("end_image id = %lu != clist image id = %lu!\n",
                 (ulong) pie->id, (ulong) cdev->image_enum_id);
        return_error(gs_error_Fatal);
    }
#endif
    code = write_image_end_all(dev, pie);
    cdev->image_enum_id = gs_no_id;
    gx_image_free_enum(&info);
    return code;
}

/* Create a compositor device. */
int
clist_create_compositor(gx_device * dev,
                        gx_device ** pcdev, const gs_composite_t * pcte,
                        gs_gstate * pgs, gs_memory_t * mem, gx_device *cldev)
{
    byte * dp;
    uint size = 0, size_dummy;
    gx_device_clist_writer * const cdev =
                    &((gx_device_clist *)dev)->writer;
    int ry, rheight, cropping_op;
    int band_height = cdev->page_info.band_params.BandHeight;
    int last_band = cdev->nbands - 1;
    int first_band = 0, no_of_bands = cdev->nbands;
    int code = pcte->type->procs.write(pcte, 0, &size, cdev);
    int temp_cropping_min, temp_cropping_max;

    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cdev);

    /* determine the amount of space required */
    if (code < 0 && code != gs_error_rangecheck)
        return code;
    size += 2 + 1;      /* 2 bytes for the command code, one for the id */

    /* Create a compositor device for clist writing (if needed) */
    code = pcte->type->procs.clist_compositor_write_update(pcte, dev,
                                                        pcdev, pgs, mem);
    if (code < 0)
        return code;

    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cdev);

    code = pcte->type->procs.get_cropping(pcte, &ry, &rheight, cdev->cropping_min, cdev->cropping_max);

    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cdev);

    if (code < 0)
        return code;

    cropping_op = code;
    code = 0;

    if (cropping_op == PUSHCROP || cropping_op == SAMEAS_PUSHCROP_BUTNOPUSH) {
        first_band = ry / band_height;
        last_band = (ry + rheight - 1) / band_height;
    } else if (cropping_op == POPCROP || cropping_op == CURRBANDS) {
        first_band = cdev->cropping_min / band_height;
        last_band = (cdev->cropping_max - 1) / band_height;
    }

    if (last_band - first_band > no_of_bands * 2 / 3) {
        /* Covering many bands, so write "all bands" command for shorter clist. */
        cropping_op = ALLBANDS;
    }

    /* Using 'v' here instead of 'L' since this is used almost exclusively with
       the transparency code */

#ifdef DEBUG
    if (gs_debug_c('v')) {

        if(cropping_op != 0) {

           dmprintf2(dev->memory, "[v] cropping_op = %d. Total number of bands is %d \n",
                     cropping_op, no_of_bands);
           dmprintf2(dev->memory, "[v]  Writing out from band %d through band %d \n",
                     first_band, last_band);

        } else {

           dmprintf1(dev->memory, "[v] cropping_op = %d. Writing out to all bands \n",
                     cropping_op);

        }
    }
#endif

    if (cropping_op == ALLBANDS) {
        /* overprint applies to all bands */
        size_dummy = size;
        code = set_cmd_put_all_op(& dp,
                                   (gx_device_clist_writer *)dev,
                                   cmd_opv_extend,
                                   size );
        if (code < 0)
            return code;

        /* insert the command and compositor identifier */
        dp[1] = cmd_opv_ext_create_compositor;
        dp[2] = pcte->type->comp_id;

        /* serialize the remainder of the compositor */
        if ((code = pcte->type->procs.write(pcte, dp + 3, &size_dummy, cdev)) < 0)
            ((gx_device_clist_writer *)dev)->cnext = dp;
        return code;
    }
    if (cropping_op == PUSHCROP) {
        code = clist_writer_push_cropping(cdev, ry, rheight);
        if (code < 0)
            return code;
    }
    if (cropping_op == SAMEAS_PUSHCROP_BUTNOPUSH) {
        /* Set the range even though it is not pushed until the group occurs
           This occurs only when we had blend changes with a group push */
        temp_cropping_min = max(cdev->cropping_min, ry);
        temp_cropping_max = min(cdev->cropping_max, ry + rheight);
    } else {
        temp_cropping_min = cdev->cropping_min;
        temp_cropping_max = cdev->cropping_max;
    }
    if (temp_cropping_min < temp_cropping_max) {
        /* The pdf14 compositor could be applied
           only to bands covered by the pcte->params.bbox. */
        cmd_rects_enum_t re;

        RECT_ENUM_INIT(re, temp_cropping_min, temp_cropping_max - temp_cropping_min);
        do {
            RECT_STEP_INIT(re);
            code = set_cmd_put_op(&dp, cdev, re.pcls, cmd_opv_extend, size);
            if (code >= 0) {
                size_dummy = size;
                dp[1] = cmd_opv_ext_create_compositor;
                dp[2] = pcte->type->comp_id;
                code = pcte->type->procs.write(pcte, dp + 3, &size_dummy, cdev);
            }
            if (code < 0)
                return code;
        } while ((re.y += re.height) < re.yend);
    }
    if (cropping_op == POPCROP) {
        code = clist_writer_pop_cropping(cdev);
        if (code < 0)
            return code;
    }

    return code;
}

/* ------ Utilities ------ */

/* Add a command to set data_x. */
static int
cmd_put_set_data_x(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                   int data_x)
{
    byte *dp;
    int code;

    if (data_x > 0x1f) {
        int dx_msb = data_x >> 5;

        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_misc,
                              2 + cmd_size_w(dx_msb));
        if (code >= 0) {
            dp[1] = cmd_set_misc_data_x + 0x20 + (data_x & 0x1f);
            cmd_put_w(dx_msb, dp + 2);
        }
    } else {
        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_misc, 2);
        if (code >= 0)
            dp[1] = cmd_set_misc_data_x + data_x;
    }
    return code;
}

/* Add commands to represent a full (device) halftone. */
int
cmd_put_halftone(gx_device_clist_writer * cldev, const gx_device_halftone * pdht)
{
    uint    ht_size = 0, req_size;
    byte *  dp;
    byte *  dp0 = 0;
    byte *  pht_buff = 0;
    int     code = gx_ht_write(pdht, (gx_device *)cldev, 0, &ht_size);

    /*
     * Determine the required size, and if necessary allocate a buffer.
     *
     * The full serialized representation consists of:
     *  command code (2 bytes)
     *  length of serialized halftone (enc_u_sizew(ht_size)
     *  one or more halfton segments, which consist of:
     *    command code (2 bytes)
     *    segment size (enc_u_sizew(seg_size) (seg_size < cbuf_ht_seg_max_size)
     *    the serialized halftone segment (seg_size)
     *
     * Serialized halftones may be larger than the command buffer, so it
     * is sent in segments. The cmd_opv_extend/cmd_opv_ext_put_halftone
     * combination indicates that a device halftone is being sent, and
     * provides the length of the entire halftone. This is followed by
     * one or more cmd_opv_extend/cmd_opv_ext_ht_seg commands, which
     * convey the segments of the serialized hafltone. The reader can
     * identify the final segment by adding segment lengths.
     *
     * This complexity is hidden from the serialization code. If the
     * halftone is larger than a single halftone buffer, we allocate a
     * buffer to hold the entire representation, and divided into
     * segments in this routine.
     */
    if (code < 0 && code != gs_error_rangecheck)
        return code;
    req_size = 2 + enc_u_sizew(ht_size);

    /* output the "put halftone" command */
    if ((code = set_cmd_put_all_op(&dp, cldev, cmd_opv_extend, req_size)) < 0)
        return code;
    dp[1] = cmd_opv_ext_put_halftone;
    dp += 2;
    enc_u_putw(ht_size, dp);

    /* see if a separate allocated buffer is required */
    if (ht_size > cbuf_ht_seg_max_size) {
        pht_buff = gs_alloc_bytes( cldev->bandlist_memory,
                                   ht_size,
                                   "cmd_put_halftone" );
        if (pht_buff == 0)
            return_error(gs_error_VMerror);
    } else {
        /* send the only segment command */
        req_size += ht_size;
        code = set_cmd_put_all_op(&dp, cldev, cmd_opv_extend, req_size);
        if (code < 0)
            return code;
        dp0 = dp;
        dp[1] = cmd_opv_ext_put_ht_seg;
        dp += 2;
        enc_u_putw(ht_size, dp);
        pht_buff = dp;
    }

    /* serialize the halftone */
    code = gx_ht_write(pdht, (gx_device *)cldev, pht_buff, &ht_size);
    if (code < 0) {
        if (ht_size > cbuf_ht_seg_max_size)
            gs_free_object( cldev->bandlist_memory,
                            pht_buff,
                            "cmd_put_halftone" );
        else
            cldev->cnext = dp0;
        return code;
    }

    /*
     * If the halftone fit into a single command buffer, we are done.
     * Otherwise, process the individual segments.
     *
     * If bandlist memory is exhausted while processing the segments,
     * we do not make any attempt to recover the partially submitted
     * halftone. The reader will discard any partially sent hafltone
     * when it receives the next cmd_opv_extend/
     * cmd_opv_ext_put_halftone combination.
     */
    if (ht_size > cbuf_ht_seg_max_size) {
        byte *  pbuff = pht_buff;

        while (ht_size > 0 && code >= 0) {
            int     seg_size, tmp_size;

            seg_size = ( ht_size > cbuf_ht_seg_max_size ? cbuf_ht_seg_max_size
                                                        : ht_size );
            tmp_size = 2 + enc_u_sizew(seg_size) + seg_size;
            code = set_cmd_put_all_op(&dp, cldev, cmd_opv_extend, tmp_size);
            if (code >= 0) {
                dp[1] = cmd_opv_ext_put_ht_seg;
                dp += 2;
                enc_u_putw(seg_size, dp);
                memcpy(dp, pbuff, seg_size);
                ht_size -= seg_size;
                pbuff += seg_size;
            }
        }
        gs_free_object( cldev->bandlist_memory, pht_buff, "cmd_put_halftone");
        pht_buff = 0;
    }

    if (code >= 0)
        cldev->device_halftone_id = pdht->id;

    return code;
}

/* Write out any necessary color mapping data. */
int
cmd_put_color_mapping(gx_device_clist_writer * cldev,
                      const gs_gstate * pgs)
{
    int code;
    const gx_device_halftone *pdht = pgs->dev_ht;

    /* Put out the halftone, if present. */
    if (pdht && pdht->id != cldev->device_halftone_id) {
        code = cmd_put_halftone(cldev, pdht);
        if (code < 0)
            return code;
        cldev->device_halftone_id = pdht->id;
    }
    /* Put the under color removal and black generation functions */
    code = cmd_put_color_map(cldev, cmd_map_black_generation,
                                 0, pgs->black_generation,
                                 &cldev->black_generation_id);
    if (code < 0)
        return code;
    code = cmd_put_color_map(cldev, cmd_map_undercolor_removal,
                                 0, pgs->undercolor_removal,
                                 &cldev->undercolor_removal_id);
    if (code < 0)
        return code;
    /* Now put out the transfer functions. */
    {
        uint which = 0;
        bool send_default_comp = false;
        int i;
        gs_id default_comp_id, xfer_ids[4];

        /*
         * Determine the ids for the transfer functions that we currently
         * have in the set_transfer structure.  The halftone xfer funcs
         * are sent in cmd_put_halftone.
         */
#define get_id(pgs, color, color_num) \
    ((pgs->set_transfer.color != NULL && pgs->set_transfer.color_num >= 0) \
        ? pgs->set_transfer.color->id\
        : pgs->set_transfer.gray->id)

        xfer_ids[0] = get_id(pgs, red, red_component_num);
        xfer_ids[1] = get_id(pgs, green, green_component_num);
        xfer_ids[2] = get_id(pgs, blue, blue_component_num);
        xfer_ids[3] = default_comp_id = pgs->set_transfer.gray->id;
#undef get_id

        for (i = 0; i < countof(cldev->transfer_ids); ++i) {
            if (xfer_ids[i] != cldev->transfer_ids[i])
                which |= 1 << i;
            if (xfer_ids[i] == default_comp_id &&
                cldev->transfer_ids[i] != default_comp_id)
                send_default_comp = true;
        }
        /* There are 3 cases for transfer functions: nothing to write, */
        /* a single function, and multiple functions. */
        if (which == 0)
            return 0;
        /*
         * Send default transfer function if changed or we need it for a
         * component
         */
        if (send_default_comp || cldev->transfer_ids[0] != default_comp_id) {
            gs_id dummy = gs_no_id;

            code = cmd_put_color_map(cldev, cmd_map_transfer, 0,
                pgs->set_transfer.gray, &dummy);
            if (code < 0)
                return code;
            /* Sending a default will force all xfers to default */
            for (i = 0; i < countof(cldev->transfer_ids); ++i)
                cldev->transfer_ids[i] = default_comp_id;
        }
        /* Send any transfer functions which have changed */
        if (cldev->transfer_ids[0] != xfer_ids[0]) {
            code = cmd_put_color_map(cldev, cmd_map_transfer_0,
                        pgs->set_transfer.red_component_num,
                        pgs->set_transfer.red, &cldev->transfer_ids[0]);
            if (code < 0)
                return code;
        }
        if (cldev->transfer_ids[1] != xfer_ids[1]) {
            code = cmd_put_color_map(cldev, cmd_map_transfer_1,
                        pgs->set_transfer.green_component_num,
                        pgs->set_transfer.green, &cldev->transfer_ids[1]);
            if (code < 0)
                return code;
        }
        if (cldev->transfer_ids[2] != xfer_ids[2]) {
            code = cmd_put_color_map(cldev, cmd_map_transfer_2,
                        pgs->set_transfer.blue_component_num,
                        pgs->set_transfer.blue, &cldev->transfer_ids[2]);
            if (code < 0)
                return code;
        }
    }

    return 0;
}

/*
 * Compute the subrectangle of an image that intersects a band;
 * return false if it is empty.
 * It is OK for this to be too large; in fact, with the present
 * algorithm, it will be quite a bit too large if the transformation isn't
 * well-behaved ("well-behaved" meaning either xy = yx = 0 or xx = yy = 0).
 */
#define I_FLOOR(x) ((int)floor(x))
#define I_CEIL(x) ((int)ceil(x))
static void
box_merge_point(gs_int_rect * pbox, double x, double y)
{
    int t;

    if ((t = I_FLOOR(x)) < pbox->p.x)
        pbox->p.x = t;
    if ((t = I_CEIL(x)) > pbox->q.x)
        pbox->q.x = t;
    if ((t = I_FLOOR(y)) < pbox->p.y)
        pbox->p.y = t;
    if ((t = I_CEIL(y)) > pbox->q.y)
        pbox->q.y = t;
}
static bool
image_band_box(gx_device * dev, const clist_image_enum * pie, int y, int h,
               gs_int_rect * pbox)
{
    fixed by0 = int2fixed(y);
    fixed by1 = int2fixed(y + h);
    int
        px = pie->rect.p.x, py = pie->rect.p.y,
        qx = pie->rect.q.x, qy = pie->rect.q.y;
    gs_fixed_rect cbox;         /* device clipping box */
    gs_rect bbox;               /* cbox intersected with band */

    /* Intersect the device clipping box and the band. */
    (*dev_proc(dev, get_clipping_box)) (dev, &cbox);
    /* The fixed_half here is to allow for adjustment. */
    bbox.p.x = fixed2float(cbox.p.x - fixed_half);
    bbox.q.x = fixed2float(cbox.q.x + fixed_half);
    bbox.p.y = fixed2float(max(cbox.p.y, by0) - fixed_half);
    bbox.q.y = fixed2float(min(cbox.q.y, by1) + fixed_half);
    /* Limit the box further if possible (because of a clipping path) */
    if (bbox.p.y < pie->ymin)
        bbox.p.y = pie->ymin;
    if (bbox.q.y > pie->ymax)
        bbox.q.y = pie->ymax;
#ifdef DEBUG
    if (gs_debug_c('b')) {
        dmlprintf6(dev->memory, "[b]band box for (%d,%d),(%d,%d), band (%d,%d) =>\n",
                   px, py, qx, qy, y, y + h);
        dmlprintf10(dev->memory, "      (%g,%g),(%g,%g), matrix=[%g %g %g %g %g %g]\n",
                    bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y,
                    pie->matrix.xx, pie->matrix.xy, pie->matrix.yx,
                    pie->matrix.yy, pie->matrix.tx, pie->matrix.ty);
    }
#endif
    if (is_xxyy(&pie->matrix) || is_xyyx(&pie->matrix)) {
        /*
         * The inverse transform of the band is a rectangle aligned with
         * the coordinate axes, so we can just intersect it with the
         * image subrectangle.
         */
        gs_rect ibox;           /* bbox transformed back to image space */

        if (gs_bbox_transform_inverse(&bbox, &pie->matrix, &ibox) < 0)
            return false;
        pbox->p.x = max(px, I_FLOOR(ibox.p.x));
        pbox->q.x = min(qx, I_CEIL(ibox.q.x));
        pbox->p.y = max(py, I_FLOOR(ibox.p.y));
        pbox->q.y = min(qy, I_CEIL(ibox.q.y));
    } else {
        /*
         * The inverse transform of the band is not aligned with the
         * axes, i.e., is a general parallelogram.  To compute an exact
         * bounding box, we need to find the intersections of this
         * parallelogram with the image subrectangle.
         *
         * There is probably a much more efficient way to do this
         * computation, but we don't know what it is.
         */
        gs_point rect[4];
        gs_point corners[5];
        int i;

        /* Store the corners of the image rectangle. */
        rect[0].x = rect[3].x = px;
        rect[1].x = rect[2].x = qx;
        rect[0].y = rect[1].y = py;
        rect[2].y = rect[3].y = qy;
        /*
         * Compute the corners of the clipped band in image space.  If
         * the matrix is singular or an overflow occurs, the result will
         * be nonsense: in this case, there isn't anything useful we
         * can do, so return an empty intersection.
         */
        if (gs_point_transform_inverse(bbox.p.x, bbox.p.y, &pie->matrix,
                                       &corners[0]) < 0 ||
            gs_point_transform_inverse(bbox.q.x, bbox.p.y, &pie->matrix,
                                       &corners[1]) < 0 ||
            gs_point_transform_inverse(bbox.q.x, bbox.q.y, &pie->matrix,
                                       &corners[2]) < 0 ||
            gs_point_transform_inverse(bbox.p.x, bbox.q.y, &pie->matrix,
                                       &corners[3]) < 0
            ) {
            if_debug0m('b', dev->memory,
                       "[b]can't inverse-transform a band corner!\n");
            return false;
        }
        corners[4] = corners[0];
        pbox->p.x = qx, pbox->p.y = qy;
        pbox->q.x = px, pbox->q.y = py;
        /*
         * We iterate over both the image rectangle and the band
         * parallelogram in a single loop for convenience, even though
         * there is no coupling between the two.
         */
        for (i = 0; i < 4; ++i) {
            gs_point pa, pt;
            double dx, dy;

            /* Check the image corner for being inside the band. */
            pa = rect[i];
            gs_point_transform(pa.x, pa.y, &pie->matrix, &pt);
            if (pt.x >= bbox.p.x && pt.x <= bbox.q.x &&
                pt.y >= bbox.p.y && pt.y <= bbox.q.y
                )
                box_merge_point(pbox, pa.x, pa.y);
            /* Check the band corner for being inside the image. */
            pa = corners[i];
            if (pa.x >= px && pa.x <= qx && pa.y >= py && pa.y <= qy)
                box_merge_point(pbox, pa.x, pa.y);
            /* Check for intersections of band edges with image edges. */
            dx = corners[i + 1].x - pa.x;
            dy = corners[i + 1].y - pa.y;
#define in_range(t, tc, p, q)\
  (0 <= t && t <= 1 && (t = tc) >= p && t <= q)
            if (dx != 0) {
                double t = (px - pa.x) / dx;

                if_debug3m('b', dev->memory, "   (px) t=%g => (%d,%g)\n",
                           t, px, pa.y + t * dy);
                if (in_range(t, pa.y + t * dy, py, qy))
                    box_merge_point(pbox, (double) px, t);
                t = (qx - pa.x) / dx;
                if_debug3m('b', dev->memory, "   (qx) t=%g => (%d,%g)\n",
                           t, qx, pa.y + t * dy);
                if (in_range(t, pa.y + t * dy, py, qy))
                    box_merge_point(pbox, (double) qx, t);
            }
            if (dy != 0) {
                double t = (py - pa.y) / dy;

                if_debug3m('b', dev->memory, "   (py) t=%g => (%g,%d)\n",
                           t, pa.x + t * dx, py);
                if (in_range(t, pa.x + t * dx, px, qx))
                    box_merge_point(pbox, t, (double) py);
                t = (qy - pa.y) / dy;
                if_debug3m('b', dev->memory, "   (qy) t=%g => (%g,%d)\n",
                           t, pa.x + t * dx, qy);
                if (in_range(t, pa.x + t * dx, px, qx))
                    box_merge_point(pbox, t, (double) qy);
            }
#undef in_range
        }
    }
    if_debug4m('b', dev->memory, "    => (%d,%d),(%d,%d)\n",
               pbox->p.x, pbox->p.y, pbox->q.x, pbox->q.y);
    /*
     * If necessary, add pixels around the edges so we will have
     * enough information to do interpolation.
     */
    if ((pbox->p.x -= pie->support.x) < pie->rect.p.x)
        pbox->p.x = pie->rect.p.x;
    if ((pbox->p.y -= pie->support.y) < pie->rect.p.y)
        pbox->p.y = pie->rect.p.y;
    if ((pbox->q.x += pie->support.x) > pie->rect.q.x)
        pbox->q.x = pie->rect.q.x;
    if ((pbox->q.y += pie->support.y) > pie->rect.q.y)
        pbox->q.y = pie->rect.q.y;
    return (pbox->p.x < pbox->q.x && pbox->p.y < pbox->q.y);
}

/* Determine which image-related properties are unknown */
static uint     /* mask of unknown properties(see pcls->known) */
clist_image_unknowns(gx_device *dev, const clist_image_enum *pie)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    const gs_gstate *const pgs = pie->pgs;
    uint unknown = 0;

    /*
     * Determine if the CTM, color space, and clipping region (and, for
     * masked images or images with CombineWithColor, the current color)
     * are unknown. Set the device state in anticipation of the values
     * becoming known.
     */
    if (cdev->gs_gstate.ctm.xx != pgs->ctm.xx ||
        cdev->gs_gstate.ctm.xy != pgs->ctm.xy ||
        cdev->gs_gstate.ctm.yx != pgs->ctm.yx ||
        cdev->gs_gstate.ctm.yy != pgs->ctm.yy ||
        cdev->gs_gstate.ctm.tx != pgs->ctm.tx ||
        cdev->gs_gstate.ctm.ty != pgs->ctm.ty
        ) {
        unknown |= ctm_known;
        cdev->gs_gstate.ctm = pgs->ctm;
    }
    if (pie->color_space.id == gs_no_id) { /* masked image */
        cdev->color_space.space = 0; /* for GC */
    } else {                    /* not masked */
        if (cdev->color_space.id == pie->color_space.id) {
            /* The color space pointer might not be valid: update it. */
            cdev->color_space.space = pie->color_space.space;
        } else {
            unknown |= color_space_known;
            cdev->color_space = pie->color_space;
        }
    }
    if (cmd_check_clip_path(cdev, pie->pcpath))
        unknown |= clip_path_known;
    /*
     * Note: overprint and overprint_mode are implemented via a compositor
     * device, which is passed separately through the command list. Hence,
     * though both parameters are passed in the state as well, this usually
     * has no effect.
     */
    if (cdev->gs_gstate.overprint != pgs->overprint ||
        cdev->gs_gstate.overprint_mode != pgs->overprint_mode ||
        cdev->gs_gstate.blend_mode != pgs->blend_mode ||
        cdev->gs_gstate.text_knockout != pgs->text_knockout ||
        cdev->gs_gstate.renderingintent != pgs->renderingintent) {
        unknown |= op_bm_tk_known;
        cdev->gs_gstate.overprint = pgs->overprint;
        cdev->gs_gstate.overprint_mode = pgs->overprint_mode;
        cdev->gs_gstate.blend_mode = pgs->blend_mode;
        cdev->gs_gstate.text_knockout = pgs->text_knockout;
        cdev->gs_gstate.renderingintent = pgs->renderingintent;
    }
    if (cdev->gs_gstate.opacity.alpha != pgs->opacity.alpha) {
        unknown |= opacity_alpha_known;
        cdev->gs_gstate.opacity.alpha = pgs->opacity.alpha;
    }
    if (cdev->gs_gstate.shape.alpha != pgs->shape.alpha) {
        unknown |= shape_alpha_known;
        cdev->gs_gstate.shape.alpha = pgs->shape.alpha;
    }
    if (cdev->gs_gstate.alpha != pgs->alpha) {
        unknown |= alpha_known;
        cdev->gs_gstate.alpha = pgs->alpha;
    }
    return unknown;
}

/* Construct the begin_image command. */
static int
begin_image_command(byte *buf, uint buf_size, const gs_image_common_t *pic)
{
    int i;
    stream s;
    const gs_color_space *ignore_pcs;
    int code;

    for (i = 0; i < gx_image_type_table_count; ++i)
        if (gx_image_type_table[i] == pic->type)
            break;
    if (i >= gx_image_type_table_count)
        return_error(gs_error_rangecheck);
    s_init(&s, NULL);
    swrite_string(&s, buf, buf_size);
    sputc(&s, (byte)i);
    code = pic->type->sput(pic, &s, &ignore_pcs);
    return (code < 0 ? code : stell(&s));
}

/* Write data for a partial image. */
static int
cmd_image_plane_data(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                     const gx_image_plane_t * planes,
                     const gx_image_enum_common_t * pie,
                     uint bytes_per_plane, const uint * offsets,
                     int dx, int h)
{
    int data_x = planes[0].data_x + dx;
    uint nbytes = bytes_per_plane * pie->num_planes * h;
    uint len = 1 + cmd_size2w(h, bytes_per_plane) + nbytes;
    byte *dp;
    uint offset = 0;
    int plane, i;
    int code;

    if (data_x) {
        code = cmd_put_set_data_x(cldev, pcls, data_x);
        if (code < 0)
            return code;
        offset = ((data_x & ~7) * cldev->clist_color_info.depth) >> 3;
    }
    code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_image_data, len);
    if (code < 0)
        return code;
    dp++;
    cmd_put2w(h, bytes_per_plane, &dp);
    for (plane = 0; plane < pie->num_planes; ++plane)
        for (i = 0; i < h; ++i) {
            memcpy(dp,
                   planes[plane].data + i * planes[plane].raster +
                   offsets[plane] + offset,
                   bytes_per_plane);
            dp += bytes_per_plane;
        }
    return 0;
}

/* Write data for a partial image with color monitor. */
static int
cmd_image_plane_data_mon(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                     const gx_image_plane_t * planes,
                     const gx_image_enum_common_t * pie,
                     uint bytes_per_plane, const uint * offsets,
                     int dx, int h, bool *found_color)
{
    clist_image_enum *pie_c = (clist_image_enum *) pie;
    int data_x = planes[0].data_x + dx;
    uint nbytes = bytes_per_plane * pie->num_planes * h;
    uint len = 1 + cmd_size2w(h, bytes_per_plane) + nbytes;
    byte *dp;
    uint offset = 0;
    int plane, i;
    int code;
    int width = pie_c->rect.q.x - pie_c->rect.p.x;
    int dsize = (((width + (planes[0]).data_x) * pie_c->decode.spp *
        pie_c->decode.bps / pie->num_planes + 7) >> 3);
    int data_size = pie_c->decode.spread / pie->num_planes;

    *found_color = false;

    if (data_x) {
        code = cmd_put_set_data_x(cldev, pcls, data_x);
        if (code < 0)
            return code;
        offset = ((data_x & ~7) * cldev->clist_color_info.depth) >> 3;
    }
    code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_image_data, len);
    if (code < 0)
        return code;
    dp++;

    cmd_put2w(h, bytes_per_plane, &dp);

    for (i = 0; i < h; ++i) {
        if (!(*found_color)) {
            /* Here we need to unpack and actually look at the image data
               to see if we have any non-neutral colors */
            int pdata_x;
            byte *data_ptr =  (byte *)(planes[0].data + i * planes[0].raster + offsets[0] + offset);
            byte *buffer = (byte *)(*pie_c->decode.unpack)(pie_c->buffer, &pdata_x, 
                                     data_ptr, 0, dsize, pie_c->decode.map,
                pie_c->decode.spread, pie_c->decode.spp);

            for (plane = 1; plane < pie->num_planes; ++plane) {
                /* unpack planes after the first (if any), relying on spread to place the */
                /* data at the correct spacing, with the buffer start adjusted for each plane */
                data_ptr = (byte *)(planes[plane].data + i * planes[plane].raster + offsets[plane] + offset);
                (*pie_c->decode.unpack)(pie_c->buffer + (data_size * plane), &pdata_x, data_ptr, 0,
                    dsize, pie_c->decode.map, pie_c->decode.spread, pie_c->decode.spp);
            }
            if (row_has_color(buffer, pie_c, data_size, width)) {
                /* Has color.  We are done monitoring */
                *found_color = true;
            }
        }
        /* Now copy the plane data into the clist buffer */
        for (plane = 0; plane < pie->num_planes; ++plane) {
            memcpy(dp, planes[plane].data + i * planes[plane].raster +
                   offsets[plane] + offset, bytes_per_plane);
            dp += bytes_per_plane;
        }
    }
    return 0;
}

/* Write image_end commands into all bands */
static int      /* ret 0 ok, else -ve error status */
write_image_end_all(gx_device *dev, const clist_image_enum *pie)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    int code;
    int ry = pie->ymin;
    int rheight = pie->ymax - ry;
    cmd_rects_enum_t re;

    /*
     * We need to check specially for images lying entirely outside the
     * page, since the RECT writing logic doesn't do this.
     */
    if (pie->ymax < 0 || ry >= dev->height)
        return 0;
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    RECT_ENUM_INIT(re, ry, rheight);
    do {
        byte *dp;

        RECT_STEP_INIT(re);
        if (re.pcls->known & begin_image_known) {
            if_debug1m('L', dev->memory, "[L]image_end for band %d\n", re.band);
            code = set_cmd_put_op(&dp, cdev, re.pcls, cmd_opv_image_data, 2);
            if (code < 0)
                return code;
            dp[1] = 0;      /* EOD */
            re.pcls->known ^= begin_image_known;
        }
    } while ((re.y += re.height) < re.yend);
    /* Make sure to clean up the buffer if we were monitoring */
    if (pie->buffer != NULL) {
        gs_free_object(pie->memory, pie->buffer, "write_image_end_all");
    }
    return 0;
}

/*
 * Compare a rectangle vs. clip path.  Return true if there is no clipping
 * path, if the rectangle is unclipped, or if the clipping path is a
 * rectangle and intersects the given rectangle.
 */
static bool
check_rect_for_trivial_clip(
    const gx_clip_path *pcpath, /* May be NULL, clip to evaluate */
    int px, int py, int qx, int qy      /* corners of box to test */
)
{
    gs_fixed_rect obox;
    gs_fixed_rect imgbox;

    if (!pcpath)
        return true;

    imgbox.p.x = int2fixed(px);
    imgbox.p.y = int2fixed(py);
    imgbox.q.x = int2fixed(qx);
    imgbox.q.y = int2fixed(qy);
    if (gx_cpath_includes_rectangle(pcpath,
                                    imgbox.p.x, imgbox.p.y,
                                    imgbox.q.x, imgbox.q.y))
        return true;

    return (gx_cpath_outer_box(pcpath, &obox) /* cpath is rectangle */ &&
            obox.p.x <= imgbox.q.x && obox.q.x >= imgbox.p.x &&
            obox.p.y <= imgbox.q.y && obox.q.y >= imgbox.p.y );
}
