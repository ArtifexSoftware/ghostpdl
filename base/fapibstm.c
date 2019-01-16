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


/* $Id: fapibstm.c 11056 2010-04-13 09:38:22Z chrisl $ */
/* Bitstream font engine plugin */

/* GS includes : */
#include "stdio_.h"
#include "memory_.h"
#include "math_.h"
#include "gserrors.h"
#include "iplugin.h"
#include "ifapi.h"
#include "strmio.h"
#include "gsmalloc.h"

#include "config.h"
#include "t2k.h"

typedef struct fapi_bitstream_server_s fapi_bitstream_server;

typedef struct fapi_bitstream_server_s
{
    FAPI_server If;
    int bInitialized;
    FAPI_font *ff;
    i_plugin_client_memory client_mem;
} Bitstream_server;

typedef struct Bitstream_face_s
{
    tsiMemObject *MemObject;
    InputStream *Input;
    sfntClass *sfnt;
    T2K *T2K;
    unsigned int HRes, VRes;
    T2K_TRANS_MATRIX trans;
    unsigned int Initialised;

    /* Non-null if font data is owned by this object. */
    unsigned char *font_data;
} Bitstream_face;

static int
InitFont(Bitstream_server * server, FAPI_font * ff)
{
    unsigned char *own_font_data = NULL, *temp;
    gs_memory_t *mem = (gs_memory_t *) server->client_mem.client_data;
    Bitstream_face *face = (Bitstream_face *) ff->server_font_data;
    long length, written;
    int error, type;

    if (ff->font_file_path) {
    }
    /* Load a typeface from a representation in GhostScript's memory. */
    else {
        if (ff->is_type1) {
            type = ff->get_word(ff, FAPI_FONT_FEATURE_FontType, 0);

            /* Tell the FAPI interface that we need to *not* decrypt the /Subrs data. */
            ff->need_decrypt = false;

            /*
               Serialise a type 1 font in PostScript source form, or
               a Type 2 font in binary form, so that FreeType can read it.
             */
            if (type == 1)
                length = FF_serialize_type1_font_complete(ff, NULL, 0);
            else
                length = FF_serialize_type2_font(ff, NULL, 0);
            own_font_data = gs_malloc(mem, 1, length, "Type 1 font copy");
            if (type == 1)
                written =
                    FF_serialize_type1_font_complete(ff, own_font_data,
                                                     length);
            else
                written = FF_serialize_type2_font(ff, own_font_data, length);
            if (written != length)
                return (gs_error_unregistered);        /* Must not happen. */
        }
        /* It must be type 42 (see code in FAPI_FF_get_glyph in zfapi.c). */
        else {
            /* Get the length of the TrueType data. */
            long length = ff->get_long(ff, FAPI_FONT_FEATURE_TT_size, 0);

            if (length == 0)
                return = gs_error_invalidfont;

            /* Load the TrueType data into a single buffer. */
            own_font_data = gs_malloc(mem, 1, length, "Type 42 fotn copy");
            if (!own_font_data)
                return_error(gs_error_VMerror);
            if (ff->serialize_tt_font(ff, own_font_data, length))
                return_error(gs_error_invalidfont);
        }
    }
    face->font_data = own_font_data;
    face->Input =
        New_InputStream3(face->MemObject, own_font_data, length, &error);
    if (ff->is_type1) {
        if (type == 1)
            face->sfnt =
                FF_New_sfntClass(face->MemObject, FONT_TYPE_1, 1, face->Input,
                                 NULL, NULL, &error);
        else
            face->sfnt =
                FF_New_sfntClass(face->MemObject, FONT_TYPE_2, 1, face->Input,
                                 NULL, NULL, &error);
    }
    else {
        face->sfnt =
            FF_New_sfntClass(face->MemObject, FONT_TYPE_TT_OR_T2K, 1,
                             face->Input, NULL, NULL, &error);
    }
    face->T2K = NewT2K(face->MemObject, face->sfnt, &error);
    face->Initialised = 1;
    return 0;
}

static FAPI_retcode
ensure_open(FAPI_server * server, const byte * server_param,
            int server_param_size)
{
    return 0;
}

