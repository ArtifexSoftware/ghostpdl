#include "ghostsvg.h"

/* Color keywords (white, blue, fuchsia)
 * System color keywords (ActiveBorder, ButtonFace -- need to find reasonable defaults)
 * #fb0 (expand to #ffbb00)
 * #ffbb00
 * rgb(255,255,255)
 * rgb(100%,100%,100%)
 *
 * "red icc-color(profileName,255,0,0)" (not going to support for now)
 */

struct
{
    const char *name;
    float red, green, blue;
}
svg_predefined_colors[] =
{
#include "svgcolorlist.h"
};

static int unhex(int chr)
{
    const char *hextable = "0123456789ABCDEF";
    return strchr(hextable, (toupper(chr))) - hextable;
}

void
svg_set_color(svg_context_t *ctx, float *rgb)
{
    gs_client_color cc;

    cc.pattern = 0;
    cc.paint.values[0] = rgb[0];
    cc.paint.values[1] = rgb[1];
    cc.paint.values[2] = rgb[2];

    gs_setcolor(ctx->pgs, &cc);
}

void svg_set_fill_color(svg_context_t *ctx)
{
    svg_set_color(ctx, ctx->fill_color);
}

void svg_set_stroke_color(svg_context_t *ctx)
{
    svg_set_color(ctx, ctx->stroke_color);
}

int
svg_parse_color(char *str, float *rgb)
{
    int l, m, r, cmp;

    rgb[0] = 0.0;
    rgb[1] = 0.0;
    rgb[2] = 0.0;

    /* Crack hex-coded RGB */

    if (str[0] == '#')
    {
	str ++;

	if (strlen(str) == 3)
	{
	    rgb[0] = (unhex(str[0]) * 16 + unhex(str[0])) / 255.0;
	    rgb[1] = (unhex(str[1]) * 16 + unhex(str[1])) / 255.0;
	    rgb[2] = (unhex(str[2]) * 16 + unhex(str[2])) / 255.0;
	    return 0;
	}

	if (strlen(str) == 6)
	{
	    rgb[0] = (unhex(str[0]) * 16 + unhex(str[1])) / 255.0;
	    rgb[1] = (unhex(str[2]) * 16 + unhex(str[3])) / 255.0;
	    rgb[2] = (unhex(str[4]) * 16 + unhex(str[5])) / 255.0;
	    return 0;
	}

	return gs_throw(-1, "syntax error in color - wrong length of string after #");
    }

    /* TODO: parse rgb(X,Y,Z) syntax */
    /* TODO: parse icc-profile(X,Y,Z,W) syntax */

    /* Search for a pre-defined color */

    else
    {
	l = 0;
	r = sizeof(svg_predefined_colors) / sizeof(svg_predefined_colors[0]);

	while (l <= r)
	{
	    m = (l + r) / 2;
	    cmp = strcmp(svg_predefined_colors[m].name, str);
	    if (cmp > 0)
		r = m - 1;
	    else if (cmp < 0)
		l = m + 1;
	    else
	    {
		rgb[0] = svg_predefined_colors[m].red;
		rgb[1] = svg_predefined_colors[m].green;
		rgb[2] = svg_predefined_colors[m].blue;
		return 0;
	    }
	}
    }

    return gs_throw1(-1, "cannot recognize color syntax: '%s'", str);
}
