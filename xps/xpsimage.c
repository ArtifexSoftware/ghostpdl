/* Copyright (C) 2006-2010 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* XPS interpreter - image support */

/* TODO: we should be smarter here and do incremental decoding
 * and rendering instead of uncompressing the whole image to
 * memory before drawing.
 */

#include "ghostxps.h"

/*
 * Un-interleave the alpha channel.
 */

static void
xps_isolate_alpha_channel_8(xps_context_t *ctx, xps_image_t *image)
{
    int n = image->comps;
    int y, x, k;
    byte *sp, *dp, *ap;

    if ((image->colorspace != XPS_GRAY_A) &&
        (image->colorspace != XPS_RGB_A) &&
        (image->colorspace != XPS_CMYK_A) &&
        (image->colorspace != XPS_ICC_A))
        return;

    image->alpha = xps_alloc(ctx, image->width * image->height);

    for (y = 0; y < image->height; y++)
    {
        sp = image->samples + image->width * n * y;
        dp = image->samples + image->width * (n - 1) * y;
        ap = image->alpha + image->width * y;
        for (x = 0; x < image->width; x++)
        {
            for (k = 0; k < n - 1; k++)
                *dp++ = *sp++;
            *ap++ = *sp++;
        }
    }

    if (image->colorspace == XPS_GRAY_A)
        image->colorspace = XPS_GRAY;
    if (image->colorspace == XPS_RGB_A)
        image->colorspace = XPS_RGB;
    if (image->colorspace == XPS_CMYK_A)
        image->colorspace = XPS_CMYK;
    if (image->colorspace == XPS_ICC_A)
        image->colorspace = XPS_ICC;

    image->comps --;
    image->stride = image->width * image->comps;
}

static void
xps_isolate_alpha_channel_16(xps_context_t *ctx, xps_image_t *image)
{
    int n = image->comps;
    int y, x, k;
    unsigned short *sp, *dp, *ap;

    if ((image->colorspace != XPS_GRAY_A) &&
        (image->colorspace != XPS_RGB_A) &&
        (image->colorspace != XPS_CMYK_A) &&
        (image->colorspace != XPS_ICC_A))
        return;

    image->alpha = xps_alloc(ctx, image->width * image->height * 2);

    for (y = 0; y < image->height; y++)
    {
        sp = ((unsigned short*)image->samples) + (image->width * n * y);
        dp = ((unsigned short*)image->samples) + (image->width * (n - 1) * y);
        ap = ((unsigned short*)image->alpha) + (image->width * y);
        for (x = 0; x < image->width; x++)
        {
            for (k = 0; k < n - 1; k++)
                *dp++ = *sp++;
            *ap++ = *sp++;
        }
    }

    if (image->colorspace == XPS_GRAY_A)
        image->colorspace = XPS_GRAY;
    if (image->colorspace == XPS_RGB_A)
        image->colorspace = XPS_RGB;
    if (image->colorspace == XPS_CMYK_A)
        image->colorspace = XPS_CMYK;
    if (image->colorspace == XPS_ICC_A)
        image->colorspace = XPS_ICC;

    image->comps --;
    image->stride = image->width * image->comps * 2;
}

static int
xps_image_has_alpha(xps_context_t *ctx, xps_part_t *part)
{
    byte *buf = part->data;
    int len = part->size;

    if (len < 8)
    {
        gs_warn("unknown image file format");
        return 0;
    }

    if (buf[0] == 0xff && buf[1] == 0xd8)
        return 0; /* JPEG never has an alpha channel */
    else if (memcmp(buf, "\211PNG\r\n\032\n", 8) == 0)
        return xps_png_has_alpha(ctx->memory, buf, len);
    else if (memcmp(buf, "II", 2) == 0 && buf[2] == 0xBC)
        return xps_hdphoto_has_alpha(ctx->memory, buf, len);
    else if (memcmp(buf, "MM", 2) == 0)
        return xps_tiff_has_alpha(ctx->memory, buf, len);
    else if (memcmp(buf, "II", 2) == 0)
        return xps_tiff_has_alpha(ctx->memory, buf, len);

    return 0;
}

