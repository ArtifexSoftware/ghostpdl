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
#include "pdf_text.h"
#include "pdf_image.h"
#include "pdf_stack.h"

int pdfi_T_star(pdf_context *ctx)
{
    return 0;
}

int pdfi_Tc(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_Td(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 2)
        pdfi_pop(ctx, 2);
    else
        pdfi_clearstack(ctx);
    return 0;
}

int pdfi_TD(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 2)
        pdfi_pop(ctx, 2);
    else
        pdfi_clearstack(ctx);
    return 0;
}

int pdfi_Tj(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_TJ(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_TL(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_Tm(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 6)
        pdfi_pop(ctx, 6);
    else
        pdfi_clearstack(ctx);
    return 0;
}

int pdfi_Tr(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_Ts(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_Tw(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_Tz(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_singlequote(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 1)
        pdfi_pop(ctx, 1);
    return 0;
}

int pdfi_doublequote(pdf_context *ctx)
{
    if (pdfi_count_stack(ctx) >= 3)
        pdfi_pop(ctx, 3);
    return 0;
}
