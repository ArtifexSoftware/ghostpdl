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
/*$Id$ */

/* implement device switching NB */

/* pctop.c - PCL5c and pcl5e top-level API */

#include "malloc_.h"
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"
#include "scommon.h"			/* for pparse.h */
#include "pcparse.h"
#include "pcpage.h"
#include "pcstate.h"
#include "pldebug.h"
#include "gdebug.h"
#include "gsmatrix.h"		/* for gsstate.h */
#include "gsrop.h"
#include "gspaint.h"            /* for gs_erasepage */
#include "gsstate.h"
#include "gxalloc.h"
#include "gxdevice.h"
#include "gxstate.h"
#include "pjparse.h"
#include "pltop.h"
#include "pctop.h"
#include "pccrd.h"
#include "pcpalet.h"
#include "gsiccmanage.h"



/* Configuration table for modules */
extern const pcl_init_t  pcparse_init; 
extern const pcl_init_t  rtmisc_init; 
extern const pcl_init_t  rtraster_init; 
extern const pcl_init_t  pcjob_init; 
extern const pcl_init_t  pcpage_init; 
extern const pcl_init_t  pcfont_init; 
extern const pcl_init_t  pctext_init; 
extern const pcl_init_t  pcsymbol_init; 
extern const pcl_init_t  pcsfont_init; 
extern const pcl_init_t  pcmacros_init; 
extern const pcl_init_t  pcrect_init; 
extern const pcl_init_t  pcstatus_init; 
extern const pcl_init_t  pcmisc_init; 
extern const pcl_init_t  pcursor_init; 
extern const pcl_init_t  pcl_cid_init; 
extern const pcl_init_t  pcl_color_init; 
extern const pcl_init_t  pcl_udither_init; 
extern const pcl_init_t  pcl_frgrnd_init; 
extern const pcl_init_t  pcl_lookup_tbl_init; 
extern const pcl_init_t  pcl_palette_init; 
extern const pcl_init_t  pcl_pattern_init; 
extern const pcl_init_t  pcl_xfm_init; 
extern const pcl_init_t  pcl_upattern_init; 
extern const pcl_init_t  rtgmode_init; 
extern const pcl_init_t  pccprint_init; 
extern const pcl_init_t  pginit_init; 
extern const pcl_init_t  pgframe_init; 
extern const pcl_init_t  pgconfig_init; 
extern const pcl_init_t  pgvector_init; 
extern const pcl_init_t  pgpoly_init; 
extern const pcl_init_t  pglfill_init; 
extern const pcl_init_t  pgchar_init; 
extern const pcl_init_t  pglabel_init; 
extern const pcl_init_t  pgcolor_init; 
extern const pcl_init_t  fontpg_init;

const pcl_init_t *    pcl_init_table[] = {
    &pcparse_init,
    &rtmisc_init,
    &rtraster_init,
    &pcjob_init,
    &pcpage_init,
    &pcsymbol_init, /* NOTE symbols must be intialized before fonts */
    &pcfont_init,
    &pctext_init,
    &pcsfont_init,
    &pcmacros_init,
    &pcrect_init,
    &pcstatus_init,
    &pcmisc_init,
    &pcursor_init,
    &pcl_cid_init,
    &pcl_color_init,
    &pcl_udither_init,
    &pcl_frgrnd_init,
    &pcl_lookup_tbl_init,
    &pcl_palette_init,
    &pcl_pattern_init,
    &pcl_xfm_init,
    &pcl_upattern_init,
    &rtgmode_init,
    &pccprint_init,
    &pginit_init,
    &pgframe_init,
    &pgconfig_init,
    &pgvector_init,
    &pgpoly_init,
    &pglfill_init,
    &pgchar_init,
    &pglabel_init,
    &pgcolor_init,
    &fontpg_init,
    0
};

/*
 * Define the gstate client procedures.
 */
  static void *
