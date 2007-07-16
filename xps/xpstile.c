#include "ghostxps.h"

/*
 * Parse a tiling brush (visual and image brushes at this time) common
 * properties. Use the callback to draw the individual tiles.
 */

enum { TILE_NONE, TILE_TILE, TILE_FLIP_X, TILE_FLIP_Y, TILE_FLIP_X_Y };

struct tile_closure_s
{
    xps_context_t *ctx;
    xps_resource_t *dict;
    xps_item_t *tag;
    void *user;
    int (*func)(xps_context_t*, xps_resource_t*, xps_item_t*, void*);
};

static int
xps_paint_tiling_brush(const gs_client_color *pcc, gs_state *newpgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    const struct tile_closure_s *c = ppat->client_data;
    gs_state *oldpgs;
    int code;

    oldpgs = c->ctx->pgs;
    c->ctx->pgs = newpgs;

    code = c->func(c->ctx, c->dict, c->tag, c->user);

    c->ctx->pgs = oldpgs;

    return code;
}

int
xps_parse_tiling_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root,
	int (*func)(xps_context_t*, xps_resource_t*, xps_item_t*, void*), void *user)
{
    xps_item_t *node;

    char *opacity_att;
    char *transform_att;
    char *viewbox_att;
    char *viewport_att;
    char *tile_mode_att;
    char *viewbox_units_att;
    char *viewport_units_att;

    xps_item_t *transform_tag = NULL;

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

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "ImageBrush.Transform"))
	    transform_tag = xps_down(node);
	if (!strcmp(xps_tag(node), "VisualBrush.Transform"))
	    transform_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);

    gs_make_identity(&transform);
    if (transform_att)
	xps_parse_render_transform(ctx, transform_att, &transform);
    if (transform_tag)
	xps_parse_matrix_transform(ctx, transform_tag, &transform);

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

    gs_gsave(ctx->pgs);

    gs_concat(ctx->pgs, &transform);

    xps_begin_opacity(ctx, dict, opacity_att, NULL);

    if (tile_mode != TILE_NONE)
    {
	struct tile_closure_s closure;

	gs_client_pattern gspat;
	gs_client_color gscolor;
	gs_color_space *cs;
	gs_matrix mat;

	closure.ctx = ctx;
	closure.dict = dict;
	closure.tag = root;
	closure.user = user;
	closure.func = func;

	gs_pattern1_init(&gspat);
	uid_set_UniqueID(&gspat.uid, gs_next_ids(ctx->memory, 1));
	gspat.PaintType = 1;
	gspat.TilingType = 1;
	gspat.PaintProc = xps_paint_tiling_brush;
	gspat.client_data = &closure;

	gspat.BBox.p.x = viewbox.p.x;
	gspat.BBox.p.y = viewbox.p.y;
	gspat.BBox.q.x = viewbox.q.x;
	gspat.BBox.q.y = viewbox.q.y;
	gspat.XStep = viewbox.q.x - viewbox.p.x;
	gspat.YStep = viewbox.q.y - viewbox.p.y;

	gs_translate(ctx->pgs, viewport.p.x, viewport.p.y);
	gs_scale(ctx->pgs, scalex, scaley);
	gs_translate(ctx->pgs, -viewbox.p.x, -viewbox.p.y);

	cs = gs_cspace_new_DeviceRGB(ctx->memory);
	gs_setcolorspace(ctx->pgs, cs);
	gs_make_identity(&mat);
	gs_makepattern(&gscolor, &gspat, &mat, ctx->pgs, NULL);
	gs_setpattern(ctx->pgs, &gscolor);

	xps_fill(ctx);
    }
    else
    {
	xps_clip(ctx);

	gs_translate(ctx->pgs, viewport.p.x, viewport.p.y);
	gs_scale(ctx->pgs, scalex, scaley);
	gs_translate(ctx->pgs, -viewbox.p.x, viewbox.p.y);

	gs_moveto(ctx->pgs, viewbox.p.x, viewbox.p.y);
	gs_lineto(ctx->pgs, viewbox.p.x, viewbox.q.y);
	gs_lineto(ctx->pgs, viewbox.q.x, viewbox.q.y);
	gs_lineto(ctx->pgs, viewbox.q.x, viewbox.p.y);
	gs_closepath(ctx->pgs);
	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);

	func(ctx, dict, root, user);
    }

    xps_end_opacity(ctx, dict, opacity_att, NULL);

    gs_grestore(ctx->pgs);

    return 0;
}

