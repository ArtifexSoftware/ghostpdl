/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* plht.c - shared halftone resource. */
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
#include "gxdevice.h"
#include "plht.h"

int
pl_set_pcl_halftone(gs_state *pgs, gs_mapping_proc transfer_proc,
                    int width, int height,
                    gs_string threshold_data,
                    int phase_x,
                    int phase_y)
{

    int code;
    gs_halftone ht;
    /* nothing to do for a contone device */
    if ( !gx_device_must_halftone(gs_currentdevice(pgs)) )
         return 0;
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
}
