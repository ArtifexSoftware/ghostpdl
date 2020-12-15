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

/* code for type 1 font handling */
#include "pdf_int.h"

#include "strmio.h"
#include "stream.h"
#include "gsgdata.h"
#include "gstype1.h"
#include "gscencs.h"

#include "strimpl.h"
#include "stream.h"
#include "sfilter.h"

#include "pdf_deref.h"
#include "pdf_types.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_file.h"
#include "pdf_font_types.h"
#include "pdf_font.h"
#include "pdf_fmap.h"
#include "pdf_font1.h"
#include "pdf_font1C.h"
#include "pdf_fontps.h"

/* CALLBACKS */
static int
pdfi_t1_glyph_data(gs_font_type1 * pfont, gs_glyph glyph, gs_glyph_data_t * pgd)
{
    int code = 0;
    pdf_font_type1 *pdffont1 = (pdf_font_type1 *) pfont->client_data;
    pdf_name *glyphname = NULL;
    pdf_string *charstring = NULL;

    if (glyph >= gs_c_min_std_encoding_glyph) {
        gs_const_string str;

        code = gs_c_glyph_name(glyph, &str);
        if (code >= 0) {
            code = pdfi_name_alloc(pdffont1->ctx, (byte *) str.data, str.size, (pdf_obj **) & glyphname);
            if (code >= 0)
                pdfi_countup(glyphname);
        }
    }
    else {
        code = pdfi_array_get(pdffont1->ctx, pdffont1->Encoding, (uint64_t) glyph, (pdf_obj **) & glyphname);
    }
    if (code >= 0) {
        code = pdfi_dict_get_by_key(pdffont1->ctx, pdffont1->CharStrings, glyphname, (pdf_obj **) & charstring);
        if (code >= 0)
            gs_glyph_data_from_bytes(pgd, charstring->data, 0, charstring->length, NULL);
    }
    pdfi_countdown(charstring);
    pdfi_countdown(glyphname);

    return code;
}

static int
pdfi_t1_subr_data(gs_font_type1 * pfont, int index, bool global, gs_glyph_data_t * pgd)
{
    int code = 0;
    pdf_font_type1 *pdffont1 = (pdf_font_type1 *) pfont->client_data;

    if (global ||index >= pdffont1->NumSubrs) {
        code = gs_note_error(gs_error_rangecheck);
    }
    else {
        gs_glyph_data_from_bytes(pgd, pdffont1->Subrs[index].data, 0, pdffont1->Subrs[index].size, NULL);
    }
    return code;
}

static int
pdfi_t1_seac_data(gs_font_type1 * pfont, int ccode, gs_glyph * pglyph,
                  gs_const_string * gstr, gs_glyph_data_t * pgd)
{
    return_error(gs_error_rangecheck);
}

/* push/pop are null ops here */
static int
pdfi_t1_push(void *callback_data, const fixed * pf, int count)
{
    (void)callback_data;
    (void)pf;
    (void)count;
    return 0;
}
static int
pdfi_t1_pop(void *callback_data, fixed * pf)
{
    (void)callback_data;
    (void)pf;
    return 0;
}

static int
pdfi_t1_enumerate_glyph(gs_font * pfont, int *pindex,
                        gs_glyph_space_t glyph_space, gs_glyph * pglyph)
{
    int code;
    pdf_font_type1 *t1font = (pdf_font_type1 *) pfont->client_data;
    pdf_context *ctx = (pdf_context *) t1font->ctx;
    pdf_name *key;
    uint64_t i = (uint64_t) * pindex;

    (void)glyph_space;

    if (*pindex <= 0)
        code = pdfi_dict_key_first(ctx, t1font->CharStrings, (pdf_obj **) & key, &i);
    else
        code = pdfi_dict_key_next(ctx, t1font->CharStrings, (pdf_obj **) & key, &i);
    if (code < 0) {
        *pindex = 0;
        code = gs_note_error(gs_error_undefined);
    }
    else {
        uint dummy = GS_NO_GLYPH;

        code = ctx->get_glyph_index(pfont, key->data, key->length, &dummy);
        if (code < 0) {
            *pglyph = (gs_glyph) * pindex;
            goto exit;
        }
        *pglyph = dummy;
        if (*pglyph == GS_NO_GLYPH)
            *pglyph = (gs_glyph) * pindex;
        *pindex = (int)i + 1;
    }
  exit:
    pdfi_countdown(key);
    return code;
}

