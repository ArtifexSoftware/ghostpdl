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

/* code for CFF (type 1C) font handling */

#include "pdf_int.h"

#include "gscedata.h"
#include "gscencs.h"
#include "gxfont0.h"
#include "gxfcid.h"

#include "pdf_types.h"
#include "pdf_font_types.h"
#include "pdf_font.h"
#include "pdf_font1C.h"
#include "pdf_fontps.h"
#include "pdf_dict.h"
#include "pdf_deref.h"
#include "pdf_file.h"
#include "pdf_array.h"

#include "gxtype1.h"        /* for gs_type1_state_s */
#include "gsutil.h"        /* For gs_next_ids() */

static byte *
pdfi_find_cff_index(byte *p, byte *e, int idx, byte **pp, byte **ep);

/* This is a super set of the contents of a pdfi Type 1C font/CIDFont.
   Meaning we can store everying as we interpret, and not worry
   about the actual font type until the end
 */
typedef struct pdfi_cff_font_priv_s {
    pdf_font_common;
    pdf_array *Subrs;
    int NumSubrs;
    pdf_array *GlobalSubrs;
    int NumGlobalSubrs;
    pdf_dict *CharStrings;
    byte *cffdata;
    byte *cffend;
    byte *gsubrs;
    byte *subrs;
    byte *charstrings;
    int ncharstrings;
    pdf_dict *CIDSystemInfo;
    int64_t DW;
    pdf_array *W;
    pdf_array *DW2;
    pdf_array *W2;
    pdf_buffer *cidtogidmap;
    pdf_array *FDArray;
    /* The registry and ordering strings in gs_font_cid0_data are just references to
       strings assumed to be managed be managed by the interpreter - so we have to stash
       them in the pdfi font, too.
     */
    pdf_string *registry;
    pdf_string *ordering;
    int supplement;
    int cidcount;
    int uidbase;
    font_proc_glyph_info((*orig_glyph_info));
} pdfi_cff_font_priv;

/* Same thing for the Ghostscript font
 */
typedef struct pdfi_gs_cff_font_priv_s {
    gs_font_base_common;
    gs_type1_data type1data;
    gs_font_cid0_data cidata;
    bool forcecid;
    pdfi_cff_font_priv pdfcffpriv;
} pdfi_gs_cff_font_priv;

typedef struct pdfi_gs_cff_font_common_priv_s {
    gs_font_base_common;
} pdfi_gs_cff_font_common_priv;

typedef struct cff_font_offsets_s
{
    unsigned int fdarray_off;
    unsigned int fdselect_off;
    unsigned int charset_off;
    unsigned int encoding_off;
    unsigned int strings_off;
    unsigned int strings_size;
    unsigned int private_off;
    unsigned int private_size;
    bool have_ros;
    bool have_matrix;
} cff_font_offsets;

static int
pdfi_make_string_from_sid(pdf_context *ctx, pdf_obj **str,
                          pdfi_cff_font_priv *font, cff_font_offsets *offsets, unsigned int sid);

static void
pdfi_init_cff_font_priv(pdf_context *ctx, pdfi_gs_cff_font_priv *cffpriv,
                        byte *buf, int buflen, bool for_fdarray);

static int
pdfi_alloc_cff_font(pdf_context *ctx, pdf_font_cff ** font, uint32_t obj_num, bool for_fdarray);

/* CALLBACKS */
static int
pdfi_cff_glyph_data(gs_font_type1 *pfont, gs_glyph glyph, gs_glyph_data_t *pgd)
{
    int code = 0;
    pdf_font_cff *cfffont = (pdf_font_cff *) pfont->client_data;
    pdf_context *ctx = (pdf_context *) cfffont->ctx;
    pdf_name *glyphname = NULL;
    pdf_string *charstring = NULL;

    /* Getting here with Encoding == NULL means it's a subfont from an FDArray
       so we index directly by gid
     */
    if (cfffont->Encoding == NULL) {
        char indstring[33];
        int l = gs_snprintf(indstring, sizeof(indstring), "%u", (unsigned int)glyph);

        code = pdfi_name_alloc(ctx, (byte *) indstring, l, (pdf_obj **) &glyphname);
        if (code >= 0)
            pdfi_countup(glyphname);
    }
    else {
        gs_const_string gname;
        code = (*ctx->get_glyph_name)((gs_font *)pfont, glyph, &gname);
        if (code >= 0) {
            code = pdfi_name_alloc(ctx, (byte *) gname.data, gname.size, (pdf_obj **) &glyphname);
            if (code >= 0)
                pdfi_countup(glyphname);
        }
    }
    if (code >= 0) {
        code = pdfi_dict_get_by_key(ctx, cfffont->CharStrings, glyphname, (pdf_obj **) &charstring);
        if (code < 0) {
            code = pdfi_map_glyph_name_via_agl(cfffont->CharStrings, glyphname, &charstring);
        }
        if (code >= 0)
            gs_glyph_data_from_bytes(pgd, charstring->data, 0, charstring->length, NULL);
    }

    pdfi_countdown(glyphname);
    pdfi_countdown(charstring);

    return code;
}

static int
pdfi_cff_subr_data(gs_font_type1 *pfont, int index, bool global, gs_glyph_data_t *pgd)
{
    int code = 0;
    pdf_font_cff *cfffont = (pdf_font_cff *) pfont->client_data;

    if ((global &&index >= cfffont->NumGlobalSubrs)||(!global &&index >= cfffont->NumSubrs)) {
        code = gs_note_error(gs_error_rangecheck);
    }
    else {
        pdf_string *subrstring;
        pdf_array *s = global ? cfffont->GlobalSubrs : cfffont->Subrs;

        code = pdfi_array_get(cfffont->ctx, s, (uint64_t) index, (pdf_obj **) &subrstring);
        if (code >= 0) {
            gs_glyph_data_from_bytes(pgd, subrstring->data, 0, subrstring->length, NULL);
            pdfi_countdown(subrstring);
        }
    }
    return code;
}

static int
pdfi_cff_seac_data(gs_font_type1 *pfont, int ccode, gs_glyph *pglyph, gs_const_string *gstr, gs_glyph_data_t *pgd)
{
    int code = 0;
    pdf_font_cff *cfffont = (pdf_font_cff *) pfont->client_data;
    pdf_context *ctx = (pdf_context *) cfffont->ctx;
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
            code = pdfi_dict_get_by_key(ctx, cfffont->CharStrings, glyphname, (pdf_obj **)&charstring);
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
pdfi_cff_push(void *callback_data, const fixed *pf, int count)
{
    (void)callback_data;
    (void)pf;
    (void)count;
    return 0;
}
static int
pdfi_cff_pop(void *callback_data, fixed *pf)
{
    (void)callback_data;
    (void)pf;
    return 0;
}

static int
pdfi_cff_enumerate_glyph(gs_font *pfont, int *pindex,
                         gs_glyph_space_t glyph_space, gs_glyph *pglyph)
{
    int code, j;
    pdf_name *key = NULL;
    uint64_t i = (uint64_t) *pindex;
    pdf_dict *cstrings;
    pdf_font *pdffont = (pdf_font *) pfont->client_data;
    pdf_context *ctx = (pdf_context *) pdffont->ctx;

    (void)glyph_space;

    /* Slightly naff: if build_char is NULL, this is an FDArray subfont */
    if (pfont->procs.build_char == NULL) {
        *pindex = 0;
        *pglyph = GS_NO_GLYPH;
        return 0;
    }
    else if (pdffont->pdfi_font_type == e_pdf_cidfont_type0) {
        pdf_cidfont_type0 *cffcidfont = (pdf_cidfont_type0 *) pdffont;
        cstrings = cffcidfont->CharStrings;
    }
    else {
        pdf_font_cff *cfffont = (pdf_font_cff *) pdffont;

        cstrings = cfffont->CharStrings;
    }
    if (*pindex <= 0)
        code = pdfi_dict_key_first(pdffont->ctx, cstrings, (pdf_obj **) &key, &i);
    else
        code = pdfi_dict_key_next(pdffont->ctx, cstrings, (pdf_obj **) &key, &i);
    if (code < 0) {
        i = 0;
        code = gs_note_error(gs_error_undefined);
    }
    /* If Encoding == NULL, it's an FDArray subfont */
    else if (pdffont->pdfi_font_type != e_pdf_cidfont_type0 && pdffont->Encoding != NULL) {
        unsigned int nindex;
        code = (*ctx->get_glyph_index)(pfont, key->data, key->length, &nindex);
        if (code < 0) {
            code = (*ctx->get_glyph_index)(pfont, (byte *)".notdef", 7, &nindex);
            if (code < 0)
                *pglyph = GS_NO_GLYPH;
            else
                *pglyph = (gs_glyph)nindex;
        }
        else
            *pglyph = (gs_glyph)nindex;
    }
    else {
        char kbuf[32];
        int l;
        unsigned int val;
        /* If this font started life as a CFF font that we've force to
           act like a CIDFont, we can end up with a ".notdef" glyph name
         */
        if (key->length == 7 && memcmp(key->data, ".notdef", 7) == 0) {
            val = 0;
            l = 1;
        }
        else {
            memcpy(kbuf, key->data, key->length);
            kbuf[key->length] = 0;

            l = sscanf(kbuf, "%ud", &val);
        }
        if (l > 0) {
            pdf_cidfont_type0 *cffcidfont = (pdf_cidfont_type0 *) pdffont;
            if (cffcidfont->cidtogidmap != NULL && cffcidfont->cidtogidmap->length > 0) {
                for (j = (cffcidfont->cidtogidmap->length >> 1) - 1; j >= 0; j--) {
                    if (val == (cffcidfont->cidtogidmap->data[j << 1] << 8 | cffcidfont->cidtogidmap->data[(j << 1) + 1])) {
                        val = j;
                        break;
                    }
                }
            }
            *pglyph = (gs_glyph) (val) + GS_MIN_CID_GLYPH;
        }
    }
    *pindex = (int)i;
    pdfi_countdown(key);
    return code;
}

/* This *should* only get called for SEAC lookups, which have to come from StandardEncoding
   so just try to lookup the string in the standard encodings
 */
int
pdfi_cff_global_glyph_code(const gs_font *pfont, gs_const_string *gstr, gs_glyph *pglyph)
{
    *pglyph = gs_c_name_glyph(gstr->data, gstr->size);
    return 0;
}

static int
pdfi_cff_glyph_outline(gs_font *pfont, int WMode, gs_glyph glyph,
                       const gs_matrix *pmat, gx_path *ppath, double sbw[4])
{
    gs_glyph_data_t gd;
    gs_glyph_data_t *pgd = &gd;
    gs_font_type1 *pfont1;
    int code;

    if (pfont->FontType == ft_CID_encrypted) {
        gs_font_cid0 *pfcid0 = (gs_font_cid0 *) pfont;
        int fididx = 0;

        code = (*pfcid0->cidata.glyph_data) ((gs_font_base *) pfont, glyph, pgd, &fididx);
        if (fididx < pfcid0->cidata.FDArray_size)
            pfont1 = pfcid0->cidata.FDArray[fididx];
        else
            code = gs_note_error(gs_error_invalidaccess);
    }
    else {
        pfont1 = (gs_font_type1 *) pfont;
        code = (*pfont1->data.procs.glyph_data) ((gs_font_type1 *) pfont, glyph, pgd);
    }

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
pdfi_cff_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat, int members, gs_glyph_info_t *info)
{
    if ((members & GLYPH_INFO_OUTLINE_WIDTHS) == 0)
        return gs_type1_glyph_info(font, glyph, pmat, members, info);

    return gs_default_glyph_info(font, glyph, pmat, members, info);
}

static int
pdfi_cff_fdarray_glyph_data(gs_font_type1 *pfont, gs_glyph glyph, gs_glyph_data_t *pgd)
{
    return_error(gs_error_invalidfont);
}

static int
pdfi_cff_fdarray_seac_data(gs_font_type1 *pfont, int ccode,
                           gs_glyph *pglyph, gs_const_string *gstr, gs_glyph_data_t *pgd)
{
    return_error(gs_error_invalidfont);
}

/* Note that pgd may be NULL - so only retrieve the fidx */
static int
pdfi_cff_cid_glyph_data(gs_font_base *pbfont, gs_glyph glyph, gs_glyph_data_t *pgd, int *pfidx)
{
    int code = 0;
    pdf_cidfont_type0 *pdffont9 = (pdf_cidfont_type0 *) pbfont->client_data;
    gs_font_cid0 *gscidfont = (gs_font_cid0 *) pbfont;
    pdf_name *glyphname = NULL;
    pdf_string *charstring = NULL;
    char nbuf[64];
    uint32_t l;
    gs_glyph gid;

    *pfidx = 0;

    if (glyph < GS_MIN_CID_GLYPH)
        gid = glyph;
    else
        gid = glyph - GS_MIN_CID_GLYPH;

    if (pdffont9->cidtogidmap != NULL && pdffont9->cidtogidmap->length > (gid << 1) + 1) {
        gid = pdffont9->cidtogidmap->data[gid << 1] << 8 | pdffont9->cidtogidmap->data[(gid << 1) + 1];
    }

    l = gs_snprintf(nbuf, sizeof(nbuf), "%" PRId64, gid);

    code = pdfi_name_alloc(pdffont9->ctx, (byte *) nbuf, l, (pdf_obj **) &glyphname);
    if (code >= 0) {
        pdfi_countup(glyphname);
        code = pdfi_dict_get_by_key(pdffont9->ctx, pdffont9->CharStrings, glyphname, (pdf_obj **) &charstring);
        if (code >= 0 && charstring->length >= gscidfont->cidata.FDBytes) {
            if (gscidfont->cidata.FDBytes != 0) {
                if ((int)charstring->data[0] > gscidfont->cidata.FDArray_size)
                    code = gs_note_error(gs_error_invalidfont);
                else
                    *pfidx = (int)charstring->data[0];
            }

            if (code >= 0 && pgd && ((int64_t)charstring->length - (int64_t)gscidfont->cidata.FDBytes) >= 0)
                gs_glyph_data_from_bytes(pgd, charstring->data + gscidfont->cidata.FDBytes, 0, charstring->length - gscidfont->cidata.FDBytes, NULL);
        }
    }
    pdfi_countdown(charstring);
    pdfi_countdown(glyphname);

    return code;
}


static int
pdfi_cff_cidfont_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
                     int members, gs_glyph_info_t *info)
{
    int code;
    gs_font_cid0 *pcidfont = (gs_font_cid0 *)font;
    pdf_cidfont_type0 *pdffont9 = (pdf_cidfont_type0 *)font->client_data;
    code = (*pdffont9->orig_glyph_info)(font, glyph, pmat, members, info);
    if (code < 0)
        return code;

    if ((members & GLYPH_INFO_WIDTHS) != 0
      && glyph > GS_MIN_CID_GLYPH
      && glyph < GS_MIN_GLYPH_INDEX) {
        double widths[6] = {0};
        int fidx;
        gs_matrix imat;
        gs_matrix mat1 = font->FontMatrix;
        gs_glyph g = glyph - GS_MIN_CID_GLYPH;

        code = (*pcidfont->cidata.glyph_data) ((gs_font_base *)font, g + GS_MIN_CID_GLYPH, NULL, &fidx);
        if (code < 0)
            return code;
        if (fidx < pcidfont->cidata.FDArray_size) {
            gs_font_type1 *pfdfont = pcidfont->cidata.FDArray[fidx];
            /* The following cannot fail - if the matrix multiplication didn't work
               we'd have errored out at a higher level
             */
            (void)gs_matrix_multiply(&font->FontMatrix, &pfdfont->FontMatrix, &mat1);
        }
        code = gs_matrix_invert(&mat1, &imat);
        if (code < 0)
            return code; /* By this stage, this should be impossible */
        if (pmat) {
            gs_matrix_multiply(&imat, pmat, &mat1);
        }
        else {
            mat1 = imat;
        }

        code = pdfi_get_cidfont_glyph_metrics(font, g, widths, true);
        if (code >= 0) {
            code = gs_point_transform(widths[GLYPH_W0_WIDTH_INDEX] / 1000.0, widths[GLYPH_W0_HEIGHT_INDEX] / 1000.0, &mat1, &info->width[0]);
            if (code < 0)
                return code;
            info->members |= GLYPH_INFO_WIDTH0;

            if ((members & GLYPH_INFO_WIDTH1) != 0 && (widths[GLYPH_W1_WIDTH_INDEX] != 0 || widths[GLYPH_W1_HEIGHT_INDEX] != 0)) {
                code = gs_point_transform(widths[GLYPH_W1_WIDTH_INDEX] / 1000.0, widths[GLYPH_W1_HEIGHT_INDEX] / 1000.0, &mat1, &info->width[1]);
                info->members |= GLYPH_INFO_WIDTH1;
            }
            if ((members & GLYPH_INFO_VVECTOR1) != 0) {
                code = gs_point_transform(widths[GLYPH_W1_V_X_INDEX] / 1000.0, widths[GLYPH_W1_V_Y_INDEX] / 1000.0, &mat1, &info->v);
                info->members |= GLYPH_INFO_VVECTOR1;
            }
        }
    }
    return code;
}

