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
#include "gxpath.h"
#include "gserrors.h"
#include "gsutil.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdtf.h"
#include "gdevpdti.h"
#include "gdevpdts.h"
#include "gdevpdtw.h"
#include "gdevpdtt.h"
#include "gdevpdfo.h"

/* ---------------- Private ---------------- */

/* Define the structure for a CharProc pseudo-resource. */
/*typedef struct pdf_char_proc_s pdf_char_proc_t;*/  /* gdevpdfx.h */
struct pdf_char_proc_s {
    pdf_resource_common(pdf_char_proc_t);
    pdf_font_resource_t *font;
    pdf_char_proc_t *char_next;	/* next char_proc for same font */
    int y_offset;		/* of character (0,0) */
    gs_char char_code;
    gs_const_string char_name;
};

/* The descriptor is public for pdf_resource_type_structs. */
gs_public_st_suffix_add2_string1(st_pdf_char_proc, pdf_char_proc_t,
  "pdf_char_proc_t", pdf_char_proc_enum_ptrs, pdf_char_proc_reloc_ptrs,
  st_pdf_resource, font, char_next, char_name);

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

/* Assign a code for a char_proc. */
private int
assign_char_code(gx_device_pdf * pdev, int width)
{
    pdf_bitmap_fonts_t *pbfs = pdev->text->bitmap_fonts;
    pdf_font_resource_t *pdfont = pbfs->open_font; /* Type 3 */
    int c, code;

    if (pbfs->bitmap_encoding_id == 0)
	pbfs->bitmap_encoding_id = pdf_obj_ref(pdev);
    if (pdfont == 0 || pdfont->u.simple.LastChar == 255 ||
	!pbfs->use_open_font
	) {
	/* Start a new synthesized font. */
	char *pc;

	code = pdf_font_type3_alloc(pdev, &pdfont, pdf_write_contents_bitmap);
	if (code < 0)
	    return code;
        pdfont->u.simple.s.type3.bitmap_font = true;
	if (pbfs->open_font == 0)
	    pdfont->rname[0] = 0;
	else
	    strcpy(pdfont->rname, pbfs->open_font->rname);
	pdfont->u.simple.s.type3.FontBBox.p.x = 0;
	pdfont->u.simple.s.type3.FontBBox.p.y = 0;
	pdfont->u.simple.s.type3.FontBBox.q.x = 1000;
	pdfont->u.simple.s.type3.FontBBox.q.y = 1000;
	gs_make_identity(&pdfont->u.simple.s.type3.FontMatrix);
	/*
	 * We "increment" the font name as a radix-26 "number".
	 * This cannot possibly overflow.
	 */
	for (pc = pdfont->rname; *pc == 'Z'; ++pc)
	    *pc = '@';
	if ((*pc)++ == 0)
	    *pc = 'A', pc[1] = 0;
	pbfs->open_font = pdfont;
	pbfs->use_open_font = true;
    }
    c = ++(pdfont->u.simple.LastChar);
    pdfont->Widths[c] = pdev->char_width.x;
    if (c > pbfs->max_embedded_code)
	pbfs->max_embedded_code = c;

    /* Synthezise ToUnicode CMap :*/
    {	gs_text_enum_t *pte = pdev->pte;
        gs_font *font = pte->current_font;

	code = pdf_add_ToUnicode(pdev, font, pdfont, pte->returned.current_glyph, c); 
	if (code < 0)
	    return code;
    }
    return c;
}

