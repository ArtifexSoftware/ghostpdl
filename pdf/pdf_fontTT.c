/* Copyright (C) 2019-2024 Artifex Software, Inc.
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

/* code for TrueType font handling */

#include "pdf_int.h"
#include "pdf_font.h"
#include "pdf_fontTT.h"
#include "pdf_font1C.h" /* OTTO support */
#include "pdf_font_types.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_deref.h"
#include "gxfont42.h"
#include "gscencs.h"
#include "gsagl.h"
#include "gsutil.h"        /* For gs_next_ids() */

enum {
    CMAP_TABLE_NONE = 0,
    CMAP_TABLE_10_PRESENT = 1,
    CMAP_TABLE_30_PRESENT = 2,
    CMAP_TABLE_31_PRESENT = 4,
    CMAP_TABLE_310_PRESENT = 8
};

static int
pdfi_ttf_string_proc(gs_font_type42 * pfont, ulong offset, uint length, const byte ** pdata)
{
    pdf_font_truetype *ttfont = (pdf_font_truetype *)pfont->client_data;
    int code = 0;

    if ((uint64_t)offset + length > ttfont->sfnt->length) {
        *pdata = NULL;
        code = gs_note_error(gs_error_invalidfont);
    }
    else {
        *pdata = ttfont->sfnt->data + offset;
    }
    return code;
}

static gs_glyph pdfi_ttf_encode_char(gs_font *pfont, gs_char chr, gs_glyph_space_t sp)
{
    pdf_font_truetype *ttfont = (pdf_font_truetype *)pfont->client_data;
    gs_glyph g = GS_NO_GLYPH;
    uint ID;
    int code;

    if ((ttfont->descflags & 4) != 0 || sp == GLYPH_SPACE_INDEX) {
        int code = pdfi_fapi_check_cmap_for_GID(pfont, (uint)chr, &ID);
        if (code < 0 || ID == 0)
            code = pdfi_fapi_check_cmap_for_GID(pfont, (uint)(chr | 0xf0 << 8), &ID);
        g = (gs_glyph)ID;
    }
    else {
        pdf_context *ctx = (pdf_context *)ttfont->ctx;

        if (ttfont->Encoding != NULL) { /* safety */
            pdf_name *GlyphName = NULL;
            code = pdfi_array_get(ctx, ttfont->Encoding, (uint64_t)chr, (pdf_obj **)&GlyphName);
            if (code >= 0) {
                code = (*ctx->get_glyph_index)(pfont, (byte *)GlyphName->data, GlyphName->length, &ID);
                pdfi_countdown(GlyphName);
                if (code >= 0)
                    g = (gs_glyph)ID;
            }
        }
    }

    return g;
}

int pdfi_find_post_entry(gs_font_type42 *pfont, gs_const_string *gname, uint *cc)
{
    pdf_font_truetype *ttfont = (pdf_font_truetype *)pfont->client_data;
    pdf_context *ctx = (pdf_context *)ttfont->ctx;
    int code = 0;
    if (ttfont->post != NULL) {
        pdf_name *name = NULL;
        pdf_num *n = NULL;

        code = pdfi_name_alloc(ctx, (byte *)gname->data, gname->size, (pdf_obj **)&name);
        if (code >= 0) {
            pdfi_countup(name);
            code = pdfi_dict_get_by_key(ctx, ttfont->post, name, (pdf_obj **)&n);
            if (code >= 0 && pdfi_type_of(n) == PDF_INT) {
                *cc = (uint)n->value.i;
            }
            else
              *cc = 0;
            pdfi_countdown(name);
            pdfi_countdown(n);
        }
    }
    else
        code = gs_error_VMerror;

    if (code == gs_error_VMerror){
        uint i;
        gs_string postname = {0};

        code = gs_error_undefined;
        if (pfont->data.numGlyphs > 0) { /* protect from corrupt font */
            for (i = 0; i < pfont->data.numGlyphs; i++) {
                code = gs_type42_find_post_name(pfont, (gs_glyph)i, &postname);
                if (code >= 0) {
                    if (gname->data[0] == postname.data[0]
                        && gname->size == postname.size
                        && !strncmp((char *)gname->data, (char *)postname.data, postname.size))
                    {
                        *cc = i;
                        code = 0;
                        break;
                    }
                }
            }
        }
    }
    return code;
}