/* END CALLBACKS */

#if 0                           /* not currently used */
static inline int
s16(const byte *p)
{
    return (signed short)((p[0] << 8) | p[1]);
}
#endif /* not currently used */

static inline int
u16(const byte *p, const byte *e, int *ret)
{
    if (p + 1 > e) {
        *ret = 0;
        return_error(gs_error_invalidfont);
    }
    *ret = (p[0] << 8) | p[1];
    return 0;
}

static inline int
u24(const byte *p, const byte *e, int *ret)
{
    if (p + 2 > e) {
        *ret = 0;
        return_error(gs_error_invalidfont);
    }
    *ret = (p[0] << 16) | (p[1] << 8) | p[2];
    return 0;
}

static inline int
u32(const byte *p, const byte *e, int *ret)
{
    if (p + 3 > e) {
        *ret = 0;
        return_error(gs_error_invalidfont);
    }
    *ret = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    return 0;
}


static int
subrbias(int count)
{
    return count < 1240 ? 107 : count < 33900 ? 1131 : 32768;
}

static int
uofs(const byte *p, const byte *e, int offsize, int *ret)
{
    if (p >= e) {
        *ret = 0;
        return_error(gs_error_invalidfont);
    }
    if (offsize == 1) {
        *ret = p[0];
        return 0;
    }
    if (offsize == 2)
        return u16(p, e, ret);
    if (offsize == 3)
        return u24(p, e, ret);
    if (offsize == 4)
        return u32(p, e, ret);

    return_error(gs_error_invalidfont);
}

static int
iso_adobe_charset_proc(const byte *p, const byte *pe, unsigned i)
{
    if (i < 228)
        return i + 1;
    else
        return_error(gs_error_rangecheck);
}

static int
expert_charset_proc(const byte *p, const byte *pe, unsigned i)
{
    if (i < gs_c_known_encoding_lengths[6])
        return gs_c_known_encodings[6][i];

    return_error(gs_error_rangecheck);
}

static int
expert_subset_charset_proc(const byte *p, const byte *pe, unsigned int i)
{
#if 0
    if (i < sizeof(expert_subset_charset) / sizeof(*expert_subset_charset))
        return expert_subset_charset[i];
#endif
    return_error(gs_error_rangecheck);
}

static int
format0_charset_proc(const byte *p, const byte *pe, unsigned int i)
{
    int code, ret;
    if (p + 2 * i > pe)
        return gs_error_rangecheck;

    if ((code = u16(p + 2 * i, pe, &ret)) < 0) {
        return code;
    }
    return ret;
}

static int
format1_charset_proc(const byte *p, const byte *pe, unsigned int i)
{
    int code = gs_error_rangecheck;
    unsigned int cid = 0;

    while (p < pe - 3) {
        unsigned int first, count;

        code = (unsigned int)u16(p, pe, (int *)&first);
        if (code < 0)
            break;
        count = (unsigned int)p[2] + 1;

        if (i < cid + count) {
            code = first + i - cid;
            break;
        }
        p += 3;
        cid += count;
    }
    return code;
}

static int
format2_charset_proc(const byte *p, const byte *pe, unsigned int i)
{
    int code = gs_error_rangecheck;
    unsigned int cid = 0;

    while (p < pe - 4) {
        unsigned int first, count;

        code = u16(p, pe, (int *)&first);
        if (code >= 0)
            code = u16(p + 2, pe, (int *)&count);
        if (code < 0)
            break;

        count += 1;

        if (i < cid + count) {
            code = first + i - cid;
            break;
        }
        p += 4;
        cid += count;
    }
    return code;
}

static int
format0_fdselect_proc(const byte *p, const byte *pe, unsigned int i)
{
    return (int)(*(p + i));
}

static int
format3_fdselect_proc(const byte *p, const byte *pe, unsigned int i)
{
    unsigned int n_ranges;
    int code;

    if ((code = u16(p, pe, (int *)&n_ranges)) < 0)
        return code;

    p += 2;

    while (n_ranges-- && p + 5 <= pe) {
        unsigned int first, last;

        code = u16(p, pe, (int *)&first);
        if (code >= 0)
            code = u16(p + 3, pe, (int *)&last);

        if (code < 0)
            break;

        if (i >= first && i < last) {
            return (int)(*(p + 2));
        }
        p += 3;
    }
    return_error(gs_error_rangecheck);
}


static byte *
pdfi_read_cff_real(byte *p, byte *e, float *val)
{
    char buf[65];
    char *txt = buf;

    /* b0 was 30 */

    while (txt < buf + (sizeof buf) - 5 && p < e) {
        int b, n;

        b = *p++;

        n = (b >> 4) &0xf;
        if (n < 0xA) {
            *txt++ = n + '0';
        }
        else if (n == 0xA) {
            *txt++ = '.';
        }
        else if (n == 0xB) {
            *txt++ = 'E';
        }
        else if (n == 0xC) {
            *txt++ = 'E';
            *txt++ = '-';
        }
        else if (n == 0xE) {
            *txt++ = '-';
        }
        else if (n == 0xF) {
            break;
        }

        n = b &0xf;
        if (n < 0xA) {
            *txt++ = n + '0';
        }
        else if (n == 0xA) {
            *txt++ = '.';
        }
        else if (n == 0xB) {
            *txt++ = 'E';
        }
        else if (n == 0xC) {
            *txt++ = 'E';
            *txt++ = '-';
        }
        else if (n == 0xE) {
            *txt++ = '-';
        }
        else if (n == 0xF) {
            break;
        }
    }

    *txt = 0;

    *val = atof(buf);

    return p;
}

static byte *
pdfi_read_cff_integer(byte *p, byte *e, int b0, int *val)
{
    int b1, b2, b3, b4;

    if (b0 == 28) {
        if (p + 2 > e) {
            gs_throw(-1, "corrupt dictionary (integer)");
            return 0;
        }
        b1 = *p++;
        b2 = *p++;
        *val = (b1 << 8) | b2;
    }

    else if (b0 == 29) {
        if (p + 4 > e) {
            gs_throw(-1, "corrupt dictionary (integer)");
            return 0;
        }
        b1 = *p++;
        b2 = *p++;
        b3 = *p++;
        b4 = *p++;
        *val = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
    }

    else if (b0 < 247) {
        *val = b0 - 139;
    }

    else if (b0 < 251) {
        if (p + 1 > e) {
            gs_throw(-1, "corrupt dictionary (integer)");
            return 0;
        }
        b1 = *p++;
        *val = (b0 - 247) * 256 + b1 + 108;
    }

    else {
        if (p + 1 > e) {
            gs_throw(-1, "corrupt dictionary (integer)");
            return 0;
        }
        b1 = *p++;
        *val = -(b0 - 251) * 256 - b1 - 108;
    }

    return p;
}

#define PDFI_CFF_STACK_SIZE 48

