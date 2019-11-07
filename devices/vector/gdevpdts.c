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


/* Text state management for pdfwrite */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdtx.h"
#include "gdevpdtf.h"		/* for pdfont->FontType */
#include "gdevpdts.h"
#include "gdevpdtt.h"
#include "gdevpdti.h"

/* ================ Types and structures ================ */

#define TEXT_BUFFER_DEFAULT\
    { { 0, 0 } },		/* moves */\
    { 0 },			/* chars */\
    0,				/* count_moves */\
    0				/* count_chars */

static const pdf_text_state_t ts_default = {
    /* State as seen by client */
    { TEXT_STATE_VALUES_DEFAULT },	/* in */
    { 0, 0 },			/* start */
    { TEXT_BUFFER_DEFAULT },	/* buffer */
    0,				/* wmode */
    /* State relative to content stream */
    { TEXT_STATE_VALUES_DEFAULT },	/* out */
    0,				/* leading */
    0 /*false*/,		/* use_leading */
    0 /*false*/,		/* continue_line */
    { 0, 0 },			/* line_start */
    { 0, 0 },			/* output position */
    0.0,                /* PaintType0Width */
    1 /* false */       /* can_use_TJ */
};
/* GC descriptor */
gs_private_st_ptrs2(st_pdf_text_state, pdf_text_state_t,  "pdf_text_state_t",
                    pdf_text_state_enum_ptrs, pdf_text_state_reloc_ptrs,
                    in.pdfont, out.pdfont);

/* ================ Procedures ================ */

/* ---------------- Private ---------------- */

/*
 * Append a writing-direction movement to the text being accumulated.  If
 * the buffer is full, or the requested movement is not in writing
 * direction, return <0 and do nothing.  (This is different from
 * pdf_append_chars.)  Requires pts->buffer.count_chars > 0.
 */
static int
append_text_move(pdf_text_state_t *pts, double dw)
{
    int count = pts->buffer.count_moves;
    int pos = pts->buffer.count_chars;
    double rounded;

    if (count > 0 && pts->buffer.moves[count - 1].index == pos) {
        /* Merge adjacent moves. */
        dw += pts->buffer.moves[--count].amount;
    }
    /* Round dw if it's very close to an integer. */
    rounded = floor(dw + 0.5);
    if (fabs(dw - rounded) < 0.001)
        dw = rounded;
    if (dw < -MAX_USER_COORD) {
        /* Acrobat reader 4.0c, 5.0 can't handle big offsets.
           Adobe Reader 6 can. */
        return -1;
    }
    if (dw != 0) {
        if (count == MAX_TEXT_BUFFER_MOVES)
            return -1;
        pts->buffer.moves[count].index = pos;
        pts->buffer.moves[count].amount = dw;
        ++count;
    }
    pts->buffer.count_moves = count;
    return 0;
}

/*
 * Set *pdist to the distance (dx,dy), in the space defined by *pmat.
 */
static int
set_text_distance(gs_point *pdist, double dx, double dy, const gs_matrix *pmat)
{
    int code;
    double rounded;

    if (dx > 1e38 || dy > 1e38)
        code = gs_error_undefinedresult;
    else
        code = gs_distance_transform_inverse(dx, dy, pmat, pdist);

    if (code == gs_error_undefinedresult) {
        /* The CTM is degenerate.
           Can't know the distance in user space.
           Set zero because we believe it is not important for rendering.
           We want to copy the text to PDF to make it searchable.
           Bug 689006.
         */
        pdist->x = pdist->y = 0;
    } else if (code < 0)
        return code;
    /* If the distance is very close to integers, round it. */
    if (fabs(pdist->x - (rounded = floor(pdist->x + 0.5))) < 0.0005)
        pdist->x = rounded;
    if (fabs(pdist->y - (rounded = floor(pdist->y + 0.5))) < 0.0005)
        pdist->y = rounded;
    return 0;
}

