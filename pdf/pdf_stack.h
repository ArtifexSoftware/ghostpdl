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

/* Stack operations for the PDF interpreter */

#ifndef PDF_STACK_OPERATIONS
#define PDF_STACK_OPERATIONS

int pdf_pop(pdf_context *ctx, int num);
int pdf_push(pdf_context *ctx, pdf_obj *o);
int pdf_mark_stack(pdf_context *ctx, pdf_obj_type type);
void pdf_clearstack(pdf_context *ctx);
int pdf_count_to_mark(pdf_context *ctx, uint64_t *count);
int pdf_clear_to_mark(pdf_context *ctx);

#endif
