/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* GS lite initialization and setup */

#include <stdlib.h>
#include "stdio_.h"
#include "math_.h"
#include "string_.h"

#include "gp.h"
#include "gscdefs.h"
#include "gserrors.h"
#include "gslib.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsutil.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxpath.h"

#include "gslt.h"

/* Include the extern for the device list. */
extern_gs_lib_device_list();

init_proc(gs_iodev_init);

gs_memory_t *
gslt_init_library()
{
    gs_memory_t *mem;

    /* A simple allocator to avoid the complications of the full
     * featured memory allocator
     */
    mem = gslt_alloc_init();
    gp_init();
    gs_lib_init1(mem);

    /* Debugging flags
     */
    memset(gs_debug, 0, 128);
    gs_debug['@'] = 1;
    gs_debug['?'] = 1;
    gs_log_errors = 1; /* print error returns */

    /* gs_iodev_init must be called after the rest of the inits, for
     * obscure reasons that really should be documented!
     */
    gs_iodev_init(mem);

    return mem;
}

gx_device *
gslt_init_device(gs_memory_t *mem, char *name)
{
    const gx_device *const *list;
    gx_device *dev;
    int count, i;

    count = gs_lib_device_list(&list, NULL);
    for (i = 0; i < count; ++i)
    {
        if (!strcmp(gs_devicename(list[i]), name))
        {
            gs_copydevice(&dev, list[i], mem);
            // stefan foo: pulled for linking to gs 8.13
            //check_device_separable(dev);
            gx_device_fill_in_procs(dev); /* really? i thought gs_setdevice_no_erase did that. */
            return dev;
        }
    }

    lprintf1("device %s not found\n", name);
    exit(1);

    return NULL;
}

void
gslt_get_device_param(gs_memory_t *mem, gx_device *dev, char *key)
{
    gs_c_param_list list;
    gs_param_string nstr;
    int code;

    gs_c_param_list_write(&list, mem);
    code = gs_getdeviceparams(dev, (gs_param_list *) & list);
    if (code < 0)
    {
        lprintf1("getdeviceparams failed! code = %d\n", code);
        exit(1);
    }

    gs_c_param_list_read(&list);
    code = param_read_string((gs_param_list *) & list, key, &nstr);
    if (code < 0)
    {
        lprintf1("reading device parameter %s failed\n", key);
        exit(1);
    }

    dprintf1("device %s = ", key);
    debug_print_string(nstr.data, nstr.size);
    dputs("\n");
    gs_c_param_list_release(&list);
}

void
gslt_set_device_param(gs_memory_t *mem, gx_device *dev, char *key, char *val)
{
    gs_c_param_list list;
    gs_param_string nstr;
    int code;

    gs_c_param_list_write(&list, mem);
    param_string_from_string(nstr, val);
    code = param_write_string((gs_param_list *)&list, key, &nstr);
    if (code < 0)
    {
        lprintf1("writing device parameter %s failed\n", key);
        exit(1);
    }

    gs_c_param_list_read(&list);
    code = gs_putdeviceparams(dev, (gs_param_list *)&list);
    gs_c_param_list_release(&list);
    if (code < 0 && code != gs_error_undefined)
    {
        lprintf1("putdeviceparams failed! code = %d\n", code);
        exit(1);
    }
}

gs_state *gslt_init_state(gs_memory_t *mem, gx_device *dev)
{
    gs_state *pgs;

    pgs = gs_state_alloc(mem);
    gs_setdevice_no_erase(pgs, dev); /* set device, but don't erase */

    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    gs_gsave(pgs);

    return pgs;
}

void gslt_free_state(gs_memory_t *mem, gs_state *pgs)
{
    gs_state_free(pgs);
}

void gslt_free_device(gs_memory_t *mem, gx_device *dev)
{
    gs_free_object(mem, dev, "gslt device"); // TODO: how do i free devices?
}

void gslt_free_library(gs_memory_t *mem)
{
    // TODO: how do i free the allocator?
}

#if 0

int
main(int argc, const char *argv[])
{
    gs_memory_t *mem;
    gx_device *dev;
    gs_state *pgs;

    mem = gslt_init_library();
    dev = gslt_init_device(mem, "x11");
    pgs = gslt_init_state(mem, dev);

    gslt_get_device_param(mem, dev, "Name");
    gslt_set_device_param(mem, dev, "OutputFile", "-");

    gs_erasepage(pgs);

    gs_moveto(pgs, 10, 10);
    gs_lineto(pgs, 20, 50);
    gs_stroke(pgs);

    gs_output_page(pgs, 1, 1);

    fgetc(mem->gs_lib_ctx->fstdin);

    gslt_free_library(mem, dev, pgs);
    return 0;
}

#endif