static int
pdfi_read_cff_dict(byte *p, byte *e, pdfi_gs_cff_font_priv *ptpriv, cff_font_offsets *offsets, bool topdict)
{
    pdfi_cff_font_priv *font = &ptpriv->pdfcffpriv;
    struct
    {
        int ival;
        float fval;
    } args[PDFI_CFF_STACK_SIZE];
    int offset;
    int b0, n;
    double f;
    int i;
    int code = 0;
    bool do_priv = false;

    memset(args, 0x00, sizeof(args));

    offset = p - font->cffdata;

    n = 0;
    while (p < e && code >= 0) {
        b0 = *p;
        p++;

        switch (b0) {
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 31:
            case 255:
                continue;
            default:
                break;
        }

        if (b0 < 22) {
            if (b0 == 12) {
                if (p + 1 > e) {
                    return gs_throw(-1, "corrupt dictionary (operator)");
                }
                b0 = 0x100 | *p++;
            }
            switch (b0) {
                case 13: /* UniqueID */
                  break;

                case 14:
                    break; /* XUID */

                /* some CFF file offsets */
                case 15:
                {
                    if (args[0].ival < 0) {
                        code = gs_note_error(gs_error_invalidfont);
                        break;
                    }
                    offsets->charset_off = args[0].ival;
                    break;
                }
                case 16:
                {
                    if (args[0].ival < 0) {
                        code = gs_note_error(gs_error_invalidfont);
                        break;
                    }
                    offsets->encoding_off = args[0].ival;
                    break;
                }
                case 17:
                {
                    if (args[0].ival < 0) {
                        code = gs_note_error(gs_error_invalidfont);
                        break;
                    }
                    font->charstrings = font->cffdata + args[0].ival;
                    break;
                }

                case 18:
                {
                    offsets->private_size = args[0].ival;
                    if (args[1].ival < 0) {
                        code = gs_note_error(gs_error_invalidfont);
                        break;
                    }
                    offsets->private_off = args[1].ival;
                    /* Catch a broken font with a self referencing Private dict */
                    if (topdict == true)
                        do_priv = offsets->private_size > 0 ? true : false;
                    else {
                        do_priv = false;
                        code = gs_error_invalidfont;
                        break;
                    }
                    break;
                }

                case 19:
                {
                    if (args[0].ival < 0) {
                        code = gs_note_error(gs_error_invalidfont);
                        break;
                    }
                    font->subrs = font->cffdata + offset + args[0].ival;
                    break;
                }

                case 256 | 30:
                {
                    code = pdfi_make_string_from_sid(font->ctx, (pdf_obj **) &font->registry, font, offsets, args[0].ival);
                    if (code < 0)
                        break;
                    code = pdfi_make_string_from_sid(font->ctx, (pdf_obj **) &font->ordering, font, offsets, args[1].ival);
                    if (code < 0)
                        break;
                    font->supplement = args[2].ival;
                    offsets->have_ros = true;
                    ptpriv->FontType = ft_CID_encrypted;
                    break;
                }

                case 256 | 34:
                {
                    font->cidcount = args[0].ival;
                    break;
                }

                case 256 | 35:
                {
                    font->uidbase = args[0].ival;
                    break;
                }

                case 256 | 36:
                {
                    offsets->fdarray_off = args[0].ival;
                    break;
                }

                case 256 | 37:
                {
                    offsets->fdselect_off = args[0].ival;
                    break;
                }

                case 256 | 38:
                {
                    pdf_string *fnamestr = NULL;

                    code = pdfi_make_string_from_sid(font->ctx, (pdf_obj **) &fnamestr, font, offsets, args[0].ival);
                    if (code >= 0) {
                        int nlen = fnamestr->length > gs_font_name_max ? gs_font_name_max : fnamestr->length;
                        memcpy(ptpriv->font_name.chars, fnamestr->data, nlen);
                        memcpy(ptpriv->key_name.chars, fnamestr->data, nlen);
                        ptpriv->font_name.size = ptpriv->key_name.size = nlen;
                        pdfi_countdown(fnamestr);
                    }
                    break;
                }

                /* Type1 stuff that need to be set for the ptpriv struct */

                case 256 | 6:
                {
                    if (args[0].ival == 1) {
                        ptpriv->type1data.interpret = gs_type1_interpret;
                        ptpriv->type1data.lenIV = -1;       /* FIXME */
                    }
                    break;
                }

                case 256 | 7:
                {
                    ptpriv->FontMatrix.xx = args[0].fval;
                    ptpriv->FontMatrix.xy = args[1].fval;
                    ptpriv->FontMatrix.yx = args[2].fval;
                    ptpriv->FontMatrix.yy = args[3].fval;
                    ptpriv->FontMatrix.tx = args[4].fval;
                    ptpriv->FontMatrix.ty = args[5].fval;
                    offsets->have_matrix = true;
                    break;
                }
                case 5:
                {
                    ptpriv->FontBBox.p.x = args[0].fval;
                    ptpriv->FontBBox.p.y = args[1].fval;
                    ptpriv->FontBBox.q.x = args[2].fval;
                    ptpriv->FontBBox.q.y = args[3].fval;
                    break;
                }

                case 20:
                {
                    ptpriv->type1data.defaultWidthX = float2fixed(args[0].fval);
                    break;
                }

                case 21:
                {
                    ptpriv->type1data.nominalWidthX = float2fixed(args[0].fval);
                    break;
                }

                case 256 | 19:
                {
                    ptpriv->type1data.initialRandomSeed = args[0].ival;
                    break;
                }

                case 6:
                {
                    if (n > max_BlueValues * 2) n = max_BlueValues * 2;
                    ptpriv->type1data.BlueValues.count = n;
                    ptpriv->type1data.BlueValues.values[0] = args[0].fval;
                    for (i = 1; i < n; i++) {
                        ptpriv->type1data.BlueValues.values[i] = ptpriv->type1data.BlueValues.values[i - 1] + args[i].fval;
                    }
                    break;
                }

                case 7:
                {
                    if (n > max_OtherBlues * 2) n = max_OtherBlues * 2;
                    ptpriv->type1data.OtherBlues.count = n;
                    ptpriv->type1data.OtherBlues.values[0] = args[0].fval;
                    for (i = 1; i < n; i++) {
                        ptpriv->type1data.OtherBlues.values[i] = ptpriv->type1data.OtherBlues.values[i - 1] + args[i].fval;
                    }
                    break;
                }

                case 8:
                {
                    if (n > max_FamilyBlues * 2) n = max_FamilyBlues * 2;
                    ptpriv->type1data.FamilyBlues.count = n;
                    ptpriv->type1data.FamilyBlues.values[0] = args[0].fval;
                    for (i = 1; i < n; i++) {
                        ptpriv->type1data.FamilyBlues.values[i] = ptpriv->type1data.FamilyBlues.values[i - 1] + args[i].fval;
                    }
                    break;
                }

                case 9:
                {
                    if (n > max_FamilyOtherBlues * 2) n = max_FamilyOtherBlues * 2;
                    ptpriv->type1data.FamilyOtherBlues.count = n;
                    ptpriv->type1data.FamilyOtherBlues.values[0] = args[0].fval;
                    for (i = 1; i < n; i++) {
                        ptpriv->type1data.FamilyOtherBlues.values[i] = ptpriv->type1data.FamilyOtherBlues.values[i - 1] + args[i].fval;
                    }
                    break;
                }

                case 10:
                {
                    ptpriv->type1data.StdHW.count = 1;
                    ptpriv->type1data.StdHW.values[0] = args[0].fval;
                    break;
                }

                case 11:
                {
                    ptpriv->type1data.StdVW.count = 1;
                    ptpriv->type1data.StdVW.values[0] = args[0].fval;
                    break;
                }

                case 256 | 9:
                {
                    ptpriv->type1data.BlueScale = args[0].fval;
                    break;
                }

                case 256 | 10:
                {
                    ptpriv->type1data.BlueShift = args[0].fval;
                    break;
                }

                case 256 | 11:
                {
                    ptpriv->type1data.BlueFuzz = (int)args[0].fval;
                    break;
                }

                case 256 | 12:
                {
                    if (n > max_StemSnap) n = max_StemSnap;
                    ptpriv->type1data.StemSnapH.count = n;
                    for (f = 0, i = 0; i < n; f += args[i].fval, i++)
                        ptpriv->type1data.StemSnapH.values[i] = f;
                    break;
                }

                case 256 | 13:
                {
                    if (n > max_StemSnap) n = max_StemSnap;
                    ptpriv->type1data.StemSnapV.count = n;
                    for (f = 0, i = 0; i < n; f += args[i].fval, i++)
                        ptpriv->type1data.StemSnapV.values[i] = f;
                    break;
                }

                case 256 | 14:
                {
                    ptpriv->type1data.ForceBold = args[0].ival;
                    break;
                }

                case 256 | 17:
                {
                    ptpriv->type1data.LanguageGroup = args[0].ival;
                    break;
                }

                case 256 | 18:
                {
                    ptpriv->type1data.ExpansionFactor = args[0].fval;
                    break;
                }
                default:
                    break;
            }
            n = 0;
        }
        else {
            if (b0 == 30) {
                p = pdfi_read_cff_real(p, e, &args[n].fval);
                if (!p) {
                    dmprintf(ptpriv->memory, "\nCFF: corrupt dictionary operand\n");
                    break;
                }
                args[n].ival = (int)args[n].fval;
                n++;
            }
            else if (b0 == 28 || b0 == 29 || (b0 >= 32 && b0 <= 254)) {
                /* If we run out of data reading an integer at the very end of the stream, don't throw an error
                   just return.
                 */
                bool near_end = ((e - p) <= 4);
                p = pdfi_read_cff_integer(p, e, b0, &args[n].ival);
                if (!p) {
                    if (!near_end)
                        code = gs_note_error(gs_error_invalidfont);
                    dmprintf(ptpriv->memory, "\nCFF: corrupt dictionary operand\n");
                    break;
                }
                args[n].fval = (float)args[n].ival;
                n++;
            }
            else {
                dmprintf1(ptpriv->memory, "CFF: corrupt dictionary operand (b0 = %d)", b0);
            }
        }
        if (n >= PDFI_CFF_STACK_SIZE) {
            code = gs_error_invalidfont;
            break;
        }
    }

    /* recurse for the private dictionary */
    if (do_priv && code >= 0) {
        byte *dend = font->cffdata + offsets->private_off + offsets->private_size;

        if (dend > font->cffend)
            dend = font->cffend;

        if (p == NULL)
            code = gs_error_invalidfont;
        else
            code = pdfi_read_cff_dict(font->cffdata + offsets->private_off, dend, ptpriv, offsets, false);

        if (code < 0)
            dmprintf(ptpriv->memory, "CFF: cannot read private dictionary");
    }

    return code;
}

/*
 * Get the number of items in an INDEX, and return
 * a pointer to the end of the INDEX or NULL on
 * failure.
 */
static byte *
pdfi_count_cff_index(byte *p, byte *e, int *countp)
{
    int count, offsize, last;
    int code;

    if (p + 3 > e) {
        gs_throw(-1, "not enough data for index header");
        return 0;
    }

    if ((code = u16(p, e, &count)) < 0)
        return NULL;

    p += 2;
    *countp = count;

    if (count == 0)
        return p;

    offsize = *p++;

    if (offsize < 1 || offsize > 4) {
        gs_throw(-1, "corrupt index header");
        return 0;
    }

    if (p + count * offsize > e) {
        gs_throw(-1, "not enough data for index offset table");
        return 0;
    }

    p += count * offsize;
    code = uofs(p, e, offsize, &last);
    p += offsize;
    p--;                        /* stupid offsets */

    if (last < 0 || code < 0) {
        gs_throw(-1, "corrupt index");
        return 0;
    }

    if (p + last > e) {
        gs_throw(-1, "not enough data for index data");
        return 0;
    }

    p += last;

    return p;
}

/*
 * Locate and store pointers to the data of an
 * item in the index that starts at 'p'.
 * Return pointer to the end of the index,
 * or NULL on failure.
 */
static byte *
pdfi_find_cff_index(byte *p, byte *e, int idx, byte ** pp, byte ** ep)
{
    int code, count, offsize, sofs, eofs, last;

    if (p == NULL)
        return 0;

    if (p + 3 > e) {
        gs_throw(-1, "not enough data for index header");
        return 0;
    }

    if (u16(p, e, &count) < 0)
        return NULL;

    p += 2;
    if (count == 0)
        return 0;

    offsize = *p++;

    if (offsize < 1 || offsize > 4) {
        gs_throw(-1, "corrupt index header");
        return 0;
    }

    if (p + count * offsize > e) {
        gs_throw(-1, "not enough data for index offset table");
        return 0;
    }

    if (idx < 0 || idx >= count) {
        gs_throw(-1, "tried to access non-existing index item");
        return 0;
    }

    code = uofs(p + idx * offsize, e,  offsize, &sofs);
    if (code >= 0)
        code = uofs(p + (idx + 1) * offsize, e, offsize, &eofs);
    if (code >= 0)
        code = uofs(p + count * offsize, e, offsize, &last);

    if (code < 0) {
        gs_throw(-1, "not enough data for index data");
        return 0;
    }

    p += count * offsize;
    p += offsize;
    p--;                        /* stupid offsets */

    if (p + last > e) {
        gs_throw(-1, "not enough data for index data");
        return 0;
    }

    if (sofs < 0 || eofs < 0 || sofs > eofs || eofs > last) {
        gs_throw(-1, "corrupt index offset table");
        return 0;
    }

    *pp = p + sofs;
    *ep = p + eofs;

    return p + last;
}

static int
pdfi_make_name_from_sid(pdf_context *ctx, pdf_obj ** nm, pdfi_cff_font_priv *font, cff_font_offsets *offsets, unsigned int sid)
{
    gs_string str;
    byte *p;

    if (sid < gs_c_known_encoding_lengths[10]) {
        gs_glyph gl = gs_c_known_encode(sid, 10);

        (void)gs_c_glyph_name(gl, (gs_const_string *) &str);
    }
    else {
        byte *strp, *stre;

        p = pdfi_find_cff_index(font->cffdata + offsets->strings_off, font->cffend, sid - gs_c_known_encoding_lengths[10], &strp, &stre);
        if (p == NULL)
            return_error(gs_error_rangecheck);
        str.data = strp;
        str.size = stre - strp;
    }
    return pdfi_name_alloc(ctx, str.data, str.size, nm);
}

static int
pdfi_make_string_from_sid(pdf_context *ctx, pdf_obj ** s0, pdfi_cff_font_priv *font, cff_font_offsets *offsets, unsigned int sid)
{
    byte *p;
    int code;
    gs_string str;
    pdf_string *s = NULL;

    if (sid < gs_c_known_encoding_lengths[10]) {
        gs_glyph gl = gs_c_known_encode(sid, 10);

        (void)gs_c_glyph_name(gl, (gs_const_string *) &str);
    }
    else {
        byte *strp, *stre;

        p = pdfi_find_cff_index(font->cffdata + offsets->strings_off, font->cffend,
                                sid - gs_c_known_encoding_lengths[10], &strp, &stre);
        if (p == NULL)
            return_error(gs_error_rangecheck);
        str.data = strp;
        str.size = stre - strp;
    }
    code = pdfi_object_alloc(ctx, PDF_STRING, str.size, (pdf_obj **) &s);
    if (code < 0)
        return code;
    pdfi_countup(s);
    memcpy(s->data, str.data, str.size);
    s->length = str.size;

    *s0 = (pdf_obj *) s;
    return 0;
}