/*
 * Test whether the transformation parts of two matrices are compatible.
 */
static bool
matrix_is_compatible(const gs_matrix *pmat1, const gs_matrix *pmat2)
{
    return (pmat2->xx == pmat1->xx && pmat2->xy == pmat1->xy &&
            pmat2->yx == pmat1->yx && pmat2->yy == pmat1->yy);
}

/*
 * Try to handle a change of text position with TJ or a space
 * character.  If successful, return >=0, if not, return <0.
 */
static int
add_text_delta_move(gx_device_pdf *pdev, const gs_matrix *pmat)
{
    pdf_text_state_t *const pts = pdev->text->text_state;

    if (matrix_is_compatible(pmat, &pts->in.matrix)) {
        double dx = pmat->tx - pts->in.matrix.tx,
            dy = pmat->ty - pts->in.matrix.ty;
        gs_point dist;
        double dw, dnotw, tdw;
        int code;

        code = set_text_distance(&dist, dx, dy, pmat);
        if (code < 0)
            return code;
        if (pts->wmode)
            dw = dist.y, dnotw = dist.x;
        else
            dw = dist.x, dnotw = dist.y;
        tdw = dw * -1000.0 / pts->in.size;

        /* can_use_TJ is normally true, it is false only when we get a
         * x/y/xyshow, and the width != real_width. In this case we cannot
         * be certain of exactly how we got there. If its a PDF file with
         * a /Widths override, and the operation is an x/y/xyshow (which
         * will happen if the FontMatrix is nither horizontal not vertical)
         * then we don't want to use a TJ as that will apply the Width once
         * for the xhow and once for the Width override. Otherwise, we do
         * want to use TJ as it makes for smaller files.
         */
        if (pts->can_use_TJ && dnotw == 0 && pts->buffer.count_chars > 0 &&
            /*
             * Acrobat Reader limits the magnitude of user-space
             * coordinates.  Also, AR apparently doesn't handle large
             * positive movement values (negative X displacements), even
             * though the PDF Reference says this bug was fixed in AR3.
             *
             * Old revisions used the upper threshold 1000 for tdw,
             * but it appears too big when a font sets a too big
             * character width in setcachedevice. Particularly this happens
             * with a Type 3 font generated by Aldus Freehand 4.0
             * to represent a texture - see bug #687051.
             * The problem is that when the Widths is multiplied
             * to the font size, the viewer represents the result
             * with insufficient fraction bits to represent the precise width.
             * We work around that problem here restricting tdw
             * with a smaller threshold 990. Our intention is to
             * disable Tj when the real glyph width appears smaller
             * than 1% of the width specified in setcachedevice.
             * A Td instruction will be generated instead.
             * Note that the value 990 is arbitrary and may need a
             * further adjustment.
             */
             /* Revised the above. It seems unreasonable to use a fixed
              * value which is not based on the point size, when the problem is
              * caused by a large point size being multiplied by the width. The
              * original fix also caused bitmap fonts (from PCL and other sources)
              * to fail to use kerning, as these fonts are scaled to 1 point and
              * therefore use large kerning values. Instead we check the kerned value
              * multiplied by the point size of the font.
              */
            (tdw >= -MAX_USER_COORD && (tdw * pts->in.size) < MAX_USER_COORD)
            ) {
            /* Use TJ. */
            int code;

            if (tdw < MAX_USER_COORD || pdev->CompatibilityLevel > 1.4)
                code = append_text_move(pts, tdw);
            else
                return -1;

            if (code >= 0)
                goto finish;
        }
    }
    return -1;
 finish:
    pts->in.matrix = *pmat;
    return 0;
}

/*
 * Set the text matrix for writing text.  The translation component of the
 * matrix is the text origin.  If the non-translation components of the
 * matrix differ from the current ones, write a Tm command; if there is only
 * a Y translation, set use_leading so the next text string will be written
 * with ' rather than Tj; otherwise, write a Td command.
 */