/* This *should* only get called for SEAC lookups, which have to come from StandardEncoding
   so just try to lookup the string in the standard encodings
 */
int
pdfi_t1_global_glyph_code(const gs_font * pfont, gs_const_string * gstr, gs_glyph * pglyph)
{
    *pglyph = gs_c_name_glyph(gstr->data, gstr->size);
    return 0;
}

static int
pdfi_t1_glyph_outline(gs_font * pfont, int WMode, gs_glyph glyph,
                      const gs_matrix * pmat, gx_path * ppath, double sbw[4])
{
    gs_glyph_data_t gd;
    gs_glyph_data_t *pgd = &gd;
    gs_font_type1 *pfont1 = (gs_font_type1 *) pfont;
    int code = pdfi_t1_glyph_data(pfont1, glyph, pgd);

    if (code >= 0) {
        gs_type1_state cis = { 0 };
        gs_type1_state *pcis = &cis;
        gs_gstate gs;
        int value;

        if (pmat)
            gs_matrix_fixed_from_matrix(&gs.ctm, pmat);
        else {
            gs_matrix imat;

            gs_make_identity(&imat);
            gs_matrix_fixed_from_matrix(&gs.ctm, &imat);
        }
        gs.flatness = 0;
        code = gs_type1_interp_init(pcis, &gs, ppath, NULL, NULL, true, 0, pfont1);
        if (code < 0)
            return code;

        pcis->no_grid_fitting = true;
        gs_type1_set_callback_data(pcis, NULL);
        /* Continue interpreting. */
      icont:
        code = pfont1->data.interpret(pcis, pgd, &value);
        switch (code) {
            case 0:            /* all done */
                /* falls through */
            default:           /* code < 0, error */
                return code;
            case type1_result_callothersubr:   /* unknown OtherSubr */
                return_error(gs_error_rangecheck);      /* can't handle it */
            case type1_result_sbw:     /* [h]sbw, just continue */
                type1_cis_get_metrics(pcis, sbw);
                pgd = 0;
                goto icont;
        }
    }
    return code;
}

/* END CALLBACKS */

static stream *
push_pfb_filter(gs_memory_t * mem, byte * buf, byte * bufend)
{
    stream *fs, *ffs = NULL;
    stream *sstrm;
    stream_PFBD_state *st;
    byte *strbuf;

    sstrm = file_alloc_stream(mem, "push_pfb_filter(buf stream)");
    if (sstrm == NULL)
        return NULL;

    sread_string(sstrm, buf, bufend - buf);
    sstrm->close_at_eod = false;

    fs = s_alloc(mem, "push_pfb_filter(fs)");
    strbuf = gs_alloc_bytes(mem, 4096, "push_pfb_filter(buf)");
    st = gs_alloc_struct(mem, stream_PFBD_state, s_PFBD_template.stype, "push_pfb_filter(st)");
    if (fs == NULL || st == NULL || strbuf == NULL) {
        sclose(sstrm);
        gs_free_object(mem, sstrm, "push_pfb_filter(buf stream)");
        gs_free_object(mem, fs, "push_pfb_filter(fs)");
        gs_free_object(mem, st, "push_pfb_filter(st)");
        goto done;
    }
    memset(st, 0x00, sizeof(stream_PFBD_state));

    s_std_init(fs, strbuf, 4096, &s_filter_read_procs, s_mode_read);
    st->memory = mem;
    st->templat = &s_PFBD_template;
    fs->state = (stream_state *) st;
    fs->procs.process = s_PFBD_template.process;
    fs->strm = sstrm;
    fs->close_at_eod = false;
    ffs = fs;
  done:
    return ffs;
}

