/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$*/

/* Top-level API implementation of METRO */

#include "memory_.h"
#include "metstate.h"
#include "metparse.h"
#include "zipstate.h"
#include "pltop.h"
#include "gsstate.h"
#include "gserrors.h"
#include "gspaint.h"

/* ------------ METRO wrapper handling here  ------- */


/* ------------ METRO wrapper handling done  ------- */


/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

/*
 * METRO interpeter: derived from pl_interp_t
 */
typedef struct met_interp_s {
    pl_interp_t              pl;               /* common part: must be first */
    gs_memory_t              *pmemory;         /* memory allocator to use */
} met_interp_t;

/*
 * MET interpreter instance: derived from pl_interp_instance_t
 */
typedef struct met_interp_instance_s {
    pl_interp_instance_t     pl;                /* common part: must be first */
    gs_memory_t              *pmemory;          /* memory allocator to use */
    met_parser_state_t       *pst;              /* parser state */
    zip_state_t              *pzip;             /* zip state */
    met_state_t              *pmets;            /* interp state */
    gs_state                 *pgs;              /* grafix state */
    pl_page_action_t         pre_page_action;   /* action before page out */
    void                     *pre_page_closure; /* closure to call pre_page_action with */
    pl_page_action_t         post_page_action;  /* action before page out */
    void                     *post_page_closure;/* closure to call post_page_action with */
} met_interp_instance_t;


/* Get implemtation's characteristics */
const pl_interp_characteristics_t * /* always returns a descriptor */
met_impl_characteristics(
  const pl_interp_implementation_t *pimpl     /* implementation of interpereter to alloc */
)
{
    /* version and build date are not currently used */
#define METVERSION NULL
#define METBUILDDATE NULL
  static pl_interp_characteristics_t met_characteristics = {
    "METRO",
    // "\0xef\0xbb\0xbf<", /* UTF8 metro */
    "\0x50\0x4b", /* zip metro */
    "Artifex",
    METVERSION,
    METBUILDDATE,
    MET_PARSER_MIN_INPUT_SIZE, /* Minimum input size */
  };
  return &met_characteristics;
}

/* Don't need to do anything to MET interpreter */
private int   /* ret 0 ok, else -ve error code */
met_impl_allocate_interp(
  pl_interp_t                      **ppinterp,       /* RETURNS abstract interpreter struct */
  const pl_interp_implementation_t *pimpl,  /* implementation of interpereter to alloc */
  gs_memory_t                      *pmem            /* allocator to allocate interp from */
)
{
    static pl_interp_t interp;      /* there's only one interpreter */

    /* There's only one MET interp, so return the static */
    *ppinterp = &interp;
    return 0;   /* success */
}

/* 
 * End-of-page called back by METRO
 */
private int
met_end_page_top(met_state_t *pmets,
                 int         num_copies,
                 int         flush
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)(pmets->client_data);
    pl_interp_instance_t *pinstance = (pl_interp_instance_t *)pmeti;
    int code = 0;

    /* do pre-page action */
    if (pmeti->pre_page_action) {
        code = pmeti->pre_page_action(pinstance, pmeti->pre_page_closure);
        if (code < 0)
            return code;
        if (code != 0)
            return 0;    /* code > 0 means abort w/no error */
    }

    /* output the page */
    code = gs_output_page(pmeti->pgs, num_copies, flush);
    if (code < 0)
        return code;

    /* do post-page action */ 
    if (pmeti->post_page_action) {
        code = pmeti->post_page_action(pinstance, pmeti->post_page_closure);
        if (code < 0)
            return code;
    }
    return 0;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