static int
xps_decode_image(xps_context_t *ctx, char *profilename, int profile_comp,
    xps_part_t *imagepart, xps_image_t *image)
{
    byte *buf = (byte*)imagepart->data;
    int len = imagepart->size;
    int error;
    int iccinfo = image->colorspace; /* So that we know if external profile was valid */
    unsigned char *embedded_profile;
    int profile_size;
    cmm_profile_t *iccprofile;
    bool has_alpha;

    if (len < 2)
        error = gs_throw(-1, "unknown image file format");

    memset(image, 0, sizeof(xps_image_t));
    image->samples = NULL;
    image->alpha = NULL;

    if (buf[0] == 0xff && buf[1] == 0xd8)
    {
        error = xps_decode_jpeg(ctx->memory, buf, len, image,
                                &embedded_profile, &profile_size);
        if (error)
            return gs_rethrow(error, "could not decode image");
    }
    else if (memcmp(buf, "\211PNG\r\n\032\n", 8) == 0)
    {
        error = xps_decode_png(ctx->memory, buf, len, image,
            &embedded_profile, &profile_size);
        if (error)
            return gs_rethrow(error, "could not decode image");
    }
    else if (memcmp(buf, "II", 2) == 0 && buf[2] == 0xBC)
    {
        error = xps_decode_hdphoto(ctx->memory, buf, len, image,
            &embedded_profile, &profile_size);
        if (error)
            return gs_rethrow(error, "could not decode image");
    }
    else if (memcmp(buf, "MM", 2) == 0 || memcmp(buf, "II", 2) == 0)
    {
        error = xps_decode_tiff(ctx->memory, buf, len, image,
                                &embedded_profile, &profile_size);
        if (error)
            return gs_rethrow(error, "could not decode image");
    }
    else
        return gs_throw(-1, "unknown image file format");

    has_alpha = (image->colorspace == XPS_GRAY_A ||
                image->colorspace == XPS_RGB_A ||
                image->colorspace == XPS_CMYK_A);

    /* Make sure profile matches data type */
    if (iccinfo == XPS_ICC)
    {
        if (profile_comp == image->comps)
        {
            /* Profile is OK for this image. Change color space type. */
            if (has_alpha)
            {
                image->colorspace = XPS_ICC_A;
            }
            else
            {
                image->colorspace = XPS_ICC;
            }
        }
    }

    /* See if we need to use the embedded profile.
     * Only used if we have one and a valid external profile was not specified */
    image->embeddedprofile = false;
    if (image->colorspace != XPS_ICC_A && image->colorspace != XPS_ICC && embedded_profile != NULL)
    {
        /* See if we can set up to use the embedded profile. Note
        these profiles are NOT added to the xps color cache.
        As such, they must be destroyed when the image brush ends.
        Hence we need to make a note that we are using an embedded
        profile.
        */

        /* Create the profile */
        iccprofile = gsicc_profile_new(NULL, ctx->memory, NULL, 0);
        /* Set buffer */
        iccprofile->buffer = embedded_profile;
        iccprofile->buffer_size = profile_size;
        /* Parse */
        gsicc_init_profile_info(iccprofile);

        /* Free up the buffer */
        gs_free_object(ctx->memory, embedded_profile, "Embedded Profile");
        iccprofile->buffer = NULL;
        iccprofile->buffer_size = 0;

        if (iccprofile->profile_handle == NULL)
        {
            /* Problem with profile. Just ignore it */
            gsicc_profile_reference(iccprofile, -1);
        }
        else
        {
            /* Check the profile is OK for channel data count.
             * Need to be careful here since alpha is put into comps */
            if ((image->comps - has_alpha) == gsicc_getsrc_channel_count(iccprofile))
            {
                /* Use the embedded profile */
                image->embeddedprofile = true;
                ctx->icc->cmm_icc_profile_data = iccprofile;
                if (has_alpha)
                {
                    image->colorspace = XPS_ICC_A;
                }
                else
                {
                    image->colorspace = XPS_ICC;
                }
            }
            else
            {
                /* Problem with profile. Just ignore it */
                gsicc_profile_reference(iccprofile, -1);
            }

        }
    }

    /* XPS_ICC_A potentially not set when has_alpha evaluated */
    if (has_alpha || image->colorspace == XPS_ICC_A )
    {
        if (image->bits < 8)
            dprintf1("cannot isolate alpha channel in %d bpc images\n", image->bits);
        if (image->bits == 8)
            xps_isolate_alpha_channel_8(ctx, image);
        if (image->bits == 16)
            xps_isolate_alpha_channel_16(ctx, image);
    }

    return gs_okay;
}

