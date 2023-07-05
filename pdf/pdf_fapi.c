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

/* Interface to FAPI for the PDF interpreter */

/* Include this first so that we don't get a macro redefnition of 'offsetof' */
#include "pdf_int.h"

#include "memory_.h"
#include "gsmemory.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gxfont0.h"
#include "gxfcid.h"

#include "gzstate.h"
#include "gxchar.h"             /* for st_gs_show_enum */
#include "gdebug.h"
#include "gxfapi.h"
#include "gscoord.h"
#include "gspath.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_font.h"
#include "pdf_fontTT.h"
#include "gscencs.h"
#include "gsagl.h"
#include "gxfont1.h"        /* for gs_font_type1_s */
#include "gscrypt1.h"       /* for crypt_c1 */

extern single_glyph_list_t SingleGlyphList[];


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
pdfi_fapi_retrieve_tt_font(gs_fapi_font * ff, void **buf, int *buf_size);

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
    pdfi_fapi_retrieve_tt_font,                     /* retrieve_tt_font */
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
        case gs_fapi_font_feature_GlobalSubrs_count:
            {
                if (pfont->FontType == ft_encrypted2) {
                    pdf_font_cff *pdffont2 = (pdf_font_cff *)pfont->client_data;
                    *ret = pdffont2->GlobalSubrs == NULL ? 0 : pdfi_array_size(pdffont2->GlobalSubrs);
                }
                else {
                    *ret = 0;
                    code = gs_note_error(gs_error_invalidaccess);
                }
                break;
            }
        case gs_fapi_font_feature_Subrs_count:
            {
                if (pfont->FontType == ft_encrypted) {
                    pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                    *ret = pdffont1->Subrs == NULL ? 0 : pdfi_array_size(pdffont1->Subrs);
                }
                else if (pfont->FontType == ft_encrypted2) {
                    pdf_font_cff *pdffont2 = (pdf_font_cff *)pfont->client_data;
                    *ret = pdffont2->Subrs == NULL ? 0 : pdfi_array_size(pdffont2->Subrs);
                }
                else {
                    *ret = 0;
                    code = gs_note_error(gs_error_invalidaccess);
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
        case gs_fapi_font_feature_BlendBlueValues_length:
        case gs_fapi_font_feature_BlendOtherBlues_length:
        case gs_fapi_font_feature_BlendOtherBlues_count:
        case gs_fapi_font_feature_BlendBlueScale_count:
        case gs_fapi_font_feature_BlendBlueShift_count:
        case gs_fapi_font_feature_BlendBlueShift:
        case gs_fapi_font_feature_BlendBlueFuzz_count:
        case gs_fapi_font_feature_BlendBlueFuzz:
        case gs_fapi_font_feature_BlendForceBold_count:
        case gs_fapi_font_feature_BlendForceBold:
        case gs_fapi_font_feature_BlendStdHW_length:
        case gs_fapi_font_feature_BlendStdHW_count:
        case gs_fapi_font_feature_BlendStdHW:
        case gs_fapi_font_feature_BlendStdVW_length:
        case gs_fapi_font_feature_BlendStdVW_count:
        case gs_fapi_font_feature_BlendStdVW:
        case gs_fapi_font_feature_BlendStemSnapH_length:
        case gs_fapi_font_feature_BlendStemSnapH_count:
        case gs_fapi_font_feature_BlendStemSnapH:
        case gs_fapi_font_feature_BlendStemSnapV_length:
        case gs_fapi_font_feature_BlendStemSnapV_count:
        case gs_fapi_font_feature_BlendStemSnapV:
            {
                code = 0;
                *ret = 0;
            }
            break;
        case gs_fapi_font_feature_DollarBlend:
            {
                if (pfont->data.WeightVector.count > 0) { /* If count > 0, it's MM font, and we "have" a $Blend */
                    *ret = 1;
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_DollarBlend_length:
            {
                /* Use the built-in boiler plate */
                *ret = 0;
            }
            break;
        case gs_fapi_font_feature_BlendAxisTypes_count:
            {
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                if (pdffont1->blendaxistypes != NULL) {
                    *ret = (unsigned short)pdffont1->blendaxistypes->size;
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_WeightVector_count:
            {
                *ret = (unsigned short)pfont->data.WeightVector.count;
            }
            break;
        case gs_fapi_font_feature_BlendDesignPositionsArrays_count:
            {
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                if (pdffont1->blenddesignpositions != NULL) {
                    *ret = (unsigned short)pdffont1->blenddesignpositions->size;
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_BlendDesignMapArrays_count:
            {
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                if (pdffont1->blenddesignmap != NULL) {
                    *ret = (unsigned short)pdffont1->blenddesignmap->size;
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_BlendDesignMapSubArrays_count:
            {
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                if (pdffont1->blenddesignmap != NULL) {
                    pdf_array *suba;
                    code = pdfi_array_get(pdffont1->ctx, pdffont1->blenddesignmap, index, (pdf_obj **)&suba);
                    if (code < 0) {
                        *ret = 0;
                        break;
                    }
                    *ret = (unsigned short)suba->size;
                    pdfi_countdown(suba);
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_BlendFontBBox_length:
            {
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                if (pdffont1->blendfontbbox != NULL) {
                    *ret = (unsigned short)pdffont1->blendfontbbox->size;
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_BlendFontBBox:
            {
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
                if (pdffont1->blendfontbbox != NULL) {
                    pdf_array *suba;
                    double d;
                    int ind, aind;
                    ind = index % 4;
                    aind = (index - ind) / 4;
                    code = pdfi_array_get(pdffont1->ctx, pdffont1->blendfontbbox, aind, (pdf_obj **)&suba);
                    if (code < 0) {
                        *ret = 0;
                        break;
                    }
                    code = pdfi_array_get_number(pdffont1->ctx, suba, ind, &d);
                    pdfi_countdown(suba);
                    if (code < 0) {
                        *ret = 0;
                        break;
                    }
                    *ret = (unsigned short)d;
                }
                else {
                    *ret = 0;
                }
            }
            break;
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
                    for (i = 0; i < (pdffont1->Subrs == NULL ? 0 : pdfi_array_size(pdffont1->Subrs)); i++) {
                        pdf_string *subr_str = NULL;
                        code = pdfi_array_get_type(pdffont1->ctx, pdffont1->Subrs, i, PDF_STRING, (pdf_obj **)&subr_str);
                        if (code >= 0) {
                             *ret += subr_str->length;
                        }
                        pdfi_countdown(subr_str);
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
        case gs_fapi_font_feature_WeightVector:
            {
                gs_font_type1 *pfont1 = (gs_font_type1 *) pbfont;
                if (index < pfont1->data.WeightVector.count) {
                    *ret = pfont1->data.WeightVector.values[index];
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_BlendDesignPositionsArrayValue:
            {
                int array_index, subind;
                pdf_array *suba;
                double d;
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pbfont->client_data;
                *ret = 0;

                code = pdfi_array_get(pdffont1->ctx, pdffont1->blenddesignpositions, 0, (pdf_obj **)&suba);
                if (code < 0)
                    break;

                /* The FAPI code assumes gs style storage - i.e. it allocates the maximum number of entries
                   permissable by the spec, and fills in only those required.
                   pdfi doesn't, so we unpick that here
                 */
                index = (index % 8) + (suba->size * index / 8);

                array_index = index / suba->size;
                subind = index % suba->size;
                pdfi_countdown(suba);
                code = pdfi_array_get(pdffont1->ctx, pdffont1->blenddesignpositions, array_index, (pdf_obj**)&suba);
                if (code < 0) {
                    code = 0;
                    break;
                }

                code = pdfi_array_get_number(pdffont1->ctx, suba, subind, &d);
                pdfi_countdown(suba);
                if (code < 0) {
                    code = 0;
                    break;
                }
                *ret = (float)d;
            }
            break;
        case gs_fapi_font_feature_BlendDesignMapArrayValue:
            {
                int i, j, k;
                pdf_array *suba, *subsuba;
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pbfont->client_data;
                *ret = (float)0;
                code = 0;

                for (i = 0; i < pdffont1->blenddesignmap->size && code >= 0; i++) {
                    code = pdfi_array_get(pdffont1->ctx, pdffont1->blenddesignmap, i, (pdf_obj **)&suba);
                    if (code < 0)
                        continue;
                    for (j = 0; j < suba->size && code >= 0; j++) {
                        code = pdfi_array_get(pdffont1->ctx, suba, i, (pdf_obj **)&subsuba);
                        if (code < 0)
                            continue;
                        for (k = 0; k < subsuba->size && code >= 0; k++) {
                            /* The FAPI code assumes gs style storage - i.e. it allocates the maximum number of entries
                               permissable by the spec, and fills in only those required.
                               pdfi doesn't, hence the multiplications by 64.
                             */
                            if ((i * 64) + (j * 64) + k  == index) {
                                double d;
                                code = pdfi_array_get_number(pdffont1->ctx, suba, i, &d);
                                if (code < 0)
                                    continue;
                                *ret = (float)d;
                                pdfi_countdown(subsuba);
                                pdfi_countdown(suba);
                                goto gotit;
                            }
                        }
                        pdfi_countdown(subsuba);
                    }
                    pdfi_countdown(suba);
                }
                code = 0;
            }
gotit:
            break;

        default:
            code = gs_error_undefined;
    }

    return code;
}

/* Only required for multiple masters, I believe. Buffer is guaranteed to exist */
static int
pdfi_fapi_get_name(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *buffer, int len)
{
    gs_font_base *pbfont = (gs_font_base *) ff->client_font_data2;
    int code = 0;

    switch ((int)var_id) {
        case gs_fapi_font_feature_BlendAxisTypes:
            {
                pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pbfont->client_data;
                pdf_name *n;
                code = pdfi_array_get(pdffont1->ctx, pdffont1->blendaxistypes, index, (pdf_obj **)&n);
                if (code < 0)
                    break;
                if (n->length <= len - 1) {
                    memcpy(buffer, n->data, n->length);
                    buffer[n->length] = '\0';
                }
                else
                    code = gs_error_limitcheck;
                pdfi_countdown(n);
            }
            break;
        default:
            code = gs_error_undefined;
    }
    return code;
}

/* Only required for multiple masters, I believe */
static int
pdfi_fapi_get_proc(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *buffer)
{
    int code = 0;

    (void)index;
    (void)buffer;

    switch ((int)var_id) {
        case gs_fapi_font_feature_DollarBlend:
            break;
        default:
            code = gs_error_undefined;
    }
    return code;
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
pdfi_fapi_get_gsubr(gs_fapi_font *ff, int index, byte *buf, int buf_length)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    int code = 0;
    if (pfont->FontType == ft_encrypted2) {
        pdf_font_cff *pdffont2 = (pdf_font_cff *)pfont->client_data;
        if (index > (pdffont2->GlobalSubrs == NULL ? 0 : pdfi_array_size(pdffont2->GlobalSubrs))) {
            code = gs_error_rangecheck;
        }
        else {
            int leniv = (pfont->data.lenIV > 0 ? pfont->data.lenIV : 0);
            pdf_string *subrstring = NULL;

            code = pdfi_array_get(pdffont2->ctx, pdffont2->GlobalSubrs, index, (pdf_obj **)&subrstring);
            if (code >= 0) {
                if (pdfi_type_of(subrstring) == PDF_STRING) {
                    code = subrstring->length - leniv;
                    if (buf && buf_length >= code) {
                        if (ff->need_decrypt && pfont->data.lenIV >= 0) {
                            decode_bytes(buf, subrstring->data, code + leniv, pfont->data.lenIV);
                        }
                        else {
                            memcpy(buf, subrstring->data, code);
                        }
                    }
                }
                else {
                    code = gs_note_error(gs_error_invalidfont);
                }
                pdfi_countdown(subrstring);
            }
        }
    }
    else {
        code = gs_note_error(gs_error_invalidfont);
    }
    return code;
}

static int
pdfi_fapi_get_subr(gs_fapi_font *ff, int index, byte *buf, int buf_length)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    int code = 0;

    if (pfont->FontType == ft_encrypted) {
        pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pfont->client_data;
        if (index > (pdffont1->Subrs == NULL ? 0 : pdfi_array_size(pdffont1->Subrs))) {
            code = gs_note_error(gs_error_rangecheck);
        }
        else {
            int leniv = (pfont->data.lenIV > 0 ? pfont->data.lenIV : 0);
            pdf_string *subr_str = NULL;

            code = pdfi_array_get_type(pdffont1->ctx, pdffont1->Subrs, index, PDF_STRING, (pdf_obj **)&subr_str);
            if (code >= 0) {
                 code = subr_str->length - leniv;
                 if (buf && buf_length >= code) {
                     if (ff->need_decrypt && pfont->data.lenIV >= 0) {
                         decode_bytes(buf, subr_str->data, code + leniv, pfont->data.lenIV);
                     }
                     else {
                         memcpy(buf, subr_str->data, code);
                     }
                 }
            }
            else {
                /* Ignore invalid or missing subrs */
                code = 0;
            }
            pdfi_countdown(subr_str);
        }
    }
    else if (pfont->FontType == ft_encrypted2) {
        pdf_font_cff *pdffont2 = (pdf_font_cff *)pfont->client_data;
        if (index > (pdffont2->Subrs == NULL ? 0 : pdfi_array_size(pdffont2->Subrs))) {
            code = gs_error_rangecheck;
        }
        else {
            int leniv = (pfont->data.lenIV > 0 ? pfont->data.lenIV : 0);
            pdf_string *subrstring;

            if (pdffont2->Subrs == NULL)
                code = gs_note_error(gs_error_invalidfont);
            else
                code = pdfi_array_get(pdffont2->ctx, pdffont2->Subrs, index, (pdf_obj **)&subrstring);
            if (code >= 0) {
                if (pdfi_type_of(subrstring) == PDF_STRING) {
                    if (subrstring->length > 0) {
                        code = subrstring->length - leniv;
                        if (buf && buf_length >= code) {
                            if (ff->need_decrypt && pfont->data.lenIV >= 0) {
                                decode_bytes(buf, subrstring->data, code + leniv, pfont->data.lenIV);
                            }
                            else {
                                memcpy(buf, subrstring->data, code);
                            }
                        }
                    }
                }
                else {
                    code = gs_note_error(gs_error_invalidfont);
                }
                pdfi_countdown(subrstring);
            }
        }
    }
    else {
        code = gs_note_error(gs_error_invalidfont);
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
        if (index > (pdffont1->Subrs == NULL ? 0 : pdfi_array_size(pdffont1->Subrs))) {
            code = gs_error_rangecheck;
        }
        else {
            pdf_string *subr_str = NULL;
            code = pdfi_array_get_type(pdffont1->ctx, pdffont1->Subrs, index, PDF_STRING, (pdf_obj **)&subr_str);
            if (code >= 0) {
                code = subr_str->length;
                if (buf && buf_length >= code) {
                    memcpy(buf, subr_str->data, code);
                }
            }
            else {
                /* Ignore missing or invalid subrs */
                code = 0;
            }
            pdfi_countdown(subr_str);
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

static int
pdfi_fapi_get_glyphname_or_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, gs_glyph ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID)
{
    gs_fapi_server *I = pbfont->FAPI;
    int code = 0;
    pdf_context *ctx = (pdf_context *) ((pdf_font *)pbfont->client_data)->ctx;

    if (pbfont->FontType == ft_CID_TrueType) {
        pdf_cidfont_type2 *pttfont = (pdf_cidfont_type2 *)pbfont->client_data;
        gs_glyph gid;

        if (ccode >= GS_MIN_CID_GLYPH)
            ccode = ccode - GS_MIN_CID_GLYPH;

        if (pttfont->substitute == false) {
            gid = ccode;
            if (pttfont->cidtogidmap != NULL && pttfont->cidtogidmap->length > (ccode << 1) + 1) {
                gid = pttfont->cidtogidmap->data[ccode << 1] << 8 | pttfont->cidtogidmap->data[(ccode << 1) + 1];
            }
            cr->client_char_code = ccode;
            cr->char_codes[0] = gid;
            cr->is_glyph_index = true;
        }
        else { /* If the composite font has a decoding, then this is a subsituted CIDFont with a "known" ordering */
            unsigned int gc = 0, cc = (unsigned int)ccode;
            byte uc[4];
            int l, i;

            if (penum->text.operation & TEXT_FROM_SINGLE_CHAR) {
                cc = penum->text.data.d_char;
            } else if (penum->text.operation & TEXT_FROM_SINGLE_GLYPH) {
                cc = penum->text.data.d_glyph - GS_MIN_CID_GLYPH;
            }
            else {
                byte *c = (byte *)&penum->text.data.bytes[penum->index - penum->bytes_decoded];
                cc = 0;
                for (i = 0; i < penum->bytes_decoded ; i++) {
                    cc |= c[i] << ((penum->bytes_decoded - 1) - i) * 8;
                }
            }

            l = penum->orig_font->procs.decode_glyph((gs_font *)penum->orig_font, ccode, (gs_char)cc, (ushort *)uc, 4);
            if (l > 0 && l <= sizeof(uc)) {
                cc = 0;
                for (i = 0; i < l; i++) {
                    cc |= uc[l - 1 - i] << (i * 8);
                }
            }
            else
                cc = ccode;
            /* All known cmap tables map 32 to the space glyph, so if it looks like
               we're going to use a notdef, then substitute the glyph for the code point 32
             */
            if (l != 0) {
                code = pdfi_fapi_check_cmap_for_GID((gs_font *)pbfont, cc, &gc);
                if (code < 0 || gc == 0)
                    (void)pdfi_fapi_check_cmap_for_GID((gs_font *)pbfont, 32, &gc);
            }
            else {
                if (ccode == 0) {
                    (void)pdfi_fapi_check_cmap_for_GID((gs_font *)pbfont, 32, &gc);
                }
                else {
                    gc = ccode;
                }
            }

            cr->client_char_code = ccode;
            cr->char_codes[0] = gc;
            cr->is_glyph_index = true;
        }
        return 0;
    }
    /* For cff based CIDFonts (and thus "Type 1" based CIDFonts, since the code
     * is common to both) and cff fonts we only claim to the FAPI server that
     * we have one (or two?) glyphs, because it makes the "stub" font simpler.
     * But Freetype bounds checks the character code (or gid) against the number
     * of glyphs in the font *before* asking us for the glyph data. So we need
     * to extract the charstring here, store it in the fapi font, and unpack it
     * in pdfi_fapi_get_glyph(), so we can then claim we're always rendering glyph
     * index zero, and thus pass the bounds check.
     */
    else if (penum->current_font->FontType == ft_CID_encrypted) {
        gs_font_cid0 *pfont9 = (gs_font_cid0 *)penum->current_font;
        gs_glyph_data_t gd;
        int f_ind;

        code = (*pfont9->cidata.glyph_data)((gs_font_base *)pfont9, ccode, &gd, &f_ind);
        if (code < 0) {
            code = (*pfont9->cidata.glyph_data)((gs_font_base *)pfont9, 0, &gd, &f_ind);
        }
        if (code < 0)
            return_error(gs_error_invalidfont);

        I->ff.char_data = (void *)gd.bits.data;
        I->ff.char_data_len = gd.bits.size;

        cr->client_char_code = 0;
        cr->char_codes[0] = 0;
        cr->is_glyph_index = true;
        I->ff.client_font_data2 = penum->fstack.items[penum->fstack.depth].font;

        return 0;
    }
    else if (pbfont->FontType == ft_encrypted2) {
        pdf_font_cff *cfffont = (pdf_font_cff *)pbfont->client_data;
        pdf_name *glyphname = NULL;
        pdf_string *charstr = NULL;
        gs_const_string gname;

        code = (*ctx->get_glyph_name)((gs_font *)pbfont, ccode, &gname);
        if (code >= 0) {
            code = pdfi_name_alloc(ctx, (byte *) gname.data, gname.size, (pdf_obj **) &glyphname);
            if (code < 0)
                return code;
            pdfi_countup(glyphname);
        }

        if (code < 0) {
            pdfi_countdown(glyphname);
            return code;
        }
        code = pdfi_dict_get_by_key(cfffont->ctx, cfffont->CharStrings, glyphname, (pdf_obj **)&charstr);
        if (code < 0) {
            code = pdfi_map_glyph_name_via_agl(cfffont->CharStrings, glyphname, &charstr);
            if (code < 0)
                code = pdfi_dict_get(cfffont->ctx, cfffont->CharStrings, ".notdef", (pdf_obj **)&charstr);
        }
        pdfi_countdown(glyphname);
        if (code < 0)
            return code;

        I->ff.char_data = charstr->data;
        I->ff.char_data_len = charstr->length;

        cr->client_char_code = 0;
        cr->char_codes[0] = 0;
        cr->is_glyph_index = true;

        pdfi_countdown(charstr);
        return code;
    }
    else if (pbfont->FontType == ft_TrueType) {
        /* I'm not clear if the heavy lifting should be here or in pdfi_tt_encode_char() */
        pdf_font_truetype *ttfont = (pdf_font_truetype *)pbfont->client_data;
        pdf_name *GlyphName = NULL;
        gs_const_string gname;
        int i;
        uint cc = 0;
        if ((ttfont->descflags & 4) != 0) {
            if (ttfont->cmap == pdfi_truetype_cmap_30) {

                ccode = cr->client_char_code;
                code = pdfi_fapi_check_cmap_for_GID((gs_font *)pbfont, (uint)ccode, &cc);
                if (code < 0 || cc == 0)
                    cr->char_codes[0] = ccode | 0xf0 << 8;
                else
                    cr->char_codes[0] = ccode;
                cr->is_glyph_index = false;
            }
            else {
                cr->char_codes[0] = cr->client_char_code;
                cr->is_glyph_index = false;
            }
        }
        else {

            code = (*ctx->get_glyph_name)((gs_font *)pbfont, ccode, &gname);
            if (code >= 0) {
                code = pdfi_name_alloc(ctx, (byte *) gname.data, gname.size, (pdf_obj **) &GlyphName);
                if (code >= 0)
                    pdfi_countup(GlyphName);
            }

            cr->char_codes[0] = cr->client_char_code;
            cr->is_glyph_index = false;
            if (code < 0)
                return 0;

            if (ttfont->cmap == pdfi_truetype_cmap_10) {
                gs_glyph g;

                g = gs_c_name_glyph((const byte *)GlyphName->data, GlyphName->length);
                if (g != GS_NO_GLYPH) {
                    g = (gs_glyph)gs_c_decode(g, ENCODING_INDEX_MACROMAN);
                }
                else {
                    g = GS_NO_CHAR;
                }

                if (g != GS_NO_CHAR) {
                    code = pdfi_fapi_check_cmap_for_GID((gs_font *)pbfont, (uint)g, &cc);
                }

                if (code < 0 || cc == 0) {
                    gs_font_type42 *pfonttt = (gs_font_type42 *)pbfont;
                    gs_string gname;
                    gname.data = GlyphName->data;
                    gname.size = GlyphName->length;

                    code = pdfi_find_post_entry(pfonttt, (gs_const_string *)&gname, &cc);
                    if (code >= 0) {
                        cr->char_codes[0] = cc;
                        cr->is_glyph_index = true;
                    }
                }
                else {
                    cr->char_codes[0] = g;
                    cr->is_glyph_index = false;
                }
            }
            else if (ttfont->cmap == pdfi_truetype_cmap_31) {
                    unsigned int cc;
                    single_glyph_list_t *sgl = (single_glyph_list_t *)&(SingleGlyphList);
                    /* Not to spec, but... if we get a "uni..." formatted name, use
                       the hex value from that.
                     */
                    if (GlyphName->length > 5 && !strncmp((char *)GlyphName->data, "uni", 3)) {
                        char gnbuf[64];
                        int l = (GlyphName->length - 3) > 63 ? 63 : GlyphName->length - 3;

                        memcpy(gnbuf, GlyphName->data + 3, l);
                        gnbuf[l] = '\0';
                        l = sscanf(gnbuf, "%x", &cc);
                        if (l > 0)
                            cr->char_codes[0] = cc;
                        else
                            cr->char_codes[0] = 0;
                    }
                    else {
                        /* Slow linear search, we could binary chop it */
                        for (i = 0; sgl[i].Glyph != 0x00; i++) {
                            if (sgl[i].Glyph[0] == GlyphName->data[0]
                                && strlen(sgl[i].Glyph) == GlyphName->length
                                && !strncmp((char *)sgl[i].Glyph, (char *)GlyphName->data, GlyphName->length))
                                break;
                        }
                        if (sgl[i].Glyph != NULL) {
                            code = pdfi_fapi_check_cmap_for_GID((gs_font *)pbfont, (uint)sgl[i].Unicode, &cc);
                            if (code < 0 || cc == 0)
                                cc = 0;
                            else
                                cc = sgl[i].Unicode;
                        }
                        else
                            cc = 0;

                        if (cc == 0) {
                            gs_font_type42 *pfonttt = (gs_font_type42 *)pbfont;
                            gs_string gname;
                            gname.data = GlyphName->data;
                            gname.size = GlyphName->length;

                            code = pdfi_find_post_entry(pfonttt, (gs_const_string *)&gname, &cc);
                            if (code >= 0) {
                                cr->char_codes[0] = cc;
                                cr->is_glyph_index = true;
                            }
                        }
                        else {
                            cr->char_codes[0] = cc;
                            cr->is_glyph_index = false;
                        }
                    }
            }
            pdfi_countdown(GlyphName);
            return 0;
        }
    }
    else if (pbfont->FontType == ft_encrypted) {
        gs_const_string gname;
        code = (*ctx->get_glyph_name)((gs_font *)pbfont, ccode, &gname);
        I->ff.char_data = enc_char_name->data = (byte *)gname.data;
        I->ff.char_data_len = enc_char_name->size = gname.size;
        cr->is_glyph_index = false;
    }
    return code;
}

static int
pdfi_fapi_get_glyph(gs_fapi_font * ff, gs_glyph char_code, byte * buf, int buf_length)
{
    gs_font_base *pbfont = (gs_font_base *) ff->client_font_data2;
    gs_fapi_server *I = pbfont->FAPI;
    int code = 0;
    pdf_name *encn;
    int cstrlen = 0;

    if (ff->is_type1) {

        if (pbfont->FontType == ft_encrypted) {
            gs_font_type1 *pfont1 = (gs_font_type1 *) pbfont;
            pdf_name *glyphname = NULL;
            pdf_string *charstring = NULL;
            pdf_font_type1 *pdffont1 = (pdf_font_type1 *)pbfont->client_data;
            int leniv = pfont1->data.lenIV > 0 ? pfont1->data.lenIV : 0;

            if (I->ff.char_data != NULL) {
                code = pdfi_name_alloc(pdffont1->ctx, (byte *)I->ff.char_data, I->ff.char_data_len, (pdf_obj **)&glyphname);
                if (code < 0)
                    return code;
                pdfi_countup(glyphname);
                code = pdfi_dict_get_by_key(pdffont1->ctx, pdffont1->CharStrings, glyphname, (pdf_obj **)&charstring);
                if (code < 0) {
                    code = pdfi_map_glyph_name_via_agl(pdffont1->CharStrings, glyphname, &charstring);
                    if (code < 0) {
                        code = pdfi_dict_get(pdffont1->ctx, pdffont1->CharStrings, ".notdef", (pdf_obj **)&charstring);
                        if (code < 0) {
                            pdfi_countdown(glyphname);
                            code = gs_note_error(gs_error_invalidfont);
                            goto done;
                        }
                    }
                }
                pdfi_countdown(glyphname);
                cstrlen = charstring->length - leniv;
                if (buf != NULL && cstrlen <= buf_length) {
                    if (ff->need_decrypt && pfont1->data.lenIV >= 0)
                        decode_bytes(buf, charstring->data, cstrlen + leniv, leniv);
                    else
                        memcpy(buf, charstring->data, charstring->length);
                }
                pdfi_countdown(charstring);
                /* Trigger the seac case below - we can do this safely
                   because I->ff.char_data points to a string managed
                   by the Encoding array in the pdf_font object
                 */
                if (buf != NULL)
                    I->ff.char_data = NULL;
            }
            else { /* SEAC */
                gs_const_string encstr;
                gs_glyph enc_ind = gs_c_known_encode(char_code, ENCODING_INDEX_STANDARD);

                if (enc_ind == GS_NO_GLYPH) {
                    code = gs_error_invalidfont;
                }
                else {
                   gs_c_glyph_name(enc_ind, &encstr);
                   code = pdfi_name_alloc(pdffont1->ctx, (byte *)encstr.data, encstr.size, (pdf_obj **)&encn);
                   if (code < 0)
                       goto done;

                   pdfi_countup(encn);
                   code = pdfi_dict_get_by_key(pdffont1->ctx, pdffont1->CharStrings, encn, (pdf_obj **)&charstring);
                   pdfi_countdown(encn);
                   if (code < 0)
                       goto done;
                   cstrlen = charstring->length - leniv;
                   if (buf != NULL && code <= buf_length) {
                       if (ff->need_decrypt && leniv >= 0)
                           decode_bytes(buf, charstring->data, cstrlen + leniv, leniv);
                       else
                           memcpy(buf, charstring->data, charstring->length);
                   }
                   pdfi_countdown(charstring);
                }
            }
        }
        else if (pbfont->FontType == ft_CID_encrypted || pbfont->FontType == ft_encrypted2) {
            gs_font_type1 *pfont = (gs_font_type1 *) (gs_font_base *) ff->client_font_data;
            int leniv = pfont->data.lenIV > 0 ? pfont->data.lenIV : 0;

            if (I->ff.char_data_len > 0 && I->ff.char_data != NULL) {
                cstrlen = I->ff.char_data_len - leniv;

                if (buf && buf_length >= cstrlen) {
                    if (ff->need_decrypt && pfont->data.lenIV >= 0)
                        decode_bytes(buf, I->ff.char_data, cstrlen + leniv, leniv);
                    else
                        memcpy(buf, I->ff.char_data, cstrlen);

                    /* Trigger the seac case below - we can do this safely
                       because I->ff.char_data points to a string managed
                       by the charstrings dict in the pdf_font object
                     */
                    I->ff.char_data = NULL;
                }
            }
            else {
                pdf_font_cff *pdffont = (pdf_font_cff *)pfont->client_data;
                pdf_name *encn;
                pdf_string *charstring;

                if (pbfont->FontType == ft_CID_encrypted) {
                    /* we're dealing with a font that's an entry in a CIDFont FDArray
                       so don't try to use an Encoding
                     */
                     char indstring[33];
                     int l;
                     l = gs_snprintf(indstring, 32, "%u", (unsigned int)char_code);
                     code = pdfi_name_alloc(pdffont->ctx, (byte *)indstring, l, (pdf_obj **)&encn);
                }
                else {
                    gs_const_string encstr;
                    gs_glyph enc_ind = gs_c_known_encode(char_code, ENCODING_INDEX_STANDARD);

                    /* Nonsense values for char_code should probably trigger an error (as above)
                       but other consumers seem tolerant, so....
                     */
                    if (enc_ind == GS_NO_GLYPH)
                        enc_ind = gs_c_known_encode(0, ENCODING_INDEX_STANDARD);

                    code = gs_c_glyph_name(enc_ind, &encstr);

                    if (code < 0)
                        code = pdfi_name_alloc(pdffont->ctx, (byte *)".notdef", 7, (pdf_obj **)&encn);
                    else
                        code = pdfi_name_alloc(pdffont->ctx, (byte *)encstr.data, encstr.size, (pdf_obj **)&encn);
                }
                if (code < 0)
                    goto done;

                pdfi_countup(encn);
                code = pdfi_dict_get_by_key(pdffont->ctx, pdffont->CharStrings, encn, (pdf_obj **)&charstring);
                pdfi_countdown(encn);
                if (code < 0)
                    goto done;
                cstrlen = charstring->length - leniv;
                if (buf != NULL && cstrlen <= buf_length) {
                    if (ff->need_decrypt && pfont->data.lenIV >= 0)
                        decode_bytes(buf, charstring->data, cstrlen + leniv, leniv);
                    else
                        memcpy(buf, charstring->data, charstring->length);
                }
                pdfi_countdown(charstring);
            }

        }
    }
    else {
        gs_font_type42 *pfont42 = (gs_font_type42 *)pbfont;
        gs_glyph_data_t pgd;

        code = pfont42->data.get_outline(pfont42, char_code, &pgd);
        cstrlen = pgd.bits.size;
        if (code >= 0) {
            if (buf && buf_length >= cstrlen) {
                memcpy(buf, pgd.bits.data, cstrlen);
            }
        }
    }
done:
    if (code < 0)
        return code;

    return cstrlen;
}

static int
pdfi_fapi_serialize_tt_font(gs_fapi_font * ff, void *buf, int buf_size)
{
    return 0;
}

static int
pdfi_fapi_retrieve_tt_font(gs_fapi_font * ff, void **buf, int *buf_size)
{
    gs_font_type42 *pfonttt = (gs_font_type42 *) ff->client_font_data;
    pdf_font_truetype *pdfttf = (pdf_font_truetype *)pfonttt->client_data;

    *buf = pdfttf->sfnt->data;
    *buf_size = pdfttf->sfnt->length;

    return 0;
}


static int
pdfi_get_glyphdirectory_data(gs_fapi_font * ff, int char_code,
                           const byte ** ptr)
{
    return (0);
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
    int code2;
    gs_gstate *pgs = penum->pgs;
    float w2[10];
    double widths[6] = {0};
    gs_point pt;
    gs_font_base *pbfont1 = (gs_font_base *)pbfont;
    gs_matrix imat;
    gs_matrix mat1;

    mat1 = pbfont1->FontMatrix;

    if (penum->orig_font->FontType == ft_composite) {

        if (cid >= GS_MIN_CID_GLYPH) {
            cid = cid - GS_MIN_CID_GLYPH;
        }

        if (pbfont->FontType == ft_encrypted || pbfont->FontType == ft_encrypted2) {
            gs_fapi_server *I = (gs_fapi_server *)pbfont1->FAPI;
            pbfont1 = (gs_font_base *)I->ff.client_font_data2;
            /* The following cannot fail - if the matrix multiplication didn't work
               we'd have errored out at a higher level
             */
            (void)gs_matrix_multiply(&pbfont->FontMatrix, &pbfont1->FontMatrix, &mat1);
        }

        code = pdfi_get_cidfont_glyph_metrics((gs_font *)pbfont1, cid, widths, true);
        if (code < 0) {
            /* Insert warning here! */
            code = 0; /* Using the defaults */
        }
    }
    else {
        code = -1;
    }

    /* Since we have to tranverse a few things by the inverse font matrix,
       invert the matrix once upfront.
     */
    code2 = gs_matrix_invert(&mat1, &imat);
    if (code2 < 0)
        return code2; /* By this stage, this is basically impossible */

    if (code < 0) {
        w2[0] = (float)pwidth[0];
        w2[1] = (float)pwidth[1];
    }
    else {
        /* gs_distance_transform() cannot return an error */
        (void)gs_distance_transform(widths[GLYPH_W0_WIDTH_INDEX], widths[GLYPH_W0_HEIGHT_INDEX], &imat, &pt);

        w2[0] = pt.x / 1000.0;
        w2[1] = pt.y / 1000.0;
    }
    w2[2] = pbbox->p.x;
    w2[3] = pbbox->p.y;
    w2[4] = pbbox->q.x;
    w2[5] = pbbox->q.y;

    (void)gs_distance_transform(widths[GLYPH_W1_WIDTH_INDEX], widths[GLYPH_W1_HEIGHT_INDEX], &imat, &pt);
    w2[6] =  pt.x / 1000.0;
    w2[7] =  pt.y / 1000.0;
    (void)gs_distance_transform(widths[GLYPH_W1_V_X_INDEX], widths[GLYPH_W1_V_Y_INDEX], &imat, &pt);
    w2[8] =  pt.x / 1000.0;
    w2[9] =  pt.y / 1000.0;

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
    int code = 0;
    gs_font_base *pbfont1;
    gs_fapi_server *I;

    /* gs_fapi_do_char() expects the "natural" glyph, not the offset value */
    if (glyph >= GS_MIN_CID_GLYPH)
        glyph -= GS_MIN_CID_GLYPH;

    pbfont1 = (gs_font_base *)pfont;

    if (penum->fstack.depth >= 0) {
        gs_font_cid0 *cidpfont = (gs_font_cid0 *)penum->fstack.items[penum->fstack.depth].font;
        if (cidpfont->FontType == ft_CID_encrypted) {
            pbfont1 = (gs_font_base *)cidpfont->cidata.FDArray[penum->fstack.items[penum->fstack.depth].index];
            I = (gs_fapi_server *)pbfont1->FAPI;
            I->ff.client_font_data2 = cidpfont;
        }
    }
    /* If between the font's creation and now another interpreter has driven FAPI (i.e. in a Postscript Begin/EndPage
       context, the FAPI server data may end up set appropriately for the other interpreter, if that's happened, put
       ours back before trying to interpret the glyph.
    */
    if (((gs_fapi_server *)pbfont1->FAPI)->ff.get_glyphname_or_cid != pdfi_fapi_get_glyphname_or_cid) {
        code = pdfi_fapi_passfont((pdf_font *)pbfont1->client_data, 0, NULL, NULL, NULL, 0);
    }

    if (code >= 0)
        code = gs_fapi_do_char((gs_font *)pbfont1, pgs, (gs_text_enum_t *) penum, NULL, false, NULL, NULL, chr, glyph, 0);

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
    gs_string *fdatap = &fdata;
    gs_font_base *pbfont = (gs_font_base *)font->pfont;
    gs_fapi_font local_pdf_ff_stub = pdfi_ff_stub;
    gs_fapi_ttf_cmap_request symbolic_req[GS_FAPI_NUM_TTF_CMAP_REQ] = {{3, 0}, {1, 0}, {3, 1}, {3, 10}, {-1, -1}};
    gs_fapi_ttf_cmap_request nonsymbolic_req[GS_FAPI_NUM_TTF_CMAP_REQ] = {{3, 1}, {1, 0}, {3, 0}, {-1, -1}, {-1, -1}};
    int plat, enc;

    if (!gs_fapi_available(pbfont->memory, NULL)) {
        return (code);
    }

    if (font->pdfi_font_type == e_pdf_font_truetype) {
        fdatap = NULL;
    }
    else if (font->pdfi_font_type == e_pdf_cidfont_type2) {
        fdatap->data = ((pdf_cidfont_type2 *)font)->sfnt->data;
        fdatap->size = ((pdf_cidfont_type2 *)font)->sfnt->length;
    }
    else {
        fdatap->data = font_data;
        fdatap->size = font_data_len;
    }

    if (font->pdfi_font_type == e_pdf_font_truetype) {
        pdf_font_truetype *ttfont = (pdf_font_truetype *)font;
        *local_pdf_ff_stub.ttf_cmap_req = (ttfont->descflags & 4) ? *symbolic_req : *nonsymbolic_req;
    }
    else {
        /* doesn't really matter for non-ttf */
        *local_pdf_ff_stub.ttf_cmap_req = *nonsymbolic_req;
    }

    gs_fapi_set_servers_client_data(pbfont->memory,
                                    (const gs_fapi_font *)&local_pdf_ff_stub,
                                    (gs_font *)pbfont);

    code =
        gs_fapi_passfont((gs_font *)pbfont, subfont, (char *)file_name, fdatap,
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
pdfi_fapi_check_cmap_for_GID(gs_font *pfont, uint cid, uint *gid)
{
    if (pfont->FontType == ft_TrueType
     || pfont->FontType == ft_CID_TrueType) {
        gs_font_base *pbfont = (gs_font_base *)pfont;
        gs_fapi_server *I = pbfont->FAPI;

        if (I) {
            uint c = cid;
            I->ff.server_font_data = pbfont->FAPI_font_data;
            I->check_cmap_for_GID(I, &c);
            *gid = c;
            return 0;
        }
    }
    return_error(gs_error_invalidfont);
}
