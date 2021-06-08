/* Copyright (C) 2019-2021 Artifex Software, Inc.
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

/* Include this first so that we don't get a macro redefnition of 'offsetof' */
#include "pdf_int.h"

#include "strmio.h"
#include "stream.h"

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

extern const pdfi_cid_decoding_t *pdfi_cid_decoding_list[];
extern const pdfi_cid_subst_nwp_table_t *pdfi_cid_substnwp_list[];

static void pdfi_font0_cid_subst_tables(const char *reg, const int reglen, const char *ord,
                const int ordlen, pdfi_cid_decoding_t **decoding, pdfi_cid_subst_nwp_table_t **substnwp)
{
    int i;
    *decoding = NULL;
    *substnwp = NULL;
    /* This only makes sense for Adobe orderings */
    if (reglen == 5 && !memcmp(reg, "Adobe", 5)) {
        for (i = 0; pdfi_cid_decoding_list[i] != NULL; i++) {
            if (strlen(pdfi_cid_decoding_list[i]->s_order) == ordlen &&
                !memcmp(pdfi_cid_decoding_list[i]->s_order, ord, ordlen)) {
                *decoding = (pdfi_cid_decoding_t *)pdfi_cid_decoding_list[i];
                break;
            }
        }
        /* For now, also only for Adobe orderings */
        for (i = 0; pdfi_cid_substnwp_list[i] != NULL; i++) {
            if (strlen(pdfi_cid_substnwp_list[i]->ordering) == ordlen &&
                !memcmp(pdfi_cid_substnwp_list[i]->ordering, ord, ordlen)) {
                *substnwp = (pdfi_cid_subst_nwp_table_t *)pdfi_cid_substnwp_list[i];
                break;
            }
        }
    }
}

typedef enum
{
    decfont_type_cidtype0 = 1,
    decfont_type_cidtype0cff = 2,
    decfont_type_cidtype2 = 42
} decfont_type;

static decfont_type pdfi_fonttype_picker(byte *buf, int64_t buflen)
{
#define MAKEMAGIC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

    if (buflen >= 4) {
        if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC(0, 1, 0, 0)
            || MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('t', 'r', 'u', 'e')
            || MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('t', 't', 'c', 'f')) {
            return decfont_type_cidtype2;
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('O', 'T', 'T', 'O')) {
            return decfont_type_cidtype0cff; /* OTTO will end up as CFF */
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], 0) == MAKEMAGIC('%', '!', 'P', 0)) {
            return decfont_type_cidtype0; /* pfa */
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], 0) == MAKEMAGIC(1, 0, 4, 0)) {
            return decfont_type_cidtype0cff; /* 1C/CFF */
        }
        else if (MAKEMAGIC(buf[0], buf[1], 0, 0) == MAKEMAGIC(128, 1, 0, 0)) {
            return decfont_type_cidtype0; /* pfb */
        }
    }
    return 0;

#undef MAKEMAGIC
}

/* Call with a CIDFont name to try to find the CIDFont on disk
   call if with ffname NULL to load the default fallback CIDFont
   substitue
   Currently only loads subsitute - DroidSansFallback
 */
