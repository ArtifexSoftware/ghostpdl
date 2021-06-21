/* Copyright (C) 2018-2021 Artifex Software, Inc.
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

/* Font operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_array.h"
#include "pdf_font.h"
#include "pdf_stack.h"
#include "pdf_misc.h"
#include "pdf_doc.h"
#include "pdf_font_types.h"
#include "pdf_font0.h"
#include "pdf_font1.h"
#include "pdf_font1C.h"
#include "pdf_font3.h"
#include "pdf_fontTT.h"
#include "pdf_font0.h"
#include "pdf_fmap.h"
#include "gscencs.h"            /* For gs_c_known_encode and gs_c_glyph_name */

#include "strmio.h"
#include "stream.h"

static int pdfi_gs_setfont(pdf_context *ctx, gs_font *pfont)
{
    int code = 0;
    pdf_font *old_font = pdfi_get_current_pdf_font(ctx);

    code = gs_setfont(ctx->pgs, pfont);
    if (code >= 0)
        pdfi_countdown(old_font);

    return code;
}

/* These are fonts for which we have to ignore "named" encodings */
typedef struct known_symbolic_font_name_s
{
    const char *name;
    const int namelen;
} known_symbolic_font_name_t;

#define DEFINE_NAME_LEN(s) #s, sizeof(#s) - 1
static const known_symbolic_font_name_t known_symbolic_font_names[] =
{
  {DEFINE_NAME_LEN(Wingdings2)},
  {DEFINE_NAME_LEN(Wingdings)},
  {DEFINE_NAME_LEN(ZapfDingbats)},
  {NULL , 0}
};
#undef DEFINE_NAME_LEN

bool pdfi_font_known_symbolic(pdf_obj *basefont)
{
    bool ignore = false;
    int i;
    pdf_name *nm = (pdf_name *)basefont;

    if (basefont != NULL && basefont->type == PDF_NAME) {
        for (i = 0; known_symbolic_font_names[i].name != NULL; i++) {
            if (nm->length == known_symbolic_font_names[i].namelen
             && !strncmp((char *)nm->data, known_symbolic_font_names[i].name, nm->length)) {
                ignore = true;
                break;
            }
        }
    }
    return ignore;
}

static int
pdfi_font_match_glyph_widths(pdf_font *pdfont)
{
    int code = 0;
    int i;
    int sindex, lindex;
    gs_font_base *pbfont = pdfont->pfont;
    double fw = 0.0, ww = 0.0;

    if (pdfont->FirstChar < 0 || pdfont->LastChar < 0 || pdfont->Widths == NULL)
        return 0; /* Technically invalid - carry on, hope for the best */

    /* For "best" results, restrict to what we *hope* are A-Z,a-z */
    sindex = pdfont->FirstChar < 96 ? 96 : pdfont->FirstChar;
    lindex = pdfont->LastChar > 122 ? 122 : pdfont->LastChar;

    for (i = sindex; i < lindex; i++) {
        gs_glyph_info_t ginfo = {0};

        /* We're only interested in non-zero Widths entries for glyphs that actually exist in the font */
        if (pdfont->Widths[i - pdfont->FirstChar] != 0.0
          && (*pbfont->procs.glyph_info)((gs_font *)pbfont, (gs_glyph)i, NULL, GLYPH_INFO_WIDTH0, &ginfo) >= 0) {
            fw += hypot(ginfo.width[0].x, ginfo.width[0].y);
            ww += pdfont->Widths[i - pdfont->FirstChar];
        }
    }
    /* Only reduce font width, don't expand */
    if (ww != 0.0 && fw != 0.0 && ww / fw < 1.0) {
        gs_matrix nmat, smat = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
        double wscale;
        smat.xx = smat.yy = ww/fw;
        wscale = 1.0 / smat.xx;

        gs_matrix_multiply(&pbfont->FontMatrix, &smat, &nmat);
        memcpy(&pbfont->FontMatrix, &nmat, sizeof(pbfont->FontMatrix));

        for (i = pdfont->FirstChar; i <= pdfont->LastChar; i++) {
            pdfont->Widths[i - pdfont->FirstChar] *= wscale;
        }

        /* Purging a font can be expensive, but in this case, we know
           we have no scaled instances (pdfi doesn't work that way)
           and we know we have no fm pairs, nor glyphs to purge (we
           *just* created the font!).
           So "purging" the font is really just removing it from the
           doubly linked list of font objects in the font directory
         */
        code = gs_purge_font((gs_font *)pbfont);
        if (code >= 0)
            code = gs_definefont(pbfont->dir, (gs_font *)pbfont);
        if (code >= 0)
            code = pdfi_fapi_passfont((pdf_font *)pdfont, 0, NULL, NULL, NULL, 0);
    }

    return code;
}


/* Call with a CIDFont name to try to find the CIDFont on disk
   call if with ffname NULL to load the default fallback CIDFont
   substitue
   Currently only loads subsitute - DroidSansFallback
 */
