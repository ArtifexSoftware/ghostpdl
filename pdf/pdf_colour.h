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

/* colour operations for the PDF interpreter */

#ifndef PDF_COLOUR_OPERATORS
#define PDF_COLOUR_OPERATORS

#include "gscolor1.h"

int pdf_setgraystroke(pdf_context *ctx);
int pdf_setgrayfill(pdf_context *ctx);
int pdf_setrgbstroke(pdf_context *ctx);
int pdf_setrgbfill(pdf_context *ctx);
int pdf_setcmykstroke(pdf_context *ctx);
int pdf_setcmykfill(pdf_context *ctx);
int pdf_setstrokecolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdf_setfillcolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdf_setstrokecolor(pdf_context *ctx);
int pdf_setfillcolor(pdf_context *ctx);
int pdf_setstrokecolorN(pdf_context *ctx);
int pdf_setfillcolorN(pdf_context *ctx);
int pdf_ri(pdf_context *ctx);

#endif