static int
pdfi_cff_build_encoding(pdf_context *ctx, pdfi_gs_cff_font_priv *ptpriv, cff_font_offsets *offsets,
                        int (*charset_proc)(const byte *p, const byte *pe, unsigned int i))
{
    pdfi_cff_font_priv *font = &ptpriv->pdfcffpriv;
    int code = 0;
    byte *s, *e, *lp;
    pdf_string *pstr;
    unsigned int i, gid, enc_format = 0;
    int sid;
    pdf_name *ndname = NULL;
    unsigned char gid2char[256];
    unsigned supp_enc_offset = 0;

    if (offsets->encoding_off <= 1) {
        /* Either standard or expert encoding */
        pdf_name *enm = NULL;
        const char *const stdenc = "StandardEncoding";
        const char *const expenc = "MacExpertEncoding";
        char const *enctouse;

        if (offsets->encoding_off < 1) {
            enctouse = stdenc;
        }
        else {
            enctouse = expenc;
        }
        code = pdfi_name_alloc(ctx, (byte *) enctouse, strlen(enctouse), (pdf_obj **) &enm);
        if (code >= 0) {
            pdfi_countup(enm);
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, (pdf_obj *) enm, NULL, (pdf_obj **) &font->Encoding);
            pdfi_countdown(enm);
        }
    }
    else {
        if (font->cffdata + offsets->encoding_off > font->cffend) {
            code = gs_note_error(gs_error_invalidfont);
        }
        else {
            code = pdfi_object_alloc(ctx, PDF_ARRAY, 256, (pdf_obj **) &font->Encoding);
            if (code < 0)
                return code;

            code = pdfi_name_alloc(ctx, (byte *) ".notdef", 7, (pdf_obj **) &ndname);
            if (code < 0)
                return code;


            pdfi_countup(font->Encoding);
            pdfi_countup(ndname);
            code = 0;
            /* Prepopulate with notdefs */
            for (i = 0; i < 256 && code >= 0; i++) {
                code = pdfi_array_put(ctx, font->Encoding, (uint64_t) i, (pdf_obj *) ndname);
            }

            if (code >= 0) {
                byte *p = font->cffdata + offsets->encoding_off;

                enc_format = p[0];

                lp = pdfi_find_cff_index(font->charstrings, font->cffend, 0, &s, &e);
                if (lp == NULL) {
                    code = gs_note_error(gs_error_rangecheck);
                    goto done;
                }
                code = pdfi_object_alloc(ctx, PDF_STRING, e - s, (pdf_obj **) &pstr);
                if (code < 0)
                    goto done;
                memcpy(pstr->data, s, e - s);
                pdfi_countup(pstr);
                code =
                    pdfi_dict_put_obj(ctx, font->CharStrings, (pdf_obj *) ndname, (pdf_obj *) pstr, true);
                pdfi_countdown(pstr);
                if (code < 0) {
                    goto done;
                }
                pdfi_countdown(ndname);
                ndname = NULL;  /* just to avoid bad things! */

                if ((enc_format &0x7f) == 0) {
                    unsigned int n_codes = p[1];

                    if (p + 2 + n_codes > font->cffend) {
                        return_error(gs_error_invalidfont);
                    }
                    gid2char[0] = 0;
                    for (i = 0; i < n_codes; i++) {
                        gid2char[i + 1] = p[2 + i];
                    }
                    memset(gid2char + n_codes + 1, 0, sizeof(gid2char) - n_codes - 1);
                    supp_enc_offset = 2 + n_codes;
                }
                else if ((enc_format &0x7f) == 1) {
                    unsigned int n_ranges = p[1];
                    unsigned int first, left, j, k = 1;

                    if (p + 2 + 2 * n_ranges > font->cffend) {
                        return_error(gs_error_invalidfont);
                    }
                    gid2char[0] = 0;
                    for (i = 0; i < n_ranges; i++) {
                        first = p[2 + 2 * i];
                        left = p[3 + 2 * i];
                        for (j = 0; j <= left && k < 256; j++)
                            gid2char[k++] = first + j;
                    }
                    memset(gid2char + k, 0, sizeof(gid2char) - k);
                    supp_enc_offset = 2 * n_ranges + 2;
                }
                else {
                    return_error(gs_error_rangecheck);
                }
            }
        }
    }
    if (code >= 0) {
        pdf_obj *gname;

        code = 0;

        lp = pdfi_find_cff_index(font->charstrings, font->cffend, 0, &s, &e);
        if (lp == NULL) {
            code = gs_note_error(gs_error_rangecheck);
            goto done;
        }
        code = pdfi_object_alloc(ctx, PDF_STRING, e - s, (pdf_obj **) &pstr);
        if (code < 0)
            goto done;
        memcpy(pstr->data, s, e - s);
        pdfi_countup(pstr);
        if (ptpriv->forcecid) {
            char buf[40];
            int len = gs_snprintf(buf, sizeof(buf), "%d", 0);

            code = pdfi_name_alloc(ctx, (byte *) buf, len, &gname);
            if (code < 0) {
                pdfi_countdown(pstr);
                return code;
            }
            pdfi_countup(gname);
        }
        else {
            code = pdfi_name_alloc(ctx, (byte *) ".notdef", 7, &gname);
            if (code < 0) {
                pdfi_countdown(pstr);
                goto done;
            }
            pdfi_countup(gname);
        }
        code = pdfi_dict_put_obj(ctx, font->CharStrings, gname, (pdf_obj *) pstr, true);
        pdfi_countdown(pstr);
        pdfi_countdown(gname);
        if (code < 0)
            goto done;

        for (gid = 1; gid < font->ncharstrings && code >= 0; gid++) {

            lp = pdfi_find_cff_index(font->charstrings, font->cffend, gid, &s, &e);
            if (lp == NULL) {
                code = gs_note_error(gs_error_rangecheck);
                continue;
            }
            code = pdfi_object_alloc(ctx, PDF_STRING, e - s, (pdf_obj **) &pstr);
            if (code < 0)
                return code;
            memcpy(pstr->data, s, e - s);
            pdfi_countup(pstr);

            if (ptpriv->forcecid) {
                char buf[40];
                int len = gs_snprintf(buf, sizeof(buf), "%d", gid);

                code = pdfi_name_alloc(ctx, (byte *) buf, len, &gname);
                if (code < 0) {
                    pdfi_countdown(pstr);
                    return code;
                }
            }
            else {
                sid = (*charset_proc) (font->cffdata + offsets->charset_off + 1, font->cffend, gid - 1);
                if (sid < 0) {
                    pdfi_countdown(pstr);
                    return sid;
                }
                if ((code = pdfi_make_name_from_sid(ctx, &gname, font, offsets, sid)) < 0) {
                    char buf[40];
                    int len = gs_snprintf(buf, sizeof(buf), "sid-%d", sid);

                    code = pdfi_name_alloc(ctx, (byte *) buf, len, &gname);
                    if (code < 0) {
                        pdfi_countdown(pstr);
                        return code;
                    }
                }
            }
            pdfi_countup(gname);
            code = pdfi_dict_put_obj(ctx, font->CharStrings, gname, (pdf_obj *) pstr, true);
            pdfi_countdown(pstr);
            if (code < 0) {
                pdfi_countdown(gname);
                return code;
            }
            if (offsets->encoding_off > 1 && gid < 256) {
                code = pdfi_array_put(ctx, font->Encoding, (int64_t) gid2char[gid], gname);
            }
            pdfi_countdown(gname);
        }

        if (offsets->encoding_off > 1 && (enc_format & 0x80)) {
            unsigned int n_supp, charcode, sid;
            byte *p = font->cffdata + offsets->encoding_off + supp_enc_offset;
            pdf_obj *gname;

            n_supp = p[0];

            for (i = 0; i < n_supp && code >= 0; i++) {
                charcode = p[1 + 3 * i];
                code = u16(p + 2 + 3 * i, e, (int *)&sid);
                if (code < 0) continue;

                if ((code = pdfi_make_name_from_sid(ctx, &gname, font, offsets, sid)) < 0) {
                    char buf[40];
                    int len = gs_snprintf(buf, sizeof(buf), "sid-%d", sid);

                    if (len > 0)
                        code = pdfi_name_alloc(ctx, (byte *) buf, len, &gname);
                    else
                        code = 0;

                    if (code < 0)
                        continue;
                }
                pdfi_countup(gname);
                code = pdfi_array_put(ctx, font->Encoding, (int64_t) charcode, gname);
                pdfi_countdown(gname);
            }
        }
    }
  done:
    if (code < 0) {
        pdfi_countdown(ndname);
    }
    return code;
}


/*
 * Scan the CFF file structure and extract important data.
 */