static int
xps_paint_image_brush_imp(xps_context_t *ctx, xps_image_t *image, int alpha)
{
    gs_image_enum *penum;
    gs_color_space *colorspace;
    gs_image_t gsimage;
    int code;

    unsigned int count;
    unsigned int used;
    byte *samples;

    if (alpha)
    {
        colorspace = ctx->gray;
        samples = image->alpha;
        count = (image->width * image->bits + 7) / 8 * image->height;
        used = 0;
    }
    else
    {
        switch (image->colorspace)
        {
        case XPS_GRAY: colorspace = ctx->gray; break;
        case XPS_RGB: colorspace = ctx->srgb; break;
        case XPS_CMYK: colorspace = ctx->cmyk; break;
        case XPS_ICC: colorspace = ctx->icc; break;
        default:
            return gs_throw(-1, "cannot draw images with interleaved alpha");
        }
        samples = image->samples;
        count = image->stride * image->height;
        used = 0;
    }

    memset(&gsimage, 0, sizeof(gsimage));
    gs_image_t_init(&gsimage, colorspace);
    gsimage.ColorSpace = colorspace;
    gsimage.BitsPerComponent = image->bits;
    gsimage.Width = image->width;
    gsimage.Height = image->height;

    gsimage.ImageMatrix.xx = image->xres / 96.0;
    gsimage.ImageMatrix.yy = image->yres / 96.0;

    gsimage.Interpolate = 1;

    penum = gs_image_enum_alloc(ctx->memory, "xps_parse_image_brush (gs_image_enum_alloc)");
    if (!penum)
        return gs_throw(-1, "gs_enum_allocate failed");

    if ((code = gs_image_init(penum, &gsimage, false, ctx->pgs)) < 0)
        return gs_throw(code, "gs_image_init failed");

    if ((code = gs_image_next(penum, samples, count, &used)) < 0)
        return gs_throw(code, "gs_image_next failed");

    if (count < used)
        return gs_throw2(-1, "not enough image data (image=%d used=%d)", count, used);

    if (count > used)
        return gs_throw2(0, "too much image data (image=%d used=%d)", count, used);

    gs_image_cleanup_and_free_enum(penum, ctx->pgs);

    return 0;
}

static int
xps_paint_image_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *root, void *vimage)
{
    xps_image_t *image = vimage;
    int code;

    if (ctx->opacity_only)
    {
        if (image->alpha)
        {
            code = xps_paint_image_brush_imp(ctx, image, 1);
            if (code < 0)
                return gs_rethrow(code, "cannot draw alpha channel image");
        }
        return 0;
    }

    if (image->alpha)
    {
        gs_transparency_mask_params_t params;
        gs_transparency_group_params_t tgp;
        gs_rect bbox;

        xps_bounds_in_user_space(ctx, &bbox);

        gs_trans_mask_params_init(&params, TRANSPARENCY_MASK_Luminosity);
        gs_begin_transparency_mask(ctx->pgs, &params, &bbox, 0);
        code = xps_paint_image_brush_imp(ctx, image, 1);
        if (code < 0)
            return gs_rethrow(code, "cannot draw alpha channel image");
        gs_end_transparency_mask(ctx->pgs, TRANSPARENCY_CHANNEL_Opacity);

        gs_trans_group_params_init(&tgp);
        gs_begin_transparency_group(ctx->pgs, &tgp, &bbox);
        code = xps_paint_image_brush_imp(ctx, image, 0);
        if (code < 0)
            return gs_rethrow(code, "cannot draw color channel image");
        gs_end_transparency_group(ctx->pgs);
    }
    else
    {
        code = xps_paint_image_brush_imp(ctx, image, 0);
        if (code < 0)
            return gs_rethrow(code, "cannot draw image");
    }
    return 0;
}

