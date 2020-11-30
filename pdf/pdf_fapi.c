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

/* Interface to FAPI for the PDF interpreter */

/* Include this first so that we don't get a macro redefnition of 'offsetof' */
#include "pdf_int.h"

#include "memory_.h"
#include "gsmemory.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gzstate.h"
#include "gxchar.h"             /* for st_gs_show_enum */
#include "gdebug.h"
#include "gxfapi.h"
#include "gscoord.h"
#include "gspath.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_font.h"
#include "pdf_agl.h"

/* forward declarations for the pdfi_ff_stub definition */
static int
pdfi_fapi_get_word(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned short *ret);

static int
pdfi_fapi_get_long(gs_fapi_font * ff, gs_fapi_font_feature var_id, int index, unsigned long *ret);

static int
pdfi_fapi_get_float(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, float *ret);

static int
pdfi_fapi_get_name(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *buffer, int len);

static int
pdfi_fapi_get_proc(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *buffer);

static int
pdfi_fapi_get_gsubr(gs_fapi_font *ff, int index, byte *buf, int buf_length);

static int
pdfi_fapi_get_subr(gs_fapi_font *ff, int index, byte *buf, int buf_length);

static int
pdfi_fapi_get_raw_subr(gs_fapi_font *ff, int index, byte *buf, int buf_length);

static int
pdfi_fapi_serialize_tt_font(gs_fapi_font * ff, void *buf, int buf_size);

static int
pdfi_fapi_get_charstring(gs_fapi_font *ff, int index, byte *buf, ushort buf_length);

static int
pdfi_fapi_get_charstring_name(gs_fapi_font *ff, int index, byte *buf, ushort buf_length);


static int
pdfi_fapi_get_glyphname_or_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, gs_glyph ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID);

static int
pdfi_fapi_get_glyph(gs_fapi_font * ff, gs_glyph char_code, byte * buf, int buf_length);


static int
pdfi_get_glyphdirectory_data(gs_fapi_font * ff, int char_code,
                           const byte ** ptr);

static int
pdfi_fapi_set_cache(gs_text_enum_t * penum, const gs_font_base * pbfont,
                  const gs_string * char_name, gs_glyph cid,
                  const double pwidth[2], const gs_rect * pbbox,
                  const double Metrics2_sbw_default[4], bool * imagenow);

static int
pdfi_fapi_get_metrics(gs_fapi_font * ff, gs_string * char_name, gs_glyph cid, double *m, bool vertical);

static const gs_fapi_font pdfi_ff_stub = {
    0,                                              /* server_font_data */
    0,                                              /* need_decrypt */
    NULL,                                           /* const gs_memory_t */
    0,                                              /* font_file_path */
    0,                                              /* full_font_buf */
    0,                                              /* full_font_buf_len */
    0,                                              /* subfont */
    false,                                          /* is_type1 */
    false,                                          /* is_cid */
    false,                                          /* is_outline_font */
    false,                                          /* is_mtx_skipped */
    false,                                          /* is_vertical */
    false,                                          /* metrics_only */
    {{3, 1}, {1, 0}, {3, 0}, {3, 10}, {-1, -1}},    /* ttf_cmap_req */
    {-1, -1},                                       /* ttf_cmap_selected */
    0,                                              /* client_ctx_p */
    0,                                              /* client_font_data */
    0,                                              /* client_font_data2 */
    0,                                              /* char_data */
    0,                                              /* char_data_len */
    0,                                              /* embolden */
    pdfi_fapi_get_word,                             /* get_word */
    pdfi_fapi_get_long,                             /* get_long */
    pdfi_fapi_get_float,                            /* get_float */
    pdfi_fapi_get_name,                             /* get_name */
    pdfi_fapi_get_proc,                             /* get_proc */
    pdfi_fapi_get_gsubr,                            /* get_gsubr */
    pdfi_fapi_get_subr,                             /* get_subr */
    pdfi_fapi_get_raw_subr,                         /* get_raw_subr */
    pdfi_fapi_get_glyph,                            /* get_glyph */
    pdfi_fapi_serialize_tt_font,                    /* serialize_tt_font */
    pdfi_fapi_get_charstring,                       /* get_charstring */
    pdfi_fapi_get_charstring_name,                  /* get_charstring_name */
    pdfi_get_glyphdirectory_data,                   /* get_GlyphDirectory_data_ptr */
    pdfi_fapi_get_glyphname_or_cid,                 /* get_glyphname_or_cid */
    pdfi_fapi_get_metrics,                          /* fapi_get_metrics */
    pdfi_fapi_set_cache                             /* fapi_set_cache */
};

