/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Support for parameter lists */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsstruct.h"

/* GC procedures */
ENUM_PTRS_WITH(gs_param_typed_value_enum_ptrs, gs_param_typed_value *pvalue) return 0;
    case 0:
    switch (pvalue->type) {
    case gs_param_type_string:
        return ENUM_STRING(&pvalue->value.s);
    case gs_param_type_name:
        return ENUM_STRING(&pvalue->value.n);
    case gs_param_type_int_array:
        return ENUM_OBJ(pvalue->value.ia.data);
    case gs_param_type_float_array:
        return ENUM_OBJ(pvalue->value.fa.data);
    case gs_param_type_string_array:
        return ENUM_OBJ(pvalue->value.sa.data);
    case gs_param_type_name_array:
        return ENUM_OBJ(pvalue->value.na.data);
    default:
        return ENUM_OBJ(0);	/* don't stop early */
    }
ENUM_PTRS_END
RELOC_PTRS_WITH(gs_param_typed_value_reloc_ptrs, gs_param_typed_value *pvalue) {
    switch (pvalue->type) {
    case gs_param_type_string:
    case gs_param_type_name: {
        gs_const_string str;

        str.data = pvalue->value.s.data; /* n == s */
        str.size = pvalue->value.s.size;
        RELOC_CONST_STRING_VAR(str);
        pvalue->value.s.data = str.data;
        break;
    }
    case gs_param_type_int_array:
        RELOC_VAR(pvalue->value.ia.data);
        break;
    case gs_param_type_float_array:
        RELOC_VAR(pvalue->value.fa.data);
        break;
    case gs_param_type_string_array:
        RELOC_VAR(pvalue->value.sa.data);
        break;
    case gs_param_type_name_array:
        RELOC_VAR(pvalue->value.na.data);
        break;
    default:
        break;
    }
}
RELOC_PTRS_END

/* Internal procedure to initialize the common part of a parameter list. */
void
gs_param_list_init(gs_param_list *plist, const gs_param_list_procs *procs,
                   gs_memory_t *mem)
{
    plist->procs = procs;
    plist->memory = mem;
    plist->persistent_keys = true;
}

/* Set whether the keys for param_write_XXX are persistent. */
void
gs_param_list_set_persistent_keys(gs_param_list *plist, bool persistent)
{
    plist->persistent_keys = persistent;
}

/* Reset a gs_param_key_t enumerator to its initial state */
void
param_init_enumerator(gs_param_enumerator_t * enumerator)
{
    memset(enumerator, 0, sizeof(*enumerator));
}