static int
xps_find_image_brush_source_part(xps_context_t *ctx, char *base_uri, xps_item_t *root,
    xps_part_t **imagepartp, char **profile_name)
{
    xps_part_t *imagepart;
    char *image_source_att;
    char buf[1024];
    char partname[1024];
    char *image_name;
    char *p;

    image_source_att = xps_att(root, "ImageSource");
    if (!image_source_att)
        return gs_throw(-1, "missing ImageSource attribute");

    /* "{ColorConvertedBitmap /Resources/Image.tiff /Resources/Profile.icc}" */
    if (strstr(image_source_att, "{ColorConvertedBitmap") == image_source_att)
    {
        image_name = NULL;
        *profile_name = NULL;

        xps_strlcpy(buf, image_source_att, sizeof buf);
        p = strchr(buf, ' ');
        if (p)
        {
            image_name = p + 1;
            p = strchr(p + 1, ' ');
            if (p)
            {
                *p = 0;
                *profile_name = p + 1;
                p = strchr(p + 1, '}');
                if (p)
                    *p = 0;
            }
        }
    }
    else
    {
        image_name = image_source_att;
        *profile_name = NULL;
    }

    if (!image_name)
        return gs_throw1(-1, "cannot parse image resource name '%s'", image_source_att);

    xps_absolute_path(partname, base_uri, image_name, sizeof partname);
    imagepart = xps_read_part(ctx, partname);
    if (!imagepart)
        return gs_throw1(-1, "cannot find image resource part '%s'", partname);

    *imagepartp = imagepart;

    return 0;
}

int
xps_parse_image_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *root)
{
    xps_part_t *imagepart;
    xps_image_t *image;
    int code;
    char *profilename;
    gs_color_space *colorspace;
    int profile_comp = 0;

    code = xps_find_image_brush_source_part(ctx, base_uri, root, &imagepart, &profilename);
    if (code < 0)
        return gs_rethrow(code, "cannot find image source");

    image = xps_alloc(ctx, sizeof(xps_image_t));
    if (!image)
        return gs_throw(-1, "out of memory: image struct");

    /*
    If we have an ICC profile, go ahead and set the color space now.
    Make sure the profile is valid etc. If we find one in the image
    then we will use that if one is not externally defined. If it is
    we get back our ICC color space object. However, we also need
    to see if the profile count matches the number of channels in the image.
    If it does not, then we do not use the profile. Also we may need
    to use the embedded profile.
    */

    image->colorspace = XPS_NOTICC;
    if (profilename != NULL)
    {
        colorspace = NULL;
        xps_set_icc(ctx, base_uri, profilename, &colorspace);
        if (colorspace != NULL)
        {
            /* Now, see what the channel count is */
            profile_comp =
                gsicc_getsrc_channel_count(colorspace->cmm_icc_profile_data);
            image->colorspace = XPS_ICC;
        }
    }

    code = xps_decode_image(ctx, profilename, profile_comp, imagepart, image);
    if (code < 0)
        return gs_rethrow(-1, "cannot decode image resource");

    code = xps_parse_tiling_brush(ctx, base_uri, dict, root, xps_paint_image_brush, image);
    if (code < 0)
        return gs_rethrow(-1, "cannot parse tiling brush");

    /* If we used an embedded profile. then release as these are not cached */
    if (image->embeddedprofile)
    {
        gsicc_profile_reference(ctx->icc->cmm_icc_profile_data, -1);
    }

    xps_free_image(ctx, image);

    xps_free_part(ctx, imagepart);

    return 0;
}

int
xps_image_brush_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_part_t *imagepart;
    int code;
    int has_alpha;
    char *profilename;

    code = xps_find_image_brush_source_part(ctx, base_uri, root, &imagepart, &profilename);
    if (code < 0)
    {
        gs_catch(code, "cannot find image source");
        return 0;
    }

    has_alpha = xps_image_has_alpha(ctx, imagepart);

    xps_free_part(ctx, imagepart);

    return has_alpha;
}

void
xps_free_image(xps_context_t *ctx, xps_image_t *image)
{
    if (image->samples)
        xps_free(ctx, image->samples);
    if (image->alpha)
        xps_free(ctx, image->alpha);
    xps_free(ctx, image);
}
