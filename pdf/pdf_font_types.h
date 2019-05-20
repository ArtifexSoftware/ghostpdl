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

/* font structures for the PDF interpreter */

#ifndef PDF_FONT_DATA
#define PDF_FONT_DATA

#include "gxfont.h"
#include "gsfont.h"

typedef enum pdf_font_type_e {
    e_pdf_font_type0,           /* Somewhat special a font type 0 is a placeholder for a CIDFont */
    e_pdf_font_type1,
    e_pdf_font_cff,
    e_pdf_font_type3,
    e_pdf_cidfont_type0 = 9,
    e_pdf_cidfont_type1,
    e_pdf_cidfont_type2,
    e_pdf_cidfont_type4 = 32,
    e_pdf_font_truetype = 42,
} pdf_font_type;

typedef struct pdf_font_base_s {
    gs_font *pfont;             /* Graphics library font structure */
    uint64_t object;
    uint64_t generation;

    pdf_font_type font_type;    /* See above */
    pdf_obj *BaseFont;          /* Should be a name object, but best allow for strings as well */
    pdf_dict *FontDescriptor;   /* For PDF up to 1.4 this may be absent for the base 14 */
} pdf_font_base;

typedef struct pdf_font_common {
    pdf_font_base base;

    pdf_obj *Name;              /* Should be a name object, but best allow for strings as well */
    uint64_t FirstChar;         /* For PDF up to 1.4 this may be absent for the base 14 */
    uint64_t LastChar;          /* For PDF up to 1.4 this may be absent for the base 14 */
    float *Widths;              /* For PDF up to 1.4 this may be absent for the base 14 */
    pdf_obj *Encoding;          /* Name or dictionary */
    pdf_obj *ToUnicode;         /* Name or stream (technically should be a stream, but we've seen Identity names */
} pdf_font_common;

typedef struct pdf_font_type0_s {
    pdf_font_base base;         /* For this font type, the FontDescriptor will be NULL, as will the pfont, (we use the DescendantFont) */

    pdf_obj *Encoding;          /* Name or sream for the CMap */
    pdf_array *DescendantFonts; /* A single element array specifying the CIDFont dictionary */
    pdf_obj *ToUnicode;         /* Name or stream (technically shoudl be a stream, but we've seen Identity names */
} pdf_font_type0;

typedef struct pdf_font_type1_s {
    pdf_font_common common;
} pdf_font_type1;

typedef struct pdf_font_type3_s {
    pdf_font_common common;

    pdf_array *FontBBox;
    pdf_array *FontMatrix;
    pdf_dict *CharProcs;
    pdf_dict *Resources;
} pdf_font_type3;

typedef struct pdf_font_truetype_s {
    pdf_font_common common;
} pdf_font_truetype;

typedef struct pdf_cidfont {
    pdf_font_base base;

    pdf_dict *CIDSystemInfo;
    uint64_t W;
    pdf_array *DW;
    uint64_t W2;
    pdf_array *DW2;
    pdf_obj *CIDToGIDMap;
} pdf_cidfont;

#endif
