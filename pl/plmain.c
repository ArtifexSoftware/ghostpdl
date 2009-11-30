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

/* plmain.c */
/* Main program command-line interpreter for PCL interpreters */
#include "string_.h"
#include "gdebug.h"
#include "gscdefs.h"
#include "gsio.h"
#include "gstypes.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "plalloc.h"
#include "gsmalloc.h"
#include "gsstruct.h"
#include "gxalloc.h"
#include "gsalloc.h"
#include "gsargs.h"
#include "gp.h"
#include "gsdevice.h"
#include "gxdevice.h"
#include "gsnogc.h"
#include "gsparam.h"
#include "gslib.h"
#include "pjtop.h"
#include "plparse.h"
#include "plplatf.h"
#include "plmain.h"
#include "pltop.h"
#include "pltoputl.h"
#include "plapi.h"
#include "gslibctx.h"
#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
#include "dwtrace.h"
#include "vdtrace.h"
#endif

/*
 * Define bookeeping for interperters and devices 
 */
typedef struct pl_main_universe_s {
    gs_memory_t             *mem;                /* mem alloc to dealloc devices */
    pl_interp_implementation_t const * const *
                            pdl_implementation;  /* implementations to choose from */
    pl_interp_instance_t *  pdl_instance_array[100];	/* parallel to pdl_implementation */
    pl_interp_t *           pdl_interp_array[100];	/* parallel to pdl_implementation */
    pl_interp_implementation_t const
                            *curr_implementation;
    pl_interp_instance_t *  curr_instance;
    gx_device               *curr_device;
} pl_main_universe_t;


/* Include the extern for the device list. */
extern_gs_lib_device_list();

/* Extern for PJL */
extern pl_interp_implementation_t pjl_implementation;

/* Extern for PDL(s): currently in one of: plimpl.c (XL & PCL), */
/* pcimpl.c (PCL only), or pximpl (XL only) depending on make configuration.*/
extern pl_interp_implementation_t const * const pdl_implementation[];	/* zero-terminated list */

/* Define the usage message. */
static const char *pl_usage = "\
Usage: %s [option* file]+...\n\
Options: -dNOPAUSE -E[#] -h -C -L<PCL|PCLXL> -K<maxK> -P<PCL5C|PCL5E|RTL> -Z...\n\
         -sDEVICE=<dev> -g<W>x<H> -r<X>[x<Y>] -d{First|Last}Page=<#>\n\
	 -sOutputFile=<file> (-s<option>=<string> | -d<option>[=<value>])*\n\
         -J<PJL commands>\n";

/* ---------------- Static data for memory management ------------------ */

static gs_gc_root_t device_root;

#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
void *hwndtext; /* Hack: Should be of HWND type. */
#endif

/* ---------------- Forward decls ------------------ */
/* Functions to encapsulate pl_main_universe_t */
int   /* 0 ok, else -1 error */
pl_main_universe_init(
	pl_main_universe_t     *universe,            /* universe to init */
	char                   *err_str,             /* RETURNS error str if error */
	gs_memory_t            *mem,                 /* deallocator for devices */
	pl_interp_implementation_t const * const
	                       pdl_implementation[], /* implementations to choose from */
	pl_interp_instance_t   *pjl_instance,        /* pjl to reference */
	pl_main_instance_t     *inst,                /* instance for pre/post print */
	pl_page_action_t       pl_pre_finish_page,   /* pre-page action */
	pl_page_action_t       pl_post_finish_page   /* post-page action */
);
int   /* 0 ok, else -1 error */
pl_main_universe_dnit(
	pl_main_universe_t     *universe,            /* universe to dnit */
	char                   *err_str              /* RETRUNS errmsg if error return */
);
pl_interp_instance_t *    /* rets current interp_instance, 0 if err */
pl_main_universe_select(
	pl_main_universe_t               *universe,              /* universe to select from */
	char                             *err_str,               /* RETURNS error str if error */
	pl_interp_instance_t             *pjl_instance,          /* pjl */
	pl_interp_implementation_t const *desired_implementation,/* impl to select */
        pl_main_instance_t               *pti,                   /* inst contains device */
	gs_param_list                    *params                 /* device params to use */
);

static pl_interp_implementation_t const *
pl_auto_sense(
   const char*                      name,         /* stream  */
   int                              buffer_length, /* length of stream */
   pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
);

static pl_interp_implementation_t const *
pl_select_implementation(
  pl_interp_instance_t *pjl_instance,
  pl_main_instance_t *pmi, 
  pl_top_cursor_t r
);


/* Process the options on the command line. */
static FILE *pl_main_arg_fopen(const char *fname, void *ignore_data);

/* Initialize the instance parameters. */
void pl_main_init_instance(pl_main_instance_t *pmi, gs_memory_t *memory);
void pl_main_reinit_instance(pl_main_instance_t *pmi);

/* Process the options on the command line, including making the
   initial device and setting its parameters.  */
int pl_main_process_options(pl_main_instance_t *pmi, arg_list *pal,
			    gs_c_param_list *params,
			    pl_interp_instance_t *pjl_instance,
			    pl_interp_implementation_t const * const impl_array[],
			    char **filename);

/* Find default language implementation */
pl_interp_implementation_t const *
pl_auto_sense(const char* buf, int buf_len, pl_interp_implementation_t const * const impl_array[]);

static pl_interp_implementation_t const *
pl_pjl_select(pl_interp_instance_t *pjl_instance,
	      pl_interp_implementation_t const * const impl_array[]);

/* Pre-page portion of page finishing routine */
int	/* ret 0 if page should be printed, 1 if no print, else -ve error */
pl_pre_finish_page(pl_interp_instance_t *interp, void *closure);