extern single_glyph_list_t SingleGlyphList[];

static uint pdfi_type42_get_glyph_index(gs_font_type42 *pfont, gs_glyph glyph)
{
    pdf_font_truetype *ttfont = (pdf_font_truetype *)pfont->client_data;
    uint gind = 0;
    uint cc = 0;
    int i, code = 0;

    if ((ttfont->descflags & 4) != 0 || glyph >= GS_MIN_GLYPH_INDEX) {
        gind = (uint)glyph < GS_MIN_GLYPH_INDEX ? glyph : glyph - GS_MIN_GLYPH_INDEX;
    }
    else {
        pdf_context *ctx = (pdf_context *)ttfont->ctx;
        gs_const_string gname;

        code = (*ctx->get_glyph_name)((gs_font *)pfont, glyph, &gname);
        if (code < 0 || gname.data == NULL) {
            return (uint)glyph;
        }
        if (gname.size == 7 && gname.data[0] == '.' && strncmp((char *)gname.data, ".notdef", 7) == 0) {
            return 0; /* .notdef is GID 0, so short cut the rest of the function */
        }

        if (ttfont->cmap == pdfi_truetype_cmap_10) {
            gs_glyph g;

            g = gs_c_name_glyph((const byte *)gname.data, gname.size);
            if (g != GS_NO_GLYPH) {
                g = (gs_glyph)gs_c_decode(g, ENCODING_INDEX_MACROMAN);
            }
            else {
                g = GS_NO_CHAR;
            }

            if (g != GS_NO_CHAR) {
                code = pdfi_fapi_check_cmap_for_GID((gs_font *)pfont, (uint)g, &cc);
            }

            if (code < 0 || cc == 0) {
                code = pdfi_find_post_entry(pfont, &gname, &cc);
                if (code < 0) {
                    cc = 0;
                    code = 0;
                }
            }
        }
        else {
            /* In theory, this should be 3,1 cmap, but we have examples that use 0,1 or other
               Unicode "platform" cmap tables, so "hail mary" just try it
             */
            single_glyph_list_t *sgl = (single_glyph_list_t *)&(SingleGlyphList);
            /* Not to spec, but... if we get a "uni..." formatted name, use
               the hex value from that.
             */
            if (gname.size > 5 && !strncmp((char *)gname.data, "uni", 3)) {
                char gnbuf[64];
                int l = (gname.size - 3) > 63 ? 63 : gname.size - 3;

                memcpy(gnbuf, gname.data + 3, l);
                gnbuf[l] = '\0';
                l = sscanf(gnbuf, "%x", &gind);
                if (l > 0)
                    (void)pdfi_fapi_check_cmap_for_GID((gs_font *)pfont, (uint)gind, &cc);
                else
                    cc = 0;
            }
            else {
                /* Slow linear search */
                for (i = 0; sgl->Glyph != 0x00; i++) {
                    if (sgl->Glyph[0] == gname.data[0]
                        && strlen(sgl->Glyph) == gname.size
                        && !strncmp((char *)sgl->Glyph, (char *)gname.data, gname.size))
                        break;
                    sgl++;
                }
                if (sgl->Glyph != NULL) {
                    code = pdfi_fapi_check_cmap_for_GID((gs_font *)pfont, (uint)sgl->Unicode, &cc);
                    if (code < 0 || cc == 0)
                        cc = 0;
                }
                else
                    cc = 0;

                if (cc == 0) {
                    code = pdfi_find_post_entry(pfont, &gname, &cc);
                    if (code < 0) {
                        cc = (uint)glyph;
                        code = 0;
                    }
                }
            }
        }
        gind = cc;
    }
    return gind;
}

static int
pdfi_ttf_enumerate_glyph(gs_font *font, int *pindex, gs_glyph_space_t glyph_space, gs_glyph *pglyph)
{
    if (glyph_space == GLYPH_SPACE_INDEX) {
        return gs_type42_enumerate_glyph(font, pindex, glyph_space, pglyph);
    }
    else if (glyph_space == GLYPH_SPACE_NAME) {
        pdf_font_truetype *ttfont = (pdf_font_truetype *)font->client_data;

        if ((ttfont->descflags & 4) == 0) {
            if (*pindex <= 0) {
                *pindex = 0;
            }
            *pglyph = (*font->procs.encode_char)(font, (gs_char)*pindex, glyph_space);
            if (*pglyph == GS_NO_GLYPH)
                *pindex = 0;
            else
                (*pindex)++;
        }
    }
    else
        *pindex = 0;
    return 0;
}

