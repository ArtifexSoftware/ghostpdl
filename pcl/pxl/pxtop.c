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


/* pxtop.c */
/* Top-level API implementation of PCL/XL */

#include "stdio_.h"
#include "string_.h"
#include "gdebug.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstate.h"            /* must precede gsdevice.h */
#include "gsdevice.h"
#include "gsstruct.h"           /* for gxalloc.h */
#include "gspaint.h"
#include "gsfont.h"
#include "gxalloc.h"
#include "gxstate.h"
#include "gxdevice.h"
#include "plparse.h"
#include "pxattr.h"             /* for pxparse.h */
#include "pxerrors.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxfont.h"
#include "pxvalue.h"            /* for pxparse.h */
#include "pxparse.h"
#include "pxptable.h"
#include "pxstate.h"
#include "pltop.h"
#include "plmain.h"
#include "gsicc_manage.h"
#include "gxdevsop.h"

/* Imported operators */
px_operator_proc(pxEndPage);
px_operator_proc(pxBeginSession);

/* Forward decls */
static int pxl_end_page_top(px_state_t * pxs, int num_copies, int flush);

static int px_top_init(px_parser_state_t * st, px_state_t * pxs,
                       bool big_endian);

/* ------------ PCXL stream header processor ------- */
/* State used to process an XL stream header */
typedef struct px_stream_header_process_s
{
    enum { PSHPReady, PSHPSkipping, PSHPDone } state;
    px_parser_state_t *st;      /* parser state to refer to */
    px_state_t *pxs;            /* xl state to refer to */
} px_stream_header_process_t;

/* Initialize stream header processor */
static void
px_stream_header_init(px_stream_header_process_t * process,
                      px_parser_state_t * st,
                      px_state_t * pxs)
{
    process->state = PSHPReady;
    process->st = st;
    process->pxs = pxs;
}

/* Process stream header input */
/* ret -ve error, 0 if needs more input, 1 if done successfully */
static int
px_stream_header_process(px_stream_header_process_t * process,
                         stream_cursor_read * cursor)
{
    while (cursor->ptr != cursor->limit) {
        switch (process->state) {
        case PSHPReady:
            process->state = PSHPSkipping;  /* switch to next state */
            switch ((*++cursor->ptr)) {
            case '(':
                px_top_init(process->st, process->pxs, true);
                break;
            case ')':
                px_top_init(process->st, process->pxs, false);
                break;
            default:
                /* Initialize state to avoid confusion */
                px_top_init(process->st, process->pxs, true);
                return gs_note_error(errorUnsupportedBinding);
            }
            break;
        case PSHPSkipping:
            if ((*++cursor->ptr) == '\n') {
                process->state = PSHPDone;
                return 1;
            }
            break;
        case PSHPDone:
        default:
            /* Shouldn't ever come here */
            return gs_note_error(errorIllegalStreamHeader);
        }
    }
    return 0;                   /* need more input */
}

/* De-initialize stream header processor */
static void
px_stream_header_dnit(px_stream_header_process_t * process)
{
    /* empty proc */
}

/*
 * PXL interpreter instance: derived from pl_interp_implementation_t
 */
typedef struct pxl_interp_instance_s
{
    gs_memory_t *memory;        /* memory allocator to use */
    px_parser_state_t *st;      /* parser state */
    px_state_t *pxs;            /* interp state */
    gs_gstate *pgs;             /* graphics state */
    enum {
        PSHeader,
        PSXL,
        PSDone
    } processState;             /* interp's processing state */
    px_stream_header_process_t headerState;        /* used to decode stream header */
} pxl_interp_instance_t;

static int
pxl_detect_language(const char *s, int len)
{
   if (len < 11)
       return 0;
   if (memcmp(s, ") HP-PCL XL", 11) == 0)
       return 100;
   return 0;
}