pcl_gstate_client_alloc(
    gs_memory_t *   mem
)
{
    /*
     * We don't want to allocate anything here, but we don't have any way
     * to say we want to share the client data. Since this will only ever
     * be called once, return something random.
     */
    return (void *)1;
}

/*
 * set and get for pcl's target device.  This is the device at the end
 * of the pipeline.  
 */
  static int
pcl_gstate_client_copy_for(
    void *                  to,
    void *                  from,
    gs_state_copy_reason_t  reason
)
{
    return 0;
}

  static void
pcl_gstate_client_free(
    void *          old,
    gs_memory_t *   mem
)
{}

static const gs_state_client_procs pcl_gstate_procs = {
    pcl_gstate_client_alloc,
    0,				/* copy -- superseded by copy_for */
    pcl_gstate_client_free,
    pcl_gstate_client_copy_for
};

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

/*
 * PCL interpeter: derived from pl_interp_t
 */
typedef struct pcl_interp_s {
  pl_interp_t              pl;               /* common part: must be first */
  gs_memory_t              *memory;          /* memory allocator to use */
} pcl_interp_t;

/*
 * PCL interpreter instance: derived from pl_interp_instance_t
 */
typedef struct pcl_interp_instance_s {
    pl_interp_instance_t      pl;                /* common part: must be first */
    gs_memory_t               *memory;           /* memory allocator to use */
    pcl_state_t               pcs;               /* pcl state */
    pcl_parser_state_t        pst;               /* parser state */
    pl_page_action_t          pre_page_action;   /* action before page out */
    void                      *pre_page_closure; /* closure to call pre_page_action with */
    pl_page_action_t          post_page_action;  /* action before page out */
    void                      *post_page_closure;/* closure to call post_page_action with */
} pcl_interp_instance_t;


/* Get implemtation's characteristics */
const pl_interp_characteristics_t * /* always returns a descriptor */
pcl_impl_characteristics(
  const pl_interp_implementation_t *impl     /* implementation of interpereter to alloc */
)
{
#define PCLVERSION NULL
#define PCLBUILDDATE NULL
  static pl_interp_characteristics_t pcl_characteristics = {
    "PCL",
    "\033E",
    "Artifex",
    PCLVERSION,
    PCLBUILDDATE,
    17      /* size of min buffer == sizeof UEL */
  };
  return &pcl_characteristics;
}

/* yuck */
pcl_state_t *
pcl_get_gstate(pl_interp_instance_t *instance)
{
    pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
    return &pcli->pcs;
}