static int
pdfi_open_CIDFont_substitute_file(pdf_context * ctx, pdf_dict *font_dict, pdf_dict *fontdesc, bool fallback, byte ** buf, int64_t * buflen)
{
    int code = gs_error_invalidfont;

    if (fallback == true) {
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
    else {
        code = gs_error_invalidfont;
    }

    return code;
}

enum
{
    pdfi_font_flag_none =        0x00000,
    pdfi_font_flag_fixed =       0x00001,
    pdfi_font_flag_serif =       0x00002,
    pdfi_font_flag_symbolic =    0x00004,
    pdfi_font_flag_script =      0x00008,
    pdfi_font_flag_nonsymbolic = 0x00020,
    pdfi_font_flag_italic =      0x00040,
    pdfi_font_flag_allcap =      0x10000,
    pdfi_font_flag_smallcap =    0x20000,
    pdfi_font_flag_forcebold =   0x40000
};

/* Barefaced theft from mupdf! */
static const char *pdfi_base_font_names[][10] =
{
  { "Courier", "CourierNew", "CourierNewPSMT", "CourierStd", NULL },
  { "Courier-Bold", "CourierNew,Bold", "Courier,Bold", "CourierNewPS-BoldMT", "CourierNew-Bold", NULL },
  { "Courier-Oblique", "CourierNew,Italic", "Courier,Italic", "CourierNewPS-ItalicMT", "CourierNew-Italic", NULL },
  { "Courier-BoldOblique", "CourierNew,BoldItalic", "Courier,BoldItalic", "CourierNewPS-BoldItalicMT", "CourierNew-BoldItalic", NULL },
  { "Helvetica", "ArialMT", "Arial", NULL },
  { "Helvetica-Bold", "Arial-BoldMT", "Arial,Bold", "Arial-Bold", "Helvetica,Bold", NULL },
  { "Helvetica-Oblique", "Arial-ItalicMT", "Arial,Italic", "Arial-Italic", "Helvetica,Italic", "Helvetica-Italic", NULL },
  { "Helvetica-BoldOblique", "Arial-BoldItalicMT", "Arial,BoldItalic", "Arial-BoldItalic", "Helvetica,BoldItalic", "Helvetica-BoldItalic", NULL },
  { "Times-Roman", "TimesNewRomanPSMT", "TimesNewRoman", "TimesNewRomanPS", NULL },
  { "Times-Bold", "TimesNewRomanPS-BoldMT", "TimesNewRoman,Bold", "TimesNewRomanPS-Bold", "TimesNewRoman-Bold", NULL },
  { "Times-Italic", "TimesNewRomanPS-ItalicMT", "TimesNewRoman,Italic", "TimesNewRomanPS-Italic", "TimesNewRoman-Italic", NULL },
  { "Times-BoldItalic", "TimesNewRomanPS-BoldItalicMT", "TimesNewRoman,BoldItalic", "TimesNewRomanPS-BoldItalic", "TimesNewRoman-BoldItalic", NULL },
  { "Symbol", "Symbol,Italic", "Symbol,Bold", "Symbol,BoldItalic", "SymbolMT", "SymbolMT,Italic", "SymbolMT,Bold", "SymbolMT,BoldItalic", NULL },
  { "ZapfDingbats", NULL }
};

static int strncmp_ignore_space(const char *a, const char *b, int64_t len)
{
    while (len--)
    {
        while (*a == ' ')
            a++;
        while (*b == ' ')
            b++;
        if (*a != *b)
            return 1;
        if (*a == 0)
            return *a != *b;
        if (*b == 0)
            return *a != *b;
        a++;
        b++;
    }
    return 0;
}

static const char *pdfi_clean_font_name(const pdf_name *fontname)
{
    int i, k;
    for (i = 0; i < (sizeof(pdfi_base_font_names)/sizeof(pdfi_base_font_names[0])); i++) {
        for (k = 0; pdfi_base_font_names[i][k]; k++) {
            if (!strncmp_ignore_space(pdfi_base_font_names[i][k], (const char *)fontname->data, fontname->length))
                return pdfi_base_font_names[i][0];
        }
    }
    return NULL;
}

static const char *pdfi_font_substitute_by_flags(unsigned int flags)
{
    bool fixed = ((flags & pdfi_font_flag_fixed) != 0);
    bool serif = ((flags & pdfi_font_flag_serif) != 0);
    bool italic = ((flags & pdfi_font_flag_italic) != 0);
    bool bold = ((flags & pdfi_font_flag_forcebold) != 0);

    if (fixed) {
        if (bold) {
            if (italic) {
                return "Courier-BoldOblique";
            }
            else {
                return "Courier-Bold";
            }
        }
        else {
            if (italic) {
                return "Courier-Oblique";
            }
            else {
                return "Courier";
            }
        }
    }
    else if (serif) {
        if (bold) {
            if (italic) {
                return "Times-BoldItalic";
            }
            else {
                return "Times-Bold";
            }
        }
        else {
            if (italic) {
                return "Times-Italic";
            }
            else {
                return "Times-Roman";
            }
        }
    } else {
        if (bold) {
            if (italic) {
                return "Helvetica-BoldOblique";
            }
            else {
                return "Helvetica-Bold";
            }
        }
        else {
            if (italic) {
                return "Helvetica-Oblique";
            }
            else {
                return "Helvetica";
            }
        }
    }
    return "Helvetica"; /* Really shouldn't ever happen */
}

static void pdfi_emprint_font_name(pdf_context *ctx, pdf_name *n)
{
    int i;
    for (i = 0; i < n->length; i++) {
        dmprintf1(ctx->memory, "%c", n->data[i]);
    }
}

static int
pdfi_open_font_substitute_file(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *fontdesc, bool fallback, byte **buf, int64_t *buflen)
{
    int code;
    char fontfname[gp_file_name_sizeof];
    const char *romfsprefix = "%rom%Resource/Font/";
    const int romfsprefixlen = strlen(romfsprefix);
    pdf_obj *basefont = NULL, *mapname;
    pdf_obj *fontname = NULL;
    stream *s;
    const char *fn;

    code = pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &basefont);
    if (code < 0)
        fallback = true;

    if (fallback == true) {
        const char *fbname;
        int64_t flags = 0;
        if (fontdesc != NULL) {
            code = pdfi_dict_get_int(ctx, fontdesc, "Flags", &flags);
        }
        fbname = pdfi_font_substitute_by_flags((int)flags);
        code = pdfi_name_alloc(ctx, (byte *)fbname, strlen(fbname), (pdf_obj **) &fontname);
        if (code < 0)
            return code;
        pdfi_countup(fontname);
    }
    else {
        fontname = basefont;
        pdfi_countup(fontname);
    }

    fn = pdfi_clean_font_name((pdf_name *)fontname);
    if (fn != NULL) {
        pdfi_countdown(fontname);

        code = pdfi_name_alloc(ctx, (byte *)fn, strlen(fn), (pdf_obj **) &fontname);
        if (code < 0)
            return code;
        pdfi_countup(fontname);
    }

    code = pdf_fontmap_lookup_font(ctx, (pdf_name *) fontname, &mapname);
    if (code < 0) {
        mapname = fontname;
        pdfi_countup(mapname);
        code = 0;
    }
    if (mapname->type == PDF_NAME) {
        pdf_name *mname = (pdf_name *) mapname;
        memcpy(fontfname, romfsprefix, romfsprefixlen);
        memcpy(fontfname + romfsprefixlen, mname->data, mname->length);
        fontfname[romfsprefixlen + mname->length] = '\0';
    }

    s = sfopen(fontfname, "r", ctx->memory);
    if (s == NULL) {
        code = gs_note_error(gs_error_undefinedfilename);
    }
    else {
        if (basefont) {
            dmprintf(ctx->memory, "Loading font ");
            pdfi_emprint_font_name(ctx, (pdf_name *)basefont);
            dmprintf(ctx->memory, " (or substitute) from ");
        }
        else {
            dmprintf(ctx->memory, "Loading nameless font from ");
        }
        dmprintf1(ctx->memory, "%s.\n", fontfname);

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

    pdfi_countdown(basefont);
    pdfi_countdown(mapname);
    pdfi_countdown(fontname);
    return code;
}

enum {
  no_type_font = -1,
  type0_font = 0,
  type1_font = 1,
  cff_font = 2,
  type3_font = 3,
  tt_font = 42
};

static int pdfi_fonttype_picker(byte *buf, int64_t buflen)
{
#define MAKEMAGIC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

    if (buflen >= 4) {
        if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC(0, 1, 0, 0)
            || MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('t', 'r', 'u', 'e')
            || MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('t', 't', 'c', 'f')) {
            return tt_font;
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('O', 'T', 'T', 'O')) {
            return cff_font; /* OTTO will end up as CFF */
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], 0) == MAKEMAGIC('%', '!', 'P', 0)) {
            return type1_font; /* pfa */
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], 0) == MAKEMAGIC(1, 0, 4, 0)) {
            return cff_font; /* 1C/CFF */
        }
        else if (MAKEMAGIC(buf[0], buf[1], 0, 0) == MAKEMAGIC(128, 1, 0, 0)) {
            return type1_font; /* pfb */
        }
    }
    return no_type_font;
