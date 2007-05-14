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
xps_parse_image_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;
    xps_part_t *part;
    xps_image_t image;
    int code;

    char *opacity_att;
    char *transform_att;
    char *viewbox_att;
    char *viewport_att;
    char *tile_mode_att;
    char *viewbox_units_att;
    char *viewport_units_att;
    char *image_source_att;

    xps_item_t *transform_tag = NULL;

    char partname[1024];

    opacity_att = xps_att(root, "Opacity");
    transform_att = xps_att(root, "Transform");
    viewbox_att = xps_att(root, "Viewbox");
    viewport_att = xps_att(root, "Viewport");
    tile_mode_att = xps_att(root, "TileMode");
    viewbox_units_att = xps_att(root, "ViewboxUnits");
    viewport_units_att = xps_att(root, "ViewportUnits");
    image_source_att = xps_att(root, "ImageSource");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "ImageBrush.Transform"))
	    transform_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);

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

    gs_gsave(ctx->pgs);
    {
	gs_image_enum *penum;
	gs_color_space *colorspace;
	gs_image_t gsimage;
	unsigned int count = image.stride * image.height;
	unsigned int used = 0;
	gs_matrix transform;
	gs_rect viewbox;
	gs_rect viewport;
	float scalex, scaley;
	float resx, resy;

	/*
	 * Figure out transformation.
	 */

	resx = image.xres / 96.0;
	resy = image.yres / 96.0;

	dprintf2("  image resolution = %d x %d\n", image.xres, image.yres);

	gs_make_identity(&transform);
	if (transform_att)
	    xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
	    xps_parse_matrix_transform(ctx, transform_tag, &transform);
	gs_concat(ctx->pgs, &transform);

	dprintf6("  image transform [%g %g %g %g %g %g]\n",
		transform.xx, transform.xy,
		transform.yx, transform.yy,
		transform.tx, transform.ty);

	viewbox.p.x = 0.0; viewbox.p.y = 0.0;
	viewbox.q.x = 1.0; viewbox.q.y = 1.0;
	if (viewbox_att)
	    xps_parse_rectangle(ctx, viewbox_att, &viewbox);

	dprintf4("  image viewbox = [%g %g %g %g]\n",
		viewbox.p.x, viewbox.p.y,
		viewbox.q.x, viewbox.q.y);

	viewport.p.x = 0.0; viewport.p.y = 0.0;
	viewport.q.x = 1.0; viewport.q.y = 1.0;
	if (viewport_att)
	    xps_parse_rectangle(ctx, viewport_att, &viewport);

	dprintf4("  image viewport = [%g %g %g %g]\n",
		viewport.p.x, viewport.p.y,
		viewport.q.x, viewport.q.y);

	viewbox.p.x *= resx; viewbox.p.y *= resy;
	viewbox.q.x *= resx; viewbox.q.y *= resy;

	scalex = (viewport.q.x - viewport.p.x) / (viewbox.q.x - viewbox.p.x);
	scaley = (viewport.q.y - viewport.p.y) / (viewbox.q.y - viewbox.p.y);

	gs_translate(ctx->pgs, viewport.p.x, viewport.p.y);
	gs_scale(ctx->pgs, scalex, scaley);
	gs_translate(ctx->pgs, -viewbox.p.x, viewbox.p.y);

	/*
	 * Set up colorspace and image structs.
	 */

	switch (image.colorspace)
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
	gsimage.BitsPerComponent = image.bits;
	gsimage.Width = image.width;
	gsimage.Height = image.height;

	/*
	 * Voodoo.
	 */

	penum = gs_image_enum_alloc(ctx->memory, "xps_parse_image_brush (gs_image_enum_alloc)");
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

	gs_image_cleanup_and_free_enum(penum, ctx->pgs);
	// TODO: free colorspace
    }
    gs_grestore(ctx->pgs);

    xps_free(ctx, image.samples);

    return 0;
}