/* Post-page portion of page finishing routine */
int	/* ret 0, else -ve error */
pl_post_finish_page(pl_interp_instance_t *interp, void *closure);

      /* -------------- Read file cursor operations ---------- */
/* Open a read cursor w/specified file */
int pl_main_cursor_open(const gs_memory_t *, pl_top_cursor_t *, const char *, byte *, unsigned);

#ifdef DEBUG
/* Refill from input, avoid extra call level for efficiency */
int pl_main_cursor_next(pl_top_cursor_t *cursor);
#else
 #define pl_main_cursor_next(curs) (pl_top_cursor_next(curs))
#endif

/* Read back curr file position */
long pl_main_cursor_position(pl_top_cursor_t *cursor);

/* Close read cursor */
void pl_main_cursor_close(pl_top_cursor_t *cursor);


/* return index in gs device list -1 if not found */
static inline int
get_device_index(const gs_memory_t *mem, const char *value)
{
    const gx_device *const *dev_list;
    int num_devs = gs_lib_device_list(&dev_list, NULL);
    int di;

    for ( di = 0; di < num_devs; ++di )
	if ( !strcmp(gs_devicename(dev_list[di]), value) )
	    break;
    if ( di == num_devs ) {
	errprintf("Unknown device name %s.\n", value);
	return -1;
    }
    return di;
}

static int
close_job(pl_main_universe_t *universe, pl_main_instance_t *pti)
{	
    if ( pti->print_page_count )
        dlprintf1("%%%%PageCount: %d\n", pti->page_count);
    return pl_dnit_job(universe->curr_instance);
}

static void
pl_reclaim(pl_main_instance_t *pti)
    /* NB - gs_nogc_reclaim does a bit more than expected.  It has the
       side effect of resetting the string memory procedures in the
       memory "procs" table and other setup business prerequisite to
       the nogc.dev allocator functioning properly.  It also
       reclaims/consolidates memory. */
{
    vm_spaces *spaces = &pti->spaces;
    gs_nogc_reclaim(spaces, true);
}



/* ----------- Command-line driver for pl_interp's  ------ */
/* 
 * Here is the real main program.
 */