static inline ushort
float_to_ushort(float v)
{
    return ((ushort) (v * 16)); /* fixme : the scale may depend on renderer */
}

static int
pdfi_fapi_get_word(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned short *ret)
{
    int code = 0;
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;

    switch ((int)var_id) {
        case gs_fapi_font_feature_Weight:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_ItalicAngle:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_IsFixedPitch:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_UnderLinePosition:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_UnderlineThickness:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_FontType:
            *ret = (pfont->FontType == 2 ? 2 : 1);
            break;
        case gs_fapi_font_feature_FontBBox:
            switch (index) {
                case 0:
                    *ret = ((ushort) pfont->FontBBox.p.x);
                    break;
                case 1:
                    *ret = ((ushort) pfont->FontBBox.p.y);
                    break;
                case 2:
                    *ret = ((ushort) pfont->FontBBox.q.x);
                    break;
                case 3:
                    *ret = ((ushort) pfont->FontBBox.q.y);
                    break;
                default:
                    code = gs_note_error(gs_error_rangecheck);
            }
            break;
        case gs_fapi_font_feature_BlueValues_count:
            *ret = pfont->data.BlueValues.count;
            break;
        case gs_fapi_font_feature_BlueValues:
            *ret =  (float_to_ushort(pfont->data.BlueValues.values[index]));
            break;
        case gs_fapi_font_feature_OtherBlues_count:
            *ret = pfont->data.OtherBlues.count;
            break;
        case gs_fapi_font_feature_OtherBlues:
            *ret = (float_to_ushort(pfont->data.OtherBlues.values[index]));
            break;
        case gs_fapi_font_feature_FamilyBlues_count:
            *ret = pfont->data.FamilyBlues.count;
            break;
        case gs_fapi_font_feature_FamilyBlues:
            *ret = (float_to_ushort(pfont->data.FamilyBlues.values[index]));
            break;
        case gs_fapi_font_feature_FamilyOtherBlues_count:
            *ret = pfont->data.FamilyOtherBlues.count;
            break;
        case gs_fapi_font_feature_FamilyOtherBlues:
            *ret = (float_to_ushort(pfont->data.FamilyOtherBlues.values[index]));
            break;
        case gs_fapi_font_feature_BlueShift:
            *ret = float_to_ushort(pfont->data.BlueShift);
            break;
        case gs_fapi_font_feature_BlueFuzz:
            *ret = float_to_ushort(pfont->data.BlueShift);
            break;
        case gs_fapi_font_feature_StdHW:
            *ret = (pfont->data.StdHW.count == 0 ? 0 : float_to_ushort(pfont->data.StdHW.values[0]));   /* UFST bug ? */
            break;
        case gs_fapi_font_feature_StdVW:
            *ret = (pfont->data.StdVW.count == 0 ? 0 : float_to_ushort(pfont->data.StdVW.values[0]));   /* UFST bug ? */
            break;
        case gs_fapi_font_feature_StemSnapH_count:
            *ret = pfont->data.StemSnapH.count;
            break;
        case gs_fapi_font_feature_StemSnapH:
            *ret = float_to_ushort(pfont->data.StemSnapH.values[index]);
            break;
        case gs_fapi_font_feature_StemSnapV_count:
            *ret = pfont->data.StemSnapV.count;
            break;
        case gs_fapi_font_feature_StemSnapV:
            *ret = float_to_ushort(pfont->data.StemSnapV.values[index]);
            break;
        case gs_fapi_font_feature_ForceBold:
            *ret = pfont->data.ForceBold;
            break;
        case gs_fapi_font_feature_LanguageGroup:
            *ret = pfont->data.LanguageGroup;
            break;
        case gs_fapi_font_feature_lenIV:
            *ret = ff->need_decrypt ? 0 : pfont->data.lenIV;
            break;
#if 0
        case gs_fapi_font_feature_GlobalSubrs_count:
            {
                ref *Private, *GlobalSubrs;

                if (pfont->FontType == ft_encrypted2) {
                    if (dict_find_string(pdr, "Private", &Private) <= 0) {
                        *ret = 0;
                        break;
                    }
                    if (dict_find_string(Private, "GlobalSubrs", &GlobalSubrs) <= 0) {
                        *ret = 0;
                        break;
                    }
                    *ret = r_size(GlobalSubrs);
                    break;
                }
                *ret = 0;
                break;
            }
#endif
        case gs_fapi_font_feature_Subrs_count:
            {
                if (pfont->FontType == ft_encrypted) {
                    pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                    *ret = pdffont1->NumSubrs;
                }
                break;
            }
        case gs_fapi_font_feature_CharStrings_count:
            {
                if (pfont->FontType == ft_encrypted) {
                    pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                    *ret = pdffont1->CharStrings->entries;
                }
                break;
            }
        default:
            code = gs_error_undefined;
            *ret = -1;
    }
    return code;
}

