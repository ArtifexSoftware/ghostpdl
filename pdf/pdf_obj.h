/* Copyright (C) 2020-2021 Artifex Software, Inc.
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

#ifndef PDF_OBJECTS
#define PDF_OBJECTS

int pdfi_object_alloc(pdf_context *ctx, pdf_obj_type type, unsigned int size, pdf_obj **obj);
void pdfi_free_object(pdf_obj *o);
int pdfi_obj_to_string(pdf_context *ctx, pdf_obj *obj, byte **data, int *len);
int pdfi_obj_dict_to_stream(pdf_context *ctx, pdf_dict *dict, pdf_stream **stream, bool do_convert);
int pdfi_obj_charstr_to_string(pdf_context *ctx, const char *charstr, pdf_string **string);
int pdfi_obj_charstr_to_name(pdf_context *ctx, const char *charstr, pdf_name **name);
int pdfi_obj_get_label(pdf_context *ctx, pdf_obj *obj, char **label);
int pdfi_num_alloc(pdf_context *ctx, double d, pdf_num **num);

#endif
