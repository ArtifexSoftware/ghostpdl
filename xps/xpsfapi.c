/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  1305 Grant Avenue - Suite 200,
   Novato, CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* Font API support  */
#include "memory_.h"
#include "gsmemory.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gzstate.h"
#include "gxchar.h"             /* for st_gs_show_enum */
#include "gdebug.h"
#include "gxfapi.h"

#include "xpsfapi.h"

/* forward declarations for the pl_ff_stub definition */
static int
xps_fapi_get_long(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned long *ret);

static int
xps_fapi_get_glyph(gs_fapi_font *ff, gs_glyph char_code, byte *buf,
                   int buf_length);

static int
xps_fapi_serialize_tt_font(gs_fapi_font *ff, void *buf, int buf_size);

static int
xps_get_glyphdirectory_data(gs_fapi_font *ff, int char_code,
                            const byte **ptr);

static int
xps_fapi_set_cache(gs_text_enum_t *penum, const gs_font_base *pbfont,
                   const gs_string *char_name, gs_glyph cid,
                   const double pwidth[2], const gs_rect *pbbox,
                   const double Metrics2_sbw_default[4], bool *imagenow);

static int
xps_fapi_get_metrics(gs_fapi_font *ff, gs_string *char_name, gs_glyph cid,
                     double *m, bool vertical);

static const gs_fapi_font pl_ff_stub = {
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
    {
    {3,10}, {3,1},{3,5},{3,4},\
      {3,3},{3,2},{3,0},{1,0},
      {-1,-1}},                 /* ttf_cmap_req */
    {-1, -1},                   /* ttf_cmap_selected */
    0,                          /* client_ctx_p */
    0,                          /* client_font_data */
    0,                          /* client_font_data2 */
    0,                          /* char_data */
    0,                          /* char_data_len */
    0,                          /* embolden */
    NULL,                       /* get_word */
    xps_fapi_get_long,          /* get_long */
    NULL,                       /* get_float */
    NULL,                       /* get_name */
    NULL,                       /* get_proc */
    NULL,                       /* get_gsubr */
    NULL,                       /* get_subr */
    NULL,                       /* get_raw_subr */
    xps_fapi_get_glyph,         /* get_glyph */
    xps_fapi_serialize_tt_font, /* serialize_tt_font */
    NULL,                       /* get_charstring */
    NULL,                       /* get_charstring_name */
    xps_get_glyphdirectory_data,        /* get_GlyphDirectory_data_ptr */
    0,                          /* get_glyphname_or_cid */
    xps_fapi_get_metrics,       /* fapi_get_metrics */
    xps_fapi_set_cache          /* fapi_set_cache */
};

static int
xps_fapi_get_long(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned long *ret)
{
    ulong value = -1;

    return (value);
}

static int
xps_fapi_get_glyph(gs_fapi_font *ff, gs_glyph char_code, byte *buf,
                   int buf_length)
{
    int size = -1;

    return (size);
}

static int
xps_fapi_serialize_tt_font(gs_fapi_font *ff, void *buf, int buf_size)
{
    int code = -1;

    return (code);
}

static int
xps_get_glyphdirectory_data(gs_fapi_font *ff, int char_code,
                            const byte **ptr)
{
    return (0);
}

static int
xps_fapi_get_metrics(gs_fapi_font *ff, gs_string *char_name, gs_glyph cid,
                     double *m, bool vertical)
{
    return (0);
}

static int
xps_fapi_set_cache(gs_text_enum_t *penum, const gs_font_base *pbfont,
                   const gs_string *char_name, gs_glyph cid,
                   const double pwidth[2], const gs_rect *pbbox,
                   const double Metrics2_sbw_default[4], bool *imagenow)
{
    gs_gstate *pgs = penum->pgs;
    float w2[6];

    w2[0] = pwidth[0];
    w2[1] = pwidth[1];
    w2[2] = pbbox->p.x;
    w2[3] = pbbox->p.y;
    w2[4] = pbbox->q.x;
    w2[5] = pbbox->q.y;
    if (pbfont->PaintType) {
        double expand =
            max(1.415,
                gs_currentmiterlimit(pgs)) * gs_currentlinewidth(pgs) / 2;

        w2[2] -= expand;
        w2[3] -= expand;
        w2[4] += expand;
        w2[5] += expand;
    }
    *imagenow = true;
    return (gs_setcachedevice((gs_show_enum *)penum, pgs, w2));
}

static int
xps_fapi_build_char(gs_show_enum *penum, gs_gstate *pgs, gs_font *pfont,
                    gs_char chr, gs_glyph glyph)
{
    int code;

    code =
        gs_fapi_do_char(pfont, pgs,(gs_text_enum_t *)penum, NULL, false, NULL,
                        NULL, chr, glyph, 0);

    return (code);
}

static void
xps_get_server_param(gs_fapi_server *I, const char *subtype,
                     char **server_param, int *server_param_size)
{
    *server_param = NULL;
    *server_param_size = 0;
}

int
xps_fapi_passfont(gs_font *pfont, char *fapi_request, char *file_name,
                  byte *font_data, int font_data_len)
{
    char *fapi_id = NULL;
    int code = 0;
    gs_string fdata;

    if (!gs_fapi_available(pfont->memory, NULL)) {
        return (code);
    }

    fdata.data = font_data;
    fdata.size = font_data_len;

    /* The plfont should contain everything we need, but setting the client data for the server
     * to pbfont makes as much sense as setting it to NULL.
     */
    gs_fapi_set_servers_client_data(pfont->memory, &pl_ff_stub, pfont);

    code =
        gs_fapi_passfont(pfont, 0, file_name, &fdata, fapi_request, NULL,
                         &fapi_id,
                         (gs_fapi_get_server_param_callback)xps_get_server_param);

    if (code >= 0 && fapi_id == NULL) {
        code = gs_error_invalidfont;
    }

    pfont->procs.build_char = xps_fapi_build_char;
    return (code);
}
