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
pdfi_fapi_get_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
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
    {{3, 10}, {3, 1}, {-1, -1}, {-1, -1}, {-1, -1}},    /* ttf_cmap_req */
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
    pdfi_fapi_get_cid,            /* get_glyphname_or_cid */
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
pdfi_fapi_get_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, int ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID)
{
    return (0);
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

    if (!gs_fapi_available(pfont->memory, NULL)) {
        return (code);
    }

    fdata.data = font_data;
    fdata.size = font_data_len;

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
