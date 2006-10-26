#include "stdio_.h"			/* std.h + NULL */
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

private int
pcl_end_page_noop(pcl_state_t *pcs, int num_copies, int flush)
{
    return pxPassThrough;
}
    
int
pxPassthrough(px_args_t *par, px_state_t *pxs)
{

    /* NB we create a new parser state each time */
    /* NB needs a pxl gsave and grestore */
    stream_cursor_read r;
    int code = 0;
    pcl_state_t *pcs = global_pcs;  /* alias global_pcs */
  
    /* apparently if there is no open data source we open one */
    if ( !pxs->data_source_open ) {
        pxs->data_source_open = true;
	pxs->data_source_big_endian = true;
    }
    if ( par->source.available == 0 )
        return pxNeedData;
    /* retrieve the current pcl state and initialize pcl */
    if ( !pcs ) {
        global_pcs = pcs = pcl_get_gstate(pxs->pcls);
        pcs->personality = 0; /* default to pcl5c */
        /* do an initial reset to set up a permanent reset.  The
           permanent reset will fail if some common resources
           (i.e. font dictionary are not in the state */
        pcl_do_resets(pcs, pcl_reset_initial);
        pcl_do_resets(pcs, pcl_reset_permanent);
        /* mark the page as blank and install xl's page device in pcl's state */
        pcs->page_marked = 0;
        code = gs_setdevice_no_erase(pcs->pgs, gs_currentdevice(pxs->pgs));
        if ( code < 0 )
            return code;
        /* now reset with the new page device and the reset permanent "clean slate" */
        pcl_do_resets(pcs, pcl_reset_initial);
        /* if pxs has dirtied the page - snippet mode */
        if ( pxs->have_page ) {
            /* set the page size and orientation.  NB FIXME - ignores return val. */
            new_logical_page_for_passthrough_snippet(pcs, (int)pxs->orientation, 2 /* letter NB FIXME */);
            /* set the cap */
            {
                gs_point xlcp, pclcp, dp;
                /* this just guarantees the pcl ctm is right.  After
                   resets the hpgl/2 ctm is active */
                pcl_set_graphics_state(pcs);
                /* xl current point -> device point -> pcl current
                   point.  If anything fails we assume the current
                   point is not valid and use the cap from the pcl
                   state initialization - pcl's origin */
                if ( (gs_currentpoint(pxs->pgs, &xlcp) >= 0) &&
                     (gs_transform(pxs->pgs, xlcp.x, xlcp.y, &dp) >= 0) &&
                     (gs_itransform(pcs->pgs, dp.x, dp.y, &pclcp) >= 0) ) {
                    pcs->cap.x = (coord)pclcp.x;
                    pcs->cap.y = (coord)pclcp.y;
                }
                /* NB fixme - should inherit xl's user units - assume 600 */
                pcs->uom_cp = 7200L / 600L;
            }
            pcs->end_page = pcl_end_page_noop;
        }
        else
            pcs->end_page = pcl_end_page_top;
        
        state.definitions = pcs->pcl_commands;
        state.hpgl_parser_state=&glstate;
        pcl_process_init(&state);
    }
    /* NB cie does not work yet */
    pcs->useciecolor = 0;
    r.ptr = par->source.data - 1;
    r.limit = par->source.data + par->source.available - 1;
    code = pcl_process(&state, pcs, &r);
    par->source.available -= (r.ptr + 1 - par->source.data);
    par->source.data = r.ptr + 1;
    if ( code < 0 )
        return code;
    return pxPassThrough;
}

void
pxpcl_release(void)
{
    if (global_pcs) {
        pcl_grestore(global_pcs);
        gs_grestore_only(global_pcs->pgs);
	gs_nulldevice(global_pcs->pgs);
        pcl_do_resets(global_pcs, pcl_reset_permanent);
        global_pcs = NULL;
    }
}