static int
pdfi_read_cff(pdf_context *ctx, pdfi_gs_cff_font_priv *ptpriv)
{
    pdfi_cff_font_priv *font = &ptpriv->pdfcffpriv;
    byte *pstore, *p = font->cffdata;
    byte *e = font->cffend;
    byte *dictp, *dicte;
    byte *strp, *stre;
    byte *nms, *nmp, *nme;
    int count;
    int i, code = 0;
    cff_font_offsets offsets = { 0 };
    int (*charset_proc)(const byte *p, const byte *pe, unsigned int i);
    int major, minor, hdrsize;

    /* CFF header */
    if (p + 4 > e)
        return gs_throw(gs_error_invalidfont, "not enough data for header");

    major = *p;
    minor = *(p + 1);
    hdrsize = *(p + 2);

    if (major != 1 || minor != 0)
        return gs_throw(gs_error_invalidfont, "not a CFF 1.0 file");

    if (p + hdrsize > e)
        return gs_throw(gs_error_invalidfont, "not enough data for extended header");
    p += hdrsize;

    /* Name INDEX */
    nms = p;
    p = pdfi_count_cff_index(p, e, &count);
    if (p == NULL)
        return gs_throw(gs_error_invalidfont, "cannot read name index");
    if (count != 1)
        return gs_throw(gs_error_invalidfont, "file did not contain exactly one font");

    nms = pdfi_find_cff_index(nms, e, 0, &nmp, &nme);
    if (!nms)
        return gs_throw(gs_error_invalidfont, "cannot read names index");
    else {
        int len = nme - nmp < sizeof(ptpriv->key_name.chars) ? nme - nmp : sizeof(ptpriv->key_name.chars);
        memcpy(ptpriv->key_name.chars, nmp, len);
        memcpy(ptpriv->font_name.chars, nmp, len);
        ptpriv->key_name.size = ptpriv->font_name.size = len;
    }

    /* Top Dict INDEX */
    p = pdfi_find_cff_index(p, e, 0, &dictp, &dicte);
    if (p == NULL)
        return gs_throw(gs_error_invalidfont, "cannot read top dict index");

    /* String index */
    pstore = p;
    p = pdfi_find_cff_index(p, e, 0, &strp, &stre);

    offsets.strings_off = pstore - font->cffdata;

    p = pdfi_count_cff_index(pstore, e, &count);
    if (p == NULL)
        return_error(gs_error_invalidfont);

    offsets.strings_size = (unsigned int)count;

    /* Global Subr INDEX */
    font->gsubrs = p;
    p = pdfi_count_cff_index(p, e, &font->NumGlobalSubrs);
    if (p == NULL) {
        font->GlobalSubrs = NULL;
        font->NumGlobalSubrs = 0;
    }
    /* Read the top and private dictionaries */
    code = pdfi_read_cff_dict(dictp, dicte, ptpriv, &offsets, true);
    if (code < 0)
        return gs_rethrow(code, "cannot read top dictionary");

    /* Check the subrs index */
    font->NumSubrs = 0;
    if (font->subrs) {
        p = pdfi_count_cff_index(font->subrs, e, &font->NumSubrs);
        if (p == NULL || font->NumSubrs > 65536) {
            font->Subrs = NULL;
            font->NumSubrs = 0;
        }
        else {
            ptpriv->type1data.subroutineNumberBias = subrbias(font->NumSubrs);
        }
    }


    font->GlobalSubrs = NULL;
    if (font->NumGlobalSubrs > 0 && font->NumGlobalSubrs <= 65536) {
        ptpriv->type1data.gsubrNumberBias = subrbias(font->NumGlobalSubrs);
        code = pdfi_object_alloc(ctx, PDF_ARRAY, font->NumGlobalSubrs, (pdf_obj **) &font->GlobalSubrs);
        if (code >= 0) {
            font->GlobalSubrs->refcnt = 1;
            for (i = 0; i < font->NumGlobalSubrs; i++) {
                pdf_string *gsubrstr;

                p = pdfi_find_cff_index(font->gsubrs, font->cffend, i, &strp, &stre);
                if (p) {
                    code = pdfi_object_alloc(ctx, PDF_STRING, stre - strp, (pdf_obj **) &gsubrstr);
                    if (code >= 0) {
                        memcpy(gsubrstr->data, strp, gsubrstr->length);
                        code =
                            pdfi_array_put(ctx, font->GlobalSubrs, (uint64_t) i,
                                           (pdf_obj *) gsubrstr);
                        if (code < 0) {
                            gsubrstr->refcnt = 1;
                            pdfi_countdown(gsubrstr);
                        }
                    }
                }
                else {
                    code = pdfi_array_put(ctx, font->GlobalSubrs, (uint64_t) i, PDF_NULL_OBJ);
                    if (code < 0) {
                        pdfi_countdown(font->GlobalSubrs);
                        font->GlobalSubrs = NULL;
                    }
                }
            }
        }
    }

    font->Subrs = NULL;
    if (font->NumSubrs > 0) {
        code = pdfi_object_alloc(ctx, PDF_ARRAY, font->NumSubrs, (pdf_obj **) &font->Subrs);
        if (code >= 0 && font->Subrs != NULL) {
            font->Subrs->refcnt = 1;
            for (i = 0; i < font->NumSubrs; i++) {
                pdf_string *subrstr;

                p = pdfi_find_cff_index(font->subrs, font->cffend, i, &strp, &stre);
                if (p) {
                    code = pdfi_object_alloc(ctx, PDF_STRING, stre - strp, (pdf_obj **) &subrstr);
                    if (code >= 0) {
                        memcpy(subrstr->data, strp, subrstr->length);
                        code = pdfi_array_put(ctx, font->Subrs, (uint64_t) i, (pdf_obj *) subrstr);
                        if (code < 0) {
                            subrstr->refcnt = 1;
                            pdfi_countdown(subrstr);
                        }
                    }
                }
                else {
                    code = pdfi_array_put(ctx, font->Subrs, (uint64_t) i, PDF_NULL_OBJ);
                    if (code < 0) {
                        pdfi_countdown(font->Subrs);
                        font->Subrs = NULL;
                        font->NumSubrs = 0;
                        break;
                    }
                }
            }
        }
    }

    /* Check the charstrings index */
    if (font->charstrings) {
        p = pdfi_count_cff_index(font->charstrings, e, &font->ncharstrings);
        if (!p || font->ncharstrings > 65535)
            return gs_rethrow(-1, "cannot read charstrings index");
    }
    code = pdfi_object_alloc(ctx, PDF_DICT, font->ncharstrings, (pdf_obj **) &font->CharStrings);
    if (code < 0)
        return code;
    pdfi_countup(font->CharStrings);

    switch (offsets.charset_off) {
        case 0:
            charset_proc = iso_adobe_charset_proc;
            break;
        case 1:
            charset_proc = expert_charset_proc;
            break;
        case 2:
            charset_proc = expert_subset_charset_proc;
            break;
        default:{
                if (font->cffdata + offsets.charset_off >= font->cffend)
                    return_error(gs_error_rangecheck);

                switch ((int)font->cffdata[offsets.charset_off]) {
                    case 0:
                        charset_proc = format0_charset_proc;
                        break;
                    case 1:
                        charset_proc = format1_charset_proc;
                        break;
                    case 2:
                        charset_proc = format2_charset_proc;
                        break;
                    default:
                        return_error(gs_error_rangecheck);
                }
            }
    }

    if (offsets.have_ros) {     /* CIDFont */
        int fdarray_size;
        bool topdict_matrix = offsets.have_matrix;
        int (*fdselect_proc)(const byte *p, const byte *pe, unsigned int i);

        p = pdfi_count_cff_index(font->cffdata + offsets.fdarray_off, e, &fdarray_size);
        if (!p || fdarray_size < 1 || fdarray_size > 64) /* 64 is arbitrary, but seems a reasonable upper limit */
            return gs_rethrow(-1, "cannot read charstrings index");

        ptpriv->cidata.FDBytes = 1;     /* Basically, always 1 just now */

        ptpriv->cidata.FDArray = (gs_font_type1 **) gs_alloc_bytes(ctx->memory, fdarray_size * sizeof(gs_font_type1 *), "pdfi_read_cff(fdarray)");
        if (!ptpriv->cidata.FDArray)
            return_error(gs_error_VMerror);
        ptpriv->cidata.FDArray_size = fdarray_size;

        code = pdfi_object_alloc(ctx, PDF_ARRAY, fdarray_size, (pdf_obj **) &font->FDArray);
        if (code < 0) {
            gs_free_object(ctx->memory, ptpriv->cidata.FDArray, "pdfi_read_cff(fdarray)");
            ptpriv->cidata.FDArray = NULL;
        }
        else {
            pdfi_countup(font->FDArray);
            code = 0;
            for (i = 0; i < fdarray_size && code == 0; i++) {
                byte *fddictp, *fddicte;
                pdfi_gs_cff_font_priv fdptpriv = { 0 };
                pdf_font_cff *pdffont = NULL;
                gs_font_type1 *pt1font;

                pdfi_init_cff_font_priv(ctx, &fdptpriv, font->cffdata, (font->cffend - font->cffdata), true);

                offsets.private_off = 0;

                p = pdfi_find_cff_index(font->cffdata + offsets.fdarray_off, e, i, &fddictp, &fddicte);
                if (!p) {
                    ptpriv->cidata.FDArray[i] = NULL;
                    code = gs_note_error(gs_error_invalidfont);
                    continue;
                }
                if (fddicte > font->cffend)
                    fddicte = font->cffend;

                code = pdfi_read_cff_dict(fddictp, fddicte, &fdptpriv, &offsets, true);
                if (code < 0) {
                    ptpriv->cidata.FDArray[i] = NULL;
                    code = gs_note_error(gs_error_invalidfont);
                    continue;
                }
                code = pdfi_alloc_cff_font(ctx, &pdffont, 0, true);
                if (code < 0) {
                    ptpriv->cidata.FDArray[i] = NULL;
                    code = gs_note_error(gs_error_invalidfont);
                    continue;
                }
                pt1font = (gs_font_type1 *) pdffont->pfont;
                memcpy(pt1font, &fdptpriv, sizeof(pdfi_gs_cff_font_common_priv));
                memcpy(&pt1font->data, &fdptpriv.type1data, sizeof(fdptpriv.type1data));
                pt1font->base = (gs_font *) pdffont->pfont;

                if (!topdict_matrix && offsets.have_matrix) {
                    gs_matrix newfmat, onekmat = { 1000, 0, 0, 1000, 0, 0 };
                    code = gs_matrix_multiply(&onekmat, &pt1font->FontMatrix, &newfmat);
                    memcpy(&pt1font->FontMatrix, &newfmat, sizeof(newfmat));
                }

                pt1font->FAPI = NULL;
                pt1font->client_data = pdffont;

                /* Check the subrs index */
                pdffont->Subrs = NULL;
                if (fdptpriv.pdfcffpriv.subrs) {
                    p = pdfi_count_cff_index(fdptpriv.pdfcffpriv.subrs, e, &pdffont->NumSubrs);
                    if (!p) {
                        pdffont->Subrs = NULL;
                        pdffont->NumSubrs = 0;
                    }
                }

                if (pdffont->NumSubrs > 0) {
                    code = pdfi_object_alloc(ctx, PDF_ARRAY, pdffont->NumSubrs, (pdf_obj **) &pdffont->Subrs);
                    if (code >= 0) {
                        int j;

                        pdffont->Subrs->refcnt = 1;
                        for (j = 0; j < pdffont->NumSubrs; j++) {
                            pdf_string *subrstr;

                            p = pdfi_find_cff_index(fdptpriv.pdfcffpriv.subrs, e, j, &strp, &stre);
                            if (p) {
                                code = pdfi_object_alloc(ctx, PDF_STRING, stre - strp, (pdf_obj **) &subrstr);
                                if (code >= 0) {
                                    memcpy(subrstr->data, strp, subrstr->length);
                                    code = pdfi_array_put(ctx, pdffont->Subrs, (uint64_t) j, (pdf_obj *) subrstr);
                                    if (code < 0) {
                                        subrstr->refcnt = 1;
                                        pdfi_countdown(subrstr);
                                    }
                                }
                            }
                        }
                    }
                }

                pdffont->GlobalSubrs = font->GlobalSubrs;
                pdffont->NumGlobalSubrs = font->NumGlobalSubrs;
                pdfi_countup(pdffont->GlobalSubrs);
                pdffont->CharStrings = font->CharStrings;
                pdfi_countup(pdffont->CharStrings);
                pt1font->data.subroutineNumberBias = subrbias(pdffont->NumSubrs);
                pt1font->data.gsubrNumberBias = subrbias(pdffont->NumGlobalSubrs);

                ptpriv->cidata.FDArray[i] = pt1font;
                (void)pdfi_array_put(ctx, font->FDArray, i, (pdf_obj *) pdffont);
                pdfi_countdown(pdffont);
            }
            if (code < 0) {
                pdfi_countdown(font->FDArray);
                font->FDArray = NULL;
                for (i = 0; i < ptpriv->cidata.FDArray_size; i++) {
                    ptpriv->cidata.FDArray[i] = NULL;
                }
            }
            else {
                if (font->cffdata + offsets.fdselect_off > font->cffend)
                    return_error(gs_error_rangecheck);

                switch ((int)font->cffdata[offsets.fdselect_off]) {
                    case 0:
                        fdselect_proc = format0_fdselect_proc;
                        break;
                    case 3:
                        fdselect_proc = format3_fdselect_proc;
                        break;
                    default:
                        return_error(gs_error_rangecheck);
                }

                if (font->ncharstrings > 0) {
                    int maxcid = 0;
                    for (i = 0; i < font->ncharstrings; i++) {
                        int fd, g;
                        char gkey[64];
                        pdf_string *charstr;

                        fd = fdarray_size <= 1 ? 0 : (*fdselect_proc) (font->cffdata + offsets.fdselect_off + 1, font->cffend, i);

                        p = pdfi_find_cff_index(font->charstrings, font->cffend, i, &strp, &stre);
                        if (!p)
                            continue;

                        code = pdfi_object_alloc(ctx, PDF_STRING, (stre - strp) + 1, (pdf_obj **) &charstr);
                        if (code < 0)
                            continue;
                        charstr->data[0] = (byte) fd;
                        memcpy(charstr->data + 1, strp, charstr->length - 1);

                        if (i == 0) {
                            g = 0;
                        }
                        else {
                            g = (*charset_proc) (font->cffdata + offsets.charset_off + 1, font->cffend, i - 1);
                        }

                        if (g > maxcid) maxcid = g;
                        gs_snprintf(gkey, sizeof(gkey), "%d", g);
                        code = pdfi_dict_put_unchecked(ctx, font->CharStrings, gkey, (pdf_obj *) charstr);
                    }
                    if (maxcid > ptpriv->pdfcffpriv.cidcount - 1)
                        ptpriv->pdfcffpriv.cidcount = maxcid + 1;
                }
            }
        }
    }
    else {
        code = pdfi_cff_build_encoding(ctx, ptpriv, &offsets, charset_proc);
    }
    return code;
}

static int
pdfi_alloc_cff_cidfont(pdf_context *ctx, pdf_cidfont_type0 ** font, uint32_t obj_num)
{
    pdf_cidfont_type0 *cffcidfont = NULL;
    gs_font_cid0 *pfont = NULL;
    gs_matrix defmat = { 0.001f, 0.0f, 0.0f, 0.001f, 0.0f, 0.0f };

    cffcidfont = (pdf_cidfont_type0 *) gs_alloc_bytes(ctx->memory, sizeof(pdf_cidfont_type0), "pdfi (cff pdf_cidfont_type0)");
    if (cffcidfont == NULL)
        return_error(gs_error_VMerror);

    memset(cffcidfont, 0x00, sizeof(pdf_cidfont_type0));
    cffcidfont->ctx = ctx;
    cffcidfont->type = PDF_FONT;
    cffcidfont->pdfi_font_type = e_pdf_cidfont_type0;

#if REFCNT_DEBUG
    cffcidfont->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %" PRIi64 "\n", cffcidfont->type,
              cffcidfont->UID);
#endif

    pdfi_countup(cffcidfont);

    pfont = (gs_font_cid0 *) gs_alloc_struct(ctx->memory, gs_font_cid0, &st_gs_font_cid0, "pdfi (cff cid pfont)");
    if (pfont == NULL) {
        pdfi_countdown(cffcidfont);
        return_error(gs_error_VMerror);
    }
    memset(pfont, 0x00, sizeof(gs_font_cid0));

    cffcidfont->pfont = (gs_font_base *) pfont;
    memcpy(&pfont->orig_FontMatrix, &defmat, sizeof(defmat));
    memcpy(&pfont->FontMatrix, &defmat, sizeof(defmat));
    pfont->next = pfont->prev = 0;
    pfont->memory = ctx->memory;
    pfont->dir = ctx->font_dir;
    pfont->is_resource = false;
    gs_notify_init(&pfont->notify_list, ctx->memory);
    pfont->base = (gs_font *) cffcidfont->pfont;
    pfont->client_data = cffcidfont;
    pfont->WMode = 0;
    pfont->PaintType = 0;
    pfont->StrokeWidth = 0;
    pfont->is_cached = 0;
    pfont->FAPI = NULL;
    pfont->FAPI_font_data = NULL;
    pfont->procs.init_fstack = gs_type0_init_fstack;
    pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pfont->FontType = ft_CID_encrypted;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    /* We may want to do something clever with an XUID here */
    pfont->id = gs_next_ids(ctx->memory, 1);
    uid_set_invalid(&pfont->UID);

    /* The buildchar proc will be filled in by FAPI -
       we won't worry about working without FAPI */
    pfont->procs.encode_char = pdfi_encode_char;
    pfont->procs.glyph_name = ctx->get_glyph_name;
    pfont->procs.decode_glyph = pdfi_cidfont_decode_glyph;
    pfont->procs.define_font = gs_no_define_font;
    pfont->procs.make_font = gs_no_make_font;

    cffcidfont->default_font_info = gs_default_font_info;
    pfont->procs.font_info = pdfi_default_font_info;

    pfont->procs.glyph_info = gs_default_glyph_info;
    pfont->procs.glyph_outline = pdfi_cff_glyph_outline;
    pfont->procs.build_char = NULL;
    pfont->procs.same_font = gs_default_same_font;
    pfont->procs.enumerate_glyph = pdfi_cff_enumerate_glyph;

    pfont->cidata.glyph_data = pdfi_cff_cid_glyph_data;

    pfont->encoding_index = ENCODING_INDEX_UNKNOWN;
    pfont->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

    pfont->client_data = (void *)cffcidfont;

    *font = cffcidfont;
    return 0;
}

static int
pdfi_alloc_cff_font(pdf_context *ctx, pdf_font_cff ** font, uint32_t obj_num, bool for_fdarray)
{
    pdf_font_cff *cfffont = NULL;
    gs_font_type1 *pfont = NULL;
    gs_matrix defmat_font = { 0.001f, 0.0f, 0.0f, 0.001f, 0.0f, 0.0f };
    gs_matrix defmat_fd = { 1.00f, 0.0f, 0.0f, 1.000f, 0.0f, 0.0f };
    gs_matrix *defmat = (for_fdarray ? &defmat_fd : &defmat_font);

    cfffont = (pdf_font_cff *) gs_alloc_bytes(ctx->memory, sizeof(pdf_font_cff), "pdfi (cff pdf_font)");
    if (cfffont == NULL)
        return_error(gs_error_VMerror);

    memset(cfffont, 0x00, sizeof(pdf_font_cff));
    cfffont->ctx = ctx;
    cfffont->type = PDF_FONT;
    cfffont->pdfi_font_type = e_pdf_font_cff;

#if REFCNT_DEBUG
    cfffont->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %" PRIi64 "\n", cfffont->type,
              cfffont->UID);
