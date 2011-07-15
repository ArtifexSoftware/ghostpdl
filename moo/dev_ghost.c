#include "mooscript.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

typedef struct fz_ghost_device_s fz_ghost_device;

struct fz_ghost_device_s
{
	gs_memory_t *memory;
	gs_state *pgs;
	gs_font_dir *fontdir;
};

static gs_matrix
gs_matrix_from_fz_matrix(fz_matrix ctm)
{
	gs_matrix m;
	m.xx = ctm.a;
	m.xy = ctm.b;
	m.yx = ctm.c;
	m.yy = ctm.d;
	m.tx = ctm.e;
	m.ty = ctm.f;
	return m;
}

static void
fz_ghost_matrix(gs_state *pgs, fz_matrix ctm)
{
	gs_matrix m = gs_matrix_from_fz_matrix(ctm);
	gs_setmatrix(pgs, &m);
}

static void
fz_ghost_color(gs_state *pgs, fz_colorspace *colorspace, float *color, float alpha)
{
	gs_setopacityalpha(pgs, alpha);
	if (colorspace == fz_device_gray)
		gs_setgray(pgs, color[0]);
	else if (colorspace == fz_device_rgb)
		gs_setrgbcolor(pgs, color[0], color[1], color[2]);
	else if (colorspace == fz_device_cmyk)
		gs_setcmykcolor(pgs, color[0], color[1], color[2], color[3]);
	else {
		float rgb[3];
		fz_convert_color(colorspace, color, fz_device_rgb, rgb);
		gs_setrgbcolor(pgs, rgb[0], rgb[1], rgb[2]);
	}
}

static void
fz_ghost_path(gs_state *pgs, fz_path *path, int indent)
{
	float x, y, x2, y2, x3, y3;
	int i = 0;
	while (i < path->len)
	{
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			gs_moveto(pgs, x, y);
			break;
		case FZ_LINETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			gs_lineto(pgs, x, y);
			break;
		case FZ_CURVETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			x2 = path->items[i++].v;
			y2 = path->items[i++].v;
			x3 = path->items[i++].v;
			y3 = path->items[i++].v;
			gs_curveto(pgs, x, y, x2, y2, x3, y3);
			break;
		case FZ_CLOSE_PATH:
			gs_closepath(pgs);
			break;
		}
	}
}

static int move_to(const FT_Vector *p, void *pgs)
{
	gs_moveto(pgs, p->x / 64.0f, p->y / 64.0f);
	return 0;
}

static int line_to(const FT_Vector *p, void *pgs)
{
	gs_lineto(pgs, p->x / 64.0f, p->y / 64.0f);
	return 0;
}

static int conic_to(const FT_Vector *c, const FT_Vector *p, void *pgs)
{
	gs_point s, c1, c2;
	float cx = c->x / 64.0f, cy = c->y / 64.0f;
	float px = p->x / 64.0f, py = p->y / 64.0f;
	gs_currentpoint(pgs, &s);
	c1.x = (s.x + cx * 2) / 3;
	c1.y = (s.y + cy * 2) / 3;
	c2.x = (px + cx * 2) / 3;
	c2.y = (py + cy * 2) / 3;
	gs_curveto(pgs, c1.x, c1.y, c2.x, c2.y, px, py);
	return 0;
}

static int cubic_to(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *p, void *pgs)
{
	gs_curveto(pgs, c1->x/64.0f, c1->y/64.0f, c2->x/64.0f, c2->y/64.0f, p->x/64.0f, p->y/64.0f);
	return 0;
}

static const FT_Outline_Funcs outline_funcs = {
	move_to, line_to, conic_to, cubic_to, 0, 0
};

static int
build_char(gs_show_enum *penum, gs_state *pgs, gs_font *gsfont, gs_char cid, gs_glyph gid)
{
	fz_font *fzfont = gsfont->client_data;
	float w2[6] = { 0, 1, -1, -1, 2, 2 };
	gs_fixed_point saved_adjust;

	gs_setcachedevice(penum, pgs, w2);

	if (fzfont->ft_face) {	
		FT_Face face = fzfont->ft_face;
		FT_Set_Char_Size(face, 64, 64, 72, 72);
		FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
		gs_moveto(pgs, 0, 0);
		FT_Outline_Decompose(&face->glyph->outline, &outline_funcs, pgs);
		gs_closepath(pgs);
	} else {
		// TODO: type3 fonts
	}

	/* enable dropout prevention by fiddling with fill_adjust */
	saved_adjust = pgs->fill_adjust;
	pgs->fill_adjust.x = -1;
	pgs->fill_adjust.y = -1;
	gs_fill(pgs);
	pgs->fill_adjust = saved_adjust;

	return 0;
}

