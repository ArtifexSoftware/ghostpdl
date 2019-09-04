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

/* Miscellaneous routines */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_misc.h"
#include "pdf_gstate.h"

/* Get current bbox, possibly from stroking current path (utility function) */
int pdfi_get_current_bbox(pdf_context *ctx, gs_rect *bbox, bool stroked)
{
    int code, code1;

    if (stroked) {
        code = pdfi_gsave(ctx);
        if (code < 0)
            return code;
        code = gs_strokepath(ctx->pgs);
        if (code < 0)
            goto exit;
    }
    code = gs_upathbbox(ctx->pgs, bbox, false);

 exit:
    if (stroked) {
        code1 = pdfi_grestore(ctx);
        if (code == 0)
            code = code1;
    }
    return code;
}

int
pdfi_name_strcmp(const pdf_name *n, const char *s)
{
    int len = strlen(s);
    if (n->length == len)
        return memcmp(n->data, s, len);
    return -1;
}

bool
pdfi_name_is(const pdf_name *n, const char *s)
{
    int len = strlen(s);
    if (n->length == len)
        return (memcmp(n->data, s, len) == 0);
    return false;
}

int
pdfi_name_cmp(const pdf_name *n1, const pdf_name *n2)
{
    if (n1->length != n2->length)
        return -1;
    return memcmp(n1->data, n2->data, n1->length);
}
