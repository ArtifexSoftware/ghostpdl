#include "ghostxps.h"

int xps_draw_font_glyph_to_path(xps_context_t *ctx, xps_font_t *font, int gid, float x, float y)
{
    return 0;
}

int xps_fill_font_glyph(xps_context_t *ctx, xps_font_t *font, int gid, float x, float y)
{
    gs_text_params_t params;
    gs_text_enum_t *textenum;
    gs_matrix matrix;
    int code;

    params.operation = TEXT_FROM_SINGLE_GLYPH | TEXT_DO_DRAW;
    params.data.d_glyph = gid;
    params.size = 1;

    matrix.xx = 12.0;
    matrix.xy = 0.0;
    matrix.yx = 0.0;
    matrix.yy = -12.0;
    matrix.tx = 0.0;
    matrix.ty = 0.0;

    // dprintf3("filling glyph '%c' at %g,%g\n", gid, x, y);

    gs_setfont(ctx->pgs, font->font);
    gs_setcharmatrix(ctx->pgs, &matrix);
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