#endif

    pdfi_countup(cfffont);

    pfont = (gs_font_type1 *) gs_alloc_struct(ctx->memory, gs_font_type1, &st_gs_font_type1, "pdfi (truetype pfont)");
    if (pfont == NULL) {
        pdfi_countdown(cfffont);
        return_error(gs_error_VMerror);
    }
    memset(pfont, 0x00, sizeof(gs_font_type1));

    cfffont->pfont = (gs_font_base *) pfont;
    memcpy(&pfont->orig_FontMatrix, defmat, sizeof(*defmat));
    memcpy(&pfont->FontMatrix, defmat, sizeof(*defmat));
    pfont->next = pfont->prev = 0;
    pfont->memory = ctx->memory;
    pfont->dir = ctx->font_dir;
    pfont->is_resource = false;
    gs_notify_init(&pfont->notify_list, ctx->memory);
    pfont->base = (gs_font *) cfffont->pfont;
    pfont->client_data = cfffont;
    pfont->WMode = 0;
    pfont->PaintType = 0;
    pfont->StrokeWidth = 0;
    pfont->is_cached = 0;
    pfont->FAPI = NULL;
    pfont->FAPI_font_data = NULL;
    pfont->procs.init_fstack = gs_default_init_fstack;
    pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pfont->FontType = ft_encrypted2;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    /* We may want to do something clever with an XUID here */
    pfont->id = gs_next_ids(ctx->memory, 1);
    uid_set_invalid(&pfont->UID);

    /* The buildchar proc will be filled in by FAPI -
       we won't worry about working without FAPI */
    pfont->procs.encode_char = pdfi_encode_char;
    pfont->procs.glyph_name = ctx->get_glyph_name;
    pfont->procs.decode_glyph = pdfi_decode_glyph;
    pfont->procs.define_font = gs_no_define_font;
    pfont->procs.make_font = gs_no_make_font;

    cfffont->default_font_info = gs_default_font_info;
    pfont->procs.font_info = pdfi_default_font_info;

    pfont->procs.glyph_info = gs_default_glyph_info;
    pfont->procs.glyph_outline = pdfi_cff_glyph_outline;
    pfont->procs.build_char = NULL;
    pfont->procs.same_font = gs_default_same_font;
    pfont->procs.enumerate_glyph = pdfi_cff_enumerate_glyph;

    pfont->data.procs.glyph_data = for_fdarray ? pdfi_cff_fdarray_glyph_data : pdfi_cff_glyph_data;
    pfont->data.procs.subr_data = pdfi_cff_subr_data;
    pfont->data.procs.seac_data = for_fdarray ? pdfi_cff_fdarray_seac_data : pdfi_cff_seac_data;
    pfont->data.procs.push_values = pdfi_cff_push;
    pfont->data.procs.pop_value = pdfi_cff_pop;
    pfont->data.interpret = gs_type2_interpret;
    pfont->data.lenIV = -1;

    pfont->encoding_index = ENCODING_INDEX_UNKNOWN;
    pfont->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

    pfont->client_data = (void *)cfffont;

    *font = cfffont;
    return 0;
}

static void
pdfi_init_cff_font_priv(pdf_context *ctx, pdfi_gs_cff_font_priv *cffpriv,
                        byte *buf, int buflen, bool for_fdarray)
{
    gs_matrix defmat_font = { 0.001f, 0.0f, 0.0f, 0.001f, 0.0f, 0.0f };
    gs_matrix defmat_fd = { 1.00f, 0.0f, 0.0f, 1.000f, 0.0f, 0.0f };
    gs_matrix *defmat = (for_fdarray ? &defmat_fd : &defmat_font);

    memset(cffpriv, 0x00, sizeof(pdfi_gs_cff_font_priv));

    cffpriv->pdfcffpriv.ctx = ctx;
    cffpriv->pdfcffpriv.type = PDF_FONT;
    cffpriv->pdfcffpriv.pdfi_font_type = e_pdf_font_cff;
    cffpriv->client_data = (void *)(&cffpriv->pdfcffpriv);
    cffpriv->pdfcffpriv.pfont = (gs_font_base *)cffpriv;
    /* Dummy value for dummy object */
    cffpriv->pdfcffpriv.refcnt = 0xf0f0f0f0;
    cffpriv->pdfcffpriv.cffdata = buf;
    cffpriv->pdfcffpriv.cffend = buf + buflen;
    cffpriv->pdfcffpriv.cidcount = 8720;

    memcpy(&cffpriv->orig_FontMatrix, defmat, sizeof(*defmat));
    memcpy(&cffpriv->FontMatrix, defmat, sizeof(*defmat));
    cffpriv->next = cffpriv->prev = 0;
    cffpriv->memory = ctx->memory;
    cffpriv->dir = ctx->font_dir;
    cffpriv->is_resource = false;
    gs_notify_init(&cffpriv->notify_list, ctx->memory);
    cffpriv->WMode = 0;
    cffpriv->PaintType = 0;
    cffpriv->StrokeWidth = 0;
    cffpriv->is_cached = 0;
    cffpriv->FAPI = NULL;
    cffpriv->FAPI_font_data = NULL;
    cffpriv->procs.init_fstack = gs_default_init_fstack;
    cffpriv->procs.next_char_glyph = gs_default_next_char_glyph;
    cffpriv->FontType = ft_encrypted2;
    cffpriv->ExactSize = fbit_use_outlines;
    cffpriv->InBetweenSize = fbit_use_outlines;
    cffpriv->TransformedChar = fbit_use_outlines;
    /* We may want to do something clever with an XUID here */
    cffpriv->id = gs_next_ids(ctx->memory, 1);
    uid_set_invalid(&cffpriv->UID);


    /* The buildchar proc will be filled in by FAPI -
       we won't worry about working without FAPI */
    cffpriv->procs.encode_char = pdfi_encode_char;
    cffpriv->procs.glyph_name = ctx->get_glyph_name;
    cffpriv->procs.decode_glyph = pdfi_decode_glyph;
    cffpriv->procs.define_font = gs_no_define_font;

    cffpriv->pdfcffpriv.default_font_info = gs_default_font_info;
    cffpriv->procs.font_info = pdfi_default_font_info;

    cffpriv->procs.glyph_info = gs_default_glyph_info;
    cffpriv->procs.glyph_outline = pdfi_cff_glyph_outline;
    cffpriv->procs.build_char = NULL;
    cffpriv->procs.same_font = gs_default_same_font;
    cffpriv->procs.enumerate_glyph = pdfi_cff_enumerate_glyph;

    cffpriv->type1data.procs.glyph_data = pdfi_cff_glyph_data;
    cffpriv->type1data.procs.subr_data = pdfi_cff_subr_data;
    cffpriv->type1data.procs.seac_data = pdfi_cff_seac_data;
    cffpriv->type1data.procs.push_values = pdfi_cff_push;
    cffpriv->type1data.procs.pop_value = pdfi_cff_pop;
    cffpriv->type1data.interpret = gs_type2_interpret;
    cffpriv->type1data.lenIV = -1;

    cffpriv->encoding_index = ENCODING_INDEX_UNKNOWN;
    cffpriv->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;
}

