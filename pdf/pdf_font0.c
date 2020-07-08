/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

/* code for type 0 (CID) font handling */
#include "gxfont.h"
#include "gxfont0.h"

#include "pdf_int.h"
#include "pdf_font.h"
#include "pdf_font0.h"
#include "pdf_font_types.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_file.h"
#include "pdf_cmap.h"
#include "pdf_deref.h"

static font_type pdfi_fonttype_picker(byte *buf, int64_t buflen)
{
#define MAKEMAGIC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

    if (buflen >= 4) {
        if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC(0, 1, 0, 0)) {
            return ft_CID_TrueType;
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('O', 'T', 'T', 'O')) {
            return ft_CID_encrypted; /* OTTO will end up as CFF */
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], 0) == MAKEMAGIC('%', '!', 'P', 0)) {
            return ft_CID_encrypted; /* pfa */
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC(1, 0, 4, 1)) {
            return ft_CID_encrypted; /* 1C/CFF */
        }
        else if (MAKEMAGIC(buf[0], buf[1], 0, 0) == MAKEMAGIC(128, 1, 0, 0)) {
            return ft_CID_encrypted; /* pfb */
        }
    }
    return 0;

#undef MAKEMAGIC
}

static int
pdfi_font0_glyph_name(gs_font *font, gs_glyph index, gs_const_string *pstr)
{
    return_error(gs_error_rangecheck);
}

static int
pdfi_font0_map_glyph_to_unicode(gs_font *font, gs_glyph glyph, int ch, ushort *u, unsigned int length)
{
    if (glyph > GS_MIN_CID_GLYPH)
        glyph -= GS_MIN_CID_GLYPH;

    return glyph;
}