static void
pop_pfb_filter(gs_memory_t * mem, stream * s)
{
    stream *src = s->strm;
    byte *b = s->cbuf;

    sclose(s);
    gs_free_object(mem, s, "push_pfb_filter(s)");
    gs_free_object(mem, b, "push_pfb_filter(b)");
    if (src)
        sclose(src);
    gs_free_object(mem, src, "push_pfb_filter(strm)");
}

static int
pdfi_t1_decode_pfb(pdf_context * ctx, byte * inbuf, int inlen, byte ** outbuf, int *outlen)
{
    stream *strm;
    int c, code = 0;
    int decodelen = 0;
    byte *d, *decodebuf = NULL;

    *outbuf = NULL;
    *outlen = 0;

    strm = push_pfb_filter(ctx->memory, inbuf, inbuf + inlen + 1);
    if (strm == NULL) {
        code = gs_note_error(gs_error_VMerror);
    }
    else {
        while (1) {
            c = sgetc(strm);
            if (c < 0)
                break;
            decodelen++;
        }
        pop_pfb_filter(ctx->memory, strm);
        decodebuf = gs_alloc_bytes(ctx->memory, decodelen, "pdfi_t1_decode_pfb(decodebuf)");
        if (decodebuf == NULL) {
            code = gs_note_error(gs_error_VMerror);
        }
        else {
            d = decodebuf;
            strm = push_pfb_filter(ctx->memory, inbuf, inbuf + inlen + 1);
            while (1) {
                c = sgetc(strm);
                if (c < 0)
                    break;
                *d = c;
                d++;
            }
            pop_pfb_filter(ctx->memory, strm);
            *outbuf = decodebuf;
            *outlen = decodelen;
        }
    }
    return code;
}

static int
pdfi_open_t1_font_file(pdf_context * ctx, char *fontfile, byte ** buf, int64_t * buflen)
{
    int code = 0;

    /* FIXME: romfs hardcoded coded for now */
    stream *s;

    s = sfopen(fontfile, "r", ctx->memory);
    if (s == NULL) {
        code = gs_note_error(gs_error_undefinedfilename);
    }
    else {
        sfseek(s, 0, SEEK_END);
        *buflen = sftell(s);
        sfseek(s, 0, SEEK_SET);
        *buf = gs_alloc_bytes(ctx->memory, *buflen, "pdfi_open_t1_font_file(buf)");
        if (*buf != NULL) {
            sfread(*buf, 1, *buflen, s);
        }
        else {
            code = gs_note_error(gs_error_VMerror);
        }
        sfclose(s);
    }
    return code;
}

