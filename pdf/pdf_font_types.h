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

/* Fonts are, unfortunately, complicated by the fact that the graphics state font object
 * the 'gs_font' structure is not reference counted. The PostScript interpreter relies upon
 * the garbage collector to clean these up, but since we aren't using a garbage collector
 * we have to manage the lifetime of these ourselves.
 *
 * The complication is that the gs_font pointer is stored in the graphics state and there
 * is no simple mechanism to know when a font in a graphics state is no longer being used
 * (in the absence of a garbage collector). So what we do is use the 'client_data' member
 * of the gs_font structure to point to our font object, a pdf_font *, which itself
 * contains a pointer to the gs_font structure.
 *
 * On each pdfi_gsave() we look at the gs_font in the current graphics state (if any) and
 * use the client_data to find the corresponding pdf_font, we then cout that object up. On
 * each pdfi_grestore we again use the client_data to find our pdf_font object, and count
 * it down. The Tf operator replaces the gs_font in the current graphics state, and we count
 * the current one down, and the new one up, before replacing it.
 *
 * When the pdf_font object's reference count reaches 0 we can safely free the associated
 * gs_font structure. Note that the cache may still contain a reference to the pdf_font
 * object, so it may not be freed immediately it is no longer in use. This helps us avoid
 * too much redefinition of fonts.
 *
 * When we create a pdf_font object, we replace the cached object, which is a dictionary,
 * with the pdf_font object. So when we call pdfi_dereference() if we get back a pdf_font
 * object we know that this is an existing font (keyed by object number) and don't need to
 * rebuild it. If we get a pdf_dict object then we do need to build the graphics library
 * font.
 *
 * This still does leave the (admittedly remote) possibility of using a font in one graphics
 * state, gsave'ing, setting a new font, and then dereferencing enough objects to force the
 * original font out of the cache, then using that same font again. While there is no
 * obvious problem with that, it is inefficient. I think we can evade the problem by using
 * the next and prev pointers in the gs_fotn structure. If we keep the linked list correct
 * when creating and freeing fonts, we can walk the list of 'active' fonts, following
 * the clien_data pointer to get the pdf_font object, and checking its object number against
 * the one we are looking for. We only need to do this if the result of pdfi_derference()
 * Is a pdf_dict object. Obviously if we don't find an active font then we need to go
 * through the effort of creating one.
 *
 * FIXME - none of the above is yet implemented!
 */

#ifndef PDF_FONT_DATA
#define PDF_FONT_DATA

#include "pdf_types.h"
#include "gxfont.h"
#include "gsfont.h"

typedef enum pdf_font_type_e {
    e_pdf_font_type0,               /* Somewhat special a font type 0 is a placeholder for a CIDFont */
    e_pdf_font_type1,
    e_pdf_font_cff,
    e_pdf_font_type3,
    e_pdf_cidfont_type0 = 9,
    e_pdf_cidfont_type1,
    e_pdf_cidfont_type2,
    e_pdf_cidfont_type4 = 32,
    e_pdf_font_truetype = 42,
} pdf_font_type;

#define pdf_font_base \
    pdf_obj_common;\
    pdf_context *ctx;               /* We may need access to the PDF level from the font */\
    gs_font_base *pfont;            /* Graphics library font structure */\
    pdf_font_type pdfi_font_type;   /* See above */\
    pdf_dict *PDF_font;             /* The original font dictionary from the PDF file */\
    pdf_obj *BaseFont;              /* Should be a name object, but best allow for strings as well */\
    pdf_dict *FontDescriptor        /* For PDF up to 1.4 this may be absent for the base 14 */

#define pdf_font_common \
    pdf_font_base;\
    pdf_obj *Name;                  /* Should be a name object, but best allow for strings as well */\
    unsigned int FirstChar;         /* For PDF up to 1.4 this may be absent for the base 14 */\
    unsigned int LastChar;          /* For PDF up to 1.4 this may be absent for the base 14 */\
    double *Widths;                  /* For PDF up to 1.4 this may be absent for the base 14 */\
    pdf_array *Encoding;            /* Array built from name or dictionary */\
    pdf_obj *ToUnicode              /* Name or stream (technically should be a stream, but we've seen Identity names */

typedef struct pdf_font_s {
    pdf_font_common;
}pdf_font;

typedef struct pdf_font_type0_s {
    pdf_font_base;                  /* For this font type, the FontDescriptor will be NULL, as will the pfont, (we use the DescendantFont) */

    pdf_obj *Encoding;              /* Name or sream for the CMap */
    pdf_array *DescendantFonts;     /* A single element array specifying the CIDFont dictionary */
    pdf_obj *ToUnicode;             /* Name or stream (technically shoudl be a stream, but we've seen Identity names */
} pdf_font_type0;

typedef struct pdf_font_type1_s {
    pdf_font_common;
} pdf_font_type1;

typedef struct pdf_font_type3_s {
    pdf_font_common;

    pdf_array *FontBBox;
    pdf_array *FontMatrix;
    pdf_dict *CharProcs;
} pdf_font_type3;

typedef struct pdf_font_truetype_s {
    pdf_font_common;
    gs_string sfnt;
    int64_t descflags;
} pdf_font_truetype;

typedef struct pdf_cidfont {
    pdf_font_base;

    pdf_dict *CIDSystemInfo;
    uint64_t W;
    pdf_array *DW;
    uint64_t W2;
    pdf_array *DW2;
    pdf_obj *CIDToGIDMap;
} pdf_cidfont;

#endif
