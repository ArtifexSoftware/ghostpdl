/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* plchar.c */
/* PCL font handling library -- operations on individual characters */
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"             /* for gdebug.h */
#include "gdebug.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gsimage.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsbittab.h"
#include "gxarith.h"            /* for igcd */
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "plvalue.h"
#include "gscoord.h"
#include "gsstate.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxpath.h"
/* We really shouldn't need the following, but currently they are needed */
/* for pgs->path and penum->log2_current_scale in pl_tt_build_char. */
#include "gxfixed.h"
#include "gxchar.h"
#include "gxfcache.h"
#include "gzstate.h"

/* agfa includes */
#undef true
#undef false
#undef frac_bits
#include "cgconfig.h"
#include "ufstport.h"
#include "shareinc.h"

#include "gxfapiu.h"
#include "plchar.h"

#if UFST_VERSION_MAJOR >= 6 && UFST_VERSION_MINOR >= 2
#undef true
#undef false

#define false FALSE
#define true TRUE
#define UNICODE UFST_UNICODE
#endif

/* ---------------- UFST utilities ---------------- */

#define UFST_SCALE  16
#define FXD_ONE     (1L << UFST_SCALE)  /* fixed point 1.0000 */

static const pl_font_t *plfont_last;    /* last selected font */
static const gs_matrix pl_identmtx = { identity_matrix_body };

extern IF_STATE if_state;

extern PIF_STATE pIFS;

/*
 * Set up a generic FONTCONTEXT structure.
 *
 * NB: UFST automatically inverts the y-axis when generating a bitmap, so
 *     it is necessary to account for that change in this routine.
 */
static void
pl_init_fc(const pl_font_t * plfont,
           gs_gstate * pgs,
           int need_outline, FONTCONTEXT * pfc, bool width_request)
{
    gs_font *pfont = plfont->pfont;
    /* set the current tranformation matrix - EM's... if this is a
       width request we don't necessarily have a current graphics
       state... use identity for resolution and ctm */
    gs_matrix mat;
    double xres, yres;

    if (width_request) {
        gs_make_identity(&mat);
        xres = yres = 1;
    } else {
        gs_currentmatrix(pgs, &mat);
        xres = gs_currentdevice(pgs)->HWResolution[0];
        yres = gs_currentdevice(pgs)->HWResolution[1];
    }
    pfc->font_id = 0;
    pfc->xspot = F_ONE;
    pfc->yspot = F_ONE;
    /* symbol set id used for no symbol set mapping */
    pfc->ssnum = 0x8000;
    /* filled in if downloaded font and/or symbol set. */
    pfc->font_hdr = NULL;
    pfc->dl_ssnum = 0;
    /* union selector for transformation type m0..m3 - pcl uses pt.
       size */
    pfc->fc_type = FC_MAT2_TYPE;
    /* calculate point size, set size etc based on current CTM in EM's */
    {
        double hx = hypot(mat.xx, mat.xy);
        double hy = hypot(mat.yx, mat.yy);
        /* fixed point scaling */
        double mscale = 1L << 16;

        pfc->s.m2.matrix_scale = 16;
        pfc->s.m2.point_size = (int)((hy * plfont->pts_per_inch / yres) + 0.5) * 8;     /* 1/8ths */
        pfc->s.m2.set_size =
            (int)((hx * plfont->pts_per_inch / xres) + 0.5) * 8;
        pfc->s.m2.m[0] = mscale * mat.xx / hx;
        pfc->s.m2.m[1] = mscale * -mat.xy / hx;
        pfc->s.m2.m[2] = mscale * mat.yx / hy;
        pfc->s.m2.m[3] = mscale * -mat.yy / hy;
        pfc->s.m2.world_scale = 16;
        pfc->s.m2.xworld_res = mscale * xres;
        pfc->s.m2.yworld_res = mscale * yres;
    }

    if (need_outline) {
        pfc->s.m2.m[1] = -pfc->s.m2.m[1];
        pfc->s.m2.m[3] = -pfc->s.m2.m[3];
    }

    /* always use our own symbol set mapping */
    pfc->ExtndFlags = EF_NOSYMSETMAP;
    if (plfont->is_xl_format) {
        pfc->ExtndFlags = EF_XLFONT_TYPE;
        if ((pfont->WMode & 0x1) != 0)  /* vertical substitution */
            pfc->ExtndFlags |= EF_VERTSUBS_TYPE;
    } else if (plfont->scaling_technology == plfst_TrueType
               && plfont->large_sizes) {
        pfc->ExtndFlags = EF_FORMAT16_TYPE | EF_GALLEYSEG_TYPE;
        if ((pfont->WMode & 0x1) != 0)  /* vertical substitution */
            pfc->ExtndFlags |= EF_VERTSUBS_TYPE;
    }
    pfc->ExtndFlags |= EF_NOUSBOUNDBOX; /* UFST 5.0+ addition */

    /* handle artificial emboldening */
    if (plfont->bold_fraction && !need_outline) {
        pfc->pcl6bold = 32768 * plfont->bold_fraction;
    } else
        pfc->pcl6bold = 0;
    /* set the format */
    pfc->format = FC_PCL6_EMU | FC_INCHES_TYPE;
    pfc->format |= (need_outline ? FC_LINEAR_TYPE : FC_BITMAP_TYPE);
}

