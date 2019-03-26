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

#ifndef PDF_ARRAY_FUNCTIONS
#define PDF_ARRAY_FUNCTIONS

#define pdfi_array_size(a) ((a)->size)

void pdfi_free_array(pdf_obj *o);
int pdfi_array_peek(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj **o);
int pdfi_array_get(pdf_array *a, uint64_t index, pdf_obj **o);
int pdfi_array_get_type(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj_type t, pdf_obj **o);
int pdfi_array_get_int(pdf_context *ctx, pdf_array *a, uint64_t index, int64_t *i);
int pdfi_array_get_number(pdf_context *ctx, pdf_array *a, uint64_t index, double *f);
int pdfi_array_put(pdf_array *a, uint64_t index, pdf_obj *o);

bool pdfi_array_known(pdf_array *a, pdf_obj *, int *index);
#endif
