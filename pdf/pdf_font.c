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

/* Font operations for the PDF interpreter */

#include "pdf_int.h"
#include "pdf_dict.h"
#include "pdf_loop_detect.h"
#include "pdf_array.h"
#include "pdf_font.h"
#include "pdf_stack.h"
#include "pdf_misc.h"
#include "pdf_font_types.h"
#include "pdf_font0.h"
#include "pdf_font1.h"
#include "pdf_font1C.h"
#include "pdf_font3.h"
#include "pdf_fontTT.h"
#include "gscencs.h"            /* For gs_c_known_encode and gs_c_glyph_name */


int pdfi_d0(pdf_context *ctx)
{
    int code = 0, gsave_level = 0;
    double width[2];

    if (ctx->inside_CharProc == false)
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
    if(ctx->current_text_enum == NULL) {
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

    code = gs_text_setcharwidth(ctx->current_text_enum, width);

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

    if (ctx->inside_CharProc == false)
        ctx->pdf_warnings |= W_PDF_NOTINCHARPROC;

    ctx->CharProc_is_d1 = true;

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

    code = gs_text_setcachedevice(ctx->current_text_enum, wbox);

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

static int pdfi_gs_setfont(pdf_context *ctx, gs_font *pfont)
{
    int code = 0;
    pdf_font *old_font = pdfi_get_current_pdf_font(ctx);

    code = gs_setfont(ctx->pgs, pfont);
    if (code >= 0)
        pdfi_countdown(old_font);

    return code;
}

int pdfi_Tf(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    double point_size = 0;
    pdf_obj *o = NULL;
    int code = 0;
    pdf_dict *font_dict = NULL;
    gs_font *pfont = NULL;

    if (pdfi_count_stack(ctx) >= 2) {
        o = ctx->stack_top[-1];
        if (o->type == PDF_INT)
            point_size = (double)((pdf_num *)o)->value.i;
        else {
            if (o->type == PDF_REAL)
                point_size = ((pdf_num *)o)->value.d;
            else {
                code = gs_note_error(gs_error_typecheck);
                goto Tf_error;
            }
        }
        code = gs_setPDFfontsize(ctx->pgs, point_size);
        if (code < 0)
            goto Tf_error;

        o = ctx->stack_top[-2];
        if (o->type != PDF_NAME) {
            code = gs_note_error(gs_error_typecheck);
            goto Tf_error;
        }

        code = pdfi_loop_detector_mark(ctx);
        if (code < 0)
            goto Tf_error;

        code = pdfi_find_resource(ctx, (unsigned char *)"Font", (pdf_name *)o, stream_dict, page_dict, (pdf_obj **)&font_dict);
        (void)pdfi_loop_detector_cleartomark(ctx);
        if (code < 0)
            goto Tf_error;

        o = NULL;

        if (font_dict->type != PDF_DICT) {
            pdf_font *font = (pdf_font *)font_dict;

            if (font_dict->type != PDF_FONT) {
                code = gs_note_error(gs_error_typecheck);
                goto Tf_error;
            }
            /* Don't swap fonts if this is already the current font */
            if (font->pfont != (gs_font_base *)ctx->pgs->font) {
                code = pdfi_gs_setfont(ctx, (gs_font *)font->pfont);
            }
            else
                pdfi_countdown(font_dict);
            pdfi_pop(ctx, 2);
            return code;
        }

        code = pdfi_dict_knownget_type(ctx, font_dict, "Type", PDF_NAME, &o);
        if (code < 0)
            goto Tf_error_o;
        if (code == 0) {
            code = gs_note_error(gs_error_undefined);
            goto Tf_error_o;
        }
        if (!pdfi_name_is((const pdf_name *)o, "Font")){
            code = gs_note_error(gs_error_typecheck);
            goto Tf_error_o;
        }
        pdfi_countdown(o);
        o = NULL;

        code = pdfi_dict_knownget_type(ctx, font_dict, "Subtype", PDF_NAME, &o);
        if (code < 0)
            goto Tf_error_o;

        if (pdfi_name_is((const pdf_name *)o, "Type0")) {
            code = pdfi_read_type0_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &pfont);
            if (code < 0)
                goto Tf_error_o;
        } else {
            if (pdfi_name_is((const pdf_name *)o, "Type1")) {
                code = pdfi_read_type1_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &pfont);
                if (code < 0)
                    goto Tf_error_o;
            } else {
                if (pdfi_name_is((const pdf_name *)o, "Type1C")) {
                    code = pdfi_read_type1C_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &pfont);
                    if (code < 0)
                        goto Tf_error_o;
                } else {
                    if (pdfi_name_is((const pdf_name *)o, "Type3")) {
                        code = pdfi_read_type3_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &pfont);
                        if (code < 0)
                            goto Tf_error_o;
                    } else {
                        if (pdfi_name_is((const pdf_name *)o, "TrueType")) {
                            code = pdfi_read_truetype_font(ctx, (pdf_dict *)font_dict, stream_dict, page_dict, &pfont);
                        } else {
                            code = gs_note_error(gs_error_undefined);
                                goto Tf_error_o;
                        }
                    }
                }
            }
        }

        code = pdfi_gs_setfont(ctx, pfont);
        if (code < 0)
            goto Tf_error_o;

        pdfi_countdown(o);

        pdfi_countdown(font_dict);

        pdfi_pop(ctx, 2);
    }
    else {
        pdfi_clearstack(ctx);
        code = gs_note_error(gs_error_typecheck);
        goto Tf_error;
    }
    return 0;

