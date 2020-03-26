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

/* Get the current color space (the base one) from a color space
 */
gs_color_space_index pdfi_get_color_space_index(pdf_context *ctx, const gs_color_space *pcs)
{
    gs_color_space_index csi;

    /* Get the color space index */
    csi = gs_color_space_get_index(pcs);

    /* If its an Indexed space, then use the base space */
    if (csi == gs_color_space_index_Indexed)
        csi = gs_color_space_get_index(pcs->base_space);

    /* If its ICC, see if its a substitution for one of the device
     * spaces. If so then we will want to behave as if we were using the
     * device space.
     */
    if (csi == gs_color_space_index_ICC && pcs->cmm_icc_profile_data)
        csi = gsicc_get_default_type(pcs->cmm_icc_profile_data);

    return csi;
}

/* Get the current color space (the base one) from current graphics state.
 * index -- tells whether to pull from 0 or 1 (probably 0)
 */
gs_color_space_index pdfi_currentcolorspace(pdf_context *ctx, int index)
{
    const gs_color_space *pcs;

    pcs = ctx->pgs->color[index].color_space;

    return pdfi_get_color_space_index(ctx, pcs);
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

/* Set rendering intent, translating from name to number */
int pdfi_setrenderingintent(pdf_context *ctx, pdf_name *n)
{
    int code = 0;

    if (pdfi_name_is(n, "Perceptual")) {
        code = gs_setrenderingintent(ctx->pgs, 0);
    } else if (pdfi_name_is(n, "Saturation")) {
        code = gs_setrenderingintent(ctx->pgs, 2);
    } else if (pdfi_name_is(n, "RelativeColorimetric")) {
        code = gs_setrenderingintent(ctx->pgs, 1);
    } else if (pdfi_name_is(n, "AbsoluteColorimetric")) {
        code = gs_setrenderingintent(ctx->pgs, 3);
    } else {
        code = gs_error_undefined;
    }
    return code;
}

int pdfi_string_from_name(pdf_context *ctx, pdf_name *n, char **str, int *len)
{
    if (n->type != PDF_NAME)
        return gs_note_error(gs_error_typecheck);

    *str = NULL;
    *len = 0;

    *str = (char *)gs_alloc_bytes(ctx->memory, n->length + 1, "pdfi_string_from_name");
    if (*str == NULL)
        return gs_note_error(gs_error_VMerror);

    memcpy(*str, n->data, n->length);
    (*str)[n->length + 1] = 0x00;
    *len = n->length;

    return 0;
}
