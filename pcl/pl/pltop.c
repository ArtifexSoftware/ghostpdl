/* Copyright (C) 2001-2020 Artifex Software, Inc.
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
#include "gserrors.h"
#include "stream.h"
#include "strmio.h"

/* Get implementation's characteristics */
const pl_interp_characteristics_t *     /* always returns a descriptor */
pl_characteristics(const pl_interp_implementation_t * impl)      /* implementation of interpreter to alloc */
{
    return impl->proc_characteristics(impl);
}

/* Do instance interpreter allocation/init. No device is set yet */
int                             /* ret 0 ok, else -ve error code */
pl_allocate_interp_instance(pl_interp_implementation_t * impl,
                            gs_memory_t                * mem)   /* allocator to allocate instance from */
{
    return impl->proc_allocate_interp_instance(impl, mem);
}

/*
 * Get the allocator with which to allocate a device
 */
gs_memory_t *
pl_get_device_memory(pl_interp_implementation_t *impl)
{
    if (impl->proc_get_device_memory == NULL)
        return NULL;
    return impl->proc_get_device_memory(impl);
}

int
pl_set_param(pl_interp_implementation_t *impl,
             gs_param_list              *plist)
{
    if (impl->proc_set_param == NULL)
        return 0;

    return impl->proc_set_param(impl, plist);
}

int
pl_add_path(pl_interp_implementation_t *impl,
            const char                 *path)
{
    if (impl->proc_add_path == NULL)
        return 0;

    return impl->proc_add_path(impl, path);
}

int pl_post_args_init(pl_interp_implementation_t *impl)
{
    if (impl->proc_post_args_init == NULL)
        return 0;

    return impl->proc_post_args_init(impl);
}

/* Prepare interp instance for the next "job" */
int                             /* ret 0 ok, else -ve error code */
pl_init_job(pl_interp_implementation_t * impl,     /* interp instance to start job in */
            gx_device                  * device) /* device to set (open or closed) */
{
    return impl->proc_init_job(impl, device);
}

int                             /* ret 0 ok, else -ve error code */
pl_run_prefix_commands(pl_interp_implementation_t * impl,     /* interp instance to start job in */
                       const char                 * prefix)
{
    if (prefix == NULL)
        return 0;
    if (impl->proc_run_prefix_commands == NULL)
        return -1;
    return impl->proc_run_prefix_commands(impl, prefix);
}

/* Parse a random access seekable file.
   This function is mutually exclusive with pl_process and pl_flush_to_eoj,
   and is only called if the file is seekable and the function pointer is
   not NULL.
 */
int
pl_process_file(pl_interp_implementation_t * impl, const char *filename)
{
    gs_memory_t *mem;
    int code, code1;
    stream *s;

    if (impl->proc_process_file != NULL)
        return impl->proc_process_file(impl, filename);

    /* We have to process the file in chunks. */
    mem = pl_get_device_memory(impl);
    code = 0;

    s = sfopen(filename, "r", mem);
    if (s == NULL)
        return gs_error_undefinedfilename;

    code = pl_process_begin(impl);

    while (code == gs_error_NeedInput || code >= 0) {
        if (s->cursor.r.ptr == s->cursor.r.limit && sfeof(s))
            break;
        code = s_process_read_buf(s);
        if (code < 0)
            break;

        code = pl_process(impl, &s->cursor.r);
        if_debug2m('I', mem, "processed (%s) job to offset %ld\n",
                   pl_characteristics(impl)->language,
                   sftell(s));
    }

    code1 = pl_process_end(impl);
    if (code >= 0 && code1 < 0)
        code = code1;

    sfclose(s);

    return code;
}

/* Do setup to for parsing cursor-fulls of data */
int
pl_process_begin(pl_interp_implementation_t * impl) /* interp instance to process data job in */
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
pl_process(pl_interp_implementation_t * impl,       /* interp instance to process data job in */
           stream_cursor_read         * cursor)     /* data to process */
{
    return impl->proc_process(impl, cursor);
}

int
pl_process_end(pl_interp_implementation_t * impl)   /* interp instance to process data job in */
{
    return impl->proc_process_end(impl);
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
int
pl_flush_to_eoj(pl_interp_implementation_t * impl,  /* interp instance to flush for */
                stream_cursor_read         * cursor)/* data to process */
{
    return impl->proc_flush_to_eoj(impl, cursor);
}

/* Parser action for end-of-file (also resets after unexpected EOF) */
int                             /* ret 0 or +ve if ok, else -ve error code */
pl_process_eof(pl_interp_implementation_t * impl)   /* interp instance to process data job in */
{
    return impl->proc_process_eof(impl);
}

/* Report any errors after running a job */
int                             /* ret 0 ok, else -ve error code */
pl_report_errors(pl_interp_implementation_t * impl,          /* interp instance to wrap up job in */
                 int                          code,          /* prev termination status */
                 long                         file_position, /* file position of error, -1 if unknown */
                 bool                         force_to_cout) /* force errors to cout */
{
    return impl->proc_report_errors
        (impl, code, file_position, force_to_cout);
}

/* Wrap up interp instance after a "job" */
int                             /* ret 0 ok, else -ve error code */
pl_dnit_job(pl_interp_implementation_t * impl)     /* interp instance to wrap up job in */
{
    return impl->proc_dnit_job(impl);
}

/* Deallocate a interpreter instance */
int                             /* ret 0 ok, else -ve error code */
pl_deallocate_interp_instance(pl_interp_implementation_t * impl)   /* instance to dealloc */
{
    if (impl->interp_client_data == NULL)
        return 0;
    return impl->proc_deallocate_interp_instance(impl);
}