static int pdfi_ttf_glyph_name(gs_font *pfont, gs_glyph glyph, gs_const_string * pstr)
{
    pdf_font_truetype *ttfont = (pdf_font_truetype *)pfont->client_data;
    pdf_context *ctx = (pdf_context *)ttfont->ctx;
    uint ID = 0;
    int l, code = -1;

    if (glyph >= GS_MIN_CID_GLYPH) {	/* Fabricate a numeric name. */
        char cid_name[sizeof(gs_glyph) * 3 + 1];

        l = gs_snprintf(cid_name, sizeof(cid_name), "%lu", (ulong) glyph);
        /* We need to create the entry in the name table... */
        code = (*ctx->get_glyph_index)(pfont, (byte *)cid_name, l, &ID);
        if (code < 0)
            return -1;

        code = (*ctx->get_glyph_name)(pfont, (gs_glyph)ID, pstr);
        if (code < 0)
            return -1; /* No name, trigger pdfwrite Type 3 fallback */
    } else {
        if ((ttfont->descflags & 4) != 0) {
            code = gs_type42_find_post_name((gs_font_type42 *)pfont, glyph, (gs_string *)pstr);
            if (code < 0) {
                char buf[64];
                l = gs_snprintf(buf, sizeof(buf), "~gs~gName~%04x", (uint)glyph);
                code = (*ctx->get_glyph_index)(pfont, (byte *)buf, l, &ID);
            }
            else {
                code = (*ctx->get_glyph_index)(pfont, (byte *)pstr->data, pstr->size, &ID);
            }
            if (code < 0)
                return -1; /* No name, trigger pdfwrite Type 3 fallback */

            code = (*ctx->get_glyph_name)(pfont, (gs_glyph)ID, pstr);
            if (code < 0)
                return -1; /* No name, trigger pdfwrite Type 3 fallback */
        }
        else {
            code = (*ctx->get_glyph_name)(pfont, glyph, pstr);
            if (code < 0)
                return -1; /* No name, trigger pdfwrite Type 3 fallback */
        }
    }
    return code;

}

static void pdfi_make_post_dict(gs_font_type42 *pfont)
{
    pdf_font_truetype *ttfont = (pdf_font_truetype *)pfont->client_data;
    pdf_context *ctx = (pdf_context *)ttfont->ctx;
    int i, code = 0;
    if (ttfont->post == NULL && pfont->data.numGlyphs > 0) {
        code = pdfi_dict_alloc(ctx, pfont->data.numGlyphs, &ttfont->post);
        if (code < 0)
            return;

        pdfi_countup(ttfont->post);

        for (i = 0; i < pfont->data.numGlyphs; i++) {
            gs_string postname = {0};
            pdf_name *key;
            pdf_num *ind;

            code = gs_type42_find_post_name(pfont, (gs_glyph)i, &postname);
            if (code < 0) {
                continue;
            }
            code = pdfi_name_alloc(ctx, postname.data, postname.size, (pdf_obj **)&key);
            if (code < 0) {
               continue;
            }
            pdfi_countup(key);
            code = pdfi_object_alloc(ctx, PDF_INT, 0, (pdf_obj **)&ind);
            if (code < 0) {
               pdfi_countdown(key);
               continue;
            }
            pdfi_countup(ind);
            ind->value.i = i;
            (void)pdfi_dict_put_obj(ctx, ttfont->post, (pdf_obj *)key, (pdf_obj *)ind, true);
            pdfi_countdown(key);
            pdfi_countdown(ind);
        }
    }
}