#undef MAKEMAGIC
}

enum {
  font_embedded = 0,
  font_from_file = 1,
  font_substitute = 2
};

int pdfi_load_font(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_dict *font_dict, gs_font **ppfont, bool cidfont)
{
    int code;
    pdf_font *ppdffont;
    pdf_name *Type = NULL;
    pdf_name *Subtype = NULL;
    pdf_dict *fontdesc = NULL;
    pdf_stream *fontfile = NULL;
    pdf_name *ffsubtype = NULL;
    int fftype = no_type_font;
    byte *fbuf = NULL;
    int64_t fbuflen;
    int substitute = font_embedded;

    code = pdfi_dict_get_type(ctx, font_dict, "Type", PDF_NAME, (pdf_obj **)&Type);
    if (code < 0)
        goto exit;
    if (!pdfi_name_is(Type, "Font")){
        code = gs_note_error(gs_error_typecheck);
        goto exit;
    }
    code = pdfi_dict_get_type(ctx, font_dict, "Subtype", PDF_NAME, (pdf_obj **)&Subtype);

    /* Beyond Type 0 and Type 3, there is no point trusting the Subtype key */
    if (code >= 0 && pdfi_name_is(Subtype, "Type0")) {
        code = pdfi_read_type0_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &ppdffont);
    }
    else if (code >= 0 && pdfi_name_is(Subtype, "Type3")) {
        code = pdfi_read_type3_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &ppdffont);
        if (code < 0)
            goto exit;
    }
    else {
        /* We should always have a font descriptor here, but we have to carry on
           even if we don't
         */
        code = pdfi_dict_get_type(ctx, font_dict, "FontDescriptor", PDF_DICT, (pdf_obj**)&fontdesc);
        if (fontdesc != NULL && fontdesc->type == PDF_DICT) {
            code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile", PDF_STREAM, (pdf_obj**)&fontfile);
            if (code >= 0)
                fftype = type1_font;
            else {
                code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile2", PDF_STREAM, (pdf_obj**)&fontfile);
                fftype = tt_font;
            }
            if (code < 0) {
                code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile3", PDF_STREAM, (pdf_obj**)&fontfile);
                if (fontfile != NULL) {
                    code = pdfi_dict_get_type(ctx, fontfile->stream_dict, "Subtype", PDF_NAME, (pdf_obj **)&ffsubtype);
                    if (code >= 0) {
                        if (pdfi_name_is(ffsubtype, "Type1"))
                            fftype = type1_font;
                        else if (pdfi_name_is(ffsubtype, "Type1C"))
                            fftype = cff_font;
                        else if (pdfi_name_is(ffsubtype, "OpenType"))
                            fftype = cff_font;
                        else if (pdfi_name_is(ffsubtype, "CIDFontType0C"))
                            fftype = cff_font;
                        else if (pdfi_name_is(ffsubtype, "TrueType"))
                            fftype = tt_font;
                        else
                            fftype = no_type_font;
                    }
                }
            }
        }

        if (fontfile != NULL) {
            code = pdfi_stream_to_buffer(ctx, (pdf_stream *) fontfile, &fbuf, &fbuflen);
            pdfi_countdown(fontfile);
            if (fbuflen == 0) {
                gs_free_object(ctx->memory, fbuf, "pdfi_load_font(fbuf)");
                fbuf = NULL;
                code = gs_note_error(gs_error_invalidfont);
            }
        }

        while (1) {
            if (fbuf != NULL) {
                /* First, see if we can glean the type from the magic number */
                int sftype = pdfi_fonttype_picker(fbuf, fbuflen);
                if (sftype == no_type_font) {
                    if (fftype != no_type_font)
                        sftype = fftype;
                    else {
                        if (pdfi_name_is(Subtype, "Type1") || pdfi_name_is(Subtype, "MMType1"))
                            sftype = type1_font;
                        else if (pdfi_name_is(Subtype, "Type1C"))
                            sftype = cff_font;
                        else if (pdfi_name_is(Subtype, "TrueType"))
                            sftype = tt_font;
                    }
                }
                /* fbuf ownership passes to the font loader */
                switch (sftype) {
                    case type1_font:
                        code = pdfi_read_type1_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, fbuf, fbuflen, &ppdffont);
                        fbuf = NULL;
                        break;
                    case cff_font:
                        code = pdfi_read_cff_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, fbuf, fbuflen, cidfont, &ppdffont);
                        fbuf = NULL;
                        break;
                    case tt_font:
                        {
                            if (cidfont)
                                code = pdfi_read_cidtype2_font(ctx, font_dict, stream_dict, page_dict, fbuf, fbuflen, &ppdffont);
                            else
                                code = pdfi_read_truetype_font(ctx, font_dict, stream_dict, page_dict, fbuf, fbuflen, &ppdffont);
                            fbuf = NULL;
                        }
                        break;
                    default:
                        code = gs_note_error(gs_error_invalidfont);
                }
                if (code < 0 && substitute == font_embedded) {
                    dmprintf2(ctx->memory, "**** Error: can't process embedded stream for font object %d %d.\n", font_dict->object_num, font_dict->generation_num);
                    dmprintf(ctx->memory, "**** Attempting to load substitute font.\n");
                }
            }

            if (code < 0 && code != gs_error_VMerror && substitute == font_embedded) {
                /* Font not embedded, or embedded font not usable - use a substitute */
                if (fbuf != NULL) {
                    gs_free_object(ctx->memory, fbuf, "pdfi_load_font(fbuf)");
                }

                substitute = font_from_file;

                if (cidfont == true) {
                    code =  pdfi_open_CIDFont_substitute_file(ctx, font_dict, fontdesc, false, &fbuf, &fbuflen);
                    if (code < 0) {
                        code =  pdfi_open_CIDFont_substitute_file(ctx, font_dict, fontdesc, true, &fbuf, &fbuflen);
                        substitute |= font_substitute;
                    }

                    if (code < 0)
                        goto exit;
                }
                else {
                    code = pdfi_open_font_substitute_file(ctx, font_dict, fontdesc, false, &fbuf, &fbuflen);
                    if (code < 0) {
                        code = pdfi_open_font_substitute_file(ctx, font_dict, fontdesc, true, &fbuf, &fbuflen);
                        substitute |= font_substitute;
                    }

                    if (code < 0)
                        goto exit;
                }
                continue;
            }
            break;
        }
    }

    if (ppdffont == NULL)
        code = gs_note_error(gs_error_invalidfont);
    else {
        if (cidfont) {
            ((pdf_cidfont_t *)ppdffont)->substitute = (substitute != font_embedded);
        }
        else {
            if ((substitute & font_substitute) == font_substitute)
                code = pdfi_font_match_glyph_widths(ppdffont);
        }
        *ppfont = (gs_font *)ppdffont->pfont;
     }