static FAPI_retcode
get_scaled_font(FAPI_server * server, FAPI_font * ff,
                const FAPI_font_scale * font_scale, const char *xlatmap,
                FAPI_descendant_code dc)
{
    Bitstream_server *Bserver = (Bitstream_server *) server;
    Bitstream_face *face = (Bitstream_face *) ff->server_font_data;

    if (face == NULL) {
        gs_memory_t *mem = (gs_memory_t *) Bserver->client_mem.client_data;
        int error;

        face =
            (Bitstream_face *) gs_malloc(mem, 1, sizeof(Bitstream_face),
                                         "Bitstream_face alloc");
        face->MemObject = tsi_NewMemhandler(&error);
        if (error)
            return -1;
        face->Initialised = 0;
        ff->server_font_data = face;
        Bserver->ff = ff;
    }
    face->HRes = font_scale->HWResolution[0] >> 16;
    face->VRes = font_scale->HWResolution[1] >> 16;
    face->trans.t00 = ONE16Dot16 * 50;  /* font_scale->matrix[0]; */
    face->trans.t01 = 0;        /*font_scale->matrix[1]; */
    face->trans.t10 = 0;        /*font_scale->matrix[2]; */
    face->trans.t11 = ONE16Dot16 * 50;  /*font_scale->matrix[3]; */
    return 0;
}

static FAPI_retcode
get_decodingID(FAPI_server * server, FAPI_font * ff,
               const char **decodingID_result)
{
    return 0;
}

static FAPI_retcode
get_font_bbox(FAPI_server * server, FAPI_font * ff, int BBox[4], int unitsPerEm[2])
{
    return 0;
}

static FAPI_retcode
get_font_proportional_feature(FAPI_server * server, FAPI_font * ff,
                              bool * bProportional)
{
    return 0;
}

static FAPI_retcode
can_retrieve_char_by_name(FAPI_server * server, FAPI_font * ff,
                          FAPI_char_ref * c, int *result)
{
    *result = 1;
    return 0;
}

static FAPI_retcode
can_replace_metrics(FAPI_server * server, FAPI_font * ff, FAPI_char_ref * c,
                    int *result)
{
    *result = 1;
    return 0;
}

static FAPI_retcode
get_fontmatrix(FAPI_server * I, gs_matrix * m)
{
    gs_matrix *base_font_matrix = &I->initial_FontMatrix;

    m->xx = I->initial_FontMatrix.xx;
    m->xy = I->initial_FontMatrix.xy;
    m->yx = I->initial_FontMatrix.yx;
    m->yy = I->initial_FontMatrix.yy;
    m->tx = I->initial_FontMatrix.tx;
    m->ty = I->initial_FontMatrix.ty;
    return 0;
}

static FAPI_retcode
get_char_width(FAPI_server * server, FAPI_font * ff, FAPI_char_ref * c,
               FAPI_metrics * metrics)
{
    return 0;
}

static FAPI_retcode
get_char_raster_metrics(FAPI_server * server, FAPI_font * ff,
                        FAPI_char_ref * c, FAPI_metrics * metrics)
{
    Bitstream_face *face = (Bitstream_face *) ff->server_font_data;
    int error;

    if (!face->Initialised)
        InitFont((Bitstream_server *) server, ff);

    T2K_NewTransformation(face->T2K, 1, face->HRes, face->VRes, &face->trans,
                          false, &error);
    T2K_RenderGlyph(face->T2K, c->char_codes[0], 0, 0, BLACK_AND_WHITE_BITMAP,
                    T2K_NAT_GRID_FIT | T2K_RETURN_OUTLINES | T2K_SCAN_CONVERT,
                    &error);

    metrics->bbox_x0 = face->T2K->fLeft26Dot6 >> 6;
    metrics->bbox_y0 = 0;
    metrics->bbox_x1 =
        metrics->bbox_x0 + face->T2K->xLinearAdvanceWidth16Dot16 >> 16;
    metrics->bbox_y1 = face->T2K->fTop26Dot6 >> 6;
    metrics->escapement = face->T2K->xLinearAdvanceWidth16Dot16 >> 16;
    metrics->v_escapement = face->T2K->yLinearAdvanceWidth16Dot16 >> 16;
    metrics->em_x = face->T2K->xPixelsPerEm;
    metrics->em_y = face->T2K->yPixelsPerEm;
    return 0;
}