/* Get implementation's characteristics */
static const pl_interp_characteristics_t *      /* always returns a descriptor */
pxl_impl_characteristics(const pl_interp_implementation_t * impl)        /* implementation of interpreter to alloc */
{
    /* version and build date are not currently used */
#define PXLVERSION NULL
#define PXLBUILDDATE NULL
    static pl_interp_characteristics_t pxl_characteristics = {
        "PCLXL",
        pxl_detect_language,
    };
    return &pxl_characteristics;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int                      /* ret 0 ok, else -ve error code */
pxl_impl_allocate_interp_instance(pl_interp_implementation_t *impl,
                                  gs_memory_t * mem)
{
    /* Allocate everything up front */
    pxl_interp_instance_t *pxli      /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
        = (pxl_interp_instance_t *) gs_alloc_bytes(mem,
                                                   sizeof
                                                   (pxl_interp_instance_t),
                                                   "pxl_allocate_interp_instance(pxl_interp_instance_t)");
    gs_gstate *pgs = gs_gstate_alloc(mem);
    px_parser_state_t *st = px_process_alloc(mem);      /* parser init, cheap */
    px_state_t *pxs = px_state_alloc(mem);      /* inits interp state, potentially expensive */

    /* If allocation error, deallocate & return */
    if (!pxli || !pgs || !st || !pxs) {
        if (pxli)
            gs_free_object(mem, pxli,
                           "pxl_impl_allocate_interp_instance(pxl_interp_instance_t)");
        if (pgs)
            gs_gstate_free(pgs);
        if (st)
            px_process_release(st);
        if (pxs)
            px_state_release(pxs);
        return gs_error_VMerror;
    }
    gsicc_init_iccmanager(pgs);

    /* Setup pointers to allocated mem within instance */
    pxli->memory = mem;
    pxli->pgs = pgs;
    pxli->pxs = pxs;
    pxli->st = st;

    /* General init of pxl_state */
    px_state_init(pxs, pgs);    /*pgs only needed to set pxs as pgs' client */
    pxs->client_data = pxli;
    pxs->end_page = pxl_end_page_top;   /* after px_state_init */

    pxs->pjls = pl_main_get_pjl_instance(mem);

    /* The PCL instance is needed for PassThrough mode */
    pxs->pcls = pl_main_get_pcl_instance(mem);

    /* Return success */
    impl->interp_client_data = pxli;
    return 0;
}

static int
pxl_set_icc_params(pl_interp_implementation_t * impl, gs_gstate * pgs)
{
    pxl_interp_instance_t *pxli  = impl->interp_client_data;
    return pl_set_icc_params(pxli->memory, pgs);
}

/*
 * Set a Boolean parameter.
 */
static int
put_param_bool(pxl_interp_instance_t *pxli, gs_param_name pkey, bool value)
{
    gs_c_param_list list;
    int code;

    gs_c_param_list_write(&list, pxli->memory);
    code = param_write_bool((gs_param_list *) & list, pkey, &value);
    if (code >= 0) {
        gs_c_param_list_read(&list);

        code = gs_gstate_putdeviceparams(pxli->pgs,
                                         gs_currentdevice(pxli->pgs),
                                         (gs_param_list *) &list);
    }

    gs_c_param_list_release(&list);
    return code;
}

/* Prepare interp instance for the next "job" */
/* ret 0 ok, else -ve error code */
static int
pxl_impl_init_job(pl_interp_implementation_t * impl,
                  gx_device                  * device)
{
    int code = 0;
    pxl_interp_instance_t *pxli = impl->interp_client_data;
    px_state_t *pxs = pxli->pxs;
    gs_memory_t *mem = pxli->memory;

    enum { Sbegin, Ssetdevice, Sinitg, Sgsave, Serase, Sdone } stage;

    px_reset_errors(pxli->pxs);
    px_process_init(pxli->st, true);

    /* set input status to: expecting stream header */
    px_stream_header_init(&pxli->headerState, pxli->st, pxli->pxs);
    pxli->processState = PSHeader;

    stage = Sbegin;

    pxs->interpolate = pl_main_get_interpolate(mem);
    pxs->nocache = pl_main_get_nocache(mem);
    gs_setscanconverter(pxli->pgs, pl_main_get_scanconverter(mem));

    if (pxs->nocache)
        gs_setcachelimit(pxs->font_dir, 0);

    /* Set the device into the gstate */
    stage = Ssetdevice;
    if ((code = gs_setdevice_no_erase(pxli->pgs, device)) < 0)  /* can't erase yet */
        goto pisdEnd;

    /* Init XL graphics */
    stage = Sinitg;
    if ((code = px_initgraphics(pxli->pxs)) < 0)
        goto pisdEnd;

    code = pxl_set_icc_params(impl, pxli->pgs);
    if (code < 0)
        goto pisdEnd;

    /* Do inits of gstate that may be reset by setdevice */
    gs_setaccuratecurves(pxli->pgs, true);      /* All H-P languages want accurate curves. */

    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    stage = Sgsave;
    if ((code = gs_gsave(pxli->pgs)) < 0)
        goto pisdEnd;

    stage = Serase;
    if ((code = gs_erasepage(pxli->pgs)) < 0)
        goto pisdEnd;

    stage = Sdone;              /* success */
    /* Unwind any errors */
  pisdEnd:
    switch (stage) {
        case Sdone:            /* don't undo success */
            break;

        case Serase:           /* gs_erasepage failed */
            /* undo  gsave */
            gs_grestore_only(pxli->pgs);        /* destroys gs_save stack */
            /* fall thru to next */
        case Sgsave:           /* gsave failed */
        case Sinitg:
            /* undo setdevice */
            gs_nulldevice(pxli->pgs);
            /* fall thru to next */

        case Ssetdevice:       /* gs_setdevice failed */
        case Sbegin:           /* nothing left to undo */
            break;
    }

    /* Warn the device that PXL uses ROPs. */
    if (code == 0) {
        code = put_param_bool(pxli, "LanguageUsesROPs", true);

        if (!device->is_open)
            code = gs_opendevice(device);
    }

    return code;
}

/* Do any setup for parser per-cursor */
static int
pxl_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Parse a cursor-full of data */
/* ret 0 or +ve if ok, else -ve error code */
static int
pxl_impl_process(pl_interp_implementation_t * impl,
                 stream_cursor_read * cursor)
{
    pxl_interp_instance_t *pxli = impl->interp_client_data;
    int code = 0;

    /* Process some input */
    switch (pxli->processState) {
    case PSDone:
        return e_ExitLanguage;
    case PSHeader:         /* Input stream header */
        code = px_stream_header_process(&pxli->headerState, cursor);
        if (code == 0)
            break;          /* need more input later */
        else if (code < 0) {        /* stream header termination */
            pxli->processState = PSDone;
            return code;    /* return error */
        } else {
            pxli->processState = PSXL;
        }
        /* fall thru to PSXL */
    case PSXL:             /* PCL XL */
        code = px_process(pxli->st, pxli->pxs, cursor);
        if (code == e_ExitLanguage) {
            pxli->processState = PSDone;
        } else if (code == errorWarningsReported) {
            /* The parser doesn't skip over the EndSession */
            /* operator, because an "error" occurred. */
            cursor->ptr++;
        } else if (code < 0)
            /* Map library error codes to PCL XL codes when possible. */
            switch (code) {
            case gs_error_invalidfont:
                code = errorIllegalFontData;
                break;
            case gs_error_limitcheck:
                code = errorInternalOverflow;
                break;
            case gs_error_nocurrentpoint:
                code = errorCurrentCursorUndefined;
                break;
            case gs_error_rangecheck:
                code = errorIllegalAttributeValue;
                break;
            case gs_error_VMerror:
                code = errorInsufficientMemory;
                break;
            }
        break;              /* may need more input later */
    }
    return code;
}

static int
pxl_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}