exit:
    pdfi_countdown(fontdesc);
    pdfi_countdown(Type);
    pdfi_countdown(Subtype);
    pdfi_countdown(ffsubtype);
    return code;
}

int pdfi_load_dict_font(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_dict *font_dict, double point_size)
{
    int code;
    gs_font *pfont;

    if (font_dict->type == PDF_FONT) {
        pfont = (gs_font *)((pdf_font *)font_dict)->pfont;
        code = 0;
    }
    else {
        if (font_dict->type != PDF_DICT) {
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }
        code = pdfi_load_font(ctx, stream_dict, page_dict, font_dict, &pfont, false);
    }
    if (code < 0)
        goto exit;

    /* Everything looks good, set the font, unless it's the current font */
    if (pfont != ctx->pgs->font)
        code = pdfi_gs_setfont(ctx, pfont);

    if (code < 0)
        goto exit;

    code = gs_setPDFfontsize(ctx->pgs, point_size);
exit:
    return code;
}

static int pdfi_load_resource_font(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_name *fontname, double point_size)
{
    int code;
    pdf_dict *font_dict = NULL;

    if (fontname->type != PDF_NAME)
        return_error(gs_error_typecheck);

    /* Look fontname up in the resources */
    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        goto exit;
    code = pdfi_find_resource(ctx, (unsigned char *)"Font", fontname, stream_dict, page_dict, (pdf_obj **)&font_dict);
    (void)pdfi_loop_detector_cleartomark(ctx);
    if (code < 0)
        goto exit;
    code = pdfi_load_dict_font(ctx, stream_dict, page_dict, font_dict, point_size);

exit:
    if (code < 0)
        pdfi_countdown(font_dict);
    return code;
}

