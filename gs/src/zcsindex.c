/* Copyright (C) 2001-2006 artofcode LLC.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Indexed color space support */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gscolor.h"
#include "gsmatrix.h"		/* for gxcolor2.h */
#include "gxcspace.h"
#include "gxfixed.h"		/* ditto */
#include "gxcolor2.h"
#include "estack.h"
#include "ialloc.h"
#include "icsmap.h"
#include "igstate.h"
#include "ivmspace.h"
#include "store.h"

/* Imported from gscolor2.c */
extern const gs_color_space_type gs_color_space_type_Indexed;

/* Forward references. */
private int indexed_map1(i_ctx_t *);

/* <array> .setindexedspace - */
/* The current color space is the base space for the indexed space. */
private int
zsetindexedspace(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref *pproc = &istate->colorspace.procs.special.index_proc;
    const ref *pcsa;
    gs_color_space *pcs;
    gs_color_space *pcs_base;
    ref_colorspace cspace_old;
    uint edepth = ref_stack_count(&e_stack);
    int num_entries;
    int code;

    check_read_type(*op, t_array);
    if (r_size(op) != 4)
	return_error(e_rangecheck);
    pcsa = op->value.const_refs + 1;
    check_type_only(pcsa[1], t_integer);
    if (pcsa[1].value.intval < 0 || pcsa[1].value.intval > 4095)
	return_error(e_rangecheck);
    num_entries = (int)pcsa[1].value.intval + 1;
    pcs_base = gs_currentcolorspace(igs);
    if (!pcs_base->type->can_be_base_space)
	return_error(e_rangecheck);
    cspace_old = istate->colorspace;

    if (r_has_type(&pcsa[2], t_string)) {
	int num_values = num_entries * cs_num_components(pcs_base);

	check_read(pcsa[2]);
	/*
	 * The PDF and PS specifications state that the lookup table must have
	 * the exact number of of data bytes needed.  However we have found
	 * PDF files from Amyuni with extra data bytes.  Acrobat 6.0 accepts
	 * these files without complaint, so we ignore the extra data.
	 */
	if (r_size(&pcsa[2]) < num_values)
	    return_error(e_rangecheck);
	pcs = gs_cspace_alloc(imemory, &gs_color_space_type_Indexed);
	pcs->base_space = pcs_base;
	rc_increment(pcs_base);
	pcs->params.indexed.lookup.table.data = pcsa[2].value.const_bytes;
	pcs->params.indexed.lookup.table.size = num_values;
	pcs->params.indexed.use_proc = 0;
	make_null(pproc);
	code = 0;
    } else {
	gs_indexed_map *map;

	check_proc(pcsa[2]);
	/*
	 * We have to call zcs_begin_map before moving the parameters,
	 * since if the color space is a DeviceN or Separation space,
	 * the memmove will overwrite its parameters.
	 */
	code = zcs_begin_map(i_ctx_p, &map, &pcsa[2], num_entries,
			     pcs_base, indexed_map1);
	if (code < 0)
	    return code;
	pcs = gs_cspace_alloc(imemory, &gs_color_space_type_Indexed);
	pcs->base_space = pcs_base;
	rc_increment(pcs_base);
	pcs->params.indexed.use_proc = 1;
	*pproc = pcsa[2];
	map->proc.lookup_index = lookup_indexed_map;
	pcs->params.indexed.lookup.map = map;
    }
    pcs->params.indexed.hival = num_entries - 1;
    pcs->params.indexed.n_comps = cs_num_components(pcs_base);
    code = gs_setcolorspace(igs, pcs);
    /* release reference from construction */
    rc_decrement_only(pcs, "zsetindexedspace");
    if (code < 0) {
	istate->colorspace = cspace_old;
	ref_stack_pop_to(&e_stack, edepth);
	return code;
    }
    pop(1);
    return (ref_stack_count(&e_stack) == edepth ? 0 : o_push_estack);	/* installation will load the caches */
}

/* Continuation procedure for saving mapped Indexed color values. */
private int
indexed_map1(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr ep = esp;
    int i = (int)ep[csme_index].value.intval;

    if (i >= 0) {		/* i.e., not first time */
	int m = (int)ep[csme_num_components].value.intval;
	int code = float_params(op, m, &r_ptr(&ep[csme_map], gs_indexed_map)->values[i * m]);

	if (code < 0)
	    return code;
	pop(m);
	op -= m;
	if (i == (int)ep[csme_hival].value.intval) {	/* All done. */
	    esp -= num_csme;
	    return o_pop_estack;
	}
    }
    push(1);
    ep[csme_index].value.intval = ++i;
    make_int(op, i);
    make_op_estack(ep + 1, indexed_map1);
    ep[2] = ep[csme_proc];	/* lookup proc */
    esp = ep + 2;
    return o_push_estack;
}

/* ------ Initialization procedure ------ */

const op_def zcsindex_l2_op_defs[] =
{
    op_def_begin_level2(),
    {"1.setindexedspace", zsetindexedspace},
		/* Internal operators */
    {"1%indexed_map1", indexed_map1},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Allocate, and prepare to load, the index or tint map. */
int
zcs_begin_map(i_ctx_t *i_ctx_p, gs_indexed_map ** pmap, const ref * pproc,
	      int num_entries,  const gs_color_space * base_space,
	      op_proc_t map1)
{
    gs_memory_t *mem = gs_state_memory(igs);
    int space = imemory_space((gs_ref_memory_t *)mem);
    int num_components = cs_num_components(base_space);
    int num_values = num_entries * num_components;
    gs_indexed_map *map;
    int code = alloc_indexed_map(&map, num_values, mem,
				 "setcolorspace(mapped)");
    es_ptr ep;

    if (code < 0)
	return code;
    *pmap = map;
    /* Map the entire set of color indices.  Since the */
    /* o-stack may not be able to hold N*4096 values, we have */
    /* to load them into the cache as they are generated. */
    check_estack(num_csme + 1);	/* 1 extra for map1 proc */
    ep = esp += num_csme;
    make_int(ep + csme_num_components, num_components);
    make_struct(ep + csme_map, space, map);
    ep[csme_proc] = *pproc;
    make_int(ep + csme_hival, num_entries - 1);
    make_int(ep + csme_index, -1);
    push_op_estack(map1);
    return o_push_estack;
}
