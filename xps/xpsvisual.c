#include "ghostxps.h"

/* TODO: split the common tiled brush stuff out somewhere */

int
xps_parse_visual_brush(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

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

    return 0;
}

