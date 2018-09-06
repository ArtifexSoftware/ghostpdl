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

/* psitop.c */
/* Top-level API implementation of PS Language Interface */

#include "stdio_.h"
#include "ghost.h"
#include "imain.h"
#include "imainarg.h"
#include "iapi.h"
#include "string_.h"
#include "gdebug.h"
#include "gp.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsstate.h"		/* must precede gsdevice.h */
#include "gxdevice.h"		/* must precede gsdevice.h */
#include "gsdevice.h"
#include "icstate.h"		/* for i_ctx_t */
#include "iminst.h"
#include "gsstruct.h"		/* for gxalloc.h */
#include "gspaint.h"
#include "gxalloc.h"
#include "gxstate.h"
#include "plparse.h"
#include "pltop.h"
#include "gzstate.h"
#include "gsicc_manage.h"

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

/* Import operator procedures */
extern int zflush(i_ctx_t *);

/*
 * PS interpreter instance: derived from pl_interp_implementation_t
 */
typedef struct ps_interp_instance_s {
    pl_interp_implementation_t pl;
} ps_interp_instance_t;

static int
ps_detect_language(const char *s, int len)
{
    /* For postscript, we look for %! */
    if (len >= 2) {
        /* Be careful to avoid shell scripts (e.g. #!/bin/bash) if possible */
        if (s[0] == '%' && s[1] == '!' && (len < 3 || s[2] != '/'))
            return 0;
    }
    /* For PDF, we allow for leading crap, then a postscript version marker */
    {
        const char *t = s;
        int left = len-22;
        /* Search within just the first 4K, plus a bit for the marker length. */
        if (left > 4096+22)
            left = 4096+22;
        while (left > 22) {
            if (memcmp(t, "%PDF-", 5) == 0 &&
                t[5] >= '1' && t[5] <= '9' &&
                t[6] == '.' &&
                t[7] >= '0' && t[7] <= '9') {
                return 0;
            }
            if (memcmp(t, "%!PS-Adobe-", 11) == 0 &&
                t[11] >= '0' && t[11] <= '9' &&
                t[12] == '.' &&
                t[13] >= '0' && t[13] <= '9' &&
                memcmp(t+14, " PDF-", 5) == 0 &&
                t[19] >= '0' && t[19] <= '9' &&
                t[20] == '.' &&
                t[21] >= '0' && t[21] <= '9') {
                return 0;
            }
            t++;
            left--;
        }
    }

    return 1;
}

/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
ps_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
    /* version and build date are not currently used */
#define PSVERSION NULL
#define PSBUILDDATE NULL
  static const pl_interp_characteristics_t ps_characteristics = {
    "POSTSCRIPT",
    ps_detect_language,
    "Artifex",
    PSVERSION,
    PSBUILDDATE,
    1				/* minimum input size to PostScript */
  };
#undef PSVERSION
#undef PSBUILDDATE

  return &ps_characteristics;
}

/* Do per-instance interpreter allocation/init. */
static int
ps_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    ps_interp_instance_t *psi
        = (ps_interp_instance_t *)gs_alloc_bytes( mem,
                                                  sizeof(ps_interp_instance_t),
                                                  "ps_impl_allocate_interp_instance");

    int code;
#define GS_MAX_NUM_ARGS 10
    const char *gsargs[GS_MAX_NUM_ARGS] = {0};
    int nargs = 0;
    
    if (!psi)
        return gs_error_VMerror;
        
    gsargs[nargs++] = "gpdl";
    /* We start gs with the nullpage device, and replace the device with the
     * set_device call from the language independent code.
     */
    gsargs[nargs++] = "-dNODISPLAY";
    /* As we're "printer targetted, use a jobserver */
    gsargs[nargs++] = "-dJOBSERVER";

    code = gsapi_new_instance(&impl->interp_client_data, NULL);
    if (code < 0)
        gs_free_object(mem, psi, "ps_impl_allocate_interp_instance");

    /* Tell gs not to ignore a UEL, but do an interpreter exit */
    gsapi_act_on_uel(impl->interp_client_data);

    code = gsapi_init_with_args(impl->interp_client_data, nargs, (char **)gsargs);
    if (code < 0) {
        gsapi_delete_instance(impl->interp_client_data);
        gs_free_object(mem, psi, "ps_impl_allocate_interp_instance");
    }
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
ps_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    return gsapi_get_device_memory(impl->interp_client_data);
}

/* Set a device into an interpreter instance */
static int
ps_impl_set_device(pl_interp_implementation_t *impl, gx_device *device)
{
    return gsapi_set_device(impl->interp_client_data, device);
}