int pdfi_read_type0_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, gs_font **pfont)
{
    int code, nlen;
    pdf_obj *cmap = NULL;
    pdf_cmap *pcmap = NULL;
    pdf_array *arr = NULL;
    pdf_dict *decfont = NULL; /* there can only be one */
    pdf_name *n = NULL;
    pdf_obj *basefont = NULL;
    pdf_obj *tounicode = NULL;
    int cidftype;
    const char *ffstrings[] = {"FontFile", "FontFile2", "FontFile3"};
    int ff;
    pdf_dict *fontdesc = NULL;
    pdf_stream *ffile = NULL;
    byte *buf;
    int64_t buflen;
    pdf_font *descpfont = NULL;
    pdf_font_type0 *pdft0 = NULL;
    gs_font_type0 *pfont0 = NULL;

    code = pdfi_dict_get(ctx, font_dict, "Encoding", &cmap);
    if (code < 0) goto error;

    code = pdfi_read_cmap(ctx, cmap, &pcmap);
    pdfi_countdown(cmap);
    if (code < 0) goto error;

    code = pdfi_dict_get(ctx, font_dict, "DescendantFonts", (pdf_obj **)&arr);
    if (code < 0) goto error;

    if (arr->type != PDF_ARRAY || arr->size != 1) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    code = pdfi_array_get(ctx, arr, 0, (pdf_obj **)&decfont);
    pdfi_countdown(arr);
    arr = NULL;
    if (code < 0) goto error;
    if (decfont->type != PDF_DICT) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    code = pdfi_dict_get(ctx, (pdf_dict *)decfont, "Type", (pdf_obj **)&n);
    if (code < 0) goto error;
    if (n->type != PDF_NAME || n->length != 4 || memcmp(n->data, "Font", 4) != 0) {
        pdfi_countdown(n);
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    pdfi_countdown(n);
    code = pdfi_dict_get(ctx, (pdf_dict *)decfont, "Subtype", (pdf_obj **)&n);
    if (code < 0)
        goto error;

    if (n->type != PDF_NAME || n->length != 12 || memcmp(n->data, "CIDFontType", 11) != 0) {
        pdfi_countdown(n);
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    /* cidftype is ignored for now, but we may need to know it when
       subsitutions are allowed
     */
    cidftype = n->data[11] - 48;
    pdfi_countdown(n);

    code = pdfi_dict_get(ctx, font_dict, "BaseFont", (pdf_obj **)&basefont);
    if (code < 0) {
        basefont = NULL;
    }

    code = pdfi_dict_get(ctx, font_dict, "ToUnicode", (pdf_obj **)&tounicode);
    if (code < 0 || (tounicode != NULL && tounicode->type != PDF_DICT)) {
        pdfi_countdown(tounicode);
        tounicode = NULL;
    }

    code = pdfi_dict_get(ctx, decfont, "FontDescriptor", (pdf_obj **)&fontdesc);
    if (code < 0)
        goto error;

    if (fontdesc->type != PDF_DICT) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    for (ff = 0;ff < (sizeof(ffstrings) / sizeof(ffstrings[0])); ff++) {
        code = pdfi_dict_get(ctx, fontdesc, ffstrings[ff], (pdf_obj **)&ffile);
        if (code >= 0) break;
    }
    if (code < 0)
        goto error;

    if (ffile->type != PDF_STREAM) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    pdfi_countdown(fontdesc);
    fontdesc = NULL;

    /* We are supposed to be able to discern the underlying font type
     * from FontFile, FontFile2, or FontFile3 + Subtype of the stream.
     * Typical PDF, none of these prove trustworthy, so we have to
     * create the memory buffer here, so we can get a font type from
     * the first few bytes of the font data.
     * Owndership of the buffer is passed to the font reading function.
     */
    code = pdfi_stream_to_buffer(ctx, ffile, &buf, &buflen);
    pdfi_countdown(ffile);
    ffile = NULL;
    if (code < 0)
        goto error;

    switch (pdfi_fonttype_picker(buf, buflen)) {
        case ft_CID_TrueType:
          code = pdfi_read_cidtype2_font(ctx, decfont, buf, buflen, &descpfont);
          break;
        case ft_CID_encrypted:
        default:
          code = pdfi_read_cidtype0_font(ctx, decfont, buf, buflen, &descpfont);
          break;
    }
    /* reference is now owned by the descendent font created above */
    pdfi_countdown(decfont);
    decfont = NULL;
    if (code < 0) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    /* If we're got this far, we have a CMap and a descendant font, let's make the Type 0 */
    pdft0 = (pdf_font_type0 *)gs_alloc_bytes(ctx->memory, sizeof(pdf_font_type0), "pdfi (type0 pdf_font)");
    if (pdft0 == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    code = pdfi_array_alloc(ctx, 1, &arr);
    if (code < 0) {
        gs_free_object(ctx->memory, pdft0, "pdfi_read_type0_font(pdft0)");
        goto error;
    }
    arr->refcnt = 1;
    code = pdfi_array_put(ctx, arr, 0, (pdf_obj *)descpfont);
    if (code < 0) {
        gs_free_object(ctx->memory, pdft0, "pdfi_read_type0_font(pdft0)");
        goto error;
    }

    pdft0->type = PDF_FONT;
    pdft0->pdfi_font_type = e_pdf_font_type0;
    pdft0->ctx = ctx;
#if REFCNT_DEBUG
    pdft0->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %"PRIi64"\n", pdft0->type, pdft0->UID);
#endif
    pdft0->refcnt = 1;
    pdft0->object_num = font_dict->object_num;
    pdft0->generation_num = font_dict->generation_num;
    pdft0->indirect_num = font_dict->indirect_num;
    pdft0->indirect_gen = font_dict->indirect_gen;
    pdft0->Encoding = (pdf_obj *)pcmap;
    pdft0->ToUnicode = tounicode;
    pdft0->DescendantFonts = arr;
    pdft0->PDF_font = font_dict;
    pdfi_countup(font_dict);
    pdft0->FontDescriptor = NULL;
    pdft0->BaseFont = basefont;

    /* Ownership transferred to pdft0, if we jump to error
     * these will now be freed by counting down pdft0.
     */
    tounicode = NULL;
    arr = NULL;
    basefont = NULL;

    pdft0->pfont = NULL; /* In case we error out */

    pfont0 = (gs_font_type0 *)gs_alloc_struct(ctx->memory, gs_font, &st_gs_font_type0, "pdfi gs type 0 font");
    if (pfont0 == NULL) {
        gs_free_object(ctx->memory, pdft0, "pdfi_read_type0_font(pdft0)");
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    gs_make_identity(&pfont0->orig_FontMatrix);
    gs_make_identity(&pfont0->FontMatrix);
    pfont0->next = pfont0->prev = 0;
    pfont0->memory = ctx->memory;
    pfont0->dir = ctx->font_dir;
    pfont0->is_resource = false;
    gs_notify_init(&pfont0->notify_list, ctx->memory);
    pfont0->id = gs_next_ids(ctx->memory, 1);
    pfont0->base = (gs_font *) pfont0;
    pfont0->client_data = pdft0;
    pfont0->WMode = pcmap->wmode;
    pfont0->FontType = ft_composite;
    pfont0->PaintType = 0;
    pfont0->StrokeWidth = 0;
    pfont0->is_cached = 0;
    if (pdft0->BaseFont != NULL) {
        pdf_name *nobj = (pdf_name *)pdft0->BaseFont;
        nlen = nobj->length > gs_font_name_max ? gs_font_name_max : nobj->length;

        memcpy(pfont0->key_name.chars, nobj->data, nlen);
        pfont0->key_name.chars[nlen] = 0;
        pfont0->key_name.size = nlen;
        memcpy(pfont0->font_name.chars, nobj->data, nlen);
        pfont0->font_name.chars[nlen] = 0;
        pfont0->font_name.size = nlen;
    }
    else {
        nlen = descpfont->pfont->key_name.size > gs_font_name_max ? gs_font_name_max : descpfont->pfont->key_name.size;

        memcpy(pfont0->key_name.chars, descpfont->pfont->key_name.chars, nlen);
        pfont0->key_name.chars[nlen] = 0;
        pfont0->key_name.size = nlen;
        memcpy(pfont0->font_name.chars, descpfont->pfont->font_name.chars, nlen);
        pfont0->font_name.chars[nlen] = 0;
        pfont0->font_name.size = nlen;
    }
    if (pcmap->name.size > 0) {
        if (pfont0->key_name.size + pcmap->name.size + 1 < gs_font_name_max) {
            memcpy(pfont0->key_name.chars + pfont0->key_name.size, "-", 1);
            memcpy(pfont0->key_name.chars + pfont0->key_name.size + 1, pcmap->name.data, pcmap->name.size);
            pfont0->key_name.size += pcmap->name.size + 1;
            pfont0->key_name.chars[pfont0->key_name.size] = 0;
            memcpy(pfont0->font_name.chars + pfont0->font_name.size, "-", 1);
            memcpy(pfont0->font_name.chars + pfont0->font_name.size + 1, pcmap->name.data, pcmap->name.size);
            pfont0->font_name.size += pcmap->name.size + 1;
            pfont0->font_name.chars[pfont0->key_name.size] = 0;
        }
    }
    pfont0->procs.define_font = gs_no_define_font;
    pfont0->procs.make_font = gs_no_make_font;
    pfont0->procs.font_info = gs_default_font_info;
    pfont0->procs.same_font = gs_default_same_font;
    pfont0->procs.encode_char = pdfi_encode_char;
    pfont0->procs.decode_glyph = pdfi_font0_map_glyph_to_unicode;
    pfont0->procs.enumerate_glyph = gs_no_enumerate_glyph;
    pfont0->procs.glyph_info = gs_default_glyph_info;
    pfont0->procs.glyph_outline = gs_no_glyph_outline;
    pfont0->procs.glyph_name = pdfi_font0_glyph_name;
    pfont0->procs.init_fstack = gs_type0_init_fstack;
    pfont0->procs.next_char_glyph = gs_type0_next_char_glyph;
    pfont0->procs.build_char = gs_no_build_char;

    pfont0->data.FMapType = fmap_CMap;
    pfont0->data.EscChar = 0xff;
    pfont0->data.ShiftIn = 0x0f;
    pfont0->data.SubsVector.data = NULL;
    pfont0->data.SubsVector.size = 0;
    pfont0->data.subs_size = pfont0->data.subs_width = 0;

    pfont0->data.Encoding = (uint *)gs_alloc_bytes(ctx->memory, sizeof(uint), "pdfi_read_type0_font Encoding");
    if (pfont0->data.Encoding == NULL) {
        gs_free_object(ctx->memory, pfont0, "pdfi_read_type0_font(pfont0)");
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    *pfont0->data.Encoding = 0;

    pfont0->data.encoding_size = 1;
    pfont0->data.FDepVector = (gs_font **)gs_alloc_bytes(ctx->memory, sizeof(gs_font *), "pdfi_read_type0_font FDepVector");
    if (pfont0->data.FDepVector == NULL) {
        /* We transferred ownership of pcmap to pfont0 above, but we didn't null the pointer
         * so we could keep using it. We must NULL it out before returning an error to prevent
         * reference counting problems.
         */
        pcmap = NULL;
        gs_free_object(ctx->memory, pfont0, "pdfi_read_type0_font(pfont0)");
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }
    *pfont0->data.FDepVector = (gs_font *)descpfont->pfont;
    pdfi_countdown(descpfont);
    descpfont = NULL;
    pfont0->data.fdep_size = 1;
    pfont0->data.CMap = (gs_cmap_t *)pcmap->gscmap;

    /* NULL he pointer to prevent any reference counting problems, ownership was
     * transferred to pfont0, but we maintained the pointer for easy access until this
     * point.
     */
    pcmap = NULL;

    pdft0->pfont = (gs_font_base *)pfont0;

    code = gs_definefont(ctx->font_dir, (gs_font *)pdft0->pfont);
    if (code < 0) {
        gs_free_object(ctx->memory, pfont0, "pdfi_read_type0_font(pfont0)");
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    /* object_num can be zero if the dictionary was defined inline */
    if (pdft0->object_num != 0) {
        code = replace_cache_entry(ctx, (pdf_obj *)pdft0);
        if (code < 0) {
            gs_free_object(ctx->memory, pfont0, "pdfi_read_type0_font(pfont0)");
            code = gs_note_error(gs_error_VMerror);
            goto error;
        }
    }

    *pfont = (gs_font *)pdft0->pfont;
    return 0;

error:
    pdfi_countdown(arr);
    pdfi_countdown(pcmap);
    pdfi_countdown(tounicode);
    pdfi_countdown(basefont);
    pdfi_countdown(decfont);
    pdfi_countdown(fontdesc);
    pdfi_countdown(ffile);
    pdfi_countdown(descpfont);
    pdfi_countdown(pdft0);

    return code;
}

int
pdfi_free_font_type0(pdf_obj *font)
{
    pdf_font_type0 *pdft0 = (pdf_font_type0 *)font;
    gs_font_type0 *pfont0 = (gs_font_type0 *)pdft0->pfont;
    pdfi_countdown(pdft0->PDF_font);
    pdfi_countdown(pdft0->BaseFont);
    pdfi_countdown(pdft0->FontDescriptor);
    pdfi_countdown(pdft0->Encoding);
    pdfi_countdown(pdft0->DescendantFonts);
    pdfi_countdown(pdft0->ToUnicode);
    gs_free_object(OBJ_MEMORY(pdft0), pfont0->data.Encoding, "pdfi_free_font_type0(data.Encoding)");
    /* We shouldn't need to free the fonts in the FDepVector, that should happen
        with DescendantFonts above.
     */
    gs_free_object(OBJ_MEMORY(pdft0), pfont0->data.FDepVector, "pdfi_free_font_type0(data.FDepVector)");
    gs_free_object(OBJ_MEMORY(pdft0), pfont0, "pdfi_free_font_type0(pfont0)");
    gs_free_object(OBJ_MEMORY(pdft0), pdft0, "pdfi_free_font_type0(pdft0)");

    return 0;
}
