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

/* dictionary handling for the PDF interpreter */
#include "ghostpdf.h"
#include "pdf_types.h"
#include "pdf_dict.h"
#include "pdf_stack.h"
#include "pdf_int.h"

void pdfi_free_dict(pdf_obj *o)
{
    pdf_dict *d = (pdf_dict *)o;
    int i;

    for (i=0;i < d->entries;i++) {
        if (d->keys[i] != NULL)
            pdfi_countdown(d->keys[i]);
        if (d->values[i] != NULL)
            pdfi_countdown(d->values[i]);
    }
    gs_free_object(d->memory, d->keys, "pdf interpreter free dictionary keys");
    gs_free_object(d->memory, d->values, "pdf interpreter free dictioanry values");
    gs_free_object(d->memory, d, "pdf interpreter free dictionary");
}

int pdfi_dict_from_stack(pdf_context *ctx)
{
    uint64_t index = 0;
    pdf_dict *d = NULL;
    uint64_t i = 0;
    int code;

    code = pdfi_count_to_mark(ctx, &index);
    if (code < 0)
        return code;

    if (index & 1)
        return_error(gs_error_rangecheck);

    d = (pdf_dict *)gs_alloc_bytes(ctx->memory, sizeof(pdf_dict), "pdfi_dict_from_stack");
    if (d == NULL)
        return_error(gs_error_VMerror);

    memset(d, 0x00, sizeof(pdf_dict));
    d->memory = ctx->memory;
    d->type = PDF_DICT;

    d->size = d->entries = index >> 1;

    d->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdfi_dict_from_stack");
    if (d->keys == NULL) {
        gs_free_object(d->memory, d, "pdfi_read_dict error");
        return_error(gs_error_VMerror);
    }
    memset(d->keys, 0x00, d->size * sizeof(pdf_obj *));

    d->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdfi_dict_from_stack");
    if (d->values == NULL) {
        gs_free_object(d->memory, d->keys, "pdfi_read_dict error");
        gs_free_object(d->memory, d, "pdfi_read_dict error");
        return_error(gs_error_VMerror);
    }
    memset(d->values, 0x00, d->size * sizeof(pdf_obj *));

    while (index) {
        i = (index / 2) - 1;

        /* In PDF keys are *required* to be names, so we ought to check that here */
        if (((pdf_obj *)ctx->stack_top[-2])->type == PDF_NAME) {
            d->keys[i] = ctx->stack_top[-2];
            pdfi_countup(d->keys[i]);
            d->values[i] = ctx->stack_top[-1];
            pdfi_countup(d->values[i]);
        } else {
            pdfi_free_dict((pdf_obj *)d);
            return_error(gs_error_typecheck);
        }

        pdfi_pop(ctx, 2);
        index -= 2;
    }

    code = pdfi_clear_to_mark(ctx);
    if (code < 0)
        return code;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, "\n >>\n");

#if REFCNT_DEBUG
    d->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated dictionary object with UID %"PRIi64"\n", d->UID);
#endif
    code = pdfi_push(ctx, (pdf_obj *)d);
    if (code < 0)
        pdfi_free_dict((pdf_obj *)d);

    return code;
}


/* Convenience routine for common case where there are two possible keys */
int
pdfi_dict_get2(pdf_context *ctx, pdf_dict *d, const char *Key1,
               const char *Key2, pdf_obj **o)
{
    int code;

    code = pdfi_dict_get(ctx, d, Key1, o);
    if (code == gs_error_undefined)
        code = pdfi_dict_get(ctx, d, Key2, o);
    return code;
}

/* The object returned by pdfi_dict_get has its reference count incremented by 1 to
 * indicate the reference now held by the caller, in **o.
 */
int pdfi_dict_get(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o)
{
    int i=0, code;
    pdf_name *t;

    *o = NULL;

    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_strcmp((pdf_name *)t, Key) == 0) {
                if (d->values[i]->type == PDF_INDIRECT) {
                    pdf_indirect_ref *r = (pdf_indirect_ref *)d->values[i];

                    code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, o);
                    if (code < 0)
                        return code;
                    pdfi_countdown(d->values[i]);
                    d->values[i] = *o;
                }
                *o = d->values[i];
                pdfi_countup(*o);
                return 0;
            }
        }
    }
    return_error(gs_error_undefined);
}

/* As per pdfi_dict_get(), but doesn't replace an indirect reference in a dictionary with a
 * new object. This is for Resources following, such as Do, where we will have to seek and
 * read the indirect object anyway, and we need to ensure that Form XObjects (for example)
 * don't have circular calls.
 */
