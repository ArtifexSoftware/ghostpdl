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


/* pltop.c */
/* Top-level API for interpreters */

#include "string_.h"
#include "gdebug.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsdevice.h"
#include "pltop.h"

/* Get implementation's characteristics */
const pl_interp_characteristics_t *     /* always returns a descriptor */
pl_characteristics(const pl_interp_implementation_t * impl)      /* implementation of interpreter to alloc */
{
    return impl->proc_characteristics(impl);
}

/* Do instance interpreter allocation/init. No device is set yet */
int                             /* ret 0 ok, else -ve error code */
pl_allocate_interp_instance(pl_interp_implementation_t * impl,
                            gs_memory_t * mem   /* allocator to allocate instance from */
    )
{
    return impl->proc_allocate_interp_instance(impl, mem);
}

/*
 * Get the allocator with which to allocate a device
 */
gs_memory_t *
pl_get_device_memory(pl_interp_implementation_t *impl)
{
    if (impl->proc_get_device_memory)
        return impl->proc_get_device_memory(impl);
    else
        return NULL;
}

/* Get and interpreter prefered device memory allocator if any */
int                             /* ret 0 ok, else -ve error code */
pl_set_device(pl_interp_implementation_t * impl,  /* interp instance to use */
              gx_device * device        /* device to set (open or closed) */
    )
{
    return impl->proc_set_device(impl, device);
}

/* Prepare interp instance for the next "job" */
int                             /* ret 0 ok, else -ve error code */
pl_init_job(pl_interp_implementation_t * impl     /* interp instance to start job in */
    )
{
    return impl->proc_init_job(impl);
}

/* Parse a random access seekable file.
   This function is mutually exclusive with pl_process and pl_flush_to_eoj,
   and is only called if the file is seekable and the function pointer is
   not NULL.
 */
int
pl_process_file(pl_interp_implementation_t * impl, char *filename)
{
    return impl->proc_process_file(impl, filename);
}

/* Do setup to for parsing cursor-fulls of data */
int
pl_process_begin(pl_interp_implementation_t * impl     /* interp instance to process data job in */)
{
    return impl->proc_process_begin(impl);
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
pl_process(pl_interp_implementation_t * impl,     /* interp instance to process data job in */
           stream_cursor_read * cursor  /* data to process */
    )
{
    return impl->proc_process(impl, cursor);
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
int
pl_flush_to_eoj(pl_interp_implementation_t * impl,        /* interp instance to flush for */
                stream_cursor_read * cursor     /* data to process */
    )
{
    return impl->proc_flush_to_eoj(impl, cursor);
}

/* Parser action for end-of-file (also resets after unexpected EOF) */
int                             /* ret 0 or +ve if ok, else -ve error code */
pl_process_eof(pl_interp_implementation_t * impl  /* interp instance to process data job in */
    )
{
    return impl->proc_process_eof(impl);
}

/* Report any errors after running a job */
int                             /* ret 0 ok, else -ve error code */
pl_report_errors(pl_interp_implementation_t * impl,       /* interp instance to wrap up job in */
                 int code,      /* prev termination status */
                 long file_position,    /* file position of error, -1 if unknown */
                 bool force_to_cout     /* force errors to cout */
    )
{
    return impl->proc_report_errors
        (impl, code, file_position, force_to_cout);
}

/* Wrap up interp instance after a "job" */
int                             /* ret 0 ok, else -ve error code */
pl_dnit_job(pl_interp_implementation_t * impl     /* interp instance to wrap up job in */
    )
{
    return impl->proc_dnit_job(impl);
}

/* Remove a device from an interperter instance */
int                             /* ret 0 ok, else -ve error code */
pl_remove_device(pl_interp_implementation_t * impl        /* interp instance to use */
    )
{
    return impl->proc_remove_device(impl);
}

/* Deallocate a interpreter instance */
int                             /* ret 0 ok, else -ve error code */
pl_deallocate_interp_instance(pl_interp_implementation_t * impl   /* instance to dealloc */
    )
{
    if (impl->interp_client_data == NULL)
        return 0;
    return impl->proc_deallocate_interp_instance(impl);
}