static int
pdf_set_text_matrix(gx_device_pdf * pdev)
{
    pdf_text_state_t *pts = pdev->text->text_state;
    stream *s = pdev->strm;

    pts->use_leading = false;
    if (matrix_is_compatible(&pts->out.matrix, &pts->in.matrix)) {
        gs_point dist;
        int code;

        code = set_text_distance(&dist, pts->start.x - pts->line_start.x,
                          pts->start.y - pts->line_start.y, &pts->in.matrix);
        if (code < 0)
            return code;
        if (dist.x == 0 && dist.y < 0) {
            /* Use TL, if needed, and T* or '. */
            float dist_y = (float)-dist.y;

            if (fabs(pts->leading - dist_y) > 0.0005) {
                pprintg1(s, "%g TL\n", dist_y);
                pts->leading = dist_y;
            }
            pts->use_leading = true;
        } else {
            /* Use Td. */
            pprintg2(s, "%g %g Td\n", dist.x, dist.y);
        }
    } else {			/* Use Tm. */
        /*
         * See stream_to_text in gdevpdfu.c for why we need the following
         * matrix adjustments.
         */
        double sx = 72.0 / pdev->HWResolution[0],
            sy = 72.0 / pdev->HWResolution[1], ax = sx, bx = sx, ay = sy, by = sy;

        /* We have a precision limit on decimal places with %g, make sure
         * we don't end up with values which will be truncated to 0
         */
        if (pts->in.matrix.xx != 0 && fabs(pts->in.matrix.xx) * ax < 0.00000001)
            ax = ceil(0.00000001 / pts->in.matrix.xx);
        if (pts->in.matrix.xy != 0 && fabs(pts->in.matrix.xy) * ay < 0.00000001)
            ay = ceil(0.00000001 / pts->in.matrix.xy);
        if (pts->in.matrix.yx != 0 && fabs(pts->in.matrix.yx) * bx < 0.00000001)
            bx = ceil(0.00000001 / pts->in.matrix.yx);
        if (pts->in.matrix.yy != 0 && fabs(pts->in.matrix.yy) * by < 0.00000001)
            by = ceil(0.00000001 / pts->in.matrix.yy);
        pprintg6(s, "%g %g %g %g %g %g Tm\n",
                 pts->in.matrix.xx * ax, pts->in.matrix.xy * ay,
                 pts->in.matrix.yx * bx, pts->in.matrix.yy * by,
                 pts->start.x * sx, pts->start.y * sy);
    }
    pts->line_start.x = pts->start.x;
    pts->line_start.y = pts->start.y;
    pts->out.matrix = pts->in.matrix;
    return 0;
}

/* ---------------- Public ---------------- */

/*
 * Allocate and initialize text state bookkeeping.
 */
pdf_text_state_t *
pdf_text_state_alloc(gs_memory_t *mem)
{
    pdf_text_state_t *pts =
        gs_alloc_struct(mem, pdf_text_state_t, &st_pdf_text_state,
                        "pdf_text_state_alloc");

    if (pts == 0)
        return 0;
    *pts = ts_default;
    return pts;
}

/*
 * Set the text state to default values.
 */
void
pdf_set_text_state_default(pdf_text_state_t *pts)
{
    *pts = ts_default;
}

/*
 * Copy the text state.
 */
void
pdf_text_state_copy(pdf_text_state_t *pts_to, pdf_text_state_t *pts_from)
{
    *pts_to = *pts_from;
}

/*
 * Reset the text state to its condition at the beginning of the page.
 */
void
pdf_reset_text_page(pdf_text_data_t *ptd)
{
    pdf_set_text_state_default(ptd->text_state);
}

/*
 * Reset the text state after a grestore.
 */
void
pdf_reset_text_state(pdf_text_data_t *ptd)
{
    pdf_text_state_t *pts = ptd->text_state;

    pts->out = ts_default.out;
    pts->leading = 0;
}

