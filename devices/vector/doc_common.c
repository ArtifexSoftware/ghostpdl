#include "doc_common.h"

#include "gxfont.h"
#include "gserrors.h"
#include "gxfcid.h"
#include "gsagl.h"
#include "gxdevcli.h"
#include "gxgstate.h"

#include <string.h>


extern single_glyph_list_t SingleGlyphList[];
extern double_glyph_list_t DoubleGlyphList[];
extern treble_glyph_list_t TrebleGlyphList[];
extern quad_glyph_list_t QuadGlyphList[];


static int
font_orig_matrix(const gs_font *font, gs_glyph cid, gs_matrix *pmat)
{
    int code;

    switch (font->FontType) {
    case ft_composite:                /* subfonts have their own FontMatrix */
    case ft_TrueType:
    case ft_CID_TrueType:
        /* The TrueType FontMatrix is 1 unit per em, which is what we want. */
        gs_make_identity(pmat);
        return 0;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_CID_encrypted:
    case ft_user_defined:
    case ft_PCL_user_defined:
    case ft_GL2_stick_user_defined:
    case ft_GL2_531:
        /*
         * Type 1 fonts are supposed to use a standard FontMatrix of
         * [0.001 0 0 0.001 0 0], with a 1000-unit cell.  However,
         * Windows NT 4.0 creates Type 1 fonts, apparently derived from
         * TrueType fonts, that use a 2048-unit cell and corresponding
         * FontMatrix.  Also, some PS programs perform font scaling by
         * replacing FontMatrix like this :
         *
         *   /f12 /Times-Roman findfont
         *   copyfont          % (remove FID)
         *   dup /FontMatrix [0.012 0 0 0.012 0 0] put
         *   definefont
         *   /f12 1 selectfont
         *
         * Such fonts are their own "base font", but the orig_matrix
         * must still be set to 0.001, not 0.012 .
         *
         * The old code used a heuristic to detect and correct for this here.
         * Unfortunately it doesn't work properly when it meets a font
         * with FontMatrix like this :
         *
         *   /FontMatrix [1 2288 div 0 0 1 2288 div 0 0 ] def
         *
         * (the bug 686970). Also comparefiles\455690.pdf appears to
         * have similar problem. Therefore we added a support to lib/gs_fonts.ps,
         * src/zbfont.c, src/gsfont.c that provides an acces to the original
         * font via a special key OrigFont added to the font dictionary while definefont.
         * Now we work through this access with PS interpreter,
         * but keep the old heuristic for other clients.
         */
        {
            const gs_font *base_font = font;

            while (base_font->base != base_font)
                base_font = base_font->base;
            if (font->FontType == ft_user_defined ||
                font->FontType == ft_PCL_user_defined ||
                font->FontType == ft_GL2_stick_user_defined ||
                font->FontType == ft_GL2_531)
                *pmat = base_font->FontMatrix;
            else if (base_font->orig_FontMatrix.xx != 0 || base_font->orig_FontMatrix.xy != 0 ||
                base_font->orig_FontMatrix.yx != 0 || base_font->orig_FontMatrix.yy != 0)
                *pmat = base_font->orig_FontMatrix;
            else {
                /*  Must not happen with PS interpreter.
                    Provide a hewuristic for other clients.
                */
                if (base_font->FontMatrix.xx == 1.0/2048 &&
                    base_font->FontMatrix.xy == 0 &&
                    base_font->FontMatrix.yx == 0 &&
                    any_abs(base_font->FontMatrix.yy) == 1.0/2048
                    )
                    *pmat = base_font->FontMatrix;
                else
                    gs_make_scaling(0.001, 0.001, pmat);
            }
        }
        if (font->FontType == ft_CID_encrypted && cid != -1) {
            int fidx;

            if (cid < GS_MIN_CID_GLYPH)
                cid = GS_MIN_CID_GLYPH;
            code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                cid, NULL, &fidx);
            if (code < 0) {
                code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                (gs_glyph)GS_MIN_CID_GLYPH, NULL, &fidx);
            }
            if (code >= 0) {
                gs_matrix_multiply(&(gs_cid0_indexed_font(font, fidx)->FontMatrix),
                                pmat, pmat);
            }
        }
        return 0;
    default:
        return_error(gs_error_rangecheck);
    }
}

