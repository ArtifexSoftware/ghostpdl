/* Portions Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pctop.c - PCL5c top-level API */

/* 
   NOTICE from JD: There is still some work to do pending improved PCL
   de/init. Those places are commented and flagged with @@@. The
   following documents some of those limitations:

   The remaining de-init issues fall into 3 categories, in order of urgency:
   i) parser state cleanup after unexpected input stream termination
   ii) de-init to enable device-specific re-init when switching devices
   iii) complete de-init to recover all memory allocated to PCL

   Issue (i): There is currently no way to reset the state of PCL in
   mid-flight, say if there's an unexpected EOF and the interpreter is
   currently defining a macro or a font (this is not an exhaustive list
   of trouble spots). What is needed is a new PCL function which specifically
   does the appropriate resets to "reasonably" start processing a new stream
   of data. HP does not document just how far such implied resets 
   go -- probably not as far as an <esc>E reset does -- so some experiments 
   with HP hardware will be necessary.

   Issue (ii), There is a class of resets which are currently done by calling 
   pcl_do_resets(pcs, pcl_reset_initial), followed by 
   pcl_load_built_in_symbol_sets(pcs). A device must have been selected into 
   the associated gstate before those resets are invoked, since some of the reset 
   actions are device-specific. The upshot of this arrangement is that some reset 
   actions must be carried out again when a new device is selected into the 
   interpreter. Unfortunately, the two above functions can only be called once 
   at the beginning of the interpreter's lifetime, and there are no functions 
   to terminate the interpreter. 

   There are 4 possible ways modify PCL to fix the problem: a) make the functions 
   in question callable multiple times, b) create special re-init functions that 
   are callable multiple times, c) create de-init functions that are a mirror 
   image of the initial resets; to re-init, one would de-init, then do initial 
   resets again, d) make it possible to de-init the entire interpreter; one would 
   destroy the interpreter, then start another. According to Henry at Artifex, 
   c & d would have some performance hits, though probably acceptable when 
   switching devices. Options a & b are interchangeable and don't suffer from the 
   same performance hits, but don't help with issue (iii), below.

   Issue (iii): PCL does two layers of 1-time init at startup, both of
   which allocate memory: I) the "static" inits called by the various
   pcl_init_table[]->do_init's, II) the above-mentioned pcl_do_resets
   (pcs, pcl_reset_initial), followed by
   pcl_load_built_in_symbol_sets(pcs).  In both cases, no
   corresponding function exists to free the allocated memory, so
   reallocation functions are needed. If resolution (c) to issue (ii)
   is adopted, you only need a function to undo (I). Otherwise, you
   need a complete de-init function(s).  My initial feeling is that
   it'd be quickest to implement resolution (c) to issue (ii), and
   resolve issue (iii) by implementing a function to undo (I). 
*/

#include "malloc_.h"
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"
#include "scommon.h"			/* for pparse.h */
#include "pcparse.h"
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
#include "gdevbbox.h"
#include "pclver.h"
#include "pjparse.h"
#include "pltop.h"
#include "pctop.h"

/* Define the table of pointers to initialization data. */
#define init_(init) extern const pcl_init_t init;
#include "pconfig.h"
#undef init_

const pcl_init_t *    pcl_init_table[] = {
#define init_(init) &init,
#include "pconfig.h"
#undef init_
    0
};



/* Built-in symbol set initialization procedure */
extern  bool    pcl_load_built_in_symbol_sets( pcl_state_t * );
private int pcl_end_page_top(P3(pcl_state_t *pcs, int num_copies, int flush));


/*
 * Define the gstate client procedures.
 */
  private void *
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
   void
pcl_set_target_device(pcl_state_t *pcs, gx_device *pdev)
{
    pcs->ptarget_device = pdev;
}

  gx_device *
pcl_get_target_device(pcl_state_t *pcs)
{
    return pcs->ptarget_device;
}

  private int
