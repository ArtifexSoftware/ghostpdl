/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* idisp.c */

/*
 * This allows the interpreter to set the callback structure
 * of the "display" device.  This must set at the end of
 * initialization and before the device is used.
 * This is harmless if the 'display' device is not included.
 * If gsapi_set_display_callback() is not called, this code
 * won't even be used.
 */

#include "stdio_.h"
#include "stdpre.h"
#include "iapi.h"
#include "ghost.h"
#include "gp.h"
#include "gscdefs.h"
#include "gsmemory.h"
#include "gstypes.h"
#include "gsdevice.h"
#include "iref.h"
#include "imain.h"
#include "iminst.h"
#include "oper.h"
#include "ostack.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "idisp.h"
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gdevdsp.h"
#include "gdevdsp2.h"
#include "gxgstate.h"
#include "gxdevsop.h"

int
reopen_device_if_required(gs_main_instance *minst)
{
    i_ctx_t *i_ctx_p;
    int code;
    gx_device *dev;

    i_ctx_p = minst->i_ctx_p;	/* run_string may change i_ctx_p if GC */
    dev = gs_currentdevice_inline(i_ctx_p->pgs);
    if (dev == NULL)
        /* This can happen if we invalidated devices on the stack by calling nulldevice after they were pushed */
        return_error(gs_error_undefined);

    if (!dev->is_open)
        return 0;

    if (dev_proc(dev, dev_spec_op)(dev, gxdso_reopen_after_init, NULL, 0) != 1)
       return 0;

    code = gs_closedevice(dev);
    if (code < 0)
        return code;

    code = gs_opendevice(dev);
    if (code < 0) {
        dmprintf(dev->memory, "**** Unable to reopen the device, quitting.\n");
        return code;
    }
    return 0;
}
