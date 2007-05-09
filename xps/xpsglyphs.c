#include "ghostxps.h"

#include <ctype.h>

static inline int unhex(int i)
{
    if (isdigit(i))
	return i - '0';
    return tolower(i) - 'a' + 10;
}

void xps_debug_path(xps_context_t *ctx)
{
    segment *seg;
    curve_segment *cseg;

    seg = (segment*)ctx->pgs->path->first_subpath;
    while (seg)
    {
        switch (seg->type)
        {
        case s_start:
	    dprintf2("%g %g moveto\n", 
                    fixed2float(seg->pt.x) * 0.001,
                    fixed2float(seg->pt.y) * 0.001);
            break;
        case s_line:
	    dprintf2("%g %g lineto\n", 
                    fixed2float(seg->pt.x) * 0.001,
                    fixed2float(seg->pt.y) * 0.001);
            break;
        case s_line_close:
	    dputs("closepath\n");
            break;
        case s_curve:
            cseg = (curve_segment*)seg;
	    dprintf6("%g %g %g %g %g %g curveto\n", 
                    fixed2float(cseg->p1.x) * 0.001,
                    fixed2float(cseg->p1.y) * 0.001,
                    fixed2float(cseg->p2.x) * 0.001,
                    fixed2float(cseg->p2.y) * 0.001,
                    fixed2float(seg->pt.x) * 0.001,
                    fixed2float(seg->pt.y) * 0.001);
            break;
        }
        seg = seg->next;
    }
}

/*
 * Some fonts in XPS are obfuscated by XOR:ing the first 32 bytes of the
 * data with the GUID in the fontname.
 */
int
xps_deobfuscate_font_resource(xps_context_t *ctx, xps_part_t *part)
{
    byte buf[33];
    byte key[16];
    char *p;
    int i;

    dprintf1("deobfuscating font data in part '%s'\n", part->name);

    p = strrchr(part->name, '/');
    if (!p)
	p = part->name;

    for (i = 0; i < 32 && *p; p++)
    {
	if (isxdigit(*p))
	    buf[i++] = *p;
    }
    buf[i] = 0;

    if (i != 32)
	return gs_throw(-1, "cannot extract GUID from part name");

    for (i = 0; i < 16; i++)
	key[i] = unhex(buf[i*2+0]) * 16 + unhex(buf[i*2+1]);

    dputs("KEY ");
    for (i = 0; i < 16; i++)
    {
	dprintf1("%02x", key[i]);
	part->data[i] ^= key[15-i];
	part->data[i+16] ^= key[15-i];
    }
    dputs("\n");

    {
	static int id = 0;
	char buf[25];
	FILE *fp;
	sprintf(buf, "font%d.otf", id++);
	dprintf1("saving font data to %s\n", buf);
	fp = fopen(buf, "wb");
	fwrite(part->data, part->size, 1, fp);
	fclose(fp);
    }

    return 0;
}

int
xps_select_best_font_encoding(xps_font_t *font)
{
    static struct { int pid, eid; } xps_cmap_list[] =
    {
	{ 3, 10 },      /* Unicode with surrogates */
	{ 3, 1 },       /* Unicode without surrogates */
	{ 3, 5 },       /* Wansung */
	{ 3, 4 },       /* Big5 */
	{ 3, 3 },       /* Prc */
	{ 3, 2 },       /* ShiftJis */
	{ 3, 0 },       /* Symbol */
	// { 0, * }, -- Unicode (deprecated)
	{ 1, 0 },
	{ -1, -1 },
    };

    int i, k, n, pid, eid;

    n = xps_count_font_encodings(font);
    for (k = 0; xps_cmap_list[k].pid != -1; k++)
    {
	for (i = 0; i < n; i++)
	{
	    xps_identify_font_encoding(font, i, &pid, &eid);
	    if (pid == xps_cmap_list[k].pid && eid == xps_cmap_list[k].eid)
	    {
		xps_select_font_encoding(font, i);
		return 0;
	    }
	}
    }

    return gs_throw(-1, "could not find a suitable cmap");
}

/*
 * Call text drawing primitives.
 */

int xps_draw_font_glyph_to_path(xps_context_t *ctx, xps_font_t *font, int gid, float x, float y)
{
    gs_text_params_t params;
    gs_text_enum_t *textenum;
    int code;

    params.operation = TEXT_FROM_SINGLE_GLYPH | TEXT_DO_FALSE_CHARPATH;
    params.data.d_glyph = gid;
    params.size = 1;

    // this code is not functional. it needs a lot more matrix voodoo.

    dputs("--draw glyph to path--\n");

    gs_moveto(ctx->pgs, x, y);

    if (gs_text_begin(ctx->pgs, &params, ctx->memory, &textenum) != 0)
	return gs_throw(-1, "cannot gs_text_begin()");
    if (gs_text_process(textenum) != 0)
	return gs_throw(-1, "cannot gs_text_process()");
    gs_text_release(textenum, "gslt font outline");

    xps_debug_path(ctx);
    gs_fill(ctx->pgs);

    dputs("--end--\n");

    return 0;
}