pcl_gstate_client_copy_for(
    void *                  to,
    void *                  from,
    gs_state_copy_reason_t  reason
)
{
    return 0;
}

  private void
pcl_gstate_client_free(
    void *          old,
    gs_memory_t *   mem
)
{}

private const gs_state_client_procs pcl_gstate_procs = {
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
    pcl_state_t               pcs;              /* pcl state */
    pcl_parser_state_t        pst;               /* parser state */
    gx_device_bbox            *bbox_device;      /* device used to detect blank pages */
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
  static pl_interp_characteristics_t pcl_characteristics = {
    "PCL5",
    "Artifex",
    PCLVERSION,
    PCLBUILDDATE,
    17      /* size of min buffer == sizeof UEL */
  };
  return &pcl_characteristics;
}

/* Do static init of PCL interpreter, since there's nothing to allocate */
private int   /* ret 0 ok, else -ve error code */
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
private int   /* ret 0 ok, else -ve error code */
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
    gx_device_bbox *bbox = gs_alloc_struct_immovable(
						     mem,
						     gx_device_bbox,
						     &st_device_bbox,
						     "pcl_allocate_interp_intance(bbox device)"
						     );
    gs_state *pgs = gs_state_alloc(mem);

    /* If allocation error, deallocate & return */
    if (!pcli || !bbox || !pgs) {
	if (!pcli)
	    gs_free_object(mem, pcli, "pcl_allocate_interp_instance(pcl_interp_instance_t)");
	if (!bbox)
	    gs_free_object(mem, bbox, "pcl_allocate_interp_intance(bbox device)");
	if (!pgs)
	    gs_state_free(pgs);
	return gs_error_VMerror;
    }

    /* Setup pointers to allocated mem within instance */
    pcli->bbox_device = bbox;
    pcli->memory = mem;

    /* start off setting everything to 0.  NB this should not be here
       at all, but I don't have time to analyze the consequences of
       removing it. */
    memset( &pcli->pcs, 0, sizeof(pcl_state_t) );

    /* zero-init pre/post page actions for now */
    pcli->pre_page_action = 0;
    pcli->post_page_action = 0;

    /* General init of pcl_state */
    pcl_init_state(&pcli->pcs, mem);
    pcli->pcs.client_data = pcli;
    pcli->pcs.pgs = pgs;
    /* provide an end page procedure */
    pcli->pcs.end_page = pcl_end_page_top;
    /* Init gstate to point to pcl state */
    gs_state_set_client(pgs, &pcli->pcs, &pcl_gstate_procs);
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
private int   /* ret 0 ok, else -ve error code */
pcl_impl_set_client_instance(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_interp_instance_t   *client        /* client to set */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	pcli->pcs.pjls = client;
	return 0;
}

/* Set an interpreter instance's pre-page action */
private int   /* ret 0 ok, else -ve err */
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
private int   /* ret 0 ok, else -ve err */
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

