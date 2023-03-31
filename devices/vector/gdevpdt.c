/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Miscellaneous external entry points for pdfwrite text */
#include "gx.h"
#include "memory_.h"
#include "gdevpdfx.h"
#include "gdevpdtx.h"
#include "gdevpdtf.h"
#include "gdevpdti.h"

/* GC descriptors */
private_st_pdf_text_data();

/* ---------------- Initialization ---------------- */

/*
 * Allocate and initialize the text data structure.
 */
pdf_text_data_t *
pdf_text_data_alloc(gs_memory_t *mem)
{
    pdf_text_data_t *ptd =
        gs_alloc_struct(mem, pdf_text_data_t, &st_pdf_text_data,
                        "pdf_text_data_alloc");
    pdf_outline_fonts_t *pofs = pdf_outline_fonts_alloc(mem);
    pdf_bitmap_fonts_t *pbfs = pdf_bitmap_fonts_alloc(mem);
    pdf_text_state_t *pts = pdf_text_state_alloc(mem);

    if (pts == 0 || pbfs == 0 || pofs == 0 || ptd == 0) {
        gs_free_object(mem, pts, "pdf_text_data_alloc");
        gs_free_object(mem, pbfs, "pdf_text_data_alloc");
        gs_free_object(mem, pofs, "pdf_text_data_alloc");
        gs_free_object(mem, ptd, "pdf_text_data_alloc");
        return 0;
    }
    memset(ptd, 0, sizeof(*ptd));
    ptd->outline_fonts = pofs;
    ptd->bitmap_fonts = pbfs;
    ptd->text_state = pts;
    return ptd;
}

int text_data_free(gs_memory_t *mem, pdf_text_data_t *ptd)
{
    gs_free_object(mem, ptd->outline_fonts->standard_fonts, "Free text Outline standard fonts");;
    gs_free_object(mem, ptd->outline_fonts, "Free text Outline fonts");;
    gs_free_object(mem, ptd->bitmap_fonts, "Free text Bitmap fotns");;
    gs_free_object(mem, ptd->text_state, "Free text state");;
    gs_free_object(mem, ptd, "Free text");

    return 0;
}