/* Write the contents of a Type 3 bitmap or vector font resource. */
int
pdf_write_contents_bitmap(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    stream *s = pdev->strm;
    const pdf_char_proc_t *pcp;
    long diff_id = 0;
    int code;

    if (pdfont->u.simple.s.type3.bitmap_font)
	diff_id = pdev->text->bitmap_fonts->bitmap_encoding_id;
    else {
	/* See comment in pdf_write_encoding. */
        diff_id = pdf_obj_ref(pdev);
    }
    code = pdf_write_encoding_ref(pdev, pdfont, diff_id);
    if (code < 0)
	return code;
    stream_puts(s, "/CharProcs <<");
    /* Write real characters. */
    for (pcp = pdfont->u.simple.s.type3.char_procs; pcp;
	 pcp = pcp->char_next
	 ) {
	if (pdfont->u.simple.s.type3.bitmap_font)
	    pprintld2(s, "/a%ld %ld 0 R\n", (long)pcp->char_code,
		      pdf_char_proc_id(pcp));
	else {
	    pdf_put_name(pdev, pcp->char_name.data, pcp->char_name.size);
	    pprintld1(s, " %ld 0 R\n", pdf_char_proc_id(pcp));
	}
    }
    stream_puts(s, ">>");
    pprintg6(s, "/FontMatrix[%g %g %g %g %g %g]", 
	    (float)pdfont->u.simple.s.type3.FontMatrix.xx,
	    (float)pdfont->u.simple.s.type3.FontMatrix.xy,
	    (float)pdfont->u.simple.s.type3.FontMatrix.yx,
	    (float)pdfont->u.simple.s.type3.FontMatrix.yy,
	    (float)pdfont->u.simple.s.type3.FontMatrix.tx,
	    (float)pdfont->u.simple.s.type3.FontMatrix.ty);
    code = pdf_finish_write_contents_type3(pdev, pdfont);
    if (code < 0)
	return code;
    s = pdev->strm; /* pdf_finish_write_contents_type3 changes pdev->strm . */
    if (!pdfont->u.simple.s.type3.bitmap_font && diff_id > 0) {
	code = pdf_write_encoding(pdev, pdfont, diff_id, 0);
	if (code < 0)
	    return code;
    }
    return 0;
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

/* Begin a CharProc for a synthesized font. */
private int
pdf_begin_char_proc_generic(gx_device_pdf * pdev, pdf_font_resource_t *pdfont,
		    gs_id id, gs_char char_code, 
		    pdf_char_proc_t ** ppcp, pdf_stream_position_t * ppos)
{
    pdf_resource_t *pres;
    pdf_char_proc_t *pcp;
    int code;

    code = pdf_begin_resource(pdev, resourceCharProc, id, &pres);
    if (code < 0)
	return code;
    pcp = (pdf_char_proc_t *) pres;
    pcp->font = pdfont;
    pcp->char_next = pdfont->u.simple.s.type3.char_procs;
    pdfont->u.simple.s.type3.char_procs = pcp;
    pcp->char_code = char_code;
    pres->object->written = true;
    pcp->char_name.data = 0; 
    pcp->char_name.size = 0;

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
    *ppcp = pcp;
    return 0;
}

/* Begin a CharProc for a synthesized (bitmap) font. */
int
pdf_begin_char_proc(gx_device_pdf * pdev, int w, int h, int x_width,
		    int y_offset, gs_id id, pdf_char_proc_t ** ppcp,
		    pdf_stream_position_t * ppos)
{
    int char_code = assign_char_code(pdev, x_width);
    pdf_bitmap_fonts_t *const pbfs = pdev->text->bitmap_fonts; 
    pdf_font_resource_t *font = pbfs->open_font; /* Type 3 */
    int code = pdf_begin_char_proc_generic(pdev, font, id, char_code, ppcp, ppos);
    
    if (code < 0)
	return code;
    (*ppcp)->y_offset = y_offset;
    font->u.simple.s.type3.FontBBox.p.y =
	min(font->u.simple.s.type3.FontBBox.p.y, y_offset);
    font->u.simple.s.type3.FontBBox.q.x =
	max(font->u.simple.s.type3.FontBBox.q.x, w);
    font->u.simple.s.type3.FontBBox.q.y =
	max(font->u.simple.s.type3.FontBBox.q.y, y_offset + h);
    font->u.simple.s.type3.max_y_offset =
	max(font->u.simple.s.type3.max_y_offset, h + (h >> 2));
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
    pdf_append_chars(pdev, &ch, 1, pdfont->Widths[ch] * pimat->xx, 0.0, false);
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

/*
 * Install charproc accumulator for a Type 3 font.
 */
int
pdf_install_charproc_accum(gx_device_pdf *pdev, gs_font *font, const double *pw, 
		gs_text_cache_control_t control, gs_char ch, gs_const_string *gnstr)
{
    pdf_font_resource_t *pdfont;
    byte *glyph_usage;
    pdf_resource_t *pres;
    pdf_char_proc_t *pcp;
    double *real_widths;
    int char_cache_size, width_cache_size;
    int code;

    code = pdf_attached_font_resource(pdev, font, &pdfont,
		&glyph_usage, &real_widths, &char_cache_size, &width_cache_size);
    if (code < 0)
	return code;
    if (ch != GS_NO_CHAR) {
	int i;
	gs_glyph glyph0 = font->procs.encode_char(font, ch, GLYPH_SPACE_NAME);

	if (ch >= char_cache_size || ch >= width_cache_size)
	    return_error(gs_error_unregistered);
	real_widths[ch * 2    ] = pdfont->Widths[ch] = pw[font->WMode ? 6 : 0];
	real_widths[ch * 2 + 1] = pw[font->WMode ? 7 : 1];
	glyph_usage[ch / 8] |= 0x80 >> (ch & 7);
	pdfont->used[ch >> 3] |= 0x80 >> (ch & 7);
	if (pdfont->u.simple.v != NULL && font->WMode) {
	    pdfont->u.simple.v[ch].x = pw[8];
	    pdfont->u.simple.v[ch].y = pw[9];
	}
	for (i = 0; i < 256; i++) {
	    gs_glyph glyph = font->procs.encode_char(font, i, 
			font->FontType == ft_user_defined ? GLYPH_SPACE_NOGEN
							  : GLYPH_SPACE_NAME);

	    if (glyph == glyph0) {
		real_widths[i * 2    ] = real_widths[ch * 2    ];
		real_widths[i * 2 + 1] = real_widths[ch * 2 + 1];
		glyph_usage[i / 8] |= 0x80 >> (i & 7);
		pdfont->used[i >> 3] |= 0x80 >> (i & 7);
		pdfont->u.simple.v[i] = pdfont->u.simple.v[ch];
		pdfont->Widths[i] = pdfont->Widths[ch];
	    }
	}
    }
    code = pdf_enter_substream(pdev, resourceCharProc, gs_next_ids(1), &pres);
    if (code < 0)
	return code;
    pcp = (pdf_char_proc_t *) pres;
    pcp->font = pdfont;
    pcp->char_next = pdfont->u.simple.s.type3.char_procs;
    pdfont->u.simple.s.type3.char_procs = pcp;
    pcp->char_code = ch;
    pcp->char_name = *gnstr;
    pdev->skip_colors = true;
    if (control == TEXT_SET_CHAR_WIDTH)
	pprintg2(pdev->strm, "%g %g d0\n", (float)pw[0], (float)pw[1]);
    else
	pprintg6(pdev->strm, "%g %g %g %g %g %g d1\n", 
	    (float)pw[0], (float)pw[1], (float)pw[2], 
	    (float)pw[3], (float)pw[4], (float)pw[5]);
    return 0;
}

/*
 * Enter the substream accumulation mode.
 */
int
pdf_enter_substream(gx_device_pdf *pdev, pdf_resource_type_t rtype, 
			    gs_id id, pdf_resource_t **ppres) 
{
    int sbstack_ptr = pdev->sbstack_depth;
    stream *s, *save_strm = pdev->strm;
    pdf_resource_t *pres;
    pdf_data_writer_t writer;
    int code;
    static const pdf_filter_names_t fnames = {
	PDF_FILTER_NAMES
    };

    if (pdev->sbstack_depth >= pdev->sbstack_size)
	return_error(gs_error_unregistered); /* Must not happen. */
    code = pdf_alloc_aside(pdev, PDF_RESOURCE_CHAIN(pdev, rtype, id),
		pdf_resource_type_structs[rtype], &pres, 0);
    if (code < 0)
	return code;
    cos_become(pres->object, cos_type_stream);
    s = cos_write_stream_alloc((cos_stream_t *)pres->object, pdev, "pdf_enter_substream");
    if (s == 0)
	return_error(gs_error_VMerror);
    if (pdev->sbstack[sbstack_ptr].text_state == 0) {
	pdev->sbstack[sbstack_ptr].text_state = pdf_text_state_alloc(pdev->pdf_memory);
	if (pdev->sbstack[sbstack_ptr].text_state == 0)
	    return_error(gs_error_VMerror);
    }
    pdev->strm = s;
    code = pdf_begin_data_stream(pdev, &writer,
			     DATA_STREAM_NOT_BINARY | DATA_STREAM_NOLENGTH |
			     (pdev->CompressFonts ? DATA_STREAM_COMPRESS : 0));
    if (code < 0) {
	pdev->strm = save_strm;
	return code;
    }
    code = pdf_put_filters((cos_dict_t *)pres->object, pdev, writer.binary.strm, &fnames);
    if (code < 0) {
	pdev->strm = save_strm;
	return code;
    }
    pdev->strm = writer.binary.strm;
    pdev->sbstack[sbstack_ptr].context = pdev->context;
    pdf_text_state_copy(pdev->sbstack[sbstack_ptr].text_state, pdev->text->text_state);
    pdf_set_text_state_default(pdev->text->text_state);
    pdev->sbstack[sbstack_ptr].clip_path = pdev->clip_path;
    pdev->clip_path = 0;
    pdev->sbstack[sbstack_ptr].clip_path_id = pdev->clip_path_id;
    pdev->clip_path_id = pdev->no_clip_path_id;
    pdev->sbstack[sbstack_ptr].vgstack_bottom = pdev->vgstack_bottom;
    pdev->vgstack_bottom = pdev->vgstack_depth;
    pdev->sbstack[sbstack_ptr].strm = save_strm;
    pdev->sbstack[sbstack_ptr].procsets = pdev->procsets;
    pdev->sbstack[sbstack_ptr].substream_Resources = pdev->substream_Resources;
    pdev->sbstack[sbstack_ptr].skip_colors = pdev->skip_colors;
    pdev->skip_colors = false;
    pdev->sbstack_depth++;
    pdev->procsets = 0;
    pdev->context = PDF_IN_STREAM;
    pdf_reset_graphics(pdev);
    *ppres = pres;
    return 0;
}

/*
 * Exit the substream accumulation mode.
 */
int
pdf_exit_substream(gx_device_pdf *pdev) 
{
    int code = pdf_open_contents(pdev, PDF_IN_STREAM), code1;
    int sbstack_ptr;
    stream *s = pdev->strm;
    pdf_procset_t procsets;

    if (pdev->sbstack_depth <= 0)
	return_error(gs_error_unregistered); /* Must not happen. */
    sbstack_ptr = pdev->sbstack_depth - 1;
    while (pdev->vgstack_depth > pdev->vgstack_bottom) {
	code1 = pdf_restore_viewer_state(pdev, s);
	if (code >= 0)
	    code = code1;
    }
    if (pdev->clip_path != 0)
	gx_path_free(pdev->clip_path, "pdf_end_charproc_accum");
    {	/* We should call pdf_end_data here, but we don't want to put pdf_data_writer_t
	   into pdf_substream_save stack to simplify garbager descriptors. 
	   Use a lower level functions instead that. */
	int status = s_close_filters(&s, cos_write_stream_from_pipeline(s));
	cos_stream_t *pcs = cos_stream_from_pipeline(s);


	if (status < 0 && code >=0)
	     code = gs_note_error(gs_error_ioerror);
	pcs->is_open = false;
    }
    sclose(s);
    pdev->context = pdev->sbstack[sbstack_ptr].context;
    pdf_text_state_copy(pdev->text->text_state, pdev->sbstack[sbstack_ptr].text_state);
    pdev->clip_path = pdev->sbstack[sbstack_ptr].clip_path;
    pdev->sbstack[sbstack_ptr].clip_path = 0;
    pdev->clip_path_id = pdev->sbstack[sbstack_ptr].clip_path_id;
    pdev->vgstack_bottom = pdev->sbstack[sbstack_ptr].vgstack_bottom;
    pdev->strm = pdev->sbstack[sbstack_ptr].strm;
    pdev->sbstack[sbstack_ptr].strm = 0;
    procsets = pdev->procsets;
    pdev->procsets = pdev->sbstack[sbstack_ptr].procsets;
    pdev->substream_Resources = pdev->sbstack[sbstack_ptr].substream_Resources;
    pdev->sbstack[sbstack_ptr].substream_Resources = 0;
    pdev->skip_colors = pdev->sbstack[sbstack_ptr].skip_colors;
    pdev->sbstack_depth = sbstack_ptr;
    return code;
}

/*
 * Complete charproc accumulation for a Type 3 font.
 */
int
pdf_end_charproc_accum(gx_device_pdf *pdev) 
{
    return pdf_exit_substream(pdev);
}

/* Add procsets to substream Resources. */
int
pdf_add_procsets(cos_dict_t *pcd, pdf_procset_t procsets)
{
    char str[5 + 7 + 7 + 7 + 5 + 2];
    cos_value_t v;

    strcpy(str, "[/PDF");
    if (procsets & ImageB)
	strcat(str, "/ImageB");
    if (procsets & ImageC)
	strcat(str, "/ImageC");
    if (procsets & ImageI)
	strcat(str, "/ImageI");
    if (procsets & Text)
	strcat(str, "/Text");
    strcat(str, "]");
    cos_string_value(&v, (byte *)str, strlen(str));
    return cos_dict_put_c_key(pcd, "/ProcSet", &v);
}

/* Add a resource to substream Resources. */
int
pdf_add_resource(gx_device_pdf *pdev, cos_dict_t *pcd, const char *key, pdf_resource_t *pres)
{
    if (pcd != 0) {
	const cos_value_t *v = cos_dict_find(pcd, (const byte *)key, strlen(key));
	cos_dict_t *list;
	int code;
	char buf[10 + (sizeof(long) * 8 / 3 + 1)], buf1[sizeof(pres->rname) + 1];

	sprintf(buf, "%ld 0 R\n", pres->object->id);
	if (v != NULL) {
	    if (v->value_type != COS_VALUE_OBJECT && 
		v->value_type != COS_VALUE_RESOURCE)
		return_error(gs_error_unregistered); /* Must not happen. */
	    list = (cos_dict_t *)v->contents.object;	
	    if (list->cos_procs != &cos_dict_procs)
		return_error(gs_error_unregistered); /* Must not happen. */
	} else {
	    list = cos_dict_alloc(pdev, "pdf_add_resource");
	    if (list == NULL)
		return_error(gs_error_VMerror);
	    code = cos_dict_put_c_key_object((cos_dict_t *)pcd, key, (cos_object_t *)list);
	    if (code < 0)
		return code;
	}
	buf1[0] = '/';
	strcpy(buf1 + 1, pres->rname);
	return cos_dict_put_string(list, (const byte *)buf1, strlen(buf1),
			(const byte *)buf, strlen(buf));
    }
    return 0;
}

