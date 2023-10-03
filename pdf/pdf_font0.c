/* Copyright (C) 2019-2023 Artifex Software, Inc.
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

/* Include this first so that we don't get a macro redefnition of 'offsetof' */
#include "pdf_int.h"

/* code for type 0 (CID) font handling */
#include "gxfont.h"
#include "gxfont0.h"

#include "pdf_int.h"
#include "pdf_font.h"
#include "pdf_font0.h"
#include "pdf_font1C.h"
#include "pdf_font_types.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_file.h"
#include "pdf_cmap.h"
#include "pdf_deref.h"

#include "gsutil.h"        /* For gs_next_ids() */

static int
pdfi_font0_glyph_name(gs_font *pfont, gs_glyph index, gs_const_string *pstr)
{
    int code;
    pdf_font_type0 *pt0font = (pdf_font_type0 *)pfont->client_data;
    char gnm[64];
    pdf_context *ctx = pt0font->ctx;
    uint gindex = 0;

    gs_snprintf(gnm, 64, "%lu", (long)index);
    code = (*ctx->get_glyph_index)((gs_font *)pfont, (byte *)gnm, strlen(gnm), &gindex);
    if (code < 0)
        return code;
    code = (*ctx->get_glyph_name)(pfont, (gs_glyph)gindex, (gs_const_string *)pstr);

    return code;
}

static int
pdfi_font0_map_glyph_to_unicode(gs_font *font, gs_glyph glyph, int ch, ushort *u, unsigned int length)
{
    gs_glyph cc = glyph < GS_MIN_CID_GLYPH ? glyph : glyph - GS_MIN_CID_GLYPH;
    pdf_font_type0 *pt0font = (pdf_font_type0 *)font->client_data;
    int code = gs_error_undefined, i;
    uchar *unicode_return = (uchar *)u;
    pdf_cidfont_type2 *decfont = NULL;
    pdfi_cid_subst_nwp_table_t *substnwp = pt0font->substnwp;

    code = pdfi_array_get(pt0font->ctx, pt0font->DescendantFonts, 0, (pdf_obj **)&decfont);
    if (code < 0 || pdfi_type_of(decfont) != PDF_FONT) {
        pdfi_countdown(decfont);
        return gs_error_undefined;
    }

    code = gs_error_undefined;
    while (1) { /* Loop to make retrying with a substitute CID easier */
        /* Favour the ToUnicode if one exists */
        code = pdfi_tounicode_char_to_unicode(pt0font->ctx, (pdf_cmap *)pt0font->ToUnicode, glyph, ch, u, length);

        if (code == gs_error_undefined && pt0font->decoding) {
            const int *n;

            if (cc / 256 < pt0font->decoding->nranges) {
                n = (const int *)pt0font->decoding->ranges[cc / 256][cc % 256];
                for (i = 0; i < pt0font->decoding->val_sizes; i++) {
                    unsigned int cmapcc;
                    if (n[i] == -1)
                        break;
                    cc = n[i];
                    cmapcc = (unsigned int)cc;
                    if (decfont->pdfi_font_type == e_pdf_cidfont_type2)
                        code = pdfi_fapi_check_cmap_for_GID((gs_font *)decfont->pfont, (unsigned int)cc, &cmapcc);
                    else
                        code = 0;
                    if (code >= 0 && cmapcc != 0){
                        code = 0;
                        break;
                    }
                }
                /* If it's a TTF derived CIDFont, we prefer a code point supported by the cmap table
                   but if not, use the first available one
                 */
                if (code < 0 && n[0] != -1) {
                    cc = n[0];
                    code = 0;
                }
            }
            if (code >= 0) {
                if (cc > 65535) {
                    code = 4;
                    if (unicode_return != NULL && length >= code) {
                        unicode_return[0] = (cc & 0xFF000000)>> 24;
                        unicode_return[1] = (cc & 0x00FF0000) >> 16;
                        unicode_return[2] = (cc & 0x0000FF00) >> 8;
                        unicode_return[3] = (cc & 0x000000FF);
                    }
                }
                else {
                    code = 2;
                    if (unicode_return != NULL && length >= code) {
                        unicode_return[0] = (cc & 0x0000FF00) >> 8;
                        unicode_return[1] = (cc & 0x000000FF);
                    }
                }
            }
        }
        /* If we get here, and still don't have a usable code point, check for a
           pre-defined CID substitution, and if there's one, jump back to the start
           and try again.
         */
        if (code == gs_error_undefined && substnwp) {
            for (i = 0; substnwp->subst[i].s_type != 0; i++ ) {
                if (cc >= substnwp->subst[i].s_scid && cc <= substnwp->subst[i].e_scid) {
                    cc = substnwp->subst[i].s_dcid + (cc - substnwp->subst[i].s_scid);
                    substnwp = NULL;
                    break;
                }
                if (cc >= substnwp->subst[i].s_dcid
                 && cc <= substnwp->subst[i].s_dcid + (substnwp->subst[i].e_scid - substnwp->subst[i].s_scid)) {
                    cc = substnwp->subst[i].s_scid + (cc - substnwp->subst[i].s_dcid);
                    substnwp = NULL;
                    break;
                }
            }
            if (substnwp == NULL)
                continue;
        }
        break;
    }
    pdfi_countdown(decfont);
    return (code < 0 ? 0 : code);
}

