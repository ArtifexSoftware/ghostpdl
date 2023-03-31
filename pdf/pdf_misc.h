/* Copyright (C) 2019-2023 Artifex Software, Inc.
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

#ifndef PDF_MISC
#define PDF_MISC

int pdfi_get_current_bbox(pdf_context *ctx, gs_rect *bbox, bool stroked);
int pdfi_name_strcmp(const pdf_name *n, const char *s);
bool pdfi_string_is(const pdf_string *n, const char *s);
bool pdfi_name_is(const pdf_name *n, const char *s);
int pdfi_name_cmp(const pdf_name *n1, const pdf_name *n2);
int pdfi_string_cmp(const pdf_string *n1, const pdf_string *n2);
int pdfi_string_from_name(pdf_context *ctx, pdf_name *n, char **str, int *len);
int pdfi_free_string_from_name(pdf_context *ctx, char *str);

gs_color_space_index pdfi_get_color_space_index(pdf_context *ctx, const gs_color_space *pcs);
gs_color_space_index pdfi_currentcolorspace(pdf_context *ctx, int index);
int pdfi_setrenderingintent(pdf_context *ctx, pdf_name *n);

void normalize_rectangle(double *d);

void pdfi_free_cstring_array(pdf_context *ctx, char ***pstrlist);
int pdfi_parse_name_cstring_array(pdf_context *ctx, char *data, uint64_t size, char ***pstrlist);


#endif
