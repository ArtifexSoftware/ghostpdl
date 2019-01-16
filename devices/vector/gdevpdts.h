/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Text state structure and API for pdfwrite */

#ifndef gdevpdts_INCLUDED
#  define gdevpdts_INCLUDED

#include "gsmatrix.h"
#include "gdevpdtx.h"

/*
 * See gdevpdtt.h for a discussion of the multiple coordinate systems that
 * the text code must use.
 */

/* ================ Types and structures ================ */

/*
 * Clients pass in the text state values; the implementation decides when
 * (and, in the case of text positioning, how) to emit PDF commands to
 * set them in the output.
 */
typedef struct pdf_text_state_values_s {
    float character_spacing;	/* Tc */
    pdf_font_resource_t *pdfont; /* for Tf */
    double size;		/* for Tf */
    /*
     * The matrix is the transformation from text space to user space, which
     * in pdfwrite text output is the same as device space.  Thus this
     * matrix combines the effect of the PostScript CTM and the FontMatrix,
     * scaled by the inverse of the font size value.
     */
    gs_matrix matrix;		/* Tm et al */
    int render_mode;		/* Tr */
    float word_spacing;		/* Tw */
} pdf_text_state_values_t;
#define TEXT_STATE_VALUES_DEFAULT\
    0,				/* character_spacing */\
    NULL,			/* font */\
    0,				/* size */\
    { identity_matrix_body },	/* matrix */\
    0,				/* render_mode */\
    0				/* word_spacing */

/*
 * We accumulate text, and possibly horizontal or vertical moves (depending
 * on the font's writing direction), until forced to emit them.  This
 * happens when changing text state parameters, when the buffer is full, or
 * when exiting text mode.
 *
 * Note that movement distances are measured in unscaled text space.
 */
typedef struct pdf_text_move_s {
    int index;			/* within buffer.chars */
    float amount;
} pdf_text_move_t;
#define MAX_TEXT_BUFFER_CHARS 200 /* arbitrary, but overflow costs 5 chars */
#define MAX_TEXT_BUFFER_MOVES 50 /* ibid. */
typedef struct pdf_text_buffer_s {
    /*
     * Invariant:
     *   count_moves <= MAX_TEXT_BUFFER_MOVES
     *   count_chars <= MAX_TEXT_BUFFER_CHARS
     *   0 < moves[0].index < moves[1].index < ... moves[count_moves-1].index
     *	   <= count_chars
     *   moves[*].amount != 0
     */
    pdf_text_move_t moves[MAX_TEXT_BUFFER_MOVES + 1];
    byte chars[MAX_TEXT_BUFFER_CHARS];
    int count_moves;
    int count_chars;
} pdf_text_buffer_t;
/*
 * We maintain two sets of text state values (as defined in gdevpdts.h): the
 * "in" set reflects the current state as seen by the client, while the
 * "out" set reflects the current state as seen by an interpreter processing
 * the content stream emitted so far.  We emit commands to make "out" the
 * same as "in" when necessary.
 */
/*typedef struct pdf_text_state_s pdf_text_state_t;*/  /* gdevpdts.h */
struct pdf_text_state_s {
    /* State as seen by client */
    pdf_text_state_values_t in; /* see above */
    gs_point start;		/* in.txy as of start of buffer */
    pdf_text_buffer_t buffer;
    int wmode;			/* WMode of in.font */
    /* State relative to content stream */
    pdf_text_state_values_t out; /* see above */
    double leading;		/* TL (not settable, only used internally) */
    bool use_leading;		/* if true, use T* or ' */
    bool continue_line;
    gs_point line_start;
    gs_point out_pos;		/* output position */
    double PaintType0Width;
    bool can_use_TJ;
};
/* ================ Procedures ================ */

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

/* ------ Used only within the text code ------ */

/*
 * Test whether a change in render_mode requires resetting the stroke
 * parameters.
 */
bool pdf_render_mode_uses_stroke(const gx_device_pdf *pdev,
                                 const pdf_text_state_values_t *ptsv);

/*
 * Read the stored client view of text state values.
 */
void pdf_get_text_state_values(gx_device_pdf *pdev,
                               pdf_text_state_values_t *ptsv);

/*
 * Set wmode to text state.
 */
void pdf_set_text_wmode(gx_device_pdf *pdev, int wmode);

/*
 * Set the stored client view of text state values.
 */
int pdf_set_text_state_values(gx_device_pdf *pdev,
                              const pdf_text_state_values_t *ptsv);

/*
 * Transform a distance from unscaled text space (text space ignoring the
 * scaling implied by the font size) to device space.
 */
int pdf_text_distance_transform(double wx, double wy,
                                const pdf_text_state_t *pts,
                                gs_point *ppt);

/*
 * Return the current (x,y) text position as seen by the client, in
 * unscaled text space.
 */
void pdf_text_position(const gx_device_pdf *pdev, gs_point *ppt);

int pdf_bitmap_char_update_bbox(gx_device_pdf * pdev,int x_offset, int y_offset, double x, double y);
/*
 * Append characters to text being accumulated, giving their advance width
 * in device space.
 */
int pdf_append_chars(gx_device_pdf * pdev, const byte * str, uint size,
                     double wx, double wy, bool nobreak);

bool pdf_compare_text_state_for_charpath(pdf_text_state_t *pts, gx_device_pdf *pdev,
                             gs_gstate *pgs, gs_font *font,
                             const gs_text_params_t *text);

int pdf_get_text_render_mode(pdf_text_state_t *pts);
void pdf_set_text_render_mode(pdf_text_state_t *pts, int mode);
int pdf_modify_text_render_mode(pdf_text_state_t *pts, int render_mode);
int pdf_set_PaintType0_params (gx_device_pdf *pdev, gs_gstate *pgs,
                               float size, double scaled_width,
                               const pdf_text_state_values_t *ptsv);
int sync_text_state(gx_device_pdf *pdev);
#endif /* gdevpdts_INCLUDED */