/*
 * Special version of font_orig_matrix(), that considers FDArray font's FontMatrix too.
 * Called only by txt_glyph_width().
 * 'cid' is only consulted if 'font' is a CIDFontType 0 CID font.
 */
static int
glyph_orig_matrix(const gs_font *font, gs_glyph cid, gs_matrix *pmat)
{
    int code = font_orig_matrix(font, cid, pmat);
    if (code >= 0) {
        if (font->FontType == ft_CID_encrypted) {
            int fidx;

            if (cid < GS_MIN_CID_GLYPH)
                cid = GS_MIN_CID_GLYPH;
            code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                cid, NULL, &fidx);
            if (code < 0) {
                code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                (gs_glyph)GS_MIN_CID_GLYPH, NULL, &fidx);
            }
            if (code >= 0) {
                gs_matrix_multiply(&(gs_cid0_indexed_font(font, fidx)->FontMatrix),
                                pmat, pmat);
            }
        }
    }
    return code;
}

float txt_calculate_text_size(gs_gstate *pgs, gs_font *ofont,
                              const gs_matrix *pfmat, gs_matrix *smat, gs_matrix *tmat,
                              gs_font *font, gx_device *pdev)
{
    gs_matrix orig_matrix;
    double
        sx = pdev->HWResolution[0] / 72.0,
        sy = pdev->HWResolution[1] / 72.0;
    float size;

    /* Get the original matrix of the base font. */

    font_orig_matrix(ofont, -1, &orig_matrix);
    /* Compute the scaling matrix and combined matrix. */

    if (gs_matrix_invert(&orig_matrix, smat) < 0) {
        gs_make_identity(smat);
        return 1; /* Arbitrary */
    }
    gs_matrix_multiply(smat, pfmat, smat);
    *tmat = ctm_only(pgs);
    tmat->tx = tmat->ty = 0;
    gs_matrix_multiply(smat, tmat, tmat);

    /* Try to find a reasonable size value. */

    size = hypot(tmat->yx, tmat->yy) / sy;
    if (size < 0.01)
        size = hypot(tmat->xx, tmat->xy) / sx;
    if (size < 0.01)
        size = 1;

    return(size);
}

static int
store_glyph_width(txt_glyph_width_t *pwidth, int wmode, const gs_matrix *scale,
                  const gs_glyph_info_t *pinfo)
{
    double w, v;

    gs_distance_transform(pinfo->width[wmode].x, pinfo->width[wmode].y, scale, &pwidth->xy);
    if (wmode)
        w = pwidth->xy.y, v = pwidth->xy.x;
    else
        w = pwidth->xy.x, v = pwidth->xy.y;
    if (v != 0)
        return 1;
    pwidth->w = w;
    gs_distance_transform(pinfo->v.x, pinfo->v.y, scale, &pwidth->v);
    return 0;
}

static int
get_missing_width(gs_font *font, int wmode, const gs_matrix *scale_c,
                    txt_glyph_widths_t *pwidths)
{
    gs_font_info_t finfo;
    int code;

    code = font->procs.font_info((gs_font *)font, NULL,
                                  FONT_INFO_MISSING_WIDTH, &finfo);
    if (code < 0)
        return code;
    if (!(finfo.members & FONT_INFO_MISSING_WIDTH))
        return_error(gs_error_undefined);

    if (wmode) {
        gs_distance_transform(0.0, -finfo.MissingWidth, scale_c, &pwidths->real_width.xy);
        pwidths->Width.xy.x = 0;
        pwidths->Width.xy.y = pwidths->real_width.xy.y;
        pwidths->Width.w = pwidths->real_width.w =
                pwidths->Width.xy.y;
        pwidths->Width.v.x = - pwidths->Width.xy.y / 2;
        pwidths->Width.v.y = - pwidths->Width.xy.y;
    } else {
        gs_distance_transform(finfo.MissingWidth, 0.0, scale_c, &pwidths->real_width.xy);
        pwidths->Width.xy.x = pwidths->real_width.xy.x;
        pwidths->Width.xy.y = 0;
        pwidths->Width.w = pwidths->real_width.w =
                pwidths->Width.xy.x;
        pwidths->Width.v.x = pwidths->Width.v.y = 0;
    }
    /*
     * Don't mark the width as known, just in case this is an
     * incrementally defined font.
     */
    return 1;
}

