/* Copyright (C) 2018-2025 Artifex Software, Inc.
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

/* Font operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_font_types.h"
#include "pdf_gstate.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_array.h"
#include "pdf_font.h"
#include "pdf_stack.h"
#include "pdf_misc.h"
#include "pdf_doc.h"
#include "pdf_font0.h"
#include "pdf_font1.h"
#include "pdf_font1C.h"
#include "pdf_font3.h"
#include "pdf_fontTT.h"
#include "pdf_fontmt.h"
#include "pdf_font0.h"
#include "pdf_fmap.h"
#include "gscencs.h"            /* For gs_c_known_encode and gs_c_glyph_name */
#include "gsagl.h"

#include "strmio.h"
#include "stream.h"
#include "gsstate.h"            /* For gs_setPDFfontsize() */

extern single_glyph_list_t SingleGlyphList[];

static int pdfi_gs_setfont(pdf_context *ctx, gs_font *pfont)
{
    int code = 0;
    pdfi_int_gstate *igs = (pdfi_int_gstate *)ctx->pgs->client_data;
    pdf_font *old_font = igs->current_font;

    code = gs_setfont(ctx->pgs, pfont);
    if (code >= 0) {
        igs->current_font = (pdf_font *)pfont->client_data;
        pdfi_countup(igs->current_font);
        pdfi_countdown(old_font);
    }
    return code;
}

static void pdfi_cache_resource_font(pdf_context *ctx, pdf_obj *fontdesc, pdf_obj *ppdffont)
{
    resource_font_cache_t *entry = NULL;
    int i;

    if (ctx->resource_font_cache == NULL) {
        ctx->resource_font_cache = (resource_font_cache_t *)gs_alloc_bytes(ctx->memory, RESOURCE_FONT_CACHE_BLOCK_SIZE * sizeof(pdfi_name_entry_t), "pdfi_cache_resource_font");
        if (ctx->resource_font_cache == NULL)
            return;
        ctx->resource_font_cache_size = RESOURCE_FONT_CACHE_BLOCK_SIZE;
        memset(ctx->resource_font_cache, 0x00, RESOURCE_FONT_CACHE_BLOCK_SIZE * sizeof(pdfi_name_entry_t));
        entry = &ctx->resource_font_cache[0];
    }

    for (i = 0; entry == NULL && i < ctx->resource_font_cache_size; i++) {
        if (ctx->resource_font_cache[i].pdffont == NULL) {
            entry = &ctx->resource_font_cache[i];
        }
        else if (i == ctx->resource_font_cache_size - 1) {
            entry = (resource_font_cache_t *)gs_resize_object(ctx->memory, ctx->resource_font_cache, sizeof(pdfi_name_entry_t) * (ctx->resource_font_cache_size + RESOURCE_FONT_CACHE_BLOCK_SIZE), "pdfi_cache_resource_font");
            if (entry == NULL)
                break;
            memset(entry + ctx->resource_font_cache_size, 0x00, RESOURCE_FONT_CACHE_BLOCK_SIZE * sizeof(pdfi_name_entry_t));
            ctx->resource_font_cache = entry;
            entry = &ctx->resource_font_cache[ctx->resource_font_cache_size];
            ctx->resource_font_cache_size += RESOURCE_FONT_CACHE_BLOCK_SIZE;
        }
    }
    if (entry != NULL) {
        entry->desc_obj_num = fontdesc->object_num;
        entry->pdffont = ppdffont;
        pdfi_countup(ppdffont);
    }
}

void pdfi_purge_cache_resource_font(pdf_context *ctx)
{
    int i;
    for (i = 0; i < ctx->resource_font_cache_size; i++) {
       pdfi_countdown(ctx->resource_font_cache[i].pdffont);
    }
    gs_free_object(ctx->memory, ctx->resource_font_cache, "pdfi_purge_cache_resource_font");
    ctx->resource_font_cache = NULL;
    ctx->resource_font_cache_size = 0;
}

