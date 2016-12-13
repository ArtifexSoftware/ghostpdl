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


/* pltop.c */
/* Top-level API for interpreters */

#include "string_.h"
#include "gdebug.h"
#include "gsnogc.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsdevice.h"
#include "pltop.h"

/* Get implemtation's characteristics */
const pl_interp_characteristics_t *     /* always returns a descriptor */
pl_characteristics(const pl_interp_implementation_t * impl      /* implementation of interpereter to alloc */
    )
{
    return impl->proc_characteristics(impl);
}

/* Do init of interp */
int                             /* ret 0 ok, else -ve error code */
pl_allocate_interp(pl_interp_t ** interp,       /* RETURNS abstract interpreter struct */
                   const pl_interp_implementation_t * impl,     /* implementation of interpereter to alloc */
                   gs_memory_t * mem    /* allocator to allocate interp from */
    )
{
    int code = impl->proc_allocate_interp(interp, impl, mem);

    if (code < 0)
        return code;
    (*interp)->implementation = impl;
    return code;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
int                             /* ret 0 ok, else -ve error code */
pl_allocate_interp_instance(pl_interp_instance_t ** instance,   /* RETURNS instance struct */
                            pl_interp_t * interp,       /* dummy interpreter */
                            gs_memory_t * mem   /* allocator to allocate instance from */
    )
{
    pl_interp_instance_t *pli;

    int code =
        interp->implementation->proc_allocate_interp_instance(instance,
                                                              interp, mem);
    if (code < 0)
        return code;
    pli = *instance;
    pli->interp = interp;

    return code;
}

/* Set an interpreter instance's pre-page action */
int                             /* ret 0 ok, else -ve err */
pl_set_pre_page_action(pl_interp_instance_t * instance, /* interp instance to use */
                       pl_page_action_t action, /* action to execute (rets 1 to abort w/o err) */
                       void *closure    /* closure to call action with */
    )
{
    return instance->interp->implementation->proc_set_pre_page_action
        (instance, action, closure);
}

/* Set an interpreter instance's post-page action */
int                             /* ret 0 ok, else -ve err */
pl_set_post_page_action(pl_interp_instance_t * instance,        /* interp instance to use */
                        pl_page_action_t action,        /* action to execute */
                        void *closure   /* closure to call action with */
    )
{
    return instance->interp->implementation->proc_set_post_page_action
        (instance, action, closure);
}

/* Get and interpreter prefered device memory allocator if any */
int                             /* ret 0 ok, else -ve error code */
pl_set_device(pl_interp_instance_t * instance,  /* interp instance to use */
              gx_device * device        /* device to set (open or closed) */
    )
{
    return instance->interp->implementation->proc_set_device(instance,
                                                             device);
}

/* Prepare interp instance for the next "job" */
int                             /* ret 0 ok, else -ve error code */
pl_init_job(pl_interp_instance_t * instance     /* interp instance to start job in */
    )
{
    return instance->interp->implementation->proc_init_job(instance);
}

/* Parse a random access seekable file.
   This function is mutually exclusive with pl_process and pl_flush_to_eoj,
   and is only called if the file is seekable and the function pointer is
   not NULL.
 */
int
pl_process_file(pl_interp_instance_t * instance, char *filename)
{
    return instance->interp->implementation->proc_process_file(instance,
                                                               filename);
}

/* Parse a cursor-full of data */
/* The parser reads data from the input
 * buffer and returns either:
 *      >=0 - OK, more input is needed.
 *      e_ExitLanguage - A UEL or other return to the default parser was
 *      detected.
 *      other <0 value - an error was detected.
 */
int
pl_process(pl_interp_instance_t * instance,     /* interp instance to process data job in */
           stream_cursor_read * cursor  /* data to process */
    )
{
    return instance->interp->implementation->proc_process(instance, cursor);
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
int
pl_flush_to_eoj(pl_interp_instance_t * instance,        /* interp instance to flush for */
                stream_cursor_read * cursor     /* data to process */
    )
{
    return instance->interp->implementation->proc_flush_to_eoj(instance,
                                                               cursor);
}

/* Parser action for end-of-file (also resets after unexpected EOF) */
int                             /* ret 0 or +ve if ok, else -ve error code */
pl_process_eof(pl_interp_instance_t * instance  /* interp instance to process data job in */
    )
{
    return instance->interp->implementation->proc_process_eof(instance);
}

/* Report any errors after running a job */
int                             /* ret 0 ok, else -ve error code */
pl_report_errors(pl_interp_instance_t * instance,       /* interp instance to wrap up job in */
                 int code,      /* prev termination status */
                 long file_position,    /* file position of error, -1 if unknown */
                 bool force_to_cout     /* force errors to cout */
    )
{
    return instance->interp->implementation->proc_report_errors
        (instance, code, file_position, force_to_cout);
}

/* Wrap up interp instance after a "job" */
int                             /* ret 0 ok, else -ve error code */
pl_dnit_job(pl_interp_instance_t * instance     /* interp instance to wrap up job in */
    )
{
    if (!instance || !instance->interp || !instance->interp->implementation)
        return 0;

    return instance->interp->implementation->proc_dnit_job(instance);
}

/* Remove a device from an interperter instance */
int                             /* ret 0 ok, else -ve error code */
pl_remove_device(pl_interp_instance_t * instance        /* interp instance to use */
    )
{
    if (!instance || !instance->interp || !instance->interp->implementation)
        return 0;

    return instance->interp->implementation->proc_remove_device(instance);
}

/* Deallocate a interpreter instance */
int                             /* ret 0 ok, else -ve error code */
pl_deallocate_interp_instance(pl_interp_instance_t * instance   /* instance to dealloc */
    )
{
    if (!instance || !instance->interp || !instance->interp->implementation)
        return 0;

    return instance->interp->implementation->
                proc_deallocate_interp_instance(instance);
}

/* Do static deinit of interpreter */
int                             /* ret 0 ok, else -ve error code */
pl_deallocate_interp(pl_interp_t * interp       /* interpreter to deallocate */
    )
{
    if (!interp || !interp->implementation)
        return 0;

    return interp->implementation->proc_deallocate_interp(interp);
}
