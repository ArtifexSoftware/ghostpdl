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
/* $Id$ */

/* pxtop.c */
/* Top-level API implementation of PS Language Interface */

#include "stdio_.h"
#include "ghost.h"
#include "imain.h"
#include "imainarg.h"
#include "iapi.h"
#include "string_.h"
#include "gdebug.h"
#include "gserrors.h"
#include "../gs/src/errors.h"	/* FIXME: Microsoft seems to pull in <errors.h> */
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
/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

typedef struct ps_arg_list_s {
    struct ps_arg_list_s  *next;
    const char		  *string;
} ps_arg_list_t;

/*
 * PS interpreter instance: derived from pl_interp_instance_t
 */
typedef struct ps_interp_instance_s {
  pl_interp_instance_t     pl;               /* common part: must be first */
  gs_memory_t              *plmemory;        /* memory allocator to use with pl objects */
  gs_main_instance	   *minst;	     /* PS interp main instance */
  int			   params_set;	     /* == 0 when params need setting */
  gx_device		   *device;	     /* save for deferred param setting */
  gs_param_list		   *params;	     /* save for deferred param setting */
  int			   argc;	     /* count of ps specific args */
  ps_arg_list_t		   *ps_arg_head;     /* ps_only arg list */
  ps_arg_list_t		   *ps_arg_tail;     /* ps_only arg list */
} ps_interp_instance_t;

/* Get implemtation's characteristics */
const pl_interp_characteristics_t * /* always returns a descriptor */
ps_impl_characteristics(
  const pl_interp_implementation_t *impl     /* implementation of interpereter to alloc */
)
{
    /* version and build date are not currently used */
#define PSVERSION NULL
#define PSBUILDDATE NULL
  static pl_interp_characteristics_t ps_characteristics = {
    "PS",
    "%!",
    "Artifex",
    PSVERSION,
    PSBUILDDATE,
    1				/* minimum input size to PostScript */
  };
  return &ps_characteristics;
}

/* Don't need to do anything to PS interpreter */
private int   /* ret 0 ok, else -ve error code */
ps_impl_allocate_interp(
  pl_interp_t                      **interp,       /* RETURNS abstract interpreter struct */
  const pl_interp_implementation_t *impl,  /* implementation of interpereter to alloc */
  gs_memory_t                      *mem            /* allocator to allocate interp from */
)
{
	static pl_interp_t pi;	/* there's only one interpreter */

	/* There's only one PS interp, so return the static */
	*interp = &pi;
	return 0;   /* success */
}

/* Do per-instance interpreter allocation/init. No device is set yet */
private int   /* ret 0 ok, else -ve error code */
ps_impl_allocate_interp_instance(
  pl_interp_instance_t   **instance,     /* RETURNS instance struct */
  pl_interp_t            *interp,        /* dummy interpreter */
  gs_memory_t            *mem            /* allocator to allocate instance from */
)
{
	int code = 0, exit_code;
	int argc = 1;
	char *argv[1] = { "" };

	ps_interp_instance_t *psi  /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
	    = (ps_interp_instance_t *)gs_alloc_bytes( mem,
						      sizeof(ps_interp_instance_t),
						      "ps_allocate_interp_instance(ps_interp_instance_t)"
						      );
	/* If allocation error, deallocate & return */
	if (!psi) {
	  return gs_error_VMerror;
	}

	/* Setup pointer to mem used by PostScript */
	psi->plmemory = mem;
	psi->minst = gs_main_instance_default();
	psi->device = NULL;
	psi->params = NULL;
	psi->params_set = 0;
	psi->argc = 0;
	psi->ps_arg_head = NULL;
	psi->ps_arg_tail = NULL;
	code = gs_main_init_with_args(psi->minst, argc, argv);
	if (code<0)
	    return code;

	/* General init of PS interp instance */
        
	/* gs_memory_t_default = psi->minst->heap; */ /* while running PS interp */
	
	code = gsapi_run_string_begin(psi->minst, 0, &exit_code);

	/* Because PS uses its own 'heap' allocator, we restore to the pl 
	 * allocator before returning (PostScript will use minst->heap)	  
	 */
	
	/* gs_memory_t_default = psi->plmemory;
         */
	if (code<0)
	    return exit_code;

	/* Return success */
	*instance = (pl_interp_instance_t *)psi;
	return 0;
}