int xps_fill_font_glyph(xps_context_t *ctx, xps_font_t *font, int gid, float x, float y)
{
    gs_text_params_t params;
    gs_text_enum_t *textenum;
    int code;

    params.operation = TEXT_FROM_SINGLE_GLYPH | TEXT_DO_DRAW;
    params.data.d_glyph = gid;
    params.size = 1;

    // dprintf3("filling glyph %d at %g,%g\n", gid, x, y);

    gs_moveto(ctx->pgs, x, y);

    code = gs_text_begin(ctx->pgs, &params, ctx->memory, &textenum);
    if (code != 0)
	return gs_throw1(-1, "cannot gs_text_begin() (%d)", code);

    code = gs_text_process(textenum);
    if (code != 0)
	return gs_throw1(-1, "cannot gs_text_process() (%d)", code);

    gs_text_release(textenum, "gslt font render");

    return 0;
}


/*
 * Parse and draw an XPS <Glyphs> element.
 *
 * Indices syntax:

 GlyphIndices   = GlyphMapping ( ";" GlyphMapping ) 
 GlyphMapping   = ( [ClusterMapping] GlyphIndex ) [GlyphMetrics] 
 ClusterMapping = "(" ClusterCodeUnitCount [":" ClusterGlyphCount] ")" 
 ClusterCodeUnitCount = * DIGIT 
 ClusterGlyphCount    = * DIGIT 
 GlyphIndex     = * DIGIT 
 GlyphMetrics   = "," AdvanceWidth ["," uOffset ["," vOffset]] 
 AdvanceWidth   = ["+"] RealNum 
 uOffset        = ["+" | "-"] RealNum 
 vOffset        = ["+" | "-"] RealNum 
 RealNum        = ((DIGIT ["." DIGIT]) | ("." DIGIT)) [Exponent] 
 Exponent       = ( ("E"|"e") ("+"|"-") DIGIT ) 

 */

static char *
xps_parse_digits(char *s, int *digit)
{
    *digit = 0;
    while (*s >= '0' && *s <= '9')
    {
	*digit = *digit * 10 + (*s - '0');
	s ++;
    }
    return s;
}

static int is_real_num_char(int c)
{
    return (c >= '0' && c <= '9') || c == 'e' || c == 'E' || c == '+' || c == '-' || c == '.';
}

static char *
xps_parse_real_num(char *s, float *number)
{
    char buf[64];
    char *p = buf;
    while (is_real_num_char(*s))
	*p++ = *s++;
    *p = 0;
    if (buf[0])
	*number = atof(buf);
    return s;
}

static char *
xps_parse_cluster_mapping(char *s, int *code_count, int *glyph_count)
{
    if (*s == '(')
	s = xps_parse_digits(s + 1, code_count);
    if (*s == ':')
	s = xps_parse_digits(s + 1, glyph_count);
    if (*s == ')')
	s ++;
    return s;
}

static char *
xps_parse_glyph_index(char *s, int *glyph_index)
{
    if (*s >= '0' && *s <= '9')
	s = xps_parse_digits(s, glyph_index);
    return s;
}

static char *
xps_parse_glyph_metrics(char *s, float *advance, float *uofs, float *vofs)
{
    if (*s == ',')
	s = xps_parse_real_num(s + 1, advance);
    if (*s == ',')
	s = xps_parse_real_num(s + 1, uofs);
    if (*s == ',')
	s = xps_parse_real_num(s + 1, vofs);
    return s;
}

