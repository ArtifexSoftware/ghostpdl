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
#include "psapi.h"
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
    gs_memory_t  *memory;
    uint          bytes_fed;
    gs_lib_ctx_t *psapi_instance;
    int           init_completed;
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

    psi->memory = mem;
    psi->bytes_fed = 0;
    psi->init_completed = 0;
    psi->psapi_instance = gs_lib_ctx_get_interp_instance(mem);
    code = psapi_new_instance(&psi->psapi_instance, NULL);
    if (code < 0)
        gs_free_object(mem, psi, "ps_impl_allocate_interp_instance");

    impl->interp_client_data = psi;

    /* Tell gs not to ignore a UEL, but do an interpreter exit */
    psapi_act_on_uel(psi->psapi_instance);

    code = psapi_init_with_args01(psi->psapi_instance, nargs, (char **)gsargs);
    if (code < 0) {
        psapi_delete_instance(psi->psapi_instance);
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
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;

    return psapi_get_device_memory(psi->psapi_instance);
}

static int
ps_impl_set_param(pl_interp_implementation_t *impl,
                  pl_set_param_type           type,
                  const char                 *param,
                  const void                 *val)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;

    return psapi_set_param(psi->psapi_instance, type, param, val);
}

/* Prepare interp instance for the next "job" */
static int
ps_impl_init_job(pl_interp_implementation_t *impl,
                 gx_device                  *device)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    int exit_code, code1;
    int code = 0;

    if (!psi->init_completed) {
        const float *resolutions;
        const long *page_sizes;

        pl_main_get_forced_geometry(psi->memory, &resolutions, &page_sizes);
        psapi_force_geometry(psi->psapi_instance, resolutions, page_sizes);

        code = psapi_init_with_args2(psi->psapi_instance);
        if (code < 0)
            return code;
        psi->init_completed = 1;
    }

    /* Possibly should be done in ps_impl_set_device */
    code = psapi_run_string_begin(psi->psapi_instance, 0, &exit_code);
    if (code > 0)
        code = psapi_run_string_continue(psi->psapi_instance, "erasepage", 10, 0, &exit_code);

    code1 = psapi_run_string_end(psi->psapi_instance, 0, &exit_code);

    code = code < 0 ? code : code1;

    if (code >= 0) {
        code = psapi_set_device(psi->psapi_instance, device);
        if (code < 0)
            code1 = psapi_set_device(psi->psapi_instance, NULL);
        (void)code1;
    }

    return code;
}

/* Not complete. */
static int
ps_impl_process_file(pl_interp_implementation_t *impl, char *filename)
{
    /* NB incomplete */
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    int exit_code;

    return psapi_run_file(psi->psapi_instance, filename, 0, &exit_code);
}

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
ps_impl_process_begin(pl_interp_implementation_t * impl)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    int exit_code;

    psi->bytes_fed = 0;
    return psapi_run_string_begin(psi->psapi_instance, 0, &exit_code);
}

/* TODO: in some fashion have gs pass back how far into the input buffer it
 * had read, so we don't need to explicitly search the buffer for the UEL
 */
static int
ps_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    const unsigned int len = pr->limit - pr->ptr;
    int code, exit_code = 0;

    code = psapi_run_string_continue(psi->psapi_instance, (const char *)pr->ptr + 1, len, 0, &exit_code);
    if (exit_code == gs_error_InterpreterExit) {
        int64_t offset;

        offset = psapi_get_uel_offset(psi->psapi_instance) - psi->bytes_fed;
        pr->ptr += offset;
        psi->bytes_fed += offset + 1;

#ifdef SEND_CTRLD_AFTER_UEL
        {
            const char eot[1] = {4};
            code = psapi_run_string_continue(psi->psapi_instance, eot, 1, 0, &exit_code);
            (void)code; /* Ignoring code here */
        }
#endif
        return gs_error_InterpreterExit;
    }
    else {
        pr->ptr = pr->limit;
        psi->bytes_fed += len;
    }
    return code;
}

static int                      /* ret 0 or +ve if ok, else -ve error code */
ps_impl_process_end(pl_interp_implementation_t * impl)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    int exit_code, code;

    code = psapi_run_string_end(psi->psapi_instance, 0, &exit_code);

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
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;

    return psapi_set_device(psi->psapi_instance, NULL);
}

/* Deallocate a interpreter instance */
static int
ps_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    int                   code;

    if (psi == NULL)
        return 0;
    code = psapi_exit(psi->psapi_instance);
    psapi_delete_instance(psi->psapi_instance);
    gs_free_object(psi->memory, psi, "ps_impl_allocate_interp_instance");
    impl->interp_client_data = NULL;
    return code;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t ps_implementation = {
  ps_impl_characteristics,
  ps_impl_allocate_interp_instance,
  ps_impl_get_device_memory,
  ps_impl_set_param,
  ps_impl_init_job,
  ps_impl_process_file,
  ps_impl_process_begin,
  ps_impl_process,
  ps_impl_process_end,
  ps_impl_flush_to_eoj,
  ps_impl_process_eof,
  ps_impl_report_errors,
  ps_impl_dnit_job,
  ps_impl_deallocate_interp_instance,
  NULL
};
