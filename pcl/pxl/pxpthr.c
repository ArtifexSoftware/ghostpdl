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


/* pxpthr.c.c */
/* PCL XL passtrough mode */
#include "stdio_.h"             /* std.h + NULL */
#include "gsdevice.h"
#include "gstypes.h"
#include "gspath.h"
#include "gscoord.h"
#include "gsfont.h"
#include "gsstate.h"
#include "gsicc_manage.h"
#include "pcommand.h"
#include "pgmand.h"
#include "pcstate.h"
#include "pcfont.h"
#include "pcparse.h"
#include "pctop.h"
#include "pcpage.h"
#include "pcdraw.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxfont.h"
#include "pxgstate.h"
#include "pxpthr.h"
#include "pxparse.h"
#include "plfont.h"
#include "pjtop.h"
#include "pxptable.h"


/* forward decl */
void pxpcl_release(px_state_t* pxs);

void pxpcl_pagestatereset(px_state_t* pxs);

/* NB: tests for this function are used to flag pxl snippet mode
 */
static int
pcl_end_page_noop(pcl_state_t * pcs, int num_copies, int flush)
{
    return pxPassThrough;
}

/* set variables other than setting the page device that do not
   default to pcl reset values */
int
pxPassthrough_pcl_state_nonpage_exceptions(px_state_t * pxs)
{
    /* xl cursor -> pcl cursor position */
    gs_point xlcp, pclcp, dp;
    int code;

    /* make the pcl ctm active, after resets the hpgl/2 ctm is
       active. */
    code = pcl_set_graphics_state(pxs->pcs);
    if (code < 0) {
        return code;
    }

    /* xl current point -> device point -> pcl current
       point.  If anything fails we assume the current
       point is not valid and use the cap from the pcl
       state initialization - pcl's origin */
    if (gs_currentpoint(pxs->pgs, &xlcp) ||
        gs_transform(pxs->pgs, xlcp.x, xlcp.y, &dp) ||
        gs_itransform(pxs->pcs->pgs, dp.x, dp.y, &pclcp)) {
        pxs->pcs->cap.x = 0;
        pxs->pcs->cap.y = inch2coord(2.0 / 6.0);      /* 1/6" off by 2x in resolution. */
        if (gs_debug_c('i'))
            dmprintf2(pxs->memory,
                      "passthrough: changing cap NO currentpoint (%d, %d) \n",
                      pxs->pcs->cap.x, pxs->pcs->cap.y);
    } else {
        if (gs_debug_c('i'))
            dmprintf8(pxs->memory,
                      "passthrough: changing cap from (%d,%d) (%d,%d) (%d, %d) (%d, %d) \n",
                      pxs->pcs->cap.x, pxs->pcs->cap.y, (coord) xlcp.x,
                      (coord) xlcp.y, (coord) dp.x, (coord) dp.y,
                      (coord) pclcp.x, (coord) pclcp.y);
        pxs->pcs->cap.x = (coord) pclcp.x;
        pxs->pcs->cap.y = (coord) pclcp.y;
    }
    if (pxs->pcs->underline_enabled)
        pxs->pcs->underline_start = pxs->pcs->cap;


    pxs->char_angle = pxs->pxgs->char_angle;
    pxs->char_shear.x = pxs->pxgs->char_shear.x;
    pxs->char_shear.y = pxs->pxgs->char_shear.y;
    pxs->char_scale.x = pxs->pxgs->char_scale.x;
    pxs->char_scale.y = pxs->pxgs->char_scale.y;
    pxs->char_bold_value = pxs->pxgs->char_bold_value;

    return 0;
}

