/* Copyright (C) 2006-2008 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* XPS interpreter - color functions */

#include "ghostxps.h"

#include "stream.h" /* for sizeof(stream) to work */

int
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

    return 0;
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

int
xps_parse_color(xps_context_t *ctx, char *base_uri, char *string, gs_color_space **csp, float *samples)
{
    xps_part_t *part;
    char *profile, *p;
    int i, n, code;

    char partname[1024];

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

	return 0;
    }

    else if (string[0] == 's' && string[1] == 'c' && string[2] == '#')
    {
	if (count_commas(string) == 2)
	    sscanf(string, "sc#%g,%g,%g", samples + 1, samples + 2, samples + 3);
	if (count_commas(string) == 3)
	    sscanf(string, "sc#%g,%g,%g,%g", samples, samples + 1, samples + 2, samples + 3);
	return 0;
    }

    else if (strstr(string, "ContextColor ") == string)
    {
	/* Crack the string for profile name and sample values */

	profile = strchr(string, ' ');
	if (profile)
	{
	    *profile++ = 0;
	    p = strchr(profile, ' ');
	    if (p)
	    {
		*p++ = 0;
	    }

	    n = count_commas(p) + 1;
	    i = 0;
	    while (i < n)
	    {
		samples[i++] = atof(p);
		p = strchr(p, ',');
		if (!p)
		    break;
		p ++;
		if (*p == ' ')
		    p ++;
	    }
	    while (i < n)
	    {
		samples[i++] = 0.0;
	    }
	}

	/* Default fallbacks if the ICC stuff fails */

	if (n == 2) /* alpha + tint */
	    *csp = ctx->gray;

	if (n == 5) /* alpha + CMYK */
	    *csp = ctx->cmyk;

	/* Find ICC colorspace part */

	xps_absolute_path(partname, base_uri, profile);
	part = xps_find_part(ctx, partname);
	if (!part)
	    return gs_throw1(-1, "cannot find icc profile part '%s'", partname);

#if 0 /* disable ICC profiles for beta */

	if (!part->icc)
	{
	    code = xps_parse_icc_profile(ctx, &part->icc, (byte*)part->data, part->size, n - 1);
	    if (code < 0)
		return gs_rethrow1(code, "cannot parse icc profile part '%s'", partname);
	}

	*csp = part->icc;
#endif

	return 0;
    }

    else
    {
	return gs_throw1(-1, "cannot parse color (%s)", string);
    }
}

static stream *
xps_stream_from_buffer(xps_context_t *ctx, byte *data, int length)
{
    stream *stm;

    stm = xps_alloc(ctx, sizeof(struct stream_s));
    if (!stm)
    {
	gs_throw(-1, "out of memory: stream struct");
	return NULL;
    }

    sread_string(stm, data, length);

    return stm;
}

int
xps_parse_icc_profile(xps_context_t *ctx, gs_color_space **csp, byte *data, int length, int ncomp)
{
    gs_color_space *colorspace;
    gs_cie_icc *info;
    stream *stm;
    int code;
    int i;

    // based on zseticcspace

    stm = xps_stream_from_buffer(ctx, data, length);
    if (!stm)
	return gs_rethrow(-1, "cannot create stream from buffer");

    code = gs_cspace_build_CIEICC(&colorspace, NULL, ctx->memory);
    if (code < 0)
	return gs_rethrow(code, "cannot build ICC colorspace");

    info = colorspace->params.icc.picc_info;
    info->num_components = ncomp; /* redundant but that's what the interface requires */
    info->instrp = stm;
    info->file_id = (stm->read_id | stm->write_id);

    code = gx_load_icc_profile(info);
    if (code < 0)
	return gs_throw(code, "gx_load_icc_profile failed");

    // cie_cache_joint
    // cie_set_finish

    *csp = colorspace;

    return 0;
}

int
xps_parse_solid_color_brush(xps_context_t *ctx, char *base_uri, xps_resource_t *dict, xps_item_t *node)
{
    char *opacity_att;
    char *color_att;
    gs_color_space *colorspace;
    float samples[32];

    color_att = xps_att(node, "Color");
    opacity_att = xps_att(node, "Opacity");

    colorspace = ctx->srgb;
    samples[0] = 1.0;
    samples[1] = 0.0;
    samples[2] = 0.0;
    samples[3] = 0.0;

    if (color_att)
	xps_parse_color(ctx, base_uri, color_att, &colorspace, samples);

    if (opacity_att)
	samples[0] = atof(opacity_att);

    xps_set_color(ctx, colorspace, samples);

    xps_fill(ctx);

    return 0;
}