/* Transfer a collection of parameters. */
static const byte xfer_item_sizes[] = {
    GS_PARAM_TYPE_SIZES(0)
};
int
gs_param_read_items(gs_param_list * plist, void *obj,
                    const gs_param_item_t * items,
                    gs_memory_t *mem)
{
    const gs_param_item_t *pi;
    int ecode = 0;

    for (pi = items; pi->key != 0; ++pi) {
        const char *key = pi->key;
        void *pvalue = (void *)((char *)obj + pi->offset);
        gs_param_typed_value typed;
        int code;

        typed.type = pi->type;
        code = param_read_requested_typed(plist, key, &typed);
        switch (code) {
            default:		/* < 0 */
                ecode = code;
            case 1:
                break;
            case 0:
                if (typed.type != pi->type)	/* shouldn't happen! */
                    ecode = gs_note_error(gs_error_typecheck);
                else {
                    switch(typed.type)
                    {
                    case gs_param_type_dict:
                    case gs_param_type_dict_int_keys:
                    case gs_param_type_array:
                        return_error(gs_error_rangecheck);
                    case gs_param_type_string:
                    case gs_param_type_name:
                    {
                        void *copy;
                        gs_string *s;
                        if (mem == NULL) {
                            /* Return pointers to the data in the param list. This
                             * means that if the caller wants to keep it around it
                             * needs to copy it itself, or run the risk of the
                             * param list going away. */
                            goto copy_pointer;
                        }
                        /* Free any existing data before copying into it. */
                        s = ((gs_string *)pvalue);
                        if (typed.value.s.size != s->size) {
                            gs_free_string(mem, s->data, s->size, "gs_param_read_items");
                            s->data = NULL;
                            s->size = 0;
                            copy = gs_alloc_string(mem, typed.value.s.size, "gs_param_read_items");
                            if (copy == NULL)
                                return_error(gs_error_VMerror);
                            s->size = typed.value.s.size;
                        } else {
                            copy = s->data;
                        }
                        memcpy(copy, typed.value.s.data, typed.value.s.size);
                        s->data = copy;
                        ((gs_param_string *)pvalue)->persistent = 0; /* 0 => We own this copy */
                        break;
                    }
                    case gs_param_type_int_array:
                    case gs_param_type_float_array:
                    case gs_param_type_string_array:
                    case gs_param_type_name_array:
                    {
                        int eltsize;
                        gs_param_string_array *sa;
                        if (mem == NULL) {
                            /* Return pointers to the data in the param list. This
                             * means that if the caller wants to keep it around it
                             * needs to copy it itself, or run the risk of the
                             * param list going away. */
                            goto copy_pointer;
                        }
                        /* Free any existing data before copying into it. */
                        eltsize = gs_param_type_base_sizes[typed.type];
                        sa = ((gs_param_string_array *)pvalue);
                        if (typed.value.ia.size != sa->size) {
                            void *copy;
                            if (typed.type == gs_param_type_name_array ||
                                typed.type == gs_param_type_string_array) {
                                /* Free the strings. */
                                int i;
                                gs_param_string *arr;
                                union { const gs_param_string *cs; gs_param_string *s; } u;
                                u.cs = sa->data;
                                arr = u.s; /* Hideous dodge to avoid the const. */
                                for (i = 0; i < typed.value.sa.size; i++) {
                                    /* Hideous hackery to get around the const nature of gs_param_strings. */
                                    gs_string *arr_non_const = (gs_string *)(void *)(&arr[i]);
                                    if (arr[i].persistent == 0)
                                        gs_free_string(mem, arr_non_const->data, arr_non_const->size, "gs_param_read_items");
                                    arr_non_const->data = NULL;
                                    arr_non_const->size = 0;
                                }
                            }
                            gs_free_const_object(mem, sa->data, "gs_param_read_items");
                            sa->data = NULL;
                            sa->size = 0;
                            copy = gs_alloc_bytes(mem, eltsize * typed.value.s.size, "gs_param_read_items");
                            if (copy == NULL)
                                return_error(gs_error_VMerror);
                            memset(copy, 0, eltsize * typed.value.s.size);
                            sa->size = typed.value.s.size;
                            sa->data = copy;
                        }
                        /* Now copy the elements of the arrays. */
                        if (typed.type == gs_param_type_name_array ||
                            typed.type == gs_param_type_string_array) {
                            /* Free the strings. */
                            int i;
                            const gs_param_string *src = typed.value.sa.data;
                            gs_param_string *dst;
                            union { const gs_param_string *cs; gs_param_string *s; } u;
                            u.cs = sa->data;
                            dst = u.s; /* Hideous dodge to avoid the const. */
                            for (i = 0; i < typed.value.sa.size; i++) {
                                /* Hideous hackery to get around the const nature of gs_param_strings. */
                                gs_string *dst_non_const = (gs_string *)(void *)(&dst[i]);
                                if (dst[i].persistent == 0)
                                    gs_free_string(mem, dst_non_const->data, dst_non_const->size, "gs_param_read_items");
                                dst_non_const->data = NULL;
                                dst_non_const->size = 0;
                            }
                            /* Copy values */
                            for (i = 0; i < sa->size; i++) {
                                dst[i].data = gs_alloc_string(mem, src[i].size, "gs_param_read_items");
                                if (dst[i].data == NULL)
                                    return_error(gs_error_VMerror);
                                dst[i].size = src[i].size;
                                dst[i].persistent = 0; /* 0 => We own this copy */
                            }
                        } else {
                            /* Hideous hackery to get around the const nature of gs_param_strings. */
                            gs_string *s = (gs_string *)(void *)sa;
                            memcpy(s->data, typed.value.s.data, eltsize * typed.value.s.size);
                        }
                        ((gs_param_string *)pvalue)->persistent = 0; /* 0 => We own this copy */
                        break;
                    }
                    default:
    copy_pointer:
                        memcpy(pvalue, &typed.value, xfer_item_sizes[pi->type]);
                    }
                }
        }
    }
    return ecode;
}
int
gs_param_write_items(gs_param_list * plist, const void *obj,
                     const void *default_obj, const gs_param_item_t * items)
{
    const gs_param_item_t *pi;
    int ecode = 0;

    for (pi = items; pi->key != 0; ++pi) {
        const char *key = pi->key;
        const void *pvalue = (const void *)((const char *)obj + pi->offset);
        int size = xfer_item_sizes[pi->type];
        gs_param_typed_value typed;
        int code;

        if (default_obj != 0 &&
            !memcmp((const void *)((const char *)default_obj + pi->offset),
                    pvalue, size)
            )
            continue;
        memcpy(&typed.value, pvalue, size);
        typed.type = pi->type;
        /* Ensure the list doesn't end up keeping a pointer to our values. */
        typed.value.s.persistent = 0;
        code = (*plist->procs->xmit_typed) (plist, key, &typed);
        if (code < 0)
            ecode = code;
    }
    return ecode;
}

