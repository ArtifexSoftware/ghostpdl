/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Operators for setting trapping parameters and zones */
#include "ghost.h"
#include "oper.h"
#include "ialloc.h"
#include "iparam.h"
#include "gstrap.h"

/* Define the current trap parameters. */
/****** THIS IS BOGUS ******/
gs_trap_params_t i_trap_params;

/* <dict> .settrapparams - */
private int
zsettrapparams(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    dict_param_list list;
    int code;

    check_type(*op, t_dictionary);
    code = dict_param_list_read(&list, op, NULL, false, iimemory);
    if (code < 0)
	return code;
    code = gs_settrapparams(&i_trap_params, (gs_param_list *) & list);
    iparam_list_release(&list);
    if (code < 0)
	return code;
    pop(1);
    return 0;
}

/* - settrapzone - */
private int
zsettrapzone(i_ctx_t *i_ctx_p)
{
/****** NYI ******/
    return_error(e_undefined);
}

/* ------ Initialization procedure ------ */

const op_def ztrap_op_defs[] =
{
    op_def_begin_ll3(),
    {"1.settrapparams", zsettrapparams},
    {"0settrapzone", zsettrapzone},
    op_def_end(0)
};
