/* Copyright (C) 2001-2018 Artifex Software, Inc.
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


/* plht.c - shared halftone resource. */
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxtmap.h"
#include "gsstate.h"
#include "gxht.h"
#include "gxdevice.h"
#include "plht.h"

int
pl_set_pcl_halftone(gs_gstate * pgs, gs_mapping_proc transfer_proc,
                    int width, int height,
                    gs_string threshold_data, int phase_x, int phase_y)
{

    int code;
    gs_halftone ht;

    /* nothing to do for a contone device */
    if (!gx_device_must_halftone(gs_currentdevice(pgs)))
        return 0;
    code = gs_settransfer(pgs, transfer_proc);
    if (code < 0)
        return code;
    ht.type = ht_type_threshold;
    ht.params.threshold.width = width;
    ht.params.threshold.height = height;
    ht.params.threshold.thresholds.data = threshold_data.data;
    ht.params.threshold.thresholds.size = threshold_data.size;
    ht.params.threshold.transfer = 0;
    ht.params.threshold.transfer_closure.proc = 0;
    code = gs_sethalftone(pgs, &ht);
    if (code < 0)
        return code;
    return gs_sethalftonephase(pgs, phase_x, phase_y);
}
