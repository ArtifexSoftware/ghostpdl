/* Portions Copyright (C) 2007 artofcode LLC.
   Portions Copyright (C) 2007 Artifex Software Inc.
   Portions Copyright (C) 2007 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pxpthr.c.c */
/* PCL XL passtrough mode */
#include "stdio_.h"                     /* std.h + NULL */
#include "gsdevice.h"
#include "gstypes.h"
#include "gspath.h"
#include "gscoord.h"
#include "gsstate.h"
#include "gsicc_manage.h"
#include "pcommand.h"
#include "pgmand.h"
#include "pcstate.h"
#include "pcparse.h"
#include "pctop.h"
#include "pcpage.h"
#include "pcdraw.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxgstate.h"
#include "pxpthr.h"
#include "pxparse.h"
#include "plfont.h"

const byte apxPassthrough[] = {0, 0};

/* NB - globals needing cleanup
 */
static pcl_state_t *global_pcs = NULL;
static pcl_parser_state_t global_pcl_parser_state;
static hpgl_parser_state_t global_gl_parser_state;

/* if this is a contiguous passthrough meaning that 2 passtrough
   operators have been given back to back and pxl should not regain
   control. */
static bool global_this_pass_contiguous = false;

/* this is the first passthrough on this page */
static bool global_pass_first = true;

/* store away the current font attributes PCL can't set these,
 * they persist for XL, rotation is missing */
gs_point global_char_shear;
gs_point global_char_scale;
float global_char_bold_value;

/* forward decl */
void pxpcl_release(void);
void pxpcl_pagestatereset(void);



/* NB: tests for this function are used to flag pxl snippet mode
 */
static int
pcl_end_page_noop(pcl_state_t *pcs, int num_copies, int flush)
{
    return pxPassThrough;
}

/* set variables other that setting the page device that do not
   default to pcl reset values */
void
pxPassthrough_pcl_state_nonpage_exceptions(px_state_t *pxs)
{
    /* xl cursor -> pcl cursor position */
    gs_point xlcp, pclcp, dp;
    int code1 = 0;
    int code2 = 0;
    int code3 = 0;

    /* make the pcl ctm active, after resets the hpgl/2 ctm is
       active. */
    pcl_set_graphics_state(global_pcs);
    /* xl current point -> device point -> pcl current
       point.  If anything fails we assume the current
       point is not valid and use the cap from the pcl
       state initialization - pcl's origin */
    if ((code1 = gs_currentpoint(pxs->pgs, &xlcp)) ||
        (code2 = gs_transform(pxs->pgs, xlcp.x, xlcp.y, &dp)) ||
        (code3 = gs_itransform(global_pcs->pgs, dp.x, dp.y, &pclcp)) ) {
        global_pcs->cap.x = 0;
        global_pcs->cap.y = inch2coord(2.0/6.0); /* 1/6" off by 2x in resolution. */
        if (gs_debug_c('i'))
            dprintf2("passthrough: changing cap NO currentpoint (%d, %d) \n",
                     global_pcs->cap.x, global_pcs->cap.y);
    } else {
      if (gs_debug_c('i'))
        dprintf8("passthrough: changing cap from (%d,%d) (%d,%d) (%d, %d) (%d, %d) \n",
                 global_pcs->cap.x, global_pcs->cap.y,
                 (coord)xlcp.x, (coord)xlcp.y,
                 (coord)dp.x, (coord)dp.y,
                 (coord)pclcp.x, (coord)pclcp.y);
      global_pcs->cap.x = (coord)pclcp.x;
      global_pcs->cap.y = (coord)pclcp.y;
    }
    if (global_pcs->underline_enabled)
        global_pcs->underline_start = global_pcs->cap;

    /* Hacked copy of font state; rotation doesn't copy? */
    if ( pxs->pxgs->char_shear.x || pxs->pxgs->char_shear.y ) {
        global_char_shear.x = pxs->pxgs->char_shear.x;
        global_char_shear.y = pxs->pxgs->char_shear.y;
    }
    if ( pxs->pxgs->char_scale.x != 1.0 || pxs->pxgs->char_scale.y != 1.0 ) {
        global_char_scale.x = pxs->pxgs->char_scale.x;
        global_char_scale.y = pxs->pxgs->char_scale.y;
    }
    if ( pxs->pxgs->char_bold_value > 0.001 )
        global_char_bold_value = pxs->pxgs->char_bold_value;

}