/*
 * Set the current UFST font (any type).
 */
static int
pl_set_ufst_font(const pl_font_t * plfont, FONTCONTEXT * pfc)
{
    uint status = CGIFfont(FSA pfc);
    FONT_METRICS fm;

    if (status != 0)
        dmprintf1(plfont->pfont->memory, "CGIFfont error %d\n", status);
    else {
        /* UFST 6.2 *appears* to need this to "concretize" the font
         * we've just set.
         */
        status = CGIFfont_metrics(&fm);
        if (status != 0)
            dprintf1("CGIFfont error %d\n", status);
        else
            plfont_last = plfont;       /* record this font for use in call-backs */
    }

    return status;
}

/*
 * Set the current path from a character outline. This is more general than
 * may be necessary, depending on how the UFST module is compiled.
 */
static int
image_outline_char(PIFOUTLINE pols,
                   const gs_matrix_fixed * pmat,
                   gx_path * ppath,
                   gs_font * pfont, bool outline_sub_for_bitmap)
{
    UW16 il, numLoops = pols->ol.num_loops;
    byte *pbase = (byte *) & pols->ol.loop;
    int ishift = fixed_fraction_bits + pols->VLCpower;
    fixed tx = pmat->tx_fixed, ty = pmat->ty_fixed;

    for (il = 0; il < numLoops; il++) {
        OUTLINE_LOOP *ploop = &pols->ol.loop[il];
        uint numSegmts = ploop->num_segmts;
        byte *pseg = pbase + ploop->segmt_offset;
        PINTRVECTOR pcoord = (PINTRVECTOR) (pbase + ploop->coord_offset);
        int code;

        while (numSegmts-- > 0) {
            int segtype = *pseg++;
            int ip, npts;
            gs_fixed_point pt[3];

            if (segtype == 2 || segtype > 3)
                return_error(gs_error_rangecheck);

            npts = (segtype == 3 ? 3 : 1);
            for (ip = 0; ip < npts; ip++, ++pcoord) {
                pt[ip].x = (pcoord->x << ishift) + tx;
                /* apparently when the ufst substitutes an outline for
                   a bitmap because it is too large the matrix is
                   mirrored compared with a font that was rendered
                   directly with an outline. NB, this should be
                   looked at more carefully when time permits. */
                if (outline_sub_for_bitmap)
                    pt[ip].y = (-pcoord->y << ishift) + ty;
                else
                    pt[ip].y = (pcoord->y << ishift) + ty;
            }

            switch (segtype) {

                case 0:        /* moveto */
                    code = gx_path_add_point(ppath, pt[0].x, pt[0].y);
                    break;

                case 1:        /* lineto */
                    code = gx_path_add_line(ppath, pt[0].x, pt[0].y);
                    break;

                case 3:        /* curveto */
                    code = gx_path_add_curve(ppath,
                                             pt[0].x, pt[0].y,
                                             pt[1].x, pt[1].y,
                                             pt[2].x, pt[2].y);
            }
            if (code < 0)
                return code;
        }
        if ((code = gx_path_close_subpath(ppath)) < 0)
            return code;
    }
    return 0;
}

/*
 * Get the widt from a UFST character (any type). The caller should have
 * set the font type in advance.
 */
