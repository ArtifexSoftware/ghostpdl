/* Copyright (C) 2006-2008 Artifex Software, Inc.
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

static int
xps_isolate_alpha_channel_8(xps_context_t *ctx, xps_image_t *image)
{
    int n = image->comps;
    int y, x, k;
    byte *sp, *dp, *ap;

    if ((image->colorspace != XPS_GRAY_A) &&
	    (image->colorspace != XPS_RGB_A) &&
	    (image->colorspace != XPS_CMYK_A))
	return 0;

    image->alpha = xps_alloc(ctx, image->width * image->height);
    if (!image->alpha)
	return gs_throw(-1, "cannot allocate image alpha plane");

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

    image->comps --;
    image->stride = image->width * image->comps;

    return 0;
}

static int
xps_isolate_alpha_channel_16(xps_context_t *ctx, xps_image_t *image)
{
    int n = image->comps;
    int y, x, k;
    unsigned short *sp, *dp, *ap;

    if ((image->colorspace != XPS_GRAY_A) &&
	    (image->colorspace != XPS_RGB_A) &&
	    (image->colorspace != XPS_CMYK_A))
	return 0;

    image->alpha = xps_alloc(ctx, image->width * image->height * 2);
    if (!image->alpha)
	return gs_throw(-1, "cannot allocate image alpha plane");

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

    image->comps --;
    image->stride = image->width * image->comps * 2;

    return 0;
}

static int
xps_decode_image(xps_context_t *ctx, xps_part_t *part, xps_image_t *image)
{
    byte *buf = (byte*)part->data;
    int len = part->size;
    int error;

    if (len < 2)
	error = gs_throw(-1, "unknown image file format");

    memset(image, 0, sizeof(xps_image_t));
    image->samples = NULL;
    image->alpha = NULL;

    if (buf[0] == 0xff && buf[1] == 0xd8)
	error = xps_decode_jpeg(ctx->memory, buf, len, image);
    else if (memcmp(buf, "\211PNG\r\n\032\n", 8) == 0)
	error = xps_decode_png(ctx->memory, buf, len, image);
    else if (memcmp(buf, "II", 2) == 0 && buf[2] == 0xBC)
	error = xps_decode_hdphoto(ctx->memory, buf, len, image);
    else if (memcmp(buf, "MM", 2) == 0)
	error = xps_decode_tiff(ctx->memory, buf, len, image);
    else if (memcmp(buf, "II", 2) == 0)
	error = xps_decode_tiff(ctx->memory, buf, len, image);
    else
	error = gs_throw(-1, "unknown image file format");

    if (error)
	return gs_rethrow(error, "could not decode image");

    if (image->colorspace == XPS_GRAY_A ||
	image->colorspace == XPS_RGB_A ||
	image->colorspace == XPS_CMYK_A)
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

    if (ctx->opacity_only)
    {
	if (image->alpha)
	{
	    xps_paint_image_brush_imp(ctx, image, 1);
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
	xps_paint_image_brush_imp(ctx, image, 1);
	gs_end_transparency_mask(ctx->pgs, TRANSPARENCY_CHANNEL_Opacity);

	gs_trans_group_params_init(&tgp);
	gs_begin_transparency_group(ctx->pgs, &tgp, &bbox);
	xps_paint_image_brush_imp(ctx, image, 0);
	gs_end_transparency_group(ctx->pgs);
    }
    else
    {
	xps_paint_image_brush_imp(ctx, image, 0);
    }
    return 0;
}

static int
xps_find_image_brush_source_part(xps_context_t *ctx, char *base_uri, xps_item_t *root, xps_part_t **partp)
{
    xps_part_t *part;
    char *image_source_att;
    char buf[1024];
    char partname[1024];
    char *image_name;
    char *profile_name;
    char *p;

    image_source_att = xps_att(root, "ImageSource");
    if (!image_source_att)
	return gs_throw(-1, "missing ImageSource attribute");

    /* "{ColorConvertedBitmap /Resources/Image.tiff /Resources/Profile.icc}" */
    if (strstr(image_source_att, "{ColorConvertedBitmap") == image_source_att)
    {
	image_name = NULL;
	profile_name = NULL;

	strcpy(buf, image_source_att);
	p = strchr(buf, ' ');
	if (p)
	{
	    image_name = p + 1;
	    p = strchr(p + 1, ' ');
	    if (p)
	    {
		*p = 0;
		profile_name = p + 1;
		p = strchr(p + 1, '}');
		if (p)
		    *p = 0;
	    }
	}
    }
    else
    {
	image_name = image_source_att;
	profile_name = NULL;
    }

    if (!image_name)
	return gs_throw1(-1, "cannot parse image resource name '%s'", image_source_att);

    if (profile_name)
	dprintf2("warning: ignoring color profile '%s' associated with image '%s'\n",
		profile_name, image_name);

    xps_absolute_path(partname, base_uri, image_name);
    part = xps_find_part(ctx, partname);
    if (!part)
	return gs_throw1(-1, "cannot find image resource part '%s'", partname);

    *partp = part;
    return 0;
}

int
xps_parse_image_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *root)
{
    xps_part_t *part;
    int code;

    code = xps_find_image_brush_source_part(ctx, base_uri, root, &part);
    if (code < 0)
	return gs_rethrow(code, "cannot find image source");

    if (!part->image)
    {
	part->image = xps_alloc(ctx, sizeof(xps_image_t));
	if (!part->image)
	    return gs_throw(-1, "out of memory: image struct");

	code = xps_decode_image(ctx, part, part->image);
	if (code < 0)
	    return gs_rethrow(-1, "cannot decode image resource");
    }

    xps_parse_tiling_brush(ctx, base_uri, dict, root, xps_paint_image_brush, part->image);

    /* TODO: free the image data here if the image is only used once on the page */

    return 0;
}

int
xps_image_brush_has_transparency(xps_context_t *ctx, char *base_uri, xps_item_t *root)
{
    xps_part_t *part;
    int code;

    code = xps_find_image_brush_source_part(ctx, base_uri, root, &part);
    if (code < 0)
	return gs_rethrow(code, "cannot find image source");

    /* Hmm, we should be smarter here and only look at the image header */

    if (!part->image)
    {
	part->image = xps_alloc(ctx, sizeof(xps_image_t));
	if (!part->image)
	    return gs_throw(-1, "out of memory: image struct");

	code = xps_decode_image(ctx, part, part->image);
	if (code < 0)
	    return gs_rethrow(-1, "cannot decode image resource");
    }

    return part->image->alpha != NULL;
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

