/* Copyright (C) 2019-2021 Artifex Software, Inc.
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

/* code for type 3 font handling */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_gstate.h"
#include "pdf_font.h"
#include "pdf_font3.h"
#include "pdf_font_types.h"
#include "pdf_deref.h"
#include "gscencs.h"
#include "gscedata.h"       /* For the encoding arrays */
#include "gsccode.h"        /* For the Encoding indices */
#include "gsuid.h"          /* For no_UniqueID */

static int
pdfi_type3_build_char(gs_show_enum * penum, gs_gstate * pgs, gs_font * pfont,
                     gs_char chr, gs_glyph glyph)
{
    int code = 0;
    pdf_font_type3 *font;
    pdf_name *GlyphName = NULL;
    pdf_stream *CharProc = NULL;
    int SavedTextBlockDepth = 0;
    char Notdef[8] = {".notdef"};

    font = (pdf_font_type3 *)pfont->client_data;

    SavedTextBlockDepth = OBJ_CTX(font)->text.BlockDepth;
    code = pdfi_array_get(OBJ_CTX(font), font->Encoding, (uint64_t)chr, (pdf_obj **)&GlyphName);
    if (code < 0)
        return code;

    code = pdfi_dict_get_by_key(OBJ_CTX(font), font->CharProcs, GlyphName, (pdf_obj **)&CharProc);
    if (code == gs_error_undefined) {
        byte *Key = NULL;
        /* Can't find the named glyph, try to find a /.notdef as a substitute */
        Key = gs_alloc_bytes(OBJ_MEMORY(font), 8, "working buffer for BuildChar");
        if (Key == NULL)
            goto build_char_error;
        memset(Key, 0x00, 8);
        memcpy(Key, Notdef, 8);
        code = pdfi_dict_get(OBJ_CTX(font), font->CharProcs, (const char *)Key, (pdf_obj **)&CharProc);
        gs_free_object(OBJ_MEMORY(font), Key, "working buffer for BuildChar");
        if (code == gs_error_undefined) {
            code = 0;
            goto build_char_error;
        }
    }
    if (code < 0)
        goto build_char_error;
    if (CharProc->type != PDF_STREAM) {
        code = gs_note_error(gs_error_typecheck);
        goto build_char_error;
    }

    OBJ_CTX(font)->text.BlockDepth = 0;
    OBJ_CTX(font)->text.inside_CharProc = true;
    OBJ_CTX(font)->text.CharProc_is_d1 = false;
    pdfi_gsave(OBJ_CTX(font));
    pdfi_run_context(OBJ_CTX(font), CharProc, font->PDF_font, true, "CharProc");
    pdfi_grestore(OBJ_CTX(font));
    OBJ_CTX(font)->text.inside_CharProc = false;
    OBJ_CTX(font)->text.CharProc_is_d1 = false;
    OBJ_CTX(font)->text.BlockDepth = SavedTextBlockDepth;

build_char_error:
    pdfi_countdown(GlyphName);
    pdfi_countdown(CharProc);
    return code;
}

static int alloc_type3_font(pdf_context *ctx, pdf_font_type3 **font)
{
    pdf_font_type3 *t3font = NULL;

    t3font = (pdf_font_type3 *)gs_alloc_bytes(ctx->memory, sizeof(pdf_font_type3), "pdfi_alloc_type3_font");
    if (t3font == NULL)
        return_error(gs_error_VMerror);

    memset(t3font, 0x00, sizeof(pdf_font_type3));
    (t3font)->ctx = ctx;
    (t3font)->type = PDF_FONT;

#if REFCNT_DEBUG
    (t3font)->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %"PRIi64"\n", t3font->type, t3font->UID);
#endif


    pdfi_countup(t3font);

    t3font->pfont = gs_alloc_struct(ctx->memory, gs_font_base, &st_gs_font_base,
                            "pdfi (type 3 font)");
    if (t3font->pfont == NULL) {
        pdfi_countdown(t3font);
        return_error(gs_error_VMerror);
    }

    memset(t3font->pfont, 0x00, sizeof(gs_font_base));
    t3font->ctx = ctx;
    t3font->pdfi_font_type = e_pdf_font_type3;

    gs_make_identity(&t3font->pfont->orig_FontMatrix);
    gs_make_identity(&t3font->pfont->FontMatrix);
    t3font->pfont->next = t3font->pfont->prev = 0;
    t3font->pfont->memory = ctx->memory;
    t3font->pfont->dir = ctx->font_dir;
    t3font->pfont->is_resource = false;
    gs_notify_init(&t3font->pfont->notify_list, ctx->memory);
    t3font->pfont->base = (gs_font *) t3font->pfont;
    t3font->pfont->client_data = t3font;
    t3font->pfont->WMode = 0;
    t3font->pfont->PaintType = 0;
    t3font->pfont->StrokeWidth = 0;
    t3font->pfont->is_cached = 0;
    t3font->pfont->procs.init_fstack = gs_default_init_fstack;
    t3font->pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    t3font->pfont->FAPI = NULL;
    t3font->pfont->FAPI_font_data = NULL;
    t3font->pfont->procs.glyph_name = pdfi_glyph_name;
    t3font->pfont->procs.decode_glyph = pdfi_decode_glyph;
    t3font->pfont->procs.define_font = gs_no_define_font;
    t3font->pfont->procs.make_font = gs_no_make_font;
    t3font->pfont->procs.font_info = gs_default_font_info;
    t3font->pfont->procs.glyph_info = gs_default_glyph_info;
    t3font->pfont->procs.glyph_outline = gs_no_glyph_outline;
    t3font->pfont->procs.encode_char = pdfi_encode_char;
    t3font->pfont->procs.build_char = pdfi_type3_build_char;
    t3font->pfont->procs.same_font = gs_default_same_font;
    t3font->pfont->procs.enumerate_glyph = gs_no_enumerate_glyph;

    t3font->pfont->FontType = ft_PDF_user_defined;
    /* outlines ? Bitmaps ? */
    t3font->pfont->ExactSize = fbit_use_bitmaps;
    t3font->pfont->InBetweenSize = fbit_use_bitmaps;
    t3font->pfont->TransformedChar = fbit_transform_bitmaps;

    t3font->pfont->encoding_index = 1;          /****** WRONG ******/
    t3font->pfont->nearest_encoding_index = 1;          /****** WRONG ******/

    t3font->pfont->client_data = (void *)t3font;
    t3font->pfont->id = gs_next_ids(ctx->memory, 1);
    uid_set_UniqueID(&t3font->pfont->UID, no_UniqueID);

    *font = (pdf_font_type3 *)t3font;
    return 0;
}

