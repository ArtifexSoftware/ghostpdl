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

/* code for type 1 font handling */
#include "pdf_int.h"

#include "gsgdata.h"
#include "gstype1.h"
#include "gscencs.h"

#include "strmio.h"
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
#include "pdf_fontTT.h"

#include "gxtype1.h"        /* for gs_type1_state_s */
#include "gsutil.h"        /* For gs_next_ids() */

/* These are fonts for which we have to ignore "named" encodings */
typedef struct pdfi_t1_glyph_name_equivalents_s
{
    const char *name;
    const char *altname;
} pdfi_t1_glyph_name_equivalents_t;

static const pdfi_t1_glyph_name_equivalents_t pdfi_t1_glyph_name_equivalents[] =
{
  {"Ohungarumlaut", "Odblacute"},
  {"Uhungarumlaut", "Udblacute"},
  {"ohungarumlaut", "odblacute"},
  {"uhungarumlaut", "udblacute"},
  {NULL , NULL}
};

/* The Postscript code trawls the AGL to find all the equivalents.
   let's hope we can avoid that...
   Since none of the following show be required for a remotely valid
   Type 1, we just ignore errors (at least for now).
 */
static void pdfi_patch_charstrings_dict(pdf_dict *cstrings)
{
    int code = 0;
    pdf_obj *o;
    const pdfi_t1_glyph_name_equivalents_t *gne = pdfi_t1_glyph_name_equivalents;
    while(gne->name != NULL && code >= 0) {
        code = pdfi_dict_get(cstrings->ctx, cstrings, gne->name, &o);
        if (code >= 0) {
            code = pdfi_dict_put(cstrings->ctx, cstrings, gne->altname, o);
            pdfi_countdown(o);
        }
        if (code == gs_error_undefined)
            code = 0;
        gne++;
    }

    if (code >= 0) {
        bool key_known;
        pdf_string *pstr;
        byte notdefstr[] = { 0x9E, 0x35, 0xCE, 0xD7, 0xFF, 0xD3, 0x62, 0x2F, 0x09 };

        code = pdfi_dict_known(cstrings->ctx, cstrings, ".notdef", &key_known);
        if (code >=0 && key_known != true) {
            /* Seems there are plently of invalid Type 1 fonts without a .notdef,
               so make a fake one - a valid font will end up replacing this.
             */
            code = pdfi_object_alloc(cstrings->ctx, PDF_STRING, sizeof(notdefstr), (pdf_obj **) &pstr);
            if (code >= 0) {
                memcpy(pstr->data, notdefstr, sizeof(notdefstr));
                (void)pdfi_dict_put(cstrings->ctx, cstrings, ".notdef", (pdf_obj *) pstr);
            }
        }
    }
}

/* CALLBACKS */
static int
pdfi_t1_glyph_data(gs_font_type1 *pfont, gs_glyph glyph, gs_glyph_data_t *pgd)
{
    int code = 0;
    pdf_font_type1 *pdffont1 = (pdf_font_type1 *) pfont->client_data;
    pdf_context *ctx = (pdf_context *) pdffont1->ctx;
    pdf_name *glyphname = NULL;
    pdf_string *charstring = NULL;
    gs_const_string gname;

    code = (*ctx->get_glyph_name)((gs_font *)pfont, glyph, &gname);
    if (code >= 0) {
        code = pdfi_name_alloc(ctx, (byte *) gname.data, gname.size, (pdf_obj **)&glyphname);
        if (code >= 0)
            pdfi_countup(glyphname);
    }

    if (code >= 0) {
        code = pdfi_dict_get_by_key(ctx, pdffont1->CharStrings, glyphname, (pdf_obj **)&charstring);
        if (code < 0) {
            code = pdfi_map_glyph_name_via_agl(pdffont1->CharStrings, glyphname, &charstring);
        }
        if (code >= 0)
            gs_glyph_data_from_bytes(pgd, charstring->data, 0, charstring->length, NULL);
    }
    pdfi_countdown(charstring);
    pdfi_countdown(glyphname);

    return code;
}