/* Do static init of PCL interpreter, since there's nothing to allocate */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_allocate_interp(
  pl_interp_t                      **interp,       /* RETURNS abstract interpreter struct */
  const pl_interp_implementation_t *impl,  /* implementation of interpereter to alloc */
  gs_memory_t                      *mem            /* allocator to allocate interp from */
)
{
    static pcl_interp_t pi;	/* there's only one interpreter possible, so static */
    /* There's only one possible PCL interp, so return the static */
    pi.memory = mem;
    *interp = (pl_interp_t *)&pi;
    return 0;   /* success */
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_allocate_interp_instance(
  pl_interp_instance_t   **instance,     /* RETURNS instance struct */
  pl_interp_t            *interp,        /* dummy interpreter */
  gs_memory_t            *mem            /* allocator to allocate instance from */
)
{
    /* Allocate everything up front */
    pcl_interp_instance_t *pcli  /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
	= (pcl_interp_instance_t *)gs_alloc_bytes( mem,
						   sizeof(pcl_interp_instance_t),
						   "pcl_allocate_interp_instance(pcl_interp_instance_t)"
						   );
    gs_state *pgs = gs_state_alloc(mem);    
#ifdef ICCBRANCH
    gsicc_init_iccmanager(pgs);
#endif
    /* If allocation error, deallocate & return */
    if (!pcli || !pgs) {
	if (pcli)
	    gs_free_object(mem, pcli, "pcl_allocate_interp_instance(pcl_interp_instance_t)");
	if (pgs)
	    gs_state_free(pgs);
	return gs_error_VMerror;
    }

    pcli->memory = mem;
    /* zero-init pre/post page actions for now */
    pcli->pre_page_action = 0;
    pcli->post_page_action = 0;
    /* General init of pcl_state */
    pcl_init_state(&pcli->pcs, mem);
    pcli->pcs.client_data = pcli;
    pcli->pcs.pgs = pgs;
    pcli->pcs.xfm_state.paper_size = 0; 
    /* provide an end page procedure */
    pcli->pcs.end_page = pcl_end_page_top;
    /* Init gstate to point to pcl state */
    gs_state_set_client(pgs, &pcli->pcs, &pcl_gstate_procs, false);
    /* register commands */
    {
	int code = pcl_do_registrations(&pcli->pcs, &pcli->pst);
	if ( code < 0 )
	    return(code);
    }

    /* Return success */
    *instance = (pl_interp_instance_t *)pcli;
    return 0;
}

/* Set a client language into an interperter instance */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_set_client_instance(
  pl_interp_instance_t         *instance,     /* interp instance to use */
  pl_interp_instance_t         *client,        /* client to set */
  pl_interp_instance_clients_t which_client
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
        
        if ( which_client == PJL_CLIENT )
            pcli->pcs.pjls = client;
        /* ignore unknown clients */
	return 0;
}

/* Set an interpreter instance's pre-page action */
static int   /* ret 0 ok, else -ve err */
pcl_impl_set_pre_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute (rets 1 to abort w/o err) */
  void                   *closure       /* closure to call action with */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	pcli->pre_page_action = action;
	pcli->pre_page_closure = closure;
	return 0;
}

/* Set an interpreter instance's post-page action */
static int   /* ret 0 ok, else -ve err */
pcl_impl_set_post_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute */
  void                   *closure       /* closure to call action with */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	pcli->post_page_action = action;
	pcli->post_page_closure = closure;
	return 0;
}

/* if the device option string PCL is not given, the default
   arrangement is 1 bit devices use pcl5e other devices use pcl5c. */
static pcl_personality_t
pcl_get_personality(pl_interp_instance_t *instance, gx_device *device)
{
    if ( !strcmp(instance->pcl_personality, "PCL5C" ) )
	return pcl5c;
    else if ( !strcmp(instance->pcl_personality, "PCL5E" ) )
	return pcl5e;
    else if ( !strcmp(instance->pcl_personality, "RTL" ) )
	return rtl;
    else if ( device->color_info.depth == 1 )
	return pcl5e;
    else
	return pcl5c;
}

static bool
pcl_get_interpolation(pl_interp_instance_t *instance)
{
    return instance->interpolate;
}

#include "plmain.h"