/* retrieve the current pcl state and initialize pcl */
static int
pxPassthrough_init(px_state_t * pxs)
{
    int code;

    if (gs_debug_c('i'))
        dmprintf(pxs->memory, "passthrough: initializing global pcl state\n");
    pxs->pcs = pcl_get_gstate(pxs->pcls);

    if (pxs->have_page) {
        if (gs_debug_c('i'))
            dmprintf(pxs->memory, "passthrough: snippet mode\n");
        /* disable an end page in pcl, also used to flag in snippet mode */
        pxs->pcs->end_page = pcl_end_page_noop;
    }

    /* default to pcl5c */
    pxs->pcs->personality = 0;
    /* for now we do not support intepolation in XL passthrough mode. */
    pxs->pcs->interpolate = false;
    /* we don't see a nice way to support the following options with
       passthrough at this time (NB) */
    pxs->pcs->page_set_on_command_line = false;
    pxs->pcs->res_set_on_command_line = false;
    pxs->pcs->high_level_device = false;
    pxs->pcs->scanconverter = GS_SCANCONVERTER_DEFAULT;

    {
        char buf[100];
        int ret;
        stream_cursor_read r;

        ret =
            gs_sprintf(buf,
                    "@PJL SET PAPERLENGTH = %d\n@PJL SET PAPERWIDTH = %d\n",
                    (int)(pxs->media_dims.y * 10 + .5),
                    (int)(pxs->media_dims.x * 10 + .5));
        r.ptr = (byte *) buf - 1;
        r.limit = (byte *) buf + ret - 1;
        pjl_proc_process(pxs->pjls, &r);
    }

    /* do an initial reset to set up a permanent reset.  The
       motivation here is to avoid tracking down a slew of memory
       leaks */
    pxs->pcs->xfm_state.paper_size = pcl_get_default_paper(pxs->pcs);
    pcl_do_resets(pxs->pcs, pcl_reset_initial);
    pcl_do_resets(pxs->pcs, pcl_reset_permanent);

    /* initialize pcl and install xl's page device in pcl's state */
    pcl_init_state(pxs->pcs, pxs->memory);
    code = gs_setdevice_no_erase(pxs->pcs->pgs, gs_currentdevice(pxs->pgs));
    if (code < 0)
        return code;

    /* yet another reset with the new page device */
    pxs->pcs->xfm_state.paper_size = pcl_get_default_paper(pxs->pcs);
    pcl_do_resets(pxs->pcs, pcl_reset_initial);
    /* set the parser state and initialize the pcl parser */
    pxs->pcl_parser_state.definitions = pxs->pcs->pcl_commands;
    pxs->pcl_parser_state.hpgl_parser_state = &pxs->gl_parser_state;
    pcl_process_init(&pxs->pcl_parser_state, pxs->pcs);
    /* default 600 to match XL allow PCL to override */
    pxs->pcs->uom_cp = 7200L / 600L;
    return gs_setgray(pxs->pcs->pgs, 0);
}

static int
pxPassthrough_setpagestate(px_state_t * pxs)
{
	int code = 0;

    /* by definition we are in "snippet mode" if pxl has dirtied
       the page */
    if (pxs->have_page) {
        if (gs_debug_c('i'))
            dmprintf(pxs->memory, "passthrough: snippet mode\n");
        /* disable an end page in pcl, also used to flag in snippet mode */
        pxs->pcs->end_page = pcl_end_page_noop;
        /* set the page size and orientation.  Really just sets
           the page tranformation does not feed a page (see noop
           above) */
        code = pcl_new_logical_page_for_passthrough(pxs->pcs,
                                             (int)pxs->orientation,
                                             &pxs->media_dims);

        if (gs_debug_c('i'))
            dmprintf2(pxs->memory,
                      "passthrough: snippet mode changing orientation from %d to %d\n",
                      pxs->pcs->xfm_state.lp_orient, (int)pxs->orientation);

    } else {                    /* not snippet mode - full page mode */
        /* pcl can feed the page and presumedely pcl commands will
           be used to set pcl's state. */
        pxs->pcs->end_page = pcl_end_page_top;
        /* clean the pcl page if it was marked by a previous snippet
           and set to full page mode. */
        pxs->pcs->page_marked = 0;
        code = pcl_new_logical_page_for_passthrough(pxs->pcs,
                                             (int)pxs->orientation,
                                             &pxs->media_dims);
        if (gs_debug_c('i'))
            dmprintf(pxs->memory, "passthrough: full page mode\n");
    }
    return code;
}

