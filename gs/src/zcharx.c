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


/* Level 2 character operators */
#include "ghost.h"
#include "oper.h"
#include "gschar.h"
#include "gsmatrix.h"		/* for gxfont.h */
#include "gsstruct.h"		/* for st_stream */
#include "gxfixed.h"		/* for gxfont.h */
#include "gxfont.h"
#include "gxchar.h"
#include "estack.h"
#include "ialloc.h"
#include "ichar.h"
#include "ifont.h"
#include "igstate.h"
#include "iname.h"
#include "store.h"
#include "stream.h"
#include "ibnum.h"
#include "gspath.h"		/* gs_rmoveto prototype */

/* Table of continuation procedures. */
private int xshow_continue(P1(i_ctx_t *));
private int yshow_continue(P1(i_ctx_t *));
private int xyshow_continue(P1(i_ctx_t *));
static const op_proc_t xyshow_continues[4] = {
    0, xshow_continue, yshow_continue, xyshow_continue
};

/* Forward references */
private int moveshow(P2(i_ctx_t *, int));
private int moveshow_continue(P2(i_ctx_t *, int));

/* <charname> glyphshow - */
private int
zglyphshow(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_glyph glyph;
    gs_show_enum *penum;
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
    if ((code = op_show_enum_setup(i_ctx_p, &penum)) != 0)
	return code;
    if ((code = gs_glyphshow_init(penum, igs, glyph)) < 0) {
	ifree_object(penum, "op_show_glyph");
	return code;
    }
    op_show_finish_setup(i_ctx_p, penum, 1, NULL);
    return op_show_continue_pop(i_ctx_p, 1);
}

/* <string> <numarray|numstring> xshow - */
private int
zxshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, 1);
}

/* <string> <numarray|numstring> yshow - */
private int
zyshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, 2);
}

/* <string> <numarray|numstring> xyshow - */
private int
zxyshow(i_ctx_t *i_ctx_p)
{
    return moveshow(i_ctx_p, 3);
}

/* Common code for {x,y,xy}show */
private int
moveshow(i_ctx_t *i_ctx_p, int xymask)
{
    os_ptr op = osp;
    gs_show_enum *penum;
    int code = op_show_setup(i_ctx_p, op - 1, &penum);

    if (code != 0)
	return code;
    if ((code = gs_xyshow_n_init(penum, igs, (char *)op[-1].value.bytes, r_size(op - 1)) < 0)) {
	ifree_object(penum, "op_show_enum_setup");
	return code;
    }
    code = num_array_format(op);
    if (code < 0) {
	ifree_object(penum, "op_show_enum_setup");
	return code;
    }
    op_show_finish_setup(i_ctx_p, penum, 2, NULL);
    ref_assign(&sslot, op);
    pop(2);
    return moveshow_continue(i_ctx_p, xymask);
}

/* Continuation procedures */

private int
xshow_continue(i_ctx_t *i_ctx_p)
{
    return moveshow_continue(i_ctx_p, 1);
}

private int
yshow_continue(i_ctx_t *i_ctx_p)
{
    return moveshow_continue(i_ctx_p, 2);
}

private int
xyshow_continue(i_ctx_t *i_ctx_p)
{
    return moveshow_continue(i_ctx_p, 3);
}

/* Get one value from the encoded number string or array. */
/* Sets pvalue->value.realval. */
private int
sget_real(const ref * nsp, int format, uint index, ref * pvalue)
{
    int code;

    switch (code = num_array_get(nsp, format, index, pvalue)) {
	case t_integer:
	    pvalue->value.realval = pvalue->value.intval;
	case t_real:
	    return t_real;
	case t_null:
	    code = gs_note_error(e_rangecheck);
	default:
	    return code;
    }
}

private int
moveshow_continue(i_ctx_t *i_ctx_p, int xymask)
{
    const ref *nsp = &sslot;
    int format = num_array_format(nsp);
    int code;
    gs_show_enum *penum = senum;
    uint index = ssindex.value.intval;

    for (;;) {
	ref rwx, rwy;

	code = gs_show_next(penum);
	if (code != gs_show_move) {
	    ssindex.value.intval = index;
	    code = op_show_continue_dispatch(i_ctx_p, 0, code);
	    if (code == o_push_estack) {	/* must be gs_show_render */
		make_op_estack(esp - 1, xyshow_continues[xymask]);
	    }
	    return code;
	}
	/* Move according to the next value(s) from the stream. */
	if (xymask & 1) {
	    code = sget_real(nsp, format, index++, &rwx);
	    if (code < 0)
		break;
	} else
	    rwx.value.realval = 0;
	if (xymask & 2) {
	    code = sget_real(nsp, format, index++, &rwy);
	    if (code < 0)
		break;
	} else
	    rwy.value.realval = 0;
	code = gs_rmoveto(igs, rwx.value.realval, rwy.value.realval);
	if (code < 0)
	    break;
    }
    /* An error occurred.  Clean up before returning. */
    return op_show_free(i_ctx_p, code);
}

/* ------ Initialization procedure ------ */

const op_def zcharx_op_defs[] =
{
    op_def_begin_level2(),
    {"1glyphshow", zglyphshow},
    {"2xshow", zxshow},
    {"2xyshow", zxyshow},
    {"2yshow", zyshow},
		/* Internal operators */
    {"0%xshow_continue", xshow_continue},
    {"0%yshow_continue", yshow_continue},
    {"0%xyshow_continue", xyshow_continue},
    op_def_end(0)
};