int
xps_parse_glyphs_imp(xps_context_t *ctx, xps_font_t *font, float size,
	float originx, float originy, int is_sideways, int bidi_level,
	char *indices, char *unicode, int is_charpath)
{
    // parse unicode and indices strings and encode glyphs
    // and calculate metrics for positioning

    xps_glyph_metrics_t mtx;
    float x = originx;
    float y = originy;
    char *us = unicode;
    char *is = indices;
    int un;

    dprintf1("string (%s)\n", us);
    dprintf1("indices %.50s\n", is);

    if (!unicode)
	return gs_throw(-1, "no unicode in glyphs element");

    if (!unicode && !indices)
	return gs_throw(-1, "no text in glyphs element");

    if (us)
	un = strlen(us);

    while ((us && un > 0) || (is && *is))
    {
	int char_code = ' ';
	int glyph_index = -1;
	int code_count = 1;
	int glyph_count = 1;

	float advance;
	float u_offset = 0.0;
	float v_offset = 0.0;

	// TODO: code_count and glyph_count char skipping magic

	if (is && *is)
	{
	    is = xps_parse_cluster_mapping(is, &code_count, &glyph_count);
	    is = xps_parse_glyph_index(is, &glyph_index);
	}

	if (us && un > 0)
	{
	    int t = xps_utf8_to_ucs(&char_code, us, un);
	    if (t < 0)
		return gs_rethrow(-1, "error decoding UTF-8 string");
	    us += t; un -= t;
	}

	if (glyph_index == -1)
	    glyph_index = xps_encode_font_char(font, char_code);

	xps_measure_font_glyph(ctx, font, glyph_index, &mtx);
	if (is_sideways)
	    advance = mtx.vadv * 100.0;
	else if (bidi_level & 1)
	    advance = -mtx.hadv * 100.0;
	else
	    advance = mtx.hadv * 100.0;

	if (is && *is)
	{
	    is = xps_parse_glyph_metrics(is, &advance, &u_offset, &v_offset);
	    if (*is == ';')
		is ++;
	}

#if 0
	dprintf6("glyph mapping (%d:%d)%d,%g,%g,%g\n",
		code_count, glyph_count, glyph_index,
		advance, u_offset, v_offset);
#endif

	// TODO: is_sideways and bidi_level

	if (is_charpath)
	    xps_draw_font_glyph_to_path(ctx, font, glyph_index, x, y);
	else
	    xps_fill_font_glyph(ctx, font, glyph_index, x, y);

	if (is_sideways)
	    y += advance * 0.01 * size;
	else
	{
	    x += advance * 0.01 * size;
	}
    }
}