const byte apxPassthrough[] = { 0, 0 };

int
pxPassthrough(px_args_t * par, px_state_t * pxs)
{
    stream_cursor_read r;
    int code = 0;
    uint used;

    /* apparently if there is no open data source we open one.  By the
       spec this should already be open, in practice it is not. */
    if (!pxs->data_source_open) {
        if (gs_debug_c('i'))
            dmprintf(pxs->memory,
                     "passthrough: data source not open upon entry\n");
        pxs->data_source_open = true;
        pxs->data_source_big_endian = true;
    }

    /* source available is part of the equation to determine if this
       operator is being called for the first time */
    if (par->source.available == 0) {
        if (par->source.phase == 0) {
            if (gs_debug_c('i'))
                dmprintf(pxs->memory,
                         "passthrough starting getting more data\n");

            if (!pxs->pcs)
                pxPassthrough_init(pxs);

            /* this is the first passthrough on this page */
            if (pxs->pass_first) {
                code = pxPassthrough_setpagestate(pxs);
                if (code < 0)
                    return code;
                code = pxPassthrough_pcl_state_nonpage_exceptions(pxs);
                if (code < 0)
                    return code;
                pxs->pass_first = false;
            } else {
                /* there was a previous passthrough check if there were
                   any intervening XL commands */
                if (pxs->this_pass_contiguous == false) {
                    code = pxPassthrough_pcl_state_nonpage_exceptions(pxs);
                    if (code < 0)
                        return code;
                }
            }
            par->source.phase = 1;
        }
        return pxNeedData;
    }

    /* set pcl data stream pointers to xl's and process this batch of data. */
    r.ptr = par->source.data - 1;
    r.limit = par->source.data + par->source.available - 1;
    code = pcl_process(&pxs->pcl_parser_state, pxs->pcs, &r);
    /* updata xl's parser position to reflect what pcl has consumed. */
    used = (r.ptr + 1 - par->source.data);
    par->source.available -= used;
    par->source.data = r.ptr + 1;

    if (code < 0) {
        dmprintf1(pxs->memory, "passthrough: error return %d\n", code);
        return code;
    }
    /* always return need data and we exit at the top when the data is
       exhausted. */
    {
        if (used > px_parser_data_left(par->parser)) {
            dmprintf(pxs->memory, "error: read past end of stream\n");
            return -1;
        } else if (used < px_parser_data_left(par->parser)) {
            return pxNeedData;
        } else {
            /* end of operator and data */
            return 0;
        }
    }
}

void
pxpcl_pagestatereset(px_state_t* pxs)
{
    pxs->pass_first = true;
    if (pxs->pcs) {
        pxs->pcs->xfm_state.left_offset_cp = 0.0;
        pxs->pcs->xfm_state.top_offset_cp = 0.0;
        pxs->pcs->margins.top = 0;
        pxs->pcs->margins.left = 0;
    }
}

void
pxpcl_release(px_state_t * pxs)
{
    if (pxs->pcs) {
        if (gs_debug_c('i'))
            dmprintf(pxs->pcs->memory,
                     "passthrough: releasing global pcl state\n");
        pcl_grestore(pxs->pcs);
        gs_grestore_only(pxs->pcs->pgs);
        gs_nulldevice(pxs->pcs->pgs);
        pcl_do_resets(pxs->pcs, pcl_reset_permanent);
        pxs->pcs->end_page = pcl_end_page_top;        /* pcl_end_page handling */
        pxpcl_pagestatereset(pxs);
        pxs->pcs = NULL;
        pxs->this_pass_contiguous = false;
        pxs->pass_first = true;
        pxs->char_angle = 0;
        pxs->char_shear.x = 0;
        pxs->char_shear.y = 0;
        pxs->char_scale.x = 1.0;
        pxs->char_scale.y = 1.0;
        pxs->char_bold_value = 0.0;
    }
}

/* the pxl parser must give us this information */
void
pxpcl_passthroughcontiguous(px_state_t * pxs, bool cont)
{
    pxs->this_pass_contiguous = cont;
}