static int
pdfi_fapi_get_long(gs_fapi_font * ff, gs_fapi_font_feature var_id, int index, unsigned long *ret)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    int code = 0;

    switch ((int)var_id) {
        case gs_fapi_font_feature_UniqueID:
            *ret = pfont->UID.id;
            break;
        case gs_fapi_font_feature_BlueScale:
            *ret = (ulong) (pfont->data.BlueScale * 65536);
            break;
        case gs_fapi_font_feature_Subrs_total_size:
            {
                if (pfont->FontType == ft_encrypted) {
                    pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                    int i;
                    *ret = 0;
                    for (i = 0; i < pdffont1->NumSubrs; i++) {
                        *ret += pdffont1->Subrs[i].size;
                    }
                }
            }
            break;
        default:
            code = gs_error_undefined;
            break;
    }
    return code;
}

static int
pdfi_fapi_get_float(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, float *ret)
{
    gs_font_base *pbfont = (gs_font_base *) ff->client_font_data2;
    int code = 0;
    gs_fapi_server *I = pbfont->FAPI;

    switch ((int)var_id) {
        case gs_fapi_font_feature_FontMatrix:
            {
                double FontMatrix_div;
                gs_matrix m, *mptr;

                if (I && I->get_fontmatrix) {
                    FontMatrix_div = 1;
                    mptr = &m;
                    I->get_fontmatrix(I, mptr);
                }
                else {
                    FontMatrix_div = ((ff->is_cid && (!FAPI_ISCIDFONT(pbfont))) ? 1000 : 1);
                    mptr = &(pbfont->base->FontMatrix);
                }
                switch (index) {
                    case 0:
                    default:
                        *ret = (mptr->xx / FontMatrix_div);
                        break;
                    case 1:
                        *ret = (mptr->xy / FontMatrix_div);
                        break;
                    case 2:
                        *ret = (mptr->yx / FontMatrix_div);
                        break;
                    case 3:
                        *ret = (mptr->yy / FontMatrix_div);
                        break;
                    case 4:
                        *ret = (mptr->tx / FontMatrix_div);
                        break;
                    case 5:
                        *ret = (mptr->ty / FontMatrix_div);
                        break;
                }
                break;
            }
        default:
            code = gs_error_undefined;
    }

    return code;
}

/* Only required for multiple masters, I believe */
static int
pdfi_fapi_get_name(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *buffer, int len)
{
    (void)ff;
    (void)var_id;
    (void)index;
    (void)buffer;
    (void)len;
    return 0;
}

/* Only required for multiple masters, I believe */
static int
pdfi_fapi_get_proc(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *buffer)
{
    (void)ff;
    (void)var_id;
    (void)index;
    (void)buffer;
    return 0;
}

static int
pdfi_fapi_get_gsubr(gs_fapi_font *ff, int index, byte *buf, int buf_length)
{
    return 0;
}

static inline void
decode_bytes(byte *p, const byte *s, int l, int lenIV)
{
    ushort state = 4330;

    for (; l; s++, l--) {
        uchar c = (*s ^ (state >> 8));

        state = (*s + state) * crypt_c1 + crypt_c2;
        if (lenIV > 0)
            lenIV--;
        else {
            *p = c;
            p++;
        }
    }
}

static int
pdfi_fapi_get_subr(gs_fapi_font *ff, int index, byte *buf, int buf_length)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    int code = 0;

    if (pfont->FontType == ft_encrypted) {
        pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
        if (index > pdffont1->NumSubrs) {
            code = gs_error_rangecheck;
        }
        else {
            code = pdffont1->Subrs[index].size;
            if (buf && buf_length >= code) {
                if (ff->need_decrypt && pfont->data.lenIV >= 0) {
                    decode_bytes(buf, pdffont1->Subrs[index].data, code, pfont->data.lenIV);
                }
                else {
                    memcpy(buf, pdffont1->Subrs[index].data, code);
                }
            }
        }
    }
    return code;
}