static int
pdfi_alloc_tt_font(pdf_context *ctx, pdf_font_truetype **font, bool is_cid)
{
    pdf_font_truetype *ttfont = NULL;
    gs_font_type42 *pfont = NULL;

    ttfont = (pdf_font_truetype *)gs_alloc_bytes(ctx->memory, sizeof(pdf_font_truetype), "pdfi (truetype pdf_font)");
    if (ttfont == NULL)
        return_error(gs_error_VMerror);

    memset(ttfont, 0x00, sizeof(pdf_font_truetype));
    ttfont->type = PDF_FONT;
    ttfont->ctx = ctx;
    ttfont->pdfi_font_type = e_pdf_font_truetype;

#if REFCNT_DEBUG
    ttfont->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %"PRIi64"\n", ttfont->type, ttfont->UID);
#endif

    pdfi_countup(ttfont);

    pfont = (gs_font_type42 *)gs_alloc_struct(ctx->memory, gs_font_type42, &st_gs_font_type42,
                            "pdfi (truetype pfont)");
    if (pfont == NULL) {
        pdfi_countdown(ttfont);
        return_error(gs_error_VMerror);
    }
    memset(pfont, 0x00, sizeof(gs_font_type42));

    ttfont->pfont = (gs_font_base *)pfont;

    gs_make_identity(&pfont->orig_FontMatrix);
    gs_make_identity(&pfont->FontMatrix);
    pfont->next = pfont->prev = 0;
    pfont->memory = ctx->memory;
    pfont->dir = ctx->font_dir;
    pfont->is_resource = false;
    gs_notify_init(&pfont->notify_list, ctx->memory);
    pfont->base = (gs_font *) ttfont->pfont;
    pfont->client_data = ttfont;
    pfont->WMode = 0;
    pfont->PaintType = 0;
    pfont->StrokeWidth = 0;
    pfont->is_cached = 0;
    pfont->FAPI = NULL;
    pfont->FAPI_font_data = NULL;
    pfont->procs.init_fstack = gs_default_init_fstack;
    pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pfont->FontType = ft_TrueType;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    /* We may want to do something clever with an XUID here */
    pfont->id = gs_next_ids(ctx->memory, 1);
    uid_set_UniqueID(&pfont->UID, pfont->id);
    /* The buildchar proc will be filled in by FAPI -
       we won't worry about working without FAPI */
    pfont->procs.encode_char = pdfi_ttf_encode_char;
    pfont->data.string_proc = pdfi_ttf_string_proc;
    pfont->procs.glyph_name = pdfi_ttf_glyph_name;
    pfont->procs.decode_glyph = pdfi_decode_glyph;
    pfont->procs.define_font = gs_no_define_font;
    pfont->procs.make_font = gs_no_make_font;

    ttfont->default_font_info = gs_default_font_info;
    pfont->procs.font_info = pdfi_default_font_info;

    pfont->procs.glyph_info = gs_default_glyph_info;
    pfont->procs.glyph_outline = gs_no_glyph_outline;
    pfont->procs.build_char = NULL;
    pfont->procs.same_font = gs_default_same_font;
    pfont->procs.enumerate_glyph = gs_no_enumerate_glyph;

    pfont->encoding_index = ENCODING_INDEX_UNKNOWN;
    pfont->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

    pfont->client_data = (void *)ttfont;

    *font = ttfont;
    return 0;
}

static void pdfi_set_type42_custom_procs(pdf_font_truetype *pdfttfont)
{
    gs_font_type42 *pfont = (gs_font_type42 *)pdfttfont->pfont;

    pdfttfont->default_font_info = pfont->procs.font_info;
    pfont->procs.font_info = pdfi_default_font_info;
    pfont->data.get_glyph_index = pdfi_type42_get_glyph_index;
    pfont->procs.enumerate_glyph = pdfi_ttf_enumerate_glyph;
}