static int
pdfi_t1_subr_data(gs_font_type1 *pfont, int index, bool global, gs_glyph_data_t *pgd)
{
    int code = 0;
    pdf_font_type1 *pdffont1 = (pdf_font_type1 *) pfont->client_data;

    if (global == true || index < 0 || index >= (pdffont1->Subrs == NULL ? 0 : pdfi_array_size(pdffont1->Subrs))) {
        code = gs_note_error(gs_error_rangecheck);
    }
    else {
        pdf_string *subr_str = NULL;
        code = pdfi_array_get_type(pdffont1->ctx, pdffont1->Subrs, index, PDF_STRING, (pdf_obj **)&subr_str);
        if (code >= 0) {
            gs_glyph_data_from_bytes(pgd, subr_str->data, 0, subr_str->length, NULL);
        }
        /* decrementing is safe here, because the reference in the pdffont1->Subrs will persist */
        pdfi_countdown(subr_str);
    }
    return code;
}

static int
pdfi_t1_seac_data(gs_font_type1 *pfont, int ccode, gs_glyph *pglyph, gs_const_string *gstr, gs_glyph_data_t *pgd)
{
    int code = 0;
    pdf_font_type1 *pdffont1 = (pdf_font_type1 *) pfont->client_data;
    pdf_context *ctx = (pdf_context *) pdffont1->ctx;
    gs_glyph glyph = gs_c_known_encode((gs_char)ccode, ENCODING_INDEX_STANDARD);

    if (glyph == GS_NO_GLYPH)
        return_error(gs_error_rangecheck);

    code = gs_c_glyph_name(glyph, gstr);
    if (code >= 0) {
        unsigned int nindex;
        code = (*ctx->get_glyph_index)((gs_font *)pfont, (byte *)gstr->data, gstr->size, &nindex);
        if (pglyph != NULL)
            *pglyph = (gs_glyph)nindex;
    }

    if (code >= 0) {
        pdf_name *glyphname = NULL;
        pdf_string *charstring = NULL;
        code = pdfi_name_alloc(ctx, (byte *) gstr->data, gstr->size, (pdf_obj **) &glyphname);
        if (code >= 0) {
            pdfi_countup(glyphname);
            code = pdfi_dict_get_by_key(ctx, pdffont1->CharStrings, glyphname, (pdf_obj **)&charstring);
            pdfi_countdown(glyphname);
            if (code >= 0) {
                if (pgd != NULL) {
                    gs_glyph_data_from_bytes(pgd, charstring->data, 0, charstring->length, NULL);
                }
                pdfi_countdown(charstring);
            }
        }
    }

    return code;
}

/* push/pop are null ops here */
static int
pdfi_t1_push(void *callback_data, const fixed *pf, int count)
{
    (void)callback_data;
    (void)pf;
    (void)count;
    return 0;
}
static int
pdfi_t1_pop(void *callback_data, fixed *pf)
{
    (void)callback_data;
    (void)pf;
    return 0;
}

static int
pdfi_t1_enumerate_glyph(gs_font *pfont, int *pindex,
                        gs_glyph_space_t glyph_space, gs_glyph *pglyph)
{
    int code;
    pdf_font_type1 *t1font = (pdf_font_type1 *) pfont->client_data;
    pdf_context *ctx = (pdf_context *) t1font->ctx;
    pdf_name *key;
    uint64_t i = (uint64_t) *pindex;

    (void)glyph_space;

    if (*pindex <= 0)
        code = pdfi_dict_key_first(ctx, t1font->CharStrings, (pdf_obj **) & key, &i);
    else
        code = pdfi_dict_key_next(ctx, t1font->CharStrings, (pdf_obj **) & key, &i);
    if (code < 0) {
        *pindex = 0;
        code = 0;
    }
    else {
        uint dummy = GS_NO_GLYPH;

        code = (*ctx->get_glyph_index)(pfont, key->data, key->length, &dummy);
        if (code < 0) {
            *pglyph = (gs_glyph) *pindex;
            goto exit;
        }
        *pglyph = dummy;
        if (*pglyph == GS_NO_GLYPH)
            *pglyph = (gs_glyph) *pindex;
        *pindex = (int)i;
    }
  exit:
    pdfi_countdown(key);
    return code;
}

/* This *should* only get called for SEAC lookups, which have to come from StandardEncoding
   so just try to lookup the string in the standard encodings
 */
int
pdfi_t1_global_glyph_code(const gs_font *pfont, gs_const_string *gstr, gs_glyph *pglyph)
{
    *pglyph = gs_c_name_glyph(gstr->data, gstr->size);
    return 0;
}