static int
pdfi_alloc_t1_font(pdf_context * ctx, pdf_font_type1 ** font, uint32_t obj_num)
{
    pdf_font_type1 *t1font = NULL;
    gs_font_type1 *pfont = NULL;

    t1font = (pdf_font_type1 *) gs_alloc_bytes(ctx->memory, sizeof(pdf_font_type1), "pdfi (type 1 pdf_font)");
    if (t1font == NULL)
        return_error(gs_error_VMerror);

    memset(t1font, 0x00, sizeof(pdf_font_type1));
    t1font->ctx = ctx;
    t1font->type = PDF_FONT;
    t1font->ctx = ctx;
    t1font->pdfi_font_type = e_pdf_font_type1;

#if REFCNT_DEBUG
    t1font->UID = ctx->UID++;
    dmprintf2(ctx->memory,
              "Allocated object of type %c with UID %" PRIi64 "\n", t1font->type, t1font->UID);
#endif

    pdfi_countup(t1font);

    pfont = (gs_font_type1 *) gs_alloc_struct(ctx->memory, gs_font_type1, &st_gs_font_type1, "pdfi (Type 1 pfont)");
    if (pfont == NULL) {
        pdfi_countdown(t1font);
        return_error(gs_error_VMerror);
    }
    memset(pfont, 0x00, sizeof(gs_font_type1));

    t1font->pfont = (gs_font_base *) pfont;

    gs_make_identity(&pfont->orig_FontMatrix);
    gs_make_identity(&pfont->FontMatrix);
    pfont->next = pfont->prev = 0;
    pfont->memory = ctx->memory;
    pfont->dir = ctx->font_dir;
    pfont->is_resource = false;
    gs_notify_init(&pfont->notify_list, ctx->memory);
    pfont->base = (gs_font *) t1font->pfont;
    pfont->client_data = t1font;
    pfont->WMode = 0;
    pfont->PaintType = 0;
    pfont->StrokeWidth = 0;
    pfont->is_cached = 0;
    pfont->FAPI = NULL;
    pfont->FAPI_font_data = NULL;
    pfont->procs.init_fstack = gs_default_init_fstack;
    pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pfont->FontType = ft_encrypted;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    /* We may want to do something clever with an XUID here */
    pfont->id = gs_next_ids(ctx->memory, 1);
    uid_set_UniqueID(&pfont->UID, pfont->id);

    pfont->encoding_index = 1;          /****** WRONG ******/
    pfont->nearest_encoding_index = 1;          /****** WRONG ******/

    pfont->client_data = (void *)t1font;

    *font = t1font;
    return 0;
}
static void
pdfi_t1_font_set_procs(pdf_context * ctx, pdf_font_type1 * font)
{
    gs_font_type1 *pfont = (gs_font_type1 *) font->pfont;

    /* The build_char proc will be filled in by FAPI -
       we won't worry about working without FAPI */
    pfont->procs.build_char = NULL;

    pfont->procs.encode_char = pdfi_encode_char;
    pfont->procs.glyph_name = ctx->get_glyph_name;
    pfont->procs.decode_glyph = pdfi_decode_glyph;
    pfont->procs.define_font = gs_no_define_font;
    pfont->procs.make_font = gs_no_make_font;
    pfont->procs.font_info = gs_default_font_info;
    pfont->procs.glyph_info = gs_default_glyph_info;
    pfont->procs.glyph_outline = pdfi_t1_glyph_outline;
    pfont->procs.same_font = gs_default_same_font;
    pfont->procs.enumerate_glyph = pdfi_t1_enumerate_glyph;

    pfont->data.procs.glyph_data = pdfi_t1_glyph_data;
    pfont->data.procs.subr_data = pdfi_t1_subr_data;
    pfont->data.procs.seac_data = pdfi_t1_seac_data;
    pfont->data.procs.push_values = pdfi_t1_push;
    pfont->data.procs.pop_value = pdfi_t1_pop;
    pfont->data.interpret = gs_type1_interpret;
}