/* Set a client language into an interperter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_set_client_instance(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_interp_instance_t   *client        /* client to set */
)
{
	return 0;
}

private int   /* ret 0 arg not handled, +ve arg was handled, else -ve error code */
ps_impl_arg_handler(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  const char		 *string	/* argument string */
)
{

	ps_arg_list_t *newarg;
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

	/* check to see if PS handles this arg */
	if (string[0] != 'I')		/*** -I switch, just for example ***/
	    return 0;	/* not a PS argument  we handle */
	/* We just collect all arguments for when we actually need them */
	/* allocate a new arg list element, attach the data */
	newarg = (ps_arg_list_t *)gs_alloc_bytes(psi->plmemory,
		sizeof(ps_arg_list_t), "ps_impl_arg_handler(ps_arg_list)");
	if (!newarg)
	    return gs_error_VMerror;
	newarg->string = string;
	if (psi->argc++ == 0) {
	    psi->ps_arg_head = newarg;
	    psi->ps_arg_tail = newarg;
	} else {
	    psi->ps_arg_tail->next = newarg;
	}
	return 1;
}

/* Set an interpreter instance's pre-page action */
private int   /* ret 0 ok, else -ve err */
ps_impl_set_pre_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute (rets 1 to abort w/o err) */
  void                   *closure       /* closure to call action with */
)
{
    return 0;
}

/* Set an interpreter instance's post-page action */
private int   /* ret 0 ok, else -ve err */
ps_impl_set_post_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute */
  void                   *closure       /* closure to call action with */
)
{
	return 0;
}

private int
ps_impl_putdeviceparams(ps_interp_instance_t *psi)
{
	int code = gs_putdeviceparams(
		gs_currentdevice(psi->minst->i_ctx_p->pgs), psi->params);
	psi->params_set = 1;
	return code;
}

/* Set a device into an interperter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_set_device(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gx_device              *device        /* device to set (open or closed) */
)
{
	int code, exit_code;
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
	char tmpstring[256];

#if 0
	/*
	 * Set the device by having PostScript do it
	 * This insures that the device allocation, etc. will be correct
	 * for PostScript
	 */
	sprintf(tmpstring, "(%s) selectdevice\n", device->dname);
        gs_memory_t_default = psi->minst->heap;	/* while running PS interp */
	code = gsapi_run_string_continue(psi->minst, (const char *)tmpstring,
		strlen(tmpstring), 0, &exit_code);
	if (code == e_NeedInput)
	    code = 0;
	if (code < 0)
	    goto set_device_return;
	psi->device = device;
	/* We always set params after selecting a new device */
	code = ps_impl_putdeviceparams(psi);
set_device_return:
	psi->params_set = 1;
	gs_memory_t_default = psi->plmemory;	/* restore for pl */
#else
	/*
	 * gs_opendevice(device);
	 * code = gs_setdevice_no_erase(psi->minst->i_ctx_p->pgs, device);
	 * psi->device = device;
	 */
	/* hack into ps device 
	 */
	psi->device = psi->minst->i_ctx_p->pgs->device;
#endif
	return code;
}

/* Set parameters into PostScript's device */
private int   /* ret 0 ok, else -ve error code */
ps_impl_set_device_params(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gx_device              *device,       /* device to set (open or closed) */
  gs_param_list		 *params	/* parameters */
)
{
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

	psi->params = params;		/* save for deferred setting into device */
	psi->params_set = 0;		/* flag to say we need to set params */
	return 0;
}

/* Prepare interp instance for the next "job" */
private int	/* ret 0 ok, else -ve error code */
ps_impl_init_job(
	pl_interp_instance_t   *instance         /* interp instance to start job in */
)
{
	int code = 0, exit_code;

	/* Insert job server encapsulation here */

	return code;
}