int
xps_parse_glyphs(xps_context_t *ctx, xps_resource_t *dict, xps_item_t *root)
{
    xps_item_t *node;

    char *bidi_level_att;
    char *caret_stops_att;
    char *fill_att;
    char *font_size_att;
    char *font_uri_att;
    char *origin_x_att;
    char *origin_y_att;
    char *is_sideways_att;
    char *indices_att;
    char *unicode_att;
    char *style_att;
    char *transform_att;
    char *clip_att;
    char *opacity_att;
    char *opacity_mask_att;

    xps_item_t *transform_tag = NULL;
    xps_item_t *clip_tag = NULL;
    xps_item_t *fill_tag = NULL;
    xps_item_t *opacity_mask_tag = NULL;

    xps_part_t *part;
    xps_font_t *font;

    char partname[1024];
    char *parttype;

    gs_matrix matrix;
    float font_size = 10.0;
    int is_sideways = 0;
    int bidi_level = 0;
    int saved = 0;

    /*
     * Extract attributes and extended attributes.
     */

    bidi_level_att = xps_att(root, "BidiLevel");
    caret_stops_att = xps_att(root, "CaretStops");
    fill_att = xps_att(root, "Fill");
    font_size_att = xps_att(root, "FontRenderingEmSize");
    font_uri_att = xps_att(root, "FontUri");
    origin_x_att = xps_att(root, "OriginX");
    origin_y_att = xps_att(root, "OriginY");
    is_sideways_att = xps_att(root, "IsSideways");
    indices_att = xps_att(root, "Indices");
    unicode_att = xps_att(root, "UnicodeString");
    style_att = xps_att(root, "StyleSimulations");
    transform_att = xps_att(root, "RenderTransform");
    clip_att = xps_att(root, "Clip");
    opacity_att = xps_att(root, "Opacity");
    opacity_mask_att = xps_att(root, "OpacityMask");

    for (node = xps_down(root); node; node = xps_next(node))
    {
	if (!strcmp(xps_tag(node), "Glyphs.RenderTransform"))
	    transform_tag = xps_down(node);

	if (!strcmp(xps_tag(node), "Glyphs.OpacityMask"))
	    opacity_mask_tag = xps_down(node);

	if (!strcmp(xps_tag(node), "Glyphs.Clip"))
	    clip_tag = xps_down(node);

	if (!strcmp(xps_tag(node), "Glyphs.Fill"))
	    fill_tag = xps_down(node);
    }

    xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag);
    xps_resolve_resource_reference(ctx, dict, &clip_att, &clip_tag);
    xps_resolve_resource_reference(ctx, dict, &fill_att, &fill_tag);
    xps_resolve_resource_reference(ctx, dict, &opacity_mask_att, &opacity_mask_tag);

    /*
     * Check that we have all the necessary information.
     */

    if (!font_size_att || !font_uri_att || !origin_x_att || !origin_y_att)
	return gs_throw(-1, "missing attributes in glyphs element");

    if (!indices_att && !unicode_att)
	return 0; /* nothing to draw */

    if (is_sideways_att)
	is_sideways = !strcmp(is_sideways_att, "true");

    if (bidi_level_att)
	bidi_level = atoi(bidi_level_att);

    /*
     * Find and load the font resource
     */

    // TODO: get subfont index from # part of uri

    xps_absolute_path(partname, ctx->pwd, font_uri_att);
    part = xps_find_part(ctx, partname);
    if (!part)
	return gs_throw1(-1, "cannot find font resource part '%s'", partname);

    if (!part->font)
    {
	/* deobfuscate if necessary */
	parttype = xps_get_content_type(ctx, part->name);
	if (parttype && !strcmp(parttype, "application/vnd.ms-package.obfuscated-opentype"))
	    xps_deobfuscate_font_resource(ctx, part);

	/* stupid XPS files with content-types after the parts */
	if (!parttype && strstr(part->name, ".odttf"))
	    xps_deobfuscate_font_resource(ctx, part);

	part->font = xps_new_font(ctx, part->data, part->size, 0);
	if (!part->font)
	    return gs_rethrow1(-1, "cannot load font resource '%s'", partname);

	xps_select_best_font_encoding(part->font);
    }

    font = part->font;

    /*
     * Set up graphics state.
     */

    if (transform_att || transform_tag)
    {
	gs_matrix transform;

	dputs("glyphs transform\n");

	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	if (transform_att)
	    xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
	    xps_parse_matrix_transform(ctx, transform_tag, &transform);

	gs_concat(ctx->pgs, &transform);
    }

    if (clip_att || clip_tag)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("glyphs clip\n");

	if (clip_att)
	    xps_parse_abbreviated_geometry(ctx, clip_att);
	if (clip_tag)
	    xps_parse_path_geometry(ctx, dict, clip_tag);

	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);
    }

    font_size = atof(font_size_att);

    gs_setfont(ctx->pgs, font->font);
    gs_make_scaling(font_size, -font_size, &matrix);
    gs_setcharmatrix(ctx->pgs, &matrix);

    gs_currentcharmatrix(ctx->pgs, &matrix, true);
    dprintf4("currentcharmatrix %g %g %g %g\n", matrix.xx, matrix.xy, matrix.yx, matrix.yy);

    /*
     * If it's a solid color brush fill/stroke do a simple fill
     */

    if (fill_tag && !strcmp(xps_tag(fill_tag), "SolidColorBrush"))
    {
	fill_att = xps_att(fill_tag, "Color");
	fill_tag = NULL;
    }

    if (fill_att)
    {
	float argb[4];
	dputs("glyphs solid color fill\n");
	xps_parse_color(ctx, fill_att, argb);
	if (argb[0] > 0.001)
	{
	    gs_setrgbcolor(ctx->pgs, argb[1], argb[2], argb[3]);
	    xps_parse_glyphs_imp(ctx, font, font_size,
		    atof(origin_x_att), atof(origin_y_att),
		    is_sideways, bidi_level,
		    indices_att, unicode_att, 0);
	}
    }

    /*
     * If it's a visual brush or image, use the charpath as a clip mask to paint brush
     */

    if (fill_tag)
    {
	if (!saved)
	{
	    gs_gsave(ctx->pgs);
	    saved = 1;
	}

	dputs("glyphs complex brush\n");

	xps_parse_glyphs_imp(ctx, font, font_size,
		atof(origin_x_att), atof(origin_y_att),
		is_sideways, bidi_level, indices_att, unicode_att, 1);

	gs_clip(ctx->pgs);
	gs_newpath(ctx->pgs);

	dputs("clip\n");

	if (!strcmp(xps_tag(fill_tag), "ImageBrush"))
	    xps_parse_image_brush(ctx, dict, fill_tag);
	if (!strcmp(xps_tag(fill_tag), "VisualBrush"))
	    xps_parse_visual_brush(ctx, dict, fill_tag);
	if (!strcmp(xps_tag(fill_tag), "LinearGradientBrush"))
	    xps_parse_linear_gradient_brush(ctx, dict, fill_tag);
	if (!strcmp(xps_tag(fill_tag), "RadialGradientBrush"))
	    xps_parse_radial_gradient_brush(ctx, dict, fill_tag);
    }

    /*
     * Remember to restore if we did a gsave
     */

    if (saved)
    {
	gs_grestore(ctx->pgs);
    }

    return 0;
}

