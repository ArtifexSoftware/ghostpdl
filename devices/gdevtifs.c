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


/* TIFF-writing substructure */

#include "stdint_.h"   /* for tiff.h */
#include "stdio_.h"
#include "time_.h"
#include "gdevtifs.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gdevprn.h"
#include "minftrsz.h"
#include "gxdownscale.h"
#include "scommon.h"
#include "stream.h"
#include "strmio.h"
#include "gsicc_cache.h"
#include "gscms.h"
#include "gstiffio.h"
#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently First/Last Page and obejct filter */

int
tiff_open(gx_device *pdev)
{
    gx_device_printer *ppdev = (gx_device_printer *)pdev;
    int code;
    bool update_procs = false;

    /* Use our own warning and error message handlers in libtiff */
    tiff_set_handlers();

    code = install_internal_subclass_devices((gx_device **)&pdev, &update_procs);
    if (code < 0)
        return code;
    /* If we've been subclassed, find the terminal device */
    while(pdev->child)
        pdev = pdev->child;
    ppdev = (gx_device_printer *)pdev;

    ppdev->file = NULL;
    code = gdev_prn_allocate_memory(pdev, NULL, 0, 0);
    if (code < 0)
        return code;
    if (update_procs) {
        if (pdev->ObjectHandlerPushed) {
            gx_copy_device_procs(pdev->parent, pdev, &gs_obj_filter_device);
            pdev = pdev->parent;
        }
        if (pdev->PageHandlerPushed)
            gx_copy_device_procs(pdev->parent, pdev, &gs_flp_device);
    }
    if (ppdev->OpenOutputFile)
        code = gdev_prn_open_printer_seekable(pdev, 1, true);
    return code;
}

int
tiff_close(gx_device * pdev)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;

    if (tfdev->tif)
        TIFFCleanup(tfdev->tif);

    if (tfdev->icclink != NULL)
    {
        tfdev->icclink->procs.free_link(tfdev->icclink);
        gsicc_free_link_dev(pdev->memory, tfdev->icclink);
        tfdev->icclink = NULL;
    }
    return gdev_prn_close(pdev);
}

static int
tiff_get_some_params(gx_device * dev, gs_param_list * plist, int which)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int code = gdev_prn_get_params(dev, plist);
    int ecode = code;
    gs_param_string comprstr;

    if ((code = param_write_bool(plist, "BigEndian", &tfdev->BigEndian)) < 0)
        ecode = code;
#if (TIFFLIB_VERSION >= 20111221)
    if ((code = param_write_bool(plist, "UseBigTIFF", &tfdev->UseBigTIFF)) < 0)
        ecode = code;
#endif
    if ((code = param_write_bool(plist, "TIFFDateTime", &tfdev->write_datetime)) < 0)
        ecode = code;
    if ((code = tiff_compression_param_string(&comprstr, tfdev->Compression)) < 0 ||
        (code = param_write_string(plist, "Compression", &comprstr)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "MaxStripSize", &tfdev->MaxStripSize)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "AdjustWidth", &tfdev->AdjustWidth)) < 0)
        ecode = code;
    if (which & 1) {
        if ((code = gx_downscaler_write_params(plist, &tfdev->downscale,
                                               GX_DOWNSCALER_PARAMS_MFS |
                                               (which & 2 ? GX_DOWNSCALER_PARAMS_TRAP : 0) |
                                               (which & 4 ? GX_DOWNSCALER_PARAMS_ETS : 0))) < 0)
            ecode = code;
    }
    return ecode;
}

int
tiff_get_params(gx_device * dev, gs_param_list * plist)
{
    return tiff_get_some_params(dev, plist, 0);
}

int
tiff_get_params_downscale(gx_device * dev, gs_param_list * plist)
{
    return tiff_get_some_params(dev, plist, 1);
}

int
tiff_get_params_downscale_cmyk(gx_device * dev, gs_param_list * plist)
{
    return tiff_get_some_params(dev, plist, 3);
}