GSDLLEXPORT int GSDLLAPI 
pl_main(
    int                 argc,
    char *        argv[]
)
{
    gs_memory_t *           mem;
    gs_memory_t *           pjl_mem;
    pl_main_instance_t      inst;
    arg_list                args;
    char *                  filename = NULL;
    char                    err_buf[256];
    pl_interp_t *           pjl_interp;
    pl_interp_instance_t *  pjl_instance;
    pl_main_universe_t      universe;
    pl_interp_instance_t *  curr_instance = 0;
    gs_c_param_list         params;

    mem = pl_alloc_init();
    pl_platform_init(mem->gs_lib_ctx->fstdout);


    pjl_mem = mem;

    gs_lib_init1(pjl_mem);

    /* Create a memory allocator to allocate various states from */
    {
	/*
	 * gs_iodev_init has to be called here (late), rather than
	 * with the rest of the library init procedures, because of
	 * some hacks specific to MS Windows for patching the
	 * stdxxx IODevices.
	 */
	extern void gs_iodev_init(gs_memory_t *);
	gs_iodev_init(pjl_mem);
    }

    /* Init the top-level instance */
    gs_c_param_list_write(&params, pjl_mem);
    gs_param_list_set_persistent_keys((gs_param_list*)&params, false);
    pl_main_init_instance(&inst, mem);
    arg_init(&args, (const char **)argv, argc, pl_main_arg_fopen, NULL);

    
    /* Create PJL instance */
    if ( pl_allocate_interp(&pjl_interp, &pjl_implementation, pjl_mem) < 0
	 || pl_allocate_interp_instance(&pjl_instance, pjl_interp, pjl_mem) < 0 ) {
	errprintf("Unable to create PJL interpreter.");
	return -1;
    }

    /* Create PDL instances, etc */
    if (pl_main_universe_init(&universe, err_buf, mem, pdl_implementation,
			      pjl_instance, &inst, &pl_pre_finish_page, &pl_post_finish_page) < 0) {
	errprintf(err_buf);
	return -1;
    }

#ifdef DEBUG
    if (gs_debug_c(':'))
	pl_print_usage(&inst, "Start");
#endif


    /* ------ Begin Main LOOP ------- */
    for (;;) {
	/* Process one input file. */
	/* for debugging we test the parser with a small 256 byte
           buffer - for production systems use 8192 bytes */
#ifdef DEBUG
	byte                buf[1<<9];
#else
	byte                buf[1<<13];
#endif
	pl_top_cursor_t     r;
	int                 code = 0;
	bool                in_pjl = true;
	bool                new_job = false;


        if ( pl_init_job(pjl_instance) < 0 ) {
            errprintf("Unable to init PJL job.\n");
            return -1;
        }

	/* Process any new options. May request new device. */
	if (argc==1 || 
            pl_main_process_options(&inst, 
                                    &args,
                                    &params, 
                                    pjl_instance, pdl_implementation, &filename) < 0) {
            /* Print error verbage and return */
	    int i;
	    const gx_device **dev_list;
	    int num_devs = gs_lib_device_list((const gx_device * const **)&dev_list, NULL);

	    errprintf(pl_usage, argv[0]);

	    if (pl_characteristics(&pjl_implementation)->version)
		errprintf("Version: %s\n", pl_characteristics(&pjl_implementation)->version);
	    if (pl_characteristics(&pjl_implementation)->build_date)
		errprintf("Build date: %s\n", pl_characteristics(&pjl_implementation)->build_date);
	    errprintf("Devices:");
	    for ( i = 0; i < num_devs; ++i ) {
		if ( ( (i + 1) )  % 9 == 0 )
		    errprintf("\n");
		errprintf(" %s", gs_devicename(dev_list[i]));
	    }
	    errprintf("\n");

	    return -1;
	}

	if (!filename)
	    break;  /* no nore files to process */


	/* open file for reading - NB we should respect the minimum
           requirements specified by each implementation in the
           characteristics structure */
        if (pl_main_cursor_open(mem, &r, filename, buf, sizeof(buf)) < 0) {
            errprintf("Unable to open %s for reading.\n", filename);
            return -1;
        }
#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
	vd_trace0 = visual_tracer_init();
#endif

#ifdef DEBUG
        if (gs_debug_c(':'))
            dprintf1("%% Reading %s:\n", filename);
#endif
	/* pump data thru PJL/PDL until EOD or error */
	new_job = false;
        in_pjl = true;
        for (;;) {
            if_debug1('i', "[i][file pos=%ld]\n", pl_main_cursor_position(&r));
            /* end of data - if we are not back in pjl the job has
               ended in the middle of the data stream. */
            if (pl_main_cursor_next(&r) <= 0) {
                if_debug0('|', "End of of data\n");
                if ( !in_pjl ) {
                    if_debug0('|', "end of data stream found in middle of job\n");
		    pl_process_eof(curr_instance);
                    if ( close_job(&universe, &inst) < 0 ) {
			dprintf("Unable to deinit PDL job.\n");
			return -1;
                    }
                }
    	        break;            
	    }
            if ( in_pjl ) {
                if_debug0('|', "Processing pjl\n");
		code = pl_process(pjl_instance, &r.cursor);
		if (code == e_ExitLanguage) {
                    if_debug0('|', "Exiting pjl\n" );
		    in_pjl = false;
		    new_job = true;
		}
    	    } 
	    if ( new_job ) {
		if (mem->gs_lib_ctx->gs_next_id > 0xFF000000) {
		    dprintf("Once a year reset the gs_next_id.\n");
		    return -1;
		}

	        if_debug0('|', "Selecting PDL\n" );
		curr_instance = pl_main_universe_select(&universe, err_buf,
							pjl_instance,
							pl_select_implementation(pjl_instance, &inst, r),
							&inst, (gs_param_list *)&params);
		if ( curr_instance == NULL ) {
		    dprintf(err_buf);
		    return -1;
		}

		if ( pl_init_job(curr_instance) < 0 ) {
		    dprintf("Unable to init PDL job.\n");
		    return -1;
		}
		if_debug1('|', "selected and initializing (%s)\n",
			  pl_characteristics(curr_instance->interp->implementation)->language);
		new_job = false;
	    }
	    if ( curr_instance ) {

		/* Special case when the job resides in a seekable file and
		   the implementation has a function to process a file at a
		   time. */
		if (curr_instance->interp->implementation->proc_process_file &&
		    r.strm != mem->gs_lib_ctx->fstdin) {
		    if_debug1('|', "processing job from file (%s)\n", filename);
		    code = pl_process_file(curr_instance, filename);
		    if (code < 0) {
			dprintf1("Warning interpreter exited with error code %d\n", code);
			if (close_job(&universe, &inst) < 0) {
			    dprintf("Unable to deinit PJL.\n");
			    return -1;
			}
		    }
		    if_debug0('|', "exiting job and proceeding to next file\n");
		    break; /* break out of the loop to process the next file */
		}

	        code = pl_process(curr_instance, &r.cursor);
		if_debug1('|', "processing (%s) job\n", 
			  pl_characteristics(curr_instance->interp->implementation)->language);
		if (code == e_ExitLanguage) {
		    in_pjl = true;
		    if_debug1('|', "exiting (%s) job back to pjl\n",
			      pl_characteristics(curr_instance->interp->implementation)->language);
		    if ( close_job(&universe, &inst) < 0 ) {
		        dprintf( "Unable to deinit PDL job.\n");
			return -1;
		    }
		    if ( pl_init_job(pjl_instance) < 0 ) {
		        dprintf("Unable to init PJL job.\n");
			return -1;
		    }
                    pl_renew_cursor_status(&r);
		} else if ( code < 0 ) { /* error and not exit language */
		    dprintf1("Warning interpreter exited with error code %d\n", code );
		    dprintf("Flushing to end of job\n" );
		    /* flush eoj may require more data */
		    while ((pl_flush_to_eoj(curr_instance, &r.cursor)) == 0) {
		        if_debug1('|', "flushing to eoj for (%s) job\n",
				  pl_characteristics(curr_instance->interp->implementation)->language);
			if (pl_main_cursor_next(&r) <= 0) {
			    if_debug0('|', "end of data found while flushing\n");
			    break;
			}
		    }
		    pl_report_errors(curr_instance, code, 
				     pl_main_cursor_position(&r),
				     inst.error_report > 0);
		    if ( close_job(&universe, &inst) < 0 ) {
		        dprintf("Unable to deinit PJL.\n");
			return -1;
		    }
		    /* Print PDL status if applicable, then dnit PDL job */
		    code = 0;
		    new_job = true;
		    /* go back to pjl */
		    in_pjl = true;
		}
	    }
	}
        pl_main_cursor_close(&r);
    }

    /* ----- End Main loop ----- */

    /* Dnit PDLs */
    if (pl_main_universe_dnit(&universe, err_buf)) {
	dprintf(err_buf);
	return -1;
    }
    /* dnit pjl */
    if ( pl_deallocate_interp_instance(pjl_instance) < 0
	 || pl_deallocate_interp(pjl_interp) < 0 ) {
	dprintf("Unable to close out PJL instance.\n");
	return -1;
    }

    /* We lost the ability to print peak memory usage with the loss
     * of the memory wrappers.  
     */
    /* release param list */
    gs_c_param_list_release(&params);
    arg_finit(&args);

#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
    visual_tracer_close();
#endif
    if ( gs_debug_c('A') )
	dprintf("Final time" );
    pl_platform_dnit(0);
    pl_reclaim(&inst);
    if (inst.leak_check)
        debug_dump_allocator((gs_ref_memory_t *)mem);
    return 0;
}

