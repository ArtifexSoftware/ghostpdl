/* Copyright (C) 1989, 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* Color operators */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "estack.h"
#include "ialloc.h"
#include "igstate.h"
#include "iutil.h"
#include "store.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gxdcolor.h"		/* for gxpcolor.h */
#include "gxdevice.h"
#include "gxdevmem.h"		/* for gxpcolor.h */
#include "gxcmap.h"
#include "gxcspace.h"
#include "gxcolor2.h"
#include "gxpcolor.h"
#include "idict.h"
#include "icolor.h"
#include "idparam.h"
#include "iname.h"

/* imported from gsht.c */
extern  void    gx_set_effective_transfer(gs_state *);

/* define the number of stack slots needed for zcolor_remap_one */
const int   zcolor_remap_one_ostack = 4;
const int   zcolor_remap_one_estack = 3;


/* utility to test whether a Pattern instance uses a base space */
private inline bool
pattern_instance_uses_base_space(const gs_pattern_instance_t * pinst)
{
    return pinst->type->procs.uses_base_space(
                   pinst->type->procs.get_pattern(pinst) );
}

/*
 *  -   currentcolor   <param1>  ...  <paramN>
 *
 * Return the current color. <paramN> may be a dictionary or a null
 * object, if the current color space is a pattern color space. The
 * other parameters will be numeric.
 *
 * Note that the results of this operator differ slightly from those of
 * most currentcolor implementations. If a color component value is 
 * integral (e.g.: 0, 1), it will be pushed on the stack as an integer.
 * Most currentcolor implementations, including the earlier
 * implementation in Ghostscript, would push real objects for all
 * color spaces except indexed color space. The approach taken here is
 * equally legitimate, and avoids special handling of indexed color
 * spaces.
 */
private int
zcurrentcolor(i_ctx_t * i_ctx_p)
{
    os_ptr                  op = osp;
    const gs_color_space *  pcs = gs_currentcolorspace(igs);
    const gs_client_color * pcc = gs_currentcolor(igs);
    int                     i, n = cs_num_components(pcs);
    bool                    push_pattern = n < 0;

    /* check for pattern */
    if (push_pattern) {
        gs_pattern_instance_t * pinst = pcc->pattern;

        if (pinst == 0 || !pattern_instance_uses_base_space(pinst))
            n = 1;
        else
            n = -n;
    }

    /* check for sufficient space on the stack */
    push(n);
    op -= n - 1;

    /* push the numeric operands, if any */
    if (push_pattern)
        --n;
    for (i = 0; i < n; i++, op++) {
        float   rval = pcc->paint.values[i];
        int     ival = (int)rval;

        /* the following handles indexed color spaces */
        if (rval == ival)
            make_int(op, ival);
        else
            make_real(op, rval);
    }

    /* push the pattern dictionary or null object, if appropriate */
    if (push_pattern)
        *op = istate->pattern;

    return 0;
}

/*
 *  -   .currentcolorspace   <array>
 *
 * Return the current color space. Unlike the prior implementation, the
 * istate->color_space.array field will now always have a legitimate
 * (array) value.
 */
private int
zcurrentcolorspace(i_ctx_t * i_ctx_p)
{
    os_ptr  op = osp;   /* required by "push" macro */

    push(1);
    if ( gs_color_space_get_index(igs->color_space) == gs_color_space_index_DeviceGray ) {
        ref gray, graystr;
        ref csa = istate->colorspace.array; 
        if (array_get(&csa, 0, &gray) >= 0 && 
            r_has_type(&gray, t_name) && 
	    (name_string_ref(&gray, &graystr),
	    r_size(&graystr) == 10 &&
	    !memcmp(graystr.value.bytes, "DeviceGray", 10))) {
            
            *op = istate->colorspace.array;
        } else {
	    int code = ialloc_ref_array(op, a_all, 1, "currentcolorspace");
	    if (code < 0)
	        return code;
	    return name_enter_string("DeviceGray", op->value.refs);
        }
    } else
        *op = istate->colorspace.array;
    return 0;
}

/*
 *  -   .getuseciecolor   <bool>
 *
 * Return the current setting of the use_cie_color graphic state parameter,
 * which tracks the UseCIEColor page device parameter. This parameter may be
 * read (via this operator) at all language leves, but may only be set (via
 * the .setuseciecolor operator; see zcolor3.c) only in language level 3.
 *
 * We handle this parameter separately from the page device primarily for
 * performance reasons (the parameter may be queried frequently), but as a
 * side effect achieve proper behavior relative to the language level. The
 * interpreter is always initialized with this parameter set to false, and
 * it can only be updated (via setpagedevice) in language level 3.
 */
private int
zgetuseciecolor(i_ctx_t * i_ctx_p)
{
    os_ptr  op = osp;

    push(1);
    *op = istate->use_cie_color;
    return 0;
}