static int
pl_ufst_char_width(uint char_code,
                   const void *pgs, gs_point * pwidth, FONTCONTEXT * pfc)
{

    UW16 chIdloc = char_code;
    UW16 fontWidth[2];
    int status;
    WIDTH_LIST_INPUT_ENTRY fcode;

    if (pwidth != NULL)
        pwidth->x = pwidth->y = 0;

    CGIFchIdptr(FSA(VOID *) & chIdloc, NULL);
    fcode.CharType.TT_unicode = char_code;
    if ((status = CGIFwidth2(FSA & fcode, 1, 4, fontWidth)) != 0) {
        return status;
    }
    if (fontWidth[0] == ERR_char_unavailable || fontWidth[1] == 0)
        return 1;
    else if (pwidth != NULL) {
        double fontw = (double) fontWidth[0] / (double) fontWidth[1];
        int code = gs_distance_transform(fontw, 0.0, &pl_identmtx, pwidth);

        return code < 0 ? code : 0;
    } else
        return 0;
}

/*
 * Generate a UFST character.
 */
static int
pl_ufst_make_char(gs_show_enum * penum,
                  gs_gstate * pgs,
                  gs_font * pfont, gs_char chr, FONTCONTEXT * pfc)
{
    MEM_HANDLE memhdl;
    UW16 status, chIdloc = chr;
    gs_matrix sv_ctm, tmp_ctm;
    bool outline_sub_for_bitmap = false;
    int wasValid;

    /* ignore illegitimate characters */
    if (chr == 0xffff)
        return 0;

    CGIFchIdptr(FSA(VOID *) & chIdloc, NULL);
    if ((status = CGIFchar_handle(FSA chr, &memhdl, 0)) != 0 &&
        status != ERR_fixed_space) {

        /* if too large for a bitmap, try an outline */
        if ((status >= ERR_bm_gt_oron && status <= ERRdu_pix_range) ||
            (status == ERR_bm_buff)) {
            pfc->format = (pfc->format & ~FC_BITMAP_TYPE) | FC_CUBIC_TYPE;
            if ((status = CGIFfont(FSA pfc)) == 0) {
                CGIFchIdptr(FSA(VOID *) & chIdloc, NULL);
                status = CGIFchar_handle(FSA chr, &memhdl, 0);
                outline_sub_for_bitmap = true;
            }
        }
        if (status != 0) {
            /* returning status causes the job to be aborted */
            return gs_setcharwidth(penum, pgs, 0.0, 0.0);
        }
    }

    wasValid = pgs->char_tm_valid;
    /* move to device space */
    gs_currentmatrix(pgs, &sv_ctm);
    gs_make_identity(&tmp_ctm);
    tmp_ctm.tx = sv_ctm.tx;
    tmp_ctm.ty = sv_ctm.ty;
    gs_setmatrix(pgs, &tmp_ctm);
    pgs->char_tm_valid = wasValid;

    if (FC_ISBITMAP(pfc)) {
        PIFBITMAP psbm = (PIFBITMAP) MEMptr(memhdl);
        float wbox[6];
        gs_image_t image;
        gs_image_enum *ienum;
        int code;
        gs_point aw;

        /* set up the cache device */
        gs_distance_transform((double) psbm->escapement / psbm->du_emx,
                              0.0, &sv_ctm, &aw);

        wbox[0] = aw.x;
        wbox[1] = aw.y;
        wbox[2] = psbm->xorigin / 16.0 + psbm->left_indent;
        wbox[3] = -psbm->yorigin / 16.0 + psbm->top_indent;
        wbox[4] = psbm->black_width + wbox[2];
        wbox[5] = psbm->black_depth + wbox[3];

        /* if (status == ERR_fixed_space)
         *   we are relying on ufst to
         *   send a zero sized image; we then cache the escapements of the space character
         *   psbm->bm = psbm->width = psbm->height = 0;
         *   note that the outline code can't be reached on ERR_fixed_space
         */

        if ((code = gs_setcachedevice(penum, pgs, wbox)) < 0) {
            MEMfree(FSA CACHE_POOL, memhdl);
            gs_setmatrix(pgs, &sv_ctm);
            return code;
        }

        /* set up the image */
        ienum = gs_image_enum_alloc(pgs->memory, "pl_ufst_make_char");
        if (ienum == 0) {
            MEMfree(FSA CACHE_POOL, memhdl);
            gs_setmatrix(pgs, &sv_ctm);
            return_error(gs_error_VMerror);
        }
        gs_image_t_init_mask(&image, true);
        image.Width = psbm->width << 3;
        image.Height = psbm->depth;
        gs_make_identity(&image.ImageMatrix);
        image.ImageMatrix.tx = -psbm->xorigin / 16.0;
        image.ImageMatrix.ty = psbm->yorigin / 16.0;
        image.adjust = true;
        code = pl_image_bitmap_char(ienum,
                                    &image,
                                    (byte *) psbm->bm,
                                    psbm->width, 0, NULL, pgs);
        gs_free_object(pgs->memory, ienum, "pl_ufst_make_char");
        MEMfree(FSA CACHE_POOL, memhdl);
        gs_setmatrix(pgs, &sv_ctm);
        return (code < 0 ? code : 0);

    } else {                    /* outline */
        PIFOUTLINE pols = (PIFOUTLINE) MEMptr(memhdl);
        float scale = pow(2, pols->VLCpower);
        float wbox[6];
        int code;
        gs_point aw;

        /* set up the cache device */
        gs_distance_transform((double) pols->escapement / pols->du_emx,
                              0.0, &sv_ctm, &aw);
        wbox[0] = aw.x;
        wbox[1] = aw.y;
        wbox[2] = scale * pols->left;
        wbox[3] = scale * pols->bottom;
        wbox[4] = scale * pols->right;
        wbox[5] = scale * pols->top;

        if (status == ERR_fixed_space) {
            MEMfree(FSA CACHE_POOL, memhdl);
            code = gs_setcharwidth(penum, pgs, wbox[0], wbox[1]);
            gs_setmatrix(pgs, &sv_ctm);
            return code;
        } else if ((code = gs_setcachedevice(penum, pgs, wbox)) < 0) {
            MEMfree(FSA CACHE_POOL, memhdl);
            gs_setmatrix(pgs, &sv_ctm);
            return code;
        }

        code =
            image_outline_char(pols, &pgs->ctm, pgs->path, pfont,
                               outline_sub_for_bitmap);
        if (code >= 0) {
            code = gs_fill(pgs);
        }
        MEMfree(FSA CACHE_POOL, memhdl);
        gs_setmatrix(pgs, &sv_ctm);
        return (code < 0 ? code : 0);
    }
}