static int
pdfi_t1_glyph_outline(gs_font *pfont, int WMode, gs_glyph glyph,
                      const gs_matrix *pmat, gx_path *ppath, double sbw[4])
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

static int
pdfi_t1_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat, int members, gs_glyph_info_t *info)
{
    if ((members & GLYPH_INFO_OUTLINE_WIDTHS) == 0)
        return gs_type1_glyph_info(font, glyph, pmat, members, info);

    return gs_default_glyph_info(font, glyph, pmat, members, info);
}

/* END CALLBACKS */

static stream *
push_pfb_filter(gs_memory_t *mem, byte *buf, byte *bufend)
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
    (*s_PFBD_template.init)((stream_state *)st);
    st->binary_to_hex = true;
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
pop_pfb_filter(gs_memory_t *mem, stream *s)
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
pdfi_t1_decode_pfb(pdf_context *ctx, byte *inbuf, int inlen, byte **outbuf, int *outlen)
{
    stream *strm;
    int c, code = 0;
    int decodelen = 0;
    byte *d, *decodebuf = NULL;

    *outbuf = NULL;
    *outlen = 0;

    strm = push_pfb_filter(ctx->memory, inbuf, inbuf + inlen);
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
            strm = push_pfb_filter(ctx->memory, inbuf, inbuf + inlen);
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
pdfi_alloc_t1_font(pdf_context *ctx, pdf_font_type1 **font, uint32_t obj_num)
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

    pfont->encoding_index = ENCODING_INDEX_UNKNOWN;
    pfont->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

    pfont->client_data = (void *)t1font;

    *font = t1font;
    return 0;
}