/*
 *  <param1>  ...  <paramN>   setcolor   -
 *
 * Set the current color. All of the parameters except the topmost (paramN) are
 * numbers; the topmost (and possibly only) entry may be pattern dictionary or
 * a null object.
 *
 * The use of one operator to set both patterns and "normal" colors is
 * consistent with Adobe's documentation, but primarily reflects the use of
 * gs_setcolor for both purposes in the graphic library. An alternate
 * implementation would use a .setpattern operator, which would interface with
 * gs_setpattern.
 *
 * This operator is hidden by a pseudo-operator of the same name, so it will
 * only be invoked under controlled situations. Hence, it does no operand
 * checking.
 */
private int
zsetcolor(i_ctx_t * i_ctx_p)
{
    os_ptr                  op = osp;
    const gs_color_space *  pcs = gs_currentcolorspace(igs);
    gs_client_color         cc;
    int                     n_comps, n_numeric_comps, num_offset = 0, code;
    bool                    is_ptype2 = 0;

    /* initialize the client color pattern pointer for GC */
    cc.pattern = 0;

    /* check for a pattern color space */
    if ((n_comps = cs_num_components(pcs)) < 0) {
        n_comps = -n_comps;
        if (r_has_type(op, t_dictionary)) {
            ref *   pImpl;
            int     ptype;

            dict_find_string(op, "Implementation", &pImpl);
            cc.pattern = r_ptr(pImpl, gs_pattern_instance_t);
            n_numeric_comps = ( pattern_instance_uses_base_space(cc.pattern)
                                  ? n_comps - 1
                                  : 0 );
            (void)dict_int_param(op, "PatternType", 1, 2, 1, &ptype);
            is_ptype2 = ptype == 2;
        } else
            n_numeric_comps = 0;
        num_offset = 1;
    } else
        n_numeric_comps = n_comps;

    /* gather the numeric operands */
    float_params(op - num_offset, n_numeric_comps, cc.paint.values);
 
    /* pass the color to the graphic library */
    if ((code = gs_setcolor(igs, &cc)) >= 0) {

        if (n_comps > n_numeric_comps) {
            istate->pattern = *op;      /* save pattern dict or null */
            n_comps = n_numeric_comps + 1;
        }
        pop(n_comps);
    }

    return code;
}

/*
 *  <array>   setcolorspace   -
 *
 * Set the nominal color space. This color space will be pushd by the
 * currentcolorspace operator, but is not directly used to pass color
 * space information to the graphic library.
 *
 * This operator can only be called from within the setcolorspace
 * pseudo-operator; the definition of the latter will override this
 * definition. Because error cheching is performed by the pseudo-
 * operator, it need not be repeated here.
 */
private int
zsetcolorspace(i_ctx_t * i_ctx_p)
{
    os_ptr  op = osp;

    istate->colorspace.array = *op;
    pop(1);
    return 0;
}

/*
 *  - .includecolorspace -
 *
 * See the comment for gs_includecolorspace in gscolor2.c .
 */
private int
zincludecolorspace(i_ctx_t * i_ctx_p)
{
    return gs_includecolorspace(igs);
}


/*
 *  <int>   .setdevcspace   -
 *
 * Set a parameterless color space. This is now used to set the
 * DeviceGray, DeviceRGB, and DeviceCMYK color spaces, rather than
 * the setgray/setrgbcolor/setcmykcolor operators. All PostScript-based
 * color space substitution will have been accomplished before this
 * operator is called.
 *
 * The use of an integer to indicate the specific color space is 
 * historical and on the whole not particularly desirable, as it ties
 * the PostScript code to a specific enumeration. This may be modified
 * in the future.
 *
 * As with setcolorspace, this operator is called only under controlled
 * circumstances, hence it does no operand error checking.
 */
private int
zsetdevcspace(i_ctx_t * i_ctx_p)
{

    gs_color_space  cs;
    int             code;

    switch((gs_color_space_index)osp->value.intval) {
      default:  /* can't happen */
      case gs_color_space_index_DeviceGray:
        gs_cspace_init_DeviceGray(&cs);
        break;

      case gs_color_space_index_DeviceRGB:
        gs_cspace_init_DeviceRGB(&cs);
        break;

      case gs_color_space_index_DeviceCMYK:
        gs_cspace_init_DeviceCMYK(&cs);
        break;
    }
    if ((code = gs_setcolorspace(igs, &cs)) >= 0)
        pop(1);
    return code;
}


/*  -   currenttransfer   <proc> */
private int
zcurrenttransfer(i_ctx_t *i_ctx_p)
{
    os_ptr  op = osp;

    push(1);
    *op = istate->transfer_procs.gray;
    return 0;
}

/*
 *  -   processcolors   <int>  -
 *
 * Note: this is an undocumented operator that is not supported
 * in Level 2.
 */
