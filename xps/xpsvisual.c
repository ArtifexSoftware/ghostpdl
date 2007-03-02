#include "ghostxps.h"

enum { TILE_NONE, TILE_TILE, TILE_FLIP_X, TILE_FLIP_Y, TILE_FLIP_X_Y };

int
xps_parse_visual_brush(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;
    int i, k;
    int tile_x, tile_y;

    char *opacity_att;
    char *transform_att;
    char *viewbox_att;
    char *viewport_att;
    char *tile_mode_att;
    char *viewbox_units_att;
    char *viewport_units_att;
    char *visual_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *visual_tag = NULL;

    gs_matrix transform;
    gs_rect viewbox;
    gs_rect viewport;
    float scalex, scaley;
    int tile_mode;

    opacity_att = xps_att(root, "Opacity");
    transform_att = xps_att(root, "Transform");
    viewbox_att = xps_att(root, "Viewbox");
    viewport_att = xps_att(root, "Viewport");
    tile_mode_att = xps_att(root, "TileMode");
    viewbox_units_att = xps_att(root, "ViewboxUnits");
    viewport_units_att = xps_att(root, "ViewportUnits");
    visual_att = xps_att(root, "Visual");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "VisualBrush.Transform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "VisualBrush.Visual"))
	    visual_tag = xps_down(node);
    }


    dputs("drawing visual brush\n");

    if (!viewbox_att || !viewport_att)
	return gs_throw(-1, "missing viewbox/viewport attribute in visual brush");

    if (visual_att)
	/* find resource */ ;

    if (!visual_tag)
	return 0; /* move along, nothing to see */


    gs_gsave(ctx->pgs);

    gs_make_identity(&transform);
    if (transform_att)
	xps_parse_render_transform(ctx, transform_att, &transform);
    if (transform_tag)
	xps_parse_matrix_transform(ctx, transform_tag, &transform);
    gs_concat(ctx->pgs, &transform);

    viewbox.p.x = 0.0; viewbox.p.y = 0.0;
    viewbox.q.x = 1.0; viewbox.q.y = 1.0;
    if (viewbox_att)
	xps_parse_rectangle(ctx, viewbox_att, &viewbox);

    viewport.p.x = 0.0; viewport.p.y = 0.0;
    viewport.q.x = 1.0; viewport.q.y = 1.0;
    if (viewport_att)
	xps_parse_rectangle(ctx, viewport_att, &viewport);

    scalex = (viewport.q.x - viewport.p.x) / (viewbox.q.x - viewbox.p.x);
    scaley = (viewport.q.y - viewport.p.y) / (viewbox.q.y - viewbox.p.y);
    gs_translate(ctx->pgs, viewport.p.x, viewport.p.y);
    gs_scale(ctx->pgs, scalex, scaley);
    gs_translate(ctx->pgs, -viewbox.p.x, viewbox.p.y);

    tile_mode = TILE_NONE;
    if (tile_mode_att)
    {
	if (!strcmp(tile_mode_att, "None"))
	    tile_mode = TILE_NONE;
	if (!strcmp(tile_mode_att, "Tile"))
	    tile_mode = TILE_TILE;
	if (!strcmp(tile_mode_att, "FlipX"))
	    tile_mode = TILE_FLIP_X;
	if (!strcmp(tile_mode_att, "FlipY"))
	    tile_mode = TILE_FLIP_Y;
	if (!strcmp(tile_mode_att, "FlipXY"))
	    tile_mode = TILE_FLIP_X_Y;
    }

    if (tile_mode == TILE_NONE)
	tile_x = tile_y = 1;
    else
	tile_x = tile_y = 6;

    gs_translate(ctx->pgs,
	    (viewbox.p.x - viewbox.q.x) * (tile_x/2),
	    (viewbox.p.y - viewbox.q.y) * (tile_y/2));

    for (k = 0; k < tile_y; k++)
    {
	for (i = 0; i < tile_x; i++)
	{
	    gs_gsave(ctx->pgs);
	    gs_newpath(ctx->pgs);
	    gs_moveto(ctx->pgs, viewbox.p.x, viewbox.p.y);
	    gs_lineto(ctx->pgs, viewbox.p.x, viewbox.q.y);
	    gs_lineto(ctx->pgs, viewbox.q.x, viewbox.q.y);
	    gs_lineto(ctx->pgs, viewbox.q.x, viewbox.p.y);
	    gs_closepath(ctx->pgs);
	    gs_clip(ctx->pgs);

	    if (!strcmp(xps_tag(visual_tag), "Path"))
		xps_parse_path(ctx, visual_tag);
	    if (!strcmp(xps_tag(visual_tag), "Glyphs"))
		xps_parse_glyphs(ctx, visual_tag);
	    if (!strcmp(xps_tag(visual_tag), "Canvas"))
		xps_parse_canvas(ctx, visual_tag);

	    gs_grestore(ctx->pgs);

	    gs_translate(ctx->pgs, -(viewbox.p.x - viewbox.q.x), 0.0);
	}

	gs_translate(ctx->pgs,
		tile_x * (viewbox.p.x - viewbox.q.x),
		-(viewbox.p.y - viewbox.q.y));
    }

    gs_grestore(ctx->pgs);

    dputs("finished visual brush\n");

    return 0;
}