int pdfi_d0(pdf_context *ctx)
{
    int code = 0, gsave_level = 0;
    double width[2];

    if (ctx->text.inside_CharProc == false)
        ctx->pdf_warnings |= W_PDF_NOTINCHARPROC;

    if (pdfi_count_stack(ctx) < 2) {
        code = gs_note_error(gs_error_stackunderflow);
        goto d0_error;
    }

    if (ctx->stack_top[-1]->type != PDF_INT && ctx->stack_top[-1]->type != PDF_REAL) {
        code = gs_note_error(gs_error_typecheck);
        goto d0_error;
    }
    if (ctx->stack_top[-2]->type != PDF_INT && ctx->stack_top[-2]->type != PDF_REAL) {
        code = gs_note_error(gs_error_typecheck);
        goto d0_error;
    }
    if(ctx->text.current_enum == NULL) {
        code = gs_note_error(gs_error_undefined);
        goto d0_error;
    }

    if (ctx->stack_top[-1]->type == PDF_INT)
        width[0] = (double)((pdf_num *)ctx->stack_top[-1])->value.i;
    else
        width[0] = ((pdf_num *)ctx->stack_top[-1])->value.d;
    if (ctx->stack_top[-2]->type == PDF_INT)
        width[1] = (double)((pdf_num *)ctx->stack_top[-1])->value.i;
    else
        width[1] = ((pdf_num *)ctx->stack_top[-1])->value.d;

    gsave_level = ctx->pgs->level;

    /*
     * We don't intend to retain this, instead we will use (effectively) xyshow to apply
     * width overrides at the text level.
    if (font && font->Widths && ctx->current_chr >= font->FirstChar && ctx->current_chr <= font->LastChar)
        width[0] = font->Widths[font->ctx->current_chr - font->FirstChar];
     */

    if (ctx->text.current_enum == NULL) {
        code = gs_note_error(gs_error_unknownerror);
        goto d0_error;
    }

    code = gs_text_setcharwidth(ctx->text.current_enum, width);

    /* Nasty hackery. setcachedevice potentially pushes a new device into the graphics state
     * and there's no way to remove that device again without grestore'ing back to a point
     * before the device was loaded. To facilitate this, setcachedevice will do a gs_gsave()
     * before changing the device. Note, the grestore for this is done back in show_update()
     * which is not reached until after the CharProc has been executed.
     *
     * This is a problem for us when running a PDF content stream, because after running the
     * stream we check the gsave level and, if its not the same as it was when we started
     * the stream, we pdfi_grestore() back until it is. This mismatch of the gsave levels
     * causes all sorts of trouble with the font and we can end up counting the pdf_font
     * object down and discarding the font we're tryign to use.
     *
     * The solution (ugly though it is) is to patch up the saved gsave_level in the
     * context to expect that we have one more gsave level on exit. That wasy we won't
     * try and pdf_grestore() back to an earlier point.
     */
    if (ctx->pgs->level > gsave_level)
        ctx->current_stream_save.gsave_level += ctx->pgs->level - gsave_level;

    if (code < 0)
        goto d0_error;
    pdfi_pop(ctx, 2);
    return 0;

d0_error:
    pdfi_clearstack(ctx);
    return code;
}