/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
static int
pxl_impl_flush_to_eoj(pl_interp_implementation_t * impl,
                      stream_cursor_read * cursor)
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
/* ret 0 or +ve if ok, else -ve error code */
static int
pxl_impl_process_eof(pl_interp_implementation_t * impl)
{
    pxl_interp_instance_t *pxli = impl->interp_client_data;

    px_state_cleanup(pxli->pxs);
    px_process_init(pxli->st, true);

    return 0;
}

/* Report any errors after running a job */
/* ret 0 ok, else -ve error code */
static int
pxl_impl_report_errors(pl_interp_implementation_t * impl,
                       int code,
                       long file_position,
                       bool force_to_cout)
{
    pxl_interp_instance_t *pxli = impl->interp_client_data;
    px_parser_state_t *st = pxli->st;
    px_state_t *pxs = pxli->pxs;
    int report = pxs->error_report;
    const char *subsystem = (code <= px_error_next ? "KERNEL" : "GRAPHICS");
    char message[px_max_error_line + 1];
    int N = 0;
    int y = 0;

    if (code >= 0)
        return code;            /* not really an error */
    if (report & eErrorPage) {
        int ecode = px_begin_error_page(pxs, &y);
        if (ecode < 0) {
            return ecode;
        }
    }
    while ((N = px_error_message_line(message, N, subsystem,
                                      code, st, pxs)) >= 0) {
        if ((report & eBackChannel) || force_to_cout)
            errprintf(pxli->memory, "%s", message);
        if (report & eErrorPage)
            y = px_error_page_show(message, y, pxs);
    }
    if (((report & pxeErrorReport_next) && file_position != -1L)
        || force_to_cout)
        errprintf(pxli->memory, "file position of error = %ld\n",
                  file_position);
    if (report & eErrorPage) {
        px_args_t args;

        args.pv[0] = 0;
        pxEndPage(&args, pxs);
    }
    px_reset_errors(pxs);

    return code;
}

