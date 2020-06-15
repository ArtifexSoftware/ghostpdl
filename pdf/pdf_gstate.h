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

/* Graphics state operations for the PDF interpreter */

#ifndef PDF_GSTATE_OPERATORS
#define PDF_GSTATE_OPERATORS

/* Interpreter graphics state things (see igstate.h/int_gstate) */
typedef struct int_gstate_s {
    pdf_context *ctx;
    pdf_dict *SMask; /* PDF only, null | dictionary | true */
    gs_gstate *GroupGState; /* gstate associated with the SMask */
    gs_memory_t *memory;
} pdfi_int_gstate;

int pdfi_gstate_set_client(pdf_context *ctx, gs_gstate *pgs);
void pdfi_gstate_smask_install(pdfi_int_gstate *igs, gs_memory_t *memory, pdf_dict *SMask, gs_gstate *gstate);
void pdfi_gstate_smask_free(pdfi_int_gstate *igs);

int pdfi_get_blend_mode(pdf_context *ctx, pdf_name *name, gs_blend_mode_t *mode);

int pdfi_concat(pdf_context *ctx);
int pdfi_gsave(pdf_context *ctx);
int pdfi_grestore(pdf_context *ctx);
int pdfi_op_q(pdf_context *ctx);
int pdfi_op_Q(pdf_context *ctx);
int pdfi_setlinewidth(pdf_context *ctx);
int pdfi_setlinejoin(pdf_context *ctx);
int pdfi_setlinecap(pdf_context *ctx);
int pdfi_setflat(pdf_context *ctx);
int pdfi_setdash(pdf_context *ctx);
int pdfi_setmiterlimit(pdf_context *ctx);
int pdfi_setgstate(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_setdash_impl(pdf_context *ctx, pdf_array *a, double phase_d);

int pdfi_free_DefaultQState(pdf_context *ctx);
int pdfi_set_DefaultQState(pdf_context *ctx, gs_gstate *pgs);
gs_gstate *pdfi_get_DefaultQState(pdf_context *ctx);
int pdfi_copy_DefaultQState(pdf_context *ctx, gs_gstate **pgs);
int pdfi_restore_DefaultQState(pdf_context *ctx, gs_gstate **pgs);

#endif