/* ---------------- MicroType font support ---------------- */
/*
 * MicroType accepts unicode values a glyph identifiers, so no explicit
 * encoding is necessary.
 */
static gs_glyph
pl_mt_encode_char(gs_font * pfont, gs_char pchr, gs_glyph_space_t not_used)
{
    return (gs_glyph) pchr;
}

/*
 * Set the current UFST font to be a MicroType font.
 */
static int
pl_set_mt_font(gs_gstate * pgs,
               const pl_font_t * plfont, int need_outline, FONTCONTEXT * pfc)
{
    pl_init_fc(plfont, pgs, need_outline, pfc, /* width request iff */
               pgs == NULL);
    pfc->font_id = ((gs_font_base *) (plfont->pfont))->UID.id;
#ifdef UFST_FROM_ROM
    pfc->format |= FC_ROM_TYPE | FC_NOUSBOUNDBOX;
#endif
    pfc->format |= FC_FCO_TYPE;
    return pl_set_ufst_font(plfont, pfc);
}

/* Render a MicroType character. */
static int
pl_mt_build_char(gs_show_enum * penum,
                 gs_gstate * pgs, gs_font * pfont, gs_char chr, gs_glyph glyph)
{
    const pl_font_t *plfont = (const pl_font_t *)pfont->client_data;
    FONTCONTEXT fc;

    if (pl_set_mt_font(pgs, plfont, gs_show_in_charpath(penum), &fc) != 0)
        return 0;
    return pl_ufst_make_char(penum, pgs, pfont, chr, &fc);
}

#define MAX_LIST_SIZE 100
int list_size = 0;

typedef struct pl_glyph_width_node_s pl_glyph_width_node_t;

struct pl_glyph_width_node_s
{
    uint char_code;
    uint font_id;
    gs_point width;
    pl_glyph_width_node_t *next;
};

pl_glyph_width_node_t *head = NULL;

/* add at the front of the list */

static int
pl_glyph_width_cache_node_add(gs_memory_t * mem, gs_id font_id,
                              uint char_code, gs_point * pwidth)
{
    pl_glyph_width_node_t *node =
        (pl_glyph_width_node_t *) gs_alloc_bytes(mem,
                                                 sizeof
                                                 (pl_glyph_width_node_t),
                                                 "pl_glyph_width_cache_node_add");

    if (node == NULL)
        return -1;
    if (head == NULL) {
        head = node;
        head->next = NULL;
    } else {
        node->next = head;
        head = node;
    }

    head->char_code = char_code;
    head->font_id = font_id;
    head->width = *pwidth;
    list_size++;
    return 0;
}