private int
zprocesscolors(i_ctx_t * i_ctx_p)
{
    os_ptr  op = osp;

    push(1);
    make_int(op, gs_currentdevice(igs)->color_info.num_components);
    return 0;
}

/* <proc> settransfer - */
private int
zsettransfer(i_ctx_t * i_ctx_p)
{
    os_ptr  op = osp;
    int     code;

    check_proc(*op);
    check_ostack(zcolor_remap_one_ostack - 1);
    check_estack(1 + zcolor_remap_one_estack);
    istate->transfer_procs.red =
        istate->transfer_procs.green =
        istate->transfer_procs.blue =
        istate->transfer_procs.gray = *op;
    if ((code = gs_settransfer_remap(igs, gs_mapped_transfer, false)) < 0)
        return code;
    push_op_estack(zcolor_reset_transfer);
    pop(1);
    return zcolor_remap_one( i_ctx_p,
                             &istate->transfer_procs.gray,
                             igs->set_transfer.gray,
                             igs,
                             zcolor_remap_one_finish );
}


/*
 * Internal routines
 */

/*
 * Prepare to remap one color component (also used for black generation
 * and undercolor removal). Use the 'for' operator to gather the values.
 * The caller must have done the necessary check_ostack and check_estack.
 */
int
zcolor_remap_one(
    i_ctx_t *           i_ctx_p,
    const ref *         pproc,
    gx_transfer_map *   pmap,
    const gs_state *    pgs,
    op_proc_t           finish_proc )
{
    os_ptr              op;

    /*
     * Detect the identity function, which is a common value for one or
     * more of these functions.
     */
    if (r_size(pproc) == 0) {
	gx_set_identity_transfer(pmap);
	/*
	 * Even though we don't actually push anything on the e-stack, all
	 * clients do, so we return o_push_estack in this case.  This is
	 * needed so that clients' finishing procedures will get run.
	 */
	return o_push_estack;
    }
    op = osp += 4;
    make_real(op - 3, 0);
    make_int(op - 2, transfer_map_size - 1);
    make_real(op - 1, 1);
    *op = *pproc;
    ++esp;
    make_struct(esp, imemory_space((gs_ref_memory_t *) pgs->memory),
		pmap);
    push_op_estack(finish_proc);
    push_op_estack(zfor_samples);
    return o_push_estack;
}

/* Store the result of remapping a component. */
private int
zcolor_remap_one_store(i_ctx_t *i_ctx_p, floatp min_value)
{
    int i;
    gx_transfer_map *pmap = r_ptr(esp, gx_transfer_map);

    if (ref_stack_count(&o_stack) < transfer_map_size)
	return_error(e_stackunderflow);
    for (i = 0; i < transfer_map_size; i++) {
	double v;
	int code =
	    real_param(ref_stack_index(&o_stack, transfer_map_size - 1 - i),
		       &v);

	if (code < 0)
	    return code;
	pmap->values[i] =
	    (v < min_value ? float2frac(min_value) :
	     v >= 1.0 ? frac_1 :
	     float2frac(v));
    }
    ref_stack_pop(&o_stack, transfer_map_size);
    esp--;			/* pop pointer to transfer map */
    return o_pop_estack;
}
int
zcolor_remap_one_finish(i_ctx_t *i_ctx_p)
{
    return zcolor_remap_one_store(i_ctx_p, 0.0);
}
int
zcolor_remap_one_signed_finish(i_ctx_t *i_ctx_p)
{
    return zcolor_remap_one_store(i_ctx_p, -1.0);
}

/* Finally, reset the effective transfer functions and */
/* invalidate the current color. */
int
zcolor_reset_transfer(i_ctx_t *i_ctx_p)
{
    gx_set_effective_transfer(igs);
    return zcolor_remap_color(i_ctx_p);
}
int
zcolor_remap_color(i_ctx_t *i_ctx_p)
{
    gx_unset_dev_color(igs);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def    zcolor_op_defs[] = 
{
    { "0currentcolor", zcurrentcolor },
    { "0currentcolorspace", zcurrentcolorspace },
    { "0.getuseciecolor", zgetuseciecolor },
    { "1setcolor", zsetcolor },
    { "1setcolorspace", zsetcolorspace },
    { "1.setdevcspace", zsetdevcspace },

    /* basic transfer operators */
    { "0currenttransfer", zcurrenttransfer },
    { "0processcolors", zprocesscolors },
    { "1settransfer", zsettransfer },

    /* internal operators */
    { "1%zcolor_remap_one_finish", zcolor_remap_one_finish },
    { "1%zcolor_remap_one_signed_finish", zcolor_remap_one_signed_finish },
    { "0%zcolor_reset_transfer", zcolor_reset_transfer },
    { "0%zcolor_remap_color", zcolor_remap_color },

    /* high level device support */
    { "0.includecolorspace", zincludecolorspace },
    op_def_end(0)
};
