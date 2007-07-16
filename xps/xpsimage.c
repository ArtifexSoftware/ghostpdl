#include "ghostxps.h"

static void 
xps_remove_alpha_channel(xps_image_t *image)
{
    int cs = image->colorspace;
    int n = image->comps;
    int y, x, k;
    byte *sp, *dp;

    if (image->bits != 8)
    {
	gs_warn1("cannot strip alpha from %dbpc images", image->bits);
	return;
    }

    if ((cs != XPS_GRAY_A) && (cs != XPS_RGB_A) && (cs != XPS_CMYK_A))
	return;

    for (y = 0; y < image->height; y++)
    {
	sp = image->samples + image->width * n * y;
	dp = image->samples + image->width * (n - 1) * y;
	for (x = 0; x < image->width; x++)
	{
	    for (k = 0; k < n - 1; k++)
		*dp++ = *sp++;
	    sp++;
	}
    }

    image->colorspace --;
    image->comps --;
    image->stride = (n - 1) * image->width;
}


static int
xps_decode_image(xps_context_t *ctx, xps_part_t *part, xps_image_t *image)
{
    byte *buf = (byte*)part->data;
    int len = part->size;
    int error;

    if (len < 2)
	error = gs_throw(-1, "unknown image file format");

    if (buf[0] == 0xff && buf[1] == 0xd8)
	error = xps_decode_jpeg(ctx->memory, buf, len, image);
    else if (memcmp(buf, "\211PNG\r\n\032\n", 8) == 0)
	error = xps_decode_png(ctx->memory, buf, len, image);
    else if (memcmp(buf, "MM", 2) == 0)
	error = xps_decode_tiff(ctx->memory, buf, len, image);
    else if (memcmp(buf, "II", 2) == 0)
	error = xps_decode_tiff(ctx->memory, buf, len, image);
    else
	error = gs_throw(-1, "unknown image file format");

    if (error)
	return gs_rethrow(error, "could not decode image");

    /* NB: strip out alpha until we can handle it */
    xps_remove_alpha_channel(image);

    return gs_okay;
}

static int
xps_paint_image_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root, void *vimage)
{
    xps_image_t *image = vimage;
    gs_image_enum *penum;
    gs_color_space *colorspace;
    gs_image_t gsimage;
    int code;

    unsigned int count = image->stride * image->height;
    unsigned int used = 0;

    switch (image->colorspace)
    {
    case XPS_GRAY:
	colorspace = gs_cspace_new_DeviceGray(ctx->memory);
	break;
    case XPS_RGB:
	colorspace = gs_cspace_new_DeviceRGB(ctx->memory);
	break;
    case XPS_CMYK:
	colorspace = gs_cspace_new_DeviceCMYK(ctx->memory);
	break;
    default:
	return gs_throw(-1, "cannot handle images with alpha channels");
    }
    if (!colorspace)
	return gs_throw(-1, "cannot create colorspace for image");

    gs_image_t_init(&gsimage, colorspace);
    gsimage.ColorSpace = colorspace;
    gsimage.BitsPerComponent = image->bits;
    gsimage.Width = image->width;
    gsimage.Height = image->height;

    gsimage.ImageMatrix.xx = image->xres / 96.0;
    gsimage.ImageMatrix.yy = image->yres / 96.0;

    penum = gs_image_enum_alloc(ctx->memory, "xps_parse_image_brush (gs_image_enum_alloc)");
    if (!penum)
	return gs_throw(-1, "gs_enum_allocate failed");

    if ((code = gs_image_init(penum, &gsimage, false, ctx->pgs)) < 0)
	return gs_throw(code, "gs_image_init failed");

    if ((code = gs_image_next(penum, image->samples, count, &used)) < 0)
	return gs_throw(code, "gs_image_next failed");

    if (count < used)
	return gs_throw2(-1, "not enough image data (image=%d used=%d)", count, used);

    if (count > used)
	return gs_throw2(0, "too much image data (image=%d used=%d)", count, used);

    gs_image_cleanup_and_free_enum(penum, ctx->pgs);

    // TODO: free colorspace

#if 0
    float x0 = 0;
    float y0 = 0;
    float x1 = image->width * 96.0 / image->xres;
    float y1 = image->height * 96.0 / image->yres;
    gs_setlinewidth(ctx->pgs, 2.0);

    gs_setgray(ctx->pgs, 0.5);
    gs_moveto(ctx->pgs, x0, y0);
    gs_lineto(ctx->pgs, x0, y1);
    gs_lineto(ctx->pgs, x1, y1);
    gs_lineto(ctx->pgs, x1, y0);
    gs_closepath(ctx->pgs);
    gs_stroke(ctx->pgs);

    gs_setgray(ctx->pgs, 0.75);
    gs_moveto(ctx->pgs, x0, y0);
    gs_lineto(ctx->pgs, x1, y1);
    gs_stroke(ctx->pgs);

    gs_setgray(ctx->pgs, 0.25);
    gs_moveto(ctx->pgs, x1, y0);
    gs_lineto(ctx->pgs, x0, y1);
    gs_stroke(ctx->pgs);
#endif

    return 0;
}

int
xps_parse_image_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;
    xps_part_t *part;
    xps_image_t image;
    char *image_source_att;
    char partname[1024];
    int code;

    image_source_att = xps_att(root, "ImageSource");

    /*
     * Decode image resource.
     */

    dprintf1("drawing image brush '%s'\n", image_source_att);

    xps_absolute_path(partname, ctx->pwd, image_source_att);
    part = xps_find_part(ctx, partname);
    if (!part)
	return gs_throw1(-1, "cannot find image resource part '%s'", partname);

    code = xps_decode_image(ctx, part, &image);
    if (code < 0)
	return gs_rethrow(-1, "cannot draw image brush");

    xps_parse_tiling_brush(ctx, dict, root, xps_paint_image_brush, &image);

    xps_free(ctx, image.samples);

    return 0;
}