static void
pdfi_t1_font_set_procs(pdf_context *ctx, pdf_font_type1 *font)
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

    font->default_font_info = gs_default_font_info;
    pfont->procs.font_info = pdfi_default_font_info;

    pfont->procs.glyph_info = pdfi_t1_glyph_info;
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
pdfi_read_type1_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, byte *fbuf, int64_t fbuflen, pdf_font **ppdffont)
{
    int code = 0;
    double x_scale;
    pdf_obj *fontdesc = NULL;
    pdf_obj *basefont = NULL;
    pdf_obj *mapname = NULL;
    pdf_obj *tmp = NULL;
    pdf_font_type1 *t1f = NULL;
    pdf_obj *tounicode = NULL;
    ps_font_interp_private fpriv = { 0 };
    bool key_known;
    bool force_symbolic = false;

    if (font_dict != NULL)
        (void)pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, &fontdesc);

    if (fbuf[0] == 128 && fbuf[1] == 1) {
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

    if (code >= 0) {
        fpriv.gsu.gst1.data.lenIV = 4;
        fpriv.gsu.gst1.data.ExpansionFactor = 0.06;
        fpriv.gsu.gst1.data.BlueShift = 7;
        fpriv.gsu.gst1.data.BlueFuzz = 1;
        code = pdfi_read_ps_font(ctx, font_dict, fbuf, fbuflen, &fpriv);
        gs_free_object(ctx->memory, fbuf, "pdfi_read_type1_font");

        /* If we have a full CharStrings dictionary, we probably have enough to make a font */
        if (code < 0 || fpriv.u.t1.CharStrings == NULL || pdfi_type_of(fpriv.u.t1.CharStrings) != PDF_DICT
            || fpriv.u.t1.CharStrings->entries == 0) {
                code = gs_note_error(gs_error_invalidfont);
                goto error;
        }
        code = pdfi_alloc_t1_font(ctx, &t1f, font_dict != NULL ? font_dict->object_num : 0);
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
            fpriv.gsu.gst1.UID.xvalues = NULL; /* In case of error */
            if (fpriv.gsu.gst1.WMode != 0) {
                if (fpriv.gsu.gst1.WMode != 1)
                    pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_WMODE, "pdfi_read_type1_font", NULL);
                pfont1->WMode = 1;
            }
            else
                pfont1->WMode = 0;
            pfont1->PaintType = fpriv.gsu.gst1.PaintType;
            pfont1->StrokeWidth = fpriv.gsu.gst1.StrokeWidth;

            if (font_dict != NULL) {
                t1f->object_num = font_dict->object_num;
                t1f->generation_num = font_dict->generation_num;
                t1f->indirect_num = font_dict->indirect_num;
                t1f->indirect_gen = font_dict->indirect_gen;
            }

            t1f->PDF_font = font_dict;
            pdfi_countup(font_dict);
            t1f->FontDescriptor = (pdf_dict *) fontdesc;
            pdfi_countup(fontdesc);
            t1f->Name = mapname;
            pdfi_countup(mapname);

            /* We want basefont, but we can live without it */
            if (font_dict != NULL) {
                (void)pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &basefont);
                t1f->BaseFont = basefont;
                pdfi_countup(basefont);
            }

            if (t1f->FontDescriptor != NULL) {
                code = pdfi_dict_get_int(ctx, t1f->FontDescriptor, "Flags", &t1f->descflags);
                if (code >= 0) {
                    /* If both the symbolic and non-symbolic flag are set,
                       believe that latter.
                     */
                    if ((t1f->descflags & 32) != 0)
                        t1f->descflags = (t1f->descflags & ~4);
                }
            }

            if (pdfi_font_known_symbolic(basefont)) {
                force_symbolic = true;
                t1f->descflags |= 4;
            }

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
            t1f->ToUnicode = tounicode;
            tounicode = NULL;

            pdfi_font_set_first_last_char(ctx, font_dict, (pdf_font *)t1f);

            /* Widths are defined assuming a 1000x1000 design grid, but we apply
             * them in font space - so undo the 1000x1000 scaling, and apply
             * the inverse of the font's x scaling
             */
            x_scale = 0.001 / hypot(pfont1->FontMatrix.xx, pfont1->FontMatrix.xy);

            /* ignore errors with widths... for now */
            if (font_dict != NULL)
                (void)pdfi_font_create_widths(ctx, font_dict, (pdf_font*)t1f, x_scale);

            if (font_dict != NULL)
                code = pdfi_dict_knownget(ctx, font_dict, "Encoding", &tmp);
            else
                code = gs_error_undefined;
            if (code == 1) {
                if (pdfi_type_of(tmp) == PDF_NAME && force_symbolic == true) {
                    t1f->Encoding = fpriv.u.t1.Encoding;
                    pdfi_countup(t1f->Encoding);
                }
                else if (pdfi_type_of(tmp) == PDF_DICT && (t1f->descflags & 4) != 0) {
                    code = pdfi_create_Encoding(ctx, (pdf_font *)t1f, tmp, (pdf_obj *)fpriv.u.t1.Encoding, (pdf_obj **) & t1f->Encoding);
                    if (code >= 0)
                        code = 1;
                }
                else {
                    code = pdfi_create_Encoding(ctx, (pdf_font *)t1f, tmp, NULL, (pdf_obj **) & t1f->Encoding);
                    if (code >= 0)
                        code = 1;
                }
                pdfi_countdown(tmp);
                tmp = NULL;
            }
            else {
                pdfi_countdown(tmp);
                tmp = NULL;
                code = 0;
            }

            if (code <= 0) {
                t1f->Encoding = fpriv.u.t1.Encoding;
                pdfi_countup(t1f->Encoding);
            }
            /* Since the underlying font stream can be shared between font descriptors,
               and the font descriptors can be shared between font objects, if we change
               the encoding, we can't share cached glyphs with other instances of this
               underlying font, so invalidate the UniqueID/XUID so the glyph cache won't
               try.
            */
            if (uid_is_XUID(&t1f->pfont->UID))
                uid_free(&t1f->pfont->UID, t1f->pfont->memory, "pdfi_read_type1_font");
            uid_set_invalid(&t1f->pfont->UID);

            t1f->CharStrings = fpriv.u.t1.CharStrings;
            pdfi_countup(t1f->CharStrings);
            pdfi_patch_charstrings_dict(t1f->CharStrings);

            t1f->Subrs = fpriv.u.t1.Subrs;
            fpriv.u.t1.Subrs = NULL;

            code = pdfi_font_generate_pseudo_XUID(ctx, font_dict, t1f->pfont);
            if (code < 0) {
                goto error;
            }

            t1f->blenddesignpositions = fpriv.u.t1.blenddesignpositions;
            pdfi_countup(t1f->blenddesignpositions);
            t1f->blenddesignmap = fpriv.u.t1.blenddesignmap;
            pdfi_countup(t1f->blenddesignmap);
            t1f->blendfontbbox = fpriv.u.t1.blendfontbbox;
            pdfi_countup(t1f->blendfontbbox);
            t1f->blendaxistypes = fpriv.u.t1.blendaxistypes;
            pdfi_countup(t1f->blendaxistypes);

            key_known = false;
            if (t1f->FontDescriptor != NULL) {
                 code = pdfi_dict_known(ctx, t1f->FontDescriptor, "FontFile", &key_known);
                 if (code < 0 || key_known == false) {
                     code = pdfi_dict_known(ctx, t1f->FontDescriptor, "FontFile2", &key_known);
                     if (code < 0 || key_known == false) {
                         code = pdfi_dict_known(ctx, t1f->FontDescriptor, "FontFile3", &key_known);
                         if (code < 0) {
                             key_known = false;
                         }
                     }
                 }
            }
            t1f->pfont->is_resource = (key_known == false);

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
                (void)replace_cache_entry(ctx, (pdf_obj *) t1f);
            }
            *ppdffont = (pdf_font *) t1f;
        }
    }

  error:
    pdfi_countdown(fontdesc);
    pdfi_countdown(basefont);
    pdfi_countdown(tounicode);
    pdfi_countdown(mapname);
    pdfi_countdown(tmp);
    pdfi_countdown(fpriv.u.t1.Encoding);
    pdfi_countdown(fpriv.u.t1.CharStrings);
    pdfi_countdown(fpriv.u.t1.blenddesignpositions);
    pdfi_countdown(fpriv.u.t1.blenddesignmap);
    pdfi_countdown(fpriv.u.t1.blendfontbbox);
    pdfi_countdown(fpriv.u.t1.blendaxistypes);
    pdfi_countdown(fpriv.u.t1.Subrs);
    if (fpriv.gsu.gst1.UID.xvalues != NULL) {
        gs_free_object(ctx->memory, fpriv.gsu.gst1.UID.xvalues, "pdfi_read_type1_font(xuid)");
    }

    if (code < 0) {
        tmp = NULL;
        if (font_dict != NULL) {
            if (pdfi_dict_get(ctx, font_dict, ".Path", &tmp) >= 0)
            {
                char fname[gp_file_name_sizeof + 1];
                pdf_string *fobj = (pdf_string *)tmp;

                memcpy(fname, fobj->data, fobj->length > gp_file_name_sizeof ? gp_file_name_sizeof : fobj->length);
                fname[fobj->length > gp_file_name_sizeof ? gp_file_name_sizeof : fobj->length] = '\0';

                pdfi_set_error_var(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_type1_font", "Error reading Type 1 font file %s\n", fname);
            }
            else {
                pdfi_set_error_var(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_type1_font", "Error reading embedded Type 1 font object %u\n", font_dict->object_num);
            }
        }
        else {
            pdfi_set_error(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_truetype_font", "Error reading font\n");
        }
        pdfi_countdown(tmp);
        pdfi_countdown(t1f);
    }
    return code;
}

int
pdfi_copy_type1_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont)
{
    int code = 0;
    pdf_font_type1 *font = NULL;
    gs_font_type1 *spfont1 = (gs_font_type1 *) spdffont->pfont;
    gs_font_type1 *dpfont1;
    gs_id t_id;
    pdf_obj *tmp;
    bool force_symbolic = false;

    if (font_dict == NULL)
        return_error(gs_error_invalidfont);

    code = pdfi_alloc_t1_font(ctx, &font, font_dict->object_num);
    if (code < 0)
        return code;
    dpfont1 = (gs_font_type1 *) font->pfont;

    t_id = dpfont1->id;
    memcpy(dpfont1, spfont1, sizeof(gs_font_type1));
    dpfont1->id = t_id;
    dpfont1->FAPI = NULL;
    dpfont1->FAPI_font_data = NULL;

    memcpy(font, spdffont, sizeof(pdf_font_type1));
    font->pfont = (gs_font_base *)dpfont1;
    font->refcnt = 1;
    dpfont1->client_data = (void *)font;
    font->filename = NULL;

    dpfont1->notify_list.memory = NULL;
    dpfont1->notify_list.first = NULL;
    gs_notify_init(&dpfont1->notify_list, dpfont1->memory);

    font->PDF_font = font_dict;
    font->object_num = font_dict->object_num;
    font->generation_num = font_dict->generation_num;
    pdfi_countup(font->PDF_font);

    /* We want basefont and descriptor, but we can live without them */
    font->BaseFont = NULL;
    code = pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &font->BaseFont);
    if (code < 0) {
        pdfi_countdown(font->BaseFont);
        font->BaseFont = NULL;
    }
    font->FontDescriptor = NULL;
    code = pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, (pdf_obj **)&font->FontDescriptor);
    if (code < 0) {
        pdfi_countdown(font->FontDescriptor);
        font->FontDescriptor = NULL;
    }

    pdfi_countup(font->Name);
    pdfi_countup(font->CharStrings);
    pdfi_countup(font->blenddesignpositions);
    pdfi_countup(font->blenddesignmap);
    pdfi_countup(font->blendfontbbox);
    pdfi_countup(font->blendaxistypes);
    pdfi_countup(font->Subrs);

    if (font->BaseFont != NULL && ((pdf_name *)font->BaseFont)->length <= gs_font_name_max - 1) {
        memcpy(dpfont1->key_name.chars, ((pdf_name *)font->BaseFont)->data, ((pdf_name *)font->BaseFont)->length);
        dpfont1->key_name.size = ((pdf_name *)font->BaseFont)->length;
        dpfont1->key_name.chars[dpfont1->key_name.size] = '\0';
        memcpy(dpfont1->font_name.chars, ((pdf_name *)font->BaseFont)->data, ((pdf_name *)font->BaseFont)->length);
        dpfont1->font_name.size = ((pdf_name *)font->BaseFont)->length;
        dpfont1->font_name.chars[dpfont1->font_name.size] = '\0';
    }

    font->Encoding = NULL;
    font->ToUnicode = NULL;
    font->Widths = NULL;

    pdfi_font_set_first_last_char(ctx, font_dict, (pdf_font *)font);
    (void)pdfi_font_create_widths(ctx, font_dict, (pdf_font *)font, (double)(0.001 / hypot(dpfont1->FontMatrix.xx, dpfont1->FontMatrix.xy)));

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

    if (pdfi_font_known_symbolic(font->BaseFont)) {
        force_symbolic = true;
        font->descflags |= 4;
    }

    tmp = NULL;
    code = pdfi_dict_knownget(ctx, font_dict, "Encoding", &tmp);
    if (code == 1) {
        if (pdfi_type_of(tmp) == PDF_NAME && force_symbolic == true) {
            font->Encoding = spdffont->Encoding;
            pdfi_countup(font->Encoding);
        }
        else if (pdfi_type_of(tmp) == PDF_DICT && (font->descflags & 4) != 0) {
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, tmp, (pdf_obj *)spdffont->Encoding, (pdf_obj **) &font->Encoding);
            if (code >= 0)
                code = 1;
        }
        else {
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, tmp, NULL, (pdf_obj **) & font->Encoding);
            if (code >= 0)
                code = 1;
        }
        pdfi_countdown(tmp);
        tmp = NULL;
    }
    else {
        pdfi_countdown(tmp);
        tmp = NULL;
        code = 0;
    }

    if (code <= 0) {
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

int
pdfi_free_font_type1(pdf_obj *font)
{
    pdf_font_type1 *t1f = (pdf_font_type1 *) font;

    gs_free_object(OBJ_MEMORY(font), t1f->pfont, "Free Type 1 gs_font");

    pdfi_countdown(t1f->PDF_font);
    pdfi_countdown(t1f->BaseFont);
    pdfi_countdown(t1f->FontDescriptor);
    pdfi_countdown(t1f->Name);
    pdfi_countdown(t1f->Encoding);
    pdfi_countdown(t1f->ToUnicode);
    pdfi_countdown(t1f->CharStrings);
    pdfi_countdown(t1f->blenddesignpositions);
    pdfi_countdown(t1f->blenddesignmap);
    pdfi_countdown(t1f->blendfontbbox);
    pdfi_countdown(t1f->blendaxistypes);
    pdfi_countdown(t1f->Subrs);
    pdfi_countdown(t1f->filename);

    gs_free_object(OBJ_MEMORY(font), t1f->Widths, "Free Type 1 fontWidths");
    gs_free_object(OBJ_MEMORY(font), t1f, "Free Type 1 font");
    return 0;
}