static int
pdfi_fapi_get_raw_subr(gs_fapi_font *ff, int index, byte *buf, int buf_length)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    int code = 0;

    if (pfont->FontType == ft_encrypted) {
        pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
        if (index > pdffont1->NumSubrs) {
            code = gs_error_rangecheck;
        }
        else {
            code = pdffont1->Subrs[index].size;
            if (buf && buf_length >= code) {
                memcpy(buf, pdffont1->Subrs[index].data, code);
            }
        }
    }
    return code;
}

static int
pdfi_fapi_get_charstring(gs_fapi_font *ff, int index, byte *buf, ushort buf_length)
{
    return 0;
}

static int
pdfi_fapi_get_charstring_name(gs_fapi_font *ff, int index, byte *buf, ushort buf_length)
{
    return 0;
}

extern pdfi_single_glyph_list_t *pdfi_SingleGlyphList;
extern mac_glyph_ordering_t MacintoshOrdering[];

static int
pdfi_fapi_get_glyphname_or_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, gs_glyph ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID)
{
    if (pbfont->FontType == ft_CID_TrueType) {
        pdf_cidfont_type2 *pttfont = (pdf_cidfont_type2 *)pbfont->client_data;
        gs_glyph gid;

        if (ccode > GS_MIN_CID_GLYPH)
           ccode = ccode - GS_MIN_CID_GLYPH;

        gid = ccode;
        if (pttfont->cidtogidmap.size > (ccode << 1) + 1) {
            gid = pttfont->cidtogidmap.data[ccode << 1] << 8 | pttfont->cidtogidmap.data[(ccode << 1) + 1];
        }
        cr->client_char_code = ccode;
        cr->char_codes[0] = gid;
        cr->is_glyph_index = true;
        return 0;
    }
    else if (pbfont->FontType == ft_TrueType) {
        /* I'm not clear if the heavy lifting should be here or in pdfi_tt_encode_char() */
        pdf_font_truetype *ttfont = (pdf_font_truetype *)pbfont->client_data;
        pdf_name *GlyphName = NULL;
        int i, code = pdfi_array_get(ttfont->ctx, ttfont->Encoding, (uint64_t)ccode, (pdf_obj **)&GlyphName);

        cr->client_char_code = ccode;
        cr->is_glyph_index = false;
        if (code < 0)
            return 0;

        if (ttfont->cmap == pdfi_truetype_cmap_10) {
            if ((ttfont->descflags & 4) == 0) {
                for (i = 0; MacintoshOrdering[i].ccode != -1; i++) {
                    if (MacintoshOrdering[i].name[0] == GlyphName->data[0]
                        && strlen(MacintoshOrdering[i].name) == GlyphName->length
                        && !strncmp((char *)MacintoshOrdering[i].name, (char *)GlyphName->data, GlyphName->length)) {
                        break;
                    }
                }
                if (MacintoshOrdering[i].ccode != -1) {
                    uint cc = MacintoshOrdering[i].ccode;
                    code = pdfi_fapi_check_cmap_for_GID((gs_font *)pbfont, &cc);
                    if (code < 0 || cc == 0) {
                        gs_font_type42 *pfonttt = (gs_font_type42 *)pbfont;
                        gs_string gname = {0};

                        /* This is a very slow implementation, we may benefit from creating a
                         * a reverse post table upfront */
                        for (i = 0; i < pfonttt->data.numGlyphs; i++) {
                            code = gs_type42_find_post_name(pfonttt, (gs_glyph)i, &gname);
                            if (code >= 0) {
                                if (gname.data[0] == GlyphName->data[0]
                                    && gname.size == GlyphName->length
                                    && !strncmp((char *)gname.data, (char *)GlyphName->data, GlyphName->length))
                                {
                                    cr->char_codes[0] = i;
                                    cr->is_glyph_index = false;
                                    break;
                                }
                            }
                        }
                    }
                    else {
                        cr->char_codes[0] = MacintoshOrdering[i].ccode;
                    }
                }
            }
        }
        else if (ttfont->cmap == pdfi_truetype_cmap_31) {
            if ((ttfont->descflags & 4) == 0) {
                pdfi_single_glyph_list_t *sgl = (pdfi_single_glyph_list_t *)&(pdfi_SingleGlyphList);
                /* Not to spec, but... if we get a "uni..." formatted name, use
                   the hex value from that.
                 */
                if (GlyphName->length > 5 && !strncmp((char *)GlyphName->data, "uni", 3)) {
                    unsigned int cc;
                    sscanf((char *)(GlyphName->data + 3), "%x", &cc);
                    cr->char_codes[0] = cc;
                }
                else {
                    /* Slow linear search, we could binary chop it */
                    for (i = 0; sgl[i].Glyph != 0x00; i++) {
                        if (sgl[i].Glyph[0] == GlyphName->data[0]
                            && strlen(sgl[i].Glyph) == GlyphName->length
                            && !strncmp((char *)sgl[i].Glyph, (char *)GlyphName->data, GlyphName->length))
                            break;
                    }
                    if (sgl[i].Glyph == NULL) {
                        gs_font_type42 *pfonttt = (gs_font_type42 *)pbfont;
                        gs_string gname = {0};

                        /* This is a very slow implementation, we may benefit from creating a
                         * a reverse post table upfront */
                        for (i = 0; i < pfonttt->data.numGlyphs; i++) {
                            code = gs_type42_find_post_name(pfonttt, (gs_glyph)i, &gname);
                            if (code >= 0) {
                                if (gname.data[0] == GlyphName->data[0]
                                    && gname.size == GlyphName->length
                                    && !strncmp((char *)gname.data, (char *)GlyphName->data, GlyphName->length))
                                {
                                    cr->char_codes[0] = i;
                                    cr->is_glyph_index = false;
                                    break;
                                }
                            }
                        }
                    }
                    else {
                        cr->char_codes[0] = sgl[i].Unicode;
                        cr->is_glyph_index = false;
                    }
                }
            }
        }
        pdfi_countdown(GlyphName);
        return 0;
    }
    return pbfont->procs.glyph_name((gs_font *)pbfont, ccode, (gs_const_string *)enc_char_name);
}