int
tiff_get_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist)
{
    return tiff_get_some_params(dev, plist, 7);
}

static int
tiff_put_some_params(gx_device * dev, gs_param_list * plist, int which)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int ecode = 0;
    int code;
    const char *param_name;
    bool big_endian = tfdev->BigEndian;
    bool usebigtiff = tfdev->UseBigTIFF;
    bool write_datetime = tfdev->write_datetime;
    uint16 compr = tfdev->Compression;
    gs_param_string comprstr;
    long mss = tfdev->MaxStripSize;
    long aw = tfdev->AdjustWidth;

    /* Read BigEndian option as bool */
    switch (code = param_read_bool(plist, (param_name = "BigEndian"), &big_endian)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

    /* Read UseBigTIFF option as bool */
    switch (code = param_read_bool(plist, (param_name = "UseBigTIFF"), &usebigtiff)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

#if !(TIFFLIB_VERSION >= 20111221)
    if (usebigtiff)
        dmlprintf(dev->memory, "Warning: this version of libtiff does not support BigTIFF, ignoring parameter\n");
    usebigtiff = false;
#endif

    switch (code = param_read_bool(plist, (param_name = "TIFFDateTime"), &write_datetime)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

    /* Read Compression */
    switch (code = param_read_string(plist, (param_name = "Compression"), &comprstr)) {
        case 0:
            if ((ecode = tiff_compression_id(&compr, &comprstr)) < 0) {

                errprintf(tfdev->memory, "Unknown compression setting\n");
                param_signal_error(plist, param_name, ecode);
                return ecode;
            }

            if ( !tiff_compression_allowed(compr, (which & 1 ? 1 : (dev->color_info.depth / dev->color_info.num_components)))) {

                errprintf(tfdev->memory, "Invalid compression setting for this bitdepth\n");
                param_signal_error(plist, param_name, gs_error_rangecheck);
                return_error(gs_error_rangecheck);
            }
            break;
        case 1:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
    }
    if (which & 1)
    {
        code = gx_downscaler_read_params(plist, &tfdev->downscale,
                                         (GX_DOWNSCALER_PARAMS_MFS |
                                          (which & 2 ? GX_DOWNSCALER_PARAMS_TRAP : 0) |
                                          (which & 4 ? GX_DOWNSCALER_PARAMS_ETS : 0)));
        if (code < 0)
        {
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        }
    }
    switch (code = param_read_long(plist, (param_name = "MaxStripSize"), &mss)) {
        case 0:
            /*
             * Strip must be large enough to accommodate a raster line.
             * If the max strip size is too small, we still write a single
             * line per strip rather than giving an error.
             */
            if (mss >= 0)
                break;
            code = gs_error_rangecheck;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }
    switch (code = param_read_long(plist, (param_name = "AdjustWidth"), &aw)) {
        case 0:
            if (aw >= 0)
                break;
            code = gs_error_rangecheck;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }

    if (ecode < 0)
        return ecode;
    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        return code;

    tfdev->BigEndian = big_endian;
    tfdev->UseBigTIFF = usebigtiff;
    tfdev->write_datetime = write_datetime;
    tfdev->Compression = compr;
    tfdev->MaxStripSize = mss;
    tfdev->AdjustWidth = aw;
    return code;
}

int
tiff_put_params(gx_device * dev, gs_param_list * plist)
{
    return tiff_put_some_params(dev, plist, 0);
}

int
tiff_put_params_downscale(gx_device * dev, gs_param_list * plist)
{
    return tiff_put_some_params(dev, plist, 1);
}

int
tiff_put_params_downscale_cmyk(gx_device * dev, gs_param_list * plist)
{
    return tiff_put_some_params(dev, plist, 3);
}

int
tiff_put_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist)
{
    return tiff_put_some_params(dev, plist, 7);
}

int gdev_tiff_begin_page(gx_device_tiff *tfdev,
                         gp_file *file)
{
    gx_device_printer *const pdev = (gx_device_printer *)tfdev;
    cmm_dev_profile_t *profile_struct;
    gsicc_rendering_param_t rendering_params;
    int code;

    if (gdev_prn_file_is_new(pdev)) {
        /* open the TIFF device */
        tfdev->tif = tiff_from_filep(pdev, pdev->dname, file, tfdev->BigEndian, tfdev->UseBigTIFF);
        if (!tfdev->tif)
            return_error(gs_error_invalidfileaccess);
        /* Set up the icc link settings at this time */
        code = dev_proc(pdev, get_profile)((gx_device *)pdev, &profile_struct);
        if (code < 0)
            return_error(gs_error_undefined);
        if (profile_struct->postren_profile != NULL) {
            rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
            rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;
            rendering_params.override_icc = false;
            rendering_params.preserve_black = gsBLACKPRESERVE_OFF;
            rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;
            rendering_params.cmm = gsCMM_DEFAULT;
            if (profile_struct->oi_profile != NULL) {
                tfdev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->oi_profile, profile_struct->postren_profile,
                    &rendering_params);
            } else if (profile_struct->link_profile != NULL) {
                tfdev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->link_profile, profile_struct->postren_profile,
                    &rendering_params);
            } else {
                tfdev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->device_profile[0], profile_struct->postren_profile,
                    &rendering_params);
            }
            if (tfdev->icclink == NULL) {
                return_error(gs_error_VMerror);
            }
            /* If it is identity, release it now and set link to NULL */
            if (tfdev->icclink->is_identity) {
                tfdev->icclink->procs.free_link(tfdev->icclink);
                gsicc_free_link_dev(pdev->memory, tfdev->icclink);
                tfdev->icclink = NULL;
            }
        }
    }

    return tiff_set_fields_for_printer(pdev, tfdev->tif, tfdev->downscale.downscale_factor,
                                       tfdev->AdjustWidth,
                                       tfdev->write_datetime);
}

