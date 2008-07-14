#include "ghostxps.h"

int
xps_parse_brush(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *node)
{
    if (!strcmp(xps_tag(node), "SolidColorBrush"))
	return xps_parse_solid_color_brush(ctx, dict, node);
    if (!strcmp(xps_tag(node), "ImageBrush"))
	return xps_parse_image_brush(ctx, dict, node);
    if (!strcmp(xps_tag(node), "VisualBrush"))
	return xps_parse_visual_brush(ctx, dict, node);
    if (!strcmp(xps_tag(node), "LinearGradientBrush"))
	return xps_parse_linear_gradient_brush(ctx, dict, node);
    if (!strcmp(xps_tag(node), "RadialGradientBrush"))
	return xps_parse_radial_gradient_brush(ctx, dict, node);
    return gs_throw1(-1, "unknown brush tag: %s", xps_tag(node));
}

int
xps_parse_element(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *node)
{
    if (!strcmp(xps_tag(node), "Path"))
	return xps_parse_path(ctx, dict, node);
    if (!strcmp(xps_tag(node), "Glyphs"))
	return xps_parse_glyphs(ctx, dict, node);
    if (!strcmp(xps_tag(node), "Canvas"))
	return xps_parse_canvas(ctx, dict, node);
    /* skip unknown tags (like Foo.Resources and similar) */
    return 0;
}

void
xps_parse_render_transform(xps_context_t *ctx, char *transform, gs_matrix *matrix)
{
    float args[6];
    char *s = transform;
    int i;

    args[0] = 1.0; args[1] = 0.0;
    args[2] = 0.0; args[3] = 1.0;
    args[4] = 0.0; args[5] = 0.0;

    for (i = 0; i < 6 && *s; i++)
    {
	args[i] = atof(s);
	while (*s && *s != ',')
	    s++;
	if (*s == ',')
	    s++;
    }

    matrix->xx = args[0]; matrix->xy = args[1];
    matrix->yx = args[2]; matrix->yy = args[3];
    matrix->tx = args[4]; matrix->ty = args[5];
}

void
xps_parse_matrix_transform(xps_context_t *ctx, xps_item_t *root, gs_matrix *matrix)
{
    char *transform;

    gs_make_identity(matrix);

    if (!strcmp(xps_tag(root), "MatrixTransform"))
    {
	transform = xps_att(root, "Matrix");
	if (transform)
	    xps_parse_render_transform(ctx, transform, matrix);
    }
}

void
xps_parse_rectangle(xps_context_t *ctx, char *text, gs_rect *rect)
{
    float args[4];
    char *s = text;
    int i;

    args[0] = 0.0; args[1] = 0.0;
    args[2] = 1.0; args[3] = 1.0;

    for (i = 0; i < 4 && *s; i++)
    {
	args[i] = atof(s);
	while (*s && *s != ',')
	    s++;
	if (*s == ',')
	    s++;
    }

    rect->p.x = args[0];
    rect->p.y = args[1];
    rect->q.x = args[0] + args[2];
    rect->q.y = args[1] + args[3];
}