int pdfi_read_type0_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_font **ppdffont)
{
    int code, nlen;
    pdf_obj *cmap = NULL;
    pdf_cmap *pcmap = NULL;
    pdf_array *arr = NULL;
    pdf_dict *decfontdict = NULL; /* there can only be one */
    pdf_name *n = NULL;
    pdf_obj *basefont = NULL;
    pdf_obj *tounicode = NULL;
    pdf_dict *dfontdesc = NULL;
    pdf_dict *fontdesc = NULL;
    pdf_stream *ffile = NULL;
    pdf_font *descpfont = NULL;
    pdf_font_type0 *pdft0 = NULL;
    gs_font_type0 *pfont0 = NULL;
    pdfi_cid_decoding_t *dec = NULL;
    pdfi_cid_subst_nwp_table_t *substnwp = NULL;

    /* We're supposed to have a FontDescriptor, it can be missing, and we have to carry on */
    (void)pdfi_dict_get(ctx, font_dict, "FontDescriptor", (pdf_obj **)&fontdesc);

    if (fontdesc != NULL) {
        pdf_obj *Name = NULL;

        code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontName", PDF_NAME, (pdf_obj**)&Name);
        if (code < 0)
            pdfi_set_warning(ctx, 0, NULL, W_PDF_FDESC_BAD_FONTNAME, "pdfi_load_font", "");
        pdfi_countdown(Name);
    }

    code = pdfi_dict_get(ctx, font_dict, "Encoding", &cmap);
    if (code < 0) goto error;

    if (pdfi_type_of(cmap) == PDF_CMAP) {
        pcmap = (pdf_cmap *)cmap;
        cmap = NULL;
    }
    else {
        code = pdfi_read_cmap(ctx, cmap, &pcmap);
        pdfi_countdown(cmap);
        cmap = NULL;
        if (code < 0) goto error;
    }

    code = pdfi_dict_get(ctx, font_dict, "DescendantFonts", (pdf_obj **)&arr);
    if (code < 0) goto error;

    if (pdfi_type_of(arr) != PDF_ARRAY || arr->size != 1) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    code = pdfi_array_get(ctx, arr, 0, (pdf_obj **)&decfontdict);
    pdfi_countdown(arr);
    arr = NULL;
    if (code < 0) goto error;
    switch (pdfi_type_of(decfontdict)) {
        case PDF_FONT:
            descpfont = (pdf_font *)decfontdict;
            decfontdict = descpfont->PDF_font;
            pdfi_countup(decfontdict);
            break;
        case PDF_DICT:
            code = pdfi_dict_get(ctx, (pdf_dict *)decfontdict, "Type", (pdf_obj **)&n);
            if (code < 0) goto error;
            if (pdfi_type_of(n) != PDF_NAME || n->length != 4 || memcmp(n->data, "Font", 4) != 0) {
                pdfi_countdown(n);
                code = gs_note_error(gs_error_invalidfont);
                goto error;
            }
            pdfi_countdown(n);
            break;
        default:
            code = gs_note_error(gs_error_invalidfont);
            goto error;
    }
#if 0
    code = pdfi_dict_get(ctx, (pdf_dict *)decfontdict, "Subtype", (pdf_obj **)&n);
    if (code < 0)
        goto error;

    if (pdfi_type_of(n) != PDF_NAME || n->length != 12 || memcmp(n->data, "CIDFontType", 11) != 0) {
        pdfi_countdown(n);
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    /* cidftype is ignored for now, but we may need to know it when
       subsitutions are allowed
     */
    cidftype = n->data[11] - 48;

    pdfi_countdown(n);
#endif

    code = pdfi_dict_get(ctx, font_dict, "BaseFont", (pdf_obj **)&basefont);
    if (code < 0) {
        basefont = NULL;
    }

    if (ctx->args.ignoretounicode != true) {
        code = pdfi_dict_get(ctx, font_dict, "ToUnicode", (pdf_obj **)&tounicode);
        if (code >= 0 && pdfi_type_of(tounicode) == PDF_STREAM) {
            pdf_cmap *tu = NULL;
            code = pdfi_read_cmap(ctx, tounicode, &tu);
            pdfi_countdown(tounicode);
            tounicode = (pdf_obj *)tu;
        }
        if (code < 0 || (tounicode != NULL && pdfi_type_of(tounicode) != PDF_CMAP)) {
            pdfi_countdown(tounicode);
            tounicode = NULL;
            code = 0;
        }
    }
    else {
        tounicode = NULL;
    }

    if (descpfont == NULL) {
        gs_font *pf;

        code = pdfi_load_font(ctx, stream_dict, page_dict, decfontdict, &pf, true);
        if (code < 0)
            goto error;
        descpfont = (pdf_font *)pf->client_data;
    }

    if (descpfont->pdfi_font_type < e_pdf_cidfont_type0 || descpfont->pdfi_font_type > e_pdf_cidfont_type4) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }

    if (descpfont != NULL && ((pdf_cidfont_t *)descpfont)->substitute) {
        pdf_obj *csi = NULL;
        pdf_string *reg = NULL, *ord = NULL;
        char *r = NULL, *o = NULL;
        int rlen = 0, olen = 0;

        code = pdfi_dict_get(ctx, decfontdict, "CIDSystemInfo", (pdf_obj **)&csi);
        if (code >= 0) {
            (void)pdfi_dict_get(ctx, (pdf_dict *)csi, "Registry", (pdf_obj **)&reg);
            (void)pdfi_dict_get(ctx, (pdf_dict *)csi, "Ordering", (pdf_obj **)&ord);
            if (reg != NULL && pdfi_type_of(reg) == PDF_STRING
             && ord != NULL && pdfi_type_of(ord) == PDF_STRING) {
                r = (char *)reg->data;
                rlen = reg->length;
                o = (char *)ord->data;
                olen = ord->length;
            }
            pdfi_countdown(csi);
            pdfi_countdown(reg);
            pdfi_countdown(ord);
        }
        if (r == NULL || o == NULL) {
            r = (char *)pcmap->csi_reg.data;
            rlen = pcmap->csi_reg.size;
            o = (char *)pcmap->csi_ord.data;
            olen = pcmap->csi_ord.size;
        }
        if (rlen > 0 && olen > 0)
            pdfi_cidfont_cid_subst_tables(r, rlen, o, olen, &dec, &substnwp);

        ((pdf_cidfont_t *)descpfont)->decoding = dec;
        ((pdf_cidfont_t *)descpfont)->substnwp = substnwp;
    }
    /* reference is now owned by the descendent font created above */
    pdfi_countdown(decfontdict);
    decfontdict = NULL;
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
    pdft0->filename = NULL;
    pdft0->object_num = font_dict->object_num;
    pdft0->generation_num = font_dict->generation_num;
    pdft0->indirect_num = font_dict->indirect_num;
    pdft0->indirect_gen = font_dict->indirect_gen;
    pdft0->Encoding = (pdf_obj *)pcmap;
    pdft0->ToUnicode = tounicode;
    tounicode = NULL;
    pdft0->DescendantFonts = arr;
    pdft0->PDF_font = font_dict;
    pdfi_countup(font_dict);
    pdft0->FontDescriptor = fontdesc;
    fontdesc = NULL;
    pdft0->BaseFont = basefont;
    pdft0->decoding = dec;
    pdft0->substnwp = substnwp;

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
        pfont0->key_name.size = nlen;
        memcpy(pfont0->font_name.chars, nobj->data, nlen);
        pfont0->font_name.size = nlen;
    }
    else {
        nlen = descpfont->pfont->key_name.size > gs_font_name_max ? gs_font_name_max : descpfont->pfont->key_name.size;

        memcpy(pfont0->key_name.chars, descpfont->pfont->key_name.chars, nlen);
        pfont0->key_name.size = nlen;
        memcpy(pfont0->font_name.chars, descpfont->pfont->font_name.chars, nlen);
        pfont0->font_name.size = nlen;
    }

    if (pcmap->name.size > 0) {
        if (pfont0->key_name.size + pcmap->name.size + 1 < gs_font_name_max) {
            memcpy(pfont0->key_name.chars + pfont0->key_name.size, "-", 1);
            memcpy(pfont0->key_name.chars + pfont0->key_name.size + 1, pcmap->name.data, pcmap->name.size);
            pfont0->key_name.size += pcmap->name.size + 1;
        }
        if (pfont0->font_name.size + pcmap->name.size + 1 < gs_font_name_max) {
            memcpy(pfont0->font_name.chars + pfont0->font_name.size, "-", 1);
            memcpy(pfont0->font_name.chars + pfont0->font_name.size + 1, pcmap->name.data, pcmap->name.size);
            pfont0->font_name.size += pcmap->name.size + 1;
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
        (void)replace_cache_entry(ctx, (pdf_obj *)pdft0);
    }

    *ppdffont = (pdf_font *)pdft0;
    return 0;

error:
    pdfi_set_error_var(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_type0_font", "Error reading embedded Type0 font object %u\n", font_dict->object_num);

    pdfi_countdown(arr);
    pdfi_countdown(pcmap);
    pdfi_countdown(tounicode);
    pdfi_countdown(basefont);
    pdfi_countdown(decfontdict);
    pdfi_countdown(dfontdesc);
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
    pdfi_countdown(pdft0->filename);

    gs_free_object(OBJ_MEMORY(pdft0), pfont0->data.Encoding, "pdfi_free_font_type0(data.Encoding)");
    /* We shouldn't need to free the fonts in the FDepVector, that should happen
        with DescendantFonts above.
     */
    gs_free_object(OBJ_MEMORY(pdft0), pfont0->data.FDepVector, "pdfi_free_font_type0(data.FDepVector)");
    gs_free_object(OBJ_MEMORY(pdft0), pfont0, "pdfi_free_font_type0(pfont0)");
    gs_free_object(OBJ_MEMORY(pdft0), pdft0, "pdfi_free_font_type0(pdft0)");

    return 0;
}
