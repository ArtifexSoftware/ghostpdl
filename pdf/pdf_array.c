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

/* array handling for the PDF interpreter */

#include "ghostpdf.h"
#include "pdf_types.h"
#include "pdf_stack.h"
#include "pdf_array.h"

void pdf_free_array(pdf_obj *o)
{
    pdf_array *a = (pdf_array *)o;
    int i;

    for (i=0;i < a->entries;i++) {
        if (a->values[i] != NULL)
            pdf_countdown(a->values[i]);
    }
    gs_free_object(a->memory, a->values, "pdf interpreter free array contents");
    gs_free_object(a->memory, a, "pdf interpreter free array");
}

/* The object returned by pdf_array_get has its reference count incremented by 1 to
 * indicate the reference now held by the caller, in **o.
 */
int pdf_array_get(pdf_array *a, uint64_t index, pdf_obj **o)
{
    if (index > a->size)
        return_error(gs_error_rangecheck);

    *o = a->values[index];
    pdf_countup(*o);
    return 0;
}

int pdf_array_get_type(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdf_array_get(a, index, o);
    if (code < 0)
        return code;

    if ((*o)->type != type) {
        if ((*o)->type == PDF_INDIRECT){
            pdf_obj *o1 = NULL;
            pdf_indirect_ref *r = (pdf_indirect_ref *)*o;

            code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
            pdf_countdown(*o);
            *o = NULL;
            if (code < 0)
                return code;

            (void)pdf_array_put(a, index, o1);

            if (o1->type != type) {
                pdf_countdown(o1);
                return_error(gs_error_typecheck);
            }
            *o = o1;
        } else {
            pdf_countdown(*o);
            *o = NULL;
            return_error(gs_error_typecheck);
        }
    }
    return 0;
}

int pdf_array_get_int(pdf_context *ctx, pdf_array *a, uint64_t index, int64_t *i)
{
    int code;
    pdf_num *n;

    code = pdf_array_get_type(ctx, a, index, PDF_INT, (pdf_obj **)&n);
    if (code < 0)
        return code;
    *i = n->value.i;
    pdf_countdown(n);
    return 0;
}

int pdf_array_get_number(pdf_context *ctx, pdf_array *a, uint64_t index, double *f)
{
    int code;
    pdf_num *n;

    code = pdf_array_get(a, index, (pdf_obj **)&n);
    if (code < 0)
        return code;
    if (n->type == PDF_INT)
        *f = (double)n->value.i;
    else {
        if (n->type == PDF_REAL)
            *f = n->value.d;
        else {
            pdf_countdown(n);
            return_error(gs_error_typecheck);
        }
    }

    pdf_countdown(n);
    return 0;
}

int pdf_array_put(pdf_array *a, uint64_t index, pdf_obj *o)
{
    if (index > a->size)
        return_error(gs_error_rangecheck);

    pdf_countdown(a->values[index]);
    a->values[index] = o;
    pdf_countup(o);
    return 0;
}