static int pdfi_find_cache_resource_font(pdf_context *ctx, pdf_obj *fontdesc, pdf_obj **ppdffont)
{
    int i, code = gs_error_undefined;
    for (i = 0; i < ctx->resource_font_cache_size; i++) {
        if (ctx->resource_font_cache[i].desc_obj_num == fontdesc->object_num) {
           *ppdffont = ctx->resource_font_cache[i].pdffont;
           pdfi_countup(*ppdffont);
           code = 0;
           break;
        }
    }
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
  {DEFINE_NAME_LEN(Symbol)},
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

    if (basefont != NULL && pdfi_type_of(basefont) == PDF_NAME) {
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

    if (pdfont->LastChar <  pdfont->FirstChar || pdfont->Widths == NULL)
        return 0; /* Technically invalid - carry on, hope for the best */

    /* For "best" results, restrict to what we *hope* are A-Z,a-z */
    sindex = pdfont->FirstChar < 96 ? 96 : pdfont->FirstChar;
    lindex = pdfont->LastChar > 122 ? 123 : pdfont->LastChar + 1;

    for (i = sindex; i < lindex; i++) {
        gs_glyph_info_t ginfo = {0};
        gs_glyph g;
        g = pbfont->procs.encode_char((gs_font *)pbfont, i, GLYPH_SPACE_NAME);

        /* We're only interested in non-zero Widths entries for glyphs that actually exist in the font */
        if (g != GS_NO_GLYPH && pdfont->Widths[i - pdfont->FirstChar] != 0.0
          && (*pbfont->procs.glyph_info)((gs_font *)pbfont, g, NULL, GLYPH_INFO_WIDTH0, &ginfo) >= 0) {
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

/* Print a name object to stdout */
static void pdfi_print_font_name(pdf_context *ctx, pdf_name *n)
{
    if (ctx->args.QUIET != true)
        (void)outwrite(ctx->memory, (const char *)n->data, n->length);
}

static void pdfi_print_font_string(pdf_context *ctx, pdf_string *s)
{
    if (ctx->args.QUIET != true)
        (void)outwrite(ctx->memory, (const char *)s->data, s->length);
}

/* Print a null terminated string to stdout */
static void pdfi_print_cstring(pdf_context *ctx, const char *str)
{
    if (ctx->args.QUIET != true)
        (void)outwrite(ctx->memory, str, strlen(str));
}

/* Call with a CIDFont name to try to find the CIDFont on disk
   call if with ffname NULL to load the default fallback CIDFont
   substitue
   Currently only loads substitute - DroidSansFallback
 */
static int
pdfi_open_CIDFont_substitute_file(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *fontdesc, bool fallback, byte ** buf, int64_t * buflen, int *findex)
{
    int code = 0;
    char fontfname[gp_file_name_sizeof];
    stream *s;
    pdf_name *cidname = NULL;
    gs_const_string fname;

    (void)pdfi_dict_get(ctx, font_dict, "BaseFont", (pdf_obj **)&cidname);

    if (fallback == true) {
        pdf_string *mname = NULL;
        pdf_dict *csi = NULL;
        pdf_name *fbname;
        const char *cidfbstr = "CIDFallBack";

        code = pdfi_fontmap_lookup_cidfont(ctx, font_dict, NULL, (pdf_obj **)&mname, findex);

        if (mname == NULL || pdfi_type_of(mname) != PDF_STRING) {
            pdfi_countdown(mname);

            code = pdfi_object_alloc(ctx, PDF_NAME, strlen(cidfbstr), (pdf_obj **)&fbname);
            if (code >= 0) {
                pdfi_countup(fbname);
                memcpy(fbname->data, cidfbstr, strlen(cidfbstr));
                code = pdfi_fontmap_lookup_cidfont(ctx, font_dict, fbname, (pdf_obj **)&mname, findex);
                pdfi_countdown(fbname);
            }

            if (code < 0 || pdfi_type_of(mname) != PDF_STRING) {
                pdfi_countdown(mname);
                mname = NULL;
                code = pdfi_dict_get(ctx, font_dict, "CIDSystemInfo", (pdf_obj **)&csi);
                if (code >= 0 && pdfi_type_of(csi) == PDF_DICT) {
                    pdf_string *csi_reg = NULL, *csi_ord = NULL;

                    if (pdfi_dict_get(ctx, csi, "Registry", (pdf_obj **)&csi_reg) >= 0
                     && pdfi_dict_get(ctx, csi, "Ordering", (pdf_obj **)&csi_ord) >= 0
                     && pdfi_type_of(csi_reg) == PDF_STRING && pdfi_type_of(csi_ord) == PDF_STRING
                     && csi_reg->length + csi_ord->length + 1 < gp_file_name_sizeof - 1) {
                        pdf_name *reg_ord;
                        memcpy(fontfname, csi_reg->data, csi_reg->length);
                        memcpy(fontfname + csi_reg->length, "-", 1);
                        memcpy(fontfname + csi_reg->length + 1, csi_ord->data, csi_ord->length);
                        fontfname[csi_reg->length + csi_ord->length + 1] = '\0';

                        code = pdfi_name_alloc(ctx, (byte *)fontfname, strlen(fontfname), (pdf_obj **) &reg_ord);
                        if (code >= 0) {
                            pdfi_countup(reg_ord);
                            code = pdfi_fontmap_lookup_cidfont(ctx, font_dict, reg_ord, (pdf_obj **)&mname, findex);
                            pdfi_countdown(reg_ord);
                        }
                    }
                    pdfi_countdown(csi_reg);
                    pdfi_countdown(csi_ord);
                }
                pdfi_countdown(csi);
            }

            if (mname == NULL || pdfi_type_of(mname) != PDF_STRING) {
                pdfi_countdown(mname);
                mname = NULL;
                code = pdfi_fontmap_lookup_cidfont(ctx, font_dict, NULL, (pdf_obj **)&mname, findex);
            }
        }

        do {
            if (code < 0 || pdfi_type_of(mname) != PDF_STRING) {
                const char *fsprefix = "CIDFSubst/";
                int fsprefixlen = strlen(fsprefix);
                const char *defcidfallack = "DroidSansFallback.ttf";
                int defcidfallacklen = strlen(defcidfallack);

                pdfi_countdown(mname);

                if (ctx->args.nocidfallback == true) {
                    code = gs_note_error(gs_error_invalidfont);
                }
                else {
                    if (ctx->args.cidfsubstpath.data == NULL) {
                        memcpy(fontfname, fsprefix, fsprefixlen);
                    }
                    else {
                        if (ctx->args.cidfsubstpath.size > gp_file_name_sizeof - 1) {
                            code = gs_note_error(gs_error_rangecheck);
                            if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_CONFIG, "pdfi_open_CIDFont_substitute_file", "CIDFSubstPath parameter too long")) < 0)
                                goto exit;
                            memcpy(fontfname, fsprefix, fsprefixlen);
                        }
                        else {
                            memcpy(fontfname, ctx->args.cidfsubstpath.data, ctx->args.cidfsubstpath.size);
                            fsprefixlen = ctx->args.cidfsubstpath.size;
                        }
                    }

                    if (ctx->args.cidfsubstfont.data == NULL) {
                        int len = 0;
                        if (gp_getenv("CIDFSUBSTFONT", (char *)0, &len) < 0) {
                            if (len + fsprefixlen + 1 > gp_file_name_sizeof) {
                                code = gs_note_error(gs_error_rangecheck);
                                if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_CONFIG, "pdfi_open_CIDFont_substitute_file", "CIDFSUBSTFONT environment variable too long")) < 0)
                                    goto exit;

                                if (defcidfallacklen + fsprefixlen > gp_file_name_sizeof - 1) {
                                    pdfi_set_warning(ctx, code, NULL, W_PDF_BAD_CONFIG, "pdfi_open_CIDFont_substitute_file", "CIDFSubstPath parameter too long");
                                    memcpy(fontfname, defcidfallack, defcidfallacklen);
                                    fsprefixlen = 0;
                                } else
                                    memcpy(fontfname + fsprefixlen, defcidfallack, defcidfallacklen);
                            }
                            else {
                                (void)gp_getenv("CIDFSUBSTFONT", (char *)(fontfname + fsprefixlen), &defcidfallacklen);
                            }
                        }
                        else {
                            if (fsprefixlen + defcidfallacklen > gp_file_name_sizeof - 1) {
                                code = gs_note_error(gs_error_rangecheck);
                                if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_CONFIG, "pdfi_open_CIDFont_substitute_file", "CIDFSUBSTFONT environment variable too long")) < 0)
                                    goto exit;

                                memcpy(fontfname, defcidfallack, defcidfallacklen);
                                /* cidfsubstfont should either be a font name we find in the search path(s)
                                   or an absolute path.
                                 */
                                fsprefixlen = 0;
                            }
                            else {
                                memcpy(fontfname + fsprefixlen, defcidfallack, defcidfallacklen);
                            }
                        }
                    }
                    else {
                        if (ctx->args.cidfsubstfont.size + fsprefixlen > gp_file_name_sizeof - 1) {
                            code = gs_note_error(gs_error_rangecheck);
                            if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_CONFIG, "pdfi_open_CIDFont_substitute_file", "CIDFSubstFont parameter too long")) < 0)
                                goto exit;

                            if (fsprefixlen + defcidfallacklen > gp_file_name_sizeof - 1) {
                                pdfi_set_warning(ctx, code, NULL, W_PDF_BAD_CONFIG, "pdfi_open_CIDFont_substitute_file", "CIDFSubstPath parameter too long");
                                memcpy(fontfname, defcidfallack, defcidfallacklen);
                                fsprefixlen = 0;
                            }
                            else
                                memcpy(fontfname + fsprefixlen, defcidfallack, defcidfallacklen);
                        }
                        else {
                            if (ctx->args.cidfsubstfont.size > gp_file_name_sizeof - 1) {
                                code = gs_note_error(gs_error_rangecheck);
                                if ((code = pdfi_set_warning_stop(ctx, code, NULL, W_PDF_BAD_CONFIG, "pdfi_open_CIDFont_substitute_file", "CIDFSubstFont parameter too long")) < 0)
                                    goto exit;

                                memcpy(fontfname, defcidfallack, defcidfallacklen);
                                defcidfallacklen = ctx->args.cidfsubstfont.size;
                                /* cidfsubstfont should either be a font name we find in the search path(s)
                                   or an absolute path.
                                 */
                                fsprefixlen = 0;
                            }
                            else {
                                memcpy(fontfname, ctx->args.cidfsubstfont.data, ctx->args.cidfsubstfont.size);
                                defcidfallacklen = ctx->args.cidfsubstfont.size;
                                /* cidfsubstfont should either be a font name we find in the search path(s)
                                   or an absolute path.
                                 */
                                fsprefixlen = 0;
                            }
                        }
                    }
                    fontfname[fsprefixlen + defcidfallacklen] = '\0';

                    code = pdfi_open_resource_file(ctx, fontfname, strlen(fontfname), &s);
                    if (code < 0) {
                        pdf_name *fn;
                        code = pdfi_object_alloc(ctx, PDF_NAME, strlen(fontfname), (pdf_obj **)&fn);
                        if (code < 0) {
                            code = gs_note_error(gs_error_invalidfont);
                        }
                        else {
                            pdfi_countup(fn);
                            memcpy(fn->data, fontfname, strlen(fontfname));
                            pdfi_countdown(mname);
                            mname = NULL;
                            code = pdfi_fontmap_lookup_cidfont(ctx, font_dict, fn, (pdf_obj **)&mname, findex);
                            pdfi_countdown(fn);
                            if (code < 0)
                                code = gs_note_error(gs_error_invalidfont);
                            else
                                continue;
                        }
                    }
                    else {
                        if (cidname) {
                            pdfi_print_cstring(ctx, "Loading CIDFont ");
                            pdfi_print_font_name(ctx, (pdf_name *)cidname);
                            pdfi_print_cstring(ctx, " substitute from ");
                        }
                        else {
                            pdfi_print_cstring(ctx, "Loading nameless CIDFont from ");
                        }
                        sfilename(s, &fname);
                        if (fname.size < gp_file_name_sizeof) {
                            memcpy(fontfname, fname.data, fname.size);
                            fontfname[fname.size] = '\0';
                        }
                        else {
                            strcpy(fontfname, "unnamed file");
                        }
                        pdfi_print_cstring(ctx, fontfname);
                        pdfi_print_cstring(ctx, "\n");


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
                        break;
                    }
                }
            }
            else {
                if (mname->length + 1 > gp_file_name_sizeof) {
                    pdfi_countdown(mname);
                    return_error(gs_error_invalidfont);
                }
                memcpy(fontfname, mname->data, mname->length);
                fontfname[mname->length] = '\0';
                code = pdfi_open_resource_file(ctx, (const char *)fontfname, mname->length, &s);
                pdfi_countdown(mname);
                if (code < 0) {
                    code = gs_note_error(gs_error_invalidfont);
                }
                else {
                    if (cidname) {
                        pdfi_print_cstring(ctx, "Loading CIDFont ");
                        pdfi_print_font_name(ctx, (pdf_name *)cidname);
                        pdfi_print_cstring(ctx, " (or substitute) from ");
                    }
                    else {
                        pdfi_print_cstring(ctx, "Loading nameless CIDFont from ");
                    }
                    sfilename(s, &fname);
                    if (fname.size < gp_file_name_sizeof) {
                        memcpy(fontfname, fname.data, fname.size);
                        fontfname[fname.size] = '\0';
                    }
                    else {
                        strcpy(fontfname, "unnamed file");
                    }
                    pdfi_print_cstring(ctx, fontfname);
                    pdfi_print_cstring(ctx, "\n");
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
                    break;
                }
            }
        } while (code >= 0);
    }
    else {
        const char *fsprefix = "CIDFont/";
        const int fsprefixlen = strlen(fsprefix);

        if (cidname == NULL || pdfi_type_of(cidname) != PDF_NAME
         || fsprefixlen + cidname->length >= gp_file_name_sizeof) {
            code = gs_note_error(gs_error_invalidfont);
            goto exit;
        }

        memcpy(fontfname, fsprefix, fsprefixlen);
        memcpy(fontfname + fsprefixlen, cidname->data, cidname->length);
        fontfname[fsprefixlen + cidname->length] = '\0';

        code = pdfi_open_resource_file(ctx, fontfname, strlen(fontfname), &s);
        if (code < 0) {
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
                code = gs_note_error(gs_error_invalidfont);
            }
            sfclose(s);
        }
    }

exit:
    if (cidname != NULL)
        pdfi_countdown(cidname);

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

static int strncmp_ignore_space(const char *a, const char *b)
{
    while (1)
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
    return 0; /* Shouldn't happen */
}

static const char *pdfi_clean_font_name(const char *fontname)
{
    int i, k;
    for (i = 0; i < (sizeof(pdfi_base_font_names)/sizeof(pdfi_base_font_names[0])); i++) {
        for (k = 0; pdfi_base_font_names[i][k]; k++) {
            if (!strncmp_ignore_space(pdfi_base_font_names[i][k], (const char *)fontname))
                return pdfi_base_font_names[i][0];
        }
    }
    return NULL;
}

