#include "stdio_.h"			/* std.h + NULL */
#include "pcommand.h"
#include "pgmand.h"
#include "pcstate.h"
#include "pcparse.h"
#include "pxoper.h"
#include "pxstate.h"

const byte apxPassthrough[] = {0, 0};

/* NB - what to do with this? */
pcl_state_t *pcs = NULL;
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
    pcl_parser_state_t state;
    hpgl_parser_state_t glstate;
    stream_cursor_read r;
    int code = 0;
    
    if ( par->source.available == 0 )
        return pxNeedData;
    /* retrieve the current pcl state and initialize pcl */
    if ( !pcs ) {
        /* nb need to restore the old device */
        pcs = pcl_get_gstate(pxs->pcls);
        /* nb - if the page is marked by pxl redefine output page to
           no op - probably should disable all device calls from
           pcl */
        code = gs_setdevice_no_erase(pcs->pgs, gs_currentdevice(pxs->pgs));
        if ( code < 0 )
            return code;
        /* nb need to restore the old pcls old device */
        pcl_do_resets(pcs, pcl_reset_initial);
    }

    if ( pxs->have_page )
        pcs->end_page = pcl_end_page_noop;
    else
        pcs->end_page = pcl_end_page_top;

    state.definitions = pcs->pcl_commands;
    state.hpgl_parser_state=&glstate;
    pcl_process_init(&state);
    pcs->personality = 0; /* wrong */

    r.ptr = par->source.data - 1;
    r.limit = par->source.data + par->source.available - 1;
    code = pcl_process(&state, pcs, &r);
    par->source.data += par->source.available;
    if ( code < 0 )
        return code;
    return pxPassThrough;
}