/* retrieve the current pcl state and initialize pcl */
static int
pxPassthrough_init(px_state_t *pxs)
{
    int code;

    if (gs_debug_c('i'))
        dprintf("passthrough: initializing global pcl state\n");
    global_pcs = pcl_get_gstate(pxs->pcls);

    /* default to pcl5c */
    global_pcs->personality = 0;
    /* for now we do not support intepolation in XL passthrough mode. */
    global_pcs->interpolate = false;

    /* do an initial reset to set up a permanent reset.  The
       motivation here is to avoid tracking down a slew of memory
       leaks */
    global_pcs->xfm_state.paper_size = pcl_get_default_paper(global_pcs); /* pxl paper ? */
    pcl_do_resets(global_pcs, pcl_reset_initial);
    pcl_do_resets(global_pcs, pcl_reset_permanent);

    /* mark the page as blank and install xl's page device in
       pcl's state */
    global_pcs->page_marked = 0;
    code = gs_setdevice_no_erase(global_pcs->pgs, gs_currentdevice(pxs->pgs));
    if ( code < 0 )
        return code;
    /* Also, check if the device profile was set int the global_pcs pgs.
       If not then initialize. Fix for seg fault with T427.BIN */
#ifdef ICCBRANCH
    gsicc_init_device_profile(global_pcs->pgs, gs_currentdevice(pxs->pgs));
#endif
    /* yet another reset with the new page device */
    pcl_do_resets(global_pcs, pcl_reset_initial);
    /* set the parser state and initialize the pcl parser */
    global_pcl_parser_state.definitions = global_pcs->pcl_commands;
    global_pcl_parser_state.hpgl_parser_state=&global_gl_parser_state;
    pcl_process_init(&global_pcl_parser_state);
    /* default 600 to match XL allow PCL to override */
    global_pcs->uom_cp = 7200L / 600L;
    return 0;
}

void
pxPassthrough_setpagestate(px_state_t *pxs)
{
    /* by definition we are in "snippet mode" if pxl has dirtied
       the page */
    if ( pxs->have_page ) {
        if (gs_debug_c('i'))
            dprintf("passthrough: snippet mode\n");
        /* disable an end page in pcl, also used to flag in snippet mode */
        global_pcs->end_page = pcl_end_page_noop;
        /* set the page size and orientation.  Really just sets
           the page tranformation does not feed a page (see noop
           above) */
        pcl_new_logical_page_for_passthrough(global_pcs, (int)pxs->orientation, 2 /* letter NB FIXME */);

        if (gs_debug_c('i'))
            dprintf2("passthrough: snippet mode changing orientation from %d to %d\n",
                     global_pcs->xfm_state.lp_orient, (int)pxs->orientation);

    } else { /* not snippet mode - full page mode */
        /* pcl can feed the page and presumedely pcl commands will
           be used to set pcl's state. */
        global_pcs->end_page = pcl_end_page_top;
        /* clean the pcl page if it was marked by a previous snippet
           and set to full page mode. */
        global_pcs->page_marked = 0;
        pcl_new_logical_page_for_passthrough(global_pcs, (int)pxs->orientation, 2 /* letter NB FIXME */);
        if (gs_debug_c('i'))
            dprintf("passthrough: full page mode\n");
    }
}

