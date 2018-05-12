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

/* Text operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_image.h"

int pdf_T_star(pdf_context *ctx)
{
    return 0;
}

int pdf_Tc(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Td(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 2)
        pdf_pop(ctx, 2);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_TD(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 2)
        pdf_pop(ctx, 2);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Tj(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_TJ(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_TL(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Tm(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 6)
        pdf_pop(ctx, 6);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Tr(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Ts(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Tw(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_Tz(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_singlequote(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 1)
        pdf_pop(ctx, 1);
    else
        pdf_clearstack(ctx);
    return 0;
}

int pdf_doublequote(pdf_context *ctx)
{
    if (ctx->stack_top - ctx->stack_bot >= 3)
        pdf_pop(ctx, 3);
    else
        pdf_clearstack(ctx);
    return 0;
}
