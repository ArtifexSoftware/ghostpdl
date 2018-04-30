/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

/* Path operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_path.h"

int pdf_moveto (pdf_context *ctx)
{
    pdf_num *n1, *n2;
    int code;
    double d1, d2;

    if (ctx->stack_top - ctx->stack_bot < 2)
        return_error(gs_error_stackunderflow);

    n1 = ctx->stack_top[-1];
    n2 = ctx->stack_top[-2];
    if (n1->type == PDF_INT){
        d1 = (double)n1->value.i;
    } else{
        if (n1->type == PDF_REAL) {
            d1 = n1->value.d;
        } else
            return_error(gs_error_typecheck);
    }
    if (n2->type == PDF_INT){
        d2 = (double)n1->value.i;
    } else{
        if (n2->type == PDF_REAL) {
            d2 = n1->value.d;
        } else
            return_error(gs_error_typecheck);
    }
    
    code = gs_moveto(ctx->pgs, d1, d2);
    if (code ==0)
        pdf_pop(ctx, 2);
    return 0;
}