/*
 * Transition from stream context to text context.
 */
int
pdf_from_stream_to_text(gx_device_pdf *pdev)
{
    pdf_text_state_t *pts = pdev->text->text_state;

    gs_make_identity(&pts->out.matrix);
    pts->line_start.x = pts->line_start.y = 0;
    pts->continue_line = false; /* Not sure, probably doesn't matter. */
    pts->buffer.count_chars = 0;
    pts->buffer.count_moves = 0;
    return 0;
}

/*
 *  Flush text from buffer.
 */
static int
flush_text_buffer(gx_device_pdf *pdev)
{
    pdf_text_state_t *pts = pdev->text->text_state;
    stream *s = pdev->strm;

    if (pts->buffer.count_chars != 0) {
        pdf_font_resource_t *pdfont = pts->in.pdfont;
        int code = pdf_assign_font_object_id(pdev, pdfont);

        if (code < 0)
            return code;
        code = pdf_add_resource(pdev, pdev->substream_Resources, "/Font", (pdf_resource_t *)pdfont);
        if (code < 0)
            return code;
    }
    if (pts->buffer.count_moves > 0) {
        int i, cur = 0;

        if (pts->use_leading)
            stream_puts(s, "T*");
        stream_puts(s, "[");
        for (i = 0; i < pts->buffer.count_moves; ++i) {
            int next = pts->buffer.moves[i].index;

            pdf_put_string(pdev, pts->buffer.chars + cur, next - cur);
            pprintg1(s, "%g", pts->buffer.moves[i].amount);
            cur = next;
        }
        if (pts->buffer.count_chars > cur)
            pdf_put_string(pdev, pts->buffer.chars + cur,
                           pts->buffer.count_chars - cur);
        stream_puts(s, "]TJ\n");
    } else {
        pdf_put_string(pdev, pts->buffer.chars, pts->buffer.count_chars);
        stream_puts(s, (pts->use_leading ? "'\n" : "Tj\n"));
    }
    pts->buffer.count_chars = 0;
    pts->buffer.count_moves = 0;
    pts->use_leading = false;
    return 0;
}

/*
 * Transition from string context to text context.
 */
int
sync_text_state(gx_device_pdf *pdev)
{
    pdf_text_state_t *pts = pdev->text->text_state;
    stream *s = pdev->strm;
    int code;

    if (pts->buffer.count_chars == 0)
        return 0;		/* nothing to output */

    if (pts->continue_line)
        return flush_text_buffer(pdev);

    /* Bring text state parameters up to date. */

    if (pts->out.character_spacing != pts->in.character_spacing) {
        pprintg1(s, "%g Tc\n", pts->in.character_spacing);
        pts->out.character_spacing = pts->in.character_spacing;
    }

    if (pts->out.pdfont != pts->in.pdfont || pts->out.size != pts->in.size) {
        pdf_font_resource_t *pdfont = pts->in.pdfont;

        code = pdf_assign_font_object_id(pdev, pdfont);
        if (code < 0)
            return code;
        pprints1(s, "/%s ", pdfont->rname);
        pprintg1(s, "%g Tf\n", pts->in.size);
        pts->out.pdfont = pdfont;
        pts->out.size = pts->in.size;
        /*
         * In PDF, the only place to specify WMode is in the CMap
         * (a.k.a. Encoding) of a Type 0 font.
         */
        pts->wmode =
            (pdfont->FontType == ft_composite ?
             pdfont->u.type0.WMode : 0);
        code = pdf_used_charproc_resources(pdev, pdfont);
        if (code < 0)
            return code;
    }

    if (gs_matrix_compare(&pts->in.matrix, &pts->out.matrix) ||
         ((pts->start.x != pts->out_pos.x || pts->start.y != pts->out_pos.y) &&
          (pts->buffer.count_chars != 0 || pts->buffer.count_moves != 0))) {
        /* pdf_set_text_matrix sets out.matrix = in.matrix */
        code = pdf_set_text_matrix(pdev);
        if (code < 0)
            return code;
    }

    if (pts->out.render_mode != pts->in.render_mode) {
        pprintg1(s, "%g Tr\n", pts->in.render_mode);
        pts->out.render_mode = pts->in.render_mode;
    }

    if (pts->out.word_spacing != pts->in.word_spacing) {
        if (memchr(pts->buffer.chars, 32, pts->buffer.count_chars)) {
            pprintg1(s, "%g Tw\n", pts->in.word_spacing);
            pts->out.word_spacing = pts->in.word_spacing;
        }
    }

    return flush_text_buffer(pdev);
}

