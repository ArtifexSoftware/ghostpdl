/* Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Operators for testing font copying facilities */
#include "ghost.h"
#include "gsccode.h"
#include "gxfont.h"		/* for ifont.h */
#include "gxfcopy.h"
#include "ialloc.h"
#include "ifont.h"
#include "igstate.h"		/* for igs */
#include "iname.h"
#include "oper.h"

/*
 * The code in this file is only used for testing the font copying API
 * (gxfcopy.[hc]).  It is not included in non-debugging builds.
 */

private int
glyph_param(i_ctx_t *i_ctx_p, os_ptr op, gs_glyph *pglyph)
{
    switch (r_type(op)) {
    case t_integer:
	*pglyph = (gs_glyph)op->value.intval + GS_MIN_CID_GLYPH;
	return 0;
    case t_name:
	*pglyph = name_index(op);
	return 0;
    default:
	return_error(e_typecheck);
    }
}

/* - .copyfont - */
private int
zcopyfont(i_ctx_t *i_ctx_p)
{
    gs_font *font = gs_currentfont(igs);
    gs_font *font_new;
    int code = gs_copy_font(font, gs_memory_stable(imemory), &font_new);

    if (code < 0)
	return code;
    return gs_setfont(igs, font_new);
}

/* <sourcefont> <glyph> .copyglyph - */
private int
zcopyglyph(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font *font;
    gs_glyph glyph;
    int code = font_param(op - 1, &font);

    if (code < 0 || (code = glyph_param(i_ctx_p, op, &glyph)) < 0 ||
	(code = gs_copy_glyph(font, glyph, gs_currentfont(igs))) < 0
	)
	return code;
    pop(2);
    return 0;
}

/* <char> <glyph> .copiedencode - */
private int
zcopiedencode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_glyph glyph;
    int code = glyph_param(i_ctx_p, op, &glyph);

    check_type(op[-1], t_integer);
    if (code < 0 || (code = gs_copied_font_add_encoding(gs_currentfont(igs),
							op[-1].value.intval,
							glyph)) < 0
	)
	return code;
    pop(2);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zfctest_op_defs[] =
{
    {"0.copyfont", zcopyfont},
    {"2.copyglyph", zcopyglyph},
    {"2.copiedencode", zcopiedencode},
    op_def_end(0)
};