/* Set a device into an interperter instance */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_set_device(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gx_device              *device        /* device to set (open or closed) */
)
{
    /* NB REVIEW ME -- ROUGH DRAFT */
    int code;
    pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
    enum {Sbegin, Ssetdevice, Sinitg, Sgsave1, Spclgsave, Sreset, Serase, Sdone} stage;

    stage = Sbegin;
    /* get ad hoc paramaters personality and interpolation */
    pcli->pcs.personality = pcl_get_personality(instance, device);
    pcli->pcs.interpolate = pcl_get_interpolation(instance);

    /* Set the device into the pcl_state & gstate */

    stage = Ssetdevice;
    if ((code = gs_setdevice_no_erase(pcli->pcs.pgs, device)) < 0)	/* can't erase yet */
        goto pisdEnd;
#ifdef ICCBRANCH
    /* Initialize device ICC profile  */
    code = gsicc_init_device_profile(pcli->pcs.pgs, device);
    if (code < 0)
        return code;
#endif
    stage = Sinitg;
    /* Do inits of gstate that may be reset by setdevice */
    /* PCL no longer uses the graphic library transparency mechanism */
    gs_setsourcetransparent(pcli->pcs.pgs, false);
    gs_settexturetransparent(pcli->pcs.pgs, false);
    gs_setaccuratecurves(pcli->pcs.pgs, true);	/* All H-P languages want accurate curves. */
    gs_setfilladjust(pcli->pcs.pgs, 0, 0);

    stage = Sgsave1;
    if ( (code = gs_gsave(pcli->pcs.pgs)) < 0 )
	goto pisdEnd;
    stage = Serase;
    if ( (code = gs_erasepage(pcli->pcs.pgs)) < 0 )
	goto pisdEnd;
    /* Do device-dependent pcl inits */
    stage = Sreset;
    if ((code = pcl_do_resets(&pcli->pcs, pcl_reset_initial)) < 0 )
	goto pisdEnd;
    /* provide a PCL graphic state we can return to */
    stage = Spclgsave;
    if ( (code = pcl_gsave(&pcli->pcs)) < 0 )
	goto pisdEnd;
    stage = Sdone;	/* success */
    /* Unwind any errors */
 pisdEnd:
    switch (stage) {
    case Sdone:	/* don't undo success */
    case Sinitg: /* can't happen removes warning */
	break;

    case Spclgsave:	/* 2nd gsave failed */
	/* fall thru to next */
    case Sreset:	/* pcl_do_resets failed */
    case Serase:	/* gs_erasepage failed */
	/* undo 1st gsave */
	gs_grestore_only(pcli->pcs.pgs);	/* destroys gs_save stack */
	/* fall thru to next */

    case Sgsave1:	/* 1st gsave failed */
	/* undo setdevice */
	gs_nulldevice(pcli->pcs.pgs);
	/* fall thru to next */

    case Ssetdevice:	/* gs_setdevice failed */
    case Sbegin:	/* nothing left to undo */
	break;
    }
    return code;
}

static int   
pcl_impl_get_device_memory(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gs_memory_t **pmem)
{
    return 0;
}
  

/* Prepare interp instance for the next "job" */
static int	/* ret 0 ok, else -ve error code */
pcl_impl_init_job(
	pl_interp_instance_t   *instance         /* interp instance to start job in */
)
{
	int code = 0;
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	pcl_process_init(&pcli->pst);
	return code;
}

/* Parse a cursor-full of data */
static int	/* ret 0 or +ve if ok, else -ve error code */
pcl_impl_process(
	pl_interp_instance_t *instance,        /* interp instance to process data job in */
	stream_cursor_read   *cursor           /* data to process */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	int code = pcl_process(&pcli->pst, &pcli->pcs, cursor);
	return code;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
static int	
pcl_impl_flush_to_eoj(
	pl_interp_instance_t *instance,        /* interp instance to flush for */
	stream_cursor_read   *cursor           /* data to process */
)
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
	    return 1;  /* found eoj */
	}
    cursor->ptr = p;
    return 0; /* need more data */
}

/* Parser action for end-of-file */
static int	/* ret 0 or +ve if ok, else -ve error code */
pcl_impl_process_eof(
	pl_interp_instance_t *instance        /* interp instance to process data job in */
)
{
        int code;
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	pcl_process_init(&pcli->pst);
	code = pcl_end_page_if_marked(&pcli->pcs);
	if ( code < 0 )
	    return code;
        /* force restore & cleanup if unexpected data end was encountered */
	return 0;
}

/* Report any errors after running a job */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_report_errors(
	pl_interp_instance_t *instance,         /* interp instance to wrap up job in */
   int                  code,              /* prev termination status */
   long                 file_position,     /* file position of error, -1 if unknown */
   bool                 force_to_cout      /* force errors to cout */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	byte buf[200];
	uint count;

	while ( (count = pcl_status_read(buf, sizeof(buf), &pcli->pcs)) != 0 )
	  errwrite((const char *)buf, count);

	return 0;
}

/* Wrap up interp instance after a "job" */
static int	/* ret 0 ok, else -ve error code */
pcl_impl_dnit_job(
	pl_interp_instance_t *instance         /* interp instance to wrap up job in */
)
{
	return 0;
}

