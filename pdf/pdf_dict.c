/* Copyright (C) 2001-2020 Artifex Software, Inc.
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
#include "pdf_deref.h"
#include "pdf_dict.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "pdf_int.h"
#include "pdf_loop_detect.h"
#include "pdf_misc.h"

void pdfi_free_dict(pdf_obj *o)
{
    pdf_dict *d = (pdf_dict *)o;
    int i;
#ifdef DEBUG_DICT
    pdf_name *name;
#endif

    for (i=0;i < d->entries;i++) {
#ifdef DEBUG_DICT
        name = (pdf_name *)d->keys[i];
#endif
        if (d->values[i] != NULL)
            pdfi_countdown(d->values[i]);
        if (d->keys[i] != NULL)
            pdfi_countdown(d->keys[i]);
    }
    gs_free_object(OBJ_MEMORY(d), d->keys, "pdf interpreter free dictionary keys");
    gs_free_object(OBJ_MEMORY(d), d->values, "pdf interpreter free dictioanry values");
    gs_free_object(OBJ_MEMORY(d), d, "pdf interpreter free dictionary");
}

/* Delete a key pair, either by specifying a char * or a pdf_name *
 */
static int pdfi_dict_delete_inner(pdf_context *ctx, pdf_dict *d, pdf_name *n, const char *str)
{
    int i = 0;
#ifdef DEBUG_DICT
    pdf_name *name;
#endif

    for (i=0;i < d->entries;i++) {
#ifdef DEBUG_DICT
        name = (pdf_name *)d->keys[i];
#endif
        if (n != NULL) {
            if (pdfi_name_cmp(n, (pdf_name *)d->keys[i]) == 0)
                break;
        } else {
            if (pdfi_name_is((pdf_name *)d->keys[i], str))
                break;
        }

    }
    if (i >= d->entries)
        return_error(gs_error_undefined);

    pdfi_countdown(d->keys[i]);
    pdfi_countdown(d->values[i]);
    for(  ;i < d->entries - 1;i++) {
        d->keys[i] = d->keys[i + 1];
        d->values[i] = d->values[i + 1];
    }
    d->keys[i] = NULL;
    d->values[i] = NULL;
    d->entries--;
    return 0;
}

int pdfi_dict_delete_pair(pdf_context *ctx, pdf_dict *d, pdf_name *n)
{
    return pdfi_dict_delete_inner(ctx, d, n, NULL);
}

int pdfi_dict_delete(pdf_context *ctx, pdf_dict *d, const char *str)
{
    return pdfi_dict_delete_inner(ctx, d, NULL, str);
}

/* This function is provided for symmetry with arrays, and in case we ever
 * want to change the behaviour of pdfi_dict_from_stack() and pdfi_dict_alloc()
 * similarly to the array behaviour, where we always have null PDF objects
 * rather than NULL pointers stored in the dictionary.
 */
int pdfi_dict_alloc(pdf_context *ctx, uint64_t size, pdf_dict **d)
{
    *d = NULL;
    return pdfi_object_alloc(ctx, PDF_DICT, size, (pdf_obj **)d);
}

