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

/* function creation for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_func.h"

int pdfi_build_function(pdf_context *ctx, gs_function_t ** ppfn, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int Type, code, i;
    pdf_array *Domain = NULL, *Range = NULL, *value = NULL;

    code = pdfi_dict_get_int(ctx, stream_dict, "FunctionType", &Type);
    if (code < 0)
        return code;
    if (Type < 0 || Type > 4 || Type == 1)
        return_error(gs_error_rangecheck);

    /* First gather all the entries common to all functions */
    code = pdfi_dict_get(ctx, stream_dict, "Domain", (pdf_obj *)&Domain);
    if (code < 0)
        return code;
    if (Domain->type != PDF_ARRAY)
        return_error(gs_error_typecheck);
    if (Domain->entries & 1)
        return_error(gs_error_rangecheck);
    for (i=0;i< Domain->entries;i+=2) {
        if (Domain->values[i]->type != PDF_INT && Domain->values[i]->type != PDF_REAL) {
            pdfi_countdown(Domain);
            return_error(gs_error_typecheck);
        }
        if (Domain->values[i] > Domain->values[i+1]) {
            pdfi_countdown(Domain);
            return_error(gs_error_rangecheck);
        }
    }

    code = pdfi_dict_get(ctx, stream_dict, "Range", (pdf_obj *)&Range);
    if (code < 0 && code != gs_error_undefined) {
        pdfi_countdown(Domain);
        return code;
    }
    if (code == gs_error_undefined && (Type == 0 || Type == 4)) {
        pdfi_countdown(Domain);
        return code;
    }
    if (code == 0) {
        if (Range->type != PDF_ARRAY) {
            pdfi_countdown(Domain);
            pdfi_countdown(Range);
            return_error(gs_error_typecheck);
        }
        if (Range->entries & 1)
            return_error(gs_error_rangecheck);
        for (i=0;i< Range->entries;i+=2) {
            if (Range->values[i]->type != PDF_INT && Range->values[i]->type != PDF_REAL) {
                pdfi_countdown(Domain);
                pdfi_countdown(Range);
                return_error(gs_error_typecheck);
            }
            if (Range->values[i] > Range->values[i+1]) {
                pdfi_countdown(Domain);
                pdfi_countdown(Range);
                return_error(gs_error_rangecheck);
            }
        }
    }
    switch(Type) {
        case 0:
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        default:
            break;
    }
    pdfi_countdown(Domain);
    pdfi_countdown(Range);
    return 0;
}
