/* Copyright (C) 1992, 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.

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

/*$Id$ */
/* Level 2 character operators */
#include "ghost.h"
#include "oper.h"
#include "gsmatrix.h"		/* for gxfont.h */
#include "gstext.h"
#include "gxfixed.h"		/* for gxfont.h */
#include "gxfont.h"
#include "ialloc.h"
#include "ichar.h"
#include "igstate.h"
#include "iname.h"
#include "ibnum.h"

/* <charname> glyphshow - */
private int
zglyphshow(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_glyph glyph;
    gs_text_enum_t *penum;
    int code;

    switch (gs_currentfont(igs)->FontType) {
	case ft_CID_encrypted:
	case ft_CID_user_defined:
	case ft_CID_TrueType:
	case ft_CID_bitmap:
	    check_int_leu(*op, gs_max_glyph - gs_min_cid_glyph);
	    glyph = (gs_glyph) op->value.intval + gs_min_cid_glyph;
	    break;
	default:
	    check_type(*op, t_name);
	    glyph = name_index(op);
    }
    if ((code = op_show_enum_setup(i_ctx_p)) != 0 ||
	(code = gs_glyphshow_begin(igs, glyph, imemory, &penum)) < 0)
	return code;
    if ((code = op_show_finish_setup(i_ctx_p, penum, 1, NULL)) < 0) {
	ifree_object(penum, "op_show_glyph");
	return code;
    }
    return op_show_continue_pop(i_ctx_p, 1);
}

/* <string> <numarray|numstring> xshow - */
/* <string> <numarray|numstring> yshow - */
/* <string> <numarray|numstring> xyshow - */
private int
moveshow(i_ctx_t *i_ctx_p, bool have_x, bool have_y)
{
    os_ptr op = osp;
    gs_text_enum_t *penum;
    int code = op_show_setup(i_ctx_p, op - 1);
    int format;
    uint i, size;
    float *values;

    if (code != 0)
	return code;
    format = num_array_format(op);
    if (format < 0)
	return format;
    size = num_array_size(op, format);
    values = (float *)ialloc_byte_array(size, sizeof(float), "moveshow");
    if (values == 0)
	return_error(e_VMerror);
    for (i = 0; i < size; ++i) {
	ref value;

	switch (code = num_array_get(op, format, i, &value)) {
	case t_integer:
	    values[i] = value.value.intval; break;
	case t_real:
	    values[i] = value.value.realval; break;
	case t_null:
	    code = gs_note_error(e_rangecheck);
	    /* falls through */
	default:
	    ifree_object(values, "moveshow");
	    return code;
	}
    }
    if ((code = gs_xyshow_begin(igs, op[-1].value.bytes, r_size(op - 1),
				(have_x ? values : (float *)0),
				(have_y ? values : (float *)0),
				size, imemory, &penum)) < 0 ||
	(code = op_show_finish_setup(i_ctx_p, penum, 2, NULL)) < 0) {
	ifree_object(values, "moveshow");
	return code;
    }
    pop(2);
    return op_show_continue(i_ctx_p);
}
private int
zxshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, true, false);
}
private int
zyshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, false, true);
}
private int
zxyshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, true, true);
}

/* ------ Initialization procedure ------ */

const op_def zcharx_op_defs[] =
{
    op_def_begin_level2(),
    {"1glyphshow", zglyphshow},
    {"2xshow", zxshow},
    {"2xyshow", zxyshow},
    {"2yshow", zyshow},
    op_def_end(0)
};
