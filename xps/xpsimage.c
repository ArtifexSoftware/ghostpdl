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
    byte *buf = part->data;
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

int
xps_parse_image_brush(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;
    xps_part_t *part;
    xps_image_t image;
    int code;

    char *opacity;
    char *transform;
    char *viewbox;
    char *viewport;
    char *tile_mode;
    char *viewbox_units;
    char *viewport_units;
    char *image_source;

    xps_item_t *transform_node = NULL;

    char partname[1024];

    opacity = xps_att(root, "Opacity");
    transform = xps_att(root, "Transform");
    viewbox = xps_att(root, "Viewbox");
    viewport = xps_att(root, "Viewport");
    tile_mode = xps_att(root, "TileMode");
    viewbox_units = xps_att(root, "ViewboxUnits");
    viewport_units = xps_att(root, "ViewportUnits");
    image_source = xps_att(root, "ImageSource");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "ImageBrush.Transform"))
	    transform_node = node;
    }

    /*
     * Decode image resource.
     */

    dprintf1("drawing image brush '%s'\n", image_source);

    xps_absolute_path(partname, ctx->pwd, image_source);
    part = xps_find_part(ctx, partname);
    if (!part)
	return gs_throw1(-1, "cannot find image resource part '%s'", partname);

    code = xps_decode_image(ctx, part, &image);
    if (code < 0)
	return gs_rethrow(-1, "cannot draw image brush");

    gs_gsave(ctx->pgs);
    {
	gs_image_enum *penum;
	gs_color_space colorspace;
	gs_image_t gsimage;
	unsigned int count = image.stride * image.height;
	unsigned int used = 0;
	gs_matrix matrix;
	gs_rect gsviewbox;
	gs_rect gsviewport;
	float scalex, scaley;

	/*
	 * Figure out transformation.
	 */

	gs_make_identity(&matrix);
	if (transform)
	    xps_parse_render_transform(ctx, transform, &matrix);
	if (transform_node)
	    xps_parse_matrix_transform(ctx, transform_node, &matrix);
	gs_concat(ctx->pgs, &matrix);

	gsviewbox.p.x = 0.0; gsviewbox.p.y = 0.0;
	gsviewbox.q.x = 1.0; gsviewbox.q.y = 1.0;
	if (viewbox)
	    xps_parse_rectangle(ctx, viewbox, &gsviewbox);

	gsviewport.p.x = 0.0; gsviewport.p.y = 0.0;
	gsviewport.q.x = 1.0; gsviewport.q.y = 1.0;
	if (viewport)
	    xps_parse_rectangle(ctx, viewport, &gsviewport);

	scalex = (gsviewport.q.x - gsviewport.p.x) / (gsviewbox.q.x - gsviewbox.p.x);
	scaley = (gsviewport.q.y - gsviewport.p.y) / (gsviewbox.q.y - gsviewbox.p.y);
	gs_translate(ctx->pgs, gsviewport.p.x, gsviewport.p.y);
	gs_scale(ctx->pgs, scalex, scaley);
	gs_translate(ctx->pgs, -gsviewbox.p.x, gsviewbox.p.y);

	/*
	 * Set up colorspace and image structs.
	 */

	switch (image.colorspace)
	{
	case XPS_GRAY:
	    gs_cspace_init_DeviceGray(ctx->memory, &colorspace);
	    break;
	case XPS_RGB:
	    gs_cspace_init_DeviceRGB(ctx->memory, &colorspace);
	    break;
	case XPS_CMYK:
	    gs_cspace_init_DeviceCMYK(ctx->memory, &colorspace);
	    break;
	default:
	    return gs_throw(-1, "cannot handle images with alpha channels");
	}

	gs_image_t_init(&gsimage, &colorspace);
	gsimage.ColorSpace = &colorspace;
	gsimage.BitsPerComponent = image.bits;
	gsimage.Width = image.width;
	gsimage.Height = image.height;

	/*
	 * Voodoo.
	 */

	penum = gs_image_enum_alloc(ctx->memory, "xps_parse_image_brush");
	if (!penum)
	    return gs_throw(-1, "gs_enum_allocate failed");

	if ((code = gs_image_init(penum, &gsimage, false, ctx->pgs)) < 0)
	    return gs_throw(code, "gs_image_init failed");

	if ((code = gs_image_next(penum, image.samples, count, &used)) < 0)
	    return gs_throw(code, "gs_image_next failed");

	if (count < used)
	    return gs_throw2(-1, "not enough image data (image=%d used=%d)", count, used);

	if (count > used)
	    return gs_throw2(0, "too much image data (image=%d used=%d)", count, used);

	gs_image_cleanup(penum, ctx->pgs);
    }
    gs_grestore(ctx->pgs);

    /* TODO: free image data */

    return 0;
}