static int pdfi_font_substitute_by_flags(pdf_context *ctx, unsigned int flags, char **name, int *namelen)
{
    bool fixed = ((flags & pdfi_font_flag_fixed) != 0);
    bool serif = ((flags & pdfi_font_flag_serif) != 0);
    bool italic = ((flags & pdfi_font_flag_italic) != 0);
    bool bold = ((flags & pdfi_font_flag_forcebold) != 0);
    int code = 0;

    if (ctx->args.defaultfont_is_name == true && ctx->args.defaultfont.size == 4
        && !memcmp(ctx->args.defaultfont.data, "None", 4)) {
       *name = NULL;
       *namelen = 0;
       code = gs_error_invalidfont;
    }
    else if (ctx->args.defaultfont.data != NULL && ctx->args.defaultfont.size > 0) {
        *name = (char *)ctx->args.defaultfont.data;
        *namelen = ctx->args.defaultfont.size;
    }
    else if (fixed) {
        if (bold) {
            if (italic) {
                *name = (char *)pdfi_base_font_names[3][0];
                *namelen = strlen(*name);
            }
            else {
                *name = (char *)pdfi_base_font_names[1][0];
                *namelen = strlen(*name);
            }
        }
        else {
            if (italic) {
                *name = (char *)pdfi_base_font_names[2][0];
                *namelen = strlen(*name);
            }
            else {
                *name = (char *)pdfi_base_font_names[0][0];
                *namelen = strlen(*name);
            }
        }
    }
    else if (serif) {
        if (bold) {
            if (italic) {
                *name = (char *)pdfi_base_font_names[11][0];
                *namelen = strlen(*name);
            }
            else {
                *name = (char *)pdfi_base_font_names[9][0];
                *namelen = strlen(*name);
            }
        }
        else {
            if (italic) {
                *name = (char *)pdfi_base_font_names[10][0];
                *namelen = strlen(*name);
            }
            else {
                *name = (char *)pdfi_base_font_names[8][0];
                *namelen = strlen(*name);
            }
        }
    } else {
        if (bold) {
            if (italic) {
                *name = (char *)pdfi_base_font_names[7][0];
                *namelen = strlen(*name);
            }
            else {
                *name = (char *)pdfi_base_font_names[5][0];
                *namelen = strlen(*name);
            }
        }
        else {
            if (italic) {
                *name = (char *)pdfi_base_font_names[6][0];
                *namelen = strlen(*name);
            }
            else {
                *name = (char *)pdfi_base_font_names[4][0];
                *namelen = strlen(*name);
            }
        }
    }
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

static int pdfi_copy_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont)
{
    int code;
    if (pdfi_type_of(spdffont) != PDF_FONT)
        return_error(gs_error_typecheck);

    switch(spdffont->pdfi_font_type) {
        case e_pdf_font_type1:
          code = pdfi_copy_type1_font(ctx, spdffont, font_dict, tpdffont);
          break;
        case e_pdf_font_cff:
          code = pdfi_copy_cff_font(ctx, spdffont, font_dict, tpdffont);
          break;
        case e_pdf_font_truetype:
          code = pdfi_copy_truetype_font(ctx, spdffont, font_dict, tpdffont);
          break;
        case e_pdf_font_microtype:
          code = pdfi_copy_microtype_font(ctx, spdffont, font_dict, tpdffont);
          break;
        default:
            return_error(gs_error_invalidfont);
    }
    return code;
}

enum {
  font_embedded = 0,
  font_from_file = 1,
  font_substitute = 2
};

static int pdfi_load_font_buffer(pdf_context *ctx, byte *fbuf, int fbuflen, int fftype, pdf_name *Subtype, int findex, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_dict *font_dict, pdf_font **ppdffont, bool cidfont)
{
    int code = gs_error_invalidfont;
    if (fbuf != NULL) {
        /* First, see if we can glean the type from the magic number */
        int sftype = pdfi_fonttype_picker(fbuf, fbuflen);
        if (sftype == no_type_font) {
            if (fftype != no_type_font)
                sftype = fftype;
            else {
                /* If we don't have a Subtype, can't work it out, try Type 1 */
                if (Subtype == NULL || pdfi_name_is(Subtype, "Type1") || pdfi_name_is(Subtype, "MMType1"))
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
                code = pdfi_read_type1_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, fbuf, fbuflen, ppdffont);
                fbuf = NULL;
                break;
            case cff_font:
                code = pdfi_read_cff_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, fbuf, fbuflen, cidfont, ppdffont);
                fbuf = NULL;
                break;
            case tt_font:
                {
                    if (cidfont)
                        code = pdfi_read_cidtype2_font(ctx, font_dict, stream_dict, page_dict, fbuf, fbuflen, findex, ppdffont);
                    else
                        code = pdfi_read_truetype_font(ctx, font_dict, stream_dict, page_dict, fbuf, fbuflen, findex, ppdffont);
                    fbuf = NULL;
                }
                break;
            default:
                gs_free_object(ctx->memory, fbuf, "pdfi_load_font_buffer(fbuf)");
                code = gs_note_error(gs_error_invalidfont);
        }
    }
    return code;
}

static int pdfi_load_font_file(pdf_context *ctx, int fftype, pdf_name *Subtype, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_dict *font_dict, pdf_dict *fontdesc, bool substitute, pdf_font **ppdffont)
{
    int code;
    char fontfname[gp_file_name_sizeof];
    pdf_obj *basefont = NULL, *mapname = NULL;
    pdf_obj *fontname = NULL;
    stream *s;
    const char *fn;
    int findex = 0;
    byte *buf;
    int buflen;
    pdf_font *pdffont = NULL;
    pdf_font *substpdffont = NULL;
    bool f_retry = true;

    code = pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &basefont);
    if (substitute == false && (code < 0 || basefont == NULL || ((pdf_name *)basefont)->length == 0)) {
        pdfi_countdown(basefont);
        return_error(gs_error_invalidfont);
    }

    if (substitute == true) {
        char *fbname;
        int fbnamelen;
        int64_t flags = 0;
        if (fontdesc != NULL) {
            (void)pdfi_dict_get_int(ctx, fontdesc, "Flags", &flags);
        }
        code = pdfi_font_substitute_by_flags(ctx, (int)flags, &fbname, &fbnamelen);
        if (code < 0)
            return code;

        code = pdfi_name_alloc(ctx, (byte *)fbname, strlen(fbname), (pdf_obj **) &fontname);
        if (code < 0)
            return code;
        pdfi_countup(fontname);
    }
    else {
        fontname = basefont;
        pdfi_countup(fontname);
    }

    if (((pdf_name *)fontname)->length < gp_file_name_sizeof) {
        memcpy(fontfname, ((pdf_name *)fontname)->data, ((pdf_name *)fontname)->length);
        fontfname[((pdf_name *)fontname)->length] = '\0';
        pdfi_countdown(fontname);

        code = pdfi_name_alloc(ctx, (byte *)fontfname, strlen(fontfname), (pdf_obj **) &fontname);
        if (code < 0)
            return code;
        pdfi_countup(fontname);
    }

    do {
        code = pdfi_fontmap_lookup_font(ctx, font_dict, (pdf_name *) fontname, &mapname, &findex);
        if (code < 0) {
            if (((pdf_name *)fontname)->length < gp_file_name_sizeof) {
                memcpy(fontfname, ((pdf_name *)fontname)->data, ((pdf_name *)fontname)->length);
                fontfname[((pdf_name *)fontname)->length] = '\0';
                fn = pdfi_clean_font_name(fontfname);
                if (fn != NULL) {
                    pdfi_countdown(fontname);

                    code = pdfi_name_alloc(ctx, (byte *)fn, strlen(fn), (pdf_obj **) &fontname);
                    if (code < 0)
                        return code;
                    pdfi_countup(fontname);
                }
            }
            code = pdfi_fontmap_lookup_font(ctx, font_dict, (pdf_name *) fontname, &mapname, &findex);
            if (code < 0) {
                mapname = fontname;
                pdfi_countup(mapname);
                code = 0;
            }
        }
        if (pdfi_type_of(mapname) == PDF_FONT) {
            pdffont = (pdf_font *)mapname;
            pdfi_countup(pdffont);
            break;
        }
        if (pdfi_type_of(mapname) == PDF_NAME || pdfi_type_of(mapname) == PDF_STRING) {
            pdf_name *mname = (pdf_name *) mapname;
            if (mname->length + 1 < gp_file_name_sizeof) {
                memcpy(fontfname, mname->data, mname->length);
                fontfname[mname->length] = '\0';
            }
            else {
                pdfi_countdown(mapname);
                pdfi_countdown(fontname);
                return_error(gs_error_invalidfileaccess);
            }
        }
        else {
            pdfi_countdown(mapname);
            pdfi_countdown(fontname);
            return_error(gs_error_invalidfileaccess);
        }

        if (ctx->pdf_substitute_fonts != NULL) {
            code = pdfi_dict_knownget_type(ctx, ctx->pdf_substitute_fonts, fontfname, PDF_FONT, (pdf_obj **)&pdffont);
            if (code == 1 && pdffont->filename == NULL) {
                pdfi_countdown(pdffont);
                pdffont = NULL;
                code = 0;
            }
        }
        else
            code = 0;

        if (code != 1) {
            code = pdfi_open_font_file(ctx, fontfname, strlen(fontfname), &s);
            if (code < 0 && f_retry && pdfi_type_of(mapname) == PDF_NAME) {
                pdfi_countdown(fontname);
                fontname = mapname;
                mapname = NULL;
                f_retry = false;
                continue;
            }
            if (code >= 0) {
                gs_const_string fname;

                sfilename(s, &fname);
                if (fname.size < gp_file_name_sizeof) {
                    memcpy(fontfname, fname.data, fname.size);
                    fontfname[fname.size] = '\0';
                }
                else {
                    strcpy(fontfname, "unnamed file");
                }
                sfseek(s, 0, SEEK_END);
                buflen = sftell(s);
                sfseek(s, 0, SEEK_SET);
                buf = gs_alloc_bytes(ctx->memory, buflen, "pdfi_open_t1_font_file(buf)");
                if (buf != NULL) {
                    sfread(buf, 1, buflen, s);
                }
                else {
                    code = gs_note_error(gs_error_VMerror);
                }
                sfclose(s);
                /* Buffer owership moves to the font object */
                code = pdfi_load_font_buffer(ctx, buf, buflen, no_type_font, NULL, findex, stream_dict, page_dict, NULL, &pdffont, false);
                if (code >= 0) {
                    pdffont->filename = NULL;
                    code = pdfi_object_alloc(ctx, PDF_STRING, strlen(fontfname) , (pdf_obj **)&pdffont->filename);
                    if (code >= 0) {
                        pdfi_countup(pdffont->filename);
                        memcpy(pdffont->filename->data, fontfname, strlen(fontfname));
                        pdffont->filename->length = strlen(fontfname);
                    }

                    if (ctx->pdf_substitute_fonts == NULL) {
                        code = pdfi_dict_alloc(ctx, 16, &ctx->pdf_substitute_fonts);
                        if (code >= 0)
                            pdfi_countup(ctx->pdf_substitute_fonts);
                    }
                    if (ctx->pdf_substitute_fonts != NULL) {
                        if (pdfi_type_of(mapname) == PDF_STRING) {
                            pdf_name *n = NULL;
                            pdf_string *mn = (pdf_string *)mapname;

                            code = pdfi_name_alloc(ctx, mn->data, mn->length, (pdf_obj **)&n);
                            if (code >= 0) {
                                pdfi_countdown(mapname);
                                mapname = (pdf_obj *)n;
                                pdfi_countup(mapname);
                                code = 0;
                            }
                        }
                        else
                            code = 0;

                        if (code == 0)
                            (void)pdfi_dict_put_obj(ctx, ctx->pdf_substitute_fonts, mapname, (pdf_obj *)pdffont, true);
                        code = 0;
                    }
                }
            }
        }
        break;
    } while (1);

    if (code >= 0) {
        if (basefont) {
            pdfi_print_cstring(ctx, "Loading font ");
            pdfi_print_font_name(ctx, (pdf_name *)basefont);
            pdfi_print_cstring(ctx, " (or substitute) from ");
        }
        else {
            pdfi_print_cstring(ctx, "Loading nameless font from ");
        }
        pdfi_print_font_string(ctx, pdffont->filename);
        pdfi_print_cstring(ctx, "\n");

        code = pdfi_copy_font(ctx, pdffont, font_dict, &substpdffont);
        pdfi_countdown(pdffont);
    }

    *ppdffont = substpdffont;

    pdfi_countdown(basefont);
    pdfi_countdown(mapname);
    pdfi_countdown(fontname);
    return code;
}