int pdfi_dict_from_stack(pdf_context *ctx, uint32_t indirect_num, uint32_t indirect_gen)
{
    uint64_t index = 0;
    pdf_dict *d = NULL;
    uint64_t i = 0;
    int code;
#ifdef DEBUG_DICT
    pdf_name *key;
#endif

    code = pdfi_count_to_mark(ctx, &index);
    if (code < 0) {
        pdfi_clear_to_mark(ctx);
        return code;
    }

    if (index & 1) {
        pdfi_clear_to_mark(ctx);
        return_error(gs_error_rangecheck);
    }

    code = pdfi_dict_alloc(ctx, index >> 1, &d);
    if (code < 0) {
        pdfi_clear_to_mark(ctx);
        return code;
    }

    d->entries = d->size;

    while (index) {
        i = (index / 2) - 1;

        /* In PDF keys are *required* to be names, so we ought to check that here */
        if (((pdf_obj *)ctx->stack_top[-2])->type == PDF_NAME) {
            d->keys[i] = ctx->stack_top[-2];
            pdfi_countup(d->keys[i]);
#ifdef DEBUG_DICT
            key = (pdf_name *)d->keys[i];
#endif
            d->values[i] = ctx->stack_top[-1];
            pdfi_countup(d->values[i]);
        } else {
            pdfi_free_dict((pdf_obj *)d);
            pdfi_clear_to_mark(ctx);
            return_error(gs_error_typecheck);
        }

        pdfi_pop(ctx, 2);
        index -= 2;
    }

    code = pdfi_clear_to_mark(ctx);
    if (code < 0) {
        pdfi_free_dict((pdf_obj *)d);
        return code;
    }

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, "\n >>\n");

    d->indirect_num = indirect_num;
    d->indirect_gen = indirect_gen;

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

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_is((pdf_name *)t, Key)) {
                if (d->values[i]->type == PDF_INDIRECT) {
                    pdf_indirect_ref *r = (pdf_indirect_ref *)d->values[i];

                    code = pdfi_deref_loop_detect(ctx, r->ref_object_num, r->ref_generation_num, o);
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

/* Get object from dict without resolving indirect references
 * Will inc refcnt by 1
 */
int pdfi_dict_get_no_deref(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o)
{
    int i=0;
    pdf_name *t;

    *o = NULL;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_cmp((pdf_name *)t, Key)== 0) {
                *o = d->values[i];
                pdfi_countup(*o);
                return 0;
            }
        }
    }
    return_error(gs_error_undefined);
}

/* Get by pdf_name rather than by char *
 * The object returned by pdfi_dict_get has its reference count incremented by 1 to
 * indicate the reference now held by the caller, in **o.
 */
int pdfi_dict_get_by_key(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o)
{
    int i=0, code;
    pdf_name *t;

    *o = NULL;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_cmp((pdf_name *)t, Key)== 0) {
                if (d->values[i]->type == PDF_INDIRECT) {
                    pdf_indirect_ref *r = (pdf_indirect_ref *)d->values[i];

                    code = pdfi_deref_loop_detect(ctx, r->ref_object_num, r->ref_generation_num, o);
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

/* Get indirect reference without de-referencing it */
int pdfi_dict_get_ref(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_indirect_ref **o)
{
    int i=0;
    pdf_name *t;

    *o = NULL;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_is((pdf_name *)t, Key)) {
                if (d->values[i]->type == PDF_INDIRECT) {
                    *o = (pdf_indirect_ref *)d->values[i];
                    pdfi_countup(*o);
                    return 0;
                } else {
                    return_error(gs_error_typecheck);
                }
            }
        }
    }
    return_error(gs_error_undefined);
}

/* As per pdfi_dict_get(), but doesn't replace an indirect reference in a dictionary with a
 * new object. This is for Resources following, such as Do, where we will have to seek and
 * read the indirect object anyway, and we need to ensure that Form XObjects (for example)
 * don't have circular calls.
 *
 * Takes either strKey or nameKey param. Other will be NULL.
 */
static int pdfi_dict_get_no_store_R_inner(pdf_context *ctx, pdf_dict *d, const char *strKey,
                                          const pdf_name *nameKey, pdf_obj **o)
{
    int i=0, code;
    pdf_name *t;
    bool match = false;

    *o = NULL;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (strKey != NULL) {
                if (pdfi_name_is(t, strKey))
                    match = true;
            } else {
                if (!pdfi_name_cmp(t, nameKey))
                    match = true;
            }
            if (match) {
                if (d->values[i]->type == PDF_INDIRECT) {
                    pdf_indirect_ref *r = (pdf_indirect_ref *)d->values[i];

                    code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, o);
                    if (code < 0)
                        return code;
                } else {
                    *o = d->values[i];
                    pdfi_countup(*o);
                }
                return 0;
            }
        }
    }
    return_error(gs_error_undefined);
}

