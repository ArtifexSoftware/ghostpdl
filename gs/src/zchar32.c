/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*Id: zchar32.c  */
/* Type 32 font glyph operators */
#include "ghost.h"
#include "oper.h"
#include "gsccode.h"		/* for gxfont.h */
#include "gscoord.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxfcache.h"
#include "ifont.h"
#include "igstate.h"

/* ([wx wy llx lly urx ury] | [w0x w0y llx lly urx ury w1x w1y vx vy]) */
/*   <bitmap> <cid> <type32font> addglyph - */
private floatp
coeff_sign(floatp v)
{
    return (v < 0 ? -1.0 : v > 0 ? 1.0 : 0.0);
}
private int
zaddglyph(os_ptr op)
{
    int num_wmodes = 1;
    int int_mask = 0x3c;	/* bits for llx .. ury */
    uint msize;
    double metrics[10];
    int llx, lly, urx, ury;
    int width, height, raster;
    gs_font *pfont;
    gs_matrix save_mat;
    cached_fm_pair *pair;
    int wmode;
    int code;

    check_array(op[-3]);
    msize = r_size(op - 3);
    switch (msize) {
	case 10:
	    num_wmodes = 2;
	    int_mask = 0x33c;	/* add bits for vx, vy */
	case 6:
	    break;
	default:
	    return_error(e_rangecheck);
    }
    code = num_params(op[-3].value.refs + msize - 1, msize, metrics);
    if (code < 0)
	return code;
    if (~code & int_mask)	/* check llx .. ury for integers */
	return_error(e_typecheck);
    check_read_type(op[-2], t_string);
    llx = (int)metrics[2];
    lly = (int)metrics[3];
    urx = (int)metrics[4];
    ury = (int)metrics[5];
    width = urx - llx;
    height = ury - lly;
    raster = (width + 7) >> 3;
    if (width < 0 || height < 0 || r_size(op - 2) != raster * height)
	return_error(e_rangecheck);
    check_int_leu(op[-1], 65535);
    code = font_param(op, &pfont);
    if (code < 0)
	return code;
    if (pfont->FontType != ft_bitmap)
	return_error(e_invalidfont);
    /* Set the CTM to the (properly oriented) identity. */
    gs_currentmatrix(igs, &save_mat);
    {
	gs_matrix init_mat, font_mat;

	gs_defaultmatrix(igs, &init_mat);
	font_mat.xx = coeff_sign(init_mat.xx);
	font_mat.xy = coeff_sign(init_mat.xy);
	font_mat.yx = coeff_sign(init_mat.yx);
	font_mat.yy = coeff_sign(init_mat.yy);
	font_mat.tx = font_mat.ty = 0;
	gs_setmatrix(igs, &font_mat);
    }
    pair = gx_lookup_fm_pair(pfont, igs);
    if (pair == 0) {
	gs_setmatrix(igs, &save_mat);
	return_error(e_VMerror);
    }
    /* The APIs for adding characters to the cache are not ideal.... */
    for (wmode = 0; wmode < num_wmodes; ++wmode) {
	gx_device_memory mdev;
	static const gs_log2_scale_point no_scale =
	{0, 0};
	cached_char *cc;
	gx_bitmap_id id = gs_next_ids(1);

	mdev.memory = 0;
	mdev.target = 0;
	cc = gx_alloc_char_bits(pfont->dir, &mdev, NULL, width, height,
				&no_scale, 1);
	if (cc == 0) {
	    code = gs_note_error(e_limitcheck);
	    break;
	}
	cc->wxy.x = float2fixed(metrics[0]);
	cc->wxy.y = float2fixed(metrics[1]);
	cc->offset.x = -llx;
	cc->offset.y = -lly;
	gx_copy_mono_unaligned((gx_device *) & mdev,
			       op[-2].value.const_bytes, 0, raster, id,
			       0, 0, width, height,
			       (gx_color_index) 0, (gx_color_index) 1);
	gx_add_cached_char(pfont->dir, &mdev, cc, pair, &no_scale);
	cc->code = gs_min_cid_glyph + op[-1].value.intval;
	cc->wmode = wmode;
	gx_add_char_bits(pfont->dir, cc, &no_scale);
	/* Adjust for WMode = 1. */
	if (num_wmodes > wmode + 1) {
	    metrics[0] = metrics[6];
	    metrics[1] = metrics[7];
	    llx -= (int)metrics[8];
	    lly -= (int)metrics[9];
	}
    }
    gs_setmatrix(igs, &save_mat);
    if (code >= 0)
	pop(4);
    return code;
}

/* <cid_min> <cid_max> <type32font> removeglyphs - */
typedef struct {
    gs_glyph cid_min, cid_max;
    gs_font *font;
} font_cid_range_t;
private bool
select_cid_range(cached_char * cc, void *range_ptr)
{
    const font_cid_range_t *range = range_ptr;

    return (cc->code >= range->cid_min &&
	    cc->code <= range->cid_max &&
	    cc->pair->font == range->font);
}
private int
zremoveglyphs(os_ptr op)
{
    int code;
    font_cid_range_t range;

    check_int_leu(op[-2], 65535);
    check_int_leu(op[-1], 65535);
    code = font_param(op, &range.font);
    if (code < 0)
	return code;
    if (range.font->FontType != ft_bitmap)
	return_error(e_invalidfont);
    range.cid_min = gs_min_cid_glyph + op[-2].value.intval;
    range.cid_max = gs_min_cid_glyph + op[-1].value.intval;
    gx_purge_selected_cached_chars(range.font->dir, select_cid_range,
				   &range);
    pop(3);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zchar32_op_defs[] =
{
    {"4addglyph", zaddglyph},
    {"3removeglyphs", zremoveglyphs},
    op_def_end(0)
};