int pdfi_load_font(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, pdf_dict *font_dict, gs_font **ppfont, bool cidfont)
{
    int code;
    pdf_font *ppdffont = NULL;
    pdf_font *ppdfdescfont = NULL;
    pdf_name *Type = NULL;
    pdf_name *Subtype = NULL;
    pdf_dict *fontdesc = NULL;
    pdf_stream *fontfile = NULL;
    pdf_name *ffsubtype = NULL;
    int fftype = no_type_font;
    byte *fbuf = NULL;
    int64_t fbuflen = 0;
    int substitute = font_embedded;
    int findex = 0;

    code = pdfi_dict_get_type(ctx, font_dict, "Type", PDF_NAME, (pdf_obj **)&Type);
    if (code < 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_MISSINGTYPE, "pdfi_load_font", NULL)) < 0)
            goto exit;
    }
    else {
        if (!pdfi_name_is(Type, "Font")){
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }
    }
    code = pdfi_dict_get_type(ctx, font_dict, "Subtype", PDF_NAME, (pdf_obj **)&Subtype);
    if (code < 0) {
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_NO_SUBTYPE, "pdfi_load_font", NULL)) < 0)
            goto exit;
    }

    /* Beyond Type 0 and Type 3, there is no point trusting the Subtype key */
    if (Subtype != NULL && pdfi_name_is(Subtype, "Type0")) {
        if (cidfont == true) {
            code = gs_note_error(gs_error_invalidfont);
        }
        else {
            code = pdfi_read_type0_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &ppdffont);
        }
    }
    else if (Subtype != NULL && pdfi_name_is(Subtype, "Type3")) {
        code = pdfi_read_type3_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &ppdffont);
        if (code < 0)
            goto exit;
    }
    else {
        if (Subtype != NULL && !pdfi_name_is(Subtype, "Type1") && !pdfi_name_is(Subtype, "TrueType") && !pdfi_name_is(Subtype, "CIDFont")
            && !pdfi_name_is(Subtype, "CIDFontType2") && !pdfi_name_is(Subtype, "CIDFontType0") && !pdfi_name_is(Subtype, "MMType1")) {

            if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_typecheck), NULL, E_PDF_BAD_SUBTYPE, "pdfi_load_font", NULL)) < 0) {
                goto exit;
            }
        }
        /* We should always have a font descriptor here, but we have to carry on
           even if we don't
         */
        code = pdfi_dict_get_type(ctx, font_dict, "FontDescriptor", PDF_DICT, (pdf_obj **)&fontdesc);
        if (fontdesc != NULL && pdfi_type_of(fontdesc) == PDF_DICT) {
            pdf_obj *Name = NULL;

            code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontName", PDF_NAME, (pdf_obj **)&Name);
            if (code < 0)
                pdfi_set_warning(ctx, 0, NULL, W_PDF_FDESC_BAD_FONTNAME, "pdfi_load_font", "");
            pdfi_countdown(Name);
            Name = NULL;

            if (cidfont == true) {
                code = -1;
            }
            else {
                code = pdfi_find_cache_resource_font(ctx, (pdf_obj *)fontdesc, (pdf_obj **)&ppdfdescfont);
                if (code >= 0) {
                    code = pdfi_copy_font(ctx, ppdfdescfont, font_dict, &ppdffont);
                }
            }

            if (code < 0) {
                code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile", PDF_STREAM, (pdf_obj **)&fontfile);
                if (code >= 0)
                    fftype = type1_font;
                else {
                    code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile2", PDF_STREAM, (pdf_obj **)&fontfile);
                    fftype = tt_font;
                }
                if (code < 0) {
                    code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile3", PDF_STREAM, (pdf_obj **)&fontfile);
                    if (code >= 0 && fontfile != NULL) {
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
        }

        if (ppdffont == NULL) {
            if (fontfile != NULL) {
                code = pdfi_stream_to_buffer(ctx, (pdf_stream *) fontfile, &fbuf, &fbuflen);
                pdfi_countdown(fontfile);
                if (code < 0 || fbuflen == 0) {
                    char obj[129];
                    pdfi_print_cstring(ctx, "**** Warning: cannot process embedded stream for font object ");
                    gs_snprintf(obj, 128, "%d %d\n", (int)font_dict->object_num, (int)font_dict->generation_num);
                    pdfi_print_cstring(ctx, obj);
                    pdfi_print_cstring(ctx, "**** Attempting to load a substitute font.\n");
                    gs_free_object(ctx->memory, fbuf, "pdfi_load_font(fbuf)");
                    fbuf = NULL;
                    code = gs_note_error(gs_error_invalidfont);
                }
            }

            while (1) {
                if (fbuf != NULL) {
                    /* fbuf overship passes to pdfi_load_font_buffer() */
                    code = pdfi_load_font_buffer(ctx, fbuf, fbuflen, fftype, Subtype, findex, stream_dict, page_dict, font_dict, &ppdffont, cidfont);

                    if (code < 0 && substitute == font_embedded) {
                        char msg[129];

                        gs_snprintf(msg, 128, "Cannot process embedded stream for font object %d %d. Attempting to load a substitute font.\n", (int)font_dict->object_num, (int)font_dict->generation_num);

                        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_invalidfont), NULL, E_PDF_BAD_EMBEDDED_FONT, "pdfi_load_font", msg)) < 0) {
                            goto exit;
                        }
                        else {
                            code = gs_error_invalidfont;
                        }
                    }
                }
                else {
                    code = gs_error_invalidfont;
                }

                if (code < 0 && code != gs_error_VMerror && substitute == font_embedded) {
                    substitute = font_from_file;

                    if (cidfont == true) {
                        code =  pdfi_open_CIDFont_substitute_file(ctx, font_dict, fontdesc, false, &fbuf, &fbuflen, &findex);
                        if (code < 0) {
                            code =  pdfi_open_CIDFont_substitute_file(ctx, font_dict, fontdesc, true, &fbuf, &fbuflen, &findex);
                            substitute |= font_substitute;
                        }

                        if (code < 0)
                            goto exit;
                    }
                    else {
                        code = pdfi_load_font_file(ctx, no_type_font, Subtype, stream_dict, page_dict, font_dict, fontdesc, false, &ppdffont);
                        if (code < 0) {
                            code = pdfi_load_font_file(ctx, no_type_font, Subtype, stream_dict, page_dict, font_dict, fontdesc, true, &ppdffont);
                            substitute |= font_substitute;
                        }
                        break;
                    }
                    continue;
                }
                break;
            }
        }
    }
    if (ppdffont == NULL || code < 0) {
        *ppfont = NULL;
        code = gs_note_error(gs_error_invalidfont);
    }
    else {
        ppdffont->substitute = (substitute != font_embedded);

        if ((substitute & font_substitute) == font_substitute)
            code = pdfi_font_match_glyph_widths(ppdffont);
        else if (ppdffont->substitute != true && fontdesc != NULL && ppdfdescfont == NULL
            && (ppdffont->pdfi_font_type == e_pdf_font_type1 || ppdffont->pdfi_font_type == e_pdf_font_cff
            || ppdffont->pdfi_font_type == e_pdf_font_truetype)) {
            pdfi_cache_resource_font(ctx, (pdf_obj *)fontdesc, (pdf_obj *)ppdffont);
        }
        *ppfont = (gs_font *)ppdffont->pfont;
     }

exit:
    pdfi_countdown(ppdfdescfont);
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
    pdf_font *pdfif;

    switch (pdfi_type_of(font_dict)) {
        case PDF_FONT:
            pdfi_countup(font_dict);
            pfont = (gs_font *)((pdf_font *)font_dict)->pfont;
            code = 0;
            break;
        case PDF_DICT:
            code = pdfi_load_font(ctx, stream_dict, page_dict, font_dict, &pfont, false);
            break;
        default:
            code = gs_note_error(gs_error_typecheck);
            goto exit;
    }
    if (code < 0)
        goto exit;

    /* Everything looks good, set the font, unless it's the current font */
    if (pfont != ctx->pgs->font) {
        code = pdfi_gs_setfont(ctx, pfont);
    }
    pdfif = (pdf_font *)pfont->client_data;
    pdfi_countdown(pdfif);

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

    if (pdfi_type_of(fontname) != PDF_NAME) {
        /* Passing empty string here should fall back to a default font */
        return pdfi_font_set_internal_string(ctx, "", point_size);
    }

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
    pdfi_countdown(font_dict);
    return code;
}

