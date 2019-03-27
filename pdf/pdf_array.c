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

void pdfi_free_array(pdf_obj *o)
{
    pdf_array *a = (pdf_array *)o;
    int i;

    for (i=0;i < a->size;i++) {
        if (a->values[i] != NULL)
            pdfi_countdown(a->values[i]);
    }
    gs_free_object(a->memory, a->values, "pdf interpreter free array contents");
    gs_free_object(a->memory, a, "pdf interpreter free array");
}

/* Fetch object from array, resolving indirect reference if needed
 * (Does not increment reference count, caller needs to do that if they want to)
 */
static int
pdfi_array_fetch(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o)
{
    int code;
    pdf_obj *obj;

    *o = NULL;

    if (index >= a->size)
        return_error(gs_error_rangecheck);
    obj = a->values[index];

    if (obj->type == PDF_INDIRECT) {
        pdf_obj *o1 = NULL;
        pdf_indirect_ref *r = (pdf_indirect_ref *)obj;

        code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
        if (code < 0)
            return code;
        /* Don't need this reference */
        pdfi_countdown(o1);

        (void)pdfi_array_put(a, index, o1);
        obj = o1;
    }

    *o = obj;
    return 0;
}


/* Get the value from the pdfi_array without incrementing its reference count.
 * Caller needs to increment it themselves if they want to keep it (or just use
 * pdfi_array_get, below).
 */
int pdfi_array_peek(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o)
{
    return pdfi_array_fetch(ctx, a, index, o);
}

/* The object returned by pdfi_array_get has its reference count incremented by 1 to
 * indicate the reference now held by the caller, in **o.
 */
int pdfi_array_get(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o)
{
    int code;

    /* temporarily call this until all old uses of pdfi_array_get() are sorted */
    return pdfi_array_get_no_indirect(ctx, a, index, o);

    code = pdfi_array_fetch(ctx, a, index, o);
    if (code < 0) return code;

    *o = a->values[index];
    pdfi_countup(*o);
    return 0;
}

/* Get element from array without resolving PDF_INDIRECT
 * It looks to me like some usages need to do the checking themselves to
 * avoid circular references?  Can remove this if not really needed.
 */
int
pdfi_array_get_no_indirect(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o)
{
    if (index >= a->size)
        return_error(gs_error_rangecheck);

    *o = a->values[index];
    pdfi_countup(*o);
    return 0;
}

/* Get value from pdfi_array without incrementing its reference count.
 * Handles type-checking and resolving indirect references.
 */
int
pdfi_array_peek_type(pdf_context *ctx, pdf_array *a, uint64_t index,
                     pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdfi_array_fetch(ctx, a, index, o);
    if (code < 0)
        return code;

    if ((*o)->type != type)
        return_error(gs_error_typecheck);

    return 0;
}

/* Get value from pdfi_array, incrementing its reference count for caller.
 * Handles type-checking and resolving indirect references.
 */
int
pdfi_array_get_type(pdf_context *ctx, pdf_array *a, uint64_t index,
                    pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdfi_array_peek_type(ctx, a, index, type, o);
    if (code < 0)
        return code;

    pdfi_countup(*o);
    return 0;
}

int pdfi_array_get_int(pdf_context *ctx, pdf_array *a, uint64_t index, int64_t *i)
{
    int code;
    pdf_num *n;

    code = pdfi_array_peek_type(ctx, a, index, PDF_INT, (pdf_obj **)&n);
    if (code < 0)
        return code;
    *i = n->value.i;
    return 0;
}

int pdfi_array_get_number(pdf_context *ctx, pdf_array *a, uint64_t index, double *f)
{
    int code;
    pdf_num *n;

    code = pdfi_array_peek(ctx, a, index, (pdf_obj **)&n);
    if (code < 0)
        return code;
    if (n->type == PDF_INT)
        *f = (double)n->value.i;
    else {
        if (n->type == PDF_REAL)
            *f = n->value.d;
        else {
            return_error(gs_error_typecheck);
        }
    }

    return 0;
}

/* Check whether a particular object is in an array
 * If index is not NULL, fill it in with the index of the object
 */
bool
pdfi_array_known(pdf_array *a, pdf_obj *o, int *index)
{
    int i;

    for (i=0; i < a->size; i++) {
        pdf_obj *val;
        val = a->values[i];
        if (val->object_num == o->object_num) {
            if (index != NULL) *index = i;
            return true;
        }
    }
    return false;
}

int pdfi_array_put(pdf_array *a, uint64_t index, pdf_obj *o)
{
    if (index > a->size)
        return_error(gs_error_rangecheck);

    pdfi_countdown(a->values[index]);
    a->values[index] = o;
    pdfi_countup(o);
    return 0;
}