int tiff_set_compression(gx_device_printer *pdev,
                         TIFF *tif,
                         uint compression,
                         long max_strip_size)
{
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    if (max_strip_size == 0) {
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, pdev->height);
    }
    else {
        int rows = max_strip_size /
            gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        TIFFSetField(tif,
                     TIFFTAG_ROWSPERSTRIP,
                     TIFFDefaultStripSize(tif, max(1, rows)));
    }

    return 0;
}

int tiff_set_fields_for_printer(gx_device_printer *pdev,
                                TIFF              *tif,
                                int                factor,
                                int                adjustWidth,
                                bool               writedatetime)
{
    int width = gx_downscaler_scale(pdev->width, factor);
    int height = gx_downscaler_scale(pdev->height, factor);
    int xpi = gx_downscaler_scale((int)pdev->x_pixels_per_inch, factor);
    int ypi = gx_downscaler_scale((int)pdev->y_pixels_per_inch, factor);
    width = fax_adjusted_width(width, adjustWidth);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)xpi);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)ypi);

    {
        char revs[10];
#define maxSoftware 40
        char softwareValue[maxSoftware];

        strncpy(softwareValue, gs_product, maxSoftware);
        softwareValue[maxSoftware - 1] = 0;
        gs_sprintf(revs, " %1.2f", gs_revision / 100.0);
        strncat(softwareValue, revs,
                maxSoftware - strlen(softwareValue) - 1);

        TIFFSetField(tif, TIFFTAG_SOFTWARE, softwareValue);
    }
    if (writedatetime) {
        struct tm tms;
        time_t t;
        char dateTimeValue[20];

#ifdef CLUSTER
        memset(&t, 0, sizeof(t));
        memset(&tms, 0, sizeof(tms));
#else
        time(&t);
        tms = *localtime(&t);
#endif
        gs_sprintf(dateTimeValue, "%04d:%02d:%02d %02d:%02d:%02d",
                tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                tms.tm_hour, tms.tm_min, tms.tm_sec);

        TIFFSetField(tif, TIFFTAG_DATETIME, dateTimeValue);
    }

    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
    TIFFSetField(tif, TIFFTAG_PAGENUMBER, pdev->PageCount, 0);

    /* Set the ICC profile.  Test to avoid issues with separations and also
       if the color space is set to LAB, we do that as an enumerated type. Do
       NOT set the profile if the bit depth is less than 8 or if fast color
       was used. */
    if (pdev->color_info.depth >= 8) {
        /* Select from one of three profiles.. */
        cmm_profile_t *icc_profile;

        if (pdev->icc_struct->postren_profile != NULL)
            icc_profile = pdev->icc_struct->postren_profile;
        else if (pdev->icc_struct->oi_profile != NULL)
            icc_profile = pdev->icc_struct->oi_profile;
        else
            icc_profile = pdev->icc_struct->device_profile[0];

        if (icc_profile->num_comps == pdev->color_info.num_components &&
            icc_profile->data_cs != gsCIELAB && !(pdev->icc_struct->usefastcolor)) {
            TIFFSetField(tif, TIFFTAG_ICCPROFILE, icc_profile->buffer_size,
                icc_profile->buffer);
        }
    }
    return 0;
}

