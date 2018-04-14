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

#include "ghostpdf.h"
#include "pdf_types.h"

#ifndef PDF_INTERPRETER
#define PDF_INTERPRETER

int pdf_read_token(pdf_context *ctx, stream *s);
void pdf_free_object(pdf_obj *o);

static inline pdf_countup(pdf_obj *o)
{
    o->refcnt++;
}

static inline pdf_countdown(pdf_obj *o)
{
#ifdef DEBUG
    if (o->refcnt == 0)
        emprintf(o->memory, "Decrementing objct with recount at 0!\n");
#endif
    o->refcnt--;
    if (o->refcnt == 0)
        pdf_free_object(o);
}


#endif