/* Remove a device from an interperter instance */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_remove_device(
  pl_interp_instance_t   *instance     /* interp instance to use */
)
{
        int code;
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;

        /* NB use the PXL code */
	/* return to the original graphic state w/color mapper, bbox, target */
	code = pcl_grestore(&pcli->pcs);
	if (code < 0 )
	    dprintf1("error code %d restoring gstate, continuing\n", code );
	/* return to original gstate w/bbox, target */
	code = gs_grestore_only(pcli->pcs.pgs);	/* destroys gs_save stack */
	if (code < 0 )
	    dprintf1("error code %d destroying gstate, continuing\n", code );

	/* Deselect bbox. Bbox has been prevented from auto-closing/deleting */
	code = gs_nulldevice(pcli->pcs.pgs);
	if ( code < 0 )
	    dprintf1("error code %d installing nulldevice, continuing\n", code );
	return pcl_do_resets(&pcli->pcs, pcl_reset_permanent);
}

/* Deallocate a interpreter instance */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_deallocate_interp_instance(
  pl_interp_instance_t   *instance     /* instance to dealloc */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	gs_memory_t *mem = pcli->memory;
        /* free memory used by the parsers */
        if ( pcl_parser_shutdown(&pcli->pst, mem ) < 0 ) {
            dprintf("Undefined error shutting down parser, continuing\n" );
        }
        /* this should have a shutdown procedure like pcl above */
        gs_free_object(mem, 
                       pcli->pst.hpgl_parser_state,
                       "pcl_deallocate_interp_instance(pcl_interp_instance_t)");

        /* free default, pdflt_* objects */
	pcl_free_default_objects( mem, &pcli->pcs);

	/* free halftone cache in gs state */
	gs_state_free(pcli->pcs.pgs);
	/* remove pcl's gsave grestore stack */
	pcl_free_gstate_stk(&pcli->pcs);
	gs_free_object(mem, pcli,
		       "pcl_deallocate_interp_instance(pcl_interp_instance_t)");
	return 0;
}

/* Do static deinit of PCL interpreter */
static int   /* ret 0 ok, else -ve error code */
pcl_impl_deallocate_interp(
  pl_interp_t        *interp       /* interpreter to deallocate */
)
{
	/* Deinit interp */
	return 0;
}

/* 
 * End-of-page called back by PCL - NB now exported.
 */
 int
pcl_end_page_top(
    pcl_state_t *           pcs,
    int                     num_copies,
    int                     flush
)
{

    pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)(pcs->client_data);
    pl_interp_instance_t *instance = (pl_interp_instance_t *)pcli;
    int code = 0;
    /* do pre-page action */
    if (pcli->pre_page_action) {
        code = pcli->pre_page_action(instance, pcli->pre_page_closure);
        if ( code < 0 )
            return code;
        if ( code > 0 )
            /* don't print case */
            return 0;
    }

    /* output the page */
    code = gs_output_page(pcs->pgs, num_copies, flush);
    if (code < 0)
        return code;
    /* do post-page action */
    if (pcli->post_page_action) {
        code = pcli->post_page_action(instance, pcli->post_page_closure);
        if (code < 0)
	    return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t pcl_implementation = {
  pcl_impl_characteristics,
  pcl_impl_allocate_interp,
  pcl_impl_allocate_interp_instance,
  pcl_impl_set_client_instance,
  pcl_impl_set_pre_page_action,
  pcl_impl_set_post_page_action,
  pcl_impl_set_device,
  pcl_impl_init_job,
  NULL, /* process_file */
  pcl_impl_process,
  pcl_impl_flush_to_eoj,
  pcl_impl_process_eof,
  pcl_impl_report_errors,
  pcl_impl_dnit_job,
  pcl_impl_remove_device,
  pcl_impl_deallocate_interp_instance,
  pcl_impl_deallocate_interp,
  pcl_impl_get_device_memory
};