static FAPI_retcode
get_char_raster(FAPI_server * server, FAPI_raster * rast)
{
    FAPI_font *ff = &server->ff;
    Bitstream_face *face = (Bitstream_face *) ff->server_font_data;

    rast->p = face->T2K->baseAddr;
    rast->width = face->T2K->width;
    rast->height = face->T2K->height;
    rast->line_step = face->T2K->rowBytes;
    rast->orig_x = 0;
    rast->orig_y = 0;
    rast->left_indent = rast->top_indent = rast->black_height =
        rast->black_width = 0;
    return 0;
}

static FAPI_retcode
get_char_outline_metrics(FAPI_server * server, FAPI_font * ff,
                         FAPI_char_ref * c, FAPI_metrics * metrics)
{
    return 0;
}

static FAPI_retcode
get_char_outline(FAPI_server * server, FAPI_path * p)
{
    return 0;
}

static FAPI_retcode
release_char_data(FAPI_server * server)
{
    FAPI_font *ff = &server->ff;
    Bitstream_face *face = (Bitstream_face *) ff->server_font_data;
    int error;

    T2K_PurgeMemory(face->T2K, 1, &error);
    return 0;
}

static FAPI_retcode
release_typeface(FAPI_server * server, void *font_data)
{
    int error;
    Bitstream_server *Bserver = (Bitstream_server *) server;
    Bitstream_face *face = (Bitstream_face *) font_data;
    gs_memory_t *mem = (gs_memory_t *) Bserver->client_mem.client_data;

    if (face->Initialised) {
        DeleteT2K(face->T2K, &error);
        FF_Delete_sfntClass(face->sfnt, &error);
        Delete_InputStream(face->Input, &error);
        gs_free(mem, face->font_data, 0, 0, "free font copy");
        tsi_DeleteMemhandler(face->MemObject);
    }
    return 0;
}

static FAPI_retcode
check_cmap_for_GID(FAPI_server * server, uint index)
{
    return 0;
}

static FAPI_retcode
gs_fapi_bstm_set_mm_weight_vector(gs_fapi_server *server, gs_fapi_font *ff, float *wvector, int length)
{
    (void)server;
    (void)ff;
    (void)wvector;
    (void)length;
    
    return gs_error_invalidaccess;
}

static void gs_fapibstm_finit(i_plugin_instance * instance,
                              i_plugin_client_memory * mem);

static const i_plugin_descriptor bitstream_descriptor = {
    "FAPI",
    "BITSTREAM",
    gs_fapibstm_finit
};

static const FAPI_server If0 = {
    {&bitstream_descriptor},
    16,                         /* frac_shift */
    {gs_no_id},
    {0},
    0,
    false,
    1,
    {1, 0, 0, 1, 0, 0},
    ensure_open,
    get_scaled_font,
    get_decodingID,
    get_font_bbox,
    get_font_proportional_feature,
    can_retrieve_char_by_name,
    can_replace_metrics,
    NULL,                       /* can_simulate_style */
    get_fontmatrix,
    get_char_width,
    get_char_raster_metrics,
    get_char_raster,
    get_char_outline_metrics,
    get_char_outline,
    release_char_data,
    release_typeface,
    check_cmap_for_GID,
    NULL,     /* get_font_info */
    gs_fapi_bstm_set_mm_weight_vector
};

plugin_instantiation_proc(gs_fapibstm_instantiate);     /* check prototype */

int
gs_fapibstm_instantiate(i_plugin_client_memory * client_mem,
                        i_plugin_instance ** p_instance)
{
    int error;
    tsiMemObject *mem = NULL;
    fapi_bitstream_server *r;

    r = (fapi_bitstream_server *) client_mem->alloc(client_mem,
                                                    sizeof
                                                    (fapi_bitstream_server),
                                                    "fapi_bitstream_server");
    if (r == 0)
        return_error(gs_error_VMerror);
    memset(r, 0, sizeof(*r));
    r->If = If0;
    r->client_mem = *client_mem;
    *p_instance = &r->If.ig;

    return 0;
}

static void
gs_fapibstm_finit(i_plugin_instance * this, i_plugin_client_memory * mem)
{
    fapi_bitstream_server *r = (fapi_bitstream_server *) this;

    if (r->If.ig.d != &bitstream_descriptor)
        return;                 /* safety */
    mem->free(mem, r, "fapi_bitstream_server");
}