int
pdf_from_string_to_text(gx_device_pdf *pdev)
{
    return sync_text_state(pdev);
}

/*
 * Close the text aspect of the current contents part.
 */
void
pdf_close_text_contents(gx_device_pdf *pdev)
{
    /*
     * Clear the font pointer.  This is probably left over from old code,
     * but it is appropriate in case we ever choose in the future to write
     * out and free font resources before the end of the document.
     */
    pdf_text_state_t *pts = pdev->text->text_state;

    pts->in.pdfont = pts->out.pdfont = 0;
    pts->in.size = pts->out.size = 0;
}

/*
 * Test whether a change in render_mode requires resetting the stroke
 * parameters.
 */
bool
pdf_render_mode_uses_stroke(const gx_device_pdf *pdev,
                            const pdf_text_state_values_t *ptsv)
{
    return ((ptsv->render_mode == 1 || ptsv->render_mode == 2 ||
            ptsv->render_mode == 5 || ptsv->render_mode == 6));
}

/*
 * Read the stored client view of text state values.
 */
void
pdf_get_text_state_values(gx_device_pdf *pdev, pdf_text_state_values_t *ptsv)
{
    *ptsv = pdev->text->text_state->in;
}

/*
 * Set wmode to text state.
 */
void
pdf_set_text_wmode(gx_device_pdf *pdev, int wmode)
{
    pdf_text_state_t *pts = pdev->text->text_state;

    pts->wmode = wmode;
}

/*
 * Set the stored client view of text state values.
 */
int
pdf_set_text_state_values(gx_device_pdf *pdev,
                          const pdf_text_state_values_t *ptsv)
{
    pdf_text_state_t *pts = pdev->text->text_state;

    if (pts->buffer.count_chars > 0) {
        int code;

        if (pts->in.character_spacing == ptsv->character_spacing &&
            pts->in.pdfont == ptsv->pdfont && pts->in.size == ptsv->size &&
            pts->in.render_mode == ptsv->render_mode &&
            pts->in.word_spacing == ptsv->word_spacing
            ) {
            if (!gs_matrix_compare(&pts->in.matrix, &ptsv->matrix))
                return 0;
            /* add_text_delta_move sets pts->in.matrix if successful */
            code = add_text_delta_move(pdev, &ptsv->matrix);
            if (code >= 0)
                return 0;
        }
        code = sync_text_state(pdev);
        if (code < 0)
            return code;
    }

    pts->in = *ptsv;
    pts->continue_line = false;
    return 0;
}

/*
 * Transform a distance from unscaled text space (text space ignoring the
 * scaling implied by the font size) to device space.
 */
int
pdf_text_distance_transform(double wx, double wy, const pdf_text_state_t *pts,
                            gs_point *ppt)
{
    return gs_distance_transform(wx, wy, &pts->in.matrix, ppt);
}

/*
 * Return the current (x,y) text position as seen by the client, in
 * unscaled text space.
 */
void
pdf_text_position(const gx_device_pdf *pdev, gs_point *ppt)
{
    pdf_text_state_t *pts = pdev->text->text_state;

    ppt->x = pts->in.matrix.tx;
    ppt->y = pts->in.matrix.ty;
}