int pdfi_get_cidfont_glyph_metrics(gs_font *pfont, gs_glyph cid, double *widths, bool vertical)
{
    pdf_font *pdffont = (pdf_font *)pfont->client_data;
    int i, code = 0;
    pdf_num *c = NULL, *c2 = NULL;
    pdf_obj *o = NULL;
    pdf_array *W = NULL, *W2 = NULL, *DW2 = NULL;
    double DW;

    if (pdffont->pdfi_font_type == e_pdf_cidfont_type0) {
        pdf_cidfont_type0 *cidfont = (pdf_cidfont_type0 *)pdffont;
        DW = cidfont->DW;
        DW2 = cidfont->DW2;
        W = cidfont->W;
        W2 = cidfont->W2;
    }
    else if (pdffont->pdfi_font_type == e_pdf_cidfont_type2) {
        pdf_cidfont_type2 *cidfont = (pdf_cidfont_type2 *)pdffont;
        DW = cidfont->DW;
        DW2 = cidfont->DW2;
        W = cidfont->W;
        W2 = cidfont->W2;
    }
    else {
        return_error(gs_error_invalidfont);
    }

    widths[GLYPH_W0_WIDTH_INDEX] = DW;
    widths[GLYPH_W0_HEIGHT_INDEX] = 0;
    if (W != NULL) {
        i = 0;

        while(1) {
            pdf_obj_type type;

            if (i + 1>= W->size) break;
            code = pdfi_array_get_type(pdffont->ctx, W, i, PDF_INT, (pdf_obj **)&c);
            if (code < 0) goto cleanup;

            code = pdfi_array_get(pdffont->ctx, W, i + 1, &o);
            if (code < 0) goto cleanup;

            type = pdfi_type_of(o);
            if (type == PDF_INT) {
                double d;
                c2 = (pdf_num *)o;
                o = NULL;
                if (i + 2 >= W->size){
                    /* We countdown and NULL c, c2 and o after exit from the loop
                     * in order to avoid doing so in the break statements
                     */
                    break;
                }

                code = pdfi_array_get_number(pdffont->ctx, W, i + 2, &d);
                if (code < 0) goto cleanup;
                if (cid >= c->value.i && cid <= c2->value.i) {
                    widths[GLYPH_W0_WIDTH_INDEX] = d;
                    widths[GLYPH_W0_HEIGHT_INDEX] = 0.0;
                    /* We countdown and NULL c, c2 and o after exit from the loop
                     * in order to avoid doing so in the break statements
                     */
                    break;
                }
                else {
                    i += 3;
                    pdfi_countdown(c2);
                    pdfi_countdown(c);
                    c = c2 = NULL;
                    continue;
                }
            }
            else if (type == PDF_ARRAY) {
                pdf_array *a = (pdf_array *)o;
                o = NULL;
                if (cid >= c->value.i && cid < c->value.i + a->size) {
                    code = pdfi_array_get_number(pdffont->ctx, a, cid - c->value.i, &widths[GLYPH_W0_WIDTH_INDEX]);
                    if (code >= 0) {
                        pdfi_countdown(a);
                        widths[GLYPH_W0_HEIGHT_INDEX] = 0.0;
                        /* We countdown and NULL c, c2 and o on exit from the loop
                         * in order to avoid doing so in the break statements
                         */
                        break;
                    }
                }
                pdfi_countdown(a);
                pdfi_countdown(c);
                c = NULL;
                i += 2;
                continue;
            }
            else {
                code = gs_note_error(gs_error_typecheck);
                goto cleanup;
            }
        }
        pdfi_countdown(c2);
        pdfi_countdown(c);
        pdfi_countdown(o);
        c = c2 = NULL;
        o = NULL;
    }

    if (vertical) {
        /* Default default <sigh>! */
        widths[GLYPH_W1_WIDTH_INDEX] = 0;
        widths[GLYPH_W1_HEIGHT_INDEX] = -1000.0;
        widths[GLYPH_W1_V_X_INDEX] = (widths[GLYPH_W0_WIDTH_INDEX] / 2.0);
        widths[GLYPH_W1_V_Y_INDEX] = 880.0;

        if (DW2 != NULL && pdfi_type_of(DW2) == PDF_ARRAY
            && DW2->size >= 2) {
            code = pdfi_array_get_number(pdffont->ctx, (pdf_array *)DW2, 0, &widths[GLYPH_W1_V_Y_INDEX]);
            if (code >= 0)
                code = pdfi_array_get_number(pdffont->ctx, (pdf_array *)DW2, 1, &widths[GLYPH_W1_HEIGHT_INDEX]);
            if (code >= 0) {
                widths[GLYPH_W1_V_X_INDEX] = widths[GLYPH_W0_WIDTH_INDEX] / 2.0;
                widths[GLYPH_W1_WIDTH_INDEX] = 0.0;
            }
        }
        if (W2 != NULL && pdfi_type_of(W2) == PDF_ARRAY) {
            i = 0;
            while(1) {
                pdf_obj_type type;
                if (i + 1 >= W2->size) break;
                (void)pdfi_array_get(pdffont->ctx, W2, i, (pdf_obj **)&c);
                if (pdfi_type_of(c) != PDF_INT) {
                    code = gs_note_error(gs_error_typecheck);
                    goto cleanup;
                }
                code = pdfi_array_get(pdffont->ctx, W2, i + 1, (pdf_obj **)&o);
                if (code < 0) goto cleanup;
                type = pdfi_type_of(o);
                if (type == PDF_INT) {
                    if (cid >= c->value.i && cid <= ((pdf_num *)o)->value.i) {
                        if (i + 4 >= W2->size) {
                            /* We countdown and NULL c, and o on exit from the function
                             * so we don't need to do so in the break statements
                             */
                            break;
                        }
                        code = pdfi_array_get_number(pdffont->ctx, W2, i + 2, &widths[GLYPH_W1_HEIGHT_INDEX]);
                        if (code < 0) goto cleanup;
                        code = pdfi_array_get_number(pdffont->ctx, W2, i + 3, &widths[GLYPH_W1_V_X_INDEX]);
                        if (code < 0) goto cleanup;
                        code = pdfi_array_get_number(pdffont->ctx, W2, i + 4, &widths[GLYPH_W1_V_Y_INDEX]);
                        if (code < 0) goto cleanup;
                        /* We countdown and NULL c, and o on exit from the function
                         * so we don't need to do so in the break statements
                         */
                        break;
                    }
                    i += 5;
                }
                else if (type == PDF_ARRAY) {
                    pdf_array *a = (pdf_array *)o;
                    int l = a->size - (a->size % 3);
                    o = NULL;
                    if (cid >= c->value.i && cid < c->value.i + (l / 3)) {
                        int index = (cid - c->value.i) * 3;
                        code = pdfi_array_get_number(pdffont->ctx, a, index, &widths[GLYPH_W1_HEIGHT_INDEX]);
                        if (code < 0) {
                            pdfi_countdown(a);
                            goto cleanup;
                        }
                        code = pdfi_array_get_number(pdffont->ctx, a, index + 1, &widths[GLYPH_W1_V_X_INDEX]);
                        if (code < 0) {
                            pdfi_countdown(a);
                            goto cleanup;
                        }
                        code = pdfi_array_get_number(pdffont->ctx, a, index + 2, &widths[GLYPH_W1_V_Y_INDEX]);
                        pdfi_countdown(a);
                        if (code < 0) goto cleanup;

                        /* We countdown and NULL c, and o on exit from the function
                         * so we don't need to do so in the break statements
                         */
                        break;
                    } else
                        pdfi_countdown(a);
                    i += 2;
                }
                else {
                    code = gs_note_error(gs_error_typecheck);
                    goto cleanup;
                }
                pdfi_countdown(o);
                pdfi_countdown(c);
                o = NULL;
                c = NULL;
            }
        }
    }

cleanup:
    pdfi_countdown(c2);
    pdfi_countdown(c);
    pdfi_countdown(o);

    return code;
}

int pdfi_d0(pdf_context *ctx)
{
    int code = 0, gsave_level = 0;
    double width[2];

    if (ctx->text.inside_CharProc == false)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_NOTINCHARPROC, "pdfi_d0", NULL);

    ctx->text.CharProc_d_type = pdf_type3_d0;

    if (pdfi_count_stack(ctx) < 2) {
        code = gs_note_error(gs_error_stackunderflow);
        goto d0_error;
    }

    if (pdfi_type_of(ctx->stack_top[-1]) != PDF_INT && pdfi_type_of(ctx->stack_top[-1]) != PDF_REAL) {
        code = gs_note_error(gs_error_typecheck);
        goto d0_error;
    }
    if (pdfi_type_of(ctx->stack_top[-2]) != PDF_INT && pdfi_type_of(ctx->stack_top[-2]) != PDF_REAL) {
        code = gs_note_error(gs_error_typecheck);
        goto d0_error;
    }
    if(ctx->text.current_enum == NULL) {
        code = gs_note_error(gs_error_undefined);
        goto d0_error;
    }

    if (pdfi_type_of(ctx->stack_top[-2]) == PDF_INT)
        width[0] = (double)((pdf_num *)ctx->stack_top[-2])->value.i;
    else
        width[0] = ((pdf_num *)ctx->stack_top[-2])->value.d;
    if (pdfi_type_of(ctx->stack_top[-1]) == PDF_INT)
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
    int code = 0, gsave_level;
    double wbox[6];

    if (ctx->text.inside_CharProc == false)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_NOTINCHARPROC, "pdfi_d1", NULL);

    ctx->text.CharProc_d_type = pdf_type3_d1;

    code = pdfi_destack_reals(ctx, wbox, 6);
    if (code < 0)
        goto d1_error;

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

#if 0
    /* This code stops us caching the bitmap and filling it. This is required if the 'pdf-differences'
     * test Type3WordSpacing/Type3Test.pdf is to be rendered with a red stroke. Currently the cache
     * results in a bitmap which is filled with the current fill colour, stroke colours are not
     * implemented. I believe that, since the spec references the PLRM implementation of 'd1' that
     * this is correct. I further suspect that the authors of the test used viewers which do not implement
     * a cache at all, and redraw the glyph every time it is used. This has implications for other parts
     * of the graphics state, such as dash patterns, line joins and caps etc.
     *
     * The change in rendering results in many small differences, particularly at low resolution and PDF generated
     * by pdfwrite from PCL input.
     *
     * We need to clip the glyph description to the bounding box, because that's how the cache works with
     * a bitmap, if we don't do this then some glyphs are fully rendered which should only be partially
     * rendered. However, adding this causes two test files to render incorrectly (PostScript files
     * converted to PDF via pdfwrite). This is what decided me to revert this code.
     */
    code = gs_rectclip(ctx->pgs, (const gs_rect *)&wbox[2], 1);
    if (code < 0)
        goto d1_error;

    code = gs_text_setcharwidth(ctx->text.current_enum, wbox);
#else
    code = gs_text_setcachedevice(ctx->text.current_enum, wbox);
#endif

    /* See the comment immediately after gs_text_setcachedvice() in pdfi_d0 above */
    if (ctx->pgs->level > gsave_level)
        ctx->current_stream_save.gsave_level += ctx->pgs->level - gsave_level;

    if (code < 0)
        goto d1_error;
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
    code = pdfi_obj_to_real(ctx, point_arg, &point_size);
    if (code < 0)
        goto exit0;

    code = pdfi_load_resource_font(ctx, stream_dict, page_dict, fontname, point_size);

    /* If we failed to load font, try to load an internal one */
    if (code < 0)
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
        case e_pdf_font_microtype:
            return pdfi_free_font_microtype((pdf_obj *)font);
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

