/* Copyright (C) 2018-2024 Artifex Software, Inc.
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

/* dictionary handling for the PDF interpreter */

#ifndef PDF_DICTIONARY_FUNCTIONS
#define PDF_DICTIONARY_FUNCTIONS

static inline uint64_t pdfi_dict_entries(pdf_dict *d) { return d->entries; }

int pdfi_dict_get_common(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o, bool cache);
static inline int pdfi_dict_get(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o)
{
    return pdfi_dict_get_common(ctx, d, Key, o, true);
}

static inline int pdfi_dict_get_nocache(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o)
{
    return pdfi_dict_get_common(ctx, d, Key, o, false);
}


void pdfi_free_dict(pdf_obj *o);
int pdfi_dict_delete_pair(pdf_context *ctx, pdf_dict *d, pdf_name *n);
int pdfi_dict_delete(pdf_context *ctx, pdf_dict *d, const char *str);
int pdfi_dict_alloc(pdf_context *ctx, uint64_t size, pdf_dict **d);
int pdfi_dict_from_stack(pdf_context *ctx, uint32_t indirect_num, uint32_t indirect_gen, bool convert_string_keys);
int pdfi_dict_known(pdf_context *ctx, pdf_dict *d, const char *Key, bool *known);
int pdfi_dict_known_by_key(pdf_context *ctx, pdf_dict *d, pdf_name *Key, bool *known);
int pdfi_dict_knownget(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o);
int pdfi_dict_knownget_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o);
int pdfi_dict_knownget_type_no_store_R(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o);
int pdfi_dict_knownget_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f);
int pdfi_dict_knownget_bool(pdf_context *ctx, pdf_dict *d, const char *Key, bool *b);
int pdfi_merge_dicts(pdf_context *ctx, pdf_dict *target, pdf_dict *source);
int pdfi_dict_put_obj(pdf_context *ctx, pdf_dict *d, pdf_obj *Key, pdf_obj *value, bool replace);
int pdfi_dict_put_unchecked(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj *value);
int pdfi_dict_put(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj *value);
int pdfi_dict_put_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t value);
int pdfi_dict_put_bool(pdf_context *ctx, pdf_dict *d, const char *Key, bool value);
int pdfi_dict_put_name(pdf_context *ctx, pdf_dict *d, const char *Key, const char *name);
int pdfi_dict_get2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, pdf_obj **o);
int pdfi_dict_get_no_deref(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o);
int pdfi_dict_get_by_key(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o);
int pdfi_dict_get_no_store_R_key(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o);
int pdfi_dict_get_no_store_R(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o);
int pdfi_dict_get_type2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, pdf_obj_type type, pdf_obj **o);
int pdfi_dict_get_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type t, pdf_obj **o);
int pdfi_dict_get_type_no_store_R(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o);
int pdfi_dict_get_ref(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_indirect_ref **o);
int pdfi_dict_get_int2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, int64_t *i);
int pdfi_dict_get_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i);
int pdfi_dict_get_int_def(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i, int64_t def_val);
int pdfi_dict_get_bool2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, bool *val);
int pdfi_dict_get_bool(pdf_context *ctx, pdf_dict *d, const char *Key, bool *val);
int pdfi_dict_get_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f);
int pdfi_dict_get_number2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, double *f);
int fill_domain_from_dict(pdf_context *ctx, float *parray, int size, pdf_dict *dict);
int fill_float_array_from_dict(pdf_context *ctx, float *parray, int size, pdf_dict *dict, const char *Key);
int fill_bool_array_from_dict(pdf_context *ctx, bool *parray, int size, pdf_dict *dict, const char *Key);
int fill_matrix_from_dict(pdf_context *ctx, float *parray, pdf_dict *dict);
int pdfi_make_float_array_from_dict(pdf_context *ctx, float **parray, pdf_dict *dict, const char *Key);
int pdfi_make_int_array_from_dict(pdf_context *ctx, int **parray, pdf_dict *dict, const char *Key);
int pdfi_dict_copy(pdf_context *ctx, pdf_dict *target, pdf_dict *source);
int pdfi_dict_next(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, pdf_obj **Value, uint64_t *index);
int pdfi_dict_first(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, pdf_obj **Value, uint64_t *index);
int pdfi_dict_key_next(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, uint64_t *index);
int pdfi_dict_key_first(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, uint64_t *index);

int pdfi_dict_from_obj(pdf_context *ctx, pdf_obj *obj, pdf_dict **dict);
int64_t pdfi_stream_length(pdf_context *ctx, pdf_stream *stream);
gs_offset_t pdfi_stream_offset(pdf_context *ctx, pdf_stream *stream);
pdf_stream *pdfi_stream_parent(pdf_context *ctx, pdf_stream *stream);
void pdfi_set_stream_parent(pdf_context *ctx, pdf_stream *stream, pdf_stream *parent);
void pdfi_clear_stream_parent(pdf_context *ctx, pdf_stream *stream);
#endif