int
pdfi_read_type1_font(pdf_context * ctx, pdf_dict * font_dict,
                     pdf_dict * stream_dict, pdf_dict * page_dict, gs_font ** ppfont)
{
    int code;
    byte *fbuf = NULL;
    int64_t fbuflen = 0;
    pdf_obj *fontdesc = NULL;
    pdf_obj *fontfile = NULL;
    pdf_obj *basefont = NULL;
    pdf_obj *mapname = NULL;
    pdf_obj *tmp = NULL;
    pdf_font_type1 *t1f = NULL;
    ps_font_interp_private fpriv = { 0 };

    code = pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, &fontdesc);

    if (fontdesc != NULL) {
        code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile", PDF_STREAM, &fontfile);

        if (code < 0)
            code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile2", PDF_STREAM, &fontfile);

        if (code < 0)
            code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile3", PDF_STREAM, &fontfile);
    }

    if (fontfile != NULL) {
        code = pdfi_stream_to_buffer(ctx, (pdf_stream *) fontfile, &fbuf, &fbuflen);
        pdfi_countdown(fontfile);
    }
    else {
        char fontfname[gp_file_name_sizeof];
        const char *romfsprefix = "%rom%Resource/Font/";
        const int romfsprefixlen = strlen(romfsprefix);

        code = pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &basefont);
        if (code < 0) {
            pdfi_countdown(fontdesc);
            return_error(gs_error_invalidfont);
        }
        code = pdf_fontmap_lookup_font(ctx, (pdf_name *) basefont, &mapname);
        if (code < 0) {
            mapname = basefont;
            pdfi_countup(mapname);
        }
        if (mapname->type == PDF_NAME) {
            pdf_name *mname = (pdf_name *) mapname;

            memcpy(fontfname, romfsprefix, romfsprefixlen);
            memcpy(fontfname + romfsprefixlen, mname->data, mname->length);
            fontfname[romfsprefixlen + mname->length] = '\0';
        }
        code = pdfi_open_t1_font_file(ctx, fontfname, &fbuf, &fbuflen);
        if (code < 0) {
            pdf_obj *defname = NULL;
            pdf_name *mname;

            pdfi_countdown(mapname);
            code = pdfi_name_alloc(ctx, (byte *) "Helvetica", 9, &defname);
            if (code < 0)
                goto error;

            pdfi_countup(defname);
            code = pdf_fontmap_lookup_font(ctx, (pdf_name *) defname, &mapname);
            pdfi_countdown(defname);
            if (code < 0)
                return code;    /* Done can't carry on! */
            mname = (pdf_name *) mapname;
            memcpy(fontfname, romfsprefix, romfsprefixlen);
            memcpy(fontfname + romfsprefixlen, mname->data, mname->length);
            fontfname[romfsprefixlen + mname->length] = '\0';
            code = pdfi_open_t1_font_file(ctx, fontfname, &fbuf, &fbuflen);
            if (code < 0)
                return code;    /* Done can't carry on! */
        }
    }

#define MAKEMAGIC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
    if (MAKEMAGIC(fbuf[0], fbuf[1], fbuf[2], 0) == MAKEMAGIC('%', '!', 'P', 0)) {
        /* pfa */
    }
    else if (MAKEMAGIC(fbuf[0], fbuf[1], fbuf[2], 0) == MAKEMAGIC(1, 0, 4, 0)
             || MAKEMAGIC(fbuf[0], fbuf[1], fbuf[2], fbuf[3]) == MAKEMAGIC('O', 'T', 'T', 'O')) {
        /* CFF/1C etc */
        pdfi_countdown(fontdesc);
        gs_free_object(ctx->memory, fbuf, "pdfi_read_type1_font");
        return pdfi_read_type1C_font(ctx, font_dict, stream_dict, page_dict, ppfont);
    }
    else if (MAKEMAGIC(fbuf[0], fbuf[1], 0, 0) == MAKEMAGIC(128, 1, 0, 0)) {
        byte *decodebuf = NULL;
        int decodelen;

        code = pdfi_t1_decode_pfb(ctx, fbuf, fbuflen, &decodebuf, &decodelen);
        gs_free_object(ctx->memory, fbuf, "pdfi_read_type1_font");
        if (code < 0) {
            gs_free_object(ctx->memory, decodebuf, "pdfi_read_type1_font");
        }
        fbuf = decodebuf;
        fbuflen = decodelen;
    }
