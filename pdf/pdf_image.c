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

/* Image operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_image.h"

extern int pdf_dict_from_stack(pdf_context *ctx);

int pdf_BI(pdf_context *ctx)
{
    return pdf_mark_stack(ctx, PDF_DICT_MARK);
}

int pdf_ID(pdf_context *ctx)
{
    pdf_dict *d;
    int code;

    code = pdf_dict_from_stack(ctx);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    pdf_countup(d);
    pdf_pop(ctx, 1);

    /* Need to process it here. Punt for now */
    pdf_countdown(d);
    return 0;
}

int pdf_EI(pdf_context *ctx)
{
    pdf_clearstack(ctx);
    return 0;
}