/* Wrapper to pdfi_dict_no_store_R_inner(), takes a char * as Key */
int pdfi_dict_get_no_store_R(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o)
{
    return pdfi_dict_get_no_store_R_inner(ctx, d, Key, NULL, o);
}

/* Wrapper to pdfi_dict_no_store_R_inner(), takes a pdf_name * as Key */
int pdfi_dict_get_no_store_R_key(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o)
{
    return pdfi_dict_get_no_store_R_inner(ctx, d, NULL, Key, o);
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
        pdfi_countdown(*o);
        *o = NULL;
        return_error(gs_error_typecheck);
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

int pdfi_dict_get_number2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, double *f)
{
    int code;

    code = pdfi_dict_get_number(ctx, d, Key1, f);
    if (code == gs_error_undefined)
        code = pdfi_dict_get_number(ctx, d, Key2, f);
    return code;
}

int pdfi_dict_get_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f)
{
    int code;
    pdf_num *o;

    code = pdfi_dict_get(ctx, d, Key, (pdf_obj **)&o);
    if (code < 0)
        return code;
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
    double f;
    uint64_t array_size;

    code = pdfi_dict_get(ctx, dict, "Domain", (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    array_size = pdfi_array_size(a);
    if (array_size & 1 || array_size > size) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }

    for (i=0;i< array_size;i++) {
        code = pdfi_array_get_number(ctx, a, (uint64_t)i, &f);
        if (code < 0) {
            pdfi_countdown(a);
            return_error(code);
        }
        parray[i] = (float)f;
    }
    pdfi_countdown(a);
    return array_size;
}

int fill_float_array_from_dict(pdf_context *ctx, float *parray, int size, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;
    double f;
    uint64_t array_size;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }
    array_size = pdfi_array_size(a);
    if (array_size > size) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    for (i=0; i< array_size; i++) {
        code = pdfi_array_get_number(ctx, a, (uint64_t)i, &f);
        if (code < 0)
            goto exit;
        parray[i] = (float)f;
    }
    code = array_size;
 exit:
    pdfi_countdown(a);
    return code;
}

int fill_bool_array_from_dict(pdf_context *ctx, bool *parray, int size, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;
    pdf_bool *o;
    uint64_t array_size;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    array_size = pdfi_array_size(a);
    if (array_size > size)
        return_error(gs_error_rangecheck);

    for (i=0;i< array_size;i++) {
        code = pdfi_array_get_type(ctx, a, (uint64_t)i, PDF_BOOL, (pdf_obj **)&o);
        if (code < 0) {
            pdfi_countdown(a);
            return_error(code);
        }
        parray[i] = o->value;
        pdfi_countdown(o);
    }
    pdfi_countdown(a);
    return array_size;
}