private int   /* ret 0 ok, else -ve error code */
met_impl_allocate_interp_instance(
  pl_interp_instance_t   **ppinstance,   /* RETURNS instance struct */
  pl_interp_t            *pinterp,       /* dummy interpreter */
  gs_memory_t            *pmem           /* allocator to allocate instance from */
)
{
    /* Allocate everything up front */
    met_interp_instance_t *pmeti  /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
        = (met_interp_instance_t *)gs_alloc_bytes( pmem,
                                                   sizeof(met_interp_instance_t),
                                                   "met_allocate_interp_instance(met_interp_instance_t)"
                                                   );
    gs_state *pgs = gs_state_alloc(pmem);
    met_parser_state_t *pst = met_process_alloc(pmem);    /* parser init, cheap */
    met_state_t *pmets = met_state_alloc(pmem);      /* inits interp state */
    /* If allocation error, deallocate & return NB this doesn't
       make sense see all languages. */
    if (!pmeti || !pgs || !pst || !pmets) {
        if (!pmeti)
            gs_free_object(pmem, pmeti, "met_impl_allocate_interp_instance(met_interp_instance_t)");
        if (!pgs)
            gs_state_free(pgs);
        if (!pst)
            met_process_release(pst);
        if (!pmets)
            met_state_release(pmets);
        return gs_error_VMerror;
    }

    /* Setup pointers to allocated mem within instance */
    pmeti->pmemory = pmem;
    pmeti->pgs = pgs;
    pmeti->pmets = pmets;
    pmeti->pst = pst;

    /* zero-init pre/post page actions for now */
    pmeti->pre_page_action = 0;
    pmeti->post_page_action = 0;

    /* General init of met_state */
    met_state_init(pmets, pgs);
    pmets->client_data = pmeti;
    pmets->end_page = met_end_page_top;

    pmeti->pzip = zip_init_instance(pmem);

    /* Return success */
    *ppinstance = (pl_interp_instance_t *)pmeti;
    return 0;
}

/* Set a client language into an interperter instance */
private int   /* ret 0 ok, else -ve error code */
met_impl_set_client_instance(
  pl_interp_instance_t         *pinstance,     /* interp instance to use */
  pl_interp_instance_t         *pclient,       /* client to set */
  pl_interp_instance_clients_t which_client
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    if ( which_client == PJL_CLIENT )
        pmeti->pmets->pjls = pclient;
    /* ignore unknown clients */
    return 0;
}

/* Set an interpreter instance's pre-page action */
private int   /* ret 0 ok, else -ve err */
met_impl_set_pre_page_action(
  pl_interp_instance_t   *pinstance,    /* interp instance to use */
  pl_page_action_t       action,        /* action to execute (rets 1 to abort w/o err) */
  void                   *closure       /* closure to call action with */
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    pmeti->pre_page_action = action;
    pmeti->pre_page_closure = closure;
    return 0;
}

/* Set an interpreter instance's post-page action */
private int   /* ret 0 ok, else -ve err */
met_impl_set_post_page_action(
  pl_interp_instance_t   *pinstance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute */
  void                   *closure       /* closure to call action with */
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    pmeti->post_page_action = action;
    pmeti->post_page_closure = closure;
    return 0;
}

/* Set a device into an interperter instance */
private int   /* ret 0 ok, else -ve error code */
met_impl_set_device(
  pl_interp_instance_t   *pinstance,     /* interp instance to use */
  gx_device              *pdevice        /* device to set (open or closed) */
)
{
    int code;
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    enum {Sbegin, Ssetdevice, Sinitg, Sgsave, Serase, Sdone} stage;
    stage = Sbegin;
    gs_opendevice(pdevice);

    /* Set the device into the gstate */
    stage = Ssetdevice;
    if ((code = gs_setdevice_no_erase(pmeti->pgs, pdevice)) < 0)  /* can't erase yet */
        goto pisdEnd;

    /* Init XL graphics */
    stage = Sinitg;
    /* NB met initialize graphics ?? */

    gs_setaccuratecurves(pmeti->pgs, true);  /* NB not sure */

    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    stage = Sgsave;
    if ( (code = gs_gsave(pmeti->pgs)) < 0)
        goto pisdEnd;

    stage = Serase;
    if ( (code = gs_erasepage(pmeti->pgs)) < 0 )
        goto pisdEnd;

    stage = Sdone;      /* success */

    /* Unwind any errors */
 pisdEnd:
    switch (stage) {
    case Sdone: /* don't undo success */
        break;

    case Serase:        /* gs_erasepage failed */
        /* undo  gsave */
        gs_grestore_only(pmeti->pgs);     /* destroys gs_save stack */
        /* fall thru to next */
    case Sgsave:        /* gsave failed */
    case Sinitg:
        /* undo setdevice */
        gs_nulldevice(pmeti->pgs);
        /* fall thru to next */

    case Ssetdevice:    /* gs_setdevice failed */
    case Sbegin:        /* nothing left to undo */
        break;
    }
    return code;
}

private int   
met_impl_get_device_memory(
  pl_interp_instance_t   *pinstance,     /* interp instance to use */
  gs_memory_t **ppmem)
{
    return 0;
}