/* Set a device into an interperter instance */
private int   /* ret 0 ok, else -ve error code */
pcl_impl_set_device(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gx_device              *device        /* device to set (open or closed) */
)
{
	int code;
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	enum {Sbegin, Ssetdevice, Sgsave1, Serase, Sreset, Sload, Sgsave2, Sdone} stage;

	/* Init the bbox device & wrap it around the real device */
	stage = Sbegin;
	gx_device_bbox_init(pcli->bbox_device, device);
	rc_increment(pcli->bbox_device);	/* prevent refcnt-delete from deleting this */

	/* Set the device into the pcl_state & gstate */
	stage = Ssetdevice;
	pcl_set_target_device(&pcli->pcs, device);
	code = gs_setdevice_no_erase(pcli->pcs.pgs, (gx_device *)pcli->bbox_device);
	if (code < 0 )	/* can't erase yet */
	  goto pisdEnd;

	/* Save a gstate with the original device == bbox config */
	stage = Sgsave1;
	if ( (code = gs_gsave(pcli->pcs.pgs)) < 0 )
	  goto pisdEnd;
	stage = Serase;
	if ( (code = gs_erasepage(pcli->pcs.pgs)) < 0 )
		goto pisdEnd;

	/* Do inits of gstate that may be reset by setdevice */
	/* PCL no longer uses the graphic library transparency mechanism */
	gs_setsourcetransparent(pcli->pcs.pgs, false);
	gs_settexturetransparent(pcli->pcs.pgs, false);
	gs_setaccuratecurves(pcli->pcs.pgs, true);	/* All H-P languages want accurate curves. */

	/* Do device-dependent pcl inits */
	/* These resets will not clear any "permanent" storage (fonts, macros) */
	/* One of these resets will also install an extra color-mapper device */
//@@@there is a potential problem because mem from resets is not dealloc'd
	stage = Sreset;
	//@@@remove color mapper in dnit (think should be in PCL d/nit code)?
	if ((code = pcl_do_resets(&pcli->pcs, pcl_reset_initial)) < 0 )
	  goto pisdEnd;
//@@@ possibly remove sload - loading symbol set should be part of reset process.
	stage = Sload;

	/* provide a PCL graphic state we can return to */
	stage = Sgsave2;
	if ( (code = pcl_gsave(&pcli->pcs)) < 0 )
	  goto pisdEnd;
	stage = Sdone;	/* success */

	/* Unwind any errors */
pisdEnd:
	switch (stage) {
	case Sdone:	/* don't undo success */
	  break;

	case Sgsave2:	/* 2nd gsave failed */
	  /*@@@ unload built-in sym sets  */
	  /* fall thru to next */

	case Sload:		/* load_built_in_symbol_sets failed */
	  /*@@@ undo do_resets */
	  /* fall thru to next */

	case Sreset:	/* pcl_do_resets failed */
	case Serase:	/* gs_erasepage failed */
	  /* undo 1st gsave */
	  gs_grestore_no_wraparound(pcli->pcs.pgs);	/* destroys gs_save stack */
	  /* fall thru to next */

	case Sgsave1:	/* 1st gsave failed */
	  /* undo setdevice */
	  gs_nulldevice(pcli->pcs.pgs);
	  /* fall thru to next */

	case Ssetdevice:	/* gs_setdevice failed */
	  /* undo bbox device init */
	  gs_closedevice((gx_device *)(pcli->bbox_device));
	  gx_device_bbox_release(pcli->bbox_device);	/* also removes target from bbox */
	  /* fall thru to next */

	case Sbegin:	/* nothing left to undo */
	  break;
	}
	return code;
}

/* Prepare interp instance for the next "job" */
private int	/* ret 0 ok, else -ve error code */
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
private int	/* ret 0 or +ve if ok, else -ve error code */
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
private int	
pcl_impl_flush_to_eoj(
	pl_interp_instance_t *instance,        /* interp instance to flush for */
	stream_cursor_read   *cursor           /* data to process */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
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
	  return 1;  /* found eoj */
	}
	cursor->ptr = p;
	return 0;  /* need more */
}

/* Parser action for end-of-file */
private int	/* ret 0 or +ve if ok, else -ve error code */
pcl_impl_process_eof(
	pl_interp_instance_t *instance        /* interp instance to process data job in */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	pcl_process_init(&pcli->pst);

/* @@@@force restore & cleanup if unexpected data end was encountered */	
	return 0;
}

/* Report any errors after running a job */
private int   /* ret 0 ok, else -ve error code */
pcl_impl_report_errors(
	pl_interp_instance_t *instance,         /* interp instance to wrap up job in */
   int                  code,              /* prev termination status */
   long                 file_position,     /* file position of error, -1 if unknown */
   bool                 force_to_cout,     /* force errors to cout */
   FILE                 *cout              /* stream for back-channel reports */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	byte buf[200];
	uint count;

	while ( (count = pcl_status_read(buf, sizeof(buf), &pcli->pcs)) != 0 )
	  fwrite(buf, 1, count, cout);
	fflush(cout);

	return 0;
}

