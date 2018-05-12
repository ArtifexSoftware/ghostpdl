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

/* Font operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_image.h"

int pdf_d0(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 2)
        pdf_pop(ctx, 2);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_d1(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 6)
        pdf_pop(ctx, 6);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Tf(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 2)
        pdf_pop(ctx, 2);
    else
        pdf_clearstack(ctx);
    return 0;
}
