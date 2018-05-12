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

void pdf_free_dict(pdf_obj *o)
{
    pdf_dict *d = (pdf_dict *)o;
    int i;

    for (i=0;i < d->entries;i++) {
        if (d->keys[i] != NULL)
            pdf_countdown(d->keys[i]);
        if (d->values[i] != NULL)
            pdf_countdown(d->values[i]);
    }
    gs_free_object(d->memory, d->keys, "pdf interpreter free dictionary keys");
    gs_free_object(d->memory, d->values, "pdf interpreter free dictioanry values");
    gs_free_object(d->memory, d, "pdf interpreter free dictionary");
}

int pdf_dict_from_stack(pdf_context *ctx)
{
    uint64_t index = 0;
    pdf_dict *d = NULL;
    uint64_t i = 0;
    int code;

    code = pdf_count_to_mark(ctx, &index);
    if (code < 0)
        return code;

    if (index & 1)
        return_error(gs_error_rangecheck);

    d = (pdf_dict *)gs_alloc_bytes(ctx->memory, sizeof(pdf_dict), "pdf_dict_from_stack");
    if (d == NULL)
        return_error(gs_error_VMerror);

    memset(d, 0x00, sizeof(pdf_dict));
    d->memory = ctx->memory;
    d->type = PDF_DICT;

    d->size = d->entries = index >> 1;

    d->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_dict_from_stack");
    if (d->keys == NULL) {
        gs_free_object(d->memory, d, "pdf_read_dict error");
        return_error(gs_error_VMerror);
    }
    memset(d->keys, 0x00, d->size * sizeof(pdf_obj *));

    d->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_dict_from_stack");
    if (d->values == NULL) {
        gs_free_object(d->memory, d->keys, "pdf_read_dict error");
        gs_free_object(d->memory, d, "pdf_read_dict error");
        return_error(gs_error_VMerror);
    }
    memset(d->values, 0x00, d->size * sizeof(pdf_obj *));

    while (index) {
        i = (index / 2) - 1;

        /* In PDF keys are *required* to be names, so we ought to check that here */
        if (((pdf_obj *)ctx->stack_top[-2])->type == PDF_NAME) {
            d->keys[i] = ctx->stack_top[-2];
            pdf_countup(d->keys[i]);
            d->values[i] = ctx->stack_top[-1];
            pdf_countup(d->values[i]);
        } else {
            pdf_free_dict((pdf_obj *)d);
            return_error(gs_error_typecheck);
        }

        pdf_pop(ctx, 2);
        index -= 2;
    }

    code = pdf_clear_to_mark(ctx);
    if (code < 0)
        return code;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, "\n >>\n");

#if REFCNT_DEBUG
    d->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated dictionary object with UID %"PRIi64"\n", d->UID);
#endif
    code = pdf_push(ctx, (pdf_obj *)d);
    if (code < 0)
        pdf_free_dict((pdf_obj *)d);

    return code;
}


/* The object returned by pdf_dict_get has its reference count incremented by 1 to
 * indicate the reference now held by the caller, in **o.
 */
int pdf_dict_get(pdf_dict *d, const char *Key, pdf_obj **o)
{
    int i=0;
    pdf_obj *t;

    *o = NULL;

    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (((pdf_name *)t)->length == strlen((const char *)Key) && memcmp((const char *)((pdf_name *)t)->data, (const char *)Key, ((pdf_name *)t)->length) == 0) {
                *o = d->values[i];
                pdf_countup(*o);
                return 0;
            }
        }
    }
    return_error(gs_error_undefined);
}

int pdf_dict_get_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdf_dict_get(d, Key, o);
    if (code < 0)
        return code;

    if ((*o)->type != type) {
        if ((*o)->type == PDF_INDIRECT){
            pdf_name *NewKey = NULL;
            pdf_obj *o1 = NULL;
            pdf_indirect_ref *r = (pdf_indirect_ref *)*o;

            code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
            pdf_countdown(*o);
            *o = NULL;
            if (code < 0)
                return code;

            code = pdf_make_name(ctx, (byte *)Key, strlen(Key), (pdf_obj **)&NewKey);
            if (code == 0) {
                (void)pdf_dict_put(d, (pdf_obj *)NewKey, o1);
                pdf_countdown(NewKey);
            }
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

int pdf_dict_get_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i)
{
    int code;
    pdf_num *n;

    code = pdf_dict_get_type(ctx, d, Key, PDF_INT, (pdf_obj **)&n);
    if (code < 0)
        return code;

    *i = n->value.i;
    pdf_countdown(n);
    return 0;
}

int pdf_dict_get_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f)
{
    int code;
    pdf_num *o;

    code = pdf_dict_get(d, Key, (pdf_obj **)&o);
    if (code < 0)
        return code;
    if (o->type == PDF_INDIRECT) {
        pdf_obj *o1 = NULL;
        pdf_indirect_ref *r = (pdf_indirect_ref *)o;

        code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
        pdf_countdown(o);
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
            pdf_countdown(o);
            return_error(gs_error_typecheck);
        }
    }
    pdf_countdown(o);
    return 0;
}