static int
pdfi_open_CIDFont_file(pdf_context * ctx, const char *ffname, const int namelen, byte ** buf, int64_t * buflen)
{
    int code = gs_error_invalidfont;
    if (ffname != NULL) {
        code = gs_error_invalidfont;
    }
    else {
        char fontfname[gp_file_name_sizeof];
        const char *romfsprefix = "%rom%Resource/CIDFSubst/";
        const int romfsprefixlen = strlen(romfsprefix);
        const char *defcidfallack = "DroidSansFallback.ttf";
        const int defcidfallacklen = strlen(defcidfallack);
        stream *s;
        code = 0;

        memcpy(fontfname, romfsprefix, romfsprefixlen);
        memcpy(fontfname + romfsprefixlen, defcidfallack, defcidfallacklen);
        fontfname[romfsprefixlen + defcidfallacklen] = '\0';

        s = sfopen(fontfname, "r", ctx->memory);
        if (s == NULL) {
            code = gs_note_error(gs_error_invalidfont);
        }
        else {
            sfseek(s, 0, SEEK_END);
            *buflen = sftell(s);
            sfseek(s, 0, SEEK_SET);
            *buf = gs_alloc_bytes(ctx->memory, *buflen, "pdfi_open_CIDFont_file(buf)");
            if (*buf != NULL) {
                sfread(*buf, 1, *buflen, s);
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
            sfclose(s);
        }
    }
    return code;
}

static int
pdfi_font0_glyph_name(gs_font *font, gs_glyph index, gs_const_string *pstr)
{
    return_error(gs_error_rangecheck);
}

static int
pdfi_font0_map_glyph_to_unicode(gs_font *font, gs_glyph glyph, int ch, ushort *u, unsigned int length)
{
    gs_glyph cc = glyph < GS_MIN_CID_GLYPH ? glyph : glyph - GS_MIN_CID_GLYPH;
    pdf_font_type0 *pt0font = (pdf_font_type0 *)font->client_data;
    int code = gs_error_undefined, i;
    uchar *unicode_return = (uchar *)u;
    pdf_cidfont_type2 *decfont;
    pdf_cmap *tounicode = (pdf_cmap *)pt0font->ToUnicode;
    pdfi_cid_subst_nwp_table_t *substnwp = pt0font->substnwp;

    code = pdfi_array_get(pt0font->ctx, pt0font->DescendantFonts, 0, (pdf_obj **)&decfont);
    if (code < 0 || decfont->type != PDF_FONT)
        return gs_error_undefined;

    code = gs_error_undefined;
    while (1) { /* Loop to make retrying with a substitute CID easier */
        /* Favour the ToUnicode if one exists */
        if (tounicode) {
            int l = 0;
            gs_cmap_lookups_enum_t lenum;
            gs_cmap_lookups_enum_init((const gs_cmap_t *)tounicode->gscmap, 0, &lenum);
            while (l == 0 && (code = gs_cmap_enum_next_lookup(font->memory, &lenum)) == 0) {
                gs_cmap_lookups_enum_t counter = lenum;
                while (l == 0 && (code = gs_cmap_enum_next_entry(&counter) == 0)) {
                    if (counter.entry.value_type == CODE_VALUE_CID) {
                        unsigned int v = 0;
                        for (i = 0; i < counter.entry.key_size; i++) {
                            v |= (counter.entry.key[0][counter.entry.key_size - i - 1]) << (i * 8);
                        }
                        if (ch == v) {
                            if (counter.entry.value.size == 1) {
                                l = 2;
                                if (unicode_return != NULL && length >= l) {
                                    unicode_return[0] = counter.entry.value.data[0];
                                    unicode_return[1] = counter.entry.value.data[1];
                                }
                            }
                            else if (counter.entry.value.size == 2) {
                                l = 2;
                                if (unicode_return != NULL && length >= l) {
                                    unicode_return[0] = counter.entry.value.data[0];
                                    unicode_return[1] = counter.entry.value.data[1];
                                }
                            }
                            else if (counter.entry.value.size == 3) {
                                l = 4;
                                if (unicode_return != NULL && length >= l) {
                                    unicode_return[0] = counter.entry.value.data[0];
                                    unicode_return[1] = counter.entry.value.data[1];
                                    unicode_return[2] = counter.entry.value.data[2];
                                    unicode_return[3] = 0;
                                }
                            }
                            else {
                                l = 4;
                                if (unicode_return != NULL && length >= l) {
                                    unicode_return[0] = counter.entry.value.data[0];
                                    unicode_return[1] = counter.entry.value.data[1];
                                    unicode_return[2] = counter.entry.value.data[1];
                                    unicode_return[3] = counter.entry.value.data[3];
                                }
                            }
                        }
                    }
                    else {
                        l = 0;
                    }
                }
            }
            if (l > 0)
                code = l;
            else
                code = gs_error_undefined;
        }

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
    return (code < 0 ? 0 : code);
}

int pdfi_read_type0_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, gs_font **pfont)
{
    int code, nlen;
    pdf_obj *cmap = NULL;
    pdf_cmap *pcmap = NULL;
    pdf_array *arr = NULL;
    pdf_dict *decfontdict = NULL; /* there can only be one */
    pdf_name *n = NULL;
    pdf_obj *basefont = NULL;
    pdf_obj *tounicode = NULL;
    const char *ffstrings[] = {"FontFile", "FontFile2", "FontFile3"};
    int ff;
    pdf_dict *dfontdesc = NULL;
    pdf_dict *fontdesc = NULL;
    pdf_stream *ffile = NULL;
    byte *buf;
    int64_t buflen;
    pdf_font *descpfont = NULL;
    pdf_font_type0 *pdft0 = NULL;
    gs_font_type0 *pfont0 = NULL;
    pdfi_cid_decoding_t *dec = NULL;
    pdfi_cid_subst_nwp_table_t *substnwp = NULL;
    bool descendant_substitute = false;

    /* We're supposed to have a FontDescriptor, it can be missing, and we have to carry on */
    (void)pdfi_dict_get(ctx, font_dict, "FontDescriptor", (pdf_obj **)&fontdesc);

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
    code = pdfi_array_get(ctx, arr, 0, (pdf_obj **)&decfontdict);
    pdfi_countdown(arr);
    arr = NULL;
    if (code < 0) goto error;
    if (decfontdict->type == PDF_FONT) {
        descpfont = (pdf_font *)decfontdict;
        decfontdict = descpfont->PDF_font;
        pdfi_countup(decfontdict);
    }
    else {
        if (decfontdict->type != PDF_DICT) {
            code = gs_note_error(gs_error_invalidfont);
            goto error;
        }
        code = pdfi_dict_get(ctx, (pdf_dict *)decfontdict, "Type", (pdf_obj **)&n);
        if (code < 0) goto error;
        if (n->type != PDF_NAME || n->length != 4 || memcmp(n->data, "Font", 4) != 0) {
            pdfi_countdown(n);
            code = gs_note_error(gs_error_invalidfont);
            goto error;
        }
        pdfi_countdown(n);
    }
#if 0
    code = pdfi_dict_get(ctx, (pdf_dict *)decfontdict, "Subtype", (pdf_obj **)&n);
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
#endif

    code = pdfi_dict_get(ctx, font_dict, "BaseFont", (pdf_obj **)&basefont);
    if (code < 0) {
        basefont = NULL;
    }

    code = pdfi_dict_get(ctx, font_dict, "ToUnicode", (pdf_obj **)&tounicode);
    if (code >= 0 && tounicode->type == PDF_STREAM) {
        pdf_cmap *tu = NULL;
        code = pdfi_read_cmap(ctx, tounicode, &tu);
        pdfi_countdown(tounicode);
        tounicode = (pdf_obj *)tu;
    }
    if (code < 0 || (tounicode != NULL && tounicode->type != PDF_CMAP)) {
        pdfi_countdown(tounicode);
        tounicode = NULL;
        code = 0;
    }

    if (descpfont == NULL) {
        code = pdfi_dict_get(ctx, decfontdict, "FontDescriptor", (pdf_obj **)&dfontdesc);
        if (code < 0)
            goto error;

        if (dfontdesc->type != PDF_DICT) {
            code = gs_note_error(gs_error_invalidfont);
            goto error;
        }
        buf = NULL;
        for (ff = 0;ff < (sizeof(ffstrings) / sizeof(ffstrings[0])); ff++) {
            code = pdfi_dict_get(ctx, dfontdesc, ffstrings[ff], (pdf_obj **)&ffile);
            if (code >= 0) break;
        }

        if (code < 0) { /* Try to load from disk, or default fallback */
            char *ffn = NULL;
            int ffnlen = 0;

            if (basefont->type == PDF_NAME || basefont->type == PDF_STRING) {
                ffn = (char *)((pdf_name *)basefont)->data;
                ffnlen = ((pdf_name *)basefont)->length;
            }
            code = pdfi_open_CIDFont_file(ctx, ffn, ffnlen, &buf, &buflen);
            if (code == gs_error_invalidfont) {
                code = pdfi_open_CIDFont_file(ctx, NULL, 0, &buf, &buflen);
                if (code >= 0) {
                    descendant_substitute = true;
                }
            }
            if (code < 0)
                goto error;
        }

        if (!buf) {
            if (ffile->type != PDF_STREAM) {
                code = gs_note_error(gs_error_invalidfont);
                goto error;
            }
            pdfi_countdown(dfontdesc);
            dfontdesc = NULL;

            /* We are supposed to be able to discern the underlying font type
             * from FontFile, FontFile2, or FontFile3 + Subtype of the stream.
             * Typical PDF, none of these prove trustworthy, so we have to
             * create the memory buffer here, so we can get a font type from
             * the first few bytes of the font data.
             * Owndership of the buffer is passed to the font reading function.
             */
            code = pdfi_stream_to_buffer(ctx, ffile, &buf, &buflen);
            if (code < 0) {
                pdfi_countdown(ffile);
                ffile = NULL;
                goto error;
            }
        }
        if ((code = pdfi_fonttype_picker(buf, buflen)) == 0) {
            pdf_name *subtype = NULL;
            code = pdfi_dict_get(ctx, ffile->stream_dict, "Subtype", (pdf_obj **)&subtype);
            if (code < 0 || subtype->type != PDF_NAME) {
                /* Corrupt stream header, no subtype, <shrug> try Type 1 */
                code = decfont_type_cidtype0;
            }
            else {
                if (subtype->length >= 13 && !memcmp(subtype->data, "CIDFontType0C", 13)) {
                    code = decfont_type_cidtype0cff;
                }
                else {
                    /* <shrug> try Type 1 */
                    code = decfont_type_cidtype0;
                }
            }
            pdfi_countdown(subtype);
        }
        pdfi_countdown(ffile);
        ffile = NULL;
        switch (code) {
            case decfont_type_cidtype2:
              code = pdfi_read_cidtype2_font(ctx, decfontdict, buf, buflen, &descpfont);
              break;
            case decfont_type_cidtype0cff:
            {
                code = pdfi_read_cff_font(ctx, decfontdict, buf, buflen, &descpfont, true);
                break;
            }
            case decfont_type_cidtype0:
            default:
              code = pdfi_read_cidtype0_font(ctx, decfontdict, buf, buflen, &descpfont);
              break;
        }
        if (code >= 0)
            ((pdf_cidfont_t *)descpfont)->substitute = descendant_substitute;
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
            if (reg != NULL && ord != NULL) {
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
            pdfi_font0_cid_subst_tables(r, rlen, o, olen, &dec, &substnwp);
        else {
            dec = NULL;
            substnwp = NULL;
        }
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
    pdft0->object_num = font_dict->object_num;
    pdft0->generation_num = font_dict->generation_num;
    pdft0->indirect_num = font_dict->indirect_num;
    pdft0->indirect_gen = font_dict->indirect_gen;
    pdft0->Encoding = (pdf_obj *)pcmap;
    pdft0->ToUnicode = tounicode;
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
    gs_free_object(OBJ_MEMORY(pdft0), pfont0->data.Encoding, "pdfi_free_font_type0(data.Encoding)");
    /* We shouldn't need to free the fonts in the FDepVector, that should happen
        with DescendantFonts above.
     */
    gs_free_object(OBJ_MEMORY(pdft0), pfont0->data.FDepVector, "pdfi_free_font_type0(data.FDepVector)");
    gs_free_object(OBJ_MEMORY(pdft0), pfont0, "pdfi_free_font_type0(pfont0)");
    gs_free_object(OBJ_MEMORY(pdft0), pdft0, "pdfi_free_font_type0(pdft0)");

    return 0;
}