/* --------- Functions operating on pl_main_universe_t ----- */
/* Init main_universe from pdl_implementation */
int   /* 0 ok, else -1 error */
pl_main_universe_init(
	pl_main_universe_t     *universe,            /* universe to init */
	char                   *err_str,             /* RETURNS error str if error */
	gs_memory_t            *mem,                 /* deallocator for devices */
	pl_interp_implementation_t const * const
	                       pdl_implementation[], /* implementations to choose from */
	pl_interp_instance_t   *pjl_instance,        /* pjl to
                                                        reference */
	pl_main_instance_t     *inst,                /* instance for pre/post print */
	pl_page_action_t       pl_pre_finish_page,   /* pre-page action */
	pl_page_action_t       pl_post_finish_page   /* post-page action */
)
{
	int index;

	/* 0-init everything */
	memset(universe, 0, sizeof(*universe));
	universe->pdl_implementation = pdl_implementation;
	universe->mem = mem;
	mem->gs_lib_ctx->top_of_system = universe;
	inst->device_memory = mem;

	/* Create & init PDL all instances. Could do this lazily to save memory, */
	/* but for now it's simpler to just create all instances up front. */
	for (index = 0; pdl_implementation[index] != 0; ++index) {
	  pl_interp_instance_t *instance;

	  if ( pl_allocate_interp(&universe->pdl_interp_array[index],
	    pdl_implementation[index], mem) < 0
	  || pl_allocate_interp_instance(&universe->pdl_instance_array[index],
	   universe->pdl_interp_array[index], mem) < 0 ) {
	      if (err_str)
	        sprintf(err_str, "Unable to create %s interpreter.\n",
	         pl_characteristics(pdl_implementation[index])->language);
	      goto pmui_err;
	  }

	  instance = universe->pdl_instance_array[index];
	  if ( pl_set_client_instance(instance, pjl_instance, PJL_CLIENT) < 0 ||
               pl_set_client_instance(instance, universe->pdl_instance_array[0], PCL_CLIENT) ||
               pl_set_pre_page_action(instance, pl_pre_finish_page, inst) < 0 ||
               pl_set_post_page_action(instance, pl_post_finish_page, inst) < 0 ||
               pl_get_device_memory(instance, &inst->device_memory) < 0 
	       ) {
              if (err_str)
                  sprintf(err_str, "Unable to init %s interpreter.\n",
                          pl_characteristics(pdl_implementation[index])->language);
              goto pmui_err;
	  }
	}
	return 0;

pmui_err:
	pl_main_universe_dnit(universe, 0);
	return -1;
}

pl_interp_instance_t *get_interpreter_from_memory( const gs_memory_t *mem )
{
    pl_main_universe_t *universe = (pl_main_universe_t *) mem->gs_lib_ctx->top_of_system;
    return universe->curr_instance;
}


/* Undo pl_main_universe_init */
int   /* 0 ok, else -1 error */
pl_main_universe_dnit(
	pl_main_universe_t     *universe,            /* universe to dnit */
	char                   *err_str              /* RETRUNS errmsg if error return */
)
{
    int index;

    /* Deselect last-selected device */
    if (universe->curr_instance
     && pl_remove_device(universe->curr_instance) < 0) {
      if (err_str)
        sprintf(err_str, "Unable to close out PDL instance.\n");
      return -1;
    }

    /* dnit interps */
    for (index = 0; 
	 universe->pdl_implementation[index] != 0; 
	 ++index, universe->curr_instance = universe->pdl_instance_array[index])
      if ( (universe->pdl_instance_array[index]
        && pl_deallocate_interp_instance(universe->pdl_instance_array[index]) < 0)
       || (universe->pdl_interp_array[index]
        && pl_deallocate_interp(universe->pdl_interp_array[index]) < 0 )) {
          if (err_str)
            sprintf(err_str, "Unable to close out %s instance.\n",
             pl_characteristics(universe->pdl_implementation[index])->language);
          return -1;
      }

    /* dealloc device if sel'd */
    if (universe->curr_device) {
	gs_unregister_root(universe->curr_device->memory, &device_root, "pl_main_universe_select");
	/* ps allocator retain's the device, pl_alloc doesn't */
	gx_device_retain(universe->curr_device, false);
	universe->curr_device = NULL;
    }

    return 0;
}

