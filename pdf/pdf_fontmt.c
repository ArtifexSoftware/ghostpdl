/* Copyright (C) 2020-2025 Artifex Software, Inc.
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

#include "pdf_int.h"
#include "pdf_types.h"
#include "pdf_font_types.h"
#include "pdf_dict.h"
#include "pdf_font.h"
#include "pdf_fontmt.h"

#ifdef UFST_BRIDGE
int
pdfi_alloc_mt_font(pdf_context *ctx, pdf_string *fco_path, int32_t index, pdf_font_microtype **font)
{
    int code;
    pdf_font_microtype *pdffont = NULL;
    gs_font_base *pbfont = NULL;
    pdf_name *encname;
    static const byte *StandardEncodingString = (const byte *)"StandardEncoding";
    static const byte *SymbolEncodingString = (const byte *)"SymbolEncoding";
    static const byte *DingbatsEncodingString = (const byte *)"DingbatsEncoding";

    pdffont = (pdf_font_microtype *)gs_alloc_bytes(ctx->memory, sizeof(pdf_font_microtype), "pdfi (mt pdf_font)");
    if (pdffont == NULL)
        return_error(gs_error_VMerror);

    memset(pdffont, 0x00, sizeof(pdf_font));
    pdffont->type = PDF_FONT;
    pdffont->ctx = ctx;
    pdffont->pdfi_font_type = e_pdf_font_microtype;
    pdffont->fco_index = index;
    pdffont->filename = fco_path;
    pdfi_countup(pdffont->filename);

#if REFCNT_DEBUG
    pdffont->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %"PRIi64"\n", pdffont->type, pdffont->UID);
#endif

    pdfi_countup(pdffont);

    pbfont = (gs_font_base *)gs_alloc_struct(ctx->memory, gs_font_base, &st_gs_font_base, "pdfi (mt pbfont)");
    if (pbfont == NULL) {
        pdfi_countdown(pdffont);
        return_error(gs_error_VMerror);
    }
    memset(pbfont, 0x00, sizeof(gs_font_base));

    pdffont->pfont = pbfont;
    pbfont->client_data = pdffont;
    gs_make_identity(&pbfont->FontMatrix);
    pbfont->memory = ctx->memory;
    gs_notify_init(&pbfont->notify_list, ctx->memory);
    pbfont->FontType = ft_MicroType;
    pbfont->BitmapWidths = true;
    pbfont->ExactSize = fbit_use_outlines;
    pbfont->InBetweenSize = fbit_use_outlines;
    pbfont->TransformedChar = fbit_use_outlines;

    pbfont->FontBBox.p.x = -136.0;
    pbfont->FontBBox.p.y = -283.0;
    pbfont->FontBBox.q.x = 711.0;
    pbfont->FontBBox.q.y = 942.0;
    uid_set_invalid(&pbfont->UID);
    /* The buildchar proc will be filled in by FAPI -
       we won't worry about working without FAPI */
    pbfont->procs.encode_char = pdfi_encode_char;
    pbfont->procs.glyph_name = pdfi_glyph_name;
    pbfont->procs.decode_glyph = pdfi_decode_glyph;
    pbfont->procs.define_font = gs_no_define_font;
    pbfont->procs.make_font = gs_no_make_font;
    pbfont->procs.init_fstack = gs_default_init_fstack;
    pbfont->procs.font_info = pdfi_default_font_info;
    pbfont->procs.next_char_glyph = gs_default_next_char_glyph;

    pbfont->procs.glyph_info = gs_default_glyph_info;
    pbfont->procs.glyph_outline = gs_no_glyph_outline;
    pbfont->procs.build_char = NULL;
    pbfont->procs.same_font = gs_default_same_font;
    pbfont->procs.enumerate_glyph = gs_no_enumerate_glyph;

    pbfont->encoding_index = ENCODING_INDEX_UNKNOWN;
    pbfont->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

    switch (index) {
        case 27:
        case 76:
            code = pdfi_name_alloc(ctx, (byte *)SymbolEncodingString, strlen((char *)SymbolEncodingString), (pdf_obj **)&encname);
            break;
        case 82:
            code = pdfi_name_alloc(ctx, (byte *)DingbatsEncodingString, strlen((char *)DingbatsEncodingString), (pdf_obj **)&encname);
            break;
        default:
            code = pdfi_name_alloc(ctx, (byte *)StandardEncodingString, strlen((char *)StandardEncodingString), (pdf_obj **)&encname);
            break;
    }
    if (code < 0) {
        pdfi_countdown(pdffont);
        return code;
    }
    pdfi_countup(encname);

    code = pdfi_create_Encoding(ctx, (pdf_font *)pdffont, (pdf_obj *)encname, NULL, (pdf_obj **)&pdffont->Encoding);
    pdfi_countdown(encname);
    if (code < 0) {
        pdfi_countdown(pdffont);
        return code;
    }
    *font = pdffont;
    return 0;
}