int
tiff_print_page(gx_device_printer *dev, TIFF *tif, int min_feature_size)
{
    int code = 0;
    byte *data;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = max(size, TIFFScanlineSize(tif));
    int row;
    int bpc = dev->color_info.depth / dev->color_info.num_components;
    void *min_feature_data = NULL;
    int line_lag = 0;
    int filtered_count;

    data = gs_alloc_bytes(dev->memory, max_size, "tiff_print_page(data)");
    if (data == NULL)
        return_error(gs_error_VMerror);
    if (bpc != 1)
        min_feature_size = 1;
    if (min_feature_size > 1) {
        code = min_feature_size_init(dev->memory, min_feature_size,
                                     dev->width, dev->height,
                                     &min_feature_data);
        if (code < 0)
            goto cleanup;
    }

    code = TIFFCheckpointDirectory(tif);

    memset(data, 0, max_size);
    for (row = 0; row < dev->height && code >= 0; row++) {
        code = gdev_prn_copy_scan_lines(dev, row, data, size);
        if (code < 0)
            goto cleanup;
        if (min_feature_size > 1) {
            filtered_count = min_feature_size_process(data, min_feature_data);
            if (filtered_count == 0)
                line_lag++;
        }

        if (row - line_lag >= 0) {
#if defined(ARCH_IS_BIG_ENDIAN) && (!ARCH_IS_BIG_ENDIAN)
            if (bpc == 16)
                TIFFSwabArrayOfShort((uint16 *)data,
                                     dev->width * (long)dev->color_info.num_components);
#endif

            code = TIFFWriteScanline(tif, data, row - line_lag, 0);
        }
    }
    for (row -= line_lag ; row < dev->height && code >= 0; row++)
    {
        filtered_count = min_feature_size_process(data, min_feature_data);
        code = TIFFWriteScanline(tif, data, row, 0);
    }

    if (code >= 0)
        code = TIFFWriteDirectory(tif);
cleanup:
    if (min_feature_size > 1)
        min_feature_size_dnit(min_feature_data);
    gs_free_object(dev->memory, data, "tiff_print_page(data)");

    return code;
}

/* void gsicc_init_buffer(gsicc_bufferdesc_t *buffer_desc, unsigned char num_chan,
    unsigned char bytes_per_chan, bool has_alpha, bool alpha_first,
    bool is_planar, int plane_stride, int row_stride, int num_rows,
    int pixels_per_row); */

static int tiff_chunky_post_cm(void  *arg, byte **dst, byte **src, int w, int h,
    int raster)
{
    gsicc_bufferdesc_t input_buffer_desc, output_buffer_desc;
    gsicc_link_t *icclink = (gsicc_link_t*)arg;

    gsicc_init_buffer(&input_buffer_desc, icclink->num_input, 1, false,
        false, false, 0, raster, h, w);
    gsicc_init_buffer(&output_buffer_desc, icclink->num_output, 1, false,
        false, false, 0, raster, h, w);
    icclink->procs.map_buffer(NULL, icclink, &input_buffer_desc, &output_buffer_desc,
        src[0], dst[0]);
    return 0;
}

