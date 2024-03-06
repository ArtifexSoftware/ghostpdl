/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Definition of font type and bitmap font behavior */

#ifndef gxftype_INCLUDED
#  define gxftype_INCLUDED

/* Define the known font types. */
/* These numbers must be the same as the values of FontType */
/* in font dictionaries. */
typedef enum {
    ft_composite = 0,
    ft_encrypted = 1,
    ft_encrypted2 = 2,
    ft_user_defined = 3,
    ft_disk_based = 4,
    ft_CID_encrypted = 9,	/* CIDFontType 0 */
    ft_CID_user_defined = 10,	/* CIDFontType 1 */
    ft_CID_TrueType = 11,	/* CIDFontType 2 */
    ft_Chameleon = 14,
    ft_CID_bitmap = 32,		/* CIDFontType 4 */
    ft_TrueType = 42,
    ft_MicroType = 51,		/* MicroType 51 is arbitrary */
    /* We need to identify the PCL 'stick' font specially for pdfwrite. The
     * reason is that the stick font is drawn by stroking a path, and the width
     * of that path is supposed to be consistent. If there is anamorphic
     * scaling in play, then the scaling is taken into account during path
     * construction (when rendering) and when the path is stroked the stroke
     * width remains the same. However if we capture the path in pdfwrite as
     * a font, the scaling is applied after the glyph is drawn, and this
     * results in the stroke width being anamorphically scaled as well, which
     * is wrong. In this case we want to be able to avoid capturing the text
     * as a font and instead draw a path and stroke it. Previously the stick
     * font was just a ft_user_defined but this allows us to tell the
     * difference.
     */
    ft_GL2_stick_user_defined = 52,
    /* Another special case. PCL bitmap fonts differ from PS type 3 fonts in
     * that 'slots' (character codes) can be resued, which can't be done with
     * PS/PDF type 3 fonts. Not a problem for rendering since we just clear
     * the glyph from the caceh, which means the new definition gets rendered
     * as required. However pdfwrite needs to know that this is not a regular
     * type 3 font, otherwise it won't know to look out for the cache entry
     * being freed, and will reuse the original definition
     */
    ft_PCL_user_defined = 53,
    /* The next type has similar characteristics to the GL2 type (52)
       sharing all attributes except Em square units and the font is
       user defined, downloaded by the user, unfortunately that is
       enough to make a new type desirable. NB the stick font is not
       user defined so it is poorly named. */
    ft_GL2_531 = 54,
    /* This is for the PDF interpreter in C, rather than the one in PostScript.
     * The pdfwrite device assumes that a 'ft_user_defined' font will need to
     * to be handled by returning to the interpreter, but that is not the case
     * for the PDF itnerpreter, it handles the BuildChar immediately. So we
     * need to differentiate between the two.
     */
    ft_PDF_user_defined,
    /*
     * This is used by pdfwrite and the font embedding code. We need to be able to
     * write the original font type when we don't embed a substitute font, because it
     * was not embedded in the input. But we also need to know whether the original
     * font type was available. We use this as the inital value, so we can tell if
     * it has been altered.
     */
     ft_undefined
} font_type;

/* Define the bitmap font behaviors. */
/* These numbers must be the same as the values of the ExactSize, */
/* InBetweenSize, and TransformedChar entries in font dictionaries. */
typedef enum {
    fbit_use_outlines = 0,
    fbit_use_bitmaps = 1,
    fbit_transform_bitmaps = 2
} fbit_type;

#endif /* gxftype_INCLUDED */