int fill_matrix_from_dict(pdf_context *ctx, float *parray, pdf_dict *dict)
{
    int code, i;
    pdf_array *a = NULL;
    double f;
    uint64_t array_size;

    code = pdfi_dict_get(ctx, dict, "Matrix", (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    array_size = pdfi_array_size(a);
    if (array_size != 6) {
        pdfi_countdown(a);
        return_error(gs_error_rangecheck);
    }

    for (i=0; i< array_size; i++) {
        code = pdfi_array_get_number(ctx, a, (uint64_t)i, &f);
        if (code < 0) {
            pdfi_countdown(a);
            return_error(code);
        }
        parray[i] = (float)f;
    }
    pdfi_countdown(a);
    return array_size;
}

/* Returns < 0 for error or the number of entries allocated */
int pdfi_make_float_array_from_dict(pdf_context *ctx, float **parray, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;
    float *arr = NULL;
    double f;
    uint64_t array_size;

    *parray = NULL;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    array_size = pdfi_array_size(a);

    arr = (float *)gs_alloc_byte_array(ctx->memory, array_size,
                                       sizeof(float), "array_from_dict_key");
    *parray = arr;

    for (i=0;i< array_size;i++) {
        code = pdfi_array_get_number(ctx, a, (uint64_t)i, &f);
        if (code < 0) {
            gs_free_const_object(ctx->memory, arr, "float_array");
            *parray = NULL;
            pdfi_countdown(a);
            return_error(code);
        }
        (*parray)[i] = (float)f;
    }
    pdfi_countdown(a);
    return array_size;
}

int pdfi_make_int_array_from_dict(pdf_context *ctx, int **parray, pdf_dict *dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;
    int *arr = NULL;
    pdf_num *o;
    uint64_t array_size;

    *parray = NULL;

    code = pdfi_dict_get(ctx, dict, Key, (pdf_obj **)&a);
    if (code < 0)
        return code;
    if (a->type != PDF_ARRAY) {
        pdfi_countdown(a);
        return_error(gs_error_typecheck);
    }
    array_size = pdfi_array_size(a);
    arr = (int *)gs_alloc_byte_array(ctx->memory, array_size,
                                     sizeof(int), "array_from_dict_key");
    *parray = arr;

    for (i=0;i< array_size;i++) {
        code = pdfi_array_get_type(ctx, a, (uint64_t)i, PDF_INT, (pdf_obj **)&o);
        if (code < 0) {
            gs_free_const_object(ctx->memory, arr, "int_array");
            *parray = NULL;
            pdfi_countdown(a);
            return_error(code);
        }
        (*parray)[i] = (int)o->value.i;
        pdfi_countdown(o);
    }
    pdfi_countdown(a);
    return array_size;
}

/* Put into dictionary with key as object */
int pdfi_dict_put_obj(pdf_context *ctx, pdf_dict *d, pdf_obj *Key, pdf_obj *value)
{
    uint64_t i;
    pdf_obj **new_keys, **new_values;
    pdf_name *n;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

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

    new_keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, (d->size + 1) * sizeof(pdf_obj *), "pdfi_dict_put reallocate dictionary keys");
    new_values = (pdf_obj **)gs_alloc_bytes(ctx->memory, (d->size + 1) * sizeof(pdf_obj *), "pdfi_dict_put reallocate dictionary values");
    if (new_keys == NULL || new_values == NULL){
        gs_free_object(ctx->memory, new_keys, "pdfi_dict_put memory allocation failure");
        gs_free_object(ctx->memory, new_values, "pdfi_dict_put memory allocation failure");
        return_error(gs_error_VMerror);
    }
    memcpy(new_keys, d->keys, d->size * sizeof(pdf_obj *));
    memcpy(new_values, d->values, d->size * sizeof(pdf_obj *));

    gs_free_object(ctx->memory, d->keys, "pdfi_dict_put key reallocation");
    gs_free_object(ctx->memory, d->values, "pdfi_dict_put value reallocation");

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

/* Put into dictionary with key as string */
int pdfi_dict_put(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj *value)
{
    int code;
    pdf_obj *key = NULL;

    code = pdfi_name_alloc(ctx, (byte *)Key, strlen(Key), &key);
    if (code < 0)
        return code;
    pdfi_countup(key);

    code = pdfi_dict_put_obj(ctx, d, key, value);
    pdfi_countdown(key); /* get rid of extra ref */
    return code;
}

int pdfi_dict_put_int(pdf_context *ctx, pdf_dict *d, const char *key, int64_t value)
{
    int code;
    pdf_num *obj;

    code = pdfi_object_alloc(ctx, PDF_INT, 0, (pdf_obj **)&obj);
    obj->value.i = value;
    if (code < 0)
        return code;

    return pdfi_dict_put(ctx, d, key, (pdf_obj *)obj);
}

int pdfi_dict_put_bool(pdf_context *ctx, pdf_dict *d, const char *key, bool value)
{
    int code;
    pdf_bool *obj = NULL;

    code = pdfi_object_alloc(ctx, PDF_BOOL, 0, (pdf_obj **)&obj);
    if (code < 0)
        return code;

    obj->value = value;
    return pdfi_dict_put(ctx, d, key, (pdf_obj *)obj);
}

int pdfi_dict_put_name(pdf_context *ctx, pdf_dict *d, const char *key, const char *name)
{
    int code;
    pdf_obj *obj = NULL;

    code = pdfi_name_alloc(ctx, (byte *)name, strlen(name), &obj);
    if (code < 0)
        return code;
    pdfi_countup(obj);

    code = pdfi_dict_put(ctx, d, key, obj);
    pdfi_countdown(obj); /* get rid of extra ref */
    return code;
}

int pdfi_dict_copy(pdf_context *ctx, pdf_dict *target, pdf_dict *source)
{
    int i=0, code = 0;

    for (i=0;i< source->entries;i++) {
        code = pdfi_dict_put_obj(ctx, target, source->keys[i], source->values[i]);
        if (code < 0)
            return code;
    }
    return 0;
}

int pdfi_dict_known(pdf_context *ctx, pdf_dict *d, const char *Key, bool *known)
{
    int i;
    pdf_name *t;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    *known = false;
    for (i=0;i< d->entries;i++) {
        t = (pdf_name *)d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (pdfi_name_is(t, Key)) {
                *known = true;
                break;
            }
        }
    }
    return 0;
}

/* Tests if a Key is present in the dictionary, if it is, retrieves the value associted with the
 * key. Returns < 0 for error, 0 if the key is not found > 0 if the key is present, and initialises
 * the value in the arguments. Since this uses pdf_dict_get(), the returned value has its
 * reference count incremented by 1, just like pdfi_dict_get().
 */
int pdfi_dict_knownget(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o)
{
    bool known = false;
    int code;

    code = pdfi_dict_known(ctx, d, Key, &known);
    if (code < 0)
        return code;

    if (known == false)
        return 0;

    code = pdfi_dict_get(ctx, d, Key, o);
    if (code < 0)
        return code;

    return 1;
}

/* Like pdfi_dict_knownget() but allows the user to specify a type for the object that we get.
 * returns < 0 for error (including typecheck if the object is not the requested type)
 * 0 if the key is not found, or > 0 if the key was found and returned.
 */
int pdfi_dict_knownget_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o)
{
    bool known = false;
    int code;

    code = pdfi_dict_known(ctx, d, Key, &known);
    if (code < 0)
        return code;

    if (known == false)
        return 0;

    code = pdfi_dict_get_type(ctx, d, Key, type, o);
    if (code < 0)
        return code;

    return 1;
}