/* Select new device and/or implementation, deselect one one (opt) */
pl_interp_instance_t *    /* rets current interp_instance, 0 if err */
pl_main_universe_select(
        pl_main_universe_t               *universe,              /* universe to select from */
	char                             *err_str,               /* RETURNS error str if error */
	pl_interp_instance_t             *pjl_instance,          /* pjl */
	pl_interp_implementation_t const *desired_implementation,/* impl to select */
	pl_main_instance_t               *pti,                   /* inst contains device */
	gs_param_list                    *params                 /* device params to set */
)
{	
    int params_are_set = 0;
    
    /* requesting the device in the main instance */
    gx_device *desired_device = pti->device;
    /* If new interpreter/device is different, deselect it from old interp */
    if ((universe->curr_implementation
	 && universe->curr_implementation != desired_implementation)
	|| (universe->curr_device && universe->curr_device != desired_device)) {
	if (universe->curr_instance
	    && pl_remove_device(universe->curr_instance) < 0) {
	    if (err_str)
		strcpy(err_str, "Unable to deselect device from interp instance.\n");
	    return 0;
	}
	if (universe->curr_device && universe->curr_device != desired_device) {
	    /* Here, we close the device. Note that this is not an absolute */
	    /* requirement: we could have a pool of open devices & select them */
	    /* into interp_instances as needed. The reason we force a close */
	    /* here is that multiple *async* devices would need coordination */
	    /* since an async device is not guaranteed to have completed */
	    /* rendering until it is closed. So, we close devices here to */
	    /* avoid things like intevermingling of output streams. */
	    if (gs_closedevice(universe->curr_device) < 0) {
		if (err_str)
		    strcpy(err_str, "Unable to close device.\n");
		return 0;
	    } else {
		/* Delete the device. */
		gs_unregister_root(universe->curr_device->memory, &device_root, "pl_main_universe_select");
		gs_free_object(universe->curr_device->memory,
			       universe->curr_device, "pl_main_universe_select(gx_device)");
		universe->curr_device = 0;
	    }
	}
    }

    /* Switch to/select new interperter if indicated. */
    /* Here, we assume that instances of all interpreters are open & ready */
    /* to go. If memory were scarce, we could dynamically destroy/create */
    /* interp_instances here (or even de/init the entire interp for greater */
    /* memory savings). */
    if ((!universe->curr_implementation
	 || universe->curr_implementation != desired_implementation)
	|| !universe->curr_device) {
	int index;

	/* Select/change PDL if needed */
	if (!universe->curr_implementation
	    || universe->curr_implementation != desired_implementation) {
	    /* find instance corresponding to implementation */
	    for (index = 0;
		 desired_implementation != universe->pdl_implementation[index];
		 ++index)
		;
	    universe->curr_instance = universe->pdl_instance_array[index];
	    universe->curr_implementation = desired_implementation;
	}

	/* Open a new device if needed. */
	if (!universe->curr_device)  { /* remember that curr_device==0 if we closed it above */
	    /* Set latest params into device BEFORE setting into device. */
	    /* Do this here because PCL5 will do some 1-time initializations based */
	    /* on device geometry when pl_set_device, below, selects the device. */
	    if ( gs_putdeviceparams(desired_device, params) < 0 ) {
		strcpy(err_str, "Unable to set params into device.\n");
		return 0;
	    }
	    params_are_set = 1;

	    if (gs_opendevice(desired_device) < 0) {
		if (err_str)
		    strcpy(err_str, "Unable to open new device.\n");
		return 0;
	    } else
		universe->curr_device = desired_device;
	}

	/* NB fix me, these parameters should not be passed this way */
	universe->curr_instance->pcl_personality = pti->pcl_personality;
	universe->curr_instance->interpolate = pti->interpolate;

	/* Select curr/new device into PDL instance */
	if ( pl_set_device(universe->curr_instance, universe->curr_device) < 0 ) {
	    if (err_str)
		strcpy(err_str, "Unable to install device into PDL interp.");
	    return 0;
	}
        /* potentially downgrade the resolution */
        if ( ( pti->page_count + 1 ) < pti->first_page ) {
            if ( !pti->saved_hwres ) {
                gx_device *pdev = universe->curr_device;
                pti->saved_hwres = true;
                pti->hwres[0] = pdev->HWResolution[0];
                pti->hwres[1] = pdev->HWResolution[1];

		if (!pjl_proc_compare(pjl_instance, 
				      pjl_proc_get_envvar(pjl_instance, "viewer"), 
				      "on")) {
		    /* NB: new_logical_page shouldn't be called for every page 
		     */ 
		    pti->viewer = true; /* cache pjl variable on language select */
		    gx_device_set_resolution(pdev, 10, 10);
		}
		else {
  		    pti->viewer = false;
		    gx_device_set_resolution(pti->device, 
					     pti->hwres[0], pti->hwres[1]);
		}
            }
        }
    }

    /* Set latest params into device. Write them all in case any changed */
    if ( !params_are_set
	 && gs_putdeviceparams(universe->curr_device, params) < 0 ) {
	strcpy(err_str, "Unable to set params into device.\n");
	return 0;
    }
    return universe->curr_instance;
}

/* ------- Functions related to pl_main_instance_t ------ */

/* Initialize the instance parameters. */
void
pl_main_init_instance(pl_main_instance_t *pti, gs_memory_t *mem)
{	
    pti->memory = mem;
    { 
        int i;
        for ( i = 0; i < countof(pti->spaces.memories.indexed); ++i )
	    pti->spaces.memories.indexed[i] = 0;
        pti->spaces.memories.named.local =
            pti->spaces.memories.named.global =
	    (gs_ref_memory_t *)mem;
    }

    pl_reclaim(pti);
    pti->error_report = -1;
    pti->pause = true;
    pti->print_page_count = false;
    pti->device = 0;
    pti->implementation = 0;
    gp_get_usertime(pti->base_time);
    pti->first_page = 1;
    pti->last_page = max_int;
    pti->page_count = 0;
    pti->saved_hwres = false;
    pti->interpolate = false;
    pti->leak_check = false;
    strncpy(&pti->pcl_personality[0], "PCL", sizeof(pti->pcl_personality)-1);
}

/* -------- Command-line processing ------ */

