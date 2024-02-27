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


/* Bitmap font implementation for pdfwrite */
#include "memory_.h"
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
#include "gxchar.h"        /* For gs_show_enum */

/* ---------------- Private ---------------- */

/* Define the structure for a CharProc pseudo-resource. */
/*typedef struct pdf_char_proc_s pdf_char_proc_t;*/  /* gdevpdfx.h */
struct pdf_char_proc_s {
    pdf_resource_common(pdf_char_proc_t);
    pdf_char_proc_ownership_t *owner_fonts; /* fonts using this charproc. */
    int y_offset;                /* of character (0,0) */
    int x_offset;                /* of character (0,0) */
    gs_point real_width;        /* Not used with synthesised bitmap fonts. */
    gs_point v;                        /* Not used with synthesised bitmap fonts. */
};

/* The descriptor is public for pdf_resource_type_structs. */
gs_public_st_suffix_add1(st_pdf_char_proc, pdf_char_proc_t,
  "pdf_char_proc_t", pdf_char_proc_enum_ptrs, pdf_char_proc_reloc_ptrs,
  st_pdf_resource, owner_fonts);

struct pdf_char_proc_ownership_s {
    pdf_char_proc_t *char_proc;
    pdf_char_proc_ownership_t *font_next;        /* next char_proc for same font */
    pdf_char_proc_ownership_t *char_next;        /* next char_proc for same charproc */
    pdf_font_resource_t *font;
    gs_char char_code;                /* Character code in PDF font. */
    gs_glyph glyph;                /* Glyph id in Postscript font. */
    gs_string char_name;
    bool duplicate_char_name;
};
gs_private_st_strings1_ptrs4(st_pdf_char_proc_ownership, pdf_char_proc_ownership_t,
  "pdf_char_proc_ownership_t", pdf_char_proc_ownership_enum_ptrs,
  pdf_char_proc_ownership_reloc_ptrs, char_name, char_proc, char_next, font_next, font);

gs_private_st_ptrs1(st_pdf_bitmap_fonts, pdf_bitmap_fonts_t,
  "pdf_bitmap_fonts_t", pdf_bitmap_fonts_enum_ptrs,
  pdf_bitmap_fonts_reloc_ptrs, open_font);

static inline int64_t
pdf_char_proc_id(const pdf_char_proc_t *pcp)
{
    return pdf_resource_id((const pdf_resource_t *)pcp);
}

/* Assign a code for a char_proc. */
static int
assign_char_code(gx_device_pdf * pdev, gs_text_enum_t *pte)
{
    pdf_bitmap_fonts_t *pbfs = pdev->text->bitmap_fonts;
    pdf_font_resource_t *pdfont = pbfs->open_font; /* Type 3 */
    int i, c = 0, code;
    uint operation = pte->text.operation;

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
        pdfont->u.simple.s.type3.FontBBox.q.x = 0;
        pdfont->u.simple.s.type3.FontBBox.q.y = 0;
        pdfont->mark_glyph = NULL;
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
        pdfont->u.simple.FirstChar = 255;
    }
    if ((operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES)) ||
        (operation & (TEXT_FROM_CHARS | TEXT_FROM_SINGLE_CHAR))) {
        unsigned char p = *pte->text.data.bytes;
        unsigned char index = p / 8, bit = 0x01 << (p % 8);

        if (pdfont->used[index] & bit) {
            for (i = 0;i < 256;i++) {
                index = i / 8;
                bit = 0x01 << (i % 8);
                if (!(pdfont->used[index] & bit)) {
                    c = i;
                    break;
                }
            }
        } else
            c = p;
        pdfont->used[index] |= bit;
        if (c > pdfont->u.simple.LastChar)
            pdfont->u.simple.LastChar = c;

    } else {
        unsigned char index, bit;
        c = ++(pdfont->u.simple.LastChar);
        index = c / 8;
        bit = 0x01 << (c % 8);
        pdfont->used[index] |= bit;
    }
    if (c < pdfont->u.simple.FirstChar)
        pdfont->u.simple.FirstChar = c;

    pdfont->Widths[c] = psdf_round(pdev->char_width.x, 100, 10); /* See
                        pdf_write_Widths about rounding. We need to provide
                        a compatible data for Tj. */
    if (c > pbfs->max_embedded_code)
        pbfs->max_embedded_code = c;

    return c;
}