/*
 * Get the widths (unmodified from the copied font,
 * and possibly modified from the original font) of a given glyph.
 * Return 1 if the width was defaulted to MissingWidth.
 * Return TEXT_PROCESS_CDEVPROC if a CDevProc callout is needed.
 * cdevproc_result != NULL if we restart after a CDevProc callout.
 */
int
txt_glyph_widths(gs_font *font, int wmode, gs_glyph glyph,
                 gs_font *orig_font, txt_glyph_widths_t *pwidths,
                 const double cdevproc_result[10])
{
    gs_font *ofont = orig_font;
    gs_glyph_info_t info;
    gs_matrix scale_c, scale_o;
    int code, rcode = 0;
    gs_point v;
    int allow_cdevproc_callout = (orig_font->FontType == ft_CID_TrueType
                || orig_font->FontType == ft_CID_encrypted
                ? GLYPH_INFO_CDEVPROC : 0); /* fixme : allow more font types. */

    if (ofont->FontType == ft_composite)
        return_error(gs_error_unregistered); /* Must not happen. */
    code = glyph_orig_matrix((const gs_font *)font, glyph, &scale_c);
    if (code < 0)
        return code;
    code = glyph_orig_matrix(ofont, glyph, &scale_o);
    if (code < 0)
        return code;
    gs_matrix_scale(&scale_c, 1000.0, 1000.0, &scale_c);
    gs_matrix_scale(&scale_o, 1000.0, 1000.0, &scale_o);
    pwidths->Width.v.x = pwidths->Width.v.y = pwidths->Width.xy.x = pwidths->Width.xy.y = 0;
    pwidths->real_width.v.x = pwidths->real_width.v.y = pwidths->real_width.xy.x = pwidths->real_width.xy.y = 0;
    pwidths->Width.w = pwidths->real_width.w = 0.0;
    pwidths->replaced_v = false;
    if (glyph == GS_NO_GLYPH)
        return get_missing_width(font, wmode, &scale_c, pwidths);
    code = font->procs.glyph_info((gs_font *)font, glyph, NULL,
                                    GLYPH_INFO_WIDTH0 |
                                    (GLYPH_INFO_WIDTH0 << wmode) |
                                    GLYPH_INFO_OUTLINE_WIDTHS |
                                    (GLYPH_INFO_VVECTOR0 << wmode),
                                    &info);
    /* For CID fonts the PDF spec requires the x-component of v-vector
       to be equal to half glyph width, and AR5 takes it from W, DW.
       So make a compatibe data here.
     */
    if (font->FontType != ft_PCL_user_defined && font->FontType != ft_GL2_stick_user_defined &&
        font->FontType != ft_GL2_531
        && (code == gs_error_undefined || !(info.members & (GLYPH_INFO_WIDTH0 << wmode)))) {
        code = get_missing_width(font, wmode, &scale_c, pwidths);
        if (code < 0)
            return code;
        else
            v.y = pwidths->Width.v.y;
        if (wmode && (ofont->FontType == ft_CID_encrypted ||
            ofont->FontType == ft_CID_TrueType)) {
            txt_glyph_widths_t widths1;

            if (get_missing_width(font, 0, &scale_c, &widths1) < 0)
                v.x = 0;
            else
                v.x = widths1.Width.w / 2;
        } else
            v.x = pwidths->Width.v.x;
    } else if (code < 0)
        return code;
    else {
        code = store_glyph_width(&pwidths->Width, wmode, &scale_c, &info);
        if (code < 0)
            return code;
        rcode |= code;
        if (info.members  & (GLYPH_INFO_VVECTOR0 << wmode))
            gs_distance_transform(info.v.x, info.v.y, &scale_c, &v);
        else
            v.x = v.y = 0;
        if (wmode && (ofont->FontType == ft_CID_encrypted ||
            ofont->FontType == ft_CID_TrueType)) {
            if (info.members & (GLYPH_INFO_WIDTH0 << wmode)) {
                gs_point xy;

                gs_distance_transform(info.width[0].x, info.width[0].y, &scale_c, &xy);
                v.x = xy.x / 2;
            } else {
                txt_glyph_widths_t widths1;

                if (get_missing_width(font, 0, &scale_c, &widths1) < 0)
                    v.x = 0;
                else
                    v.x = widths1.Width.w / 2;
            }
        }
    }
    pwidths->Width.v = v;
    /* Skip only if not paralel to the axis. */
    if (code > 0 && ofont->FontType != ft_CID_encrypted &&
            ofont->FontType != ft_CID_TrueType)
        pwidths->Width.xy.x = pwidths->Width.xy.y = pwidths->Width.w = 0;
    if (cdevproc_result == NULL) {
        info.members = 0;
        code = ofont->procs.glyph_info(ofont, glyph, NULL,
                                            (GLYPH_INFO_WIDTH0 << wmode) |
                                            (GLYPH_INFO_VVECTOR0 << wmode) |
                                            allow_cdevproc_callout,
                                            &info);
        /* fixme : Move this call before cfont->procs.glyph_info. */
        if (info.members & GLYPH_INFO_CDEVPROC) {
            if (allow_cdevproc_callout)
                return TEXT_PROCESS_CDEVPROC;
        else
            return_error(gs_error_rangecheck);
        }
    } else {
        info.width[0].x = cdevproc_result[0];
        info.width[0].y = cdevproc_result[1];
        info.width[1].x = cdevproc_result[6];
        info.width[1].y = cdevproc_result[7];
        info.v.x = (wmode ? cdevproc_result[8] : 0);
        info.v.y = (wmode ? cdevproc_result[9] : 0);
        info.members = (GLYPH_INFO_WIDTH0 << wmode) |
                       (wmode ? GLYPH_INFO_VVECTOR1 : 0);
        code = 0;
    }
    if (code == gs_error_undefined || !(info.members & (GLYPH_INFO_WIDTH0 << wmode)))
        pwidths->real_width = pwidths->Width;
    else if (code < 0)
        return code;
    else {
        if ((info.members & (GLYPH_INFO_VVECTOR0 | GLYPH_INFO_VVECTOR1)) != 0)
            pwidths->replaced_v = true;
        else
            info.v.x = info.v.y = 0;
        code = store_glyph_width(&pwidths->real_width, wmode, &scale_o, &info);
        if (code < 0)
            return code;
        rcode |= code;
        gs_distance_transform(info.v.x, info.v.y, &scale_o, &pwidths->real_width.v);
    }
    return rcode;
}