static int
pl_glyph_width_cache_node_search(gs_id font_id, uint char_code,
                                 gs_point * pwidth)
{
    pl_glyph_width_node_t *current = head;

    while (current) {
        if (char_code == current->char_code && font_id == current->font_id) {
            *pwidth = current->width;
            return 0;
        }
        current = current->next;
    }
    return -1;
}

static void
pl_glyph_width_list_remove(gs_memory_t * mem)
{
    pl_glyph_width_node_t *current = head;

    while (current) {
        pl_glyph_width_node_t *next = current->next;

        gs_free_object(mem, current, "pl_glyph_width_list_remove");
        current = next;
    }
    head = NULL;
    list_size = 0;
    return;
}

/* Get character existence and escapement for an MicroType font. */
static int
pl_mt_char_width(const pl_font_t * plfont,
                 const void *pgs, uint char_code, gs_point * pwidth)
{
    FONTCONTEXT fc;
    int code;

    if (list_size > MAX_LIST_SIZE)
        pl_glyph_width_list_remove(plfont->pfont->memory);
    code =
        pl_glyph_width_cache_node_search(plfont->pfont->id, char_code,
                                         pwidth);
    if (code < 0) {             /* not found */
        /* FIXME inconsistent error code return values follow */
        if (pl_set_mt_font(NULL /* graphics state */ , plfont, false, &fc) !=
            0)
            return 0;
        code = pl_ufst_char_width(char_code, pgs, pwidth, &fc);
        if (code == 0)
            code = pl_glyph_width_cache_node_add(plfont->pfont->memory,
                                                 plfont->pfont->id,
                                                 char_code, pwidth);
    }
    return code;
}

static int
pl_mt_char_metrics(const pl_font_t * plfont, const void *pgs, uint char_code,
                   float metrics[4])
{
    gs_point width;
    metrics[0] = metrics[1] = metrics[2] = metrics[3] = 0;
    if (0 == pl_mt_char_width(plfont, pgs, char_code, &width)) {
        /* width is correct,
           stefan foo: lsb is missing. */
        metrics[2] = width.x;
        /* metrics[0] = left_side_bearing;
         */
    }
    return 0;
}

/*
 * Callback from UFST to pass PCLEO IF character data starting with header.
 *
 * For TrueType fonts, the glyph table within a font is indexed by the glyph
 * id., rather than the unicode. The char_glyphs table in the font maps the
 * latter to the former.
 */
static LPUB8
pl_PCLchId2ptr(FSP UW16 chId)
{
    const pl_font_t *plfont = plfont_last;

    if (plfont_last == NULL)
        return NULL;            /* something wrong */

    /* check for a TrueType font */
    if (plfont->char_glyphs.table != NULL) {
        pl_tt_char_glyph_t *ptcg = pl_tt_lookup_char(plfont, chId);

        if (ptcg->chr == gs_no_char)
            return NULL;        /* something wrong */
        chId = ptcg->glyph;
    }
    return (LPUB8) pl_font_lookup_glyph(plfont, chId)->data;
}

/*
 * callback from UFST to pass PCLEO TT character data starting with header.
 */
static LPUB8
pl_PCLglyphID2Ptr(FSP UW16 chId)
{
    if (plfont_last == NULL)
        return NULL;            /* something wrong */
    else
        return (LPUB8) (pl_font_lookup_glyph(plfont_last, chId)->data);
}

/*
 * callback from UFST to pass PCLEO compound character data starting
 * with header.
 */
static LPUB8
pl_PCLEO_charptr(LPUB8 pfont_hdr, UW16 char_code)
{
    if (plfont_last == NULL || plfont_last->header != pfont_hdr) {
        return NULL;            /* something wrong */
    } else
        return pl_PCLchId2ptr(FSA char_code);
}

void
plu_set_callbacks()
{
    gx_set_UFST_Callbacks(pl_PCLEO_charptr, pl_PCLchId2ptr,
                          pl_PCLglyphID2Ptr);
    /* nothing */
}

void
pl_mt_init_procs(gs_font_base * pfont)
{
    pfont->procs.encode_char = pl_mt_encode_char;
    pfont->procs.build_char = pl_mt_build_char;
#define plfont ((pl_font_t *)pfont->client_data)
    plfont->char_width = pl_mt_char_width;
    plfont->char_metrics = pl_mt_char_metrics;
#undef plfont
}