/* Prepare interp instance for the next "job" */
private int     /* ret 0 ok, else -ve error code */
met_impl_init_job(
        pl_interp_instance_t   *pinstance         /* interp instance to start job in */
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    /* do something with pmeti to stop the compiler unused var warning */
    if (!pmeti)
        return -1;
    else
        return 0;
}

/* Parse a cursor-full of data */
private int     /* ret 0 or +ve if ok, else -ve error code */
met_impl_process(
        pl_interp_instance_t *pinstance,        /* interp instance to process data job in */
        stream_cursor_read   *pcursor           /* data to process */
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    int code = 0;
    if (pmeti->pzip->zip_mode) 
	zip_process(pmeti->pst, pmeti->pmets, pmeti->pzip, pcursor);
    else
	met_process(pmeti->pst, pmeti->pmets, pmeti->pzip, pcursor);


    return code;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
private int
met_impl_flush_to_eoj(
        pl_interp_instance_t *pinstance,        /* interp instance to flush for */
        stream_cursor_read   *pcursor           /* data to process */
)
{
    /* assume met can be pjl embedded, probably a bad assumption */
    const byte *p = pcursor->ptr;
    const byte *prlimit = pcursor->limit;

    /* Skip to, but leave UEL in buffer for PJL to find later */
    for (; p < prlimit; ++p)
        if (p[1] == '\033') {
            uint avail = prlimit - p;

            if (memcmp(p + 1, "\033%-12345X", min(avail, 9)))
                continue;
            if (avail < 9)
                break;
            pcursor->ptr = p;
            return 1;  /* found eoj */
        }
    pcursor->ptr = p;
    return 0;  /* need more */
}

/* Parser action for end-of-file */
private int     /* ret 0 or +ve if ok, else -ve error code */
met_impl_process_eof(
        pl_interp_instance_t *pinstance        /* interp instance to process data job in */
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    met_process_shutdown(pmeti->pst);
    return 0;
}

/* Report any errors after running a job */
private int   /* ret 0 ok, else -ve error code */
met_impl_report_errors(
        pl_interp_instance_t *pinstance,         /* interp instance to wrap up job in */
        int                  code,              /* prev termination status */
        long                 file_position,     /* file position of error, -1 if unknown */
        bool                 force_to_cout      /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
private int     /* ret 0 ok, else -ve error code */
met_impl_dnit_job(
        pl_interp_instance_t *pinstance         /* interp instance to wrap up job in */
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    /* do something with pmeti to stop the compiler unused var warning */
    if (!pmeti)
        return -1;
    else
        return 0;
}

/* Remove a device from an interperter instance */
private int   /* ret 0 ok, else -ve error code */
met_impl_remove_device(
  pl_interp_instance_t   *pinstance     /* interp instance to use */
)
{
    int code = 0;       /* first error status encountered */
    int error;
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    /* return to original gstate  */
    gs_grestore_only(pmeti->pgs);        /* destroys gs_save stack */
    /* Deselect device */
    /* NB */
    error = gs_nulldevice(pmeti->pgs);
    if (code >= 0)
        code = error;

    return code;
}

/* Deallocate a interpreter instance */
private int   /* ret 0 ok, else -ve error code */
met_impl_deallocate_interp_instance(
  pl_interp_instance_t   *pinstance     /* instance to dealloc */
)
{
    met_interp_instance_t *pmeti = (met_interp_instance_t *)pinstance;
    gs_memory_t *pmem = pmeti->pmemory;
        
    met_process_release(pmeti->pst);
    met_state_release(pmeti->pmets);
    gs_free_object(pmem, pmeti, "met_impl_deallocate_interp_instance(met_interp_instance_t)");
    return 0;
}

/* Do static deinit of MET interpreter */
private int   /* ret 0 ok, else -ve error code */
met_impl_deallocate_interp(
  pl_interp_t        *pinterp       /* interpreter to deallocate */
)
{
    /* nothing to do */
    return 0;
}


/* Parser implementation descriptor */
const pl_interp_implementation_t met_implementation = {
  met_impl_characteristics,
  met_impl_allocate_interp,
  met_impl_allocate_interp_instance,
  met_impl_set_client_instance,
  met_impl_set_pre_page_action,
  met_impl_set_post_page_action,
  met_impl_set_device,
  met_impl_init_job,
  met_impl_process,
  met_impl_flush_to_eoj,
  met_impl_process_eof,
  met_impl_report_errors,
  met_impl_dnit_job,
  met_impl_remove_device,
  met_impl_deallocate_interp_instance,
  met_impl_deallocate_interp,
  met_impl_get_device_memory,
};