/* Wrap up interp instance after a "job" */
/* ret 0 ok, else -ve error code */
static int
pxl_impl_dnit_job(pl_interp_implementation_t * impl)
{
    int code;
    pxl_interp_instance_t *pxli = impl->interp_client_data;
    gx_device *device = gs_currentdevice(pxli->pgs);

    px_stream_header_dnit(&pxli->headerState);
    px_state_cleanup(pxli->pxs);
    px_process_init(pxli->st, true);
    /* return to original gstate  */
    code = gs_grestore_only(pxli->pgs);        /* destroys gs_save stack */

    /* Warn the device that ROP usage has come to an end */
    if (code >= 0) {
        code = put_param_bool(pxli, "LanguageUsesROPs", false);

        if (!device->is_open)
            code = gs_opendevice(device);
    }

    return code;
}

/* Deallocate a interpreter instance */
/* ret 0 ok, else -ve error code */
static int
pxl_impl_deallocate_interp_instance(pl_interp_implementation_t * impl)
{
    pxl_interp_instance_t *pxli = impl->interp_client_data;
    gs_memory_t *mem = pxli->memory;

    px_dict_release(&pxli->pxs->font_dict);
    px_dict_release(&pxli->pxs->builtin_font_dict);
    /* do total dnit of interp state */
    px_state_finit(pxli->pxs);
    /* free halftone cache */
    gs_gstate_free(pxli->pgs);
    px_process_release(pxli->st);
    px_state_release(pxli->pxs);
    gs_free_object(mem, pxli,
                   "pxl_impl_deallocate_interp_instance(pxl_interp_instance_t)");
    return 0;
}

/*
 * End-of-page called back by PXL
 */
static int
pxl_end_page_top(px_state_t * pxls, int num_copies, int flush)
{
    return pl_finish_page(pxls->memory->gs_lib_ctx->top_of_system,
                          pxls->pgs, num_copies, flush);
}

/* Parser implementation descriptor */
pl_interp_implementation_t pxl_implementation = {
    pxl_impl_characteristics,
    pxl_impl_allocate_interp_instance,
    NULL,                       /* get_device_memory */
    NULL,                       /* set_param */
    NULL,                       /* add_path */
    NULL,                       /* post_args_init */
    pxl_impl_init_job,
    NULL,                       /* run_prefix_commands */
    NULL,                       /* process_file */
    pxl_impl_process_begin,
    pxl_impl_process,
    pxl_impl_process_end,
    pxl_impl_flush_to_eoj,
    pxl_impl_process_eof,
    pxl_impl_report_errors,
    pxl_impl_dnit_job,
    pxl_impl_deallocate_interp_instance,
    NULL,                       /* pxl_impl_reset */
    NULL                        /* interp_client_data */
};

/* ---------- Utility Procs ----------- */
/* Initialize the parser state, and session parameters in case we get */
/* an error before the session is established. */
static int
px_top_init(px_parser_state_t * st, px_state_t * pxs, bool big_endian)
{
    px_args_t args;
    px_value_t v[3];

    px_process_init(st, big_endian);

    /* Measure */
    v[0].type = pxd_scalar | pxd_ubyte;
    v[0].value.i = eInch;
    args.pv[0] = &v[0];
    /* UnitsPerMeasure */
    v[1].type = pxd_xy | pxd_uint16;
    v[1].value.ia[0] = v[1].value.ia[1] = 100;  /* arbitrary */
    args.pv[1] = &v[1];
    /* ErrorReporting */
    v[2].type = pxd_scalar | pxd_ubyte;
    v[2].value.i = eErrorPage;
    args.pv[2] = &v[2];
    {
        int code = pxBeginSession(&args, pxs);

        if (code < 0)
            return code;
    }
    return 0;
}