int pdfi_dict_get_no_store_R(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o)
{
    int i=0, code;
    pdf_name *t;

    *o = NULL;

    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_strcmp((pdf_name *)t, Key) == 0) {
                if (d->values[i]->type == PDF_INDIRECT) {
                    pdf_indirect_ref *r = (pdf_indirect_ref *)d->values[i];

                    code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, o);
                    if (code < 0)
                        return code;
                } else
                    *o = d->values[i];
                pdfi_countup(*o);
                return 0;
            }
        }
    }
    return_error(gs_error_undefined);
}

/* Convenience routine for common case where there are two possible keys */
int
pdfi_dict_get_type2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdfi_dict_get_type(ctx, d, Key1, type, o);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_type(ctx, d, Key2, type, o);
    return code;
}

int pdfi_dict_get_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdfi_dict_get(ctx, d, Key, o);
    if (code < 0)
        return code;

    if ((*o)->type != type) {
        if ((*o)->type == PDF_INDIRECT){
            pdf_name *NewKey = NULL;
            pdf_obj *o1 = NULL;
            pdf_indirect_ref *r = (pdf_indirect_ref *)*o;

            code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
            pdfi_countdown(*o);
            *o = NULL;
            if (code < 0)
                return code;

            code = pdfi_make_name(ctx, (byte *)Key, strlen(Key), (pdf_obj **)&NewKey);
            if (code == 0) {
                (void)pdfi_dict_put(d, (pdf_obj *)NewKey, o1);
                pdfi_countdown(NewKey);
            }
            if (o1->type != type) {
                pdfi_countdown(o1);
                return_error(gs_error_typecheck);
            }
            *o = o1;
        } else {
            pdfi_countdown(*o);
            *o = NULL;
            return_error(gs_error_typecheck);
        }
    }
    return 0;
}

/* Convenience routine for common case where value has two possible keys */
int
pdfi_dict_get_int2(pdf_context *ctx, pdf_dict *d, const char *Key1,
                   const char *Key2, int64_t *i)
{
    int code;

    code = pdfi_dict_get_int(ctx, d, Key1, i);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_int(ctx, d, Key2, i);
    return code;
}

int pdfi_dict_get_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i)
{
    int code;
    pdf_num *n;

    code = pdfi_dict_get_type(ctx, d, Key, PDF_INT, (pdf_obj **)&n);
    if (code < 0)
        return code;

    *i = n->value.i;
    pdfi_countdown(n);
    return 0;
}

/* Get an int from dict, and if undefined, return provided default */
int pdfi_dict_get_int_def(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i,
                          int64_t def_val)
{
    int code;

    code = pdfi_dict_get_int(ctx, d, Key, i);
    if (code == gs_error_undefined) {
        *i = def_val;
        code = 0;
    }

    return code;
}

/* Convenience routine for common case where value has two possible keys */
int
pdfi_dict_get_bool2(pdf_context *ctx, pdf_dict *d, const char *Key1,
                    const char *Key2, bool *val)
{
    int code;

    code = pdfi_dict_get_bool(ctx, d, Key1, val);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_bool(ctx, d, Key2, val);
    return code;
}

int pdfi_dict_get_bool(pdf_context *ctx, pdf_dict *d, const char *Key, bool *val)
{
    int code;
    pdf_bool *b;

    code = pdfi_dict_get_type(ctx, d, Key, PDF_BOOL, (pdf_obj **)&b);
    if (code < 0)
        return code;

    *val = b->value;
    pdfi_countdown(b);
    return 0;
}

int pdfi_dict_get_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f)
{
    int code;
    pdf_num *o;

    code = pdfi_dict_get(ctx, d, Key, (pdf_obj **)&o);
    if (code < 0)
        return code;
    if (o->type == PDF_INDIRECT) {
        pdf_obj *o1 = NULL;
        pdf_indirect_ref *r = (pdf_indirect_ref *)o;

        code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
        pdfi_countdown(o);
        o = NULL;
        if (code < 0)
            return code;
        o = (pdf_num *)o1;
    }

    if (o->type == PDF_INT) {
        *f = (double)(o->value.i);
    } else {
        if (o->type == PDF_REAL){
            *f = o->value.d;
        } else {
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }
    }
    pdfi_countdown(o);
    return 0;
}

/* convenience functions for retrieving arrys, see shadings and functions */

