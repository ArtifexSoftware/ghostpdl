/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

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
/* Transparency operators */
#include "string_.h"
#include "ghost.h"
#include "oper.h"
#include "gsiparam.h"		/* for iimage2.h */
#include "gstrans.h"
#include "stream.h"		/* for files.h */
#include "files.h"
#include "igstate.h"
#include "iimage2.h"
#include "iname.h"
#include "store.h"

/* ------ Utilities ------ */

private int
set_float_value(i_ctx_t *i_ctx_p, int (*set_value)(P2(gs_state *, floatp)))
{
    os_ptr op = osp;
    double value;
    int code;

    if (real_param(op, &value) < 0)
	return_op_typecheck(op);
    if ((code = set_value(igs, value)) < 0)
	return code;
    pop(1);
    return 0;
}

private int
current_float_value(i_ctx_t *i_ctx_p,
		    float (*current_value)(P1(const gs_state *)))
{
    os_ptr op = osp;

    push(1);
    make_real(op, current_value(igs));
    return 0;
}

/* ------ Graphics state operators ------ */

private const char *const blend_mode_names[] = {
    GS_BLEND_MODE_NAMES, 0
};

/* <modename> .setblendmode - */
private int
zsetblendmode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref nsref;
    int code;
    const char *const *p;

    check_type(*op, t_name);
    name_string_ref(op, &nsref);
    for (p = blend_mode_names; *p; ++p)
	if (r_size(&nsref) == strlen(*p) &&
	    !memcmp(*p, nsref.value.const_bytes, r_size(&nsref))
	    ) {
	    code = gs_setblendmode(igs, p - blend_mode_names);
	    if (code < 0)
		return code;
	    pop(1);
	    return 0;
	}
    return_error(e_rangecheck);
}

/* - .currentblendmode <modename> */
private int
zcurrentblendmode(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const char *mode_name = blend_mode_names[gs_currentblendmode(igs)];
    ref nref;
    int code = name_enter_string(mode_name, &nref);

    if (code < 0)
	return code;
    push(1);
    *op = nref;
    return 0;
}

/* <0..1> .setopacityalpha - */
private int
zsetopacityalpha(i_ctx_t *i_ctx_p)
{
    return set_float_value(i_ctx_p, gs_setopacityalpha);
}

/* - .currentopacityalpha <0..1> */
private int
zcurrentopacityalpha(i_ctx_t *i_ctx_p)
{
    return current_float_value(i_ctx_p, gs_currentopacityalpha);
}

/* <0..1> .setshapealpha - */
private int
zsetshapealpha(i_ctx_t *i_ctx_p)
{
    return set_float_value(i_ctx_p, gs_setshapealpha);
}

/* - .currentshapealpha <0..1> */
private int
zcurrentshapealpha(i_ctx_t *i_ctx_p)
{
    return current_float_value(i_ctx_p, gs_currentshapealpha);
}

/* <bool> .settextknockout - */
private int
zsettextknockout(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_type(*op, t_boolean);
    gs_settextknockout(igs, op->value.boolval);
    pop(1);
    return 0;
}

/* - .currenttextknockout <bool> */
private int
zcurrenttextknockout(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_bool(op, gs_currenttextknockout(igs));
    return 0;
}

/* ------ Rendering stack operators ------ */

private int
rect_param(gs_rect *prect, os_ptr op)
{
    double coords[4];
    int code = num_params(op, 4, coords);

    if (code < 0)
	return code;
    prect->p.x = coords[0], prect->p.y = coords[1];
    prect->q.x = coords[2], prect->q.y = coords[3];
    return 0;
}

private int
mask_op(i_ctx_t *i_ctx_p,
	int (*mask_proc)(P2(gs_state *, gs_transparency_channel_selector_t)))
{
    int csel;
    int code = int_param(osp, 1, &csel);

    if (code < 0)
	return code;
    code = mask_proc(igs, csel);
    if (code >= 0)
	pop(1);
    return code;

}

/* <paramdict> <llx> <lly> <urx> <ury> .begintransparencygroup - */
private int
zbegintransparencygroup(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_transparency_group_params_t params;
    gs_rect bbox;
    int code;

    /****** NYI ******/
    code = rect_param(&bbox, op);
    if (code < 0)
	return code;
    code = gs_begin_transparency_group(igs, &params, &bbox);
    if (code < 0)
	return code;
    pop(5);
    return code;
}

/* - .discardtransparencygroup - */
private int
zdiscardtransparencygroup(i_ctx_t *i_ctx_p)
{
    if (gs_current_transparency_type(igs) != TRANSPARENCY_STATE_Group)
	return_error(e_rangecheck);
    return gs_discard_transparency_level(igs);
}

/* - .endtransparencygroup - */
private int
zendtransparencygroup(i_ctx_t *i_ctx_p)
{
    return gs_end_transparency_group(igs);
}

/* <paramdict> <llx> <lly> <urx> <ury> .begintransparencymask - */
private int
zbegintransparencymask(i_ctx_t *i_ctx_p)
{
    gs_transparency_mask_params_t params;
    gs_rect bbox;
    int code;

    /****** NYI ******/
    code = gs_begin_transparency_mask(igs, &params, &bbox);
    if (code < 0)
	return code;
    pop(1);
    return code;
}

/* - .discardtransparencymask - */
private int
zdiscardtransparencymask(i_ctx_t *i_ctx_p)
{
    if (gs_current_transparency_type(igs) != TRANSPARENCY_STATE_Mask)
	return_error(e_rangecheck);
    return gs_discard_transparency_level(igs);
}

/* <mask#> .endtransparencymask - */
private int
zendtransparencymask(i_ctx_t *i_ctx_p)
{
    return mask_op(i_ctx_p, gs_end_transparency_mask);
}

/* <mask#> .inittransparencymask - */
private int
zinittransparencymask(i_ctx_t *i_ctx_p)
{
    return mask_op(i_ctx_p, gs_init_transparency_mask);
}

/* ------ Initialization procedure ------ */

const op_def ztrans_op_defs[] = {
    {"1.setblendmode", zsetblendmode},
    {"0.currentblendmode", zcurrentblendmode},
    {"1.setopacityalpha", zsetopacityalpha},
    {"0.currentopacityalpha", zcurrentopacityalpha},
    {"1.setshapealpha", zsetshapealpha},
    {"0.currentshapealpha", zcurrentshapealpha},
    {"1.settextknockout", zsettextknockout},
    {"0.currenttextknockout", zcurrenttextknockout},
    {"5.begintransparencygroup", zbegintransparencygroup},
    {"0.discardtransparencygroup", zdiscardtransparencygroup},
    {"0.endtransparencygroup", zendtransparencygroup},
    {"5.begintransparencymask", zbegintransparencymask},
    {"0.discardtransparencymask", zdiscardtransparencymask},
    {"1.endtransparencymask", zendtransparencymask},
    {"1.inittransparencymask", zinittransparencymask},
    op_def_end(0)
};