/* Prepare interp instance for the next "job" */
static int
ps_impl_init_job(pl_interp_implementation_t *impl)
{
    int exit_code, code, code1;
    /* Possibly should be done in ps_impl_set_device */
    code = gsapi_run_string_begin(impl->interp_client_data, 0, &exit_code);
    if (code > 0)
        code = gsapi_run_string_continue(impl->interp_client_data, "erasepage", 10, 0, &exit_code);

    code1 = gsapi_run_string_end(impl->interp_client_data, 0, &exit_code);
    return code < 0 ? code : code1;
}

/* Not complete. */
static int
ps_impl_process_file(pl_interp_implementation_t *impl, char *filename)
{
    /* NB incomplete */
    int exit_code;
    return gsapi_run_file(impl->interp_client_data, filename, 0, &exit_code);
}

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
ps_impl_process_begin(pl_interp_implementation_t * impl)
{
    int exit_code;
    return gsapi_run_string_begin(impl->interp_client_data, 0, &exit_code);
}

/* TODO: in some fashion have gs pass back how far into the input buffer it
 * had read, so we don't need to explicitly search the buffer for the UEL
 */
static int
ps_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    const unsigned int l = pr->limit - pr->ptr;
    int code, exit_code = 0;
    const char eot[1] = {4};

    code = gsapi_run_string_continue(impl->interp_client_data, (const char *)pr->ptr + 1, l, 0, &exit_code);
    if (exit_code == gs_error_InterpreterExit) {
        const byte *p = pr->ptr;
        const byte *rlimit = pr->limit;

        /* Skip to, but leave UEL in buffer for PJL to find later */
        for (; p < rlimit; ++p) {
            if (p[1] == '\033') {
                uint avail = rlimit - p;

                if (memcmp(p + 1, "\033%-12345X", min(avail, 9)))
                    continue;
                if (avail < 9)
                    break;

                code = gsapi_run_string_continue(impl->interp_client_data, eot, 1, 0, &exit_code);

                pr->ptr = p;
                return gs_error_InterpreterExit;
            }
        }
        pr->ptr = p;
    }
    else {
        pr->ptr = pr->limit;
    }
    return code;
}

static int                      /* ret 0 or +ve if ok, else -ve error code */
ps_impl_process_end(pl_interp_implementation_t * impl)
{
    int exit_code, code;
    code = gsapi_run_string_end(impl->interp_client_data, 0, &exit_code);

    if (exit_code == gs_error_InterpreterExit || code == gs_error_NeedInput)
        code = 0;

    return code;
}

/* Not implemented */
static int
ps_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
{
    const byte *p = cursor->ptr;
    const byte *rlimit = cursor->limit;

    /* Skip to, but leave UEL in buffer for PJL to find later */
    for (; p < rlimit; ++p)
        if (p[1] == '\033') {
            uint avail = rlimit - p;

            if (memcmp(p + 1, "\033%-12345X", min(avail, 9)))
                continue;
            if (avail < 9)
                break;
            cursor->ptr = p;
            return 1;           /* found eoj */
        }
    cursor->ptr = p;
    return 0;                   /* need more */
}

/* Parser action for end-of-file */
static int
ps_impl_process_eof(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Report any errors after running a job */
static int
ps_impl_report_errors(pl_interp_implementation_t *impl,      /* interp instance to wrap up job in */
                      int                  code,           /* prev termination status */
                      long                 file_position,  /* file position of error, -1 if unknown */
                      bool                 force_to_cout    /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
ps_impl_dnit_job(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Remove a device from an interpreter instance */
static int
ps_impl_remove_device(pl_interp_implementation_t *impl)
{
    return gsapi_set_device(impl->interp_client_data, NULL);
}

/* Deallocate a interpreter instance */
static int
ps_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{

    int code = gsapi_exit(impl->interp_client_data);
    gsapi_delete_instance(impl->interp_client_data);
    return code;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t ps_implementation = {
  ps_impl_characteristics,
  ps_impl_allocate_interp_instance,
  ps_impl_get_device_memory,
  ps_impl_set_device,
  ps_impl_init_job,
  ps_impl_process_file,
  ps_impl_process_begin,
  ps_impl_process,
  ps_impl_process_end,
  ps_impl_flush_to_eoj,
  ps_impl_process_eof,
  ps_impl_report_errors,
  ps_impl_dnit_job,
  ps_impl_remove_device,
  ps_impl_deallocate_interp_instance,
  NULL
};