/* The 'fill' versions fill existing arrays, and need a size,
 * the 'make' versions allocate memory and fill it. Both varieties return the
 * number of entries on success. The fill Matrix utility expects to always
 * receive 6 values. The Domain function expects to receive an even number of
 * entries and each pair must have the second element larger than the first.
 */
int fill_domain_from_dict(pdf_context *ctx, float *parray, int size, pdf_dict *dict)
{
    int code, i;
    pdf_array *a = NULL;

    code = pdfi_dict_get(ctx, dict, "Domain", (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries & 1 || a->entries > size) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        if (a->values[i]->type == PDF_INT)
            parray[i] = (float)((pdf_num *)a->values[i])->value.i;
        else
            parray[i] = (float)((pdf_num *)a->values[i])->value.d;
    }
    pdfi_countdown(a);
    return a->entries;
}

int fill_float_array_from_dict(pdf_context *ctx, float *parray, int size, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries < size)
        return_error(gs_error_rangecheck);

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        if (a->values[i]->type == PDF_INT)
            parray[i] = (float)((pdf_num *)a->values[i])->value.i;
        else
            parray[i] = (float)((pdf_num *)a->values[i])->value.d;
    }
    pdfi_countdown(a);
    return a->entries;
}

int fill_bool_array_from_dict(pdf_context *ctx, bool *parray, int size, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries > size)
        return_error(gs_error_rangecheck);

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_BOOL) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        parray[i] = ((pdf_bool *)a->values[i])->value;
    }
    pdfi_countdown(a);
    return a->entries;
}

int fill_matrix_from_dict(pdf_context *ctx, float *parray, pdf_dict *dict)
{
    int code, i;
    pdf_array *a = NULL;

    code = pdfi_dict_get(ctx, dict, "Matrix", (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    if (a->entries != 6) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }
    if (a->entries != 6)
        return_error(gs_error_rangecheck);

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL) {
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        if (a->values[i]->type == PDF_INT)
            parray[i] = (float)((pdf_num *)a->values[i])->value.i;
        else
            parray[i] = (float)((pdf_num *)a->values[i])->value.d;
    }
    pdfi_countdown(a);
    return a->entries;
}

/* Returns < 0 for error or the number of entries allocated */
int make_float_array_from_dict(pdf_context *ctx, float **parray, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;
    float *arr = NULL;

    *parray = NULL;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }

    arr = (float *)gs_alloc_byte_array(ctx->memory, a->entries, sizeof(float), "array_from_dict_key");
    *parray = arr;

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_INT && a->values[i]->type != PDF_REAL) {
            gs_free_const_object(ctx->memory, arr, "float_array");
            *parray = NULL;
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        if (a->values[i]->type == PDF_INT)
            (*parray)[i] = (float)((pdf_num *)a->values[i])->value.i;
        else
            (*parray)[i] = (float)((pdf_num *)a->values[i])->value.d;
    }
    pdfi_countdown(a);
    return a->entries;
}

int make_int_array_from_dict(pdf_context *ctx, int **parray, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;
    int *arr = NULL;

    *parray = NULL;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    arr = (int *)gs_alloc_byte_array(ctx->memory, a->entries, sizeof(int), "array_from_dict_key");
    *parray = arr;

    for (i=0;i< a->entries;i++) {
        if (a->values[i]->type != PDF_INT) {
            gs_free_const_object(ctx->memory, arr, "int_array");
            *parray = NULL;
            pdfi_countdown(a);
            return_error(gs_error_typecheck);
        }
        (*parray)[i] = (int)((pdf_num *)a->values[i])->value.i;
    }
    pdfi_countdown(a);
    return a->entries;
}