/* copy state from pcl to pxl after a non-snippet passthrough
 */
void
pxpcl_endpassthroughcontiguous(px_state_t * pxs)
{
    if (pxs->pcs->end_page == pcl_end_page_top &&
        pxs->pcs->page_marked &&
        pxs->orientation != pxs->pcs->xfm_state.lp_orient) {

        /* end of pcl whole job; need to reflect pcl orientation changes */
        pxs->orientation = pxs->pcs->xfm_state.lp_orient;
        pxBeginPageFromPassthrough(pxs);
    }

    pxs->pxgs->char_angle = pxs->char_angle;
    pxs->pxgs->char_shear.x = pxs->char_shear.x;
    pxs->pxgs->char_shear.y = pxs->char_shear.y;
    pxs->pxgs->char_scale.x = pxs->char_scale.x;
    pxs->pxgs->char_scale.y = pxs->char_scale.y;
    pxs->pxgs->char_bold_value = pxs->char_bold_value;
}
int
pxpcl_selectfont(px_args_t * par, px_state_t * pxs)
{
    int code;
    stream_cursor_read r;
    const px_value_t *pstr = par->pv[3];
    const byte *str = (const byte *)pstr->value.array.data;
    uint len = pstr->value.array.size;
    px_gstate_t *pxgs = pxs->pxgs;
    pcl_font_selection_t *pfp;

    if (!pxs->pcs)
        pxPassthrough_init(pxs);

    /* this is the first passthrough on this page */
    if (pxs->pass_first) {
        code = pxPassthrough_setpagestate(pxs);
        if (code < 0)
            return code;
        code = pxPassthrough_pcl_state_nonpage_exceptions(pxs);
        if (code < 0)
            return code;
        pxs->pass_first = false;
    } else {
        /* there was a previous passthrough check if there were
           any intervening XL commands */
        if (pxs->this_pass_contiguous == false) {
            code = pxPassthrough_pcl_state_nonpage_exceptions(pxs);
            if (code < 0)
                return code;
        }
    }
    r.ptr = str - 1;
    r.limit = str + len - 1;

    code = pcl_process(&pxs->pcl_parser_state, pxs->pcs, &r);
    if (code < 0)
        return code;

    code = pcl_recompute_font(pxs->pcs, false);       /* select font */
    if (code < 0)
        return code;

    code = gs_setfont(pxs->pgs, pxs->pcs->font->pfont);
    if (code < 0)
        return code;

    pfp = &pxs->pcs->font_selection[pxs->pcs->font_selected];

    {
#define CP_PER_INCH         (7200.0)
#define CP_PER_MM           (7200.0/25.4)
#define CP_PER_10ths_of_MM  (CP_PER_MM/10.0)

        static const double centipoints_per_measure[4] = {
            CP_PER_INCH,        /* eInch */
            CP_PER_MM,          /* eMillimeter */
            CP_PER_10ths_of_MM, /* eTenthsOfAMillimeter */
            1                   /* pxeMeasure_next, won't reach */
        };

        gs_point sz;

        pcl_font_scale(pxs->pcs, &sz);
        pxgs->char_size = sz.x /
            centipoints_per_measure[pxs->measure] * pxs->units_per_measure.x;
    }
    pxgs->symbol_set = pfp->params.symbol_set;

    if (pcl_downloaded_and_bound(pxs->pcs->font)) {
        pxgs->symbol_map = 0;
    } else {
        px_set_symbol_map(pxs, pxs->pcs->font->font_type == plft_16bit);
    }

    {
        pl_font_t *plf = pxs->pcs->font;

        /* unfortunately the storage identifier is inconsistent
           between PCL and PCL XL, NB we should use the pxfont.h
           enumerations pxfsInternal and pxfsDownloaded but there is a
           foolish type redefinition in that header that need to be
           fixed. */

        if (plf->storage == 4)
            plf->storage = 1;
        else
            plf->storage = 0;

        pxgs->base_font = (px_font_t *) plf;
    }
    pxgs->char_matrix_set = false;
    return 0;
}