#undef MAKEMAGIC

    if (code >= 0) {
        fpriv.gsu.gst1.data.lenIV = 4;
        code = pdfi_read_ps_font(ctx, font_dict, fbuf, fbuflen, &fpriv);

        gs_free_object(ctx->memory, fbuf, "pdfi_read_type1_font");
        code = pdfi_alloc_t1_font(ctx, &t1f, font_dict->object_num);
        if (code >= 0) {
            gs_font_type1 *pfont1 = (gs_font_type1 *) t1f->pfont;

            memcpy(&pfont1->data, &fpriv.gsu.gst1.data, sizeof(pfont1->data));

            pdfi_t1_font_set_procs(ctx, t1f);

            memcpy(&pfont1->FontMatrix, &fpriv.gsu.gst1.FontMatrix, sizeof(pfont1->FontMatrix));
            memcpy(&pfont1->orig_FontMatrix, &fpriv.gsu.gst1.orig_FontMatrix, sizeof(pfont1->orig_FontMatrix));
            memcpy(&pfont1->FontBBox, &fpriv.gsu.gst1.FontBBox, sizeof(pfont1->FontBBox));
            memcpy(&pfont1->key_name, &fpriv.gsu.gst1.key_name, sizeof(pfont1->key_name));
            memcpy(&pfont1->font_name, &fpriv.gsu.gst1.font_name, sizeof(pfont1->font_name));
            if (fpriv.gsu.gst1.UID.id != 0)
                memcpy(&pfont1->UID, &fpriv.gsu.gst1.UID, sizeof(pfont1->UID));
            pfont1->WMode = fpriv.gsu.gst1.WMode;
            pfont1->PaintType = fpriv.gsu.gst1.PaintType;
            pfont1->StrokeWidth = fpriv.gsu.gst1.StrokeWidth;

            t1f->object_num = font_dict->object_num;
            t1f->generation_num = font_dict->generation_num;
            t1f->PDF_font = font_dict;
            pdfi_countup(font_dict);
            t1f->BaseFont = basefont;
            pdfi_countup(basefont);
            t1f->FontDescriptor = (pdf_dict *) fontdesc;
            pdfi_countup(fontdesc);
            t1f->Name = mapname;
            pdfi_countup(mapname);

            code = pdfi_dict_knownget_type(ctx, font_dict, "FirstChar", PDF_INT, &tmp);
            if (code == 1) {
                t1f->FirstChar = ((pdf_num *) tmp)->value.i;
                pdfi_countdown(tmp);
                tmp = NULL;
            }
            else {
                t1f->FirstChar = 0;
            }
            code = pdfi_dict_knownget_type(ctx, font_dict, "LastChar", PDF_INT, &tmp);
            if (code == 1) {
                t1f->LastChar = ((pdf_num *) tmp)->value.i;
                pdfi_countdown(tmp);
                tmp = NULL;
            }
            else {
                t1f->LastChar = 255;
            }

            t1f->fake_glyph_names = (gs_string *) gs_alloc_bytes(ctx->memory, t1f->LastChar * sizeof(gs_string), "pdfi_read_type1_font: fake_glyph_names");
            if (!t1f->fake_glyph_names) {
                code = gs_note_error(gs_error_VMerror);
                goto error;
            }
            memset(t1f->fake_glyph_names, 0x00, t1f->LastChar * sizeof(gs_string));

            code = pdfi_dict_knownget_type(ctx, font_dict, "Widths", PDF_ARRAY, &tmp);
            if (code > 0) {
                int i;
                int num_chars = t1f->LastChar - t1f->FirstChar + 1;

                if (num_chars != pdfi_array_size((pdf_array *) tmp)) {
                    code = gs_note_error(gs_error_rangecheck);
                    goto error;
                }

                t1f->Widths = (double *)gs_alloc_bytes(ctx->memory, sizeof(double) * num_chars, "Type 1 font Widths array");
                if (t1f->Widths == NULL) {
                    code = gs_note_error(gs_error_VMerror);
                    goto error;
                }
                memset(t1f->Widths, 0x00, sizeof(double) * num_chars);
                for (i = 0; i < num_chars; i++) {
                    code = pdfi_array_get_number(ctx, (pdf_array *) tmp, (uint64_t) i, &t1f->Widths[i]);
                    if (code < 0)
                        goto error;
                }
            }
            pdfi_countdown(tmp);
            tmp = NULL;

            code = pdfi_dict_knownget(ctx, font_dict, "Encoding", &tmp);
            if (code == 1) {
                code = pdfi_create_Encoding(ctx, tmp, (pdf_obj **) & t1f->Encoding);
                if (code >= 0)
                    code = 1;
                pdfi_countdown(tmp);
                tmp = NULL;
            }

            if (code <= 0) {
                t1f->Encoding = fpriv.u.t1.Encoding;
                pdfi_countup(t1f->Encoding);
            }

            code = pdfi_dict_knownget(ctx, font_dict, "ToUnicode", &tmp);
            if (code == 1) {
                t1f->ToUnicode = tmp;
                tmp = NULL;
            }
            else {
                t1f->ToUnicode = NULL;
            }
            t1f->CharStrings = fpriv.u.t1.CharStrings;
            pdfi_countup(t1f->CharStrings);
            t1f->Subrs = fpriv.u.t1.Subrs;
            fpriv.u.t1.Subrs = NULL;
            t1f->NumSubrs = fpriv.u.t1.NumSubrs;

            code = gs_definefont(ctx->font_dir, (gs_font *) t1f->pfont);
            if (code < 0) {
                goto error;
            }

            code = pdfi_fapi_passfont((pdf_font *) t1f, 0, NULL, NULL, NULL, 0);
            if (code < 0) {
                goto error;
            }
            /* object_num can be zero if the dictionary was defined inline */
            if (t1f->object_num != 0) {
                code = replace_cache_entry(ctx, (pdf_obj *) t1f);
                if (code < 0)
                    goto error;
            }
            *ppfont = (gs_font *) t1f->pfont;
        }
    }

  error:
    pdfi_countdown(fontdesc);
    pdfi_countdown(basefont);
    pdfi_countdown(mapname);
    pdfi_countdown(tmp);
    pdfi_countdown(fpriv.u.t1.Encoding);
    pdfi_countdown(fpriv.u.t1.CharStrings);
    if (fpriv.u.t1.Subrs) {
        int i;

        for (i = 0; i < fpriv.u.t1.NumSubrs; i++) {
            gs_free_object(ctx->memory, fpriv.u.t1.Subrs[i].data, "Subrs[i]");
        }
        gs_free_object(ctx->memory, fpriv.u.t1.Subrs, "Subrs");
    }
    if (code < 0) {
        pdfi_countdown(t1f);
    }
    return code;
}