int pdf_bitmap_char_update_bbox(gx_device_pdf * pdev,int x_offset, int y_offset, double x, double y)
{
    pdf_text_state_t *pts = pdev->text->text_state;
    gs_rect bbox;

    bbox.p.x = (pts->in.matrix.tx + x_offset) / (pdev->HWResolution[0] / 72);
    bbox.p.y = (pts->in.matrix.ty + y_offset) / (pdev->HWResolution[1] / 72);
    bbox.q.x = bbox.p.x + (x / (pdev->HWResolution[0] / 72));
    bbox.q.y = bbox.p.y + (y / (pdev->HWResolution[0] / 72));

    if (bbox.p.x < pdev->BBox.p.x)
        pdev->BBox.p.x = bbox.p.x;
    if (bbox.p.y < pdev->BBox.p.y)
        pdev->BBox.p.y = bbox.p.y;
    if (bbox.q.x > pdev->BBox.q.x)
        pdev->BBox.q.x = bbox.q.x;
    if (bbox.q.y > pdev->BBox.q.y)
        pdev->BBox.q.y = bbox.q.y;

    return 0;
}
/*
 * Append characters to text being accumulated, giving their advance width
 * in device space.
 */
int
pdf_append_chars(gx_device_pdf * pdev, const byte * str, uint size,
                 double wx, double wy, bool nobreak)
{
    pdf_text_state_t *pts = pdev->text->text_state;
    const byte *p = str;
    uint left = size;

    if (pts->buffer.count_chars == 0 && pts->buffer.count_moves == 0) {
        pts->out_pos.x = pts->start.x = pts->in.matrix.tx;
        pts->out_pos.y = pts->start.y = pts->in.matrix.ty;
    }
    while (left)
        if (pts->buffer.count_chars == MAX_TEXT_BUFFER_CHARS ||
            (nobreak && pts->buffer.count_chars + left > MAX_TEXT_BUFFER_CHARS)) {
            int code = sync_text_state(pdev);

            if (code < 0)
                return code;
            /* We'll keep a continuation of this line in the buffer,
             * but the current input parameters don't correspond to
             * the current position, because the text was broken in a
             * middle with unknown current point.
             * Don't change the output text state parameters
             * until input parameters are changed.
             * pdf_set_text_state_values will reset the 'continue_line' flag
             * at that time.
             */
            pts->continue_line = true;
        } else {
            int code = pdf_open_page(pdev, PDF_IN_STRING);
            uint copy;

            if (code < 0)
                return code;
            copy = min(MAX_TEXT_BUFFER_CHARS - pts->buffer.count_chars, left);
            memcpy(pts->buffer.chars + pts->buffer.count_chars, p, copy);
            pts->buffer.count_chars += copy;
            p += copy;
            left -= copy;
        }
    pts->in.matrix.tx += wx;
    pts->in.matrix.ty += wy;
    pts->out_pos.x += wx;
    pts->out_pos.y += wy;
    return 0;
}

/* Check a new piece of charpath text to see if its safe to combine
 * with a previous text operation using text rendering modes.
 */
