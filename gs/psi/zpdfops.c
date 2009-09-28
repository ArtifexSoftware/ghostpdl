/* Copyright (C) 2009 Artifex Software, Inc.
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
/* Custom operators for PDF interpreter */

#include "ghost.h"
#include "oper.h"
#include "igstate.h"
#include "istack.h"
#include "iutil.h"
#include "gspath.h"

/* Construct a smooth path passing though a number of points */
/* on the stack for PDF ink annotations. */
/* <mark> <x0> <y0> ... <xn> <yn> .pdfinkpath - */
int
zpdfinkpath(i_ctx_t *i_ctx_p)
{
    os_ptr optr, op = osp;
    uint count = ref_stack_counttomark(&o_stack);

    uint i, ocount;
    int code;
    double x, y;

    if (count == 0)
	return_error(e_unmatchedmark);
    if ((count & 1) == 0 || count < 3)
	return_error(e_rangecheck);
    
    ocount = count - 1;
    optr = op - ocount + 1;
    
    code = real_param(optr, &x);
    if (code < 0)
        return code;
    code = real_param(optr + 1, &y);
    if (code < 0)
        return code;
    code = gs_moveto(igs, x, y);
    if (code < 0)
        return code;
 
    for (i = 2; i < ocount; i += 2) {
        code = real_param(optr + i, &x);
        if (code < 0)
            return code;
        code = real_param(optr + i + 1, &y);
        if (code < 0)
            return code;
        code = gs_lineto(igs, x, y);
        if (code < 0)
            return code;
    }
    
    ref_stack_pop(&o_stack, count);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zpdfops_op_defs[] =
{
    {"0.pdfinkpath", zpdfinkpath},
    op_def_end(0)
};
