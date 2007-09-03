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
xps_parse_color(xps_context_t *ctx, char *string, float *argb)
{
    argb[0] = 1.0;
    argb[1] = 0.0;
    argb[2] = 0.0;
    argb[3] = 0.0;

    if (string[0] == '#')
    {
	if (strlen(string) == 9)
	{
	    argb[0] = unhex(string[1]) * 16 + unhex(string[2]);
	    argb[1] = unhex(string[3]) * 16 + unhex(string[4]);
	    argb[2] = unhex(string[5]) * 16 + unhex(string[6]);
	    argb[3] = unhex(string[7]) * 16 + unhex(string[8]);
	}
	else
	{
	    argb[0] = 255.0;
	    argb[1] = unhex(string[1]) * 16 + unhex(string[2]);
	    argb[2] = unhex(string[3]) * 16 + unhex(string[4]);
	    argb[3] = unhex(string[5]) * 16 + unhex(string[6]);
	}

	argb[0] /= 255.0;
	argb[1] /= 255.0;
	argb[2] /= 255.0;
	argb[3] /= 255.0;
    }

    else if (string[0] == 's' && string[1] == 'c' && string[2] == '#')
    {
	if (count_commas(string) == 2)
	    sscanf(string, "sc#%g,%g,%g", argb + 1, argb + 2, argb + 3);
	if (count_commas(string) == 3)
	    sscanf(string, "sc#%g,%g,%g,%g", argb, argb + 1, argb + 2, argb + 3);
    }

    else if (strstr(string, "ContextColor ") == string)
    {
	dprintf1("ICC profile colors are not supported (%s)\n", string);
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