bool pdf_compare_text_state_for_charpath(pdf_text_state_t *pts, gx_device_pdf *pdev,
                                         gs_gstate *pgs, gs_font *font,
                                         const gs_text_params_t *text)
{
    int code;
    float size;
    gs_matrix smat, tmat;
    struct pdf_font_resource_s *pdfont;

    /* check to ensure the new text has the same length as the saved text */
    if(text->size != pts->buffer.count_chars)
        return(false);

    if(font->FontType == ft_user_defined ||
        font->FontType == ft_PDF_user_defined ||
        font->FontType == ft_PCL_user_defined ||
        font->FontType == ft_MicroType ||
        font->FontType == ft_GL2_stick_user_defined ||
        font->FontType == ft_GL2_531)
        return(false);

    /* check to ensure the new text has the same data as the saved text */
    if(memcmp(text->data.bytes, &pts->buffer.chars, text->size))
        return(false);

    /* See if the same font is in use by checking the attahced pdfont resource for
     * the currrent font and comparing with the saved text state
     */
    code = pdf_attached_font_resource(pdev, font, &pdfont, NULL, NULL, NULL, NULL);
    if(code < 0)
        return(false);

    if(!pdfont || pdfont != pts->in.pdfont)
        return(false);

    /* Check to see the new text starts at the same point as the saved text.
     * NB! only check 2 decimal places, allow some slack in the match. This
     * still may prove to be too tight a requirement.
     */
    if(fabs(pts->start.x - pgs->current_point.x) > 0.01 ||
       fabs(pts->start.y - pgs->current_point.y) > 0.01)
        return(false);

    size = pdf_calculate_text_size(pgs, pdfont, &font->FontMatrix, &smat, &tmat, font, pdev);

    /* Finally, check the calculated size against the size stored in
     * the text state.
     */
    if(size != pts->in.size)
        return(false);

    return(true);
}

int pdf_get_text_render_mode(pdf_text_state_t *pts)
{
    return(pts->in.render_mode);
}

void pdf_set_text_render_mode(pdf_text_state_t *pts, int mode)
{
    pts->in.render_mode = mode;
}

/* Add a render mode to the rendering mode of the current text.
 * mode 0 = fill
 * mode 1 = stroke
 * mode 2 = clip
 * If the modes are not compatible returns 0. NB currently only
 * a stroke rendering mode is supported.
 */
int pdf_modify_text_render_mode(pdf_text_state_t *pts, int render_mode)
{
    switch (pts->in.render_mode) {
        case 0:
            if (render_mode == 1) {
                pts->in.render_mode = 2;
                return(1);
            }
            break;
        case 1:
            if (render_mode == 1)
                return(1);
            break;
        case 2:
            if (render_mode == 1)
                return(1);
            break;
        case 3:
            if (render_mode == 1) {
                pts->in.render_mode = 1;
                return(1);
            }
            break;
        case 4:
            if (render_mode == 1) {
                pts->in.render_mode = 6;
                return(1);
            }
            break;
        case 5:
            if (render_mode == 1)
                return(1);
            break;
        case 6:
            if (render_mode == 1)
                return(1);
            break;
        case 7:
            if (render_mode == 1) {
                pts->in.render_mode = 5;
                return(1);
            }
            break;
        default:
            break;
    }
    return(0);
}

int pdf_set_PaintType0_params (gx_device_pdf *pdev, gs_gstate *pgs, float size,
                               double scaled_width, const pdf_text_state_values_t *ptsv)
{
    pdf_text_state_t *pts = pdev->text->text_state;
    double saved_width = pgs->line_params.half_width;
    int code;

    /* This routine is used to check if we have accumulated glyphs waiting for output
     * if we do, and we are using a PaintType 0 font (stroke), which is the only way we
     * can get here, then we check to see if the stroke width has changed. If so we want to
     * flush the buffer, and set the new stroke width. This produces:
     * <width> w
     * (text) Tj
     * <new width> w
     * (new text) Tj
     *
     * instead of :
     * <width> w
     * <new width> w
     * (text) Tj
     * (new text) Tj
     */
    if (pts->buffer.count_chars > 0) {
        if (pts->PaintType0Width != scaled_width) {
            pgs->line_params.half_width = scaled_width / 2;
            code = pdf_set_text_state_values(pdev, ptsv);
            if (code < 0)
                return code;
            if (pdev->text->text_state->in.render_mode == ptsv->render_mode){
                code = pdf_prepare_stroke(pdev, pgs, false);
                if (code >= 0)
                    code = gdev_vector_prepare_stroke((gx_device_vector *)pdev,
                                              pgs, NULL, NULL, 1);
            }
            if (code < 0)
                return code;
            pgs->line_params.half_width = saved_width;
            pts->PaintType0Width = scaled_width;
        }
    }
    return 0;
}
