/* Copyright (C) 2020-2023 Artifex Software, Inc.
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

/* code for CIDFontType0/Type 9 font handling */
/* CIDFonts with PS outlines
   This will have to cope with Type 1, Type 1C
   and CFF from OTF.
*/

#include "pdf_int.h"
#include "pdf_font.h"
#include "pdf_font0.h"
#include "pdf_fontps.h"
#include "pdf_font_types.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"

int pdfi_read_cidtype0_font(pdf_context *ctx, pdf_dict *font_dict, byte *fbuf, int fbuflen, pdf_font **ppdffont)
{
#if 0
    int code;

    code = pdfi_read_ps_font(ctx, font_dict, fbuf, fbuflen, ppfont);
    if (code < 0) return code;
#else
    gs_free_object(ctx->memory, fbuf, "pdfi_read_cidtype0_font");
#endif

    return_error(gs_error_invalidfont);
}
