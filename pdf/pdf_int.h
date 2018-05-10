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

#include "ghostpdf.h"
#include "pdf_types.h"

#ifndef PDF_INTERPRETER
#define PDF_INTERPRETER

int pdf_read_token(pdf_context *ctx, pdf_stream *s);
int pdf_read_object(pdf_context *ctx, pdf_stream *s);
int pdf_read_object_of_type(pdf_context *ctx, pdf_stream *s, pdf_obj_type t);
void pdf_free_object(pdf_obj *o);
int pdf_pop(pdf_context *ctx, int num);
void pdf_clearstack(pdf_context *ctx);
int pdf_push(pdf_context *ctx, pdf_obj *o);
int pdf_mark_stack(pdf_context *ctx, pdf_obj_type type);
int pdf_count_to_mark(pdf_context *ctx, uint64_t *count);
int pdf_clear_to_mark(pdf_context *ctx);

int pdf_make_name(pdf_context *ctx, byte *key, uint32_t size, pdf_obj **o);
int pdf_alloc_dict(pdf_context *ctx, uint64_t size, pdf_dict **returned);

int pdf_dict_known(pdf_dict *d, const char *Key, bool *known);
int pdf_dict_known_by_key(pdf_dict *d, pdf_name *Key, bool *known);
int pdf_merge_dicts(pdf_dict *target, pdf_dict *source);
int pdf_dict_put(pdf_dict *d, pdf_obj *Key, pdf_obj *value);
int pdf_dict_get(pdf_dict *d, const char *Key, pdf_obj **o);
int pdf_dict_get_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type t, pdf_obj **o);
int pdf_dict_get_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i);
int pdf_dict_get_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f);
int pdf_dict_copy(pdf_dict *target, pdf_dict *source);

int pdf_array_get(pdf_array *a, uint64_t index, pdf_obj **o);
int pdf_array_get_type(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj_type t, pdf_obj **o);
int pdf_array_get_int(pdf_context *ctx, pdf_array *a, uint64_t index, int64_t *i);
int pdf_array_get_number(pdf_context *ctx, pdf_array *a, uint64_t index, double *f);
int pdf_array_put(pdf_array *a, uint64_t index, pdf_obj *o);

int pdf_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object);

int pdf_read_xref(pdf_context *ctx);

int repair_pdf_file(pdf_context *ctx);

#endif
