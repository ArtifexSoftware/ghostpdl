/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Custom operators for PDF interpreter */

#include "ghost.h"
#include "oper.h"
#include "igstate.h"
#include "istack.h"
#include "iutil.h"
#include "gspath.h"
#include "math_.h"
#include "ialloc.h"
#include "malloc_.h"
#include "string_.h"
#include "store.h"
#include "gxgstate.h"
#include "gxdevsop.h"

#ifdef HAVE_LIBIDN
#  include <stringprep.h>
#endif

/* ------ Graphics state ------ */

/* <screen_index> <x> <y> .setscreenphase - */
static int
zsetscreenphase(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;
    int x, y;

    check_type(op[-2], t_integer);
    check_type(op[-1], t_integer);
    check_type(*op, t_integer);
    x = op[-1].value.intval;
    y = op->value.intval;
    if (op[-2].value.intval < -1 ||
        op[-2].value.intval >= gs_color_select_count
        )
        return_error(gs_error_rangecheck);
    code = gs_setscreenphase(igs, x, y,
                             (gs_color_select_t) op[-2].value.intval);
    if (code >= 0)
        pop(3);
    return code;
}

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
        return_error(gs_error_unmatchedmark);
    if ((count & 1) == 0 || count < 3)
        return_error(gs_error_rangecheck);

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

static int
zpdfFormName(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint count = ref_stack_count(&o_stack);
    int code;

    if (count == 0)
        return_error(gs_error_stackunderflow);
    check_read_type(*op, t_string);

    code = (*dev_proc(i_ctx_p->pgs->device, dev_spec_op))((gx_device *)i_ctx_p->pgs->device,
        gxdso_pdf_form_name, (void *)op->value.const_bytes, r_size(op));

    if (code < 0)
        return code;

    ref_stack_pop(&o_stack, 1);
    return 0;
}

#ifdef HAVE_LIBIDN
/* Given a UTF-8 password string, convert it to the canonical form
 * defined by SASLprep (RFC 4013).  This is a permissive implementation,
 * suitable for verifying existing passwords but not for creating new
 * ones -- if you want to create a new password, you'll need to add a
 * strict mode that returns stringprep errors to the user, and uses the
 * STRINGPREP_NO_UNASSIGNED flag to disallow unassigned characters.
 * <string> .saslprep <string> */
static int
zsaslprep(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint input_size = r_size(op);
    byte *buffer;
    uint buffer_size;
    uint output_size;
    Stringprep_rc err;

    check_read_type(*op, t_string);

    /* According to http://unicode.org/faq/normalization.html, converting
     * a UTF-8 string to normalization form KC has a worst-case expansion
     * factor of 11, so we allocate 11 times the length of the string plus
     * 1 for the NUL terminator.  If somehow that's still not big enough,
     * stringprep will return STRINGPREP_TOO_SMALL_BUFFER; there's no
     * danger of corrupting memory. */
    buffer_size = input_size * 11 + 1;
    buffer = ialloc_string(buffer_size, "saslprep result");
    if (buffer == 0)
        return_error(gs_error_VMerror);

    memcpy(buffer, op->value.bytes, input_size);
    buffer[input_size] = '\0';

    err = stringprep((char *)buffer, buffer_size, 0, stringprep_saslprep);
    if (err != STRINGPREP_OK) {
        ifree_string(buffer, buffer_size, "saslprep result");

        /* Since we're just verifying the password to an existing
         * document here, we don't care about "invalid input" errors
         * like STRINGPREP_CONTAINS_PROHIBITED.  In these cases, we
         * ignore the error and return the original string unchanged --
         * chances are it's not the right password anyway, and if it
         * is we shouldn't gratuitously fail to decrypt the document.
         *
         * On the other hand, errors like STRINGPREP_NFKC_FAILED are
         * real errors, and should be returned to the user.
         *
         * Fortunately, the stringprep error codes are sorted to make
         * this easy: the errors we want to ignore are the ones with
         * codes less than 100. */
        if ((int)err < 100)
            return 0;

        return_error(gs_error_ioerror);
    }

    output_size = strlen((char *)buffer);
    buffer = iresize_string(buffer, buffer_size, output_size,
        "saslprep result");	/* can't fail */
    make_string(op, a_all | icurrent_space, output_size, buffer);

    return 0;
}
#endif

/* ------ Initialization procedure ------ */

const op_def zpdfops_op_defs[] =
{
    {"0.pdfinkpath", zpdfinkpath},
    {"1.pdfFormName", zpdfFormName},
    {"3.setscreenphase", zsetscreenphase},
#ifdef HAVE_LIBIDN
    {"1.saslprep", zsaslprep},
#endif
    op_def_end(0)
};