/* Create a default device if not already defined. */
static int
pl_top_create_device(pl_main_instance_t *pti, int index, bool is_default)
{	
    int code = 0;
    if ( index < 0 )
	return -1;
    if ( !is_default || !pti->device ) { 
	const gx_device **list;

	/* We assume that nobody else changes pti->device,
	   and this function is called from this module only.
	   Due to that device_root is always consistent with pti->device,
	   and it is regisrtered if and only if pti->device != NULL. 
	*/
	if (pti->device != NULL) {
	    pti->device = NULL;
	    gs_unregister_root(pti->device_memory, &device_root, "pl_main_universe_select");
	}
	gs_lib_device_list((const gx_device * const **)&list, NULL);
	code = gs_copydevice(&pti->device, list[index],
                             pti->device_memory);
	if (pti->device != NULL)
	  gs_register_struct_root(pti->device_memory, &device_root,
				    &pti->device, "pl_top_create_device");

    }
    return code;
}


/* Process the options on the command line. */
static FILE *
pl_main_arg_fopen(const char *fname, void *ignore_data)
{	return fopen(fname, "r");
}

static void
set_debug_flags(const char *arg, char *flags)
{
    byte value = (*arg == '-' ? (++arg, 0) : 0xff);

    while (*arg)
	flags[*arg++ & 127] = value;
}

#define arg_heap_copy(str) arg_copy(str, pmi->memory)
int
pl_main_process_options(pl_main_instance_t *pmi, arg_list *pal,
                        gs_c_param_list *params,
                        pl_interp_instance_t *pjl_instance,
                        pl_interp_implementation_t const * const impl_array[], char **filename)
{
    int code = 0;
    bool help = false;
    char *arg;

    gs_c_param_list_write_more(params);
    while ( (arg = (char *)arg_next(pal, &code)) != 0 && 
	    *arg == '-' ) { /* just - read from stdin */
        if (code < 0)
            break;
	if ( arg[1] == '\0' )
	    break;
	arg += 2;
	switch ( arg[-1] ) {
	default:
	    dprintf1("Unrecognized switch: %s\n", arg);
	    return -1;
	case '\0':
	    /* read from stdin - must be last arg */
	    continue;
        case 'c':
        case 'C':
            pmi->print_page_count = true;
            break;
	case 'd':
	case 'D':
	    if ( !strcmp(arg, "BATCH") )
		continue;
	    if ( !strcmp(arg, "NOPAUSE") ) { 
		pmi->pause = false;
		continue;
	    }
            if ( !strcmp(arg, "DOINTERPOLATE") ) {
                pmi->interpolate = true;
                continue;
            }

	    { 
		/* We're setting a device parameter to a non-string value. */
		char *eqp = strchr(arg, '=');
		const char *value;
		int vi;
		float vf;
		bool bval = true;
		char buffer[128];

		if ( eqp || (eqp = strchr(arg, '#')) )
		    value = eqp + 1;
		else {                   
		    /* -dDefaultBooleanIs_TRUE */
		    code = param_write_bool((gs_param_list *)params, arg_heap_copy(arg), &bval);
		    continue;
		}
                /* search for an int (no decimal), if fail try a float */
                if ( ( !strchr(value, '.' ) ) &&
                     ( sscanf(value, "%d", &vi) == 1 ) ) {
                    if ( !strncmp(arg, "FirstPage", 9) )
                        pmi->first_page = max(vi, 1);
                    else if ( !strncmp(arg, "LastPage", 8) )
                        pmi->last_page = vi;
                    else {
                        /* create a null terminated string */
                        strncpy(buffer, arg, eqp - arg);
                        buffer[eqp - arg] = '\0';
                        code = param_write_int((gs_param_list *)params, arg_heap_copy(buffer), &vi);
                    }
                } else if ( sscanf(value, "%f", &vf) == 1 ) {
                    /* create a null terminated string.  NB duplicated code. */
                    strncpy(buffer, arg, eqp - arg);
                    buffer[eqp - arg] = '\0';
                    code = param_write_float((gs_param_list *)params, arg_heap_copy(buffer), &vf);
                } else if ( !strcmp(value, "true") ) {
		    /* bval = true; */
		    strncpy(buffer, arg, eqp - arg);
		    buffer[eqp - arg] = '\0';
		    code = param_write_bool((gs_param_list *)params, arg_heap_copy(buffer), &bval);
                } else if ( !strcmp(value, "false") ) {
                    bval = false;
                    strncpy(buffer, arg, eqp - arg);
                    buffer[eqp - arg] = '\0';
                    code = param_write_bool((gs_param_list *)params, arg_heap_copy(buffer), &bval);
                } else {
                    dprintf("Usage for -d is -d<option>=[<integer>|<float>|true|false]\n");
		    continue;
		}
	    }
	    break;
	case 'E':
	    if ( *arg == 0 )
		gs_debug['#'] = 1;
	    else
		sscanf(arg, "%d", &pmi->error_report);
	    break;
	case 'g':
	    {
		int geom[2];
		gs_param_int_array ia;

		if ( sscanf(arg, "%ux%u", &geom[0], &geom[1]) != 2 ) { 
		    dprintf("-g must be followed by <width>x<height>\n");
		    return -1;
		}
		ia.data = geom;
		ia.size = 2;
		ia.persistent = false;
		code = param_write_int_array((gs_param_list *)params, "HWSize", &ia);
	    }
	    break;
	case 'h':
	    help = true;
	    goto out;
            /* job control line follows - PJL */
        case 'j':
        case 'J':
            /* set up the read cursor and send it to the pjl parser */
            {
                stream_cursor_read cursor;

                /* PJL lines have max length of 80 character + null terminator */
                byte buf[81];
                /* length of arg + newline (expected by PJL parser) + null */
                int buf_len = strlen(arg) + 2;
                if ( (buf_len ) > sizeof(buf) ) {
                    dprintf("pjl sequence too long\n");
                    return -1;
                }
                /* copy and concatenate newline */
                strcpy(buf, arg); strcat(buf, "\n");
                /* starting pos for pointer is always one position back */
                cursor.ptr = buf - 1;
                /* set the end of data pointer */
                cursor.limit = cursor.ptr + strlen(buf);
                /* process the pjl */
                code = pl_process(pjl_instance, &cursor);
                if ( code < 0 ) {
                    dprintf("illegal pjl sequence in -J option\n");
                    return code;
                }
            }
            break;
	case 'K':		/* max memory in K */
	    {
#ifdef OLD_ALLOCATOR
		int maxk;
		gs_malloc_memory_t *rawheap = gs_malloc_wrapped_contents(pmi->memory);

		if ( sscanf(arg, "%d", &maxk) != 1 ) { 
		    dprintf("-K must be followed by a number\n");
		    return -1;
		}
		rawheap->limit = (long)maxk << 10;
#endif
	    }
	    break;
        case 'l':
            pmi->leak_check=true;
            break;
	case 'p':
	case 'P':
	    {
		if ( !strcmp(arg, "RTL") || !strcmp(arg, "PCL5E") ||
		     !strcmp(arg, "PCL5C") )
		    strcpy(pmi->pcl_personality, arg);
		else 
		    dprintf("PCL personality must be RTL, PCL5E or PCL5C\n");
	    }
	    break;
	case 'r':
	    { 
		float res[2];
		gs_param_float_array fa;

		switch ( sscanf(arg, "%fx%f", &res[0], &res[1]) ) {
		default:
		    dprintf("-r must be followed by <res> or <xres>x<yres>\n");
		    return -1;
		case 1:	/* -r<res> */
		    res[1] = res[0];
		case 2:	/* -r<xres>x<yres> */
		    ;
		}
		fa.data = res;
		fa.size = 2;
		fa.persistent = false;
		code = param_write_float_array((gs_param_list *)params, "HWResolution", &fa);
	    }
	    break;
	case 's':
	case 'S':
	    { /* We're setting a device parameter to a string. */
		char *eqp;
		const char *value;
		gs_param_string str;
		eqp = strchr(arg, '=');
		if ( !(eqp || (eqp = strchr(arg, '#'))) ) { 
		    dprintf("Usage for -s is -s<option>=<string>\n");
	            return -1;
		}
		value = eqp + 1;
		if ( !strncmp(arg, "DEVICE", 6) ) { 
		    int code = 
			pl_top_create_device(pmi, 
					     get_device_index(pmi->memory, value), 
					     false);
		    if ( code < 0 ) return code;
		}
		else { 
		    char buffer[128];
		    strncpy(buffer, arg, eqp - arg);
		    buffer[eqp - arg] = '\0';
		    param_string_from_transient_string(str, value);
		    code = param_write_string((gs_param_list *)params, buffer,
					      &str);
		}
	    }
	    break;
#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
	case 'T':
            set_debug_flags(arg, vd_flags);
	    break;
#endif
	case 'Z':
            set_debug_flags(arg, gs_debug);
	    break;
	case 'L': /* language */
	    {
		int index;
		for (index = 0; impl_array[index] != 0; ++index)
	            if (!strcmp(arg,
				pl_characteristics(impl_array[index])->language))
			break;
		if (impl_array[index] != 0)
	            pmi->implementation = impl_array[index];
		else {
	            dprintf("Choose language in -L<language> from: ");
	            for (index = 0; impl_array[index] != 0; ++index)
			dprintf1("%s ",
				pl_characteristics(impl_array[index])->language);
	            dprintf("\n");
	            return -1;
		}
		break;
	    }
	}
    }
 out:	if ( help ) { 
        arg_finit(pal);
        gs_c_param_list_release(params);
        return -1;
    }
    gs_c_param_list_read(params);
    pl_top_create_device(pmi, 0, true); /* create default device if needed */

    /* The last argument wasn't a switch filename else NULL*/
    *filename = arg; 
    return 0;
}

