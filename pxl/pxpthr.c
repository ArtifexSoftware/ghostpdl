#include "stdio_.h"                     /* std.h + NULL */
#include "gsdevice.h"
#include "pcommand.h"
#include "pgmand.h"
#include "pcstate.h"
#include "pcparse.h"
#include "pctop.h"
#include "pcpage.h"
#include "pxoper.h"
#include "pxstate.h"

const byte apxPassthrough[] = {0, 0};

/* NB - what to do with this? */
pcl_state_t *global_pcs = NULL;
pcl_parser_state_t state;
hpgl_parser_state_t glstate;

void pxpcl_release(void);
void pxpcl_pagestatereset(void);


/* if this is a contiguous passthrough meaning that 2 passtrough
   operators have been given back to back and pxl should not regain
   control. */
bool global_this_pass_contiguous = false;

/* this is the first passthrough on this page */
bool global_pass_first = true;

bool global_first_time_this_operator = false;

private int
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

    /* make the pcl ctm is active - After resets the hpgl/2 ctm is
       active. */
    pcl_set_graphics_state(global_pcs);
    /* xl current point -> device point -> pcl current
       point.  If anything fails we assume the current
       point is not valid and use the cap from the pcl
       state initialization - pcl's origin */
    if ( (gs_currentpoint(pxs->pgs, &xlcp) >= 0) &&
         (gs_transform(pxs->pgs, xlcp.x, xlcp.y, &dp) >= 0) &&
         (gs_itransform(global_pcs->pgs, dp.x, dp.y, &pclcp) >= 0) ) {
        if (gs_debug_c('i'))
            dprintf4("passthrough: changing cap from (%d,%d) (%d,%d)\n",
                     global_pcs->cap.x, global_pcs->cap.y,
                     (coord)pclcp.x, (coord)pclcp.y);
        global_pcs->cap.x = (coord)pclcp.x;
        global_pcs->cap.y = (coord)pclcp.y;
    } else {
        /* NB not tested */
        global_pcs->cap.x = global_pcs->cap.y = 0;
    }

    /* NB fixme - should inherit xl's user units - assume 600 */
    global_pcs->uom_cp = 7200L / 600L;
}

/* retrieve the current pcl state and initialize pcl */
private int
pxPassthrough_init(px_state_t *pxs) 
{
    int code;
    if (gs_debug_c('i'))
        dprintf("passthrough: initalizing global pcl state\n");
    global_pcs = pcl_get_gstate(pxs->pcls);

    /* default to pcl5c */
    global_pcs->personality = 0;

    /* do an initial reset to set up a permanent reset.  The
       motivation here is to avoid tracking down a slew of memory
       leaks */
    pcl_do_resets(global_pcs, pcl_reset_initial);
    pcl_do_resets(global_pcs, pcl_reset_permanent);

    /* mark the page as blank and install xl's page device in
       pcl's state */
    global_pcs->page_marked = 0;
    code = gs_setdevice_no_erase(global_pcs->pgs, gs_currentdevice(pxs->pgs));
    if ( code < 0 )
        return code;

    /* yet another reset with the new page device */
    pcl_do_resets(global_pcs, pcl_reset_initial);
    /* set the parser state and initialize the pcl parser */
    state.definitions = global_pcs->pcl_commands;
    state.hpgl_parser_state=&glstate;
    pcl_process_init(&state);
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
        /* disable an end page in pcl */
        global_pcs->end_page = pcl_end_page_noop;
        /* set the page size and orientation.  Really just sets
           the page tranformation does not feed a page (see noop
           above) */
        new_logical_page_for_passthrough(global_pcs, (int)pxs->orientation, 2 /* letter NB FIXME */);

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
        new_logical_page_for_passthrough(global_pcs, (int)pxs->orientation, 2 /* letter NB FIXME */, 0);
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
    static global_passthru_op_starting = false;

    /* apparently if there is no open data source we open one.  By the
       spec this should already be open, in practice it is not. */
    if ( !pxs->data_source_open ) {
        if (gs_debug_c('i')) 
            dprintf("passtrough: data source not open upon entry\n");
        pxs->data_source_open = true;
        pxs->data_source_big_endian = true;
    }

    /* source available is part of the equation to determine if this
       operator is being called for the first time */
    if ( par->source.available == 0 ) {
        if (par->source.phase == 0) {
            if (gs_debug_c('i')) 
                dprintf("passtrough starting getting more data\n");

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
    code = pcl_process(&state, global_pcs, &r);
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

    }
}

/* the pxl parser must give us this information */
void
pxpcl_passthroughcontiguous(bool cont)
{
    global_this_pass_contiguous = cont;
}