int
pdfi_read_cff_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, byte *pfbuf, int64_t fbuflen, bool forcecid, pdf_font **ppdffont)
{
    int code = 0;

    pdf_font *ppdfont = NULL;
    pdf_obj *basefont = NULL;
    pdf_obj *tmp = NULL;
    pdf_obj *fontdesc = NULL;
    pdf_string *registry = NULL;
    pdf_string *ordering = NULL;
    byte *fbuf = pfbuf;

    if (!memcmp(fbuf, "OTTO", 4)) {
        int i, ntables;
        byte *p;
        uint32_t toffs = 0, tlen = 0;

        code = u16(fbuf + 4, fbuf + fbuflen, &ntables);

        if (code < 0 || ntables > 64)
            return_error(gs_error_invalidfont);

        for (i = 0; i < ntables; i++) {
            p = fbuf + 12 + i * 16;
            if (p >= fbuf + fbuflen)
                break;

            if (!memcmp(p, "CFF ", 4)) {
                code = u32(p + 8, fbuf + fbuflen, (int *)&toffs);
                if (code >= 0)
                    code = u32(p + 12, fbuf + fbuflen, (int *)&tlen);
                if (code < 0) {
                    toffs = tlen = 0;
                }
                break;
            }
        }
        /* Sanity check the offset and size of the CFF table and make sure the declared
         * size and position fits inside the data we have. Promote the 32-bit variables to
         * 64-bit to avoid overflow calculating the end of the table.
         */
        if (toffs == 0 || tlen == 0 || (uint64_t)toffs + (uint64_t)tlen > fbuflen) {
            gs_free_object(ctx->memory, pfbuf, "pdfi_read_cff_font(fbuf)");
            return_error(gs_error_invalidfont);
        }
        fbuf += toffs;
        fbuflen = tlen;
    }

    if (font_dict != NULL) {
        code = pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, &fontdesc);
        if (code < 0) {
            fontdesc = NULL;
        }
    }
    else {
        fontdesc = NULL;
    }

    /* Vestigial magic number check - we can't check the third byte, as we have out of
       spec fonts that have a head size > 4
     */
    if (fbuf[0] == 1 && fbuf[1] == 0 && code >= 0) {
        pdfi_gs_cff_font_priv cffpriv;

        pdfi_init_cff_font_priv(ctx, &cffpriv, fbuf, fbuflen, false);
        cffpriv.forcecid = forcecid;
        code = pdfi_read_cff(ctx, &cffpriv);

        if (code >= 0) {
            if (cffpriv.FontType == ft_CID_encrypted) {
                pdf_obj *obj = NULL;
                pdf_cidfont_type0 *cffcid;
                gs_font_cid0 *pfont;

                code = pdfi_alloc_cff_cidfont(ctx, &cffcid, font_dict->object_num);
                pfont = (gs_font_cid0 *) cffcid->pfont;
                ppdfont = (pdf_font *) cffcid;

                memcpy(pfont, &cffpriv, sizeof(pdfi_gs_cff_font_common_priv));
                memcpy(&pfont->cidata, &cffpriv.cidata, sizeof(pfont->cidata));

                pfont->procs.glyph_outline = pdfi_cff_glyph_outline;
                pfont->cidata.glyph_data = pdfi_cff_cid_glyph_data;

                cffcid->orig_glyph_info = pfont->procs.glyph_info;
                pfont->procs.glyph_info = pdfi_cff_cidfont_glyph_info;

                pfont->cidata.proc_data = NULL;
                pfont->FAPI = NULL;
                pfont->base = (gs_font *) cffcid->pfont;

                code = pdfi_dict_knownget_type(ctx, font_dict, "CIDSystemInfo", PDF_DICT, (pdf_obj **)&obj);
                if (code <= 0) {
                    cffcid->registry = cffpriv.pdfcffpriv.registry;
                    cffcid->ordering = cffpriv.pdfcffpriv.ordering;
                    cffcid->supplement = cffpriv.pdfcffpriv.supplement;
                }
                else {
                    pdf_num *suppl = NULL;

                    code = pdfi_dict_knownget_type(ctx, (pdf_dict *)obj, "Registry", PDF_STRING, (pdf_obj **)&cffcid->registry);
                    if (code <= 0) {
                        cffcid->registry = cffpriv.pdfcffpriv.registry;
                    }
                    else {
                        pdfi_countdown(cffpriv.pdfcffpriv.registry);
                        cffpriv.pdfcffpriv.registry = NULL;
                    }

                    code = pdfi_dict_knownget_type(ctx, (pdf_dict *)obj, "Ordering", PDF_STRING, (pdf_obj **)&cffcid->ordering);
                    if (code <= 0) {
                        cffcid->ordering = cffpriv.pdfcffpriv.ordering;
                    }
                    else {
                        pdfi_countdown(cffpriv.pdfcffpriv.ordering);
                        cffpriv.pdfcffpriv.ordering = NULL;
                    }
                    code = pdfi_dict_knownget_type(ctx, (pdf_dict *)obj, "Supplement", PDF_INT, (pdf_obj **)&suppl);
                    if (code <= 0 || pdfi_type_of(suppl) != PDF_INT) {
                        cffcid->supplement = cffpriv.pdfcffpriv.supplement;
                    }
                    else {
                        cffcid->supplement = suppl->value.i;
                    }
                    pdfi_countdown(suppl);
                }
                pdfi_countdown(obj);
                obj = NULL;

                pfont->cidata.common.CIDSystemInfo.Registry.data = cffcid->registry->data;
                pfont->cidata.common.CIDSystemInfo.Registry.size = cffcid->registry->length;
                pfont->cidata.common.CIDSystemInfo.Ordering.data = cffcid->ordering->data;
                pfont->cidata.common.CIDSystemInfo.Ordering.size = cffcid->ordering->length;
                pfont->cidata.common.CIDSystemInfo.Supplement = cffcid->supplement;

                cffcid->FontDescriptor = (pdf_dict *) fontdesc;
                fontdesc = NULL;

                cffcid->PDF_font = font_dict;
                pdfi_countup(font_dict);

                pfont->client_data = cffcid;

                cffcid->object_num = font_dict->object_num;
                cffcid->generation_num = font_dict->generation_num;
                cffcid->indirect_num = font_dict->indirect_num;
                cffcid->indirect_gen = font_dict->indirect_gen;

                cffcid->CharStrings = cffpriv.pdfcffpriv.CharStrings;
                cffpriv.pdfcffpriv.CharStrings = NULL;

                cffcid->Subrs = cffpriv.pdfcffpriv.Subrs;
                cffcid->NumSubrs = cffpriv.pdfcffpriv.NumSubrs;
                cffpriv.pdfcffpriv.Subrs = NULL;

                cffcid->GlobalSubrs = cffpriv.pdfcffpriv.GlobalSubrs;
                cffcid->NumGlobalSubrs = cffpriv.pdfcffpriv.NumGlobalSubrs;
                cffpriv.pdfcffpriv.GlobalSubrs = NULL;

                cffcid->FDArray = cffpriv.pdfcffpriv.FDArray;
                cffpriv.pdfcffpriv.FDArray = NULL;

                pfont->cidata.common.CIDCount = cffpriv.pdfcffpriv.cidcount;

                cffcid->cidtogidmap = NULL;
                code = pdfi_dict_knownget(ctx, font_dict, "CIDToGIDMap", (pdf_obj **) &obj);
                if (code > 0) {
                    /* CIDToGIDMap can only be a stream or a name, and if it's a name
                       it's only permitted to be "/Identity", so ignore it
                     */
                    if (pdfi_type_of(obj) == PDF_STREAM) {
                        byte *d;
                        int64_t sz = 0;

                        code = pdfi_object_alloc(ctx, PDF_BUFFER, 0, (pdf_obj **)&cffcid->cidtogidmap);
                        if (code < 0) {
                            pdfi_countdown(obj);
                            goto error;
                        }
                        pdfi_countup(cffcid->cidtogidmap);
                        code = pdfi_stream_to_buffer(ctx, (pdf_stream *)obj, &d, &sz);
                        if (code < 0) {
                            pdfi_countdown(obj);
                            goto error;
                        }
                        code = pdfi_buffer_set_data((pdf_obj *)cffcid->cidtogidmap, d, (int32_t)sz);
                        if (code < 0) {
                            pdfi_countdown(obj);
                            goto error;
                        }
                    }
                    pdfi_countdown(obj);
                    obj = NULL;

                    if (cffcid->cidtogidmap != NULL && cffcid->cidtogidmap->length > 0) {
                        pfont->cidata.common.CIDCount = cffcid->cidtogidmap->length >> 1;
                    }
                }
                pfont->cidata.common.MaxCID = pfont->cidata.common.CIDCount - 1;

                code = pdfi_dict_knownget_number(ctx, font_dict, "DW", &cffcid->DW);
                if (code <= 0) {
                    cffcid->DW = 1000;
                }

                code = pdfi_dict_knownget_type(ctx, font_dict, "DW2", PDF_ARRAY, (pdf_obj **) &obj);
                if (code > 0) {
                    cffcid->DW2 = (pdf_array *) obj;
                    obj = NULL;
                }
                else {
                    cffcid->DW2 = NULL;
                }
                code = pdfi_dict_knownget_type(ctx, font_dict, "W", PDF_ARRAY, (pdf_obj **) &obj);
                if (code > 0) {
                    cffcid->W = (pdf_array *) obj;
                    obj = NULL;
                }
                else {
                    cffcid->W = NULL;
                }
                code = pdfi_dict_knownget_type(ctx, font_dict, "W2", PDF_ARRAY, (pdf_obj **) &obj);
                if (code > 0) {
                    cffcid->W2 = (pdf_array *) obj;
                    obj = NULL;
                }
                else {
                    cffcid->W2 = NULL;
                }
                if (uid_is_XUID(&cffcid->pfont->UID))
                    uid_free(&cffcid->pfont->UID, cffcid->pfont->memory, "pdfi_read_type1_font");
                uid_set_invalid(&cffcid->pfont->UID);
                cffcid->pfont->id = gs_next_ids(ctx->memory, 1);
            }
            else if (forcecid) {
                pdf_obj *obj;
                pdf_cidfont_type0 *cffcid;
                gs_font_cid0 *pfont;
                pdf_font_cff *fdcfffont;
                gs_font_type1 *pfdfont = NULL;
                static const char *const reg = "Adobe";
                static const char *const ord = "Identity";

                code = pdfi_object_alloc(ctx, PDF_STRING, strlen(reg), (pdf_obj **) &registry);
                if (code < 0)
                    goto error;
                pdfi_countup(registry);

                code = pdfi_object_alloc(ctx, PDF_STRING, strlen(ord), (pdf_obj **) &ordering);
                if (code < 0) {
                    goto error;
                }
                pdfi_countup(ordering);

                memcpy(registry->data, reg, strlen(reg));
                registry->length = strlen(reg);
                memcpy(ordering->data, ord, strlen(ord));
                ordering->length = strlen(ord);

                code = pdfi_alloc_cff_font(ctx, &fdcfffont, 0, true);
                if (code < 0)
                    goto error;

                pfdfont = (gs_font_type1 *) fdcfffont->pfont;

                code = pdfi_alloc_cff_cidfont(ctx, &cffcid, 0);
                if (code < 0) {
                    gs_free_object(ctx->memory, fdcfffont, "pdfi_read_cff_font");
                    gs_free_object(ctx->memory, pfdfont, "pdfi_read_cff_font");
                    goto error;
                }
                ppdfont = (pdf_font *) cffcid;

                code = pdfi_object_alloc(ctx, PDF_ARRAY, 1, (pdf_obj **) &cffcid->FDArray);
                if (code < 0)
                    goto error;
                pdfi_countup(cffcid->FDArray);

                pfont = (gs_font_cid0 *) cffcid->pfont;
                pfont->cidata.FDArray = (gs_font_type1 **) gs_alloc_bytes(ctx->memory, sizeof(gs_font_type1 *), "pdfi_read_cff_font");
                pfont->base = (gs_font *)pfont;
                if (!pfont->cidata.FDArray) {
                    pdfi_countdown(cffcid->FDArray);
                    gs_free_object(ctx->memory, fdcfffont, "pdfi_read_cff_font");
                    gs_free_object(ctx->memory, pfdfont, "pdfi_read_cff_font");
                    gs_free_object(ctx->memory, cffcid, "pdfi_read_cff_font");
                    gs_free_object(ctx->memory, pfont, "pdfi_read_cff_font");
                    goto error;
                }

                memcpy(pfdfont, &cffpriv, sizeof(pdfi_gs_cff_font_common_priv));
                memcpy(&pfdfont->data, &cffpriv.type1data, sizeof(pfdfont->data));


                pfont->procs.glyph_outline = pdfi_cff_glyph_outline;
                pfont->cidata.glyph_data = pdfi_cff_cid_glyph_data;
                pfont->cidata.common.CIDCount = cffpriv.pdfcffpriv.CharStrings->entries;
                pfont->cidata.common.MaxCID = pfont->cidata.common.CIDCount - 1;

                cffcid->orig_glyph_info = pfont->procs.glyph_info;
                pfont->procs.glyph_info = pdfi_cff_cidfont_glyph_info;

                pfdfont->FAPI = NULL;
                pfdfont->base = (gs_font *)pfdfont;
                pfdfont->client_data = fdcfffont;
                pdfi_array_put(ctx, cffcid->FDArray, 0, (pdf_obj *) fdcfffont);

                fdcfffont->object_num = 0;
                fdcfffont->generation_num = 0;

                (void)pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &basefont);
                fdcfffont->BaseFont = basefont;
                fdcfffont->Name = basefont;
                pdfi_countup(basefont);

                pdfi_countdown(cffpriv.pdfcffpriv.Encoding);
                cffpriv.pdfcffpriv.Encoding = NULL;

                fdcfffont->CharStrings = cffpriv.pdfcffpriv.CharStrings;
                cffpriv.pdfcffpriv.CharStrings = NULL;
                fdcfffont->Subrs = cffpriv.pdfcffpriv.Subrs;
                cffpriv.pdfcffpriv.Subrs = NULL;
                fdcfffont->NumSubrs = cffpriv.pdfcffpriv.NumSubrs;
                fdcfffont->GlobalSubrs = cffpriv.pdfcffpriv.GlobalSubrs;
                cffpriv.pdfcffpriv.GlobalSubrs = NULL;
                fdcfffont->NumGlobalSubrs = cffpriv.pdfcffpriv.NumGlobalSubrs;

                cffcid->CharStrings = fdcfffont->CharStrings;
                pdfi_countup(cffcid->CharStrings);
                cffcid->Subrs = fdcfffont->Subrs;
                pdfi_countup(cffcid->Subrs);
                cffcid->GlobalSubrs = fdcfffont->GlobalSubrs;
                pdfi_countup(cffcid->GlobalSubrs);
                pdfi_countdown(fdcfffont);

                cffcid->FontDescriptor = (pdf_dict *) fontdesc;
                fontdesc = NULL;

                cffcid->registry = registry;
                cffcid->ordering = ordering;
                registry = ordering = NULL;
                cffcid->supplement = 0;

                /* Because we're faking a CIDFont, we want to move the scaling to the "parent" fake
                   CIDFont, and make the FDArrray use identity scaling
                 */
                memcpy(&pfont->FontMatrix, &pfdfont->FontMatrix, sizeof(pfdfont->FontMatrix));
                memcpy(&pfont->orig_FontMatrix, &pfdfont->orig_FontMatrix, sizeof(pfdfont->orig_FontMatrix));

                gs_make_identity(&pfdfont->FontMatrix);
                gs_make_identity(&pfdfont->orig_FontMatrix);

                pfont->cidata.CIDMapOffset = 0;
                pfont->cidata.FDArray_size = 1;
                pfont->cidata.FDBytes = 0;
                pfont->cidata.glyph_data = pdfi_cff_cid_glyph_data;
                pfont->cidata.FDArray[0] = pfdfont;
                pfont->cidata.common.CIDSystemInfo.Registry.data = cffcid->registry->data;
                pfont->cidata.common.CIDSystemInfo.Registry.size = cffcid->registry->length;
                pfont->cidata.common.CIDSystemInfo.Ordering.data = cffcid->ordering->data;
                pfont->cidata.common.CIDSystemInfo.Ordering.size = cffcid->ordering->length;
                pfont->cidata.common.CIDSystemInfo.Supplement = cffcid->supplement;
                pfont->client_data = cffcid;

                cffcid->object_num = font_dict->object_num;
                cffcid->generation_num = font_dict->generation_num;
                cffcid->indirect_num = font_dict->indirect_num;
                cffcid->indirect_gen = font_dict->indirect_gen;

                cffcid->PDF_font = font_dict;
                pdfi_countup(font_dict);

                cffcid->cidtogidmap = NULL;
                code = pdfi_dict_knownget(ctx, font_dict, "CIDToGIDMap", (pdf_obj **) &obj);
                if (code > 0) {
                    byte *d;
                    int64_t sz = 0;
                    /* CIDToGIDMap can only be a stream or a name, and if it's a name
                       it's only permitted to be "/Identity", so ignore it
                     */
                    if (pdfi_type_of(obj) == PDF_STREAM) {
                        code = pdfi_object_alloc(ctx, PDF_BUFFER, 0, (pdf_obj **)&cffcid->cidtogidmap);
                        if (code < 0) {
                            goto error;
                        }
                        pdfi_countup(cffcid->cidtogidmap);
                        code = pdfi_stream_to_buffer(ctx, (pdf_stream *)obj, &d, &sz);
                        if (code < 0) {
                            goto error;
                        }
                        code = pdfi_buffer_set_data((pdf_obj *)cffcid->cidtogidmap, d, (int32_t)sz);
                        if (code < 0) {
                            goto error;
                        }
                    }
                    pdfi_countdown(obj);
                    obj = NULL;

                    if (cffcid->cidtogidmap != NULL && cffcid->cidtogidmap->length > 0) {
                        pfont->cidata.common.CIDCount = cffcid->cidtogidmap->length >> 1;
                    }
                }
                pfont->cidata.common.MaxCID = pfont->cidata.common.CIDCount - 1;

                code = pdfi_dict_knownget_number(ctx, font_dict, "DW", &cffcid->DW);
                if (code <= 0) {
                    cffcid->DW = 1000;
                }

                code = pdfi_dict_knownget_type(ctx, font_dict, "DW2", PDF_ARRAY, (pdf_obj **) &obj);
                if (code > 0) {
                    cffcid->DW2 = (pdf_array *) obj;
                    obj = NULL;
                }
                else {
                    cffcid->DW2 = NULL;
                }
                code = pdfi_dict_knownget_type(ctx, font_dict, "W", PDF_ARRAY, (pdf_obj **) &obj);
                if (code > 0) {
                    cffcid->W = (pdf_array *) obj;
                    obj = NULL;
                }
                else {
                    cffcid->W = NULL;
                }
                code = pdfi_dict_knownget_type(ctx, font_dict, "W2", PDF_ARRAY, (pdf_obj **) &obj);
                if (code > 0) {
                    cffcid->W2 = (pdf_array *) obj;
                    obj = NULL;
                }
                else {
                    cffcid->W2 = NULL;
                }

                if (uid_is_XUID(&cffcid->pfont->UID))
                    uid_free(&cffcid->pfont->UID, cffcid->pfont->memory, "pdfi_read_type1_font");
                uid_set_invalid(&cffcid->pfont->UID);
                cffcid->pfont->id = gs_next_ids(ctx->memory, 1);
            }
            else {
                pdf_font_cff *cfffont;
                gs_font_type1 *pfont = NULL;
                pdf_obj *tounicode = NULL;

                code = pdfi_alloc_cff_font(ctx, &cfffont, font_dict != NULL ? font_dict->object_num : 0, false);
                pfont = (gs_font_type1 *) cfffont->pfont;
                ppdfont = (pdf_font *) cfffont;

                memcpy(pfont, &cffpriv, sizeof(pdfi_gs_cff_font_common_priv));
                memcpy(&pfont->data, &cffpriv.type1data, sizeof(pfont->data));
                pfont->FAPI = NULL;
                pfont->client_data = cfffont;
                pfont->base = (gs_font *) cfffont->pfont;

                pfont->procs.glyph_info = pdfi_cff_glyph_info;

                if (font_dict) {
                    cfffont->object_num = font_dict->object_num;
                    cfffont->generation_num = font_dict->generation_num;
                    cfffont->indirect_num = font_dict->indirect_num;
                    cfffont->indirect_gen = font_dict->indirect_gen;

                    (void)pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &basefont);
                }

                cfffont->BaseFont = basefont;
                cfffont->Name = basefont;
                pdfi_countup(basefont);

                cfffont->CharStrings = cffpriv.pdfcffpriv.CharStrings;
                cffpriv.pdfcffpriv.CharStrings = NULL;

                cfffont->Subrs = cffpriv.pdfcffpriv.Subrs;
                cfffont->NumSubrs = cffpriv.pdfcffpriv.NumSubrs;
                cffpriv.pdfcffpriv.Subrs = NULL;

                cfffont->GlobalSubrs = cffpriv.pdfcffpriv.GlobalSubrs;
                cfffont->NumGlobalSubrs = cffpriv.pdfcffpriv.NumGlobalSubrs;
                cffpriv.pdfcffpriv.GlobalSubrs = NULL;

                cfffont->FontDescriptor = (pdf_dict *) fontdesc;
                fontdesc = NULL;

                cfffont->PDF_font = font_dict;
                pdfi_countup(font_dict);

                cfffont->descflags = 0;
                if (cfffont->FontDescriptor != NULL) {
                    code = pdfi_dict_get_int(ctx, cfffont->FontDescriptor, "Flags", &cfffont->descflags);
                    if (code >= 0) {
                        /* If both the symbolic and non-symbolic flag are set,
                           believe that latter.
                         */
                        if ((cfffont->descflags & 32) != 0)
                            cfffont->descflags = (cfffont->descflags & ~4);
                    }
                }
                /* ZapfDingbats and Symbol we just have to know are symbolic */
                if (pdfi_font_known_symbolic(basefont)) {
                    cfffont->descflags |= 4;
                }

                pdfi_font_set_first_last_char(ctx, font_dict, (pdf_font *)cfffont);

                /* Widths are defined assuming a 1000x1000 design grid, but we apply
                 * them in font space - so undo the 1000x1000 scaling, and apply
                 * the inverse of the font's x scaling
                 */
                if (font_dict != NULL) {
                    /* ignore errors with widths... for now */
                    (void)pdfi_font_create_widths(ctx, font_dict, (pdf_font*)cfffont, (double)(0.001 / hypot(pfont->FontMatrix.xx, pfont->FontMatrix.xy)));
                }

                if (font_dict != NULL)
                    code = pdfi_dict_knownget(ctx, font_dict, "Encoding", &tmp);
                else
                    code = gs_error_undefined;
                if (code == 1) {
                    if ((cfffont->descflags & 4) != 0 && pdfi_type_of(tmp) == PDF_DICT) {
                        code = pdfi_create_Encoding(ctx, (pdf_font *)cfffont, tmp, (pdf_obj *)cffpriv.pdfcffpriv.Encoding, (pdf_obj **) &cfffont->Encoding);
                        if (code >= 0) {
                            pdfi_countdown(cffpriv.pdfcffpriv.Encoding);
                            cffpriv.pdfcffpriv.Encoding = NULL;
                            code = 1;
                        }
                    }
                    else if ((pdfi_type_of(tmp) == PDF_NAME || pdfi_type_of(tmp) == PDF_DICT)) {
                        code = pdfi_create_Encoding(ctx, (pdf_font *)cfffont, tmp, NULL, (pdf_obj **) &cfffont->Encoding);
                        if (code >= 0) {
                            pdfi_countdown(cffpriv.pdfcffpriv.Encoding);
                            cffpriv.pdfcffpriv.Encoding = NULL;
                            code = 1;
                        }
                    }
                    else
                        code = gs_error_undefined;

                    if (code == 1) {
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
                    cfffont->Encoding = cffpriv.pdfcffpriv.Encoding;
                    cffpriv.pdfcffpriv.Encoding = NULL;
                    cfffont->pfont->encoding_index = cffpriv.encoding_index;
                    cfffont->pfont->nearest_encoding_index = cffpriv.nearest_encoding_index;
                }

                /* Since the underlying font stream can be shared between font descriptors,
                   and the font descriptors can be shared between font objects, if we change
                   the encoding, we can't share cached glyphs with other instances of this
                   underlying font, so invalidate the UniqueID/XUID so the glyph cache won't
                   try.
                */
                if (uid_is_XUID(&cfffont->pfont->UID))
                    uid_free(&cfffont->pfont->UID, cfffont->pfont->memory, "pdfi_read_type1_font");
                uid_set_invalid(&cfffont->pfont->UID);
                cfffont->pfont->id = gs_next_ids(ctx->memory, 1);

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
                cfffont->ToUnicode = tounicode;
                tounicode = NULL;
            }
        }
