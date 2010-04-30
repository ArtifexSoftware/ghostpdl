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
#include "math_.h"

/* Construct a smooth path passing though a number of points  on the stack */
/* for PDF ink annotations. The program is based on a very simple method of */
/* smoothing polygons by Maxim Shemanarev. */
/* http://www.antigrain.com/research/bezier_interpolation/ */
/* <mark> <x0> <y0> ... <xn> <yn> .pdfinkpath - */
static int
zpdfinkpath(i_ctx_t *i_ctx_p)
{
    os_ptr optr, op = osp;
    uint count = ref_stack_counttomark(&o_stack);

    uint i, ocount;
    int code;
    double x0, y0, x1, y1, x2, y2, x3, y3, xc1, yc1, xc2, yc2, xc3, yc3;
    double len1, len2, len3, k1, k2, xm1, ym1, xm2, ym2;
    double ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y;
    const double smooth_value = 1; /* from 0..1 range */

    if (count == 0)
	return_error(e_unmatchedmark);
    if ((count & 1) == 0 || count < 3)
	return_error(e_rangecheck);
    
    ocount = count - 1;
    optr = op - ocount + 1;
    
    if ((code = real_param(optr, &x1)) < 0)
        return code;
    if ((code = real_param(optr + 1, &y1)) < 0)
        return code;
    if ((code = gs_moveto(igs, x1, y1)) < 0)
        return code;
    if (ocount == 2)
          goto pop;

    if ((code = real_param(optr + 2, &x2)) < 0)
        return code;
    if ((code = real_param(optr + 3, &y2)) < 0)
        return code;
    if (ocount == 4) {
        if((code = gs_lineto(igs, x2, y2)) < 0)
            return code;
        goto pop;
    }
    x0 = 2*x1 - x2;
    y0 = 2*y1 - y2;
    
    for (i = 4; i <= ocount; i += 2) {
        if (i < ocount) {
            if ((code = real_param(optr + i, &x3)) < 0)
                return code;
            if ((code = real_param(optr + i + 1, &y3)) < 0)
                return code;
        } else {
            x3 = 2*x2 - x1;
            y3 = 2*y2 - y1;
        }

        xc1 = (x0 + x1) / 2.0;
        yc1 = (y0 + y1) / 2.0;
        xc2 = (x1 + x2) / 2.0;
        yc2 = (y1 + y2) / 2.0;
        xc3 = (x2 + x3) / 2.0;
        yc3 = (y2 + y3) / 2.0;

        len1 = hypot(x1 - x0, y1 - y0);
        len2 = hypot(x2 - x1, y2 - y1);
        len3 = hypot(x3 - x2, y3 - y2);

        k1 = len1 / (len1 + len2);
        k2 = len2 / (len2 + len3);

        xm1 = xc1 + (xc2 - xc1) * k1;
        ym1 = yc1 + (yc2 - yc1) * k1;

        xm2 = xc2 + (xc3 - xc2) * k2;
        ym2 = yc2 + (yc3 - yc2) * k2;

        ctrl1_x = xm1 + (xc2 - xm1) * smooth_value + x1 - xm1;
        ctrl1_y = ym1 + (yc2 - ym1) * smooth_value + y1 - ym1;

        ctrl2_x = xm2 + (xc2 - xm2) * smooth_value + x2 - xm2;
        ctrl2_y = ym2 + (yc2 - ym2) * smooth_value + y2 - ym2;

        code = gs_curveto(igs, ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y, x2, y2);
        if (code < 0)
            return code;
        x0 = x1, x1 = x2, x2 = x3;
        y0 = y1, y1 = y2, y2 = y3;
    }
  pop:  
    ref_stack_pop(&o_stack, count);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zpdfops_op_defs[] =
{
    {"0.pdfinkpath", zpdfinkpath},
    op_def_end(0)
};
