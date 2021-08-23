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
#include "plmain.h"
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
} ps_interp_instance_t;

static int
check_token(int token_type, const char *s, const char *e, int *score)
{
    /* Empty tokens are always OK */
    if (s == e)
        return 0;

    switch (token_type) {
        case 'a':
            /* Angle bracket - mainly to catch << */
            break;
        case 'n':
            /* Name */
            /* FIXME: Check it's a valid name */
            break;
        case 'd':
            /* Dictionary */
            break;
        case 'i':
            /* int - ok by construction. */
            return 0;
        case 'f':
            /* float - ok by construction. */
            return 0;
    }

#define TOKEN_CHECK(n) else if (e-s == strlen(n) && memcmp(s, n, e-s) == 0) { score[0] += e-s; score[1]++; }

    if (0) { /* Initial block of checking chain */ }
    TOKEN_CHECK("dup")
    TOKEN_CHECK("exch")
    TOKEN_CHECK("grestore")
    TOKEN_CHECK("gsave")
    TOKEN_CHECK("idiv")
    TOKEN_CHECK("lineto")
    TOKEN_CHECK("mod")
    TOKEN_CHECK("mul")
    TOKEN_CHECK("moveto")
    TOKEN_CHECK("setflat")
    TOKEN_CHECK("setlinecap")
    TOKEN_CHECK("setlinejoin")
    TOKEN_CHECK("showpage")
    TOKEN_CHECK("stroke")
    TOKEN_CHECK("translate")
    TOKEN_CHECK("systemdict")

    if (score[0] > 1024 && score[1] >= 3)
        return 1;
    if (score[0] < -1024)
        return 1;

    return 0;
}

static int
score_comment(const char *s, const char *e, int *score)
{
#define COMMENT_CHECK(n) else if (e-s >= strlen(n) && memcmp(s, n, strlen(n)) == 0) { score[0] += 100; score[1]++; }

    if (0) { /* Initial block of checking chain */ }
    COMMENT_CHECK("!PS")
    COMMENT_CHECK("%Title:")
    COMMENT_CHECK("%Version:")
    COMMENT_CHECK("%Creator:")
    COMMENT_CHECK("%CreationDate:")
    COMMENT_CHECK("%Document")
    COMMENT_CHECK("%BoundingBox:")
    COMMENT_CHECK("%HiResBoundingBox:")
    COMMENT_CHECK("%Pages:")
    COMMENT_CHECK("%+ procset")
    COMMENT_CHECK("%End")
    COMMENT_CHECK("%Begin")
    COMMENT_CHECK("%Copyright")
    else {
        score[0]++; score[1]++;
    }

    if (score[0] > 1024 && score[1] >= 3)
        return 1;

    return 0;
}

