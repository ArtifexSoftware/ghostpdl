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
/* Bitmap font implementation for pdfwrite */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevpdfx.h"
#include "gdevpdtf.h"
#include "gdevpdti.h"
#include "gdevpdts.h"
#include "gdevpdtw.h"

/* ---------------- Private ---------------- */

/* Define the structure for a CharProc pseudo-resource. */
/*typedef struct pdf_char_proc_s pdf_char_proc_t;*/  /* gdevpdfx.h */
struct pdf_char_proc_s {
    pdf_resource_common(pdf_char_proc_t);
    pdf_font_resource_t *font;
    pdf_char_proc_t *char_next;	/* next char_proc for same font */
    int y_offset;		/* of character (0,0) */
    byte char_code;
};

/* The descriptor is public for pdf_resource_type_structs. */
gs_public_st_suffix_add2(st_pdf_char_proc, pdf_char_proc_t,
  "pdf_char_proc_t", pdf_char_proc_enum_ptrs, pdf_char_proc_reloc_ptrs,
  st_pdf_resource, font, char_next);

/* Define the state structure for tracking bitmap fonts. */
/*typedef struct pdf_bitmap_fonts_s pdf_bitmap_fonts_t;*/
struct pdf_bitmap_fonts_s {
    pdf_font_resource_t *open_font;  /* current Type 3 synthesized font */
    bool use_open_font;		/* if false, start new open_font */
    long bitmap_encoding_id;
    int max_embedded_code;	/* max Type 3 code used */
};
gs_private_st_ptrs1(st_pdf_bitmap_fonts, pdf_bitmap_fonts_t,
  "pdf_bitmap_fonts_t", pdf_bitmap_fonts_enum_ptrs,
  pdf_bitmap_fonts_reloc_ptrs, open_font);

inline private long
pdf_char_proc_id(const pdf_char_proc_t *pcp)
{
    return pdf_resource_id((const pdf_resource_t *)pcp);
}

private int write_contents_bitmap(gx_device_pdf *, pdf_font_resource_t *);

/* Assign a code for a char_proc. */
private int
assign_char_code(gx_device_pdf * pdev, int width)
{
    pdf_bitmap_fonts_t *pbfs = pdev->text->bitmap_fonts;
    pdf_font_resource_t *font = pbfs->open_font; /* Type 3 */
    int c;

    if (pbfs->bitmap_encoding_id == 0)
	pbfs->bitmap_encoding_id = pdf_obj_ref(pdev);
    if (font == 0 || font->u.simple.LastChar == 255 ||
	!pbfs->use_open_font
	) {
	/* Start a new synthesized font. */
	int code = pdf_font_type3_alloc(pdev, &font, write_contents_bitmap);
	char *pc;

	if (code < 0)
	    return code;
	if (pbfs->open_font == 0)
	    font->rname[0] = 0;
	else
	    strcpy(font->rname, pbfs->open_font->rname);
	/*
	 * We "increment" the font name as a radix-26 "number".
	 * This cannot possibly overflow.
	 */
	for (pc = font->rname; *pc == 'Z'; ++pc)
	    *pc = '@';
	if ((*pc)++ == 0)
	    *pc = 'A', pc[1] = 0;
	pbfs->open_font = font;
	pbfs->use_open_font = true;
    }
    c = ++(font->u.simple.LastChar);
    font->Widths[c] = pdev->char_width.x;
    if (c > pbfs->max_embedded_code)
	pbfs->max_embedded_code = c;
    return c;
}

/* Write the contents of a Type 3 bitmap font resource. */
private int
write_contents_bitmap(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    stream *s = pdev->strm;

    pprintld1(s, "/Encoding %ld 0 R/CharProcs",
	      pdev->text->bitmap_fonts->bitmap_encoding_id);

    /* Write the CharProcs. */
    {
	const pdf_char_proc_t *pcp;

	stream_puts(s, "<<");
	/* Write real characters. */
	for (pcp = pdfont->u.simple.s.type3.char_procs; pcp;
	     pcp = pcp->char_next
	     ) {
	    pprintld2(s, "/a%ld\n%ld 0 R", (long)pcp->char_code,
		      pdf_char_proc_id(pcp));
	}
	stream_puts(s, ">>");
    }

    stream_puts(s, "/FontMatrix[1 0 0 1 0 0]");
    return pdf_finish_write_contents_type3(pdev, pdfont);
}

/* ---------------- Public ---------------- */

/*
 * Allocate and initialize bookkeeping for bitmap fonts.
 */
pdf_bitmap_fonts_t *
pdf_bitmap_fonts_alloc(gs_memory_t *mem)
{
    pdf_bitmap_fonts_t *pbfs =
	gs_alloc_struct(mem, pdf_bitmap_fonts_t, &st_pdf_bitmap_fonts,
			"pdf_bitmap_fonts_alloc");

    if (pbfs == 0)
	return 0;
    memset(pbfs, 0, sizeof(*pbfs));
    pbfs->max_embedded_code = -1;
    return pbfs;
}

/*
 * Update text state at the end of a page.
 */
void
pdf_close_text_page(gx_device_pdf *pdev)
{
    /*
     * When Acrobat Reader 3 prints a file containing a Type 3 font with a
     * non-standard Encoding, it apparently only emits the subset of the
     * font actually used on the page.  Thus, if the "Download Fonts Once"
     * option is selected, characters not used on the page where the font
     * first appears will not be defined, and hence will print as blank if
     * used on subsequent pages.  Thus, we can't allow a Type 3 font to
     * add additional characters on subsequent pages.
     */
    if (pdev->CompatibilityLevel <= 1.2)
	pdev->text->bitmap_fonts->use_open_font = false;
}

