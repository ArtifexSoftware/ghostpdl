#include "ghostxps.h"

void
xps_set_color(xps_context_t *ctx, gs_color_space *cs, float *samples)
{
    gs_client_color cc;
    int i, n;

    if (ctx->opacity_only)
    {
	gs_setopacityalpha(ctx->pgs, 1.0);
	gs_setgray(ctx->pgs, samples[0]);
    }
    else
    {
	n = cs_num_components(cs);
	cc.pattern = 0;
	for (i = 0; i < n; i++)
	    cc.paint.values[i] = samples[i + 1];

	gs_setopacityalpha(ctx->pgs, samples[0]);
	gs_setcolorspace(ctx->pgs, cs);
	gs_setcolor(ctx->pgs, &cc);
    }
}

static int unhex(int chr)
{
    const char *hextable = "0123456789ABCDEF";
    return strchr(hextable, (toupper(chr))) - hextable;
}

static int count_commas(char *s)
{
    int n = 0;
    while (*s)
    {
	if (*s == ',')
	    n ++;
	s ++;
    }
    return n;
}

void
xps_parse_color(xps_context_t *ctx, char *string, gs_color_space **csp, float *samples)
{
    static gs_color_space *g_device_rgb = NULL;
    char *a, *b, *c;
    int n, i;

    samples[0] = 1.0;
    samples[1] = 0.0;
    samples[2] = 0.0;
    samples[3] = 0.0;

    *csp = ctx->srgb;

    if (string[0] == '#')
    {
	if (strlen(string) == 9)
	{
	    samples[0] = unhex(string[1]) * 16 + unhex(string[2]);
	    samples[1] = unhex(string[3]) * 16 + unhex(string[4]);
	    samples[2] = unhex(string[5]) * 16 + unhex(string[6]);
	    samples[3] = unhex(string[7]) * 16 + unhex(string[8]);
	}
	else
	{
	    samples[0] = 255.0;
	    samples[1] = unhex(string[1]) * 16 + unhex(string[2]);
	    samples[2] = unhex(string[3]) * 16 + unhex(string[4]);
	    samples[3] = unhex(string[5]) * 16 + unhex(string[6]);
	}

	samples[0] /= 255.0;
	samples[1] /= 255.0;
	samples[2] /= 255.0;
	samples[3] /= 255.0;
    }

    else if (string[0] == 's' && string[1] == 'c' && string[2] == '#')
    {
	if (count_commas(string) == 2)
	    sscanf(string, "sc#%g,%g,%g", samples + 1, samples + 2, samples + 3);
	if (count_commas(string) == 3)
	    sscanf(string, "sc#%g,%g,%g,%g", samples, samples + 1, samples + 2, samples + 3);
    }

    else if (strstr(string, "ContextColor ") == string)
    {
	dprintf1("ICC profile colors are not supported (%s)\n", string);

	a = strchr(string, ' ');
	if (a)
	{
	    *a++ = 0;
	    b = strchr(a, ' ');
	    if (b)
	    {
		*b++ = 0;
		dprintf2("context color (%s) = (%s)\n", a, b);
	    }
	    
	    n = count_commas(b) + 1;
	    i = 0;
	    while (i < n)
	    {
		samples[i++] = atof(b);
		b = strchr(b, ',');
		if (!b)
		    break;
		b ++;
		if (*b == ' ')
		    b ++;
	    }
	    while (i < n)
	    {
		samples[i++] = 0.0;
	    }
	}

	if (n == 2) /* alpha + tint */
	    *csp = ctx->gray;

	if (n == 5) /* alpha + CMYK */
	    *csp = ctx->cmyk;
    }

    else
    {
	gs_throw1(-1, "cannot parse color (%s)\n", string);
    }
}

int
xps_parse_solid_color_brush(xps_context_t *ctx,
	xps_resource_t *dict, xps_item_t *node)
{
    char *opacity_att;
    char *color_att;
    gs_color_space *colorspace;
    float samples[32];

    color_att = xps_att(node, "Color");
    opacity_att = xps_att(node, "Opacity");

    samples[0] = 1.0;
    samples[1] = 0.0;
    samples[2] = 0.0;
    samples[3] = 0.0;

    if (color_att)
	xps_parse_color(ctx, color_att, &colorspace, samples);

    if (opacity_att)
	samples[0] = atof(opacity_att);

    xps_set_color(ctx, colorspace, samples);

    xps_fill(ctx);
}

int
xps_parse_icc_profile(xps_context_t *ctx, gs_color_space **csp, byte *data, int length)
{
    gs_color_space *colorspace;
    gs_cie_icc *info;
    int code;
    int i;

    code = gs_cspace_build_CIEICC(&colorspace, NULL, ctx->memory);
    if (code < 0)
	return gs_rethrow(code, "cannot build ICC colorspace");

    info = colorspace->params.icc.picc_info;
    info->num_components = 4; /* XXX from where?! */
    info->instrp = 0; /* XXX stream */
    for (i = 0; i < info->num_components; i++)
    {
	info->Range.ranges[i].rmin = 0.0;
	info->Range.ranges[i].rmax = 1.0;
    }

    code = gx_load_icc_profile(info);
    if (code < 0)
	return gs_rethrow(code, "cannot load ICC profile");

    *csp = colorspace;

    return 0;
}