/* Read a value, with coercion if requested, needed, and possible. */
/* If mem != 0, we can coerce int arrays to float arrays. */
int
param_coerce_typed(gs_param_typed_value * pvalue, gs_param_type req_type,
                   gs_memory_t * mem)
{
    if (req_type == gs_param_type_any || pvalue->type == req_type)
        return 0;
    /*
     * Look for coercion opportunities.  It would be wonderful if we
     * could convert int/float arrays and name/string arrays, but
     * right now we can't.  However, a 0-length heterogenous array
     * will satisfy a request for any specific type.
     */
    /* Strictly speaking assigning one element of union
     * to another, overlapping element of a different size is
     * undefined behavior, hence assign to intermediate variables
     */
    switch (pvalue->type /* actual type */ ) {
        case gs_param_type_int:
            switch (req_type) {
                case gs_param_type_i64:
                {
                    int64_t i64 = (int64_t)pvalue->value.i;
                    pvalue->value.i64 = i64;
                    goto ok;
                }
                case gs_param_type_size_t:
                {
                    size_t z = (size_t)pvalue->value.i;
                    if (pvalue->value.i < 0)
                        return gs_error_rangecheck;
                    pvalue->value.z = z;
                    goto ok;
                }
                case gs_param_type_long:
                {
                    long l = (long)pvalue->value.i;
                    pvalue->value.l = l;
                    goto ok;
                }
                case gs_param_type_float:
                {
                    float fl = (float)pvalue->value.i;
                    pvalue->value.f = fl;
                    goto ok;
                }
                default:
                    break;
            }
            break;
        case gs_param_type_long:
            switch (req_type) {
                case gs_param_type_i64:
                {
                    int64_t i64 = (int64_t)pvalue->value.l;
                    pvalue->value.i64 = i64;
                    goto ok;
                }
                case gs_param_type_size_t:
                {
                    size_t z = (size_t)pvalue->value.l;
                    if (pvalue->value.l < 0
#if ARCH_SIZEOF_SIZE_T < ARCH_SIZEOF_LONG
                        || pvalue->value.l != (long)z
#endif
                        )
                        return_error(gs_error_rangecheck);
                    pvalue->value.z = z;
                    goto ok;
                }
                case gs_param_type_int:
                {
                    int int1 = (int)pvalue->value.l;
#if ARCH_SIZEOF_INT < ARCH_SIZEOF_LONG
                    if (pvalue->value.l != (long)int1)
                        return_error(gs_error_rangecheck);
#endif
                    pvalue->value.i = int1;
                    goto ok;
                }
                case gs_param_type_float:
                {
                    float fl = (float)pvalue->value.l;
                    pvalue->value.f = fl;
                    goto ok;
                }
                default:
                    break;
            }
            break;
        case gs_param_type_i64:
            switch (req_type) {
                case gs_param_type_size_t:
                {
                    size_t z = (size_t)pvalue->value.i64;
                    if (pvalue->value.i64 < 0
#if ARCH_SIZEOF_SIZE_T < 8 /* sizeof(int64_t) */
                        || pvalue->value.i64 != (int64_t)z
#endif
                        )
                        return_error(gs_error_rangecheck);
                    pvalue->value.z = z;
                    goto ok;
                }
                case gs_param_type_long:
                {
                    long l = (long)pvalue->value.i64;
#if ARCH_SIZEOF_LONG < 8 /* sizeof(int64_t) */
                    if (pvalue->value.i64 != (int64_t)l)
                        return_error(gs_error_rangecheck);
#endif
                    pvalue->value.l = l;
                    goto ok;
                }
                case gs_param_type_int:
                {
                    int int1 = (int)pvalue->value.i64;
#if ARCH_SIZEOF_INT < 8 /* sizeof(int64_t) */
                    if (pvalue->value.i64 != (int64_t)int1)
                        return_error(gs_error_rangecheck);
#endif
                    pvalue->value.i = int1;
                    goto ok;
                }
                case gs_param_type_float:
                {
                    float fl = (float)pvalue->value.i64;
                    pvalue->value.f = fl;
                    goto ok;
                }
                default:
                    break;
            }
            break;
        case gs_param_type_size_t:
            switch (req_type) {
                case gs_param_type_i64:
                {
                    int64_t i64 = (int64_t)pvalue->value.z;
                    if (i64 < 0
#if 8 /* sizeof(int64_t) */ < ARCH_SIZEOF_SIZE_T
                        /* Unlikely, but let's plan for the day when we need 128bit addressing :) */
                        || pvalue->value.z != (size_t)i64
#endif
                        )
                        return_error(gs_error_rangecheck);
                    pvalue->value.i64 = i64;
                    goto ok;
                }
                case gs_param_type_long:
                {
                    long l = (long)pvalue->value.z;
#if ARCH_SIZEOF_LONG < 8 /* sizeof(int64_t) */
                    if (pvalue->value.z != (size_t)l)
                        return_error(gs_error_rangecheck);
#endif
                    pvalue->value.l = l;
                    goto ok;
                }
                case gs_param_type_int:
                {
                    int int1 = (int)pvalue->value.z;
#if ARCH_SIZEOF_INT < 8 /* sizeof(int64_t) */
                    if (pvalue->value.z != (size_t)int1)
                        return_error(gs_error_rangecheck);
#endif
                    pvalue->value.i = int1;
                    goto ok;
                }
                case gs_param_type_float:
                {
                    float fl = (float)pvalue->value.z;
                    pvalue->value.f = fl;
                    goto ok;
                }
                default:
                    break;
            }
            break;
        case gs_param_type_string:
            if (req_type == gs_param_type_name)
                goto ok;
            break;
        case gs_param_type_name:
            if (req_type == gs_param_type_string)
                goto ok;
            break;
        case gs_param_type_int_array:
            switch (req_type) {
                case gs_param_type_float_array:{
                        uint size = pvalue->value.ia.size;
                        float *fv;
                        uint i;

                        if (mem == 0)
                            break;
                        fv = (float *)gs_alloc_byte_array(mem, size, sizeof(float),
                                                "int array => float array");

                        if (fv == 0)
                            return_error(gs_error_VMerror);
                        for (i = 0; i < size; ++i)
                            fv[i] = (float)pvalue->value.ia.data[i];
                        pvalue->value.fa.data = fv;
                        pvalue->value.fa.persistent = false;
                        goto ok;
                    }
                default:
                    break;
            }
            break;
        case gs_param_type_string_array:
            if (req_type == gs_param_type_name_array)
                goto ok;
            break;
        case gs_param_type_name_array:
            if (req_type == gs_param_type_string_array)
                goto ok;
            break;
        case gs_param_type_array:
            if (pvalue->value.d.size == 0 &&
                (req_type == gs_param_type_int_array ||
                 req_type == gs_param_type_float_array ||
                 req_type == gs_param_type_string_array ||
                 req_type == gs_param_type_name_array)
                )
                goto ok;
            break;
        default:
            break;
    }
    return_error(gs_error_typecheck);
  ok:pvalue->type = req_type;
    return 0;
}
int
param_read_requested_typed(gs_param_list * plist, gs_param_name pkey,
                           gs_param_typed_value * pvalue)
{
    gs_param_type req_type = pvalue->type;
    int code = (*plist->procs->xmit_typed) (plist, pkey, pvalue);

    if (code != 0)
        return code;
    return param_coerce_typed(pvalue, req_type, plist->memory);
}