int pdfi_dict_put(pdf_dict *d, pdf_obj *Key, pdf_obj *value)
{
    uint64_t i;
    pdf_obj **new_keys, **new_values;
    pdf_name *n;

    if (Key->type != PDF_NAME)
        return_error(gs_error_typecheck);

    /* First, do we have a Key/value pair already ? */
    for (i=0;i< d->entries;i++) {
        n = (pdf_name *)d->keys[i];
        if (n && n->type == PDF_NAME) {
            if (pdfi_name_cmp((pdf_name *)Key, n) == 0) {
                if (d->values[i] == value)
                    /* We already have this value stored with this key.... */
                    return 0;
                pdfi_countdown(d->values[i]);
                d->values[i] = value;
                pdfi_countup(value);
                return 0;
            }
        }
    }

    /* Nope, its a new Key */
    if (d->size > d->entries) {
        /* We have a hole, find and use it */
        for (i=0;i< d->size;i++) {
            if (d->keys[i] == NULL) {
                d->keys[i] = Key;
                pdfi_countup(Key);
                d->values[i] = value;
                pdfi_countup(value);
                d->entries++;
                return 0;
            }
        }
    }

    new_keys = (pdf_obj **)gs_alloc_bytes(d->memory, (d->size + 1) * sizeof(pdf_obj *), "pdfi_dict_put reallocate dictionary keys");
    new_values = (pdf_obj **)gs_alloc_bytes(d->memory, (d->size + 1) * sizeof(pdf_obj *), "pdfi_dict_put reallocate dictionary values");
    if (new_keys == NULL || new_values == NULL){
        gs_free_object(d->memory, new_keys, "pdfi_dict_put memory allocation failure");
        gs_free_object(d->memory, new_values, "pdfi_dict_put memory allocation failure");
        return_error(gs_error_VMerror);
    }
    memcpy(new_keys, d->keys, d->size * sizeof(pdf_obj *));
    memcpy(new_values, d->values, d->size * sizeof(pdf_obj *));

    gs_free_object(d->memory, d->keys, "pdfi_dict_put key reallocation");
    gs_free_object(d->memory, d->values, "pdfi_dict_put value reallocation");

    d->keys = new_keys;
    d->values = new_values;

    d->keys[d->size] = Key;
    d->values[d->size] = value;
    d->size++;
    d->entries++;
    pdfi_countup(Key);
    pdfi_countup(value);

    return 0;
}

int pdfi_dict_copy(pdf_dict *target, pdf_dict *source)
{
    int i=0, code = 0;

    for (i=0;i< source->entries;i++) {
        code = pdfi_dict_put(target, source->keys[i], source->values[i]);
        if (code < 0)
            return code;
    }
    return 0;
}

int pdfi_dict_known(pdf_dict *d, const char *Key, bool *known)
{
    int i;
    pdf_obj *t;

    *known = false;
    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_strcmp((pdf_name *)t, Key) == 0) {
                *known = true;
                break;
            }
        }
    }
    return 0;
}

int pdfi_dict_known_by_key(pdf_dict *d, pdf_name *Key, bool *known)
{
    int i;
    pdf_obj *t;

    *known = false;
    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_cmp((pdf_name *)t, Key) == 0) {
                *known = true;
                break;
            }
        }
    }
    return 0;
}

int pdfi_merge_dicts(pdf_dict *target, pdf_dict *source)
{
    int i, code;
    bool known = false;

    for (i=0;i< source->entries;i++) {
        code = pdfi_dict_known_by_key(target, (pdf_name *)source->keys[i], &known);
        if (code < 0)
            return code;
        if (!known) {
            code = pdfi_dict_put(target, source->keys[i], source->values[i]);
            if (code < 0)
                return code;
        }
    }
    return 0;
}

int pdfi_alloc_dict(pdf_context *ctx, uint64_t size, pdf_dict **returned)
{
    pdf_dict *returned_dict;

    *returned = NULL;

    returned_dict = (pdf_dict *)gs_alloc_bytes(ctx->memory, sizeof(pdf_dict), "pdfi_alloc_dict");
    if (returned_dict == NULL)
        return_error(gs_error_VMerror);

    memset(returned_dict, 0x00, sizeof(pdf_dict));
    returned_dict->memory = ctx->memory;
    returned_dict->type = PDF_DICT;
    returned_dict->refcnt = 1;

    returned_dict->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_alloc_dict");
    if (returned_dict->keys == NULL) {
        gs_free_object(ctx->memory, returned_dict, "pdfi_alloc_dict");
        return_error(gs_error_VMerror);
    }
    returned_dict->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_alloc_dict");
    if (returned_dict->keys == NULL) {
        gs_free_object(ctx->memory, returned_dict->keys, "pdfi_alloc_dict");
        gs_free_object(ctx->memory, returned_dict, "pdfi_alloc_dict");
        return_error(gs_error_VMerror);
    }
    memset(returned_dict->keys, 0x00, size * sizeof(pdf_obj *));
    memset(returned_dict->values, 0x00, size * sizeof(pdf_obj *));
    returned_dict->size = size;
    *returned = returned_dict;
#if REFCNT_DEBUG
    returned_dict->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated dictobject with UID %"PRIi64"\n", returned_dict->UID);
#endif
    return 0;
}