static int
pdfi_fapi_get_glyph(gs_fapi_font * ff, gs_glyph char_code, byte * buf, int buf_length)
{
    gs_font_base *pbfont = (gs_font_base *) ff->client_font_data2;
    int code = 0;

    /* This should only get called for Postscript-type fonts */
    if (ff->is_type1) {
        gs_font_type1 *pfont1 = (gs_font_type1 *) ff->client_font_data;
        pdf_name *glyphname = NULL;
        pdf_string *charstring = NULL;
        pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pbfont->client_data;
        code = pdfi_array_get(pdffont1->ctx, pdffont1->Encoding, (uint64_t)char_code, (pdf_obj **)&glyphname);
        if (code < 0) {
            pdfi_countdown(glyphname);
            return code;
        }
        code = pdfi_dict_get_by_key(pdffont1->ctx, pdffont1->CharStrings, glyphname, (pdf_obj **)&charstring);
        pdfi_countdown(glyphname);
        if (code < 0) {
            code = pdfi_dict_get(pdffont1->ctx, pdffont1->CharStrings, ".notdef", (pdf_obj **)&charstring);
        }
        code = charstring->length;
        if (buf != NULL && code <= buf_length) {
            if (ff->need_decrypt && pfont1->data.lenIV >= 0)
                decode_bytes(buf, charstring->data, charstring->length, pfont1->data.lenIV);
            else
                memcpy(buf, charstring->data, charstring->length);
        }
        pdfi_countdown(charstring);
    }
    else {
        code = gs_error_invalidaccess;
    }
    return code;
}

static int
pdfi_fapi_serialize_tt_font(gs_fapi_font * ff, void *buf, int buf_size)
{
    return 0;
}

static int
pdfi_get_glyphdirectory_data(gs_fapi_font * ff, int char_code,
                           const byte ** ptr)
{
    return (0);
}

#define GLYPH_W0_WIDTH_INDEX 0
#define GLYPH_W0_HEIGHT_INDEX 1

#define GLYPH_W1_WIDTH_INDEX 2
#define GLYPH_W1_HEIGHT_INDEX 3
#define GLYPH_W1_V_X_INDEX 4
#define GLYPH_W1_V_Y_INDEX 5

