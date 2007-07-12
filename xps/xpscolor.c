#include "ghostxps.h"

void
xps_set_color(xps_context_t *ctx, float *argb)
{
    if (ctx->opacity_only)
    {
	gs_setopacityalpha(ctx->pgs, 1.0);
	gs_setrgbcolor(ctx->pgs, argb[0], argb[0], argb[0]);
    }
    else
    {
	gs_setopacityalpha(ctx->pgs, argb[0]);
	gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
    }
}

void
xps_parse_color(xps_context_t *ctx, char *hexstring, float *argb)
{
    const char *hextable = "0123456789ABCDEF";
    char *hexstringp = hexstring;

#define HEXTABLEINDEX(chr) (strchr(hextable, (toupper(chr))) - hextable)

    /* nb need to look into color specification */
    if (strlen(hexstring) == 9)
    {
	argb[0] = HEXTABLEINDEX(hexstringp[1]) * 16 + HEXTABLEINDEX(hexstringp[2]);
	argb[1] = HEXTABLEINDEX(hexstringp[3]) * 16 + HEXTABLEINDEX(hexstringp[4]);
	argb[2] = HEXTABLEINDEX(hexstringp[5]) * 16 + HEXTABLEINDEX(hexstringp[6]);
	argb[3] = HEXTABLEINDEX(hexstringp[7]) * 16 + HEXTABLEINDEX(hexstringp[8]);
    }
    else
    {
	argb[0] = 255.0;
	argb[1] = HEXTABLEINDEX(hexstringp[1]) * 16 + HEXTABLEINDEX(hexstringp[2]);
	argb[2] = HEXTABLEINDEX(hexstringp[3]) * 16 + HEXTABLEINDEX(hexstringp[4]);
	argb[3] = HEXTABLEINDEX(hexstringp[5]) * 16 + HEXTABLEINDEX(hexstringp[6]);
    }

    argb[0] /= 255.0;
    argb[1] /= 255.0;
    argb[2] /= 255.0;
    argb[3] /= 255.0;
}

int
xps_parse_solid_color_brush(xps_context_t *ctx,
	xps_resource_t *dict, xps_item_t *node)
{
    char *opacity_att;
    char *color_att;

    float argb[4];

    color_att = xps_att(node, "Color");
    opacity_att = xps_att(node, "Opacity");

    argb[0] = 1.0;
    argb[1] = 0.0;
    argb[2] = 0.0;
    argb[3] = 0.0;

    if (color_att)
	xps_parse_color(ctx, color_att, argb);

    if (opacity_att)
	argb[0] = atof(opacity_att);

    xps_set_color(ctx, argb);

    xps_fill(ctx);
}

