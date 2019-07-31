/* Copyright (C) 2001-2019 Artifex Software, Inc.
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
#include "gscoord.h"
#include "gsimage.h"            /* for gs_image_enum for plchar.h */
#include "gspath.h"

#include "plfont.h"
#include "plchar.h"
#include "plfapi.h"

/* defaults for locations of font collection objects (fco's) and
   plugins the root data directory.  These are internally separated with
   ':' but environment variable use the gp separator */

#ifndef UFSTFONTDIR
#if 0
static const char *UFSTFONTDIR = "/usr/local/fontdata5.0/";     /* A bogus linux location */
#endif
static const char *UFSTFONTDIR = "";    /* A bogus linux location */
#endif

/* default list of fcos and plugins - relative to UFSTFONTDIR */
/* FIXME: better solution for file name list separators */
#ifndef __WIN32__
static const char *UFSTFCOS =
    "%rom%fontdata/mtfonts/pclps2/mt3/pclp2_xj.fco:%rom%fontdata/mtfonts/pcl45/mt3/wd____xh.fco";
#else
static const char *UFSTFCOS =
    "%rom%fontdata/mtfonts/pclps2/mt3/pclp2_xj.fco;%rom%fontdata/mtfonts/pcl45/mt3/wd____xh.fco";
#endif

static const char *UFSTPLUGINS =
    "%rom%fontdata/mtfonts/pcl45/mt3/plug__xi.fco";

static const char *UFSTDIRPARM = "UFST_SSdir=";

static const char *UFSTPLUGINPARM = "UFST_PlugIn=";

extern const char gp_file_name_list_separator;

/* forward declarations for the pl_ff_stub definition */
static ulong
pl_fapi_get_long(gs_fapi_font * ff, gs_fapi_font_feature var_id, int index);

static int
pl_fapi_get_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, int ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID);

static int
pl_fapi_get_glyph(gs_fapi_font * ff, int char_code, byte * buf,
                  ushort buf_length);

static ushort
pl_fapi_serialize_tt_font(gs_fapi_font * ff, void *buf, int buf_size);

static int
pl_get_glyphdirectory_data(gs_fapi_font * ff, int char_code,
                           const byte ** ptr);

static int
pl_fapi_set_cache(gs_text_enum_t * penum, const gs_font_base * pbfont,
                  const gs_string * char_name, int cid,
                  const double pwidth[2], const gs_rect * pbbox,
                  const double Metrics2_sbw_default[4], bool * imagenow);

static int
pl_fapi_get_metrics(gs_fapi_font * ff, gs_string * char_name, int cid,
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
    {{3, 10}, {3, 1}, {-1, -1}, {-1, -1}, {-1, -1}},    /* ttf_cmap_req */
    {-1 , -1},                                          /* ttf_cmap_selected */
    0,                          /* client_ctx_p */
    0,                          /* client_font_data */
    0,                          /* client_font_data2 */
    0,                          /* char_data */
    0,                          /* char_data_len */
    0,                          /* embolden */
    NULL,                       /* get_word */
    pl_fapi_get_long,           /* get_long */
    NULL,                       /* get_float */
    NULL,                       /* get_name */
    NULL,                       /* get_proc */
    NULL,                       /* get_gsubr */
    NULL,                       /* get_subr */
    NULL,                       /* get_raw_subr */
    pl_fapi_get_glyph,          /* get_glyph */
    pl_fapi_serialize_tt_font,  /* serialize_tt_font */
    NULL,                       /* get_charstring */
    NULL,                       /* get_charstring_name */
    pl_get_glyphdirectory_data, /* get_GlyphDirectory_data_ptr */
    pl_fapi_get_cid,            /* get_glyphname_or_cid */
    pl_fapi_get_metrics,        /* fapi_get_metrics */
    pl_fapi_set_cache           /* fapi_set_cache */
};

static ulong
pl_fapi_get_long(gs_fapi_font * ff, gs_fapi_font_feature var_id, int index)
{
    gs_font *pfont = (gs_font *) ff->client_font_data;
    pl_font_t *plfont = (pl_font_t *) pfont->client_data;
    ulong value = -1;
    (void)index;

    if (var_id == gs_fapi_font_feature_TT_size) {
        value =
            plfont->header_size - (plfont->offsets.GT +
                                   (plfont->large_sizes ? 6 : 4));
    }
    return (value);
}

