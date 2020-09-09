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

/* pdfmark handling for the PDF interpreter */

#include "pdf_int.h"
#include "plmain.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_loop_detect.h"
#include "pdf_mark.h"



/* Apparently the strings to pass to the device are:
   key1 val1 .... keyN valN CTM /ANN
   CTM is (for example) "[1.0 0 0 1.0 0 0]"

   the /pdfmark command has this array of strings as a parameter, i.e.
   [ key1 val1 .... keyN valN CTM /ANN ] /pdfmark
*/
int pdfi_mark_from_dict(pdf_context *ctx, pdf_dict *dict, gs_matrix *ctm, const char *type)
{
    int code = 0;

    /* TODO:
     * convert the dictionary into a string
     * convert ctm into a string
     * build parameter of an array of [ dict ctm type ]
     * build parameter of "pdfmark"
     * call gs_putdeviceparams()
     */

    return code;
}