/* ---------------- Fixed-type reading procedures ---------------- */

#define RETURN_READ_TYPED(alt, ptype)\
  gs_param_typed_value typed;\
  int code;\
\
  typed.type = ptype;\
  code = param_read_requested_typed(plist, pkey, &typed);\
  if ( code == 0 )\
    *pvalue = typed.value.alt;\
  return code

int
param_read_null(gs_param_list * plist, gs_param_name pkey)
{
    gs_param_typed_value typed;

    typed.type = gs_param_type_null;
    return param_read_requested_typed(plist, pkey, &typed);
}
int
param_read_bool(gs_param_list * plist, gs_param_name pkey, bool * pvalue)
{
    RETURN_READ_TYPED(b, gs_param_type_bool);
}
int
param_read_int(gs_param_list * plist, gs_param_name pkey, int *pvalue)
{
    RETURN_READ_TYPED(i, gs_param_type_int);
}
int
param_read_long(gs_param_list * plist, gs_param_name pkey, long *pvalue)
{
    RETURN_READ_TYPED(l, gs_param_type_long);
}
int
param_read_i64(gs_param_list * plist, gs_param_name pkey, int64_t *pvalue)
{
    RETURN_READ_TYPED(i64, gs_param_type_i64);
}
int
param_read_size_t(gs_param_list * plist, gs_param_name pkey, size_t *pvalue)
{
    RETURN_READ_TYPED(z, gs_param_type_size_t);
}
int
param_read_float(gs_param_list * plist, gs_param_name pkey, float *pvalue)
{
    RETURN_READ_TYPED(f, gs_param_type_float);
}
int
param_read_string(gs_param_list * plist, gs_param_name pkey,
                  gs_param_string * pvalue)
{
    RETURN_READ_TYPED(s, gs_param_type_string);
}
int
param_read_name(gs_param_list * plist, gs_param_name pkey,
                gs_param_string * pvalue)
{
    RETURN_READ_TYPED(n, gs_param_type_string);
}
int
param_read_int_array(gs_param_list * plist, gs_param_name pkey,
                     gs_param_int_array * pvalue)
{
    RETURN_READ_TYPED(ia, gs_param_type_int_array);
}
int
param_read_float_array(gs_param_list * plist, gs_param_name pkey,
                       gs_param_float_array * pvalue)
{
    RETURN_READ_TYPED(fa, gs_param_type_float_array);
}
int
param_read_string_array(gs_param_list * plist, gs_param_name pkey,
                        gs_param_string_array * pvalue)
{
    RETURN_READ_TYPED(sa, gs_param_type_string_array);
}
int
param_read_name_array(gs_param_list * plist, gs_param_name pkey,
                      gs_param_string_array * pvalue)
{
    RETURN_READ_TYPED(na, gs_param_type_name_array);
}