static int pdfi_get_glyph_metrics(gs_font *pfont, gs_glyph cid, double *widths, bool vertical)
{
    pdf_cidfont_type2 *pdffont11 = (pdf_cidfont_type2 *)pfont->client_data;
    int i;

    widths[GLYPH_W0_WIDTH_INDEX] = (double)pdffont11->DW;
    widths[GLYPH_W0_HEIGHT_INDEX] = 0;
    if (pdffont11->W != NULL) {
        i = 0;

        while(1) {
            pdf_num *c;
            pdf_obj *o;
            if (i + 1>= pdffont11->W->size) break;
            (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W, i, (pdf_obj **)&c);
            if (c->type != PDF_INT) {
                return_error(gs_error_typecheck);
            }
            (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W, i + 1, (pdf_obj **)&o);
            if (o->type == PDF_INT) {
                pdf_num *c2 = (pdf_num *)o;
                if (i + 2 >= pdffont11->W->size) break;
                (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W, i + 2, (pdf_obj **)&o);
                if (o->type == PDF_INT) {
                    if (cid >= c->value.i && cid <= c2->value.i) {
                        widths[GLYPH_W0_WIDTH_INDEX] = (double)((pdf_num *)o)->value.i;
                        widths[GLYPH_W0_HEIGHT_INDEX] = 0.0;
                        break;
                    }
                    else {
                        i += 3;
                        continue;
                    }
                }
                else {
                    return_error(gs_error_typecheck);
                }
            }
            else if (o->type == PDF_ARRAY) {
                pdf_array *a = (pdf_array *)o;
                if (cid >= c->value.i && cid < c->value.i + a->size) {
                    (void)pdfi_array_peek(pdffont11->ctx, a, cid - c->value.i, (pdf_obj **)&o);
                    if (o->type == PDF_INT) {
                        widths[GLYPH_W0_WIDTH_INDEX] = (double)((pdf_num *)o)->value.i;
                        widths[GLYPH_W0_HEIGHT_INDEX] = 0.0;
                        break;
                    }
                }
                i += 2;
                continue;
            }
            else
                return_error(gs_error_typecheck);
        }
    }
    if (vertical) {
        /* Default default <sigh>! */
        widths[GLYPH_W1_WIDTH_INDEX] = 0;
        widths[GLYPH_W1_HEIGHT_INDEX] = -1000.0;
        widths[GLYPH_W1_V_X_INDEX] = (widths[GLYPH_W0_WIDTH_INDEX] / 2.0);
        widths[GLYPH_W1_V_Y_INDEX] = 880.0;

        if (pdffont11->DW2 != NULL && pdffont11->DW2->type == PDF_ARRAY
            && pdffont11->DW2->size >= 2) {
            pdf_num *w2_0, *w2_1;

            (void)pdfi_array_peek(pdffont11->ctx, (pdf_array *)pdffont11->DW2, 0, (pdf_obj **)&w2_0);
            (void)pdfi_array_peek(pdffont11->ctx, (pdf_array *)pdffont11->DW2, 1, (pdf_obj **)&w2_1);

            if (w2_0->type == PDF_INT && w2_1->type == PDF_INT) {
                widths[GLYPH_W1_WIDTH_INDEX] = 0.0;
                widths[GLYPH_W1_HEIGHT_INDEX] = (double)w2_0->value.i;
                widths[GLYPH_W1_V_X_INDEX] = widths[GLYPH_W0_WIDTH_INDEX] / 2.0;
                widths[GLYPH_W1_V_Y_INDEX] = (double)w2_1->value.i;
            }
        }
        if (pdffont11->W2 != NULL && pdffont11->W2->type == PDF_ARRAY) {
            i = 0;
            while(1) {
                pdf_num *c;
                pdf_obj *o;
                if (i + 1 >= pdffont11->W2->size) break;
                (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W2, i, (pdf_obj **)&c);
                if (c->type != PDF_INT) {
                    return_error(gs_error_typecheck);
                }
                (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W2, i + 1, (pdf_obj **)&o);
                if (o->type == PDF_INT) {
                    if (cid >= c->value.i && cid <= ((pdf_num *)o)->value.i) {
                        pdf_num *w1y, *v1x, *v1y;
                        if (i + 4 >= pdffont11->W2->size) {
                            break;
                        }
                        (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W2, i + 1, (pdf_obj **)&w1y);
                        (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W2, i + 1, (pdf_obj **)&v1x);
                        (void)pdfi_array_peek(pdffont11->ctx, pdffont11->W2, i + 1, (pdf_obj **)&v1y);
                        if (w1y == NULL || w1y->type != PDF_INT
                         || v1x == NULL || v1x->type != PDF_INT
                         || v1y == NULL || v1y->type != PDF_INT) {
                            return_error(gs_error_typecheck);
                        }
                        widths[GLYPH_W1_WIDTH_INDEX] = 0;
                        widths[GLYPH_W1_HEIGHT_INDEX] = (double)w1y->value.i;
                        widths[GLYPH_W1_V_X_INDEX] = (double)v1x->value.i;
                        widths[GLYPH_W1_V_Y_INDEX] = (double)v1y->value.i;
                        break;
                    }
                    i += 5;
                }
                else if (o->type == PDF_ARRAY) {
                    pdf_array *a = (pdf_array *)o;
                    int l = a->size - (a->size % 3);

                    if (cid >= c->value.i && cid < c->value.i + (l / 3)) {
                        pdf_num *w1y, *v1x, *v1y;
                        int index = (cid - c->value.i) * 3;
                        (void)pdfi_array_peek(pdffont11->ctx, a, index, (pdf_obj **)&w1y);
                        (void)pdfi_array_peek(pdffont11->ctx, a, index + 1, (pdf_obj **)&v1x);
                        (void)pdfi_array_peek(pdffont11->ctx, a, index + 2, (pdf_obj **)&v1y);
                        if (w1y == NULL || w1y->type != PDF_INT
                         || v1x == NULL || v1x->type != PDF_INT
                         || v1y == NULL || v1y->type != PDF_INT) {
                            return_error(gs_error_typecheck);
                        }
                        widths[GLYPH_W1_WIDTH_INDEX] = 0.0;
                        widths[GLYPH_W1_HEIGHT_INDEX] = (double)w1y->value.i;
                        widths[GLYPH_W1_V_X_INDEX] = (double)v1x->value.i;
                        widths[GLYPH_W1_V_Y_INDEX] = (double)v1y->value.i;
                        break;
                    }
                    i += 2;
                }
                else {
                    return_error(gs_error_typecheck);
                }
            }
        }
    }

    return 0;
}