/* Assumes Encoding is an array, and parameters *ind and *near_ind set to ENCODING_INDEX_UNKNOWN */
static void
pdfi_gs_simple_font_encoding_indices(pdf_context *ctx, pdf_array *Encoding, gs_encoding_index_t *ind, gs_encoding_index_t *near_ind)
{
    uint esize = Encoding->size;
    uint best = esize / 3;	/* must match at least this many */
    int i, index, near_index = ENCODING_INDEX_UNKNOWN, code;

    for (index = 0; index < NUM_KNOWN_REAL_ENCODINGS; ++index) {
        uint match = esize;

        for (i = esize; --i >= 0;) {
            gs_const_string rstr;
            pdf_name *ename;

            code = pdfi_array_get_type(ctx, Encoding, (uint64_t)i, PDF_NAME, (pdf_obj **)&ename);
            if (code < 0) {
                return;
            }

            gs_c_glyph_name(gs_c_known_encode((gs_char)i, index), &rstr);
            if (rstr.size == ename->length &&
                !memcmp(rstr.data, ename->data, rstr.size)) {
                pdfi_countdown(ename);
                continue;
            }
            pdfi_countdown(ename);
            if (--match <= best) {
                break;
            }
        }
        if (match > best) {
            best = match;
            near_index = index;
            /* If we have a perfect match, stop now. */
            if (best == esize)
                break;
        }
    }
    if (best == esize) *ind = index;
    *near_ind = near_index;
}

static inline int pdfi_encoding_name_to_index(pdf_name *name)
{
    int ind = gs_error_undefined;
    if (pdfi_type_of(name) == PDF_NAME) {
        if (pdfi_name_is(name, "StandardEncoding")) {
            ind = ENCODING_INDEX_STANDARD;
        } else if (pdfi_name_is(name, "WinAnsiEncoding")) {
            ind = ENCODING_INDEX_WINANSI;
        }
        else if (pdfi_name_is(name, "MacRomanEncoding")) {
            ind = ENCODING_INDEX_MACROMAN;
        }
        else if (pdfi_name_is(name, "MacExpertEncoding")) {
            ind = ENCODING_INDEX_MACEXPERT;
        }
        else if (pdfi_name_is(name, "SymbolEncoding")) {
            ind = ENCODING_INDEX_SYMBOL;
        }
        else if (pdfi_name_is(name, "DingbatsEncoding")) {
            ind = ENCODING_INDEX_DINGBATS;
        }
    }
    return ind;
}

/*
 * Routine to fill in an array with each of the glyph names from a given
 * 'standard' Encoding.
 */
static int pdfi_build_Encoding(pdf_context *ctx, pdf_name *name, pdf_array *Encoding, gs_encoding_index_t *ind)
{
    int i, code = 0;
    unsigned char gs_encoding;
    gs_glyph temp;
    gs_const_string str;
    pdf_name *n = NULL;

    if (pdfi_array_size(Encoding) < 256)
        return gs_note_error(gs_error_rangecheck);

    code = pdfi_encoding_name_to_index(name);
    if (code < 0)
        return code;
    if (ind != NULL) *ind = (gs_encoding_index_t)code;
    gs_encoding = (unsigned char)code;
    code = 0;

    for (i = 0;i<256;i++) {
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
int pdfi_create_Encoding(pdf_context *ctx, pdf_font *ppdffont, pdf_obj *pdf_Encoding, pdf_obj *font_Encoding, pdf_obj **Encoding)
{
    int code = 0, i;
    gs_encoding_index_t encoding_index = ENCODING_INDEX_UNKNOWN, nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

    code = pdfi_array_alloc(ctx, 256, (pdf_array **)Encoding);
    if (code < 0)
        return code;
    pdfi_countup(*Encoding);

    switch (pdfi_type_of(pdf_Encoding)) {
        case PDF_NAME:
            code = pdfi_build_Encoding(ctx, (pdf_name *)pdf_Encoding, (pdf_array *)*Encoding, &encoding_index);
            if (code < 0) {
                pdfi_countdown(*Encoding);
                *Encoding = NULL;
                return code;
            }
            nearest_encoding_index = encoding_index;
            break;
        case PDF_DICT:
        {
            pdf_name *n = NULL;
            pdf_array *a = NULL;
            pdf_obj *o = NULL;
            int offset = 0;
            bool b_e_known;

            if (pdfi_type_of(pdf_Encoding) == PDF_DICT) {
                code = pdfi_dict_known(ctx, (pdf_dict *)pdf_Encoding, "BaseEncoding", &b_e_known);
                if (code < 0)
                    b_e_known = false;
            }
            else {
                b_e_known = false;
            }

            if (b_e_known == false && font_Encoding != NULL && pdfi_type_of(font_Encoding) == PDF_ARRAY) {
                pdf_array *fenc = (pdf_array *)font_Encoding;
                for (i = 0; i < pdfi_array_size(fenc) && code >= 0; i++) {
                    code = pdfi_array_get(ctx, fenc, (uint64_t)i, &o);
                    if (code >= 0)
                        code = pdfi_array_put(ctx, (pdf_array *)*Encoding, (uint64_t)i, o);
                    pdfi_countdown(o);
                }
                if (code < 0) {
                    pdfi_countdown(*Encoding);
                    *Encoding = NULL;
                    return code;
                }
            }
            else {
                code = pdfi_dict_get(ctx, (pdf_dict *)pdf_Encoding, "BaseEncoding", (pdf_obj **)&n);
                if (code >= 0) {
                    if (pdfi_encoding_name_to_index(n) < 0) {
                        pdfi_set_warning(ctx, 0, NULL, W_PDF_INVALID_FONT_BASEENC, "pdfi_create_Encoding", NULL);
                        pdfi_countdown(n);
                        n = NULL;
                        code = gs_error_undefined;
                    }
                    else if (pdfi_name_is(n, "StandardEncoding") == true) {
                        pdfi_set_warning(ctx, 0, NULL, W_PDF_INVALID_FONT_BASEENC, "pdfi_create_Encoding", NULL);
                    }
                }

                if (code < 0) {
                    code = pdfi_name_alloc(ctx, (byte *)"StandardEncoding", 16, (pdf_obj **)&n);
                    if (code < 0) {
                        pdfi_countdown(*Encoding);
                        *Encoding = NULL;
                        return code;
                    }
                    pdfi_countup(n);
                }

                code = pdfi_build_Encoding(ctx, n, (pdf_array *)*Encoding, NULL);
                if (code < 0) {
                    pdfi_countdown(*Encoding);
                    *Encoding = NULL;
                    pdfi_countdown(n);
                    return code;
                }
                pdfi_countdown(n);
            }
            code = pdfi_dict_knownget_type(ctx, (pdf_dict *)pdf_Encoding, "Differences", PDF_ARRAY, (pdf_obj **)&a);
            if (code <= 0) {
                if (code < 0) {
                    pdfi_countdown(*Encoding);
                    *Encoding = NULL;
                }
                return code;
            }

            for (i=0;i < pdfi_array_size(a);i++) {
                pdf_obj_type type;
                code = pdfi_array_get(ctx, a, (uint64_t)i, &o);
                if (code < 0)
                    break;
                type = pdfi_type_of(o);
                if (type == PDF_NAME) {
                    if (offset < pdfi_array_size((pdf_array *)*Encoding))
                        code = pdfi_array_put(ctx, (pdf_array *)*Encoding, (uint64_t)offset, o);
                    pdfi_countdown(o);
                    offset++;
                    if (code < 0)
                        break;
                } else if (type == PDF_INT) {
                    offset = ((pdf_num *)o)->value.i;
                    pdfi_countdown(o);
                } else {
                    code = gs_note_error(gs_error_typecheck);
                    pdfi_countdown(o);
                    break;
                }
            }
            pdfi_countdown(a);
            if (code < 0) {
                pdfi_countdown(*Encoding);
                *Encoding = NULL;
                return code;
            }
            if (ppdffont != NULL) /* No sense doing this if we can't record the result */
                pdfi_gs_simple_font_encoding_indices(ctx, (pdf_array *)(*Encoding), &encoding_index, &nearest_encoding_index);
            break;
        }
        default:
            pdfi_countdown(*Encoding);
            *Encoding = NULL;
            return gs_note_error(gs_error_typecheck);
    }
    if (ppdffont != NULL) {
        ppdffont->pfont->encoding_index = encoding_index;
        ppdffont->pfont->nearest_encoding_index = nearest_encoding_index;
    }
    return 0;
}

gs_glyph pdfi_encode_char(gs_font * pfont, gs_char chr, gs_glyph_space_t not_used)
{
    int code;
    unsigned int nindex = 0;
    gs_glyph g = GS_NO_GLYPH;

    if (pfont->FontType == ft_encrypted || pfont->FontType == ft_encrypted2
     || pfont->FontType == ft_user_defined || pfont->FontType == ft_TrueType
     || pfont->FontType == ft_MicroType
     || pfont->FontType == ft_PDF_user_defined) {
        pdf_font *font = (pdf_font *)pfont->client_data;
        pdf_context *ctx = (pdf_context *)font->ctx;

        if (font->Encoding != NULL) { /* safety */
            pdf_name *GlyphName = NULL;
            code = pdfi_array_get(ctx, font->Encoding, (uint64_t)chr, (pdf_obj **)&GlyphName);
            if (code >= 0) {
                if (pdfi_type_of(GlyphName) != PDF_NAME)
                    /* Can't signal an error, just return the 'not found' case */
                    return g;

                code = (*ctx->get_glyph_index)(pfont, (byte *)GlyphName->data, GlyphName->length, &nindex);
                pdfi_countdown(GlyphName);
                if (code >= 0)
                    g = (gs_glyph)nindex;
            }
        }
    }

    return g;
}

extern const pdfi_cid_decoding_t *pdfi_cid_decoding_list[];
extern const pdfi_cid_subst_nwp_table_t *pdfi_cid_substnwp_list[];

void pdfi_cidfont_cid_subst_tables(const char *reg, const int reglen, const char *ord,
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

int pdfi_tounicode_char_to_unicode(pdf_context *ctx, pdf_cmap *tounicode, gs_glyph glyph, int ch, ushort *unicode_return, unsigned int length)
{
    int i, l = 0;
    int code = gs_error_undefined;
    unsigned char *ucode = (unsigned char *)unicode_return;

    if (tounicode != NULL) {
        gs_cmap_lookups_enum_t lenum;
        gs_cmap_lookups_enum_init((const gs_cmap_t *)tounicode->gscmap, 0, &lenum);
        while (l == 0 && gs_cmap_enum_next_lookup(ctx->memory, &lenum) == 0) {
            gs_cmap_lookups_enum_t counter = lenum;
            while (l == 0 && gs_cmap_enum_next_entry(&counter) == 0) {
                if (counter.entry.value_type == CODE_VALUE_CID) {
                    if (counter.entry.key_is_range) {
                        unsigned int v0 = 0, v1 = 0;
                        for (i = 0; i < counter.entry.key_size; i++) {
                            v0 |= (counter.entry.key[0][counter.entry.key_size - i - 1]) << (i * 8);
                        }
                        for (i = 0; i < counter.entry.key_size; i++) {
                            v1 |= (counter.entry.key[1][counter.entry.key_size - i - 1]) << (i * 8);
                        }

                        if (ch >= v0 && ch <= v1) {
                            unsigned int offs = ch - v0;

                            if (counter.entry.value.size == 1) {
                                l = 2;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = 0x00;
                                    ucode[1] = counter.entry.value.data[0] + offs;
                                }
                            }
                            else if (counter.entry.value.size == 2) {
                                l = 2;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = counter.entry.value.data[0] + ((offs >> 8) & 0xff);
                                    ucode[1] = counter.entry.value.data[1] + (offs & 0xff);
                                }
                            }
                            else if (counter.entry.value.size == 3) {
                                l = 4;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = 0;
                                    ucode[1] = counter.entry.value.data[0] + ((offs >> 16) & 0xff);
                                    ucode[2] = counter.entry.value.data[1] + ((offs >> 8) & 0xff);
                                    ucode[3] = counter.entry.value.data[2] + (offs & 0xff);
                                }
                            }
                            else {
                                l = 4;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = counter.entry.value.data[0] + ((offs >> 24) & 0xff);
                                    ucode[1] = counter.entry.value.data[1] + ((offs >> 16) & 0xff);
                                    ucode[2] = counter.entry.value.data[2] + ((offs >> 8) & 0xff);
                                    ucode[3] = counter.entry.value.data[3] + (offs & 0xff);
                                }
                            }
                        }
                    }
                    else {
                        unsigned int v = 0;
                        for (i = 0; i < counter.entry.key_size; i++) {
                            v |= (counter.entry.key[0][counter.entry.key_size - i - 1]) << (i * 8);
                        }
                        if (ch == v) {
                            if (counter.entry.value.size == 1) {
                                l = 2;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = 0x00;
                                    ucode[1] = counter.entry.value.data[0];
                                }
                            }
                            else if (counter.entry.value.size == 2) {
                                l = 2;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = counter.entry.value.data[0];
                                    ucode[1] = counter.entry.value.data[1];
                                }
                            }
                            else if (counter.entry.value.size == 3) {
                                l = 4;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = 0;
                                    ucode[1] = counter.entry.value.data[0];
                                    ucode[2] = counter.entry.value.data[1];
                                    ucode[3] = counter.entry.value.data[2];
                                }
                            }
                            else {
                                l = 4;
                                if (ucode != NULL && length >= l) {
                                    ucode[0] = counter.entry.value.data[0];
                                    ucode[1] = counter.entry.value.data[1];
                                    ucode[2] = counter.entry.value.data[2];
                                    ucode[3] = counter.entry.value.data[3];
                                }
                            }
                        }
                    }
                }
            }
        }
        if (l > 0)
            code = l;
    }

    return code;
}