/* Special version, called with 8 bit grey input to be downsampled to 1bpp
 * output. */
int
tiff_downscale_and_print_page(gx_device_printer *dev, TIFF *tif, int factor,
                              int mfs, int aw, int bpc, int num_comps,
                              int trap_w, int trap_h, const int *trap_order,
                              int ets)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int code = 0;
    byte *data = NULL;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = max(size, TIFFScanlineSize(tif));
    int row;
    int height = dev->height/factor;
    gx_downscaler_t ds;

    code = TIFFCheckpointDirectory(tif);
    if (code < 0)
        return code;

    if (num_comps == 4) {
        if (tfdev->icclink == NULL) {
            code = gx_downscaler_init_trapped_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, &fax_adjusted_width, aw, trap_w, trap_h, trap_order, ets);
        } else {
            code = gx_downscaler_init_trapped_cm_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, &fax_adjusted_width, aw, trap_w, trap_h, trap_order,
                tiff_chunky_post_cm, tfdev->icclink, tfdev->icclink->num_output, ets);
        }
    } else {
        if (tfdev->icclink == NULL) {
            code = gx_downscaler_init_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, &fax_adjusted_width, aw, ets);
        } else {
            code = gx_downscaler_init_cm_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, &fax_adjusted_width, aw, tiff_chunky_post_cm, tfdev->icclink,
                tfdev->icclink->num_output, ets);
        }
    }
    if (code < 0)
        return code;

    data = gs_alloc_bytes(dev->memory, max_size, "tiff_print_page(data)");
    if (data == NULL) {
        gx_downscaler_fin(&ds);
        return_error(gs_error_VMerror);
    }

    for (row = 0; row < height && code >= 0; row++) {
        code = gx_downscaler_getbits(&ds, data, row);
        if (code < 0)
            break;

        code = TIFFWriteScanline(tif, data, row, 0);
        if (code < 0)
            break;
    }

    if (code >= 0)
        code = TIFFWriteDirectory(tif);

    gx_downscaler_fin(&ds);
    gs_free_object(dev->memory, data, "tiff_print_page(data)");

    return code;
}


static struct compression_string {
    uint16 id;
    const char *str;
} compression_strings [] = {
    { COMPRESSION_NONE, "none" },
    { COMPRESSION_CCITTRLE, "crle" },
    { COMPRESSION_CCITTFAX3, "g3" },
    { COMPRESSION_CCITTFAX4, "g4" },
    { COMPRESSION_LZW, "lzw" },
    { COMPRESSION_PACKBITS, "pack" },

    { 0, NULL }
};

int
tiff_compression_param_string(gs_param_string *param, uint16 id)
{
    struct compression_string *c;
    for (c = compression_strings; c->str; c++)
        if (id == c->id) {
            param_string_from_string(*param, c->str);
            return 0;
        }
    return_error(gs_error_undefined);
}

int
tiff_compression_id(uint16 *id, gs_param_string *param)
{
    struct compression_string *c;
    for (c = compression_strings; c->str; c++)
        if (!bytes_compare(param->data, param->size,
                           (const byte *)c->str, strlen(c->str)))
        {
            *id = c->id;
            return 0;
        }
    return_error(gs_error_undefined);
}

int tiff_compression_allowed(uint16 compression, byte depth)
{
    return ((depth == 1 && (compression == COMPRESSION_NONE ||
                          compression == COMPRESSION_CCITTRLE ||
                          compression == COMPRESSION_CCITTFAX3 ||
                          compression == COMPRESSION_CCITTFAX4 ||
                          compression == COMPRESSION_LZW ||
                          compression == COMPRESSION_PACKBITS))
           || ((depth == 8 || depth == 16) && (compression == COMPRESSION_NONE ||
                          compression == COMPRESSION_LZW ||
                          compression == COMPRESSION_PACKBITS)));

}