static int
ps_detect_language(const char *s, int len)
{
    /* For postscript, we look for %! */
    if (len >= 2) {
        if (s[0] != '%' || s[1] != '!') {
            /* Not what we were looking for */
        } else if (len >= 12 && memcmp(s+2, "Postscript", 10) == 0) {
            return 100;
        } else if (len >= 4 && memcmp(s+2, "PS", 2) == 0) {
            return 100;
        } else if (len >= 3 && s[2] == '/') {
            /* Looks like a shell script. Don't want that. */
            return 0;
        } else {
            /* If it begins %!, then it's PROBABLY postscript */
            return 80;
        }
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
                return 100;
            }
            if (memcmp(t, "%!PS-Adobe-", 11) == 0 &&
                t[11] >= '0' && t[11] <= '9' &&
                t[12] == '.' &&
                t[13] >= '0' && t[13] <= '9' &&
                memcmp(t+14, " PDF-", 5) == 0 &&
                t[19] >= '0' && t[19] <= '9' &&
                t[20] == '.' &&
                t[21] >= '0' && t[21] <= '9') {
                return 100;
            }
            t++;
            left--;
        }
    }

    /* Now we do some seriously hairy stuff */
    {
        const char *t = s;
        const char *token = t;
        int left = len;
        int token_type = 0;
        int score[2] = { 0, 0 };

        while (left--) {
            if (*t == '%') {
                if (check_token(token_type, token, t, score))
                    break;
                /* Skip to end of line */
                left--;
                token = ++t;
                while (left && *t != '\r' && *t != '\n') {
                    left--; t++;
                }
                if (score_comment(token, t, score))
                    break;
                /* Skip any combination of newlines */
                while (left && (*t == '\r' || *t == '\n')) {
                    left--; t++;
                }
                token_type = 0;
                continue;
            } else if (*t == 27) {
                /* Give up if we meet an ESC. It could be a UEL. */
                break;
            } else if (*t <= 32 || *(unsigned char *)t > 127) {
                if (check_token(token_type, token, t, score))
                    break;
                if (*t != 9 && *t != 10 && *t != 12 && *t != 13 && *t != 32)
                    score[0]--;
                token = t+1;
                token_type = 0;
            } else if (*t == '/') {
                if (check_token(token_type, token, t, score))
                    break;
                token = t+1;
                token_type = 'n';
            } else if (*t == '[' || *t == ']' || *t == '{' || *t == '}') {
                if (check_token(token_type, token, t, score))
                    break;
                token = t+1;
                token_type = 0;
            } else if (*t == '<') {
                if (token_type == 'a') {
                    /* << */
                    token = t+1;
                    token_type = 'd';
                } else if (token_type == 'd') {
                    /* <<< ???!? */
                    score[0] -= 10;
                } else {
                    if (check_token(token_type, token, t, score))
                        break;
                    token = t+1;
                    token_type = 'a';
                }
            } else if (*t == '>') {
                if (check_token(token_type, token, t, score))
                    break;
                token = t+1;
                token_type = 0;
            } else if (*t >= '0' && *t <= '9') {
                if (token_type == 'i') {
                    /* Keep going */
                } else if (token_type == 'f') {
                    /* Keep going */
                } else {
                    if (check_token(token_type, token, t, score))
                        break;
                    token = t;
                    token_type = 'i';
                }
            } else if (*t == '.') {
                if (token_type == 'f') {
                    /* seems unlikely */
                    score[0]--;
                    break;
                } else if (token_type == 'i') {
                    token = t;
                    token_type = 'f';
                } else {
                    if (check_token(token_type, token, t, score))
                        break;
                    token = t;
                    token_type = 'f';
                }
            } else {
                /* Assume anything else goes into the token */
            }
            t++;
        }
        if (score[0] < 0 || score[1] < 3)
            return 0; /* Unlikely to be PS */
        else if (score[0] > 0)
            return 75; /* Could be PS */
    }

    return 0;
}

/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
ps_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
    /* version and build date are not currently used */
  static const pl_interp_characteristics_t ps_characteristics = {
    "POSTSCRIPT",
    ps_detect_language,
  };

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
    psi->psapi_instance = gs_lib_ctx_get_interp_instance(mem);
    code = psapi_new_instance(&psi->psapi_instance, NULL);
    if (code < 0) {
        gs_free_object(mem, psi, "ps_impl_allocate_interp_instance");
        return code;
    }

    impl->interp_client_data = psi;

    /* Tell gs not to ignore a UEL, but do an interpreter exit */
    psapi_act_on_uel(psi->psapi_instance);

    code = psapi_init_with_args01(psi->psapi_instance, nargs, (char **)gsargs);
    if (code < 0) {
        (void)psapi_exit(psi->psapi_instance);
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
                  gs_param_list *plist)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;

    return psapi_set_param(psi->psapi_instance, plist);
}

static int
ps_impl_add_path(pl_interp_implementation_t *impl,
                 const char                 *path)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;

    return psapi_add_path(psi->psapi_instance, path);
}

static int
ps_impl_post_args_init(pl_interp_implementation_t *impl)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    const float *resolutions;
    const long *page_sizes;

    pl_main_get_forced_geometry(psi->memory, &resolutions, &page_sizes);
    psapi_force_geometry(psi->psapi_instance, resolutions, page_sizes);

    return psapi_init_with_args2(psi->psapi_instance);
}