/* either the (1) implementation has been selected on the command line or
   (2) it has been selected in PJL or (3) we need to auto sense. */
static pl_interp_implementation_t const *
pl_select_implementation(pl_interp_instance_t *pjl_instance, pl_main_instance_t *pmi, pl_top_cursor_t r)
{
    /* Determine language of file to interpret. We're making the incorrect */
    /* assumption that any file only contains jobs in one PDL. The correct */
    /* way to implement this would be to have a language auto-detector. */
    pl_interp_implementation_t const *impl;
    if (pmi->implementation)
	return pmi->implementation;  /* was specified as cmd opt */
    /* select implementation */
    if ( (impl = pl_pjl_select(pjl_instance, pdl_implementation)) != 0 )
	return impl;
    /* lookup string in name field for each implementation */
    return pl_auto_sense(r.cursor.ptr + 1, (r.cursor.limit - r.cursor.ptr), pdl_implementation);
}

/* Find default language implementation */
static pl_interp_implementation_t const *
pl_pjl_select(pl_interp_instance_t *pjl_instance,
	      pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
)
{
    pjl_envvar_t *language;
    pl_interp_implementation_t const * const * impl;
    language = pjl_proc_get_envvar(pjl_instance, "language");
    for (impl = impl_array; *impl != 0; ++impl) {
	if ( !strcmp(pl_characteristics(*impl)->language, language) )
	    return *impl;
    }
    /* Defaults to NULL */
    return 0;
}

/* Find default language implementation */
static pl_interp_implementation_t const *
pl_auto_sense(
   const char*                      name,         /* stream  */
   int                              buffer_length, /* length of stream */
   pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
)
{
    /* Lookup this string in the auto sense field for each implementation */
    pl_interp_implementation_t const * const * impl;
    for (impl = impl_array; *impl != 0; ++impl) {
	if ( buffer_length >= (strlen(pl_characteristics(*impl)->auto_sense_string)) )
	    if ( !strncmp(pl_characteristics(*impl)->auto_sense_string,
			  name,
			  (strlen(pl_characteristics(*impl)->auto_sense_string))) )
		return *impl;
    }
    /* Defaults to PCL */
    return impl_array[0];
}