int pdfi_read_truetype_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, byte *buf, int64_t buflen, int findex, pdf_font **ppdffont)
{
    pdf_font_truetype *font = NULL;
    int code = 0, i;
    pdf_obj *fontdesc = NULL;
    pdf_obj *obj = NULL;
    pdf_obj *basefont = NULL;
    int64_t descflags;
    bool encoding_known = false;
    bool forced_symbolic = false;
    pdf_obj *tounicode = NULL;

    if (ppdffont == NULL)
        return_error(gs_error_invalidaccess);

    *ppdffont = NULL;

    if (font_dict != NULL)
        (void)pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, &fontdesc);

    if ((code = pdfi_alloc_tt_font(ctx, &font, false)) < 0) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }
    if (font_dict != NULL) {
        font->object_num = font_dict->object_num;
        font->generation_num = font_dict->generation_num;
        font->indirect_num = font_dict->indirect_num;
        font->indirect_gen = font_dict->indirect_gen;
    }

    font->FontDescriptor = (pdf_dict *)fontdesc;
    fontdesc = NULL;

    pdfi_font_set_first_last_char(ctx, font_dict, (pdf_font *)font);

    code = pdfi_object_alloc(ctx, PDF_BUFFER, 0, (pdf_obj **)&font->sfnt);
    if (code < 0) {
        goto error;
    }
    pdfi_countup(font->sfnt);
    code = pdfi_buffer_set_data((pdf_obj *)font->sfnt, buf, buflen);
    if (code < 0) {
        goto error;
    }
    buf = NULL;

    /* Strictly speaking BaseFont is required, but we can continue without one */
    if (font_dict != NULL) {
        code = pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, (pdf_obj **)&basefont);
        if (code > 0) {
            pdf_name *nobj = (pdf_name *)basefont;
            int nlen = nobj->length > gs_font_name_max ? gs_font_name_max : nobj->length;

            memcpy(font->pfont->key_name.chars, nobj->data, nlen);
            font->pfont->key_name.chars[nlen] = 0;
            font->pfont->key_name.size = nlen;
            memcpy(font->pfont->font_name.chars, nobj->data, nlen);
            font->pfont->font_name.chars[nlen] = 0;
            font->pfont->font_name.size = nlen;
            pdfi_countdown(obj);
            obj = NULL;
        }
    }
    font->BaseFont = basefont;
    basefont = NULL;
    font->PDF_font = font_dict;
    pdfi_countup(font_dict);

    /* ignore errors with widths... for now */
    if (font_dict != NULL)
        (void)pdfi_font_create_widths(ctx, font_dict, (pdf_font*)font, 0.001);

    if (ctx->args.ignoretounicode != true && font_dict != NULL) {
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
    font->ToUnicode = tounicode;
    tounicode = NULL;

    if (font->FontDescriptor != NULL) {
        code = pdfi_dict_get_int(ctx, font->FontDescriptor, "Flags", &descflags);
        if (code < 0)
            descflags = 0;
    }
    else {
        descflags = 0;
    }

    if (font_dict != NULL)
        code = pdfi_dict_get(ctx, font_dict, "Encoding", &obj);
    else
        code = gs_error_undefined;

    if (code < 0) {
        static const char encstr[] = "WinAnsiEncoding";
        code = pdfi_name_alloc(ctx, (byte *)encstr, strlen(encstr), (pdf_obj **)&obj);
        if (code >= 0)
            pdfi_countup(obj);
        else
            goto error;
    }
    else {
        encoding_known = true;
        /* If we have and encoding, and both the symbolic and non-symbolic flag are set,
           believe that latter.
         */
        if ((descflags & 32) != 0)
            descflags = (descflags & ~4);
    }

    if ((forced_symbolic = pdfi_font_known_symbolic(font->BaseFont)) == true) {
        descflags |= 4;
    }

    code = pdfi_create_Encoding(ctx, (pdf_font *)font, obj, NULL, (pdf_obj **)&font->Encoding);
    /* If we get an error, and the font is non-symbolic, return the error */
    if (code < 0 && (descflags & 4) == 0)
        goto error;
    /* If we get an error, and the font is symbolic, pretend we never saw an /Encoding */
    if (code < 0)
        encoding_known = false;
    pdfi_countdown(obj);
    obj = NULL;

    code = gs_type42_font_init((gs_font_type42 *)font->pfont, 0);
    if (code < 0) {
        goto error;
    }

    pdfi_set_type42_custom_procs(font);

    /* We're probably dead in the water without cmap tables, but make sure, for safety */
    /* This is a horrendous morass of guesses at what Acrobat does with files that contravene
       what the spec says about symbolic fonts, cmap tables and encodings.
     */
    if (forced_symbolic != true && (descflags & 4) != 0 && ((gs_font_type42 *)font->pfont)->data.cmap != 0) {
        gs_font_type42 *t42f = (gs_font_type42 *)font->pfont;
        int numcmaps;
        int cmaps_available = CMAP_TABLE_NONE;
        const byte *d;
        code = (*t42f->data.string_proc)(t42f, t42f->data.cmap + 2, 2, &d);
        if (code < 0)
            goto error;
        numcmaps = d[1] | d[0] << 8;
        for (i = 0; i < numcmaps; i++) {
            code = (*t42f->data.string_proc)(t42f, t42f->data.cmap + 4 + i * 8, 4, &d);
            if (code < 0)
                goto error;
#define CMAP_PLAT_ENC_ID(a,b,c,d) (a << 24 | b << 16 | c << 8 | d)
            switch(CMAP_PLAT_ENC_ID(d[0], d[1], d[2], d[3])) {
                case CMAP_PLAT_ENC_ID(0, 1, 0, 0):
                    cmaps_available |= CMAP_TABLE_10_PRESENT;
                    break;
                case CMAP_PLAT_ENC_ID(0, 3, 0, 0):
                    cmaps_available |= CMAP_TABLE_30_PRESENT;
                    break;
                case CMAP_PLAT_ENC_ID(0, 3, 0, 1):
                    cmaps_available |= CMAP_TABLE_31_PRESENT;
                    break;
                case CMAP_PLAT_ENC_ID(0, 3, 1, 0):
                    cmaps_available |= CMAP_TABLE_310_PRESENT;
                    break;
                default: /* Not one we're interested in */
                    break;
            }
        }
#undef CMAP_PLAT_ENC_ID
        if ((cmaps_available & CMAP_TABLE_30_PRESENT) == CMAP_TABLE_30_PRESENT) {
            font->descflags = descflags;
        }
        else if (encoding_known == true) {
            static const char mrencstr[] = "MacRomanEncoding";
            static const char waencstr[] = "WinAnsiEncoding";
            const char *encstr = ((cmaps_available & CMAP_TABLE_31_PRESENT) == CMAP_TABLE_31_PRESENT) ? waencstr : mrencstr;
            font->descflags = descflags & ~4;
            code = pdfi_name_alloc(ctx, (byte *)encstr, strlen(encstr), (pdf_obj **)&obj);
            if (code >= 0)
                pdfi_countup(obj);
            else
                goto error;
            pdfi_countdown(font->Encoding);
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, obj, NULL, (pdf_obj **)&font->Encoding);
            if (code < 0)
                goto error;
            pdfi_countdown(obj);
            obj = NULL;
        }
        else
            font->descflags = descflags;
    }
    else {
        font->descflags = descflags;
    }

    if (uid_is_XUID(&font->pfont->UID))
        uid_free(&font->pfont->UID, font->pfont->memory, "pdfi_read_type1_font");
    uid_set_invalid(&font->pfont->UID);

    code = pdfi_font_generate_pseudo_XUID(ctx, font_dict, font->pfont);
    if (code < 0) {
        goto error;
    }

    if ((font->descflags & 4) == 0) {
        /* Horrid hacky solution */
        /* We don't want to draw the TTF notdef */
        gs_font_type42 *gst42 = ((gs_font_type42 *)font->pfont);
        if (gst42->data.len_glyphs != NULL && gst42->data.len_glyphs[0] > 10) {
            gst42->data.len_glyphs[0] = 0;
        }
    }

    pdfi_make_post_dict((gs_font_type42 *)font->pfont);

    code = gs_definefont(ctx->font_dir, (gs_font *)font->pfont);
    if (code < 0) {
        goto error;
    }

    code = pdfi_fapi_passfont((pdf_font *)font, 0, NULL, NULL, font->sfnt->data, font->sfnt->length);
    if (code < 0) {
        goto error;
    }

    /* object_num can be zero if the dictionary was defined inline */
    if (font->object_num != 0) {
        (void)replace_cache_entry(ctx, (pdf_obj *)font);
    }

    *ppdffont = (pdf_font *)font;
    return code;