int pdfi_d1(pdf_context *ctx)
{
    int code = 0, i, gsave_level;
    double wbox[6];

    if (ctx->text.inside_CharProc == false)
        ctx->pdf_warnings |= W_PDF_NOTINCHARPROC;

    ctx->text.CharProc_is_d1 = true;

    if (pdfi_count_stack(ctx) < 2) {
        code = gs_note_error(gs_error_stackunderflow);
        goto d1_error;
    }

    for (i=-6;i < 0;i++) {
        if (ctx->stack_top[i]->type != PDF_INT && ctx->stack_top[i]->type != PDF_REAL) {
            code = gs_note_error(gs_error_typecheck);
            goto d1_error;
        }
        if (ctx->stack_top[i]->type == PDF_INT)
            wbox[i + 6] = (double)((pdf_num *)ctx->stack_top[i])->value.i;
        else
            wbox[i + 6] = ((pdf_num *)ctx->stack_top[i])->value.d;
    }

    /*
     * We don't intend to retain this, instead we will use (effectively) xyshow to apply
     * width overrides at the text level.
    if (font && font->Widths && ctx->current_chr >= font->FirstChar && ctx->current_chr <= font->LastChar)
        wbox[0] = font->Widths[font->ctx->current_chr - font->FirstChar];
     */

    gsave_level = ctx->pgs->level;

    if (ctx->text.current_enum == NULL) {
        code = gs_note_error(gs_error_unknownerror);
        goto d1_error;
    }

    code = gs_text_setcachedevice(ctx->text.current_enum, wbox);

    /* See the comment immediately after gs_text_setcachedvice() in pdfi_d0 above */
    if (ctx->pgs->level > gsave_level)
        ctx->current_stream_save.gsave_level += ctx->pgs->level - gsave_level;

    if (code < 0)
        goto d1_error;
    pdfi_pop(ctx, 6);
    return 0;

d1_error:
    pdfi_clearstack(ctx);
    return code;
}

int pdfi_Tf(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    double point_size = 0;
    pdf_obj *point_arg = NULL;
    int code = 0;
    pdf_name *fontname = NULL;

    if (pdfi_count_stack(ctx) < 2) {
        pdfi_clearstack(ctx);
        return_error(gs_error_stackunderflow);
    }

    /* Get refs to the args and pop them */
    point_arg = ctx->stack_top[-1];
    pdfi_countup(point_arg);
    fontname = (pdf_name *)ctx->stack_top[-2];
    pdfi_countup(fontname);
    pdfi_pop(ctx, 2);

    /* Get the point_size */
    if (point_arg->type == PDF_INT)
        point_size = (double)((pdf_num *)point_arg)->value.i;
    else {
        if (point_arg->type == PDF_REAL)
            point_size = ((pdf_num *)point_arg)->value.d;
        else {
            code = gs_note_error(gs_error_typecheck);
            goto exit0;
        }
    }

    code = pdfi_load_resource_font(ctx, stream_dict, page_dict, fontname, point_size);

    /* If we failed to load font, try to load an internal one */
    if (code < 0 && fontname)
        code = pdfi_font_set_internal_name(ctx, fontname, point_size);
 exit0:
    pdfi_countdown(fontname);
    pdfi_countdown(point_arg);
    return code;
}

int pdfi_free_font(pdf_obj *font)
{
    pdf_font *f = (pdf_font *)font;

    switch (f->pdfi_font_type) {
        case e_pdf_font_type0:
            return pdfi_free_font_type0((pdf_obj *)font);
            break;
        case e_pdf_font_type1:
            return pdfi_free_font_type1((pdf_obj *)font);
            break;
        case e_pdf_font_cff:
            return pdfi_free_font_cff((pdf_obj *)font);
        case e_pdf_font_type3:
            return pdfi_free_font_type3((pdf_obj *)font);
            break;
        case e_pdf_font_truetype:
            return pdfi_free_font_truetype((pdf_obj *)font);
            break;
        case e_pdf_cidfont_type2:
            return pdfi_free_font_cidtype2((pdf_obj *)font);
            break;
        case e_pdf_cidfont_type0:
            return pdfi_free_font_cidtype0((pdf_obj *)font);
            break;
        case e_pdf_cidfont_type1:
        case e_pdf_cidfont_type4:
        default:
            return gs_note_error(gs_error_typecheck);
            break;
    }
    return 0;
}

/*
 * Routine to fill in an array with each of the glyph names from a given
 * 'standard' Encoding.
 */
static int pdfi_build_Encoding(pdf_context *ctx, pdf_name *name, pdf_array *Encoding)
{
    int i, code = 0;
    unsigned char gs_encoding;
    gs_glyph temp;
    gs_const_string str;
    pdf_name *n = NULL;

    if (pdfi_array_size(Encoding) < 256)
        return gs_note_error(gs_error_rangecheck);

    if (pdfi_name_is(name, "StandardEncoding")) {
        gs_encoding = ENCODING_INDEX_STANDARD;
    } else {
        if (pdfi_name_is(name, "WinAnsiEncoding")){
            gs_encoding = ENCODING_INDEX_WINANSI;
        } else {
            if (pdfi_name_is(name, "MacRomanEncoding")){
                gs_encoding = ENCODING_INDEX_MACROMAN;
            } else {
                if (pdfi_name_is(name, "MacExpertEncoding")){
                    gs_encoding = ENCODING_INDEX_MACEXPERT;
                } else {
                    return_error(gs_error_undefined);
                }
            }
        }
    }
    i = 0;
    for (i=0;i<256;i++) {
        temp = gs_c_known_encode(i, gs_encoding);
        gs_c_glyph_name(temp, &str);
        code = pdfi_name_alloc(ctx, (byte *)str.data, str.size, (pdf_obj **)&n);
        if (code < 0)
            return code;
        pdfi_countup(n);
        code = pdfi_array_put(ctx, Encoding, (uint64_t)i, (pdf_obj *)n);
        pdfi_countdown(n);
        if (code < 0)
            return code;
    }
    return 0;
}

