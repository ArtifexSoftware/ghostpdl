/* Copyright (C) 1999, 2000, 2002 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id: gdevpdft.h */
/* Text definitions and state for pdfwrite. */

#ifndef gdevpdft_INCLUDED
#  define gdevpdft_INCLUDED

/* ================ Types and structures ================ */

/* ------ Fonts ------ */

/* Define the standard fonts. */
#define PDF_NUM_STD_FONTS 14
#define pdf_do_std_fonts(m)\
  m("Courier", ENCODING_INDEX_STANDARD)\
  m("Courier-Bold", ENCODING_INDEX_STANDARD)\
  m("Courier-Oblique", ENCODING_INDEX_STANDARD)\
  m("Courier-BoldOblique", ENCODING_INDEX_STANDARD)\
  m("Helvetica", ENCODING_INDEX_STANDARD)\
  m("Helvetica-Bold", ENCODING_INDEX_STANDARD)\
  m("Helvetica-Oblique", ENCODING_INDEX_STANDARD)\
  m("Helvetica-BoldOblique", ENCODING_INDEX_STANDARD)\
  m("Symbol", ENCODING_INDEX_SYMBOL)\
  m("Times-Roman", ENCODING_INDEX_STANDARD)\
  m("Times-Bold", ENCODING_INDEX_STANDARD)\
  m("Times-Italic", ENCODING_INDEX_STANDARD)\
  m("Times-BoldItalic", ENCODING_INDEX_STANDARD)\
  m("ZapfDingbats", ENCODING_INDEX_DINGBATS)

/* Font-related definitions */
#ifndef pdf_font_descriptor_DEFINED
#  define pdf_font_descriptor_DEFINED
typedef struct pdf_font_descriptor_s pdf_font_descriptor_t;
#endif
typedef struct pdf_std_font_s {
    gs_font *font;		/* weak pointer, may be 0 */
    pdf_font_descriptor_t *pfd;  /* *not* a weak pointer */
    gs_matrix orig_matrix;
    gs_uid uid;			/* UniqueID, not XUID */
} pdf_std_font_t;

/* Text state */
typedef struct pdf_text_state_s {
    /* Text state parameters */
    float character_spacing;  /* Tc */
    pdf_font_t *font;	/* for Tf */
    floatp size;		/* for Tf */
    float word_spacing;	/* Tw */
    float leading;		/* TL */
    bool use_leading;	/* true => use ', false => use Tj */
    int render_mode;	/* Tr */
    /* Bookkeeping */
    gs_matrix matrix;	/* relative to device space, not user space */
    gs_point line_start;
    gs_point current;
#define max_text_buffer 200	/* arbitrary, but overflow costs 5 chars */
    byte buffer[max_text_buffer];
    int buffer_count;
} pdf_text_state_t;
#define pdf_text_state_default\
  0, NULL, 0, 0, 0, 0 /*false*/, 0,\
  { identity_matrix_body }, { 0, 0 }, { 0, 0 }, { 0 }, 0

/* Font state */
typedef struct pdf_font_state_s {
    pdf_font_t *open_font;	/* current Type 3 synthesized font */
    bool use_open_font;		/* if false, start new open_font */
    long embedded_encoding_id;
    int max_embedded_code;	/* max Type 3 code used */
    pdf_std_font_t std_fonts[PDF_NUM_STD_FONTS];
    long space_char_ids[X_SPACE_MAX - X_SPACE_MIN + 1];
} pdf_font_state_t;
#define pdf_font_state_default\
  0,				/* open_font */\
  0 /*false*/,			/* use_open_font */\
  0,				/* embedded_encoding_id */\
  -1,				/* max_embedded_code */\
  {{0}},			/* std_fonts */\
  {0}				/* space_char_ids */

/* Text data */
/*typedef*/ struct pdf_text_data_s {
    pdf_text_state_t t;
    pdf_font_state_t f;
} /*pdf_text_data_t*/;

#define pdf_text_data_default\
  { pdf_text_state_default },\
  { pdf_font_state_default }

#endif /* gdevpdft_INCLUDED */
