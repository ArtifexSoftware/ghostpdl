/* Copyright (C) 1992, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* Level 2 color operators */
#include "ghost.h"
#include "oper.h"
#include "gscolor.h"
#include "gsmatrix.h"
#include "gsstruct.h"
#include "gxcspace.h"
#include "gxfixed.h"		/* for gxcolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"		/* for gxpcolor.h */
#include "gxdevice.h"
#include "gxdevmem.h"		/* for gxpcolor.h */
#include "gxpcolor.h"
#include "estack.h"
#include "ialloc.h"
#include "istruct.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "store.h"

/* Declare a local alias for st_pattern_instance. */
/* We do this so we can have configurations with */
/* the base Level 2 color machinery but without patterns. */
gs_memory_type_ptr_t zcolor2_st_pattern_instance_p = 0;

/* Forward references */
private int store_color_params(P3(os_ptr, const gs_paint_color *,
				  const gs_color_space *));
private int load_color_params(P3(os_ptr, gs_paint_color *,
				 const gs_color_space *));

/* - currentcolor <param1> ... <paramN> */
private int
zcurrentcolor(register os_ptr op)
{
    const gs_client_color *pc = gs_currentcolor(igs);
    const gs_color_space *pcs = gs_currentcolorspace(igs);
    int n;

    check_ostack(5);		/* Worst case: CMYK + pattern */
    if (pcs->type->index == gs_color_space_index_Pattern) {
	n = 1;
	if (pc->pattern != 0 && pc->pattern->template.PaintType == 2)	/* uncolored */
	    n += store_color_params(op, &pc->paint,
		   (const gs_color_space *)&pcs->params.pattern.base_space);
	op[n] = istate->pattern;
    } else
	n = store_color_params(op, &pc->paint, pcs);
    push(n);
    return 0;
}

/* - .currentcolorspace <array|int> */
private int
zcurrentcolorspace(register os_ptr op)
{
    push(1);
    if (r_has_type(&istate->colorspace.array, t_null)) {
	/*
	 * Return the color space index.  This is only possible
	 * for the parameterless color spaces.
	 */
	make_int(op, (int)(gs_currentcolorspace(igs)->type->index));
    } else
	*op = istate->colorspace.array;
    return 0;
}

/* <param1> ... <paramN> setcolor - */
private int
zsetcolor(register os_ptr op)
{
    gs_client_color c;
    const gs_color_space *pcs = gs_currentcolorspace(igs);
    int n, code;
    gs_pattern_instance *pinst = 0;

    if (pcs->type->index == gs_color_space_index_Pattern) {
	/* Make sure *op is a real Pattern. */
	ref *pImpl;

	check_type(*op, t_dictionary);
	check_dict_read(*op);
	if (dict_find_string(op, "Implementation", &pImpl) <= 0 ||
	    !r_has_stype(pImpl, imemory, *zcolor2_st_pattern_instance_p)
	    )
	    return_error(e_rangecheck);
	pinst = r_ptr(pImpl, gs_pattern_instance);
	c.pattern = pinst;
	if (pinst->template.PaintType == 2) {	/* uncolored */
	    if (!pcs->params.pattern.has_base_space)
		return_error(e_rangecheck);
	    n = load_color_params(op - 1, &c.paint,
		   (const gs_color_space *)&pcs->params.pattern.base_space);
	    if (n < 0)
		return n;
	    n++;
	} else
	    n = 1;
    } else {
	n = load_color_params(op, &c.paint, pcs);
	c.pattern = 0;		/* for GC */
    }
    if (n < 0)
	return n;
    code = gs_setcolor(igs, &c);
    if (code < 0)
	return code;
    if (pinst != 0)
	istate->pattern = *op;
    pop(n);
    return code;
}

/* <array> .setcolorspace - */
private int
zsetcolorspace(register os_ptr op)
{
    check_type(*op, t_array);
    istate->colorspace.array = *op;
    pop(1);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zcolor2_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"0currentcolor", zcurrentcolor},
    {"0.currentcolorspace", zcurrentcolorspace},
    {"1setcolor", zsetcolor},
    {"1.setcolorspace", zsetcolorspace},
    op_def_end(0)
};

/* ------ Internal procedures ------ */

/* Store non-pattern color values on the operand stack. */
/* Return the number of values stored. */
private int
store_color_params(os_ptr op, const gs_paint_color * pc,
		   const gs_color_space * pcs)
{
    int n = cs_num_components(pcs);

    if (pcs->type->index == gs_color_space_index_Indexed)
	make_int(op + 1, (int)pc->values[0]);
    else
	make_floats(op + 1, pc->values, n);
    return n;
}

/* Load non-pattern color values from the operand stack. */
/* Return the number of values stored. */
private int
load_color_params(os_ptr op, gs_paint_color * pc, const gs_color_space * pcs)
{
    int n = cs_num_components(pcs);
    int code = float_params(op, n, pc->values);

    if (code < 0)
	return code;
    return n;
}