/* Prepare interp instance for the next "job" */
static int
ps_impl_init_job(pl_interp_implementation_t *impl,
                 gx_device                  *device)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    int exit_code;
    int code;

    /* Any error after here *must* reset the device to null */
    code = psapi_set_device(psi->psapi_instance, device);

    if (code >= 0)
        code = psapi_run_string(psi->psapi_instance, "erasepage", 0, &exit_code);

    if (code < 0) {
        int code1 = psapi_set_device(psi->psapi_instance, NULL);
        (void)code1;
    }

    return code;
}

static int
ps_impl_run_prefix_commands(pl_interp_implementation_t *impl,
                            const char                 *prefix)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)impl->interp_client_data;
    int exit_code;
    int code = 0;

    if (prefix == NULL)
        return 0;

    /* Any error after here *must* reset the device to null */
    code = psapi_run_string(psi->psapi_instance, prefix, 0, &exit_code);

    if (code < 0) {
        int code1 = psapi_set_device(psi->psapi_instance, NULL);
        (void)code1;
    }

    return code;
}

static int
ps_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
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
    unsigned int len;
    int code, exit_code = 0;

    if (psi->bytes_fed == 0)
    {
        /* Skip over whitespace/comments looking for a PDF marker. */
        while (pr->ptr < pr->limit)
        {
            int i;

            /* Skip over whitespace (as defined in PLRM) */
            if (pr->ptr[1] == 0 ||
                pr->ptr[1] == 9 ||
                pr->ptr[1] == 10 ||
                pr->ptr[1] == 12 ||
                pr->ptr[1] == 13 ||
                pr->ptr[1] == 32) {
                pr->ptr++;
                continue;
            }

            /* If we're not starting a comment, exit. */
            if (pr->ptr[1] != '%')
                break;

            /* If we're starting with a PDF header, swap to file mode. */
            if (pr->limit - pr->ptr >= 8 &&
                strncmp((const char *)&pr->ptr[2], "PDF-", 4) == 0 &&
                (pr->ptr[6] >= '1' && pr->ptr[6] <= '9') &&
                pr->ptr[7] == '.' &&
                (pr->ptr[8] >= '0' && pr->ptr[8] <= '9'))
                return_error(gs_error_NeedFile);

            /* Check for a historical PDF header. */
            if (pr->limit - pr->ptr >= 22 &&
                strncmp((const char *)&pr->ptr[2], "!PS-Adobe-", 10) == 0 &&
                (pr->ptr[12] >= '0' && pr->ptr[12] <= '9') &&
                pr->ptr[13] == '.' &&
                (pr->ptr[14] >= '0' && pr->ptr[14] <= '9') &&
                strncmp((const char *)&pr->ptr[15], " PDF-", 5) == 0 &&
                (pr->ptr[20] >= '0' && pr->ptr[20] <= '9') &&
                pr->ptr[21] == '.' &&
                (pr->ptr[22] >= '0' && pr->ptr[22] <= '9'))
                return_error(gs_error_NeedFile);

            /* Do we have a complete comment that we can skip? */
            for (i = 1; pr->ptr + i < pr->limit; i++)
                if (pr->ptr[i+1] == 10 || pr->ptr[i+1] == 13) {
                    pr->ptr += i;
                    i = 0; /* Loop again in case there are more comments. */
                    break;
                }
            /* If we fall out of the loop naturally, then we hit the end
             * of the buffer without terminating our comment. We need to
             * abort the loop and return. */
            if (i != 0)
                return_error(gs_error_NeedInput);
        }
    }

    len = pr->limit - pr->ptr;
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
  ps_impl_add_path,
  ps_impl_post_args_init,
  ps_impl_init_job,
  ps_impl_run_prefix_commands,
  ps_impl_process_file,
  ps_impl_process_begin,
  ps_impl_process,
  ps_impl_process_end,
  ps_impl_flush_to_eoj,
  ps_impl_process_eof,
  ps_impl_report_errors,
  ps_impl_dnit_job,
  ps_impl_deallocate_interp_instance,
  NULL, /* ps_impl_reset */
  NULL  /* interp_client_data */
};