error:
        if (code < 0) {
            pdfi_countdown(cffpriv.pdfcffpriv.Subrs);
            pdfi_countdown(cffpriv.pdfcffpriv.GlobalSubrs);
            pdfi_countdown(cffpriv.pdfcffpriv.CharStrings);
            pdfi_countdown(cffpriv.pdfcffpriv.CIDSystemInfo);
            pdfi_countdown(cffpriv.pdfcffpriv.W);
            pdfi_countdown(cffpriv.pdfcffpriv.DW2);
            pdfi_countdown(cffpriv.pdfcffpriv.W2);
            pdfi_countdown(cffpriv.pdfcffpriv.FDArray);
            pdfi_countdown(cffpriv.pdfcffpriv.registry);
            pdfi_countdown(cffpriv.pdfcffpriv.ordering);
            pdfi_countdown(cffpriv.pdfcffpriv.Encoding);
            if (cffpriv.FontType == ft_CID_encrypted) {
                if (ppdfont != NULL && ppdfont->pfont != NULL) {
                    ((gs_font_cid0 *)ppdfont->pfont)->cidata.FDArray = NULL;
                }
                gs_free_object(ctx->memory, cffpriv.cidata.FDArray, "pdfi_read_cff_font(gs_font FDArray, error)");
            }
        }
        else {
            code = pdfi_font_generate_pseudo_XUID(ctx, font_dict, ppdfont->pfont);
            if (code < 0) {
                goto error;
            }

            code = gs_definefont(ctx->font_dir, (gs_font *) ppdfont->pfont);

            if (code >= 0)
                code = pdfi_fapi_passfont((pdf_font *) ppdfont, 0, NULL, NULL, NULL, 0);

            /* object_num can be zero if the dictionary was defined inline */
            if (code >= 0 && ppdfont->object_num != 0) {
                (void)replace_cache_entry(ctx, (pdf_obj *) ppdfont);
            }

            if (code >= 0) {
                *ppdffont = (pdf_font *) ppdfont;
                ppdfont = NULL;
            }
        }
    }
    gs_free_object(ctx->memory, pfbuf, "pdfi_read_cff_font(fbuf)");
    pdfi_countdown(ppdfont);
    pdfi_countdown(fontdesc);
    pdfi_countdown(ordering);
    pdfi_countdown(registry);

    if (code < 0) {
        tmp = NULL;
        if (font_dict != NULL) {
            if (pdfi_dict_get(ctx, font_dict, ".Path", &tmp) >= 0)
            {
                char fname[gp_file_name_sizeof + 1];
                pdf_string *fobj = (pdf_string *)tmp;

                memcpy(fname, fobj->data, fobj->length > gp_file_name_sizeof ? gp_file_name_sizeof : fobj->length);
                fname[fobj->length > gp_file_name_sizeof ? gp_file_name_sizeof : fobj->length] = '\0';

                pdfi_set_error_var(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_cff_font", "Error reading CFF font file %s\n", fname);
            }
            else {
                pdfi_set_error_var(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_cff_font", "Error reading embedded Type1C font object %u\n", font_dict->object_num);
            }
        }
        else {
            pdfi_set_error(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_read_truetype_font", "Error reading font\n");
        }
        pdfi_countdown(tmp);
        *ppdffont = NULL;
        return_error(gs_error_invalidfont);
    }

    return code;
}

int
pdfi_read_type1C_font(pdf_context *ctx, pdf_dict *font_dict,
                      pdf_dict *stream_dict, pdf_dict *page_dict, pdf_font **ppdffont)
{
    int code;
    pdf_obj *fontdesc = NULL;
    pdf_obj *fontfile = NULL;
    byte *fbuf;
    int64_t fbuflen = 0;

    code = pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, &fontdesc);

    if (code >=0 && fontdesc != NULL) {
        code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile", PDF_STREAM, &fontfile);

        if (code < 0)
            code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile2", PDF_STREAM, &fontfile);

        if (code < 0)
            code = pdfi_dict_get_type(ctx, (pdf_dict *) fontdesc, "FontFile3", PDF_STREAM, &fontfile);
    }
    pdfi_countdown(fontdesc);

    if (code >= 0 && fontfile != NULL) {
        code = pdfi_stream_to_buffer(ctx, (pdf_stream *) fontfile, &fbuf, &fbuflen);
        pdfi_countdown(fontfile);
    }
    else {
        /* TODO - handle non-emebedded case */
        return_error(gs_error_invalidfont);
    }

    code = pdfi_read_cff_font(ctx, stream_dict, page_dict, font_dict, fbuf, fbuflen, false, ppdffont);

    return code;
}

int
pdfi_copy_cff_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont)
{
    int code = 0;
    pdf_font_cff *font = NULL;
    gs_font_type1 *spfont1 = (gs_font_type1 *) spdffont->pfont;
    gs_font_type1 *dpfont1;
    gs_id t_id;
    pdf_obj *tmp;

    if (font_dict == NULL)
        return_error(gs_error_invalidfont);

    code = pdfi_alloc_cff_font(ctx, &font, font_dict->object_num, false);
    if (code < 0)
        return code;
    dpfont1 = (gs_font_type1 *) font->pfont;

    t_id = dpfont1->id;
    memcpy(dpfont1, spfont1, sizeof(gs_font_type1));
    dpfont1->id = t_id;
    dpfont1->FAPI = NULL;
    dpfont1->FAPI_font_data = NULL;
    dpfont1->notify_list.memory = NULL;
    dpfont1->notify_list.first = NULL;
    gs_notify_init(&dpfont1->notify_list, dpfont1->memory);

    memcpy(font, spdffont, sizeof(pdf_font_type1));
    font->refcnt = 1;
    font->pfont = (gs_font_base *)dpfont1;
    dpfont1->client_data = (void *)font;
    font->filename = NULL;

    font->PDF_font = font_dict;
    font->object_num = font_dict->object_num;
    font->generation_num = font_dict->generation_num;
    pdfi_countup(font->PDF_font);

    /* We want basefont and descriptor, but we can live without them */
    font->BaseFont = NULL;
    (void)pdfi_dict_knownget_type(ctx, font_dict, "BaseFont", PDF_NAME, &font->BaseFont);
    font->FontDescriptor = NULL;
    (void)pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, (pdf_obj **)&font->FontDescriptor);

    pdfi_countup(font->Name);
    pdfi_countup(font->CharStrings);
    pdfi_countup(font->Subrs);
    pdfi_countup(font->GlobalSubrs);

    if (font->BaseFont != NULL && ((pdf_name *)font->BaseFont)->length <= gs_font_name_max) {
        memcpy(dpfont1->key_name.chars, ((pdf_name *)font->BaseFont)->data, ((pdf_name *)font->BaseFont)->length);
        dpfont1->key_name.size = ((pdf_name *)font->BaseFont)->length;
        memcpy(dpfont1->font_name.chars, ((pdf_name *)font->BaseFont)->data, ((pdf_name *)font->BaseFont)->length);
        dpfont1->font_name.size = ((pdf_name *)font->BaseFont)->length;
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
        font->descflags |= 4;
    }


    tmp = NULL;
    code = pdfi_dict_knownget(ctx, font_dict, "Encoding", &tmp);
    if (code == 1) {
        if ((font->descflags & 4) != 0 && pdfi_type_of(tmp) == PDF_DICT) {
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, tmp, NULL, (pdf_obj **) & font->Encoding);
            if (code >= 0)
                code = 1;
        }
        else if ((pdfi_type_of(tmp) == PDF_NAME || pdfi_type_of(tmp) == PDF_DICT)) {
            code = pdfi_create_Encoding(ctx, (pdf_font *)font, tmp, (pdf_obj *)spdffont->Encoding, (pdf_obj **) &font->Encoding);
            if (code >= 0)
                code = 1;
        }
        else
            code = gs_error_undefined;
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
pdfi_free_font_cff(pdf_obj *font)
{
    pdf_font_cff *pdfontcff = (pdf_font_cff *) font;

    gs_free_object(OBJ_MEMORY(font), pdfontcff->pfont, "pdfi_free_font_cff(pfont)");

    pdfi_countdown(pdfontcff->PDF_font);
    pdfi_countdown(pdfontcff->BaseFont);
    pdfi_countdown(pdfontcff->Name);
    pdfi_countdown(pdfontcff->FontDescriptor);
    pdfi_countdown(pdfontcff->CharStrings);
    pdfi_countdown(pdfontcff->Subrs);
    pdfi_countdown(pdfontcff->GlobalSubrs);
    pdfi_countdown(pdfontcff->Encoding);
    pdfi_countdown(pdfontcff->ToUnicode);
    pdfi_countdown(pdfontcff->filename);

    gs_free_object(OBJ_MEMORY(font), pdfontcff->Widths, "Type 2 fontWidths");
    gs_free_object(OBJ_MEMORY(font), pdfontcff, "pdfi_free_font_cff(pbfont)");

    return 0;
}

int
pdfi_free_font_cidtype0(pdf_obj *font)
{
    pdf_cidfont_type0 *pdfont0 = (pdf_cidfont_type0 *) font;
    gs_font_cid0 *pfont = (gs_font_cid0 *) pdfont0->pfont;

    /* Only have to free the FDArray memory here. Each gs_font in the array is
       referenced by a pdfi font, reference by pdfont0->FDArray. Freeing that
       array will free each pdfi font, freeing the pdfi font will free the gs_font.
       gs_fonts are not reference counted
     */
    gs_free_object(OBJ_MEMORY(font), pfont->cidata.FDArray, "pdfi_free_font_cidtype0(pfont->fdarray)");
    gs_free_object(OBJ_MEMORY(font), pdfont0->pfont, "pdfi_free_font_cff(pfont)");

    pdfi_countdown(pdfont0->PDF_font);
    pdfi_countdown(pdfont0->BaseFont);
    pdfi_countdown(pdfont0->FontDescriptor);
    pdfi_countdown(pdfont0->CharStrings);
    pdfi_countdown(pdfont0->Subrs);
    pdfi_countdown(pdfont0->GlobalSubrs);
    pdfi_countdown(pdfont0->CIDSystemInfo);
    pdfi_countdown(pdfont0->W);
    pdfi_countdown(pdfont0->DW2);
    pdfi_countdown(pdfont0->W2);
    pdfi_countdown(pdfont0->FDArray);
    pdfi_countdown(pdfont0->registry);
    pdfi_countdown(pdfont0->ordering);
    pdfi_countdown(pdfont0->cidtogidmap);
    pdfi_countdown(pdfont0->filename);

    gs_free_object(OBJ_MEMORY(font), pdfont0, "pdfi_free_font_cff(pbfont)");

    return 0;
}