/* Like pdfi_dict_knownget_type() but retrieves numbers (two possible types)
 */
int pdfi_dict_knownget_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f)
{
    bool known = false;
    int code;

    code = pdfi_dict_known(ctx, d, Key, &known);
    if (code < 0)
        return code;

    if (known == false)
        return 0;

    code = pdfi_dict_get_number(ctx, d, Key, f);
    if (code < 0)
        return code;

    return 1;
}

int pdfi_dict_known_by_key(pdf_context *ctx, pdf_dict *d, pdf_name *Key, bool *known)
{
    int i;
    pdf_obj *t;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

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

int pdfi_dict_next(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, pdf_obj **Value, uint64_t *index)
{
    int code;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    while (1) {
        if (*index >= d->entries) {
            *Key = NULL;
            *Value= NULL;
            return gs_error_undefined;
        }

        /* If we find NULL keys skip over them. This should never
         * happen as we check the number of entries above, and we
         * compact dictionaries on deletion of key/value pairs.
         * This is a belt and braces check in case creation of the
         * dictionary somehow ends up with NULL keys in the allocated
         * section.
         */
        *Key = d->keys[*index];
        if (*Key == NULL) {
            (*index)++;
            continue;
        }

        if (d->values[*index]->type == PDF_INDIRECT) {
            pdf_indirect_ref *r = (pdf_indirect_ref *)d->values[*index];
            pdf_obj *o;

            code = pdfi_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o);
            if (code < 0) {
                *Key = *Value = NULL;
                return code;
            }
            *Value = o;
            break;
        } else {
            *Value = d->values[*index];
            pdfi_countup(*Value);
            break;
        }
    }

    pdfi_countup(*Key);
    (*index)++;
    return 0;
}