static int
pdfi_fapi_get_metrics(gs_fapi_font * ff, gs_string * char_name, gs_glyph cid, double *m, bool vertical)
{
    return 0;
}

static int
pdfi_fapi_set_cache(gs_text_enum_t * penum, const gs_font_base * pbfont,
                  const gs_string * char_name, gs_glyph cid,
                  const double pwidth[2], const gs_rect * pbbox,
                  const double Metrics2_sbw_default[4], bool * imagenow)
{
    int code = 0;
    gs_gstate *pgs = penum->pgs;
    float w2[10];
    double widths[6] = {0};

    if (cid > GS_MIN_CID_GLYPH) {
        cid = cid - GS_MIN_CID_GLYPH;
        code = pdfi_get_glyph_metrics((gs_font *)pbfont, cid, widths, true);
    }
    else {
        code = -1;
    }

    if (code < 0) {
        w2[0] = pwidth[0];
        w2[1] = pwidth[1];
    }
    else {
        w2[0] = widths[GLYPH_W0_WIDTH_INDEX] / 1000.0;
        w2[1] = widths[GLYPH_W0_HEIGHT_INDEX] / 1000.0;
    }
    w2[2] = pbbox->p.x;
    w2[3] = pbbox->p.y;
    w2[4] = pbbox->q.x;
    w2[5] = pbbox->q.y;

    w2[6] = widths[GLYPH_W1_WIDTH_INDEX] / 1000.0;
    w2[7] = widths[GLYPH_W1_HEIGHT_INDEX] / 1000.0;
    w2[8] = widths[GLYPH_W1_V_X_INDEX] / 1000.0;
    w2[9] = widths[GLYPH_W1_V_Y_INDEX] / 1000.0;


    if ((code = gs_setcachedevice2((gs_show_enum *) penum, pgs, w2)) < 0) {
        return (code);
    }

    *imagenow = true;
    return (code);
}


static int
pdfi_fapi_build_char(gs_show_enum * penum, gs_gstate * pgs, gs_font * pfont,
                   gs_char chr, gs_glyph glyph)
{
    int code;
    /* gs_fapi_do_char() expects the "natural" glyph, not the offset value */
    if (glyph > GS_MIN_CID_GLYPH)
        glyph -= GS_MIN_CID_GLYPH;

    code =
        gs_fapi_do_char(pfont, pgs, (gs_text_enum_t *) penum, NULL, false,
                        NULL, NULL, chr, glyph, 0);

    return (code);
}