static gs_font *
make_gs_font(fz_font *fzfont, gs_memory_t *mem, gs_font_dir *dir)
{
	gs_font_base *gsfont = fz_malloc(sizeof *gsfont);

	gsfont->next = 0;
	gsfont->prev = 0;
	gsfont->memory = mem;
	gsfont->dir = dir;
	gsfont->is_resource = 0;
	gs_notify_init(&gsfont->notify_list, gs_memory_stable(mem));
	gsfont->id = gs_next_ids(mem, 1);
	gsfont->base = (gs_font*)gsfont;
	gsfont->client_data = fz_keep_font(fzfont);

	gs_make_identity(&gsfont->FontMatrix);
	gs_make_identity(&gsfont->orig_FontMatrix);

	gsfont->FontType = ft_user_defined;
	gsfont->BitmapWidths = 0;
	gsfont->ExactSize = fbit_use_outlines;
	gsfont->InBetweenSize = fbit_use_outlines;
	gsfont->TransformedChar = fbit_use_outlines;
	gsfont->WMode = 0;
	gsfont->PaintType = 0;
	gsfont->StrokeWidth = 0;
	gsfont->is_cached = 0;

	gsfont->font_name.size = 0;
	gsfont->key_name.size = 0;

	gsfont->procs.define_font = gs_no_define_font;
	gsfont->procs.make_font = gs_no_make_font;
	gsfont->procs.font_info = gs_default_font_info;
	gsfont->procs.same_font = gs_default_same_font;
	gsfont->procs.encode_char = gs_no_encode_char;
	gsfont->procs.decode_glyph = gs_no_decode_glyph;
	gsfont->procs.enumerate_glyph = gs_no_enumerate_glyph;
	gsfont->procs.glyph_info = gs_default_glyph_info;
	gsfont->procs.glyph_outline = gs_no_glyph_outline;
	gsfont->procs.glyph_name = gs_no_glyph_name;
	gsfont->procs.init_fstack = gs_default_init_fstack;
	gsfont->procs.next_char_glyph = gs_default_next_char_glyph;
	gsfont->procs.build_char = build_char;

	// gs_font is not safe, gxccache assumes all fonts derive from gs_font_base
	gsfont->FontBBox.p.x = 0;
	gsfont->FontBBox.p.y = 0;
	gsfont->FontBBox.q.x = 0;
	gsfont->FontBBox.q.y = 0;

	uid_set_UniqueID(&gsfont->UID, gsfont->id);

	gsfont->FAPI = 0;
	gsfont->FAPI_font_data = 0;
	gsfont->encoding_index = ENCODING_INDEX_UNKNOWN;
	gsfont->nearest_encoding_index = ENCODING_INDEX_ISOLATIN1;

	gs_definefont(dir, (gs_font*)gsfont);

	return (gs_font*)gsfont;
}

struct entry
{
	fz_font *fzfont;
	gs_font *gsfont;
	struct entry *next;
};

gs_font *
gs_font_from_fz_font(fz_font *fzfont, gs_memory_t *mem, gs_font_dir *dir)
{
	static struct entry *root = NULL;
	struct entry *entry;
	for (entry = root; entry; entry = entry->next)
		if (entry->fzfont == fzfont)
			return entry->gsfont;
	entry = fz_malloc(sizeof *entry);
	entry->fzfont = fzfont;
	entry->gsfont = make_gs_font(fzfont, mem, dir);
	entry->next = root;
	root = entry;
	return entry->gsfont;
}

static void
fz_ghost_fill_path(void *user, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_ghost_device *ghost = user;
	fz_ghost_color(ghost->pgs, colorspace, color, alpha);
	fz_ghost_matrix(ghost->pgs, ctm);
	fz_ghost_path(ghost->pgs, path, 0);
	if (even_odd)
		gs_eofill(ghost->pgs);
	else
		gs_fill(ghost->pgs);
}

static void
fz_ghost_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_ghost_device *ghost = user;

	gs_setlinewidth(ghost->pgs, stroke->linewidth);
	gs_setmiterlimit(ghost->pgs, stroke->miterlimit);
	gs_setlinestartcap(ghost->pgs, stroke->start_cap);
	gs_setlinedashcap(ghost->pgs, stroke->dash_cap);
	gs_setlineendcap(ghost->pgs, stroke->end_cap);
	gs_setlinejoin(ghost->pgs, stroke->linejoin);
	if (stroke->dash_len)
		gs_setdash(ghost->pgs, stroke->dash_list, stroke->dash_len, stroke->dash_phase);

	fz_ghost_color(ghost->pgs, colorspace, color, alpha);
	fz_ghost_matrix(ghost->pgs, ctm);
	fz_ghost_path(ghost->pgs, path, 0);

	gs_stroke(ghost->pgs);
}

