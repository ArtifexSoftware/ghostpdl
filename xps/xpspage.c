#include "ghostxps.h"

int xps_parse_canvas(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_resource_t *new_dict = NULL;
    xps_item_t *node;

    char *transform_att;
    char *clip_att;
    char *opacity_att;
    char *opacity_mask_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *clip_tag = NULL;
    xps_item_t *opacity_mask_tag = NULL;

    gs_matrix transform;

    transform_att = xps_att(root, "RenderTransform");
    clip_att = xps_att(root, "Clip");
    opacity_att = xps_att(root, "Opacity");
    opacity_mask_att = xps_att(root, "OpacityMask");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Canvas.Resources"))
	{
	    new_dict = xps_parse_resource_dictionary(ctx, xps_down(node));
	    if (new_dict)
	    {
		new_dict->parent = dict;
		dict = new_dict;
	    }
	}

	if (!strcmp(xps_tag(node), "Canvas.RenderTransform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "Canvas.Clip"))
	    clip_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "Canvas.OpacityMask"))
	    opacity_mask_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);
    xps_resolve_resource_reference(ctx, dict, &clip_att, &clip_tag);
    xps_resolve_resource_reference(ctx, dict, &opacity_mask_att, &opacity_mask_tag);

    gs_gsave(ctx->pgs);

    gs_make_identity(&transform);
    if (transform_att)
	xps_parse_render_transform(ctx, transform_att, &transform);
    if (transform_tag)
	xps_parse_matrix_transform(ctx, transform_tag, &transform);
    gs_concat(ctx->pgs, &transform);

    if (clip_att)
    {
	xps_parse_abbreviated_geometry(ctx, clip_att);
	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);
    }

    if (clip_tag)
    {
	xps_parse_path_geometry(ctx, dict, clip_tag);
	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);
    }

    // TODO: opacity
    // TODO: opacity_mask

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Path"))
	    xps_parse_path(ctx, dict, node);
	if (!strcmp(xps_tag(node), "Glyphs"))
	    xps_parse_glyphs(ctx, dict, node);
	if (!strcmp(xps_tag(node), "Canvas"))
	    xps_parse_canvas(ctx, dict, node);
    }

    gs_grestore(ctx->pgs);

    if (new_dict)
    {
	xps_free_resource_dictionary(ctx, new_dict);
    }

    return 0;
}

int
xps_parse_fixed_page(xps_context_t *ctx, xps_part_t *part)
{
    xps_item_t *root, *node;
    xps_resource_t *dict;
    char *width_att;
    char *height_att;
    int code;

    dprintf1("processing page %s\n", part->name);

    root = xps_parse_xml(ctx, part->data, part->size);
    if (!root)
	return gs_rethrow(-1, "cannot parse xml");

    if (strcmp(xps_tag(root), "FixedPage"))
	return gs_throw1(-1, "expected FixedPage element (found %s)", xps_tag(root));

    width_att = xps_att(root, "Width");
    height_att = xps_att(root, "Height");

    if (!width_att)
	return gs_throw(-1, "FixedPage missing required attribute: Width");
    if (!height_att)
	return gs_throw(-1, "FixedPage missing required attribute: Height");

    dict = NULL;

    /* Setup new page */
    {
	gs_memory_t *mem = ctx->memory;
	gs_state *pgs = ctx->pgs;
	gx_device *dev = gs_currentdevice(pgs);
	gs_param_float_array fa;
	float fv[2];
	gs_c_param_list list;

	gs_c_param_list_write(&list, mem);

	fv[0] = atoi(width_att) / 96.0 * 72.0;
	fv[1] = atoi(height_att) / 96.0 * 72.0;
	fa.persistent = false;
	fa.data = fv;
	fa.size = 2;

	code = param_write_float_array((gs_param_list *)&list, ".MediaSize", &fa);
	if ( code >= 0 )
	{ 
	    gs_c_param_list_read(&list);
	    code = gs_putdeviceparams(dev, (gs_param_list *)&list);
	}
	gs_c_param_list_release(&list);

	/* nb this is for the demo it is wrong and should be removed */
	gs_initgraphics(pgs);

	/* 96 dpi default - and put the origin at the top of the page */

	gs_initmatrix(pgs);

	code = gs_scale(pgs, 72.0/96.0, -72.0/96.0);
	if (code < 0)
	    return gs_rethrow(code, "cannot set page transform");

	code = gs_translate(pgs, 0.0, -atoi(height_att));
	if (code < 0)
	    return gs_rethrow(code, "cannot set page transform");

	code = gs_erasepage(pgs);
	if (code < 0)
	    return gs_rethrow(code, "cannot clear page");
    }

    /* Draw contents */

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "FixedPage.Resources"))
	{
	    dputs("FixedPage has resources\n");
	    dict = xps_parse_resource_dictionary(ctx, xps_down(node));
	}

	if (!strcmp(xps_tag(node), "Path"))
	    xps_parse_path(ctx, dict, node);

	if (!strcmp(xps_tag(node), "Glyphs"))
	    xps_parse_glyphs(ctx, dict, node);

	if (!strcmp(xps_tag(node), "Canvas"))
	    xps_parse_canvas(ctx, dict, node);
    }

    /* Flush page */
    {
	code = xps_show_page(ctx, 1, true); /* copies, flush */
	if (code < 0)
	    return gs_rethrow(code, "cannot flush page");
    }

    if (dict)
    {
	xps_free_resource_dictionary(ctx, dict);
    }

    xps_free_item(ctx, root);

    return 0;
}