int pdf_dict_put(pdf_dict *d, pdf_obj *Key, pdf_obj *value)
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
            if (n->length == ((pdf_name *)Key)->length && memcmp((const char *)n->data, ((pdf_name *)Key)->data, n->length) == 0) {
                if (d->values[i] == value)
                    /* We already have this value stored with this key.... */
                    return 0;
                pdf_countdown(d->values[i]);
                d->values[i] = value;
                pdf_countup(value);
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
                pdf_countup(Key);
                d->values[i] = value;
                pdf_countup(value);
                d->entries++;
                return 0;
            }
        }
    }

    new_keys = (pdf_obj **)gs_alloc_bytes(d->memory, (d->size + 1) * sizeof(pdf_obj *), "pdf_dict_put reallocate dictionary keys");
    new_values = (pdf_obj **)gs_alloc_bytes(d->memory, (d->size + 1) * sizeof(pdf_obj *), "pdf_dict_put reallocate dictionary values");
    if (new_keys == NULL || new_values == NULL){
        gs_free_object(d->memory, new_keys, "pdf_dict_put memory allocation failure");
        gs_free_object(d->memory, new_values, "pdf_dict_put memory allocation failure");
        return_error(gs_error_VMerror);
    }
    memcpy(new_keys, d->keys, d->size * sizeof(pdf_obj *));
    memcpy(new_values, d->values, d->size * sizeof(pdf_obj *));

    gs_free_object(d->memory, d->keys, "pdf_dict_put key reallocation");
    gs_free_object(d->memory, d->values, "pdf_dict_put value reallocation");

    d->keys = new_keys;
    d->values = new_values;

    d->keys[d->size] = Key;
    d->values[d->size] = value;
    d->size++;
    d->entries++;
    pdf_countup(Key);
    pdf_countup(value);

    return 0;
}

int pdf_dict_copy(pdf_dict *target, pdf_dict *source)
{
    int i=0, code = 0;

    for (i=0;i< source->entries;i++) {
        code = pdf_dict_put(target, source->keys[i], source->values[i]);
        if (code < 0)
            return code;
    }
    return 0;
}

int pdf_dict_known(pdf_dict *d, const char *Key, bool *known)
{
    int i;
    pdf_obj *t;

    *known = false;
    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (((pdf_name *)t)->length == strlen(Key) && memcmp(((pdf_name *)t)->data, Key, ((pdf_name *)t)->length) == 0) {
                *known = true;
                break;
            }
        }
    }
    return 0;
}

int pdf_dict_known_by_key(pdf_dict *d, pdf_name *Key, bool *known)
{
    int i;
    pdf_obj *t;

    *known = false;
    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (((pdf_name *)t)->length == Key->length && memcmp((const char *)((pdf_name *)t)->data, Key->data, ((pdf_name *)t)->length) == 0) {
                *known = true;
                break;
            }
        }
    }
    return 0;
}

int pdf_merge_dicts(pdf_dict *target, pdf_dict *source)
{
    int i, code;
    bool known = false;

    for (i=0;i< source->entries;i++) {
        code = pdf_dict_known_by_key(target, (pdf_name *)source->keys[i], &known);
        if (code < 0)
            return code;
        if (!known) {
            code = pdf_dict_put(target, source->keys[i], source->values[i]);
            if (code < 0)
                return code;
        }
    }
    return 0;
}

int pdf_alloc_dict(pdf_context *ctx, uint64_t size, pdf_dict **returned)
{
    pdf_dict *returned_dict;

    *returned = NULL;

    returned_dict = (pdf_dict *)gs_alloc_bytes(ctx->memory, sizeof(pdf_dict), "pdf_alloc_dict");
    if (returned_dict == NULL)
        return_error(gs_error_VMerror);

    memset(returned_dict, 0x00, sizeof(pdf_dict));
    returned_dict->memory = ctx->memory;
    returned_dict->type = PDF_DICT;
    returned_dict->refcnt = 1;

    returned_dict->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdf_alloc_dict");
    if (returned_dict->keys == NULL) {
        gs_free_object(ctx->memory, returned_dict, "pdf_alloc_dict");
        return_error(gs_error_VMerror);
    }
    returned_dict->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdf_alloc_dict");
    if (returned_dict->keys == NULL) {
        gs_free_object(ctx->memory, returned_dict->keys, "pdf_alloc_dict");
        gs_free_object(ctx->memory, returned_dict, "pdf_alloc_dict");
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
