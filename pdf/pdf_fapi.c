/* Copyright (C) 2001-2019 Artifex Software, Inc.
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
#include "pdf_int.h"
#include "pdf_font.h"

/* forward declarations for the pdfi_ff_stub definition */
static ulong
pdfi_fapi_get_long(gs_fapi_font * ff, gs_fapi_font_feature var_id, int index);

static int
pdfi_fapi_get_glyphname_or_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, int ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID);

static int
pdfi_fapi_get_glyph(gs_fapi_font * ff, int char_code, byte * buf,
                  ushort buf_length);

static ushort
pdfi_fapi_serialize_tt_font(gs_fapi_font * ff, void *buf, int buf_size);

static int
pdfi_get_glyphdirectory_data(gs_fapi_font * ff, int char_code,
                           const byte ** ptr);

static int
pdfi_fapi_set_cache(gs_text_enum_t * penum, const gs_font_base * pbfont,
                  const gs_string * char_name, int cid,
                  const double pwidth[2], const gs_rect * pbbox,
                  const double Metrics2_sbw_default[4], bool * imagenow);

static int
pdfi_fapi_get_metrics(gs_fapi_font * ff, gs_string * char_name, int cid,
                    double *m, bool vertical);

static const gs_fapi_font pdfi_ff_stub = {
    0,                          /* server_font_data */
    0,                          /* need_decrypt */
    NULL,                       /* const gs_memory_t */
    0,                          /* font_file_path */
    0,                          /* full_font_buf */
    0,                          /* full_font_buf_len */
    0,                          /* subfont */
    false,                      /* is_type1 */
    false,                      /* is_cid */
    false,                      /* is_outline_font */
    false,                      /* is_mtx_skipped */
    false,                      /* is_vertical */
    false,                      /* metrics_only */
    {{3, 1}, {1, 0}, {3, 0}, {3, 10}, {-1, -1}},    /* ttf_cmap_req */
    0,                          /* client_ctx_p */
    0,                          /* client_font_data */
    0,                          /* client_font_data2 */
    0,                          /* char_data */
    0,                          /* char_data_len */
    0,                          /* embolden */
    NULL,                       /* get_word */
    pdfi_fapi_get_long,           /* get_long */
    NULL,                       /* get_float */
    NULL,                       /* get_name */
    NULL,                       /* get_proc */
    NULL,                       /* get_gsubr */
    NULL,                       /* get_subr */
    NULL,                       /* get_raw_subr */
    pdfi_fapi_get_glyph,          /* get_glyph */
    pdfi_fapi_serialize_tt_font,  /* serialize_tt_font */
    NULL,                       /* get_charstring */
    NULL,                       /* get_charstring_name */
    pdfi_get_glyphdirectory_data, /* get_GlyphDirectory_data_ptr */
    pdfi_fapi_get_glyphname_or_cid, /* get_glyphname_or_cid */
    pdfi_fapi_get_metrics,        /* fapi_get_metrics */
    pdfi_fapi_set_cache           /* fapi_set_cache */
};

static ulong
pdfi_fapi_get_long(gs_fapi_font * ff, gs_fapi_font_feature var_id, int index)
{
    ulong value = -1;
    (void)index;

    return (value);
}

static int
pdfi_fapi_get_glyphname_or_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, int ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID)
{
    if (pbfont->FontType == ft_TrueType) {
        pdf_font_truetype *font = (pdf_font_truetype *)pbfont->client_data;
        cr->client_char_code = ccode;
        if (font->descflags & 4){
            cr->is_glyph_index = false;
            return 0;
        }
    }
    return pbfont->procs.glyph_name((gs_font *)pbfont, ccode, enc_char_name);
}

static int
pdfi_fapi_get_glyph(gs_fapi_font * ff, int char_code, byte * buf,
                  ushort buf_length)
{
    return 0;
}

static ushort
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

static int
pdfi_fapi_get_metrics(gs_fapi_font * ff, gs_string * char_name, int cid,
                    double *m, bool vertical)
{
    return 0;
}

static int
pdfi_fapi_set_cache(gs_text_enum_t * penum, const gs_font_base * pbfont,
                  const gs_string * char_name, int cid,
                  const double pwidth[2], const gs_rect * pbbox,
                  const double Metrics2_sbw_default[4], bool * imagenow)
{
    int code = 0;
    gs_gstate *pgs = penum->pgs;
    float w2[6];

    w2[0] = pwidth[0];
    w2[1] = pwidth[1];
    w2[2] = pbbox->p.x;
    w2[3] = pbbox->p.y;
    w2[4] = pbbox->q.x;
    w2[5] = pbbox->q.y;

    if (pbfont->PaintType) {
        double expand = max(1.415,
                            gs_currentmiterlimit(pgs)) *
            gs_currentlinewidth(pgs) / 2;

        w2[2] -= expand;
        w2[3] -= expand;
        w2[4] += expand;
        w2[5] += expand;
    }
    if ((code = gs_setcachedevice((gs_show_enum *) penum, pgs, w2)) < 0) {
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
    code =
        gs_fapi_do_char(pfont, pgs, (gs_text_enum_t *) penum, NULL, false,
                        NULL, NULL, chr, glyph, 0);

    return (code);
}

static void
pdfi_get_server_param(gs_fapi_server * I, const char *subtype,
                    char **server_param, int *server_param_size)
{
}


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

int
pdfi_fapi_passfont(pdf_font *font, int subfont, char *fapi_request,
                 char *file_name, byte * font_data, int font_data_len)
{
    char *fapi_id = NULL;
    int code = 0;
    gs_string fdata;
    gs_font *pfont = (gs_font *)font->pfont;
    gs_fapi_font local_pdf_ff_stub = pdfi_ff_stub;
    gs_fapi_ttf_cmap_request symbolic_req[GS_FAPI_NUM_TTF_CMAP_REQ] = {{1, 0}, {3, 0}, {3, 1}, {3, 10}, {-1, -1}};
    gs_fapi_ttf_cmap_request nonsymbolic_req[GS_FAPI_NUM_TTF_CMAP_REQ] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

    if (!gs_fapi_available(pfont->memory, NULL)) {
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
    gs_fapi_set_servers_client_data(pfont->memory,
                                    (const gs_fapi_font *)&pdfi_ff_stub,
                                    pfont);

    code =
        gs_fapi_passfont(pfont, subfont, (char *)file_name, &fdata,
                         (char *)fapi_request, NULL, (char **)&fapi_id,
                         (gs_fapi_get_server_param_callback)
                         pdfi_get_server_param);

    if (code < 0 || fapi_id == NULL) {
        return code;
    }

    pfont->procs.build_char = pdfi_fapi_build_char;

    return (code);
}