static int
pl_fapi_get_cid(gs_text_enum_t *penum, gs_font_base * pbfont, gs_string * charstring,
                gs_string * name, int ccode, gs_string * enc_char_name,
                char *font_file_path, gs_fapi_char_ref * cr, bool bCID)
{
    pl_font_t *plfont = pbfont->client_data;
    gs_glyph vertical, index = ccode;
    (void)charstring;
    (void)name;
    (void)enc_char_name;
    (void)font_file_path;
    (void)bCID;
    (void)penum;

    if (plfont->allow_vertical_substitutes) {
        vertical = pl_font_vertical_glyph(ccode, plfont);

        if (vertical != GS_NO_GLYPH)
            index = vertical;
    }
    cr->char_codes[0] = index;
    return (0);
}

static int
pl_fapi_get_glyph(gs_fapi_font * ff, int char_code, byte * buf,
                  ushort buf_length)
{
    gs_font *pfont = (gs_font *) ff->client_font_data;
    /* Zero is a valid size for a TTF glyph, so init to that.
     */
    int size = 0;
    gs_glyph_data_t pdata;

    if (pl_tt_get_outline((gs_font_type42 *) pfont, char_code, &pdata) == 0) {
        size = pdata.bits.size;

        if (buf && buf_length >= size) {
            memcpy(buf, pdata.bits.data, size);
        }
    }

    return (size);
}

static ushort
pl_fapi_serialize_tt_font(gs_fapi_font * ff, void *buf, int buf_size)
{
    gs_font *pfont = (gs_font *) ff->client_font_data;
    pl_font_t *plfont = (pl_font_t *) pfont->client_data;
    short code = -1;
    int offset = (plfont->offsets.GT + (plfont->large_sizes ? 6 : 4));

    if (buf_size >= (plfont->header_size - offset)) {
        code = 0;

        memcpy(buf, (plfont->header + offset), buf_size);
    }
    return ((ushort) code);
}

static int
pl_get_glyphdirectory_data(gs_fapi_font * ff, int char_code,
                           const byte ** ptr)
{
    return (0);
}

static int
pl_fapi_get_metrics(gs_fapi_font * ff, gs_string * char_name, int cid,
                    double *m, bool vertical)
{
    gs_font_base *pfont = (gs_font_base *) ff->client_font_data;
    int code = 0;

    /* We only want to supply metrics for Format 1 Class 2 glyph data (with their
     * PCL/PXL specific LSB and width metrics), all the others we leave the
     * scaler/render to use the metrics directly from the font/glyph.
     */
    if (pfont->FontType == ft_TrueType) {
        float sbw[4];

        code = pl_tt_f1c2_get_metrics((gs_font_type42 *)pfont, cid, pfont->WMode & 1, sbw);
        if (code == 0) {
            m[0] = sbw[0];
            m[1] = sbw[1];
            m[2] = sbw[2];
            m[3] = sbw[3];
            code = 2;
        }
        else
            code = 0;
    }
    return code;
}

