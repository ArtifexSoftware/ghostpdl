#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxtmap.h"
/* Define an abstract type for the PostScript graphics state. */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif
#include "gsstate.h"
#include "gxht.h"

int 
pl_set_pcl_halftone(gs_state *pgs, gs_mapping_proc transfer_proc,
                    int width, int height,
                    gs_const_string threshold_data,
                    int phase_x,
                    int phase_y)
{

    int code;
    gs_halftone ht;
    gs_settransfer(pgs, transfer_proc);
    ht.type = ht_type_threshold;
    ht.params.threshold.width = width;
    ht.params.threshold.height = height;
    ht.params.threshold.thresholds.data = threshold_data.data;
    ht.params.threshold.thresholds.size = threshold_data.size;
    ht.params.threshold.transfer = 0;
    ht.params.threshold.transfer_closure.proc = 0;
    code = gs_sethalftone(pgs, &ht);
    if ( code < 0 )
        return code;
    return gs_sethalftonephase(pgs, phase_x, phase_y);
    /*
     * Here is where we do the dreadful thing that appears to be
     * necessary to match the observed behavior of LaserJet 5 and
     * 6 printers with respect to superimposing halftoned source
     * and pattern.  NB needs review wrt later printers.
     */
    //    if ( code < 0 )
    //        return code;
    //    return gs_setscreenphase(pgs, px + source_phase_x,
    //                            py + source_phase_y, gs_color_select_source);
}