/* Return the Y offset for a bitmap character image. */
int
pdf_char_image_y_offset(const gx_device_pdf *pdev, int x, int y, int h)
{
    const pdf_text_data_t *const ptd = pdev->text;
    gs_point pt;
    int max_off, off;

    pdf_text_position(pdev, &pt);
    if (x < pt.x)
	return 0;
    max_off = (ptd->bitmap_fonts->open_font == 0 ? 0 :
	       ptd->bitmap_fonts->open_font->u.simple.s.type3.max_y_offset);
    off = (y + h) - (int)(pt.y + 0.5);
    if (off < -max_off || off > max_off)
	off = 0;
    return off;
}

/* Begin a CharProc for a synthesized (bitmap) font. */
int
pdf_begin_char_proc(gx_device_pdf * pdev, int w, int h, int x_width,
		    int y_offset, gs_id id, pdf_char_proc_t ** ppcp,
		    pdf_stream_position_t * ppos)
{
    pdf_resource_t *pres;
    pdf_char_proc_t *pcp;
    int char_code = assign_char_code(pdev, x_width);
    pdf_bitmap_fonts_t *const pbfs = pdev->text->bitmap_fonts;
    pdf_font_resource_t *font = pbfs->open_font; /* Type 3 */
    int code;

    if (char_code < 0)
	return char_code;
    code = pdf_begin_resource(pdev, resourceCharProc, id, &pres);
    if (code < 0)
	return code;
    pcp = (pdf_char_proc_t *) pres;
    pcp->font = font;
    pcp->char_next = font->u.simple.s.type3.char_procs;
    font->u.simple.s.type3.char_procs = pcp;
    pcp->char_code = char_code;
    pcp->y_offset = y_offset;
    font->u.simple.s.type3.FontBBox.p.y =
	min(font->u.simple.s.type3.FontBBox.p.y, y_offset);
    font->u.simple.s.type3.FontBBox.q.x =
	max(font->u.simple.s.type3.FontBBox.q.x, w);
    font->u.simple.s.type3.FontBBox.q.y =
	max(font->u.simple.s.type3.FontBBox.q.y, y_offset + h);
    font->u.simple.s.type3.max_y_offset =
	max(font->u.simple.s.type3.max_y_offset, h + (h >> 2));
    *ppcp = pcp;
    {
	stream *s = pdev->strm;

	/*
	 * The resource file is positionable, so rather than use an
	 * object reference for the length, we'll go back and fill it in
	 * at the end of the definition.  Take 1M as the longest
	 * definition we can handle.  (This used to be 10K, but there was
	 * a real file that exceeded this limit.)
	 */
	stream_puts(s, "<</Length       >>stream\n");
	ppos->start_pos = stell(s);
    }
    return 0;
}

/* End a CharProc. */
int
pdf_end_char_proc(gx_device_pdf * pdev, pdf_stream_position_t * ppos)
{
    stream *s = pdev->strm;
    long start_pos = ppos->start_pos;
    long end_pos = stell(s);
    long length = end_pos - start_pos;

    if (length > 999999)
	return_error(gs_error_limitcheck);
    sseek(s, start_pos - 15);
    pprintd1(s, "%d", length);
    sseek(s, end_pos);
    stream_puts(s, "endstream\n");
    pdf_end_separate(pdev);
    return 0;
}

/* Put out a reference to an image as a character in a synthesized font. */
int
pdf_do_char_image(gx_device_pdf * pdev, const pdf_char_proc_t * pcp,
		  const gs_matrix * pimat)
{
    pdf_font_resource_t *pdfont = pcp->font;
    byte ch = pcp->char_code;
    pdf_text_state_values_t values;

    values.character_spacing = 0;
    values.pdfont = pdfont;
    values.size = 1;
    values.matrix = *pimat;
    values.matrix.ty -= pcp->y_offset;
    values.render_mode = 0;
    values.word_spacing = 0;
    pdf_set_text_state_values(pdev, &values);
    pdf_append_chars(pdev, &ch, 1, pdfont->Widths[ch] * pimat->xx, 0.0);
    return 0;
}

/*
 * Write the Encoding for bitmap fonts, if needed.
 */
int
pdf_write_bitmap_fonts_Encoding(gx_device_pdf *pdev)
{
    const pdf_bitmap_fonts_t *pbfs = pdev->text->bitmap_fonts;

    if (pbfs->bitmap_encoding_id) {
	stream *s;
	int i;

	pdf_open_separate(pdev, pbfs->bitmap_encoding_id);
	s = pdev->strm;
	/*
	 * Even though the PDF reference documentation says that a
	 * BaseEncoding key is required unless the encoding is
	 * "based on the base font's encoding" (and there is no base
	 * font in this case), Acrobat 2.1 gives an error if the
	 * BaseEncoding key is present.
	 */
	stream_puts(s, "<</Type/Encoding/Differences[0");
	for (i = 0; i <= pbfs->max_embedded_code; ++i) {
	    if (!(i & 15))
		stream_puts(s, "\n");
	    pprintd1(s, "/a%d", i);
	}
	stream_puts(s, "\n] >>\n");
	pdf_end_separate(pdev);
    }
    return 0;
}