static void
pdfi_get_server_param(gs_fapi_server * I, const char *subtype,
                    char **server_param, int *server_param_size)
{
    return;
}

#if 0
static int
pdfi_fapi_set_cache_metrics(gs_text_enum_t * penum, const gs_font_base * pbfont,
                         const gs_string * char_name, int cid,
                         const double pwidth[2], const gs_rect * pbbox,
                         const double Metrics2_sbw_default[4],
                         bool * imagenow)
{
    return (gs_error_unknownerror);
}

static gs_glyph
pdfi_fapi_encode_char(gs_font * pfont, gs_char pchr, gs_glyph_space_t not_used)
{
    return (gs_glyph) pchr;
}
#endif

int
pdfi_fapi_passfont(pdf_font *font, int subfont, char *fapi_request,
                 char *file_name, byte * font_data, int font_data_len)
{
    char *fapi_id = NULL;
    int code = 0;
    gs_string fdata;
    gs_font_base *pbfont = (gs_font_base *)font->pfont;
    gs_fapi_font local_pdf_ff_stub = pdfi_ff_stub;
    gs_fapi_ttf_cmap_request symbolic_req[GS_FAPI_NUM_TTF_CMAP_REQ] = {{1, 0}, {3, 0}, {3, 1}, {3, 10}, {-1, -1}};
    gs_fapi_ttf_cmap_request nonsymbolic_req[GS_FAPI_NUM_TTF_CMAP_REQ] = {{3, 1}, {1, 0}, {3, 0}, {-1, -1}, {-1, -1}};
    int plat, enc;

    if (!gs_fapi_available(pbfont->memory, NULL)) {
        return (code);
    }

    fdata.data = font_data;
    fdata.size = font_data_len;

    if (font->pdfi_font_type == e_pdf_font_truetype) {
        pdf_font_truetype *ttfont = (pdf_font_truetype *)font;
        *local_pdf_ff_stub.ttf_cmap_req = (ttfont->descflags & 4) ? *symbolic_req : *nonsymbolic_req;
    }
    else {
        /* doesn't really matter for non-ttf */
        *local_pdf_ff_stub.ttf_cmap_req = *nonsymbolic_req;
    }
    /* The plfont should contain everything we need, but setting the client data for the server
     * to pbfont makes as much sense as setting it to NULL.
     */
    gs_fapi_set_servers_client_data(pbfont->memory,
                                    (const gs_fapi_font *)&local_pdf_ff_stub,
                                    (gs_font *)pbfont);

    code =
        gs_fapi_passfont((gs_font *)pbfont, subfont, (char *)file_name, &fdata,
                         (char *)fapi_request, NULL, (char **)&fapi_id,
                         (gs_fapi_get_server_param_callback)
                         pdfi_get_server_param);

    if (code < 0 || fapi_id == NULL) {
        return code;
    }

    if (font->pdfi_font_type == e_pdf_font_truetype) {
        pdf_font_truetype *ttfont = (pdf_font_truetype *)font;
        plat = pbfont->FAPI->ff.ttf_cmap_selected.platform_id;
        enc = pbfont->FAPI->ff.ttf_cmap_selected.encoding_id;
        ttfont->cmap = pdfi_truetype_cmap_none;

        if (plat == 1 && enc == 0) {
            ttfont->cmap = pdfi_truetype_cmap_10;
        }
        else if (plat == 3 && enc == 0) {
            ttfont->cmap = pdfi_truetype_cmap_30;
        }
        else if (plat == 3 && enc == 1) {
            ttfont->cmap = pdfi_truetype_cmap_31;
        }
        else if (plat == 3 && enc == 10) { /* Currently shouldn't arise */
            ttfont->cmap = pdfi_truetype_cmap_310;
        }
    }

    pbfont->procs.build_char = pdfi_fapi_build_char;

    return (code);
}

int
pdfi_fapi_check_cmap_for_GID(gs_font *pfont, uint *c)
{
    if (pfont->FontType == ft_TrueType) {
        gs_font_base *pbfont = (gs_font_base *)pfont;
        gs_fapi_server *I = pbfont->FAPI;

        if (I) {
            I->ff.server_font_data = pbfont->FAPI_font_data;
            I->check_cmap_for_GID(I, c);
            return 0;
        }
    }
    return_error(gs_error_invalidfont);
}
