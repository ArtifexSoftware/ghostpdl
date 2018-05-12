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

/* dictionary handling for the PDF itnerpreter */

#ifndef PDF_DICTIONARY_FUNCTIONS
#define PDF_DICTIONARY_FUNCTIONS

void pdf_free_dict(pdf_obj *o);
int pdf_dict_known(pdf_dict *d, const char *Key, bool *known);
int pdf_dict_known_by_key(pdf_dict *d, pdf_name *Key, bool *known);
int pdf_merge_dicts(pdf_dict *target, pdf_dict *source);
int pdf_dict_put(pdf_dict *d, pdf_obj *Key, pdf_obj *value);
int pdf_dict_get(pdf_dict *d, const char *Key, pdf_obj **o);
int pdf_dict_get_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type t, pdf_obj **o);
int pdf_dict_get_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i);
int pdf_dict_get_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f);
int pdf_dict_copy(pdf_dict *target, pdf_dict *source);
int pdf_alloc_dict(pdf_context *ctx, uint64_t size, pdf_dict **returned);

#endif
