/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* DeviceN color space support */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gxcspace.h"		/* must precede gscolor2.h */
#include "gscolor2.h"
#include "igstate.h"
#include "ialloc.h"
#include "iname.h"

/* Imported from gscdevn.c */
extern const gs_color_space_type gs_color_space_type_DeviceN;

/* <array> .setdevicenspace - */
/* The current color space is the alternate space for the DeviceN space. */
private int
zsetdevicenspace(register os_ptr op)
{
    const ref *pcsa;
    gs_separation_name *names;
    uint num_components;
    gs_color_space cs;
    ref_colorspace cspace_old;
    int code;

    check_read_type(*op, t_array);
    if (r_size(op) != 4)
	return_error(e_rangecheck);
    pcsa = op->value.const_refs + 1;
    if (!r_is_array(pcsa))
	return_error(e_typecheck);
    num_components = r_size(pcsa);
    if (num_components == 0)
	return_error(e_rangecheck);
    check_proc(pcsa[2]);
    cs = *gs_currentcolorspace(igs);
    if (!cs.type->can_be_alt_space)
	return_error(e_rangecheck);
    names = (gs_separation_name *)
	ialloc_byte_array(num_components, sizeof(gs_separation_name),
			  ".setdevicenspace");
    if (names == 0)
	return_error(e_VMerror);
    {
	uint i;
	ref sname;

	for (i = 0; i < num_components; ++i) {
	    array_get(pcsa, (long)i, &sname);
	    switch (r_type(&sname)) {
		case t_string:
		    code = name_from_string(&sname, &sname);
		    if (code < 0) {
			ifree_object(names, ".setdevicenspace");
			return code;
		    }
		    /* falls through */
		case t_name:
		    names[i] = name_index(&sname);
		    break;
		default:
		    ifree_object(names, ".setdevicenspace");
		    return_error(e_typecheck);
	    }
	}
    }
    /* See zcsindex.c for why we use memmove here. */
    memmove(&cs.params.device_n.alt_space, &cs,
	    sizeof(cs.params.device_n.alt_space));
    gs_cspace_init(&cs, &gs_color_space_type_DeviceN, NULL);
    cspace_old = istate->colorspace;
    istate->colorspace.procs.special.device_n.layer_names = pcsa[0];
    istate->colorspace.procs.special.device_n.tint_transform = pcsa[2];
    cs.params.device_n.names = names;
    cs.params.device_n.num_components = num_components;
    cs.params.device_n.tint_transform = 0;  /****** ? ******/
    cs.params.device_n.tint_transform_data = 0;  /****** ? ******/
    code = gs_setcolorspace(igs, &cs);
    if (code < 0) {
	istate->colorspace = cspace_old;
	ifree_object(names, ".setdevicenspace");
	return code;
    }
    pop(1);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zcsdevn_op_defs[] =
{
    op_def_begin_ll3(),
    {"1.setdevicenspace", zsetdevicenspace},
    op_def_end(0)
};