static int
pl_fapi_set_cache(gs_text_enum_t * penum, const gs_font_base * pbfont,
                  const gs_string * char_name, int cid,
                  const double pwidth[2], const gs_rect * pbbox,
                  const double Metrics2_sbw_default[4], bool * imagenow)
{
    gs_gstate *pgs = penum->pgs;
    float w2[6];
    int code = 0;
    gs_fapi_server *I = pbfont->FAPI;

    if ((penum->text.operation & TEXT_DO_DRAW) && (pbfont->WMode & 1)
        && pwidth[0] == 1.0) {
        gs_rect tmp_pbbox;
        gs_matrix save_ctm;
        const gs_matrix id_ctm = { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
        /* This is kind of messy, but the cache entry has already been calculated
           using the in-force matrix. The problem is that we have to call gs_setcachedevice
           with the in-force matrix, not the rotated one, so we have to recalculate the extents
           to be correct for the rotated glyph.
         */

        /* save the ctm */
        gs_currentmatrix(pgs, &save_ctm);
        gs_setmatrix(pgs, &id_ctm);

        /* magic numbers - we don't completelely understand
           the translation magic used by HP.  This provides a
           good approximation */
        gs_translate(pgs, 1.0 / 1.15, -(1.0 - 1.0 / 1.15));
        gs_rotate(pgs, 90);

        gs_transform(pgs, pbbox->p.x, pbbox->p.y, &tmp_pbbox.p);
        gs_transform(pgs, pbbox->q.x, pbbox->q.y, &tmp_pbbox.q);

        w2[0] = pwidth[0];
        w2[1] = pwidth[1];
        w2[2] = tmp_pbbox.p.x;
        w2[3] = tmp_pbbox.p.y;
        w2[4] = tmp_pbbox.q.x;
        w2[5] = tmp_pbbox.q.y;

        gs_setmatrix(pgs, &save_ctm);
    } else {
        w2[0] = pwidth[0];
        w2[1] = pwidth[1];
        w2[2] = pbbox->p.x;
        w2[3] = pbbox->p.y;
        w2[4] = pbbox->q.x;
        w2[5] = pbbox->q.y;
    }

    if (pbfont->PaintType) {
        double expand = max(1.415,
                            gs_currentmiterlimit(pgs)) *
            gs_currentlinewidth(pgs) / 2;

        w2[2] -= expand;
        w2[3] -= expand;
        w2[4] += expand;
        w2[5] += expand;
    }

    if (I->ff.embolden != 0) {
        code = gs_setcharwidth((gs_show_enum *) penum, pgs, w2[0], w2[1]);
    } else {
        if ((code = gs_setcachedevice((gs_show_enum *) penum, pgs, w2)) < 0) {
            return (code);
        }
    }

    if ((penum->text.operation & TEXT_DO_DRAW) && (pbfont->WMode & 1)
        && pwidth[0] == 1.0) {
        *imagenow = false;
        return (gs_error_unknownerror);
    }

    *imagenow = true;
    return (code);
}

static int
pl_fapi_set_cache_rotate(gs_text_enum_t * penum, const gs_font_base * pbfont,
                         const gs_string * char_name, int cid,
                         const double pwidth[2], const gs_rect * pbbox,
                         const double Metrics2_sbw_default[4],
                         bool * imagenow)
{
    *imagenow = true;
    return (0);
}


static int
pl_fapi_build_char(gs_show_enum * penum, gs_gstate * pgs, gs_font * pfont,
                   gs_char chr, gs_glyph glyph)
{
    int code;
    gs_matrix save_ctm;
    gs_font_base *pbfont = (gs_font_base *) pfont;
    pl_font_t *plfont = (pl_font_t *) pfont->client_data;
    gs_fapi_server *I = pbfont->FAPI;

    I->ff.embolden = plfont->bold_fraction;
    I->ff.is_mtx_skipped = plfont->is_xl_format;

    code =
        gs_fapi_do_char(pfont, pgs, (gs_text_enum_t *) penum, NULL, false,
                        NULL, NULL, chr, glyph, 0);

    if (code == gs_error_unknownerror) {
        gs_fapi_font tmp_ff;

        tmp_ff.fapi_set_cache = I->ff.fapi_set_cache;

        /* save the ctm */
        gs_currentmatrix(pgs, &save_ctm);

        /* magic numbers - we don't completelely understand
           the translation magic used by HP.  This provides a
           good approximation */
        gs_translate(pgs, 1.0 / 1.15, -(1.0 - 1.0 / 1.15));
        gs_rotate(pgs, 90);

        I->ff.fapi_set_cache = pl_fapi_set_cache_rotate;

        code =
            gs_fapi_do_char(pfont, pgs, (gs_text_enum_t *) penum, NULL, false,
                            NULL, NULL, chr, glyph, 0);

        I->ff.fapi_set_cache = tmp_ff.fapi_set_cache;

        gs_setmatrix(pgs, &save_ctm);
    }

    I->ff.embolden = 0;

    return (code);
}

/* FIXME: environment variables.... */
const char *
pl_fapi_ufst_get_fco_list(gs_memory_t * mem)
{
    return (UFSTFCOS);
}

const char *
pl_fapi_ufst_get_font_dir(gs_memory_t * mem)
{
    return (UFSTFONTDIR);
}

static void
pl_get_server_param(gs_fapi_server * I, const char *subtype,
                    char **server_param, int *server_param_size)
{
    int length = 0;
    char SEPARATOR_STRING[2];

    SEPARATOR_STRING[0] = (char)gp_file_name_list_separator;
    SEPARATOR_STRING[1] = '\0';

    length += strlen(UFSTDIRPARM);
    length += strlen(UFSTFONTDIR);
    length += strlen(SEPARATOR_STRING);
    length += strlen(UFSTPLUGINPARM);
    length += strlen(UFSTPLUGINS);
    length++;

    if ((*server_param) != NULL && (*server_param_size) >= length) {
        strcpy((char *)*server_param, (char *)UFSTDIRPARM);
        strcat((char *)*server_param, (char *)UFSTFONTDIR);
        strcat((char *)*server_param, (char *)SEPARATOR_STRING);
        strcat((char *)*server_param, (char *)UFSTPLUGINPARM);
        strcat((char *)*server_param, (char *)UFSTPLUGINS);
    } else {
        *server_param = NULL;
        *server_param_size = length;
    }
}


static inline int
pl_fapi_get_mtype_font_info(gs_font * pfont, gs_fapi_font_info item,
                            void *data, int *size)
{
    return (gs_fapi_get_font_info(pfont, item, 0, data, size));
}

int
pl_fapi_get_mtype_font_name(gs_font * pfont, byte * data, int *size)
{
    return (pl_fapi_get_mtype_font_info
            (pfont, gs_fapi_font_info_name, data, size));
}

int
pl_fapi_get_mtype_font_number(gs_font * pfont, int *font_number)
{
    int size = (int)sizeof(*font_number);

    return (pl_fapi_get_mtype_font_info
            (pfont, gs_fapi_font_info_uid, font_number, &size));
}

int
pl_fapi_get_mtype_font_spaceBand(gs_font * pfont, uint * spaceBand)
{
    int size = (int)sizeof(*spaceBand);

    return (pl_fapi_get_mtype_font_info
            (pfont, gs_fapi_font_info_pitch, spaceBand, &size));
}

int
pl_fapi_get_mtype_font_scaleFactor(gs_font * pfont, uint * scaleFactor)
{
    int size = (int)sizeof(*scaleFactor);

    return (pl_fapi_get_mtype_font_info
            (pfont, gs_fapi_font_info_design_units, scaleFactor, &size));
}

static text_enum_proc_is_width_only(pl_show_text_is_width_only);
static text_enum_proc_release(pl_text_release);

static const gs_text_enum_procs_t null_text_procs = {
    NULL, NULL,
    pl_show_text_is_width_only, NULL,
    NULL, NULL,
    pl_text_release
};

static bool
pl_show_text_is_width_only(const gs_text_enum_t *pte)
{
    return(true);
}

void pl_text_release(gs_text_enum_t *pte, client_name_t cname)
{
  (void)pte;
  (void)cname;
  return;
}

static int
pl_fapi_set_cache_metrics(gs_text_enum_t * penum, const gs_font_base * pbfont,
                         const gs_string * char_name, int cid,
                         const double pwidth[2], const gs_rect * pbbox,
                         const double Metrics2_sbw_default[4],
                         bool * imagenow)
{
    penum->returned.total_width.x = pwidth[0];
    penum->returned.total_width.y = pwidth[1];

    *imagenow = false;
    return (gs_error_unknownerror);
}

static int
pl_fapi_char_metrics(const pl_font_t * plfont, const void *vpgs,
                     gs_char char_code, float metrics[4])
{
    int code = 0;
    gs_text_enum_t *penum1;
    gs_font *pfont = plfont->pfont;
    gs_font_base *pbfont = (gs_font_base *) pfont;
    gs_text_params_t text;
    gs_char buf[2];
    gs_gstate *rpgs = (gs_gstate *) vpgs;
    /* NAFF: undefined glyph would be better handled inside FAPI */
    gs_char chr = char_code;
    gs_glyph unused_glyph = GS_NO_GLYPH;
    gs_glyph glyph;
    gs_matrix mat = {72.0, 0.0, 0.0, 72.0, 0.0, 0.0};
    gs_matrix fmat;
    gs_fapi_font tmp_ff;

    if (pfont->FontType == ft_MicroType) {
        glyph = char_code;
    } else {
        glyph = pl_tt_encode_char(pfont, chr, unused_glyph);
    }

    if (pfont->WMode & 1) {
        gs_glyph vertical = pl_font_vertical_glyph(glyph, plfont);

        if (vertical != GS_NO_GLYPH)
            glyph = vertical;
    }

    /* undefined character */
    if (glyph == 0xffff || glyph == GS_NO_GLYPH) {
        metrics[0] = metrics[1] = metrics[2] = metrics[3] = 0;
        code = 1;
    } else {
        gs_fapi_server *I = pbfont->FAPI;
        gs_gstate lpgs;
        gs_gstate *pgs = &lpgs;

        /* This is kind of naff, but it's *much* cheaper to copy
         * the parts of the gstate we need, than gsave/grestore
         */
        memset(pgs, 0x00, sizeof(lpgs));
        pgs->memory = rpgs->memory;
        pgs->ctm = rpgs->ctm;
        pgs->in_cachedevice = CACHE_DEVICE_NOT_CACHING;
        pgs->device = rpgs->device;
        pgs->log_op = rpgs->log_op;
        *(pgs->color) = *(rpgs->color);

        tmp_ff.fapi_set_cache = I->ff.fapi_set_cache;
        I->ff.fapi_set_cache = pl_fapi_set_cache_metrics;

        gs_setmatrix(pgs, &mat);
        fmat = pfont->FontMatrix;
        pfont->FontMatrix = pfont->orig_FontMatrix;
        (void)gs_setfont(pgs, pfont);

        I->ff.is_mtx_skipped = plfont->is_xl_format;

        buf[0] = char_code;
        buf[1] = '\0';

        text.operation = TEXT_FROM_CHARS | TEXT_DO_NONE | TEXT_RETURN_WIDTH;
        text.data.chars = buf;
        text.size = 1;

        if ((penum1 = gs_text_enum_alloc(pfont->memory, pgs,
                      "pl_fapi_char_metrics")) != NULL) {

            if ((code = gs_text_enum_init(penum1, &null_text_procs,
                                 NULL, pgs, &text, pfont,
                                 NULL, NULL, NULL, pfont->memory)) >= 0) {

                code = gs_fapi_do_char(pfont, pgs, penum1, plfont->font_file, false,
                            NULL, NULL, char_code, glyph, 0);

                if (code >= 0 || code == gs_error_unknownerror) {
                    metrics[0] = metrics[1] = 0;
                    metrics[2] = penum1->returned.total_width.x;
                    metrics[3] = penum1->returned.total_width.y;
                    if (code < 0)
                        code = 0;
                }
            }
            rc_decrement(penum1, "pl_fapi_char_metrics");
        }
        pfont->FontMatrix = fmat;
        I->ff.fapi_set_cache = tmp_ff.fapi_set_cache;
    }
    return (code);
}

static int
pl_fapi_char_width(const pl_font_t * plfont, const void *pgs,
                   gs_char char_code, gs_point * pwidth)
{
    float metrics[4];
    int code = 0;

    code = pl_fapi_char_metrics(plfont, pgs, char_code, metrics);

    pwidth->x = metrics[2];
    pwidth->y = 0;

    return (code);
}

static gs_glyph
pl_fapi_encode_char(gs_font * pfont, gs_char pchr, gs_glyph_space_t not_used)
{
    return (gs_glyph) pchr;
}

int
pl_fapi_passfont(pl_font_t * plfont, int subfont, char *fapi_request,
                 char *file_name, byte * font_data, int font_data_len)
{
    char *fapi_id = NULL;
    int code = 0;
    gs_string fdata;
    gs_font *pfont = plfont->pfont;
    gs_fapi_font local_pl_ff_stub;

    if (!gs_fapi_available(pfont->memory, NULL)) {
        return (code);
    }

    local_pl_ff_stub = pl_ff_stub;
    local_pl_ff_stub.is_mtx_skipped = plfont->is_xl_format;

    fdata.data = font_data;
    fdata.size = font_data_len;

    /* The plfont should contain everything we need, but setting the client data for the server
     * to pbfont makes as much sense as setting it to NULL.
     */
    gs_fapi_set_servers_client_data(pfont->memory,
                                    (const gs_fapi_font *)&local_pl_ff_stub,
                                    pfont);

    code =
        gs_fapi_passfont(pfont, subfont, (char *)file_name, &fdata,
                         (char *)fapi_request, NULL, (char **)&fapi_id,
                         (gs_fapi_get_server_param_callback)
                         pl_get_server_param);

    if (code < 0 || fapi_id == NULL) {
        return code;
    }

    pfont->procs.build_char = pl_fapi_build_char;
    if (pfont->FontType == ft_MicroType) {
        pfont->procs.encode_char = pl_fapi_encode_char;
    }
    plfont->char_width = pl_fapi_char_width;
    plfont->char_metrics = pl_fapi_char_metrics;

    return (code);
}

bool
pl_fapi_ufst_available(gs_memory_t * mem)
{
    gs_fapi_server *serv = NULL;
    int code = gs_fapi_find_server(mem, (char *)"UFST", &serv,
                                   (gs_fapi_get_server_param_callback)
                                   pl_get_server_param);

    if (code == 0 && serv != NULL) {
        return (true);
    } else {
        return (false);
    }
}
