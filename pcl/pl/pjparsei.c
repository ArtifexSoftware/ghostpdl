/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* pjparsei.c   PJL parser implementation glue file (To pjparse.c) */

#include "string_.h"
#include "gserrors.h"
#include "pjtop.h"
#include "pjparse.h"
#include "plparse.h"
#include "plver.h"

static int
pjl_detect_language(const char *s, int len)
{
    /* We will accept a single CRLF before the @PJL
     * string. This is a break with the spec, but is
     * required to cope with Bug 693269. We believe
     * this should be harmless in real world usage. */
    if (len && *s == '\r')
        s++, len--;
    if (len && *s == '\n')
        s++, len--;
    if (len < 4)
        return 0;
    if (memcmp(s, "@PJL", 4) == 0)
        return 100;
    return 0;
}

/* Get implementation's characteristics */
static const pl_interp_characteristics_t *      /* always returns a descriptor */
pjl_impl_characteristics(const pl_interp_implementation_t * impl)        /* implementation of interpreter to alloc */
{
    static const pl_interp_characteristics_t pjl_characteristics = {
        "PJL",
        pjl_detect_language,
    };
    return &pjl_characteristics;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_allocate_interp_instance(pl_interp_implementation_t *impl,
                                  gs_memory_t * mem     /* allocator to allocate instance from */
    )
{
    pjl_parser_state *pjls = pjl_process_init(mem);

    impl->interp_client_data = pjls;

    if (pjls == NULL)
        return gs_error_VMerror;

    /* Return success */
    return 0;
}

/* Prepare interp instance for the next "job" */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_init_job(pl_interp_implementation_t *impl,       /* interp instance to start job in */
                  gx_device                  *device)
{
    pjl_parser_state *pjls = impl->interp_client_data;
    if (pjls == NULL)
        return gs_error_VMerror;
    /* copy the default state to the initial state */
    return pjl_set_init_from_defaults(pjls);
}

static int
pjl_impl_process_begin(pl_interp_implementation_t *impl       /* interp instance to process data job in */)
{
    return 0;
}

/* Parse a cursor-full of data */

/* The parser reads data from the input
 * buffer and returns either:
 *	>=0 - OK, more input is needed.
 *	e_ExitLanguage - Non-PJL was detected.
 *	<0 value - an error was detected.
 */

static int
pjl_impl_process(pl_interp_implementation_t *impl,       /* interp instance to process data job in */
                 stream_cursor_read * cursor    /* data to process */
    )
{
    pjl_parser_state *pjls = impl->interp_client_data;
    int code = pjl_process(pjls, NULL, cursor);

    return code == 1 ? e_ExitLanguage : code;
}

static int
pjl_impl_process_end(pl_interp_implementation_t *impl       /* interp instance to process data job in */)
{
    return 0;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
static int
pjl_impl_flush_to_eoj(pl_interp_implementation_t * impl,  /* interp impl to flush for */
                      stream_cursor_read * cursor       /* data to process */
    )
{
    return pjl_skip_to_uel(cursor) ? 1 : 0;
}

/* Parser action for end-of-file */
static int                      /* ret 0 or +ve if ok, else -ve error code */
pjl_impl_process_eof(pl_interp_implementation_t * impl    /* interp impl to process data job in */
    )
{
    return 0;
}

/* Report any errors after running a job */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_report_errors(pl_interp_implementation_t * impl, /* interp impl to wrap up job in */
                       int code,        /* prev termination status */
                       long file_position,      /* file position of error, -1 if unknown */
                       bool force_to_cout       /* force errors to cout */
    )
{
    return 0;
}

/* Wrap up interp impl after a "job" */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_dnit_job(pl_interp_implementation_t * impl       /* interp impl to wrap up job in */
    )
{
    return 0;
}

/* Deallocate a interpreter impl */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_deallocate_interp_instance(pl_interp_implementation_t * impl     /* impl to dealloc */
    )
{
    pjl_parser_state *pjls = impl->interp_client_data;
    pjl_process_destroy(pjls);
    return 0;
}

/* Parser implementation descriptor */
pl_interp_implementation_t pjl_implementation = {
    pjl_impl_characteristics,
    pjl_impl_allocate_interp_instance,
    NULL,                       /* get_device_memory */
    NULL,                       /* set_param */
    NULL,                       /* add_path */
    NULL,                       /* post_args_init */
    pjl_impl_init_job,
    NULL,                       /* run_prefix_commands */
    NULL,                       /* process_file */
    pjl_impl_process_begin,
    pjl_impl_process,
    pjl_impl_process_end,
    pjl_impl_flush_to_eoj,
    pjl_impl_process_eof,
    pjl_impl_report_errors,
    pjl_impl_dnit_job,
    pjl_impl_deallocate_interp_instance,
    NULL, /* pjl_impl_reset */
    NULL, /* interp_client_data */
};