/* Wrap up interp instance after a "job" */
private int	/* ret 0 ok, else -ve error code */
pcl_impl_dnit_job(
	pl_interp_instance_t *instance         /* interp instance to wrap up job in */
)
{
	int code;
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;

	return code;
}

/* Remove a device from an interperter instance */
private int   /* ret 0 ok, else -ve error code */
pcl_impl_remove_device(
  pl_interp_instance_t   *instance     /* interp instance to use */
)
{
	int code = 0;	/* first error status encountered */
	int error;
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;

	/* to help with memory leak detection, issue a reset */
	code = pcl_do_resets(&pcli->pcs, pcl_reset_printer);

	/*@@@ unload built-in sym sets  */
	/*@@@ undo do_resets */

	/* return to the original graphic state w/color mapper, bbox, target */
	error = pcl_grestore(&pcli->pcs);
#define DEVICE_NAME (gs_devicename(gs_currentdevice((pcli->pcs.pgs))))
	PL_ASSERT(strcmp(DEVICE_NAME, "special color mapper") == 0);
	if (code >= 0)
	  code = error;
	/* return to original gstate w/bbox, target */
	gs_grestore_no_wraparound(pcli->pcs.pgs);	/* destroys gs_save stack */
	PL_ASSERT(strcmp(DEVICE_NAME, "bbox") == 0);
#undef DEVICE_NAME
	/* Deselect bbox. Bbox has been prevented from auto-closing/deleting */
	error = gs_nulldevice(pcli->pcs.pgs);
	if (code >= 0)
	    code = error;
	error = gs_closedevice((gx_device *)pcli->bbox_device);
	if (code >= 0)
	  code = error;
	gx_device_bbox_release(pcli->bbox_device);	/* also removes target from bbox */

	return code;
}

/* Deallocate a interpreter instance */
private int   /* ret 0 ok, else -ve error code */
pcl_impl_deallocate_interp_instance(
  pl_interp_instance_t   *instance     /* instance to dealloc */
)
{
	pcl_interp_instance_t *pcli = (pcl_interp_instance_t *)instance;
	gs_memory_t *mem = pcli->memory;

	/*@@@ Do total deinit of interpreter instance */
	/* Get rid of permanent and internal objects */
	if ( pcl_do_resets(&pcli->pcs, pcl_reset_permanent) < 0 )
	    return -1;
	/* Unwind allocation */ 
	gs_state_free(pcli->pcs.pgs);
	gs_free_object(mem, pcli->bbox_device, "pcl_deallocate_interp_intance(bbox device)");
	gs_free_object(mem, pcli, "pcl_deallocate_interp_instance(pcl_interp_instance_t)");

	return 0;
}

/* Do static deinit of PCL interpreter */
private int   /* ret 0 ok, else -ve error code */
pcl_impl_deallocate_interp(
  pl_interp_t        *interp       /* interpreter to deallocate */
)
{
	pcl_interp_t *pi = (pcl_interp_t *)interp;

	/* Deinit interp */
	/*@@@ free memory */

	return 0;
}

/* 
 * End-of-page called back by PCL
 */
private int
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
	if (pcli->pre_page_action)
	  {
	  code = pcli->pre_page_action(instance, pcli->pre_page_closure);
	  if (code < 0)
	    return code;
	  if (code != 0)
	    return 0;    /* code > 0 means abort w/no error */
	  }

	/* output the page */
	code = gs_output_page(pcs->pgs, num_copies, flush);
	if (code < 0)
	  return code;

	/* do post-page action */
	if (pcli->post_page_action)
	  {
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
  pcl_impl_process,
  pcl_impl_flush_to_eoj,
  pcl_impl_process_eof,
  pcl_impl_report_errors,
  pcl_impl_dnit_job,
  pcl_impl_remove_device,
  pcl_impl_deallocate_interp_instance,
  pcl_impl_deallocate_interp
};
