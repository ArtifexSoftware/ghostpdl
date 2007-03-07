#include "ghostxps.h"

/*
 * Parse and draw an XPS <Glyphs> element.
 */

int
xps_parse_glyphs_imp(xps_context_t *ctx, xps_font_t *font, float size,
	float originx, float originy, int is_sideways,
	char *indices, char *unicode, int is_charpath)
{
    // parse unicode and indices strings and encode glyphs
    // and calculate metrics for positioning

    xps_glyph_metrics_t mtx;
    float x = originx;
    float y = originy;
    int gid = 0;

    if (is_charpath)
	xps_draw_font_glyph_to_path(ctx, font, gid, x, y);
    else
	xps_fill_font_glyph(ctx, font, gid, x, y);

    xps_measure_font_glyph(ctx, font, gid, &mtx);
}

int
xps_parse_glyphs(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    char *bidi_level_att;
    char *caret_stops_att;
    char *fill_att;
    char *font_size_att;
    char *font_uri_att;
    char *origin_x_att;
    char *origin_y_att;
    char *is_sideways_att;
    char *indices_att;
    char *unicode_att;
    char *style_att;
    char *transform_att;
    char *clip_att;
    char *opacity_att;
    char *opacity_mask_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *clip_tag = NULL;
    xps_item_t *fill_tag = NULL;
    xps_item_t *opacity_mask_tag = NULL;

    xps_part_t *part;
    xps_font_t *font;

    char partname[1024];

    int is_sideways = 0;
    int saved = 0;

    /*
     * Extract attributes and extended attributes.
     */

    bidi_level_att = xps_att(root, "BidiLevel");
    caret_stops_att = xps_att(root, "CaretStops");
    fill_att = xps_att(root, "Fill");
    font_size_att = xps_att(root, "FontRenderingEmSize");
    font_uri_att = xps_att(root, "FontUri");
    origin_x_att = xps_att(root, "OriginX");
    origin_y_att = xps_att(root, "OriginY");
    is_sideways_att = xps_att(root, "IsSideways");
    indices_att = xps_att(root, "Indices");
    unicode_att = xps_att(root, "UnicodeString");
    style_att = xps_att(root, "StyleSimulations");
    transform_att = xps_att(root, "RenderTransform");
    clip_att = xps_att(root, "Clip");
    opacity_att = xps_att(root, "Opacity");
    opacity_mask_att = xps_att(root, "OpacityMask");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Glyphs.RenderTransform"))
	    transform_tag = xps_down(node);

	if (!strcmp(xps_tag(node), "Glyphs.OpacityMask"))
	    opacity_mask_tag = xps_down(node);

	if (!strcmp(xps_tag(node), "Glyphs.Clip"))
	    clip_tag = xps_down(node);

	if (!strcmp(xps_tag(node), "Glyphs.Fill"))
	    fill_tag = xps_down(node);
    }

    /*
     * Check that we have all the necessary information.
     */

    if (!font_size_att || !font_uri_att || !origin_x_att || !origin_y_att)
	return gs_throw(-1, "missing attributes in glyphs element");

    if (!indices_att && !unicode_att)
	return 0; /* nothing to draw */

    if (is_sideways_att)
	is_sideways = !strcmp(is_sideways_att, "true");

    /*
     * Find and load the font resource
     */

    // TODO: get subfont index from # part of uri

    xps_absolute_path(partname, ctx->pwd, font_uri_att);
    part = xps_find_part(ctx, partname);
    if (!part)
	return gs_throw1(-1, "cannot find font resource part '%s'", partname);

    font = xps_new_font(ctx, part->data, part->size, 0);
    if (!font)
	return gs_rethrow1(-1, "cannot load font resource '%s'", partname);

    /*
     * Set up graphics state.
     */

    if (transform_att || transform_tag)
    {
	gs_matrix transform;

	dputs("  glyphs transform\n");

	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	if (transform_att)
	    xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
	    xps_parse_matrix_transform(ctx, transform_tag, &transform);

	gs_concat(ctx->pgs, &transform);
    }

    if (clip_att || clip_tag)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("  glyphs clip\n");

	if (clip_att)
	{
	    xps_parse_abbreviated_geometry(ctx, clip_att);
	    gs_clip(ctx->pgs);
	}
    }

    /*
     * If it's a solid color brush fill/stroke do a simple fill
     */

    if (fill_tag && !strcmp(xps_tag(fill_tag), "SolidColorBrush"))
    {
	fill_att = xps_att(fill_tag, "Color");
	fill_tag = NULL;
    }

    if (fill_att)
    {
	float argb[4];
	dputs("  glyphs solid color fill\n");
	xps_parse_color(ctx, fill_att, argb);
	if (argb[0] > 0.001)
	{
	    gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
	    xps_parse_glyphs_imp(ctx, font, atof(font_size_att),
		    atof(origin_x_att), atof(origin_y_att),
		    is_sideways, indices_att, unicode_att, 0);
	}
    }

    /*
     * If it's a visual brush or image, use the charpath as a clip mask to paint brush
     */

    if (fill_tag)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("  glyphs complex brush\n");

	xps_parse_glyphs_imp(ctx, font, atof(font_size_att),
		atof(origin_x_att), atof(origin_y_att),
		is_sideways, indices_att, unicode_att, 1);

	gs_clip(ctx->pgs);
	dputs("clip\n");

	if (!strcmp(xps_tag(fill_tag), "ImageBrush"))
	    xps_parse_image_brush(ctx, fill_tag);
	if (!strcmp(xps_tag(fill_tag), "VisualBrush"))
	    xps_parse_visual_brush(ctx, fill_tag);
	if (!strcmp(xps_tag(fill_tag), "LinearGradientBrush"))
	    xps_parse_linear_gradient_brush(ctx, fill_tag);
	if (!strcmp(xps_tag(fill_tag), "RadialGradientBrush"))
	    xps_parse_radial_gradient_brush(ctx, fill_tag);
    }

    /*
     * Remember to restore if we did a gsave
     */

    if (saved)
    {
	gs_grestore(ctx->pgs);
    }

    if (font)
    {
	xps_free_font(ctx, font);
    }

    return 0;
}