void
txt_char_widths_to_uts(gs_font *font /* may be NULL for non-Type3 */,
                       txt_glyph_widths_t *pwidths)
{
    if (font && (font->FontType == ft_user_defined ||
        font->FontType == ft_PCL_user_defined ||
        font->FontType == ft_GL2_stick_user_defined ||
        font->FontType == ft_GL2_531)) {
        gs_matrix *pmat = &font->orig_FontMatrix;

        pwidths->Width.xy.x *= pmat->xx; /* formula simplified based on wy in glyph space == 0 */
        pwidths->Width.xy.y  = 0.0; /* WMode == 0 for PDF Type 3 fonts */
        gs_distance_transform(pwidths->real_width.xy.x, pwidths->real_width.xy.y, pmat, &pwidths->real_width.xy);
    } else {
        /*
         * For other font types:
         * - PDF design->text space is a simple scaling by 0.001.
         * - The Width.xy.x/y that should be zeroed-out per 5.3.3 "Text Space Details" is already 0.
         */
        pwidths->Width.xy.x /= 1000.0;
        pwidths->Width.xy.y /= 1000.0;
        pwidths->real_width.xy.x /= 1000.0;
        pwidths->real_width.xy.y /= 1000.0;
    }
}


int txt_get_unicode(gx_device *dev, gs_font *font, gs_glyph glyph, gs_char ch, unsigned short *Buffer)
{
    int code;
    gs_const_string gnstr;
    unsigned short fallback = ch;
    ushort *unicode = NULL;
    int length;

    length = font->procs.decode_glyph((gs_font *)font, glyph, ch, NULL, 0);
    if (length == 0) {
        if (glyph != GS_NO_GLYPH) {
            code = font->procs.glyph_name(font, glyph, &gnstr);
            if (code >= 0 && gnstr.size == 7) {
                if (!memcmp(gnstr.data, "uni", 3)) {
                    static const char *hexdigits = "0123456789ABCDEF";
                    char *d0 = strchr(hexdigits, gnstr.data[3]);
                    char *d1 = strchr(hexdigits, gnstr.data[4]);
                    char *d2 = strchr(hexdigits, gnstr.data[5]);
                    char *d3 = strchr(hexdigits, gnstr.data[6]);

                    if (d0 != NULL && d1 != NULL && d2 != NULL && d3 != NULL) {
                        *Buffer++ = ((d0 - hexdigits) << 12) + ((d1 - hexdigits) << 8) + ((d2 - hexdigits) << 4) + (d3 - hexdigits);
                        return 1;
                    }
                }
            }
            if (length == 0) {
                single_glyph_list_t *sentry = SingleGlyphList;
                double_glyph_list_t *dentry = DoubleGlyphList;
                treble_glyph_list_t *tentry = TrebleGlyphList;
                quad_glyph_list_t *qentry = QuadGlyphList;

                /* Search glyph to single Unicode value table */
                while (sentry->Glyph != 0) {
                    if (sentry->Glyph[0] < gnstr.data[0]) {
                        sentry++;
                        continue;
                    }
                    if (sentry->Glyph[0] > gnstr.data[0]){
                        break;
                    }
                    if (strlen(sentry->Glyph) == gnstr.size) {
                        if(memcmp(gnstr.data, sentry->Glyph, gnstr.size) == 0) {
                            *Buffer = sentry->Unicode;
                            return 1;
                        }
                    }
                    sentry++;
                }

                /* Search glyph to double Unicode value table */
                while (dentry->Glyph != 0) {
                    if (dentry->Glyph[0] < gnstr.data[0]) {
                        dentry++;
                        continue;
                    }
                    if (dentry->Glyph[0] > gnstr.data[0]){
                        break;
                    }
                    if (strlen(dentry->Glyph) == gnstr.size) {
                        if(memcmp(gnstr.data, dentry->Glyph, gnstr.size) == 0) {
                            memcpy(Buffer, dentry->Unicode, 2);
                            return 2;
                        }
                    }
                    dentry++;
                }

                /* Search glyph to triple Unicode value table */
                while (tentry->Glyph != 0) {
                    if (tentry->Glyph[0] < gnstr.data[0]) {
                        tentry++;
                        continue;
                    }
                    if (tentry->Glyph[0] > gnstr.data[0]){
                        break;
                    }
                    if (strlen(tentry->Glyph) == gnstr.size) {
                        if(memcmp(gnstr.data, tentry->Glyph, gnstr.size) == 0) {
                            memcpy(Buffer, tentry->Unicode, 3);
                            return 3;
                        }
                    }
                    tentry++;
                }

                /* Search glyph to quadruple Unicode value table */
                while (qentry->Glyph != 0) {
                    if (qentry->Glyph[0] < gnstr.data[0]) {
                        qentry++;
                        continue;
                    }
                    if (qentry->Glyph[0] > gnstr.data[0]){
                        break;
                    }
                    if (strlen(qentry->Glyph) == gnstr.size) {
                        if(memcmp(gnstr.data, qentry->Glyph, gnstr.size) == 0) {
                            memcpy(Buffer, qentry->Unicode, 4);
                            return 4;
                        }
                    }
                    qentry++;
                }
            }
        }
        *Buffer = fallback;
        return 1;
    } else {
        char *b, *u;
        int l = length - 1;

        unicode = (ushort *)gs_alloc_bytes(dev->memory, length, "temporary Unicode array");
        length = font->procs.decode_glyph((gs_font *)font, glyph, ch, unicode, length);
#if ARCH_IS_BIG_ENDIAN
        memcpy(Buffer, unicode, length);
#else
        b = (char *)Buffer;
        u = (char *)unicode;

        for (l=0;l<length;l+=2, u+=2){
            *b++ = *(u+1);
            *b++ = *u;
        }
#endif
        gs_free_object(dev->memory, unicode, "free temporary unicode buffer");
        return length / sizeof(short);
    }
}