private uint
scan_buffer_for_UEL(
	stream_cursor_read *cursor
)
{
	uint avail = cursor->limit - (cursor->ptr);

	return avail;
}

/* Parse a buffer full of data */
private int	/* ret 0 or +ve if ok, else -ve error code */
ps_impl_process(
	pl_interp_instance_t *instance,        /* interp instance to process data job in */
	stream_cursor_read   *cursor           /* data to process */
)
{
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
	int code, exit_code;
	uint avail;

	/* Process some input */
	avail = scan_buffer_for_UEL(cursor);

	/* Send the buffer to Ghostscript */
        gs_memory_t_default = psi->minst->heap;	/* while running PS interp */
	code = gsapi_run_string_continue(psi->minst, (const char *)(cursor->ptr + 1),
		avail, 0, &exit_code);
	gs_memory_t_default = psi->plmemory;	/* restore for pl */
	cursor->ptr += avail;
	if (code == e_NeedInput)
	   return 0;		/* Not an error */
	return (code < 0) ? exit_code : 0;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
private int
ps_impl_flush_to_eoj(
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
	return 0;  /* need more */
}

/* Parser action for end-of-file */
private int	/* ret 0 or +ve if ok, else -ve error code */
ps_impl_process_eof(
	pl_interp_instance_t *instance        /* interp instance to process data job in */
)
{
	int code = 0, exit_code;
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

        gs_memory_t_default = psi->minst->heap;	/* while running PS interp */
	/* put endjob logic here (finish encapsulation) */

	gs_memory_t_default = psi->plmemory;	/* restore for pl */

	return (code < 0) ? exit_code : 0;
}

/* Report any errors after running a job */
private int   /* ret 0 ok, else -ve error code */
ps_impl_report_errors(
	pl_interp_instance_t *instance,         /* interp instance to wrap up job in */
   int                  code,              /* prev termination status */
   long                 file_position,     /* file position of error, -1 if unknown */
   bool                 force_to_cout,     /* force errors to cout */
   FILE                 *cout              /* stream for back-channel reports */
)
{
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

	return code;
}

/* Wrap up interp instance after a "job" */
private int	/* ret 0 ok, else -ve error code */
ps_impl_dnit_job(
	pl_interp_instance_t *instance         /* interp instance to wrap up job in */
)
{
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

	return 0;
}

/* Remove a device from an interperter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_remove_device(
  pl_interp_instance_t   *instance     /* interp instance to use */
)
{
	int code = 0;	/* first error status encountered */
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

	return code;
}

/* Deallocate a interpreter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_deallocate_interp_instance(
  pl_interp_instance_t   *instance     /* instance to dealloc */
)
{
	int code = 0, exit_code;
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
	gs_memory_t *mem = psi->plmemory;
	
	/* do total dnit of interp state */
        gs_memory_t_default = psi->minst->heap;	/* while running PS interp */
	code = gsapi_run_string_end(psi->minst, 0, &exit_code); 
	gs_memory_t_default = psi->plmemory;	/* restore for pl */
	gs_free_object(mem, psi, "ps_impl_deallocate_interp_instance(ps_interp_instance_t)");

	return (code < 0) ? exit_code : 0;
}

/* Do static deinit of PS interpreter */
private int   /* ret 0 ok, else -ve error code */
ps_impl_deallocate_interp(
  pl_interp_t        *interp       /* interpreter to deallocate */
)
{
	/* nothing to do */
	return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t ps_implementation = {
  ps_impl_characteristics,
  ps_impl_allocate_interp,
  ps_impl_allocate_interp_instance,
  ps_impl_set_client_instance,
#if NOT_YET
  ps_impl_arg_handler,
#endif
  ps_impl_set_pre_page_action,
  ps_impl_set_post_page_action,
  ps_impl_set_device,
#if NOT_YET
  ps_impl_set_device_params,
#endif
  ps_impl_init_job,
  ps_impl_process,
  ps_impl_flush_to_eoj,
  ps_impl_process_eof,
  ps_impl_report_errors,
  ps_impl_dnit_job,
  ps_impl_remove_device,
  ps_impl_deallocate_interp_instance,
  ps_impl_deallocate_interp,
};
