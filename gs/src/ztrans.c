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
#include "igstate.h"
#include "iname.h"
#include "store.h"
#include "gstrans.h"

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

private int
set_mask_value(i_ctx_t *i_ctx_p,
	       int (*set_mask)(P2(gs_state *, const gs_soft_mask_t *)))
{
    os_ptr op = osp;

    /****** NYI ******/
    pop(1);
    return 0;
}

private int
current_mask_value(i_ctx_t *i_ctx_p,
		   gs_soft_mask_t * (*current_mask)(P1(const gs_state *)))
{
    os_ptr op = osp;

    /****** NYI ******/
    push(1);
    make_null(op);
    return 0;
}

/* ------ Operators ------ */

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

/* <maskdict|null> .setopacitymask - */
private int
zsetopacitymask(i_ctx_t *i_ctx_p)
{
    return set_mask_value(i_ctx_p, gs_setopacitymask);
}

/* - .currentopacitymask <maskdict|null> */
private int
zcurrentopacitymask(i_ctx_t *i_ctx_p)
{
    return current_mask_value(i_ctx_p, gs_currentopacitymask);
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

/* <maskdict|null> .setshapemask - */
private int
zsetshapemask(i_ctx_t *i_ctx_p)
{
    return set_mask_value(i_ctx_p, gs_setshapemask);
}

/* - .currentshapemask <maskdict|null> */
private int
zcurrentshapemask(i_ctx_t *i_ctx_p)
{
    return current_mask_value(i_ctx_p, gs_currentshapemask);
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

/* <isolated> <knockout> <llx> <lly> <urx> <ury> .begintransparencygroup - */
private int
zbegintransparencygroup(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    double coords[4];
    gs_rect bbox;
    int code;

    check_type(op[-5], t_boolean);
    check_type(op[-4], t_boolean);
    code = num_params(op, 4, coords);
    if (code < 0)
	return code;
    bbox.p.x = coords[0], bbox.p.y = coords[1];
    bbox.q.x = coords[2], bbox.q.y = coords[3];
    code = gs_begintransparencygroup(igs, op[-5].value.intval,
				     op[-4].value.intval, &bbox);
    if (code >= 0)
	pop(6);
    return code;
}

/* - .discardtransparencygroup - */
private int
zdiscardtransparencygroup(i_ctx_t *i_ctx_p)
{
    return gs_discardtransparencygroup(igs);
}

/* - .endtransparencygroup - */
private int
zendtransparencygroup(i_ctx_t *i_ctx_p)
{
    return gs_endtransparencygroup(igs);
}

/* ------ Initialization procedure ------ */

const op_def ztrans_op_defs[] = {
    {"1.setblendmode", zsetblendmode},
    {"0.currentblendmode", zcurrentblendmode},
    {"1.setopacityalpha", zsetopacityalpha},
    {"0.currentopacityalpha", zcurrentopacityalpha},
    {"1.setopacitymask", zsetopacitymask},
    {"0.currentopacitymask", zcurrentopacitymask},
    {"1.setshapealpha", zsetshapealpha},
    {"0.currentshapealpha", zcurrentshapealpha},
    {"1.setshapemask", zsetshapemask},
    {"0.currentshapemask", zcurrentshapemask},
    {"1.settextknockout", zsettextknockout},
    {"0.currenttextknockout", zcurrenttextknockout},
    {"6.begintransparencygroup", zbegintransparencygroup},
    {"0.discardtransparencygroup", zdiscardtransparencygroup},
    {"0.endtransparencygroup", zendtransparencygroup},
    op_def_end(0)
};
