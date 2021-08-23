/* Copyright (C) 2018-2021 Artifex Software, Inc.
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

#ifndef PDF_ARRAY_FUNCTIONS
#define PDF_ARRAY_FUNCTIONS

static inline uint64_t pdfi_array_size(pdf_array *a) { return a->size; }

void pdfi_free_array(pdf_obj *o);
int pdfi_array_alloc(pdf_context *ctx, uint64_t size, pdf_array **a);
int pdfi_array_from_stack(pdf_context *ctx, uint32_t indirect_num, uint32_t indirect_gen);
int pdfi_array_get_no_deref(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o);
int pdfi_array_get_no_store_R(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o);
int pdfi_array_get(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o);
int pdfi_array_get_type(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj_type t, pdf_obj **o);
int pdfi_array_get_int(pdf_context *ctx, pdf_array *a, uint64_t index, int64_t *i);
int pdfi_array_get_number(pdf_context *ctx, pdf_array *a, uint64_t index, double *f);
int pdfi_array_put(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj *o);
int pdfi_array_put_int(pdf_context *ctx, pdf_array *a, uint64_t index, int64_t val);
int pdfi_array_put_real(pdf_context *ctx, pdf_array *a, uint64_t index, double val);

bool pdfi_array_known(pdf_context *ctx, pdf_array *a, pdf_obj *, int *index);

void pdfi_normalize_rect(pdf_context *ctx, gs_rect *rect);
int pdfi_array_to_gs_rect(pdf_context *ctx, pdf_array *array, gs_rect *rect);
int pdfi_gs_rect_to_array(pdf_context *ctx, gs_rect *rect, pdf_array **new_array);
int pdfi_array_to_gs_matrix(pdf_context *ctx, pdf_array *array, gs_matrix *matrix);
int pdfi_array_to_num_array(pdf_context *ctx, pdf_array *array, double *out, int start, int size);
void pdfi_bbox_transform(pdf_context *ctx, gs_rect *bbox, gs_matrix *matrix);

#endif
