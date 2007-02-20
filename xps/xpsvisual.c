#include "ghostxps.h"

/* TODO: split the common tiled brush stuff out somewhere */

int
xps_parse_visual_brush(xps_context_t *ctx, xps_item_t *root)
{
    xps_item_t *node;

    char *opacity;
    char *transform;
    char *viewbox;
    char *viewport;
    char *tile_mode;
    char *viewbox_units;
    char *viewport_units;
    char *visual;
    xps_item_t *transform_node;
    xps_item_t *visual_node;

    opacity = xps_att(root, "Opacity");
    transform = xps_att(root, "Transform");
    viewbox = xps_att(root, "Viewbox");
    viewport = xps_att(root, "Viewport");
    tile_mode = xps_att(root, "TileMode");
    viewbox_units = xps_att(root, "ViewboxUnits");
    viewport_units = xps_att(root, "ViewportUnits");
    visual = xps_att(root, "Visual");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "VisualBrush.Transform"))
	    transform_node = xps_down(node);
	if (!strcmp(xps_tag(node), "VisualBrush.Visual"))
	    visual_node = xps_down(node);
    }

    dputs("drawing visual brush\n");

    return 0;
}