int pdfi_free_font_microtype(pdf_obj *font)
{
    pdf_font_microtype *mtfont = (pdf_font_microtype *)font;

    if (mtfont->pfont)
        gs_free_object(OBJ_MEMORY(mtfont), mtfont->pfont, "Free mt gs_font");

    if (mtfont->Widths)
        gs_free_object(OBJ_MEMORY(mtfont), mtfont->Widths, "Free mt font Widths array");

    pdfi_countdown(mtfont->FontDescriptor);
    pdfi_countdown(mtfont->Encoding);
    pdfi_countdown(mtfont->BaseFont);
    pdfi_countdown(mtfont->PDF_font);
    pdfi_countdown(mtfont->ToUnicode);
    pdfi_countdown(mtfont->filename);

    gs_free_object(OBJ_MEMORY(mtfont), mtfont, "Free mt font");

    return 0;
}

int
pdfi_copy_microtype_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont)
{
    pdf_obj *tmp;
    bool force_symbolic = false;
    char pathnm[gp_file_name_sizeof];
    gs_font_base *pbfont;
    pdf_font_microtype *dmtfont, *mtfont = (pdf_font_microtype *)spdffont;
    int code = pdfi_alloc_mt_font(ctx, mtfont->filename, mtfont->fco_index, &dmtfont);
    if (code < 0)
        return code;
    pbfont = dmtfont->pfont;
    memcpy(dmtfont, mtfont, sizeof(pdf_font_microtype));
    dmtfont->refcnt = 1;
    dmtfont->pfont = pbfont;
    memcpy(dmtfont->pfont, mtfont->pfont, sizeof(gs_font_base));
    dmtfont->pfont->client_data = dmtfont;
    pbfont->FAPI = NULL;
    pbfont->FAPI_font_data = NULL;
    memcpy(pathnm, dmtfont->filename->data, dmtfont->filename->length);
    pathnm[dmtfont->filename->length] = '\0';

    dmtfont->PDF_font = font_dict;
    dmtfont->object_num = font_dict->object_num;
    dmtfont->generation_num = font_dict->generation_num;
    pdfi_countup(dmtfont->PDF_font);
    /* We want basefont and descriptor, but we can live without them */
    dmtfont->BaseFont = NULL;
    (void)pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &dmtfont->BaseFont);
    dmtfont->FontDescriptor = NULL;
    (void)pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, (pdf_obj **)&dmtfont->FontDescriptor);
    pdfi_font_set_first_last_char(ctx, font_dict, (pdf_font *)dmtfont);
    (void)pdfi_font_create_widths(ctx, font_dict, (pdf_font*)dmtfont, (double)0.001);

    if (pdfi_font_known_symbolic(dmtfont->BaseFont)) {
        force_symbolic = true;
        dmtfont->descflags |= 4;
    }

    tmp = NULL;
    code = pdfi_dict_knownget(ctx, font_dict, "Encoding", &tmp);
    if (code == 1) {
        if (pdfi_type_of(tmp) == PDF_NAME && force_symbolic == true) {
            dmtfont->Encoding = spdffont->Encoding;
            pdfi_countup(dmtfont->Encoding);
        }
        else if (pdfi_type_of(tmp) == PDF_DICT && (dmtfont->descflags & 4) != 0) {
            code = pdfi_create_Encoding(ctx, (pdf_font *)dmtfont, tmp, (pdf_obj *)spdffont->Encoding, (pdf_obj **) &dmtfont->Encoding);
            if (code >= 0)
                code = 1;
        }
        else {
            code = pdfi_create_Encoding(ctx, (pdf_font *)dmtfont, tmp, NULL, (pdf_obj **) & dmtfont->Encoding);
            if (code >= 0)
                code = 1;
        }
        pdfi_countdown(tmp);
        tmp = NULL;
    }
    else {
        pdfi_countup(dmtfont->Encoding);
        pdfi_countdown(tmp);
        tmp = NULL;
        code = 0;
    }

    gs_notify_init(&dmtfont->pfont->notify_list, ctx->memory);
    code = gs_definefont(ctx->font_dir, (gs_font *) dmtfont->pfont);
    if (code < 0) {
        pdfi_countdown(dmtfont);
        return code;
    }
    code = pdfi_fapi_passfont((pdf_font *)dmtfont, dmtfont->fco_index, (char *)"UFST", pathnm, NULL, 0);
    if (code < 0) {
        pdfi_countdown(dmtfont);
        dmtfont = NULL;
    }
    *tpdffont = (pdf_font *)dmtfont;
    return code;
}

#endif /* UFST_BRIDGE */