/*
 * Create and fill in a pdf_array with an Encoding for a font. pdf_Encoding must be either
 * a name (eg StandardEncoding) or a dictionary. If its a name we use that to create the
 * entries, if its a dictionary we start by getting the BaseEncoding and using that to
 * create an array of glyph names as above, *or* for a symbolic font, we use the "predef_Encoding"
 * which is the encoding from the font description itself (i.e. the /Encoding array
 * from a Type 1 font. We then get the Differences array from the dictionary and use that to
 * refine the Encoding.
 */
int pdfi_create_Encoding(pdf_context *ctx, pdf_obj *pdf_Encoding, pdf_obj *font_Encoding, pdf_obj **Encoding)
{
    int code = 0, i;

    code = pdfi_array_alloc(ctx, 256, (pdf_array **)Encoding);
    if (code < 0)
        return code;
    pdfi_countup(*Encoding);

    if (pdf_Encoding->type == PDF_NAME) {
        code = pdfi_build_Encoding(ctx, (pdf_name *)pdf_Encoding, (pdf_array *)*Encoding);
        if (code < 0)
            return code;
    } else {
        if (pdf_Encoding->type == PDF_DICT) {
            pdf_name *n = NULL;
            pdf_array *a = NULL;
            pdf_obj *o = NULL;
            int offset = 0;

            if (font_Encoding != NULL && font_Encoding->type == PDF_ARRAY) {
                pdf_array *fenc = (pdf_array *)font_Encoding;
                for (i = 0; i < pdfi_array_size(fenc) && code >= 0; i++) {
                    code = pdfi_array_get(ctx, fenc, (uint64_t)i, &o);
                    if (code >= 0)
                        code = pdfi_array_put(ctx, (pdf_array *)*Encoding, (uint64_t)i, o);
                }
                if (code < 0) {
                    pdfi_countdown(*Encoding);
                    *Encoding = NULL;
                    return code;
                }
            }
            else {
                code = pdfi_dict_get(ctx, (pdf_dict *)pdf_Encoding, "BaseEncoding", (pdf_obj **)&n);
                if (code < 0) {
                    code = pdfi_name_alloc(ctx, (byte *)"StandardEncoding", 16, (pdf_obj **)&n);
                    if (code < 0)
                        return code;
                    pdfi_countup(n);
                }
                code = pdfi_build_Encoding(ctx, n, (pdf_array *)*Encoding);
                if (code < 0) {
                    pdfi_countdown(n);
                        return code;
                }
                pdfi_countdown(n);
            }
            code = pdfi_dict_knownget_type(ctx, (pdf_dict *)pdf_Encoding, "Differences", PDF_ARRAY, (pdf_obj **)&a);
            if (code <= 0)
                return code;
            for (i=0;i < pdfi_array_size(a);i++) {
                code = pdfi_array_get(ctx, a, (uint64_t)i, &o);
                if (code < 0)
                    break;
                if (o->type == PDF_NAME) {
                    if (offset < pdfi_array_size((pdf_array *)*Encoding))
                        code = pdfi_array_put(ctx, (pdf_array *)*Encoding, (uint64_t)offset, o);
                    pdfi_countdown(o);
                    offset++;
                    if (code < 0)
                        break;
                } else {
                    if (o->type == PDF_INT) {
                        offset = ((pdf_num *)o)->value.i;
                        pdfi_countdown(o);
                    } else {
                        code = gs_note_error(gs_error_typecheck);
                        pdfi_countdown(o);
                        break;
                    }
                }
            }
            pdfi_countdown(a);
            if (code < 0)
                return code;
        } else {
            return gs_note_error(gs_error_typecheck);
        }
    }
    return 0;
}

/*
 * Only suitable for simple fonts, I think, we just return the character code as a
 * glyph ID.
 */
gs_glyph pdfi_encode_char(gs_font * pfont, gs_char chr, gs_glyph_space_t not_used)
{
    return chr;
}

/* Get the unicode valude for a glyph FIXME - not written yet
 */
int pdfi_decode_glyph(gs_font * font, gs_glyph glyph, int ch, ushort *unicode_return, unsigned int length)
{
    return 0;
}

int pdfi_glyph_index(gs_font *pfont, byte *str, uint size, uint *glyph)
{
    int code = 0;
    pdf_font *font;

    font = (pdf_font *)pfont->client_data;
    code = pdfi_get_name_index(font->ctx, (char *)str, size, glyph);
    return code;
}

/*
 * For simple fonts (ie not CIDFonts), given a character code, look up the
 * Encoding array and return the glyph name
 */