int
pxPassthrough(px_args_t *par, px_state_t *pxs)
{

    stream_cursor_read r;
    int code = 0;
    uint used;

    /* apparently if there is no open data source we open one.  By the
       spec this should already be open, in practice it is not. */
    if ( !pxs->data_source_open ) {
        if (gs_debug_c('i'))
            dprintf("passthrough: data source not open upon entry\n");
        pxs->data_source_open = true;
        pxs->data_source_big_endian = true;
    }

    /* source available is part of the equation to determine if this
       operator is being called for the first time */
    if ( par->source.available == 0 ) {
        if (par->source.phase == 0) {
            if (gs_debug_c('i'))
                dprintf("passthrough starting getting more data\n");

            if ( !global_pcs )
                pxPassthrough_init(pxs);

            /* this is the first passthrough on this page */
            if ( global_pass_first ) {
                pxPassthrough_setpagestate(pxs);
                pxPassthrough_pcl_state_nonpage_exceptions(pxs);
                global_pass_first = false;
            } else {
                /* there was a previous passthrough check if there were
                   any intervening XL commands */
                if ( global_this_pass_contiguous == false )
                    pxPassthrough_pcl_state_nonpage_exceptions(pxs);
            }
            par->source.phase = 1;
        }
        return pxNeedData;
    }

    /* NB cie does not work yet.  I am not sure if this state value
       should come from xl or pjl */
    global_pcs->useciecolor = 0;

    /* set pcl data stream pointers to xl's and process this batch of data. */
    r.ptr = par->source.data - 1;
    r.limit = par->source.data + par->source.available - 1;
    code = pcl_process(&global_pcl_parser_state, global_pcs, &r);
    /* updata xl's parser position to reflect what pcl has consumed. */
    used = (r.ptr + 1 - par->source.data);
    par->source.available -= used;
    par->source.data = r.ptr + 1;

    if ( code < 0 ) {
        dprintf1("passthrough: error return %d\n", code);
        return code;
    }
    /* always return need data and we exit at the top when the data is
       exhausted. */
    {
        if (used > px_parser_data_left(par->parser)) {
            dprintf("error: read past end of stream\n");
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
pxpcl_pagestatereset()
{
    global_pass_first = true;
    if ( global_pcs ) {
        global_pcs->xfm_state.left_offset_cp = 0.0;
        global_pcs->xfm_state.top_offset_cp = 0.0;
        global_pcs->margins.top = 0;
        global_pcs->margins.left = 0;
    }
}

void
pxpcl_release(void)
{
    if (global_pcs) {
        if (gs_debug_c('i'))
            dprintf("passthrough: releasing global pcl state\n");
        pcl_grestore(global_pcs);
        gs_grestore_only(global_pcs->pgs);
        gs_nulldevice(global_pcs->pgs);
        pcl_do_resets(global_pcs, pcl_reset_permanent);
        global_pcs = NULL;
        pxpcl_pagestatereset();
        global_this_pass_contiguous = false;
        global_pass_first = true;
        global_char_shear.x = 0;
        global_char_shear.y = 0;
        global_char_scale.x = 1.0;
        global_char_scale.y = 1.0;
        global_char_bold_value = 0.0;
    }
}

/* the pxl parser must give us this information */
void
pxpcl_passthroughcontiguous(bool cont)
{
    global_this_pass_contiguous = cont;
}

/* copy state from pcl to pxl after a non-snippet passthrough
 */
void
pxpcl_endpassthroughcontiguous(px_state_t *pxs)
{
    if ( global_pcs->end_page == pcl_end_page_top &&
         global_pcs->page_marked &&
         pxs->orientation != global_pcs->xfm_state.lp_orient ) {

        /* end of pcl whole job; need to reflect pcl orientation changes */
        pxs->orientation = global_pcs->xfm_state.lp_orient;
        pxBeginPageFromPassthrough(pxs);
    }

    pxs->pxgs->char_shear.x = global_char_shear.x;
    pxs->pxgs->char_shear.y = global_char_shear.y;
    pxs->pxgs->char_scale.x = global_char_scale.x;
    pxs->pxgs->char_scale.y = global_char_scale.y;
    pxs->pxgs->char_bold_value = global_char_bold_value;
}
int pxpcl_selectfont(px_args_t *par, px_state_t *pxs)
{
    int code;
    stream_cursor_read r;
    const px_value_t *pstr = par->pv[3];
    const byte *str = (const byte *)pstr->value.array.data;
    uint len = pstr->value.array.size;
    px_gstate_t *pxgs = pxs->pxgs;
    pcl_font_selection_t *pfp;
    float ppi;
    float scale;
    /* NB move these to a header file */
    extern void set_symbol_map(px_state_t *pxs, bool wide16);
    extern bool pcl_downloaded_and_bound(pl_font_t *plfont);
    if ( !global_pcs )
        pxPassthrough_init(pxs);

    /* this is the first passthrough on this page */
    if ( global_pass_first ) {
        pxPassthrough_setpagestate(pxs);
        pxPassthrough_pcl_state_nonpage_exceptions(pxs);
        global_pass_first = false;
    } else {
        /* there was a previous passthrough check if there were
           any intervening XL commands */
        if ( global_this_pass_contiguous == false )
            pxPassthrough_pcl_state_nonpage_exceptions(pxs);
    }
    r.ptr = str - 1;
    r.limit = str + len - 1;
    code = pcl_process(&global_pcl_parser_state, global_pcs, &r);

    pcl_recompute_font(global_pcs, false);       /* select font */
    code = gs_setfont(pxs->pgs, global_pcs->font->pfont);
    if (code < 0)
        return code;
    pfp = &global_pcs->font_selection[global_pcs->font_selected];
    ppi = (pfp->font->scaling_technology == plfst_Intellifont) ? 72.307 : 72.0;
    if (pfp->font->params.proportional_spacing)
    {
        scale = (pfp->params.height_4ths * 0.25) * pxs->units_per_measure.x / ppi;
    }
    else
    {
        scale = 1200.0 / (pfp->params.pitch.per_inch_x100 / 100.0) * (72.0 / ppi);

        /* hack for a scalable lineprinter font.  If a real
           lineprinter bitmap font is available it will be handled
           by the bitmap scaling case above */
        if (pfp->font->params.typeface_family == 0) {
            scale = 850.0;
        }
    }
    pxgs->char_size = scale;

    pxgs->symbol_set = pfp->params.symbol_set;
    {
        pl_font_t *plf = global_pcs->font;


        /* unfortunately the storage identifier is inconsistent
           between PCL and PCL XL, NB we should use the pxfont.h
           enumerations pxfsInternal and pxfsDownloaded but there is a
           foolish type redefinition in that header that need to be
           fixed. */

        if (plf->storage == 4)
            plf->storage = 1;
        else
            plf->storage = 0;

        pxgs->base_font = (px_font_t *)plf;
    }
    if (pcl_downloaded_and_bound(global_pcs->font)) {
        pxgs->symbol_map = 0;
    } else {
        set_symbol_map(pxs, global_pcs->font->font_type == plft_16bit);
    }
    pxgs->char_matrix_set = false;
    return 0;
}