#undef RETURN_READ_TYPED

/* ---------------- Default writing procedures ---------------- */

#define RETURN_WRITE_TYPED(alt, ptype)\
  gs_param_typed_value typed;\
\
  typed.value.alt = *pvalue;\
  typed.type = ptype;\
  return param_write_typed(plist, pkey, &typed)

int
param_write_null(gs_param_list * plist, gs_param_name pkey)
{
    gs_param_typed_value typed;

    typed.type = gs_param_type_null;
    return param_write_typed(plist, pkey, &typed);
}
int
param_write_bool(gs_param_list * plist, gs_param_name pkey, const bool * pvalue)
{
    RETURN_WRITE_TYPED(b, gs_param_type_bool);
}
int
param_write_int(gs_param_list * plist, gs_param_name pkey, const int *pvalue)
{
    RETURN_WRITE_TYPED(i, gs_param_type_int);
}
int
param_write_long(gs_param_list * plist, gs_param_name pkey, const long *pvalue)
{
    RETURN_WRITE_TYPED(l, gs_param_type_long);
}
int
param_write_i64(gs_param_list * plist, gs_param_name pkey, const int64_t *pvalue)
{
    RETURN_WRITE_TYPED(i64, gs_param_type_i64);
}
int
param_write_size_t(gs_param_list * plist, gs_param_name pkey, const size_t *pvalue)
{
    RETURN_WRITE_TYPED(z, gs_param_type_size_t);
}
int
param_write_float(gs_param_list * plist, gs_param_name pkey,
                  const float *pvalue)
{
    RETURN_WRITE_TYPED(f, gs_param_type_float);
}
int
param_write_string(gs_param_list * plist, gs_param_name pkey,
                   const gs_param_string * pvalue)
{
    RETURN_WRITE_TYPED(s, gs_param_type_string);
}
int
param_write_name(gs_param_list * plist, gs_param_name pkey,
                 const gs_param_string * pvalue)
{
    RETURN_WRITE_TYPED(n, gs_param_type_name);
}
int
param_write_int_array(gs_param_list * plist, gs_param_name pkey,
                      const gs_param_int_array * pvalue)
{
    RETURN_WRITE_TYPED(ia, gs_param_type_int_array);
}
int
param_write_int_values(gs_param_list * plist, gs_param_name pkey,
                       const int *values, uint size, bool persistent)
{
    gs_param_int_array ia;

    ia.data = values, ia.size = size, ia.persistent = persistent;
    return param_write_int_array(plist, pkey, &ia);
}
int
param_write_float_array(gs_param_list * plist, gs_param_name pkey,
                        const gs_param_float_array * pvalue)
{
    RETURN_WRITE_TYPED(fa, gs_param_type_float_array);
}
int
param_write_float_values(gs_param_list * plist, gs_param_name pkey,
                         const float *values, uint size, bool persistent)
{
    gs_param_float_array fa;

    fa.data = values, fa.size = size, fa.persistent = persistent;
    return param_write_float_array(plist, pkey, &fa);
}
int
param_write_string_array(gs_param_list * plist, gs_param_name pkey,
                         const gs_param_string_array * pvalue)
{
    RETURN_WRITE_TYPED(sa, gs_param_type_string_array);
}
int
param_write_name_array(gs_param_list * plist, gs_param_name pkey,
                       const gs_param_string_array * pvalue)
{
    RETURN_WRITE_TYPED(na, gs_param_type_name_array);
}

#undef RETURN_WRITE_TYPED

/* ---------------- Default request implementation ---------------- */

int
gs_param_request_default(gs_param_list * plist, gs_param_name pkey)
{
    return 0;
}

int
gs_param_requested_default(const gs_param_list * plist, gs_param_name pkey)
{
    return -1;			/* requested by default */
}