int pdfi_glyph_name(gs_font * pfont, gs_glyph glyph, gs_const_string * pstr)
{
    int code = 0;
    unsigned int index = 0;
    pdf_font *font;
    pdf_name *GlyphName = NULL;

    font = (pdf_font *)pfont->client_data;

    if (glyph > gs_c_min_std_encoding_glyph && glyph < GS_MIN_CID_GLYPH) {
        code = gs_c_glyph_name(glyph, pstr);
    }
    else {
        if (font->Encoding != NULL)
            code = pdfi_array_get(font->ctx, font->Encoding, (uint64_t)glyph, (pdf_obj **)&GlyphName);
        if (code < 0 && !font->fake_glyph_names)
            return code;
        /* For the benefit of the vector devices, if a glyph index is outside the encoding, we create a fake name */
        if (GlyphName == NULL || GlyphName->type == PDF_NULL) {
            int i;
            char cid_name[5 + sizeof(gs_glyph) * 3 + 1];
            for (i = 0; i < font->LastChar; i++)
                if (font->fake_glyph_names[i].data == NULL) break;

             if (i == font->LastChar) return_error(gs_error_invalidfont);

             gs_sprintf(cid_name, "glyph%lu", (ulong) glyph);

             pstr->data = font->fake_glyph_names[i].data =
                           gs_alloc_bytes(OBJ_MEMORY(font), strlen(cid_name) + 1, "pdfi_glyph_name: fake name");
             if (font->fake_glyph_names[i].data == NULL)
                 return_error(gs_error_VMerror);
             pstr->size = font->fake_glyph_names[i].size = strlen(cid_name);
             memcpy(font->fake_glyph_names[i].data, cid_name, strlen(cid_name) + 1);
             return 0;
        }

        code = pdfi_get_name_index(font->ctx, (char *)GlyphName->data, GlyphName->length, &index);
        if (code < 0) {
            pdfi_countdown(GlyphName);
            return code;
        }

        code = pdfi_name_from_index(font->ctx, index, (unsigned char **)&pstr->data, &pstr->size);
        pdfi_countdown(GlyphName);
    }
    return code;
}


static int pdfi_global_glyph_code(const gs_font *pfont, gs_const_string *gstr, gs_glyph *pglyph)
{
    int code = 0;
    if (pfont->FontType == ft_encrypted) {
        code = pdfi_t1_global_glyph_code(pfont, gstr, pglyph);
    }
    else if (pfont->FontType == ft_encrypted2) {
        code = pdfi_cff_global_glyph_code(pfont, gstr, pglyph);
    }
    else {
        code = gs_note_error(gs_error_invalidaccess);
    }
    return code;
}

int pdfi_init_font_directory(pdf_context *ctx)
{
    ctx->font_dir = gs_font_dir_alloc2(ctx->memory, ctx->memory);
    if (ctx->font_dir == NULL) {
        return_error(gs_error_VMerror);
    }
    ctx->font_dir->global_glyph_code = pdfi_global_glyph_code;
    return 0;
}

/* Loads a (should be!) non-embedded font by name
   Only currently works for the Type 1 font set from romfs.
 */
int pdfi_load_font_by_name_string(pdf_context *ctx, const byte *fontname, size_t length,
                                  pdf_obj **ppdffont)
{
    pdf_obj *fname = NULL;
    pdf_obj *fontobjtype = NULL;
    pdf_dict *fdict = NULL;
    int code;
    gs_font *pgsfont = NULL;
    const char *fs = "Font";

    code = pdfi_name_alloc(ctx, (byte *)fontname, length, &fname);
    if (code < 0)
        return code;
    pdfi_countup(fname);

    code = pdfi_name_alloc(ctx, (byte *)fs, strlen(fs), &fontobjtype);
    if (code < 0)
        goto exit;
    pdfi_countup(fontobjtype);

    code = pdfi_dict_alloc(ctx, 1, &fdict);
    if (code < 0)
        goto exit;
    pdfi_countup(fdict);

    code = pdfi_dict_put(ctx, fdict, "BaseFont", fname);
    if (code < 0)
        goto exit;

    code = pdfi_dict_put(ctx, fdict, "Type", fontobjtype);
    if (code < 0)
        goto exit;

    code = pdfi_load_font(ctx, NULL, NULL, fdict, &pgsfont, false);
    if (code < 0)
        goto exit;

    *ppdffont = (pdf_obj *)pgsfont->client_data;

 exit:
    pdfi_countdown(fontobjtype);
    pdfi_countdown(fname);
    pdfi_countdown(fdict);
    return code;
}

/* Convenience function for using fonts created by
   pdfi_load_font_by_name_string
 */
int pdfi_set_font_internal(pdf_context *ctx, pdf_obj *fontobj, double point_size)
{
    int code;
    pdf_font *pdffont = (pdf_font *)fontobj;

    if (pdffont->type != PDF_FONT || pdffont->pfont == NULL)
        return_error(gs_error_invalidfont);

    code = gs_setPDFfontsize(ctx->pgs, point_size);
    if (code < 0)
        return code;

    return pdfi_gs_setfont(ctx, (gs_font *)pdffont->pfont);
}

/* Convenience function for setting font by name
 * Keeps one ref to the font, which will be in the graphics state font ->client_data
 */
static int pdfi_font_set_internal_inner(pdf_context *ctx, const byte *fontname, size_t length,
                                        double point_size)
{
    int code = 0;
    pdf_obj *font = NULL;


    code = pdfi_load_font_by_name_string(ctx, fontname, length, &font);
    if (code < 0) goto exit;

    code = pdfi_set_font_internal(ctx, font, point_size);

 exit:
    if (code < 0)
        pdfi_countdown(font); /* Keep the ref if succeeded */
    return code;
}

int pdfi_font_set_internal_string(pdf_context *ctx, const char *fontname, double point_size)
{
    return pdfi_font_set_internal_inner(ctx, (const byte *)fontname, strlen(fontname), point_size);
}

int pdfi_font_set_internal_name(pdf_context *ctx, pdf_name *fontname, double point_size)
{
    return pdfi_font_set_internal_inner(ctx, fontname->data, fontname->length, point_size);
}