error:
    pdfi_countdown(obj);
    obj = NULL;
    if (font_dict != NULL) {
        if (pdfi_dict_get(ctx, font_dict, ".Path", &obj) >= 0)
        {
            char fname[gp_file_name_sizeof + 1];
            pdf_string *fobj = (pdf_string *)obj;

            memcpy(fname, fobj->data, fobj->length > gp_file_name_sizeof ? gp_file_name_sizeof : fobj->length);
            fname[fobj->length > gp_file_name_sizeof ? gp_file_name_sizeof : fobj->length] = '\0';

            pdfi_set_error_var(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_truetype_font", "Error reading TrueType font file %s\n", fname);
        }
        else {
            pdfi_set_error_var(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_truetype_font", "Error reading embedded TrueType font object %u\n", font_dict->object_num);
        }
    }
    else {
        pdfi_set_error(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_truetype_font", "Error reading font\n");
    }
    if (buf != NULL)
        gs_free_object(ctx->memory, buf, "pdfi_read_truetype_font(buf)");
    pdfi_countdown(fontdesc);
    pdfi_countdown(basefont);
    pdfi_countdown(font);
    return code;
}

int
pdfi_copy_truetype_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont)
{
    int code = 0;
    pdf_font_truetype *font = NULL;
    gs_font_type42 *spfont1 = (gs_font_type42 *) spdffont->pfont;
    gs_font_type42 *dpfont42;
    gs_id t_id;
    pdf_obj *tmp;

    if (font_dict == NULL)
        return_error(gs_error_invalidfont);

    code = pdfi_alloc_tt_font(ctx, &font, font_dict->object_num);
    if (code < 0)
        return code;
    dpfont42 = (gs_font_type42 *) font->pfont;

    t_id = dpfont42->id;
    memcpy(dpfont42, spfont1, sizeof(gs_font_type42));
    dpfont42->id = t_id;
    dpfont42->FAPI = NULL;
    dpfont42->FAPI_font_data = NULL;
    dpfont42->notify_list.memory = NULL;
    dpfont42->notify_list.first = NULL;
    gs_notify_init(&dpfont42->notify_list, dpfont42->memory);

    memcpy(font, spdffont, sizeof(pdf_font_truetype));
    font->refcnt = 1;
    font->filename = NULL;
    pdfi_countup(font->post);

    if (dpfont42->data.len_glyphs != NULL) {
        dpfont42->data.len_glyphs = (uint *)gs_alloc_byte_array(dpfont42->memory, dpfont42->data.numGlyphs + 1, sizeof(uint), "pdfi_copy_truetype_font");
        if (dpfont42->data.len_glyphs == NULL) {
            pdfi_countdown(font);
            return_error(gs_error_VMerror);
        }
        code = gs_font_notify_register((gs_font *)dpfont42, gs_len_glyphs_release, (void *)dpfont42);
        if (code < 0) {
            gs_free_object(dpfont42->memory, dpfont42->data.len_glyphs, "gs_len_glyphs_release");
            dpfont42->data.len_glyphs = NULL;
            pdfi_countdown(font);
            return code;
        }
        memcpy(dpfont42->data.len_glyphs, spfont1->data.len_glyphs, (dpfont42->data.numGlyphs + 1) * sizeof(uint));
    }
    if (dpfont42->data.gsub != NULL) {
        dpfont42->data.gsub_size = spfont1->data.gsub_size;

        dpfont42->data.gsub = gs_alloc_byte_array(dpfont42->memory, dpfont42->data.gsub_size, 1, "pdfi_copy_truetype_font");
        if (dpfont42->data.gsub == 0) {
            pdfi_countdown(font);
            return_error(gs_error_VMerror);
        }

        code = gs_font_notify_register((gs_font *)dpfont42, gs_gsub_release, (void *)dpfont42);
        if (code < 0) {
            gs_free_object(dpfont42->memory, dpfont42->data.gsub, "gs_len_glyphs_release");
            dpfont42->data.gsub = NULL;
            pdfi_countdown(font);
            return code;
        }
        memcpy(dpfont42->data.gsub, spfont1->data.gsub, dpfont42->data.gsub_size);
    }

    font->pfont = (gs_font_base *)dpfont42;
    dpfont42->client_data = (void *)font;

    font->PDF_font = font_dict;
    font->object_num = font_dict->object_num;
    font->generation_num = font_dict->generation_num;
    pdfi_countup(font->PDF_font);

    /* We want basefont and descriptor, but we can live without them */
    font->BaseFont = NULL;
    (void)pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &font->BaseFont);
    font->FontDescriptor = NULL;
    (void)pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, (pdf_obj **)&font->FontDescriptor);

    pdfi_countup(font->sfnt);

    if (font->BaseFont != NULL && ((pdf_name *)font->BaseFont)->length <= gs_font_name_max) {
        memcpy(dpfont42->key_name.chars, ((pdf_name *)font->BaseFont)->data, ((pdf_name *)font->BaseFont)->length);
        dpfont42->key_name.size = ((pdf_name *)font->BaseFont)->length;
        memcpy(dpfont42->font_name.chars, ((pdf_name *)font->BaseFont)->data, ((pdf_name *)font->BaseFont)->length);
        dpfont42->font_name.size = ((pdf_name *)font->BaseFont)->length;
    }

    font->Encoding = NULL;
    font->ToUnicode = NULL;
    font->Widths = NULL;

    pdfi_font_set_first_last_char(ctx, font_dict, (pdf_font *)font);
    (void)pdfi_font_create_widths(ctx, font_dict, (pdf_font*)font, (double)0.001);

    font->descflags = 0;
    if (font->FontDescriptor != NULL) {
        code = pdfi_dict_get_int(ctx, font->FontDescriptor, "Flags", &font->descflags);
        if (code >= 0) {
            /* If both the symbolic and non-symbolic flag are set,
               believe that latter.
             */
            if ((font->descflags & 32) != 0)
                font->descflags = (font->descflags & ~4);
        }
    }

    tmp = NULL;
    code = pdfi_dict_knownget(ctx, font_dict, "Encoding", &tmp);
    if (code == 1) {
        if ((pdfi_type_of(tmp) == PDF_NAME || pdfi_type_of(tmp) == PDF_DICT) && (font->descflags & 4) == 0) {
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, tmp, NULL, (pdf_obj **) & font->Encoding);
        }
        else if (pdfi_type_of(tmp) == PDF_DICT && (font->descflags & 4) != 0) {
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, tmp, (pdf_obj *)spdffont->Encoding, (pdf_obj **) &font->Encoding);
        }
        else {
            code = gs_error_undefined;
        }
        pdfi_countdown(tmp);
        tmp = NULL;
    }
    else {
        pdfi_countdown(tmp);
        tmp = NULL;
        code = gs_error_undefined;
    }

    if (code < 0) {
        font->Encoding = spdffont->Encoding;
        pdfi_countup(font->Encoding);
    }

    /* Since various aspects of the font may differ (widths, encoding, etc)
       we cannot reliably use the UniqueID/XUID for copied fonts.
     */
    uid_set_invalid(&font->pfont->UID);

    if (ctx->args.ignoretounicode != true) {
        code = pdfi_dict_get(ctx, font_dict, "ToUnicode", (pdf_obj **)&tmp);
        if (code >= 0 && pdfi_type_of(tmp) == PDF_STREAM) {
            pdf_cmap *tu = NULL;
            code = pdfi_read_cmap(ctx, tmp, &tu);
            pdfi_countdown(tmp);
            tmp = (pdf_obj *)tu;
        }
        if (code < 0 || (tmp != NULL && pdfi_type_of(tmp) != PDF_CMAP)) {
            pdfi_countdown(tmp);
            tmp = NULL;
            code = 0;
        }
    }
    else {
        tmp = NULL;
    }
    font->ToUnicode = tmp;
    code = gs_definefont(ctx->font_dir, (gs_font *) font->pfont);
    if (code < 0) {
        goto error;
    }

    code = pdfi_fapi_passfont((pdf_font *) font, 0, NULL, NULL, NULL, 0);
    if (code < 0) {
        goto error;
    }
    /* object_num can be zero if the dictionary was defined inline */
    if (font->object_num != 0) {
        (void)replace_cache_entry(ctx, (pdf_obj *) font);
    }

    *tpdffont = (pdf_font *)font;

error:
    if (code < 0)
        pdfi_countdown(font);
    return code;
}

int pdfi_free_font_truetype(pdf_obj *font)
{
    pdf_font_truetype *ttfont = (pdf_font_truetype *)font;

    if (ttfont->pfont)
        gs_free_object(OBJ_MEMORY(ttfont), ttfont->pfont, "Free TrueType gs_font");

    if (ttfont->Widths)
        gs_free_object(OBJ_MEMORY(ttfont), ttfont->Widths, "Free TrueType font Widths array");

    pdfi_countdown(ttfont->sfnt);
    pdfi_countdown(ttfont->FontDescriptor);
    pdfi_countdown(ttfont->Encoding);
    pdfi_countdown(ttfont->BaseFont);
    pdfi_countdown(ttfont->PDF_font);
    pdfi_countdown(ttfont->ToUnicode);
    pdfi_countdown(ttfont->filename);
    pdfi_countdown(ttfont->post);

    gs_free_object(OBJ_MEMORY(ttfont), ttfont, "Free TrueType font");

    return 0;
}