int pdfi_free_font_type3(pdf_obj *font)
{
    pdf_font_type3 *t3font = (pdf_font_type3 *)font;

    if (t3font->pfont)
        gs_free_object(OBJ_MEMORY(t3font), t3font->pfont, "Free type 3 font");

    if (t3font->Widths)
        gs_free_object(OBJ_MEMORY(t3font), t3font->Widths, "Free type 3 font Widths array");

    pdfi_countdown(t3font->PDF_font);
    pdfi_countdown(t3font->FontDescriptor);
    pdfi_countdown(t3font->CharProcs);
    pdfi_countdown(t3font->Encoding);
    gs_free_object(OBJ_MEMORY(font), font, "Free type 3 font");
    return 0;
}


int pdfi_read_type3_font(pdf_context *ctx, pdf_dict *font_dict, pdf_dict *stream_dict, pdf_dict *page_dict, gs_font **ppfont)
{
    int code = 0, i, num_chars = 0;
    pdf_font_type3 *font = NULL;
    pdf_obj *obj = NULL;
    double f;

    *ppfont = NULL;
    code = alloc_type3_font(ctx, &font);
    if (code < 0)
        return code;

    font->object_num = font_dict->object_num;

    code = pdfi_dict_get_type(ctx, font_dict, "FontBBox", PDF_ARRAY, &obj);
    if (code < 0)
        goto font3_error;
    code = pdfi_array_to_gs_rect(ctx, (pdf_array *)obj, &font->pfont->FontBBox);
    if (code < 0)
        goto font3_error;
    pdfi_countdown(obj);
    obj = NULL;

    code = pdfi_dict_get_type(ctx, font_dict, "FontMatrix", PDF_ARRAY, &obj);
    if (code < 0)
        goto font3_error;
    code = pdfi_array_to_gs_matrix(ctx, (pdf_array *)obj, &font->pfont->orig_FontMatrix);
    if (code < 0)
        goto font3_error;
    code = pdfi_array_to_gs_matrix(ctx, (pdf_array *)obj, &font->pfont->FontMatrix);
    if (code < 0)
        goto font3_error;
    pdfi_countdown(obj);
    obj = NULL;

    code = pdfi_dict_get(ctx, font_dict, "CharProcs", (pdf_obj **)&font->CharProcs);
    if (code < 0)
        goto font3_error;

    code = pdfi_dict_get_number(ctx, font_dict, "FirstChar", &f);
    if (code < 0)
        goto font3_error;
    font->FirstChar = (int)f;

    code = pdfi_dict_get_number(ctx, font_dict, "LastChar", &f);
    if (code < 0)
        goto font3_error;
    font->LastChar = (int)f;

    num_chars = (font->LastChar - font->FirstChar) + 1;
    code = pdfi_dict_knownget_type(ctx, font_dict, "FontDescriptor", PDF_DICT, (pdf_obj **)&font->FontDescriptor);
    if (code < 0)
        goto font3_error;

    code = pdfi_dict_knownget_type(ctx, font_dict, "Widths", PDF_ARRAY, (pdf_obj **)&obj);
    if (code < 0)
        goto font3_error;
    if (code > 0) {
        if (num_chars != pdfi_array_size((pdf_array *)obj)) {
            code = gs_note_error(gs_error_rangecheck);
            goto font3_error;
        }

        font->Widths = (double *)gs_alloc_bytes(ctx->memory, sizeof(double) * num_chars, "type 3 font Widths array");
        if (font->Widths == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto font3_error;
        }
        memset(font->Widths, 0x00, sizeof(double) * num_chars);
        for (i = 0; i < num_chars; i++) {
            code = pdfi_array_get_number(ctx, (pdf_array *)obj, (uint64_t)i, &font->Widths[i]);
            if (code < 0)
                goto font3_error;
        }
    }
    pdfi_countdown(obj);
    obj = NULL;

    code = pdfi_dict_get(ctx, font_dict, "Encoding", &obj);
    if (code < 0)
        goto font3_error;

    code = pdfi_create_Encoding(ctx, obj, (pdf_obj **)&font->Encoding);
    if (code < 0)
        goto font3_error;
    pdfi_countdown(obj);

    font->PDF_font = font_dict;
    pdfi_countup(font_dict);

    code = replace_cache_entry(ctx, (pdf_obj *)font);
    if (code < 0)
        goto font3_error;

    code = gs_definefont(ctx->font_dir, (gs_font *)font->pfont);
    if (code < 0)
        goto font3_error;

    *ppfont = (gs_font *)font->pfont;

    return code;

font3_error:
    pdfi_countdown(obj);
    pdfi_countdown(font);
    return code;
}
