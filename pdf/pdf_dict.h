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

/* dictionary handling for the PDF itnerpreter */

#ifndef PDF_DICTIONARY_FUNCTIONS
#define PDF_DICTIONARY_FUNCTIONS

static inline uint64_t pdfi_dict_entries(pdf_dict *d) { return d->entries; }

void pdfi_free_dict(pdf_obj *o);
int pdfi_dict_delete_pair(pdf_context *ctx, pdf_dict *d, pdf_name *n);
int pdfi_dict_from_stack(pdf_context *ctx, uint32_t indirect_num, uint32_t indirect_gen);
int pdfi_dict_known(pdf_dict *d, const char *Key, bool *known);
int pdfi_dict_known_by_key(pdf_dict *d, pdf_name *Key, bool *known);
int pdfi_dict_knownget(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o);
int pdfi_dict_knownget_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o);
int pdfi_dict_knownget_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f);
int pdfi_merge_dicts(pdf_dict *target, pdf_dict *source);
int pdfi_dict_put_obj(pdf_dict *d, pdf_obj *Key, pdf_obj *value);
int pdfi_dict_put(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj *value);
int pdfi_dict_put_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t value);
int pdfi_dict_put_bool(pdf_context *ctx, pdf_dict *d, const char *Key, bool value);
int pdfi_dict_put_name(pdf_context *ctx, pdf_dict *d, const char *Key, const char *name);
int pdfi_dict_get2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, pdf_obj **o);
int pdfi_dict_get(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj **o);
int pdfi_dict_get_by_key(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o);
int pdfi_dict_get_no_store_R(pdf_context *ctx, pdf_dict *d, const pdf_name *Key, pdf_obj **o);
int pdfi_dict_get_type2(pdf_context *ctx, pdf_dict *d, const char *Key1, const char *Key2, pdf_obj_type type, pdf_obj **o);
int pdfi_dict_get_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type t, pdf_obj **o);
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
int make_float_array_from_dict(pdf_context *ctx, float **parray, pdf_dict *dict, const char *Key);
int make_int_array_from_dict(pdf_context *ctx, int **parray, pdf_dict *dict, const char *Key);
int pdfi_dict_copy(pdf_dict *target, pdf_dict *source);
int pdfi_alloc_dict(pdf_context *ctx, uint64_t size, pdf_dict **returned);
int pdfi_dict_next(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, pdf_obj **Value, void *index);
int pdfi_dict_first(pdf_context *ctx, pdf_dict *d, pdf_obj **Key, pdf_obj **Value, void *index);
bool pdfi_dict_is_stream(pdf_context *ctx, pdf_dict *d);
int64_t pdfi_dict_stream_length(pdf_context *ctx, pdf_dict *d);

#endif
