/* Copyright (C) 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

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