static void
fz_ghost_clip_path(void *user, fz_path *path, int even_odd, fz_matrix ctm)
{
	fz_ghost_device *ghost = user;
	gs_gsave(ghost->pgs);
	fz_ghost_matrix(ghost->pgs, ctm);
	fz_ghost_path(ghost->pgs, path, 0);
	if (even_odd)
		gs_eoclip(ghost->pgs);
	else
		gs_clip(ghost->pgs);
}

static void
fz_ghost_clip_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_ghost_device *ghost = user;

	gs_gsave(ghost->pgs);

	gs_setlinewidth(ghost->pgs, stroke->linewidth);
	gs_setmiterlimit(ghost->pgs, stroke->miterlimit);
	gs_setlinestartcap(ghost->pgs, stroke->start_cap);
	gs_setlinedashcap(ghost->pgs, stroke->dash_cap);
	gs_setlineendcap(ghost->pgs, stroke->end_cap);
	gs_setlinejoin(ghost->pgs, stroke->linejoin);
	if (stroke->dash_len)
		gs_setdash(ghost->pgs, stroke->dash_list, stroke->dash_len, stroke->dash_phase);

	fz_ghost_matrix(ghost->pgs, ctm);
	fz_ghost_path(ghost->pgs, path, 0);

	gs_strokepath(ghost->pgs);
	gs_clip(ghost->pgs);
}

static void
fz_ghost_show_text(fz_ghost_device *ghost, fz_text *text, fz_matrix ctm, int operation)
{
	gs_font *font;
	gs_text_params_t params;
	gs_text_enum_t *textenum;
	gs_glyph *gbuf;
	gs_matrix matrix;
	float *xbuf, *ybuf;
	int i;

	fz_ghost_matrix(ghost->pgs, ctm);

	font = gs_font_from_fz_font(text->font, ghost->memory, ghost->fontdir);
	gs_setfont(ghost->pgs, font);

	matrix = gs_matrix_from_fz_matrix(text->trm);
	gs_setcharmatrix(ghost->pgs, &matrix);
	gs_matrix_multiply(&matrix, &font->orig_FontMatrix, &font->FontMatrix);

	xbuf = fz_calloc(text->len, sizeof *xbuf);
	ybuf = fz_calloc(text->len, sizeof *ybuf);
	gbuf = fz_calloc(text->len, sizeof *gbuf);

	for (i = 0; i < text->len - 1; i++) {
		gbuf[i] = text->items[i].gid;
		xbuf[i] = text->items[i+1].x - text->items[i].x;
		ybuf[i] = text->items[i+1].y - text->items[i].y;
	}
	gbuf[i] = text->items[i].gid;
	xbuf[i] = 0;
	ybuf[i] = 0;

	params.operation = TEXT_FROM_GLYPHS | TEXT_REPLACE_WIDTHS | operation;
	params.data.glyphs = gbuf;
	params.size = text->len;
	params.x_widths = xbuf;
	params.y_widths = ybuf;
	params.widths_size = text->len;

	gs_moveto(ghost->pgs, text->items[0].x, text->items[0].y);
	gs_text_begin(ghost->pgs, &params, ghost->memory, &textenum);
	gs_text_process(textenum);
	gs_text_release(textenum, "fz_ghost_fill_text");

	fz_free(gbuf);
	fz_free(ybuf);
	fz_free(xbuf);
}

static void
fz_ghost_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_ghost_device *ghost = user;
	fz_ghost_color(ghost->pgs, colorspace, color, alpha);
	fz_ghost_show_text(ghost, text, ctm, TEXT_DO_DRAW);
}

static void
fz_ghost_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_ghost_device *ghost = user;
	printf("<stroke_text font=\"%s\" wmode=\"%d\"\n", text->font->name, text->wmode);
	fz_ghost_color(ghost->pgs, colorspace, color, alpha);
	fz_ghost_show_text(ghost, text, ctm, TEXT_DO_DRAW);
}

static void
fz_ghost_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_ghost_device *ghost = user;
	gs_gsave(ghost->pgs);
	printf("<clip_text font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	printf("accumulate=\"%d\"\n", accumulate);
}

static void
fz_ghost_clip_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_ghost_device *ghost = user;
	gs_gsave(ghost->pgs);
	printf("<clip_stroke_text font=\"%s\" wmode=\"%d\"\n", text->font->name, text->wmode);
}