int pdfi_dict_first(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, pdf_obj **Value, uint64_t *index)
{
    uint64_t *i = index;

    *i = 0;
    return pdfi_dict_next(ctx, d, Key, Value, index);
}

int pdfi_dict_key_next(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, uint64_t *index)
{
    uint64_t *i = index;

    if (d->type != PDF_DICT)
        return_error(gs_error_typecheck);

    while (1) {
        if (*i >= d->entries) {
            *Key = NULL;
            return gs_error_undefined;
        }

        *Key = d->keys[*i];
        if (*Key == NULL) {
            (*i)++;
            continue;
        }
        pdfi_countup(*Key);
        (*i)++;
        break;
    }
    return 0;
}

int pdfi_dict_key_first(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, uint64_t *index)
{
    uint64_t *i = index;

    *i = 0;
    return pdfi_dict_key_next(ctx, d, Key, index);
}

int pdfi_merge_dicts(pdf_context *ctx, pdf_dict *target, pdf_dict *source)
{
    int i, code;
    bool known = false;

    for (i=0;i< source->entries;i++) {
        code = pdfi_dict_known_by_key(ctx, target, (pdf_name *)source->keys[i], &known);
        if (code < 0)
            return code;
        if (!known) {
            code = pdfi_dict_put_obj(ctx, target, source->keys[i], source->values[i]);
            if (code < 0)
                return code;
        }
    }
    return 0;
}

/* Return Length of a stream, or 0 if it's not a stream
 * Caches the Length
 */
int64_t pdfi_stream_length(pdf_context *ctx, pdf_stream *stream)
{
    int64_t Length = 0;
    int code;

    if (stream->type != PDF_STREAM)
        return 0;

    if (stream->length_valid)
        return stream->Length;

    code = pdfi_dict_get_int(ctx, stream->stream_dict, "Length", &Length);
    if (code < 0)
        Length = 0;

    /* Make sure Length is not negative... */
    if (Length < 0)
        Length = 0;

    /* Cache it */
    stream->Length = Length;
    stream->length_valid = true;

    return 0;
}

/* Safely get offset from a stream object.
 * If it's not actually a stream, just return 0.
 */
gs_offset_t pdfi_stream_offset(pdf_context *ctx, pdf_stream *stream)
{
    if (stream->type != PDF_STREAM)
        return 0;
    return stream->stream_offset;
}

pdf_stream *pdfi_stream_parent(pdf_context *ctx, pdf_stream *stream)
{
    if (stream->type != PDF_STREAM)
        return 0;
    return (pdf_stream *)stream->parent_obj;
}

void pdfi_set_stream_parent(pdf_context *ctx, pdf_stream *stream, pdf_stream *parent)
{
    /* Ordinarily we would increment the reference count of the parent object here,
     * because we are taking a new reference to it. But if we do that we will end up
     * with circular references and will never count down and release the objects.
     * This is because the parent object must have a Resources dictionary which
     * references this stream, when we dereference the stream we store it in the
     * Parent's Resources dictionary. So the parent points to the child, the child
     * points to the parent and we always end up with a refcnt for each of 1. Since we
     * only ever consult parent_obj in an illegal case we deal with this by not
     * incrementing the reference count. To try and avoid any dangling references
     * we clear the parent_obj when we finish executing the stream in
     * pdfi_interpret_content_stream.
     */
    stream->parent_obj = (pdf_obj *)parent;
}

void pdfi_clear_stream_parent(pdf_context *ctx, pdf_stream *stream)
{
    stream->parent_obj = NULL;
}

/* Get the dict from a pdf_obj, returns typecheck if it doesn't have one */
int pdfi_dict_from_obj(pdf_context *ctx, pdf_obj *obj, pdf_dict **dict)
{
    *dict = NULL;
    if (obj->type == PDF_DICT)
        *dict = (pdf_dict *)obj;
    else if (obj->type == PDF_STREAM)
        *dict = ((pdf_stream *)obj)->stream_dict;
    else
        return_error(gs_error_typecheck);
    return 0;
}