int
pdfi_free_font_type1(pdf_obj * font)
{
    pdf_font_type1 *t1f = (pdf_font_type1 *) font;
    int i;

    gs_free_object(OBJ_MEMORY(font), t1f->pfont, "Free Type 1 gs_font");

    pdfi_countdown(t1f->PDF_font);
    pdfi_countdown(t1f->BaseFont);
    pdfi_countdown(t1f->FontDescriptor);
    pdfi_countdown(t1f->Name);
    pdfi_countdown(t1f->Encoding);
    pdfi_countdown(t1f->ToUnicode);
    pdfi_countdown(t1f->CharStrings);

    if (t1f->fake_glyph_names != NULL) {
        for (i = 0; i < t1f->LastChar; i++) {
            if (t1f->fake_glyph_names[i].data != NULL)
                gs_free_object(OBJ_MEMORY(font), t1f->fake_glyph_names[i].data, "Type 1 fake_glyph_name");
        }
        gs_free_object(OBJ_MEMORY(font), t1f->fake_glyph_names, "Type 1 fake_glyph_names");
    }
    if (t1f->NumSubrs > 0 && t1f->Subrs != NULL) {
        for (i = 0; i < t1f->NumSubrs; i++) {
            gs_free_object(OBJ_MEMORY(font), t1f->Subrs[i].data, "Type 1 Subr");
        }
        gs_free_object(OBJ_MEMORY(font), t1f->Subrs, "Type 1 Subrs");
    }
    gs_free_object(OBJ_MEMORY(font), t1f->Widths, "Free Type 1 fontWidths");
    gs_free_object(OBJ_MEMORY(font), t1f, "Free Type 1 font");
    return 0;
}