Tf_error_o:
    pdfi_countdown(o);
Tf_error:
    code = pdfi_gs_setfont(ctx, NULL);
    pdfi_countdown(font_dict);
    pdfi_clearstack(ctx);
    return code;
}

int pdfi_free_font(pdf_obj *font)
{
    pdf_font *f = (pdf_font *)font;

    switch (f->pdfi_font_type) {
        case e_pdf_font_type0:
        case e_pdf_font_type1:
        case e_pdf_font_cff:
        case e_pdf_font_type3:
            return pdfi_free_font_type3((pdf_obj *)font);
            break;
        case e_pdf_font_truetype:
            return pdfi_free_font_truetype((pdf_obj *)font);
            break;
        case e_pdf_cidfont_type0:
        case e_pdf_cidfont_type1:
        case e_pdf_cidfont_type2:
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
        code = pdfi_make_name(ctx, (byte *)str.data, str.size, (pdf_obj **)&n);
        if (code < 0)
            return code;
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
 * create an array of glyph names as above. We then get the Differences array from the
 * dictionary and use that to refine the Encoding.
 */
int pdfi_create_Encoding(pdf_context *ctx, pdf_obj *pdf_Encoding, pdf_obj **Encoding)
{
    int code = 0, i;

    code = pdfi_alloc_object(ctx, PDF_ARRAY, 256, Encoding);
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

            code = pdfi_dict_get(ctx, (pdf_dict *)pdf_Encoding, "BaseEncoding", (pdf_obj **)&n);
            if (code < 0) {
                code = pdfi_make_name(ctx, (byte *)"StandardEncoding", 16, (pdf_obj **)&n);
                if (code < 0)
                    return code;
            }
            code = pdfi_build_Encoding(ctx, n, (pdf_array *)*Encoding);
            if (code < 0) {
                pdfi_countdown(n);
                    return code;
            }
            pdfi_countdown(n);
            code = pdfi_dict_knownget_type(ctx, (pdf_dict *)pdf_Encoding, "Differences", PDF_ARRAY, (pdf_obj **)&a);
            if (code <= 0)
                return code;
            for (i=0;i < pdfi_array_size(a);i++) {
                code = pdfi_array_get(ctx, a, (uint64_t)i, &o);
                if (code < 0)
                    break;
                if (o->type == PDF_NAME) {
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
    if (font->Encoding != NULL)
        code = pdfi_array_get(font->ctx, font->Encoding, (uint64_t)glyph, (pdf_obj **)&GlyphName);
    if (code < 0 && !font->fake_glyph_names)
        return code;

    /* For the benefit of the vector devices, if a glyph index is outside the encoding, we create a fake name */
    if (GlyphName == NULL) {
        int i;
        char cid_name[5 + sizeof(gs_glyph) * 3 + 1];
        for (i = 0; i < font->LastChar; i++)
            if (font->fake_glyph_names[i].data == NULL) break;

         if (i == font->LastChar) return_error(gs_error_invalidfont);

         gs_sprintf(cid_name, "glyph%lu", (ulong) glyph);

         pstr->data = font->fake_glyph_names[i].data =
                       gs_alloc_bytes(font->memory, strlen(cid_name) + 1, "pdfi_glyph_name: fake name");
         if (font->fake_glyph_names[i].data == NULL)
             return_error(gs_error_VMerror);
         pstr->size = font->fake_glyph_names[i].size = strlen(cid_name);
         memcpy(font->fake_glyph_names[i].data, cid_name, strlen(cid_name) + 1);
         return 0;
    }

    code = pdfi_get_name_index(font->ctx, (char *)GlyphName->data, GlyphName->length, &index);
    if (code < 0)
        return code;

    code = pdfi_name_from_index(font->ctx, index, (unsigned char **)&pstr->data, &pstr->size);
    return code;

    pstr->data = GlyphName->data;
    pstr->size = GlyphName->length;
    pdfi_countdown(GlyphName);
    return 0;
}