int
pdfi_cidfont_decode_glyph(gs_font *font, gs_glyph glyph, int ch, ushort *u, unsigned int length)
{
    gs_glyph cc = glyph < GS_MIN_CID_GLYPH ? glyph : glyph - GS_MIN_CID_GLYPH;
    pdf_cidfont_t *pcidfont = (pdf_cidfont_t *)font->client_data;
    int code = gs_error_undefined, i;
    uchar *unicode_return = (uchar *)u;
    pdfi_cid_subst_nwp_table_t *substnwp = pcidfont->substnwp;

    code = gs_error_undefined;
    while (1) { /* Loop to make retrying with a substitute CID easier */
        /* Favour the ToUnicode if one exists */
        code = pdfi_tounicode_char_to_unicode(pcidfont->ctx, (pdf_cmap *)pcidfont->ToUnicode, glyph, ch, u, length);

        if (code == gs_error_undefined && pcidfont->decoding) {
            const int *n;

            if (cc / 256 < pcidfont->decoding->nranges) {
                n = (const int *)pcidfont->decoding->ranges[cc / 256][cc % 256];
                for (i = 0; i < pcidfont->decoding->val_sizes; i++) {
                    unsigned int cmapcc;
                    if (n[i] == -1)
                        break;
                    cc = n[i];
                    cmapcc = (unsigned int)cc;
                    if (pcidfont->pdfi_font_type == e_pdf_cidfont_type2)
                        code = pdfi_fapi_check_cmap_for_GID((gs_font *)pcidfont->pfont, (unsigned int)cc, &cmapcc);
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

/* Get the unicode valude for a glyph FIXME - not written yet
 */
int pdfi_decode_glyph(gs_font * font, gs_glyph glyph, int ch, ushort *unicode_return, unsigned int length)
{
    pdf_font *pdffont = (pdf_font *)font->client_data;
    int code = 0;

    if (pdffont->pdfi_font_type != e_pdf_cidfont_type0 && pdffont->pdfi_font_type != e_pdf_cidfont_type1
     && pdffont->pdfi_font_type != e_pdf_cidfont_type2 && pdffont->pdfi_font_type != e_pdf_cidfont_type4) {
        code = pdfi_tounicode_char_to_unicode(pdffont->ctx, (pdf_cmap *)pdffont->ToUnicode, glyph, ch, unicode_return, length);
    }
    if (code < 0) code = 0;

    return code;
}

int pdfi_glyph_index(gs_font *pfont, byte *str, uint size, uint *glyph)
{
    int code = 0;
    pdf_font *font = (pdf_font *)pfont->client_data;

    code = pdfi_get_name_index(font->ctx, (char *)str, size, glyph);

    return code;
}

int pdfi_glyph_name(gs_font * pfont, gs_glyph glyph, gs_const_string * pstr)
{
    int code = gs_error_invalidfont;

    if (pfont->FontType == ft_encrypted || pfont->FontType == ft_encrypted2
     || pfont->FontType == ft_user_defined || pfont->FontType == ft_TrueType
     || pfont->FontType == ft_PDF_user_defined) {
        pdf_font *font = (pdf_font *)pfont->client_data;

        code = pdfi_name_from_index(font->ctx, glyph, (unsigned char **)&pstr->data, &pstr->size);
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

int pdfi_map_glyph_name_via_agl(pdf_dict *cstrings, pdf_name *gname, pdf_string **cstring)
{
    single_glyph_list_t *sgl = (single_glyph_list_t *)&(SingleGlyphList);
    int i, code, ucode = gs_error_undefined;
    *cstring = NULL;

    if (gname->length == 7 && strncmp((char *)gname->data, "uni", 3) == 0) {
        char u[5] = {0};
        memcpy(u, gname->data + 3, 4);
        code = sscanf(u, "%x", &ucode);
        if (code <= 0)
            ucode = gs_error_undefined;
    }

    if (ucode == gs_error_undefined) {
        for (i = 0; sgl[i].Glyph != 0x00; i++) {
            if (sgl[i].Glyph[0] == gname->data[0]
                && strlen(sgl[i].Glyph) == gname->length
                && !strncmp((char *)sgl[i].Glyph, (char *)gname->data, gname->length)) {
                ucode = (int)sgl[i].Unicode;
                break;
            }
        }
    }
    if (ucode > 0) {
        for (i = 0; sgl[i].Glyph != 0x00; i++) {
            if (sgl[i].Unicode == (unsigned short)ucode) {
                pdf_string *s;
                code = pdfi_dict_get((pdf_context *)cstrings->ctx, cstrings, (char *)sgl[i].Glyph, (pdf_obj **)&s);
                if (code >= 0) {
                    *cstring = s;
                    break;
                }
            }
        }
        if (*cstring == NULL) {
            char u[16] = {0};
            code = gs_snprintf(u, 16, "uni%04x", ucode);
            if (code > 0) {
                pdf_string *s;
                code = pdfi_dict_get((pdf_context *)cstrings->ctx, cstrings, u, (pdf_obj **)&s);
                if (code >= 0) {
                    *cstring = s;
                }
            }
        }
    }

    if (*cstring == NULL)
        code = gs_note_error(gs_error_undefined);
    else
        code = 0;

    return code;
}


int pdfi_init_font_directory(pdf_context *ctx)
{
    gs_font_dir *pfdir = ctx->memory->gs_lib_ctx->font_dir;
    if (pfdir) {
        ctx->font_dir = gs_font_dir_alloc2_limits(ctx->memory, ctx->memory,
                   pfdir->smax, pfdir->ccache.bmax, pfdir->fmcache.mmax,
                   pfdir->ccache.cmax, pfdir->ccache.upper);
        if (ctx->font_dir == NULL) {
            return_error(gs_error_VMerror);
        }
        ctx->font_dir->align_to_pixels = pfdir->align_to_pixels;
        ctx->font_dir->grid_fit_tt = pfdir->grid_fit_tt;
    }
    else {
        ctx->font_dir = gs_font_dir_alloc2(ctx->memory, ctx->memory);
        if (ctx->font_dir == NULL) {
            return_error(gs_error_VMerror);
        }
    }
    ctx->font_dir->global_glyph_code = pdfi_global_glyph_code;
    return 0;
}

/* Loads a (should be!) non-embedded font by name
   Only currently works for Type 1 fonts set.
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
    pdf_name *Type1Name = NULL;

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

    code = pdfi_obj_charstr_to_name(ctx, "Type1", &Type1Name);
    if (code < 0)
        goto exit;

    code = pdfi_dict_put(ctx, fdict, "Subtype", (pdf_obj *)Type1Name);
    if (code < 0)
        goto exit;

    code = pdfi_load_font(ctx, NULL, NULL, fdict, &pgsfont, false);
    if (code < 0)
        goto exit;

    *ppdffont = (pdf_obj *)pgsfont->client_data;

 exit:
    pdfi_countdown(Type1Name);
    pdfi_countdown(fontobjtype);
    pdfi_countdown(fname);
    pdfi_countdown(fdict);
    return code;
}


int pdfi_font_create_widths(pdf_context *ctx, pdf_dict *fontdict, pdf_font *font, double scale)
{
    int code = 0;
    pdf_obj *obj = NULL;
    int i;

    font->Widths = NULL;

    if (font->FontDescriptor != NULL) {
        code = pdfi_dict_knownget(ctx, font->FontDescriptor, "MissingWidth", &obj);
        if (code > 0) {
            if (pdfi_type_of(obj) == PDF_INT) {
                font->MissingWidth = ((pdf_num *) obj)->value.i * scale;
            }
            else if (pdfi_type_of(obj) == PDF_REAL) {
                font->MissingWidth = ((pdf_num *) obj)->value.d  * scale;
            }
            else {
                font->MissingWidth = 0;
            }
            pdfi_countdown(obj);
            obj = NULL;
        }
        else {
            font->MissingWidth = 0;
        }
    }
    else {
        font->MissingWidth = 0;
    }

    code = pdfi_dict_knownget_type(ctx, fontdict, "Widths", PDF_ARRAY, (pdf_obj **)&obj);
    if (code > 0) {
        if (pdfi_array_size((pdf_array *)obj) < font->LastChar - font->FirstChar + 1) {
            code = gs_note_error(gs_error_rangecheck);
            goto error;
        }

        font->Widths = (double *)gs_alloc_bytes(OBJ_MEMORY(font), sizeof(double) * (font->LastChar - font->FirstChar + 1), "pdfi_font_create_widths(Widths)");
        if (font->Widths == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto error;
        }
        memset(font->Widths, 0x00, sizeof(double) * (font->LastChar - font->FirstChar + 1));
        for (i = 0; i < (font->LastChar - font->FirstChar + 1); i++) {
            code = pdfi_array_get_number(ctx, (pdf_array *)obj, (uint64_t)i, &font->Widths[i]);
            if (code < 0)
                goto error;
            font->Widths[i] *= scale;
        }
    }
error:
    pdfi_countdown(obj);
    if (code < 0) {
        gs_free_object(OBJ_MEMORY(font), font->Widths, "pdfi_font_create_widths(Widths)");
        font->Widths = NULL;
    }
    return code;
}

void pdfi_font_set_first_last_char(pdf_context *ctx, pdf_dict *fontdict, pdf_font *font)
{
    double f, l;
    int code;

    if (fontdict == NULL) {
        f = (double)0;
        l = (double)255;
    }
    else {
        code = pdfi_dict_get_number(ctx, fontdict, "FirstChar", &f);
        if (code < 0 || f < 0 || f > 255)
            f = (double)0;

        code = pdfi_dict_get_number(ctx, fontdict, "LastChar", &l);
        if (code < 0 || l < 0 || l > 255)
            l = (double)255;
    }
    if (f <= l) {
        font->FirstChar = (int)f;
        font->LastChar = (int)l;
    }
    else {
        font->FirstChar = 0;
        font->LastChar = 255;
    }
}

void pdfi_font_set_orig_fonttype(pdf_context *ctx, pdf_font *font)
{
    pdf_name *ftype;
    pdf_dict *fontdict = font->PDF_font;
    int code;

    code = pdfi_dict_get_type(ctx, fontdict, "Subtype", PDF_NAME, (pdf_obj**)&ftype);
    if (code < 0) {
        font->orig_FontType = ft_undefined;
    }
    else {
        if (pdfi_name_is(ftype, "Type1") || pdfi_name_is(ftype, "MMType1"))
            font->orig_FontType = ft_encrypted;
        else if (pdfi_name_is(ftype, "Type1C"))
            font->orig_FontType = ft_encrypted2;
        else if (pdfi_name_is(ftype, "TrueType"))
            font->orig_FontType = ft_TrueType;
        else if (pdfi_name_is(ftype, "Type3"))
            font->orig_FontType = ft_user_defined;
        else if (pdfi_name_is(ftype, "CIDFontType0"))
            font->orig_FontType = ft_CID_encrypted;
        else if (pdfi_name_is(ftype, "CIDFontType2"))
            font->orig_FontType = ft_CID_TrueType;
        else
            font->orig_FontType = ft_undefined;
    }
    pdfi_countdown(ftype);
}

/* Patch or create a new XUID based on the existing UID/XUID, a simple hash
   of the input file name and the font dictionary object number.
   This allows improved glyph cache efficiency, also ensures pdfwrite understands
   which fonts are repetitions, and which are different.
   Currently cannot return an error - if we can't allocate the new XUID values array,
   we just skip it, and assume the font is compliant.
 */
int pdfi_font_generate_pseudo_XUID(pdf_context *ctx, pdf_dict *fontdict, gs_font_base *pfont)
{
    gs_const_string fn;
    int i;
    uint32_t hash = 0;
    long *xvalues;
    int xuidlen = 3;

    sfilename(ctx->main_stream->s, &fn);
    if (fontdict!= NULL && fontdict->object_num != 0) {
        const byte *sb;
        size_t l;
        if (fn.size > 0) {
            sb = fn.data;
            l = fn.size;
        }
        else {
            s_process_read_buf(ctx->main_stream->s);
            sb = sbufptr(ctx->main_stream->s);
            l = sbufavailable(ctx->main_stream->s) > 128 ? 128: sbufavailable(ctx->main_stream->s);
        }

        for (i = 0; i < l; i++) {
            hash = ((((hash & 0xf8000000) >> 27) ^ (hash << 5)) & 0x7ffffffff) ^ sb[i];
        }
        hash = ((((hash & 0xf8000000) >> 27) ^ (hash << 5)) & 0x7ffffffff) ^ fontdict->object_num;

        if (uid_is_XUID(&pfont->UID) && uid_XUID_size(&pfont->UID) > 2 && uid_XUID_values(&pfont->UID)[0] == 1000000) {
            /* This is already a pseudo XUID, probably because we are copying an existing font object
             * So just update the hash and the object number
             */
             uid_XUID_values(&pfont->UID)[1] = hash;
             uid_XUID_values(&pfont->UID)[2] = ctx->device_state.HighLevelDevice ? fontdict->object_num : 0;
        }
        else {
            if (uid_is_XUID(&pfont->UID))
                xuidlen += uid_XUID_size(&pfont->UID);
            else if (uid_is_valid(&pfont->UID))
                xuidlen++;

            xvalues = (long *)gs_alloc_bytes(pfont->memory, xuidlen * sizeof(long), "pdfi_font_generate_pseudo_XUID");
            if (xvalues == NULL) {
                return 0;
            }
            xvalues[0] = 1000000; /* "Private" value */
            xvalues[1] = hash;

            xvalues[2] = ctx->device_state.HighLevelDevice ? fontdict->object_num : 0;

            if (uid_is_XUID(&pfont->UID)) {
                for (i = 0; i < uid_XUID_size(&pfont->UID); i++) {
                    xvalues[i + 3] = uid_XUID_values(&pfont->UID)[i];
                }
                uid_free(&pfont->UID, pfont->memory, "pdfi_font_generate_pseudo_XUID");
            }
            else if (uid_is_valid(&pfont->UID))
                xvalues[3] = pfont->UID.id;

            uid_set_XUID(&pfont->UID, xvalues, xuidlen);
        }
    }
    return 0;
}

int pdfi_default_font_info(gs_font *font, const gs_point *pscale, int members, gs_font_info_t *info)
{
    pdf_font *pdff = (pdf_font *)font->client_data;
    int code;

    /* We *must* call this first as it sets info->members = 0; */
    code = pdff->default_font_info(font, pscale, members, info);
    if (code < 0)
        return code;
    if ((members & FONT_INFO_EMBEDDED) != 0) {
        info->orig_FontType = pdff->orig_FontType;
        if (pdff->pdfi_font_type == e_pdf_font_type3) {
            info->FontEmbedded = (int)(true);
            info->members |= FONT_INFO_EMBEDDED;
        }
        else {
            info->FontEmbedded = (int)(pdff->substitute == font_embedded);
            info->members |= FONT_INFO_EMBEDDED;
        }
    }
    if (pdff->pdfi_font_type != e_pdf_font_truetype && pdff->pdfi_font_type != e_pdf_cidfont_type2) {
        if (((members & FONT_INFO_COPYRIGHT) != 0) && pdff->copyright != NULL) {
            info->Copyright.data = pdff->copyright->data;
            info->Copyright.size = pdff->copyright->length;
            info->members |= FONT_INFO_COPYRIGHT;
        }
        if (((members & FONT_INFO_NOTICE) != 0) && pdff->notice != NULL) {
            info->Notice.data = pdff->notice->data;
            info->Notice.size = pdff->notice->length;
            info->members |= FONT_INFO_NOTICE;
        }
        if (((members & FONT_INFO_FAMILY_NAME) != 0) && pdff->familyname != NULL) {
            info->FamilyName.data = pdff->familyname->data;
            info->FamilyName.size = pdff->familyname->length;
            info->members |= FONT_INFO_FAMILY_NAME;
        }
        if (((members & FONT_INFO_FULL_NAME) != 0) && pdff->fullname != NULL) {
            info->FullName.data = pdff->fullname->data;
            info->FullName.size = pdff->fullname->length;
            info->members |= FONT_INFO_FULL_NAME;
        }
    }
    return 0;
}

/* Convenience function for using fonts created by
   pdfi_load_font_by_name_string
 */
int pdfi_set_font_internal(pdf_context *ctx, pdf_obj *fontobj, double point_size)
{
    int code;
    pdf_font *pdffont = (pdf_font *)fontobj;

    if (pdfi_type_of(pdffont) != PDF_FONT || pdffont->pfont == NULL)
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
    pdfi_countdown(font);
    return code;
}

int pdfi_font_set_internal_string(pdf_context *ctx, const char *fontname, double point_size)
{
    return pdfi_font_set_internal_inner(ctx, (const byte *)fontname, strlen(fontname), point_size);
}

int pdfi_font_set_internal_name(pdf_context *ctx, pdf_name *fontname, double point_size)
{
    if (pdfi_type_of(fontname) != PDF_NAME)
        return_error(gs_error_typecheck);
    else
        return pdfi_font_set_internal_inner(ctx, fontname->data, fontname->length, point_size);
}