static void
fz_ghost_ignore_text(void *user, fz_text *text, fz_matrix ctm)
{
	printf("ignore_text font=\"%s\" wmode=\"%d\"\n", text->font->name, text->wmode);
}

static void
fz_ghost_fill_image(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	fz_ghost_device *ghost = user;
	fz_ghost_matrix(ghost->pgs, ctm);
}

static void
fz_ghost_fill_image_mask(void *user, fz_pixmap *image, fz_matrix ctm,
fz_colorspace *colorspace, float *color, float alpha)
{
	fz_ghost_device *ghost = user;
	fz_ghost_matrix(ghost->pgs, ctm);
	fz_ghost_color(ghost->pgs, colorspace, color, alpha);
}

static void
fz_ghost_clip_image_mask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_ghost_device *ghost = user;
	gs_gsave(ghost->pgs);
	fz_ghost_matrix(ghost->pgs, ctm);
}

static void
fz_ghost_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	fz_ghost_device *ghost = user;
	fz_ghost_matrix(ghost->pgs, ctm);
}

static void
fz_ghost_pop_clip(void *user)
{
	fz_ghost_device *ghost = user;
	gs_grestore(ghost->pgs);
}

static void
fz_ghost_begin_mask(void *user, fz_rect bbox, int luminosity, fz_colorspace *colorspace, float *color)
{
	printf("<mask bbox=\"%g %g %g %g\" s=\"%s\" ",
		bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		luminosity ? "luminosity" : "alpha");
	printf(">\n");
}

static void
fz_ghost_end_mask(void *user)
{
	printf("</mask>\n");
}

static void
fz_ghost_begin_group(void *user, fz_rect bbox, int isolated, int knockout, int blendmode, float alpha)
{
	printf("<group bbox=\"%g %g %g %g\" isolated=\"%d\" knockout=\"%d\" blendmode=\"%s\" alpha=\"%g\">\n",
		bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		isolated, knockout, fz_blendmode_name(blendmode), alpha);
}

static void
fz_ghost_end_group(void *user)
{
	printf("</group>\n");
}

static void
fz_ghost_begin_tile(void *user, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	fz_ghost_device *ghost = user;
	printf("<tile ");
	printf("area=\"%g %g %g %g\" ", area.x0, area.y0, area.x1, area.y1);
	printf("view=\"%g %g %g %g\" ", view.x0, view.y0, view.x1, view.y1);
	printf("xstep=\"%g\" ystep=\"%g\" ", xstep, ystep);
	fz_ghost_matrix(ghost->pgs, ctm);
	printf(">\n");
}

static void
fz_ghost_end_tile(void *user)
{
	printf("</tile>\n");
}

static void
fz_ghost_free_user(void *user)
{
	// free fontdir
}

fz_device *fz_new_ghost_device(gs_memory_t *memory, gs_state *pgs)
{
	fz_device *dev;

	fz_ghost_device *ghost = fz_malloc(sizeof *ghost);
	ghost->memory = memory;
	ghost->pgs = pgs;

	ghost->fontdir = gs_font_dir_alloc(memory);
	gs_setaligntopixels(ghost->fontdir, 1); /* no subpixels */
	gs_setgridfittt(ghost->fontdir, 1); /* see gx_ttf_outline in gxttfn.c for values */

	dev = fz_new_device(ghost);
	dev->free_user = fz_ghost_free_user;

	dev->fill_path = fz_ghost_fill_path;
	dev->stroke_path = fz_ghost_stroke_path;
	dev->clip_path = fz_ghost_clip_path;
	dev->clip_stroke_path = fz_ghost_clip_stroke_path;

	dev->fill_text = fz_ghost_fill_text;
	dev->stroke_text = fz_ghost_stroke_text;
	dev->clip_text = fz_ghost_clip_text;
	dev->clip_stroke_text = fz_ghost_clip_stroke_text;
	dev->ignore_text = fz_ghost_ignore_text;

	dev->fill_shade = fz_ghost_fill_shade;
	dev->fill_image = fz_ghost_fill_image;
	dev->fill_image_mask = fz_ghost_fill_image_mask;
	dev->clip_image_mask = fz_ghost_clip_image_mask;

	dev->pop_clip = fz_ghost_pop_clip;

	dev->begin_mask = fz_ghost_begin_mask;
	dev->end_mask = fz_ghost_end_mask;
	dev->begin_group = fz_ghost_begin_group;
	dev->end_group = fz_ghost_end_group;

	dev->begin_tile = fz_ghost_begin_tile;
	dev->end_tile = fz_ghost_end_tile;

	return dev;
}