/* Print memory and time usage. */
void
pl_print_usage(const pl_main_instance_t *pti,
  const char *msg)
{	
	long utime[2];
	gp_get_usertime(utime);
	dprintf3("%% %s time = %g, pages = %d\n",
		 msg, utime[0] - pti->base_time[0] +
		 (utime[1] - pti->base_time[1]) / 1000000000.0,
                 pti->page_count);
}

/* Log a string to console, optionally wait for input */
void
pl_log_string(const gs_memory_t *mem, const char *str, int wait_for_key)
{
    errwrite(str, strlen(str)); 
    if (wait_for_key)
	fgetc(mem->gs_lib_ctx->fstdin);
}

/* Pre-page portion of page finishing routine */
int	/* ret 0 if page should be printed, 1 if no print, else -ve error */
pl_pre_finish_page(pl_interp_instance_t *interp, void *closure)
{
    pl_main_instance_t *pti = (pl_main_instance_t *)closure;

    /* up the page count */
    ++(pti->page_count);

    /* if the next page is in range we want to restore the resolution */
    if ( (pti->page_count + 1) >= pti->first_page &&
         (pti->page_count + 1) <= pti->last_page ) {
        /* check if we downgraded the resolution */
        if ( pti->saved_hwres ) {
            pti->saved_hwres = false;
            gx_device_set_resolution(pti->device, 
                                     pti->hwres[0], pti->hwres[1]);
        }
    }
    /* nothing to do now if we are in range */
    if ( pti->page_count >= pti->first_page && pti->page_count <= pti->last_page )
        return 0;
    /* past page count we return an error so the interpreter will exit early */
    if ( pti->page_count > pti->last_page )
        return -1;
    /* finally if the next page is out of range -- must be before
       the first page if we are here.  We have to render the page
       but can optimize by downgrading the resolution. */
    if ( (pti->page_count + 1) < pti->first_page ) {
        /* If we haven't saved the hardware resolution save the
           default resolution for the device and set the
           diminished resolution if it hasn't been done
           already. NB what if language sets resolution? */
        if ( !pti->saved_hwres ) {
            gx_device *pdev = pti->device;
            pti->saved_hwres = true;
            pti->hwres[0] = pdev->HWResolution[0];
            pti->hwres[1] = pdev->HWResolution[1];
	    if ( pti->viewer ) {
	        /* NB: new_logical_page shouldn't be called for every page 
		 * viewer optimizations sometimes fail!
		 */ 
	        gx_device_set_resolution(pdev, 10, 10);
	    }
	    else
	        gx_device_set_resolution(pti->device, 
					 pti->hwres[0], pti->hwres[1]);
        }
    }
    /* out of range don't allow printing the page */
    return 1;
}

/* Post-page portion of page finishing routine */
int	/* ret 0, else -ve error */
pl_post_finish_page(pl_interp_instance_t *interp, void *closure)
{
	pl_main_instance_t *pti = (pl_main_instance_t *)closure;
	if ( pti->pause )
	  { char strbuf[256];
	    sprintf(strbuf, "End of page %d, press <enter> to continue.\n",
		  pti->page_count);
	    pl_log_string(pti->memory, strbuf, 1);
	  }
	else if ( gs_debug_c(':') )
	  pl_print_usage(pti, " done :");
        pl_reclaim(pti);
	return 0;
}

/* ---------------- Stubs ---------------- */
/* Error termination, called back from plplatf.c */
/* Only called back if abnormal termination */
void
pl_exit(int exit_status)
{
    gp_do_exit(exit_status);
}

/* -------------- Read file cursor operations ---------- */
/* Open a read cursor w/specified file */
int	/* returns 0 ok, else -ve error code */
pl_main_cursor_open(const gs_memory_t *mem,
		    pl_top_cursor_t   *cursor,        /* cursor to init/open */
		    const char        *fname,         /* name of file to open */
		    byte              *buffer,        /* buffer to use for reading */
		    unsigned          buffer_length   /* length of *buffer */
)
{
	/* try to open file */
        if (fname[0] == '-' && fname[1] == 0)
	    cursor->strm = mem->gs_lib_ctx->fstdin;
	else
	    cursor->strm = fopen(fname, "rb");
	if (!cursor->strm)
	  return gs_error_ioerror;

	return pl_top_cursor_init(cursor, cursor->strm, buffer, buffer_length);
}

#ifdef DEBUG
/* Refill from input */
int    /* rets 1 ok, else 0 EOF, -ve error */
pl_main_cursor_next(
	pl_top_cursor_t *cursor       /* cursor to operate on */
)
{
	return pl_top_cursor_next(cursor);
}
#endif /* DEBUG */

/* Read back curr file position */
long    /* offset from beginning of file */
pl_main_cursor_position(
	pl_top_cursor_t *cursor     /* cursor to operate on */
)
{
	return (long)ftell(cursor->strm)
	 - (cursor->cursor.limit - cursor->cursor.ptr);
}

/* Close read cursor */
void
pl_main_cursor_close(
	pl_top_cursor_t *cursor       /* cursor to operate on */
)
{
	pl_top_cursor_dnit(cursor);
	fclose(cursor->strm);
}

#ifndef NO_MAIN
/* ----------- Command-line driver for pl_interp's  ------ */
int
main(int argc, char **argv) {
    return pl_main(argc, argv);
}
#endif /* !defined(NO_MAIN) */