/* Write the contents of a Type 3 bitmap or vector font resource. */
int
pdf_write_contents_bitmap(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    stream *s = pdev->strm;
    const pdf_char_proc_ownership_t *pcpo;
    int64_t diff_id = 0;
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
    for (pcpo = pdfont->u.simple.s.type3.char_procs; pcpo;
         pcpo = pcpo->char_next
         ) {
        if (pdfont->u.simple.s.type3.bitmap_font)
            pprintld2(s, "/a%ld %ld 0 R\n", (long)pcpo->char_code,
                      pdf_char_proc_id(pcpo->char_proc));
        else if (!pcpo-> duplicate_char_name) {
            pdf_put_name(pdev, pcpo->char_name.data, pcpo->char_name.size);
            pprintld1(s, " %ld 0 R\n", pdf_char_proc_id(pcpo->char_proc));
        }
        pdf_record_usage_by_parent(pdev, pdf_char_proc_id(pcpo->char_proc), pdfont->object->id);
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

int
pdf_charproc_y_offset(pdf_char_proc_t *pcp)
{
    return pcp->y_offset;
}

int
pdf_charproc_x_offset(pdf_char_proc_t *pcp)
{
    return pcp->x_offset;
}

/* Attach a CharProc to a font. */
static int
pdf_attach_charproc(gx_device_pdf * pdev, pdf_font_resource_t *pdfont, pdf_char_proc_t *pcp,
                    gs_glyph glyph, gs_char char_code, const gs_const_string *gnstr)
{
    pdf_char_proc_ownership_t *pcpo;
    bool duplicate_char_name = false;

    for (pcpo = pdfont->u.simple.s.type3.char_procs; pcpo != NULL; pcpo = pcpo->char_next) {
        if (pcpo->glyph == glyph && pcpo->char_code == char_code)
            return 0;
    }
    if (!pdfont->u.simple.s.type3.bitmap_font) {
        for (pcpo = pdfont->u.simple.s.type3.char_procs; pcpo != NULL; pcpo = pcpo->char_next) {
            if (!bytes_compare(pcpo->char_name.data, pcpo->char_name.size, gnstr->data, gnstr->size)) {
                duplicate_char_name = true;
                break;
            }
        }
    }
    pcpo = gs_alloc_struct(pdev->pdf_memory,
            pdf_char_proc_ownership_t, &st_pdf_char_proc_ownership, "pdf_attach_charproc");

    if (pcpo == NULL)
        return_error(gs_error_VMerror);
    pcpo->font = pdfont;
    pcpo->char_next = pdfont->u.simple.s.type3.char_procs;
    pdfont->u.simple.s.type3.char_procs = pcpo;
    pcpo->char_proc = pcp;
    pcpo->font_next = pcp->owner_fonts;
    pcp->owner_fonts = pcpo;
    pcpo->char_code = char_code;
    pcpo->glyph = glyph;
    if (gnstr == NULL) {
        pcpo->char_name.data = 0;
        pcpo->char_name.size = 0;
    } else {
        if (gnstr->size > 0) {
            pcpo->char_name.data = gs_alloc_bytes(pdev->pdf_memory->non_gc_memory, gnstr->size, "storage for charproc name");
            memcpy(pcpo->char_name.data, gnstr->data, gnstr->size);
        }
        pcpo->char_name.size = gnstr->size;
    }
    pcpo->duplicate_char_name = duplicate_char_name;
    return 0;
}

int
pdf_free_charproc_ownership(gx_device_pdf * pdev, pdf_resource_t *pres)
{
    pdf_char_proc_ownership_t *next, *pcpo = (pdf_char_proc_ownership_t *)pres;

    while (pcpo) {
        next = pcpo->char_next;
        if(pcpo->char_name.size != 0 && pcpo->char_name.data) {
            gs_free_object(pdev->pdf_memory->non_gc_memory, pcpo->char_name.data, "free storage for charproc naem");
            /* This causes PCL some trouble, don't know why yet FIXME-MEMORY
            gs_free_string(pdev->pdf_memory, (byte *)pcpo->char_name.data, pcpo->char_name.size, "Free CharProc name");*/
            pcpo->char_name.data = (byte *)0L;
            pcpo->char_name.size = 0;
        }
        gs_free_object(pdev->pdf_memory, pcpo, "Free CharProc");
        pcpo = next;
    }
    return 0;
}

/* Begin a CharProc for a synthesized (bitmap) font. */
int
pdf_begin_char_proc(gx_device_pdf * pdev, int w, int h, int x_width,
                    int y_offset, int x_offset, gs_id id, pdf_char_proc_t ** ppcp,
                    pdf_stream_position_t * ppos)
{
    gs_char char_code = 0;
    pdf_bitmap_fonts_t *const pbfs = pdev->text->bitmap_fonts;
    pdf_font_resource_t *font;
    pdf_resource_t *pres;
    pdf_char_proc_t *pcp;
    int code;
    /* This code added to store PCL bitmap glyphs in type 3 fonts where possible */
    gs_glyph glyph = GS_NO_GLYPH;
    gs_const_string str2, *str = NULL;
    gs_show_enum *show_enum = (gs_show_enum *)pdev->pte;
    pdf_encoding_element_t *pet = 0;
    /* Since this is for text searching, its only useful if the character code
     * lies in an ASCII range, so we only handle some kinds of text layout.
     */
    int allowed_op = (show_enum->text.operation &
        (TEXT_FROM_STRING | TEXT_FROM_BYTES | TEXT_FROM_CHARS | TEXT_FROM_SINGLE_CHAR));

    /* Check to see the current font is a type 3. We can get here if pdfwrite decides
     * it can't handle a font type, and renders to a bitmap instead. If that's the
     * case then we can't add the bitmap to the existing font (its not a type 3 font)
     * and must fall back to holding it in our fallback type 3 font 'collection'.
     */
    /* Because the bitmaps are stored directly in the cache they already have any
     * effects caused by non-identity FontMatrix entries applied. So if the type 3
     * font we created has a non-identity FontMatrix we can't use it and must
     * go back to collecting the bitmap into our fallback font.
     */
    if ((show_enum->current_font->FontType == ft_user_defined ||
        show_enum->current_font->FontType == ft_PDF_user_defined ||
        show_enum->current_font->FontType == ft_PCL_user_defined ||
        show_enum->current_font->FontType == ft_MicroType ||
        show_enum->current_font->FontType == ft_GL2_stick_user_defined ||
        show_enum->current_font->FontType == ft_GL2_531) && allowed_op &&
        show_enum->current_font->FontMatrix.xx == 1 && show_enum->current_font->FontMatrix.xy == 0 &&
        show_enum->current_font->FontMatrix.yx == 0 && show_enum->current_font->FontMatrix.yy == 1) {
        pdf_char_proc_ownership_t *pcpo;

        gs_font_base *base = (gs_font_base *)show_enum->current_font;
        code = pdf_attached_font_resource(pdev, show_enum->current_font, &font, NULL, NULL, NULL, NULL);
        if (code < 0)
            return code;
        if (font == NULL)
            return_error(gs_error_invalidfont);

        /* The text processing will have run past the glyph, so we need to 'back up'
         * by one and get it again in order to get the character code and glyph, and update
         * the pointer correctly.
         */
        show_enum->index--;
        code = gs_default_next_char_glyph((gs_text_enum_t *)show_enum, (gs_char *)&char_code, &glyph);
        if (code < 0)
            return code;

        /* If the returned character code is outside the possible Encoding for
         * a type 3 font, then set pet to NULL, this means we will fall back to
         * the 'collection' font, as pet is checked below.
         */
        if ((int)char_code >= 0 && (int)char_code <= 255) {
            pet = &font->u.simple.Encoding[char_code];
            if (pet) {
                /* Check to see if we *already* have this glyph in this font. If
                 * we do then we can't add it to this font. Setting pet to 0
                 * falls back to the collection method.
                 */
                for (pcpo = font->u.simple.s.type3.char_procs; pcpo != NULL; pcpo = pcpo->char_next) {
                    if (pcpo->glyph == pet->glyph && pcpo->char_code == char_code) {
                        pet = 0x00;
                        break;
                    }
                }
            }
        }
        else
            pet = 0x00;

        /* We need a glyph name for the type 3 font's Encoding, if we haven't got one
         * then we need to give up, something about the font or text is not acceptable
         * (see various comments above).
         */
        if (pet && pet->glyph != GS_NO_GLYPH && !(pet->size == 7 &&
            !strncmp((const char *)pet->data, ".notdef", 7))) {
            if (char_code < font->u.simple.FirstChar)
                font->u.simple.FirstChar = char_code;
            if ((int)char_code > font->u.simple.LastChar)
                font->u.simple.LastChar = char_code;
            base->FontBBox.q.x = max(base->FontBBox.q.x, w);
            base->FontBBox.q.y = max(base->FontBBox.q.y, y_offset + h);
            str2.data = pet->data;
            str2.size = pet->size;
            str = &str2;
            glyph = pet->glyph;
            /* This is to work around a weird Acrobat bug. If the Encoding of a type 3
             * (possibly other types) is simply a standard encoding (eg WinAnsiEncoding)
             * then Acrobat 4 & 8 just ignore the glyphs altogether. This forces us to write
             * all the used glyphs as /Differencess, and that makes it work <sigh>
             */
            pet->is_difference = 1;
            font->Widths[char_code] = psdf_round(pdev->char_width.x, 100, 10); /* See
                        pdf_write_Widths about rounding. We need to provide
                        a compatible data for Tj. */
        } else {
            char_code = assign_char_code(pdev, pdev->pte);
            font = pbfs->open_font; /* Type 3 */
        }
    } else {
        char_code = assign_char_code(pdev, pdev->pte);
        font = pbfs->open_font; /* Type 3 */
    }

    code = pdf_begin_resource(pdev, resourceCharProc, id, &pres);
    if (code < 0)
        return code;
    pcp = (pdf_char_proc_t *) pres;
    code = pdf_attach_charproc(pdev, font, pcp, glyph, char_code, str);
    if (code < 0)
        return code;
    pres->object->written = true;
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
    code = pdf_begin_encrypt(pdev, &pdev->strm, pres->object->id);
    if (code < 0)
        return code;
    pcp->y_offset = y_offset;
    pcp->x_offset = x_offset;
    font->u.simple.s.type3.FontBBox.q.x =
        max(font->u.simple.s.type3.FontBBox.q.x, w);
    font->u.simple.s.type3.FontBBox.q.y =
        max(font->u.simple.s.type3.FontBBox.q.y, y_offset + h);
    font->u.simple.s.type3.max_y_offset =
        max(font->u.simple.s.type3.max_y_offset, h + (h >> 2));
    pcp->real_width.x = w;
    pcp->real_width.y = y_offset + h;
    *ppcp = pcp;
    return 0;
}

/* End a CharProc. */
int
pdf_end_char_proc(gx_device_pdf * pdev, pdf_stream_position_t * ppos)
{
    stream *s;
    gs_offset_t start_pos, end_pos, length;

    if (pdf_end_encrypt(pdev))
        s_close_filters(&pdev->strm, pdev->strm->strm);

    s = pdev->strm;
    start_pos = ppos->start_pos;
    end_pos = stell(s);
    length = end_pos - start_pos;
    if (length > 999999)
        return_error(gs_error_limitcheck);
    sseek(s, start_pos - 15);
    pprintd1(s, "%d", length);
    sseek(s, end_pos);
    if (pdev->PDFA != 0)
        stream_puts(s, "\n");
    stream_puts(s, "endstream\n");
    pdf_end_separate(pdev, resourceCharProc);
    return 0;
}

/* Mark glyph names for garbager. */
void
pdf_mark_glyph_names(const pdf_font_resource_t *pdfont, const gs_memory_t *memory)
{
    if (pdfont->mark_glyph == NULL) {
        /* Synthesised bitmap fonts pass here. */
        return;
    }
    if (pdfont->u.simple.Encoding != NULL) {
         int i;

         for (i = 0; i < 256; i++)
             if (pdfont->u.simple.Encoding[i].glyph != GS_NO_GLYPH)
                pdfont->mark_glyph(memory, pdfont->u.simple.Encoding[i].glyph, pdfont->mark_glyph_data);
     }
    if (pdfont->FontType == ft_user_defined ||
        pdfont->FontType == ft_PDF_user_defined ||
        pdfont->FontType == ft_PCL_user_defined ||
        pdfont->FontType == ft_MicroType ||
        pdfont->FontType == ft_GL2_stick_user_defined ||
        pdfont->FontType == ft_GL2_531) {
        const pdf_char_proc_ownership_t *pcpo = pdfont->u.simple.s.type3.char_procs;

        for (; pcpo != NULL; pcpo = pcpo->font_next)
            pdfont->mark_glyph(memory, pcpo->glyph, pdfont->mark_glyph_data);
    }
}

/* Put out a reference to an image as a character in a synthesized font. */
int
pdf_do_char_image(gx_device_pdf * pdev, const pdf_char_proc_t * pcp,
                  const gs_matrix * pimat)
{
    /* We need to choose a font, which use the charproc.
       In most cases it is the last font, which the charproc is attached to.
       If the charproc is substituted, it causes a font change. */
    const pdf_char_proc_ownership_t * pcpo = pcp->owner_fonts;
    pdf_font_resource_t *pdfont = pcpo->font;
    byte ch = pcpo->char_code;
    pdf_text_state_values_t values;

    values.character_spacing = 0;
    values.pdfont = pdfont;
    values.size = 1;
    values.matrix = *pimat;
    values.render_mode = pdev->pte->pgs->text_rendering_mode;
    values.word_spacing = 0;
    pdf_set_text_state_values(pdev, &values);
    pdf_bitmap_char_update_bbox(pdev, pcp->x_offset, pcp->y_offset, pcp->real_width.x, pcp->real_width.y);
    pdf_append_chars(pdev, &ch, 1, pdfont->Widths[ch] * pimat->xx, 0.0, false);
    return 0;
}

/*
 * Write the Encoding for bitmap fonts, if needed.
 */
int
pdf_write_bitmap_fonts_Encoding(gx_device_pdf *pdev)
{
    pdf_bitmap_fonts_t *pbfs;

    if (pdev->text == NULL || pdev->text->bitmap_fonts == NULL)
        return 0;

    pbfs = pdev->text->bitmap_fonts;

    if (pbfs->bitmap_encoding_id) {
        stream *s;
        int i;

        pdf_open_separate(pdev, pbfs->bitmap_encoding_id, resourceEncoding);
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
        pdf_end_separate(pdev, resourceEncoding);
        pbfs->bitmap_encoding_id = 0;
    }
    return 0;
}

/*
 * Start charproc accumulation for a Type 3 font.
 */
int
pdf_start_charproc_accum(gx_device_pdf *pdev)
{
    pdf_char_proc_t *pcp;
    pdf_resource_t *pres;
    int id = gs_next_ids(pdev->memory, 1);
    int code = pdf_enter_substream(pdev, resourceCharProc, id,
                                   &pres, false, pdev->CompressFonts);

    if (code < 0)
       return code;
    pres->rid = id;
    pcp = (pdf_char_proc_t *)pres;
    pcp->owner_fonts = NULL;
    return 0;
}

/*
 * Install charproc accumulator for a Type 3 font.
 */
int
pdf_set_charproc_attrs(gx_device_pdf *pdev, gs_font *font, double *pw, int narg,
                gs_text_cache_control_t control, gs_char ch, bool scale_100)
{
    pdf_font_resource_t *pdfont;
    pdf_resource_t *pres = pdev->accumulating_substream_resource;
    pdf_char_proc_t *pcp;
    int code;

    code = pdf_attached_font_resource(pdev, font, &pdfont, NULL, NULL, NULL, NULL);
    if (code < 0)
        return code;
    pcp = (pdf_char_proc_t *)pres;
    pcp->owner_fonts = NULL;
    pcp->real_width.x = pw[font->WMode && narg > 6 ? 6 : 0];
    pcp->real_width.y = pw[font->WMode && narg > 6 ? 7 : 1];
    pcp->v.x = (narg > 8 ? pw[8] : 0);
    pcp->v.y = (narg > 8 ? pw[9] : 0);
    if (control == TEXT_SET_CHAR_WIDTH) {
        /* PLRM 5.7.1 "BuildGlyph" reads : "Normally, it is unnecessary and
        undesirable to initialize the current color parameter, because show
        is defined to paint glyphs with the current color."
        However comparefiles/Bug687044.ps doesn't follow that. */
        pdev->skip_colors = false;
        pprintg1(pdev->strm, "%g 0 d0\n", (float)pw[0]);
        /* The colour change described above can't affect PCL fonts and we need
         * all glyphs to be noted as cached in order for the bitmap font cache
         * probing to work properly.
         */
        if (font->FontType == ft_PCL_user_defined || font->FontType == ft_GL2_stick_user_defined
            || font->FontType == ft_GL2_531 || font->FontType == ft_MicroType)
            pdfont->u.simple.s.type3.cached[ch >> 3] |= 0x80 >> (ch & 7);
    } else {
        double d;
        pdev->skip_colors = true;
        if (pw[4] < pw[2]) {
            d = pw[2];
            pw[2] = pw[4];
            pw[4] = d;
        }
        if (pw[5] < pw[3]) {
            d = pw[5];
            pw[5] = pw[3];
            pw[3] = d;
        }
        pprintg6(pdev->strm, "%g %g %g %g %g %g d1\n",
            (float)pw[0], (float)0.0, (float)pw[2],
            (float)pw[3], (float)pw[4], (float)pw[5]);
        pdfont->u.simple.s.type3.cached[ch >> 3] |= 0x80 >> (ch & 7);
    }
    /* See comments in pdf_text_process regarding type 3 CharProc accumulation
     * Initially this matrix was emitted there, at the start of the accumulator
     * but if we do that then GS incorrectly applied the matrix to the 'd1'
     * operator. We write the scale matrix here because this is *after* the
     * 'd1' has been emitted above, and so does not affect it.
     */
    if (scale_100) {
    code = stream_puts(pdev->strm, "0.01 0 0 0.01 0 0 cm\n");
    if (code < 0)
        return code;
    }
    return 0;
}

/*
 * Open a stream object in the temporary file.
 */

int
pdf_open_aside(gx_device_pdf *pdev, pdf_resource_type_t rtype,
        gs_id id, pdf_resource_t **ppres, bool reserve_object_id, int options)
{
    int code;
    pdf_resource_t *pres;
    stream *s, *save_strm = pdev->strm;
    pdf_data_writer_t writer;
    static const pdf_filter_names_t fnames = {
        PDF_FILTER_NAMES
    };

    pdev->streams.save_strm = pdev->strm;

    if (rtype >= NUM_RESOURCE_TYPES)
        rtype = resourceOther;
    code = pdf_alloc_aside(pdev, PDF_RESOURCE_CHAIN(pdev, rtype, id),
                pdf_resource_type_structs[rtype], &pres, reserve_object_id ? 0 : -1);
    if (code < 0)
        return code;
    cos_become(pres->object, cos_type_stream);
    s = cos_write_stream_alloc((cos_stream_t *)pres->object, pdev, "pdf_enter_substream");
    if (s == 0)
        return_error(gs_error_VMerror);
    pdev->strm = s;
    code = pdf_append_data_stream_filters(pdev, &writer,
                             options | DATA_STREAM_NOLENGTH, pres->object->id);
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
    *ppres = pres;
    return 0;
}

/*
 * Close a stream object in the temporary file.
 */
int
pdf_close_aside(gx_device_pdf *pdev)
{
    /* We should call pdf_end_data here, but we don't want to put pdf_data_writer_t
       into pdf_substream_save stack to simplify garbager descriptors.
       Use a lower level functions instead that. */
    stream *s = pdev->strm;
    cos_stream_t *pcs = cos_stream_from_pipeline(s);
    int status = s_close_filters(&s, NULL);

    pdev->strm = pdev->streams.save_strm;
    if (status < 0)
         return(gs_note_error(gs_error_ioerror));

    if (!pcs)
        return gs_note_error(gs_error_ioerror);

    pcs->is_open = false;
    return 0;
}

/*
 * Enter the substream accumulation mode.
 */
int
pdf_enter_substream(gx_device_pdf *pdev, pdf_resource_type_t rtype,
        gs_id id, pdf_resource_t **ppres, bool reserve_object_id, bool compress)
{
    int sbstack_ptr = pdev->sbstack_depth;
    pdf_resource_t *pres;
    stream *save_strm = pdev->strm;
    int code;

    if (pdev->sbstack_depth >= pdev->sbstack_size)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (pdev->sbstack[sbstack_ptr].text_state == 0) {
        pdev->sbstack[sbstack_ptr].text_state = pdf_text_state_alloc(pdev->pdf_memory);
        if (pdev->sbstack[sbstack_ptr].text_state == 0)
            return_error(gs_error_VMerror);
    }
    code = pdf_open_aside(pdev, rtype, id, &pres, reserve_object_id,
                    (compress ? DATA_STREAM_COMPRESS : 0));
    if (code < 0)
        return code;
    code = pdf_save_viewer_state(pdev, NULL);
    if (code < 0) {
        pdev->strm = save_strm;
        return code;
    }
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
    pdev->sbstack[sbstack_ptr].font3 = pdev->font3;
    pdev->sbstack[sbstack_ptr].accumulating_substream_resource = pdev->accumulating_substream_resource;
    pdev->sbstack[sbstack_ptr].charproc_just_accumulated = pdev->charproc_just_accumulated;
    pdev->sbstack[sbstack_ptr].accumulating_a_global_object = pdev->accumulating_a_global_object;
    pdev->sbstack[sbstack_ptr].pres_soft_mask_dict = pdev->pres_soft_mask_dict;
    pdev->sbstack[sbstack_ptr].objname = pdev->objname;
    pdev->sbstack[sbstack_ptr].last_charpath_op = pdev->last_charpath_op;
    pdev->skip_colors = false;
    pdev->charproc_just_accumulated = false;
    pdev->pres_soft_mask_dict = NULL;
    pdev->objname.data = NULL;
    pdev->objname.size = 0;
    /* Do not reset pdev->accumulating_a_global_object - it inherits. */
    pdev->sbstack_depth++;
    pdev->procsets = 0;
    pdev->font3 = 0;
    pdev->context = PDF_IN_STREAM;
    pdev->accumulating_substream_resource = pres;
    pdev->last_charpath_op = 0;
    /* Do not alter type3charpath, inherit the current value. We need to know if */
    /* we are inside a charpath operation, and only reset this when the charpath */
    /* is complete */
    if (rtype != resourceXObject)
        pdf_reset_graphics(pdev);
    else {
        if (pdev->vg_initial_set)
            pdev->state.blend_mode = pdev->vg_initial.blend_mode;
    }
    *ppres = pres;
    return 0;
}

/*
 * Exit the substream accumulation mode.
 */
int
pdf_exit_substream(gx_device_pdf *pdev)
{
    int code, code1;
    int sbstack_ptr;

    if (pdev->sbstack_depth <= 0)
        return_error(gs_error_unregistered); /* Must not happen. */
    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    sbstack_ptr = pdev->sbstack_depth - 1;
    while (pdev->vgstack_depth > pdev->vgstack_bottom) {
        code1 = pdf_restore_viewer_state(pdev, pdev->strm);
        if (code >= 0)
            code = code1;
    }
    if (pdev->clip_path != 0)
        gx_path_free(pdev->clip_path, "pdf_end_charproc_accum");
    code1 = pdf_close_aside(pdev);
    if (code1 < 0 && code >= 0)
        code = code1;
    pdev->context = pdev->sbstack[sbstack_ptr].context;
    pdf_text_state_copy(pdev->text->text_state, pdev->sbstack[sbstack_ptr].text_state);
    gs_free_object(pdev->pdf_memory, pdev->sbstack[sbstack_ptr].text_state, "free text state for stream");
    pdev->sbstack[sbstack_ptr].text_state = 0;
    pdev->clip_path = pdev->sbstack[sbstack_ptr].clip_path;
    pdev->sbstack[sbstack_ptr].clip_path = 0;
    pdev->clip_path_id = pdev->sbstack[sbstack_ptr].clip_path_id;
    pdev->vgstack_bottom = pdev->sbstack[sbstack_ptr].vgstack_bottom;
    pdev->strm = pdev->sbstack[sbstack_ptr].strm;
    pdev->sbstack[sbstack_ptr].strm = 0;
    pdev->procsets = pdev->sbstack[sbstack_ptr].procsets;
    pdev->substream_Resources = pdev->sbstack[sbstack_ptr].substream_Resources;
    pdev->sbstack[sbstack_ptr].substream_Resources = 0;
    pdev->skip_colors = pdev->sbstack[sbstack_ptr].skip_colors;
    pdev->font3 = pdev->sbstack[sbstack_ptr].font3;
    pdev->sbstack[sbstack_ptr].font3 = 0;
    pdev->accumulating_substream_resource = pdev->sbstack[sbstack_ptr].accumulating_substream_resource;
    pdev->sbstack[sbstack_ptr].accumulating_substream_resource = 0;
    pdev->charproc_just_accumulated = pdev->sbstack[sbstack_ptr].charproc_just_accumulated;
    pdev->accumulating_a_global_object = pdev->sbstack[sbstack_ptr].accumulating_a_global_object;
    pdev->pres_soft_mask_dict = pdev->sbstack[sbstack_ptr].pres_soft_mask_dict;
    pdev->objname = pdev->sbstack[sbstack_ptr].objname;
    pdev->last_charpath_op = pdev->sbstack[sbstack_ptr].last_charpath_op;
    pdev->sbstack_depth = sbstack_ptr;
    code1 = pdf_restore_viewer_state(pdev, NULL);
    if (code1 < 0 && code >= 0)
        code = code1;
    return code;
}

static bool
pdf_is_same_charproc_attrs1(gx_device_pdf *pdev, pdf_char_proc_t *pcp0, pdf_char_proc_t *pcp1)
{
    if (pcp0->real_width.x != pcp1->real_width.x)
        return false;
    if (pcp0->real_width.y != pcp1->real_width.y)
        return false;
    if (pcp0->v.x != pcp1->v.x)
        return false;
    if (pcp0->v.y != pcp1->v.y)
        return false;
    return true;
}

typedef struct charproc_compatibility_data_s {
    const pdf_char_glyph_pairs_t *cgp;
    pdf_font_resource_t *pdfont;
    gs_char char_code;
    gs_glyph glyph;
    gs_font *font;
} charproc_compatibility_data_t;

static bool
is_char_code_used(pdf_font_resource_t *pdfont, gs_char char_code)
{
    pdf_char_proc_ownership_t *pcpo;

    for (pcpo = pdfont->u.simple.s.type3.char_procs; pcpo != NULL; pcpo = pcpo->char_next) {
        if (pcpo->char_code == char_code) {
            return true;
        }
    }
    return false;
}

static int
pdf_is_charproc_compatible(gx_device_pdf * pdev, pdf_resource_t *pres0, pdf_resource_t *pres1)
{
    charproc_compatibility_data_t *data = (charproc_compatibility_data_t *)pdev->find_resource_param;
    pdf_char_proc_t *pcp0 = (pdf_char_proc_t *)pres0;
    pdf_char_proc_t *pcp1 = (pdf_char_proc_t *)pres1;
    pdf_font_resource_t *pdfont = data->pdfont;
    pdf_char_proc_ownership_t *pcpo;
    pdf_font_cache_elem_t **e;
    bool can_add_to_current_font = false, computed_can_add_to_current_font = false;

    /* Does it have same attributes ? */
    if (!pdf_is_same_charproc_attrs1(pdev, pcp0, pcp1))
        return 0;
    /* Is it from same font ? */
    for (pcpo = pcp1->owner_fonts; pcpo != NULL; pcpo = pcpo->char_next) {
        if (pdfont == pcpo->font) {
            /* Check for encoding conflict. */
            if (pcpo->char_code == data->char_code && pcpo->glyph == data->glyph)
                return 1; /* Same char code. */
            if (!computed_can_add_to_current_font) {
                can_add_to_current_font = !is_char_code_used(pdfont, data->char_code);
                computed_can_add_to_current_font = true;
            }
            if (can_add_to_current_font)
                return 1; /* No conflict. */
        }
    }
    /* Look for another font with same encoding,
       because we want to reduce the number of new fonts.
       We also restrict with ones attached to same PS font,
       otherwise it creates too mixed fonts and disturbs word breaks.
     */
    e = pdf_locate_font_cache_elem(pdev, data->font);
    if (e != NULL) {
        for (pcpo = pcp1->owner_fonts; pcpo != NULL; pcpo = pcpo->char_next) {
            if (pcpo->char_code != data->char_code || pcpo->glyph != data->glyph)
                continue; /* Need same Encoding to generate a proper ToUnicode. */
            if (pdfont->u.simple.s.type3.bitmap_font != pcpo->font->u.simple.s.type3.bitmap_font)
                continue;
            if (gs_matrix_compare(&pdfont->u.simple.s.type3.FontMatrix, &pcpo->font->u.simple.s.type3.FontMatrix))
                continue;
            if (data->cgp != NULL) {
                if (!pdf_check_encoding_compatibility(pcpo->font, data->cgp->s, data->cgp->num_all_chars))
                    continue;
            }
            if ((*e)->pdfont != pcpo->font)
                continue;
            data->pdfont = pcpo->font; /* Switch to the other font. */
            return 1;
        }
    }
    /* Check whether it can be added into the current font. */
    if (!computed_can_add_to_current_font)
        can_add_to_current_font = !is_char_code_used(pdfont, data->char_code);
    if (!can_add_to_current_font) {
        /* Can't substitute due to encoding conflict. */
        return 0;
    }
    /* The current font will share it with another font. */
    return 1;
}

static int
pdf_find_same_charproc_aux(gx_device_pdf *pdev,
            pdf_font_resource_t **ppdfont, pdf_char_proc_t **ppcp)
{
    pdf_char_proc_ownership_t *pcpo;
    int code;

    /* fixme: this passes parameters to pdf_is_charproc_compatible
       through special gx_device_pdf field pdev->find_resource_param
       due to prototype limitation of pdf_find_same_resource.
       It would be better to change the client data argument type in there to void. */
    for (pcpo = (*ppdfont)->u.simple.s.type3.char_procs; pcpo != NULL; pcpo = pcpo->char_next) {
        pdf_char_proc_t *pcp = pcpo->char_proc;

        if (*ppcp != pcp && pdf_is_same_charproc_attrs1(pdev, *ppcp, pcp)) {
            cos_object_t *pco0 = pcp->object;
            cos_object_t *pco1 = (*ppcp)->object;

            code = pco0->cos_procs->equal(pco0, pco1, pdev);
            if (code < 0) {
                return code;
            }
            if (code) {
                *ppcp = pcp;
                return 1;
            }
        }
    }
    return pdf_find_same_resource(pdev, resourceCharProc, (pdf_resource_t **)ppcp, pdf_is_charproc_compatible);
}
static int
pdf_find_same_charproc(gx_device_pdf *pdev,
            pdf_font_resource_t **ppdfont, const pdf_char_glyph_pairs_t *cgp,
            pdf_char_proc_t **ppcp, gs_glyph glyph, gs_char char_code,
            gs_font *font)
{
    charproc_compatibility_data_t data;
    int code;

    data.cgp = cgp;
    data.pdfont = *ppdfont;
    data.char_code = char_code;
    data.glyph = glyph;
    data.font = font;
    pdev->find_resource_param = &data;
    code = pdf_find_same_charproc_aux(pdev, ppdfont, ppcp);
    pdev->find_resource_param = NULL;
    *ppdfont = data.pdfont;
    return code;
}

static bool
pdf_is_charproc_defined(gx_device_pdf *pdev, pdf_font_resource_t *pdfont, gs_char ch)
{
    pdf_char_proc_ownership_t *pcpo;

    for (pcpo = pdfont->u.simple.s.type3.char_procs; pcpo != NULL; pcpo = pcpo->char_next) {
        if (pcpo->char_code == ch)
            return true;
    }
    return false;
}

static int
complete_adding_char(gx_device_pdf *pdev, gs_font *font,
                     gs_glyph glyph, gs_char ch, pdf_char_proc_t *pcp,
                     const gs_const_string *gnstr)
{
    pdf_font_resource_t *pdfont;
    double *real_widths;
    byte *glyph_usage;
    int char_cache_size, width_cache_size;
    pdf_encoding_element_t *pet;
    int code;

    code = pdf_attached_font_resource(pdev, font, &pdfont,
                &glyph_usage, &real_widths, &char_cache_size, &width_cache_size);
    if (code < 0)
        return code;
    code = pdf_attach_charproc(pdev, pdfont, pcp, glyph, ch, gnstr);
    if (code < 0)
        return code;
    if (ch >= char_cache_size || ch >= width_cache_size)
        return_error(gs_error_unregistered); /* Must not happen. */
    pet = &pdfont->u.simple.Encoding[ch];
    pdfont->Widths[ch] = pcp->real_width.x;
    real_widths[ch * 2    ] = pcp->real_width.x;
    real_widths[ch * 2 + 1] = pcp->real_width.y;
    glyph_usage[ch / 8] |= 0x80 >> (ch & 7);
    pdfont->used[ch >> 3] |= 0x80 >> (ch & 7);
    if (pdfont->u.simple.v != NULL && font->WMode) {
        pdfont->u.simple.v[ch].x = pcp->v.x;
        pdfont->u.simple.v[ch].y = pcp->v.x;
    }
    pet->glyph = glyph;
    pet->is_difference = true;
    if (pdfont->u.simple.LastChar < (int)ch)
        pdfont->u.simple.LastChar = (int)ch;
    if (pdfont->u.simple.FirstChar > (int)ch)
        pdfont->u.simple.FirstChar = (int)ch;
    return pdf_copy_string_to_encoding(pdev, (gs_const_string *)gnstr, pet);
}

static int
pdf_char_widths_from_charprocs(gx_device_pdf *pdev, gs_font *font)
{
    pdf_font_resource_t *pdfont;
    double *real_widths;
    byte *glyph_usage;
    int char_cache_size, width_cache_size;
    pdf_char_proc_ownership_t *pcpo;
    int code;

    code = pdf_attached_font_resource(pdev, font, &pdfont,
                &glyph_usage, &real_widths, &char_cache_size, &width_cache_size);
    if (code < 0)
        return code;
    for (pcpo = pdfont->u.simple.s.type3.char_procs; pcpo != NULL; pcpo = pcpo->char_next) {
        pdf_char_proc_t *pcp = pcpo->char_proc;
        gs_char ch = pcpo->char_code;

        real_widths[ch * 2    ] = pcp->real_width.x;
        real_widths[ch * 2 + 1] = pcp->real_width.y;
        glyph_usage[ch / 8] |= 0x80 >> (ch & 7);
    }
    return 0;
}

/*
 * Complete charproc accumulation for a Type 3 font.
 */
int
pdf_end_charproc_accum(gx_device_pdf *pdev, gs_font *font, const pdf_char_glyph_pairs_t *cgp,
                       gs_glyph glyph, gs_char output_char_code, const gs_const_string *gnstr)
{
    int code;
    pdf_resource_t *pres = (pdf_resource_t *)pdev->accumulating_substream_resource;
    /* We could use pdfont->u.simple.s.type3.char_procs insted the thing above
       unless the font is defined recursively.
       But we don't want such assumption. */
    pdf_char_proc_t *pcp = (pdf_char_proc_t *)pres;
    pdf_font_resource_t *pdfont;
    gs_char ch = output_char_code;
    bool checking_glyph_variation = false;

    if (ch == GS_NO_CHAR)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (ch >= 256)
        return_error(gs_error_unregistered); /* Must not happen. */
    code = pdf_attached_font_resource(pdev, font, &pdfont, NULL, NULL, NULL, NULL);
    if (code < 0)
        return code;
    if (pdfont != (pdf_font_resource_t *)pdev->font3)
        return_error(gs_error_unregistered); /* Must not happen. */
    code = pdf_exit_substream(pdev);
    if (code < 0)
        return code;
    if (!(pdfont->used[ch >> 3] & (0x80 >> (ch & 7))) ||
        !(pdfont->u.simple.s.type3.cached[ch >> 3] & (0x80 >> (ch & 7)))) {
        /* First appearence or not cached - check for duplicates. */
        pdf_font_resource_t *pdfont1 = pdfont;

        checking_glyph_variation = true;
        /* CAUTION : a possible font change. */
        code = pdf_find_same_charproc(pdev, &pdfont, cgp, &pcp, glyph, ch, font);
        if (code < 0)
            return code;
        if (code != 0) {
            code = pdf_cancel_resource(pdev, pres, resourceCharProc);
            if (code < 0)
                return code;
            pdf_forget_resource(pdev, pres, resourceCharProc);
            if (pdfont1 != pdfont) {
                code = pdf_attach_font_resource(pdev, font, pdfont);
                if (code < 0)
                    return code;
                code = pdf_char_widths_from_charprocs(pdev, font);
                if (code < 0)
                    return code;
            }
            pdev->charproc_just_accumulated = true;
            return complete_adding_char(pdev, font, glyph, ch, pcp, gnstr);
        }
        if (pdf_is_charproc_defined(pdev, pdfont, ch)) {
            /* Encoding conflict after a font change. */
            gs_font *base_font = font, *below;

            while ((below = base_font->base) != base_font &&
                    base_font->procs.same_font(base_font, below, FONT_SAME_OUTLINES))
                base_font = below;
            code = pdf_make_font3_resource(pdev, base_font, &pdfont);
            if (code < 0)
                return code;
            code = pdf_attach_font_resource(pdev, font, pdfont);
            if (code < 0)
                return code;
        }
    }
    pdf_reserve_object_id(pdev, pres, 0);
    if (checking_glyph_variation)
        pdev->charproc_just_accumulated = true;
    return complete_adding_char(pdev, font, glyph, ch, pcp, gnstr);
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

        if (pdev->ForOPDFRead && !pres->global && pdev->accumulating_a_global_object) {
            pres->global = true;
            code = cos_dict_put_c_key_bool((cos_dict_t *)pres->object, "/.Global", true);
            if (code < 0)
                return code;
        }
        gs_snprintf(buf, sizeof(buf), "%ld 0 R\n", pres->object->id);
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
