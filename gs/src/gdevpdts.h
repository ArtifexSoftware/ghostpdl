/* Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
  
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

/* $Id$ */
/* Text state structure and API for pdfwrite */

#ifndef gdevpdts_INCLUDED
#  define gdevpdts_INCLUDED

#include "gsmatrix.h"

/* ================ Types and structures ================ */

#ifndef pdf_text_state_DEFINED
#  define pdf_text_state_DEFINED
typedef struct pdf_text_state_s pdf_text_state_t;
#endif

/*
 * Clients pass in the text state values; the implementation decides when
 * (and, in the case of text positioning, how) to emit PDF commands to
 * set them in the output.
 */
typedef struct pdf_text_state_values_s {
    float character_spacing;	/* Tc */
#define TEXT_STATE_SET_CHARACTER_SPACING 1
    pdf_font_resource_t *pdfont; /* for Tf */
    double size;		/* for Tf */
#define TEXT_STATE_SET_FONT_AND_SIZE 2
    gs_matrix matrix;		/* Tm et al (relative to device space, */
				/* not user space) */
#define TEXT_STATE_SET_MATRIX 4
    int render_mode;		/* Tr */
#define TEXT_STATE_SET_RENDER_MODE 8
    float word_spacing;		/* Tw */
#define TEXT_STATE_SET_WORD_SPACING 16
} pdf_text_state_values_t;
#define TEXT_STATE_VALUES_DEFAULT\
    0,				/* character_spacing */\
    NULL,			/* font */\
    0,				/* size */\
    { identity_matrix_body },	/* matrix */\
    0,				/* render_mode */\
    0				/* word_spacing */

/* ================ Procedures ================ */

/* ------ Exported for gdevpdf.c ------ */

/*
 * Allocate and initialize text state bookkeeping.
 */
pdf_text_state_t *pdf_text_state_alloc(gs_memory_t *mem);

/*
 * Reset the text state at the beginning of the page.
 */
void pdf_reset_text_page(pdf_text_data_t *ptd);

/*
 * Reset the text state after a grestore.
 */
void pdf_reset_text_state(pdf_text_data_t *ptd);

/* ------ Exported for gdevpdfu.c ------ */

/*
 * Transition from stream context to text context.
 */
int pdf_from_stream_to_text(gx_device_pdf *pdev);

/*
 * Transition from string context to text context.
 */
int pdf_from_string_to_text(gx_device_pdf *pdev);

/*
 * Close the text aspect of the current contents part.
 */
void pdf_close_text_contents(gx_device_pdf *pdev); /* gdevpdts.h */

/* Used only within the text code */

/*
 * Test whether a change in render_mode requires resetting the stroke
 * parameters.
 */
bool pdf_render_mode_uses_stroke(const gx_device_pdf *pdev,
				 const pdf_text_state_values_t *ptsv);

/*
 * Set text state values (as seen by client).
 */
int pdf_set_text_state_values(gx_device_pdf *pdev,
			      const pdf_text_state_values_t *ptsv,
			      int members);

/*
 * Transform a distance from unscaled text space (text space ignoring the
 * scaling implied by the font size) to device space.
 */
int pdf_text_distance_transform(floatp wx, floatp wy,
				const pdf_text_state_t *pts,
				gs_point *ppt);

/*
 * Return the current (x,y) text position as seen by the client, in
 * unscaled text space.
 */
void pdf_text_position(const gx_device_pdf *pdev, gs_point *ppt);

/*
 * Append characters to text being accumulated.
 */
int pdf_append_chars(gx_device_pdf * pdev, const byte * str, uint size,
		     floatp wx, floatp wy);

#endif /* gdevpdts_INCLUDED */
