/* plmain.c */
/* Main program command-line interpreter for PCL interpreters */
#include "stdio_.h"
/* get stdio values before they get redefined */
 private void
pl_get_real_stdio(FILE **in, FILE **out, FILE **err)
{
    *in = stdin;
    *out = stdout;
    *err = stderr;
}
#include "string_.h"
#include "gdebug.h"
#include "gscdefs.h"
#include "gsio.h"
#include "gstypes.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsstruct.h"
#include "gxalloc.h"
#include "gsalloc.h"
#include "gsargs.h"
#include "gp.h"
#include "gsdevice.h"
#include "gsparam.h"
#include "pjtop.h"
#include "plparse.h"
#include "plplatf.h"
#include "plmain.h"
#include "pltop.h"
#include "pltoputl.h"

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
private const char *pl_usage = "\
Usage: %s [option* file]+...\n\
Options: -dNOPAUSE -E[#] -h -L<PCL|PCLXL> -K<maxK> -P<PCL5C|PCL5E|RTL> -Z...\n\
         -sDEVICE=<dev> -g<W>x<H> -r<X>[x<Y>] -d{First|Last}Page=<#>\n\
	 -sOutputFile=<file> (-s<option>=<string> | -d<option>[=<value>])*\n\
";

/* ---------------- Forward decls ------------------ */
/* Functions to encapsulate pl_main_universe_t */
int   /* 0 ok, else -1 error */
pl_main_universe_init(P8(
	pl_main_universe_t     *universe,            /* universe to init */
	char                   *err_str,             /* RETURNS error str if error */
	gs_memory_t            *mem,                 /* deallocator for devices */
	pl_interp_implementation_t const * const
	                       pdl_implementation[], /* implementations to choose from */
	pl_interp_instance_t   *pjl_instance,        /* pjl to reference */
	pl_main_instance_t     *inst,                /* instance for pre/post print */
	pl_page_action_t       pl_pre_finish_page,   /* pre-page action */
	pl_page_action_t       pl_post_finish_page   /* post-page action */
));
int   /* 0 ok, else -1 error */
pl_main_universe_dnit(P2(
	pl_main_universe_t     *universe,            /* universe to dnit */
	char                   *err_str              /* RETRUNS errmsg if error return */
));
pl_interp_instance_t *    /* rets current interp_instance, 0 if err */
pl_main_universe_select(P6(
	pl_main_universe_t               *universe,              /* universe to select from */
	char                             *err_str,               /* RETURNS error str if error */
	pl_interp_implementation_t const *desired_implementation,/* impl to select */
	gx_device                        *desired_device,        /* device to select */
	gs_param_list                    *params,                 /* device params to use */
	char *                           pcl_personality         /* an additional parameter
                                                                    for selecting pcl personality */
));

private pl_interp_implementation_t const *
pl_auto_sense(P3(
   const char*                      name,         /* stream  */
   int                              buffer_length, /* length of stream */
   pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
));

private pl_interp_implementation_t const *
pl_select_implementation(P3(
  pl_interp_instance_t *pjl_instance,
  pl_main_instance_t *pmi, 
  pl_top_cursor_t r
));


/* Process the options on the command line. */
private FILE *pl_main_arg_fopen(P2(const char *fname, void *ignore_data));

/* Initialize the instance parameters. */
void pl_main_init_instance(P2(pl_main_instance_t *pmi, gs_memory_t *memory));
void pl_main_reinit_instance(P1(pl_main_instance_t *pmi));

#ifdef DEBUG
/* Print memory and time usage. */
void pl_print_usage(P3(gs_memory_t *mem, const pl_main_instance_t *pmi,
		       const char *msg));
#endif

/* Process the options on the command line, including making the
   initial device and setting its parameters.  */
int pl_main_process_options(P4(pl_main_instance_t *pmi, arg_list *pal,
			       gs_c_param_list *params,
                               pl_interp_implementation_t const * const impl_array[]));

/* Find default language implementation */
pl_interp_implementation_t const *
pl_auto_sense(P3(const char* buf, int buf_len, pl_interp_implementation_t const * const impl_array[]));

private pl_interp_implementation_t const *
pl_pjl_select(P2(pl_interp_instance_t *pjl_instance,
	      pl_interp_implementation_t const * const impl_array[]));

/* Pre-page portion of page finishing routine */
int	/* ret 0 if page should be printed, 1 if no print, else -ve error */
pl_pre_finish_page(P2(pl_interp_instance_t *interp, void *closure));

/* Post-page portion of page finishing routine */
int	/* ret 0, else -ve error */
pl_post_finish_page(P2(pl_interp_instance_t *interp, void *closure));

      /* -------------- Read file cursor operations ---------- */
/* Open a read cursor w/specified file */
int pl_main_cursor_open(P4(pl_top_cursor_t *, const char *, byte *, unsigned));

#ifdef DEBUG
/* Refill from input, avoid extra call level for efficiency */
int pl_main_cursor_next(P1(pl_top_cursor_t *cursor));
#else
 #define pl_main_cursor_next(curs) (pl_top_cursor_next(curs))
#endif

/* Read back curr file position */
long pl_main_cursor_position(P1(pl_top_cursor_t *cursor));

/* Close read cursor */
void pl_main_cursor_close(P1(pl_top_cursor_t *cursor));


/* ----------- Command-line driver for pl_interp's  ------ */
/* 
 * Here is the real main program.
 */
  int
main(
    int                 argc,
    char **             argv
)
{
    gs_ref_memory_t *       imem;
#define mem ((gs_memory_t *)imem)
    pl_main_instance_t       inst;
    arg_list                args;
    const char *            arg;
    char                    err_buf[256];
    pl_main_universe_t      universe;
    pl_interp_t *           pjl_interp;
    pl_interp_instance_t *  pjl_instance;
    pl_interp_instance_t *  curr_instance = 0;
    gs_c_param_list         params;

    /* Init std io: set up in, our err - not much we can do here if this fails */
    pl_get_real_stdio(&gs_stdin, &gs_stdout, &gs_stderr);

    /* Initialize the platform. */
    pl_platform_init(gs_stderr);
#ifdef DEBUG
    gs_debug_out = gs_stdout;
#endif
    /* Create a memory allocator to allocate various states from */
    imem = ialloc_alloc_state((gs_raw_memory_t *)&gs_memory_default, 20000);
    imem->space = 0;		/****** WRONG ******/

    {
	/*
	 * gs_iodev_init has to be called here (late), rather than
	 * with the rest of the library init procedures, because of
	 * some hacks specific to MS Windows for patching the
	 * stdxxx IODevices.
	 */
	extern void gs_iodev_init(P1(gs_memory_t *));
	gs_iodev_init(mem);
    }

    /* Init the top-level instance */
    gs_c_param_list_write(&params, mem);
    pl_main_init_instance(&inst, mem);
    arg_init(&args, (const char **)argv, argc, pl_main_arg_fopen, NULL);

    /* Create PJL instance */
    if ( pl_allocate_interp(&pjl_interp, &pjl_implementation, mem) < 0
	 || pl_allocate_interp_instance(&pjl_instance, pjl_interp, mem) < 0 ) {
	fprintf(gs_stderr, "Unable to create PJL interpreter");
	exit(1);
    }

    /* Create PDL instances, etc */
    if (pl_main_universe_init(&universe, err_buf, mem, pdl_implementation,
			      pjl_instance, &inst, &pl_pre_finish_page, &pl_post_finish_page) < 0) {
	fputs(err_buf, gs_stderr);
	exit(1);
    }

#ifdef DEBUG
    if (gs_debug_c(':'))
	pl_print_usage(mem, &inst, "Start");
#endif


    /* ------ Begin Main LOOP ------- */
    for (;;) {
	/* Process one input file. */
	/* for debugging we test the parser with a small 256 byte
           buffer - for prodduction systems use 8192 bytes */
#ifdef DEBUG
	byte                buf[1<<9];
#else
	byte                bug[1<<13];
#endif
	pl_top_cursor_t     r;
	int                 code = 0;
	bool                in_pjl = true;
	bool                new_job = false;

	/* Process any new options. May request new device. */
	if (argc==1 || pl_main_process_options(&inst, &args, &params, pdl_implementation) < 0) {
	    int i;
	    const gx_device **dev_list;
	    int num_devs = gs_lib_device_list((const gx_device * const **)&dev_list, NULL);

	    fprintf(gs_stderr, pl_usage, argv[0]);

	    if (pl_characteristics(&pjl_implementation)->version)
		fprintf(gs_stderr, "Version: %s\n", pl_characteristics(&pjl_implementation)->version);
	    if (pl_characteristics(&pjl_implementation)->build_date)
		fprintf(gs_stderr, "Build date: %s\n", pl_characteristics(&pjl_implementation)->build_date);
	    fputs("Devices:", gs_stderr);
	    for ( i = 0; i < num_devs; ++i ) {
		if ( ( (i + 1) )  % 9 == 0 )
		    fputs("\n", gs_stderr);
		fprintf(gs_stderr, " %s", gs_devicename(dev_list[i]));
	    }
	    fputs("\n", gs_stderr);

	    exit(1);
	}
	if ( gs_debug_c('A') )
	    dprintf( "memory allocated\n" );
	/* Process the next file. process_options leaves next non-option on arg list*/
	if ((arg = arg_next(&args)) == 0)
	    break;  /* no nore files to process */

	/* open file for reading - NB we should respec the minimum
           requirements specified by each implementation in the
           characteristics structure */
        if (pl_main_cursor_open(&r, arg, buf, sizeof(buf)) < 0) {
            fprintf(gs_stderr, "Unable to open %s for reading.\n", arg);
            exit(1);
        }

#ifdef DEBUG
        if (gs_debug_c(':'))
            dprintf1("%% Reading %s:\n", arg);
#endif


        if ( pl_init_job(pjl_instance) < 0 ) {
            fprintf(gs_stderr, "Unable to init PJL job.\n");
            exit(1);
        }

	/* pump data thru PJL/PDL until EOD or error */
	new_job = false;
        in_pjl = true;
        for (;;) {
            if_debug1('i', "[i][file pos=%ld]\n", pl_main_cursor_position(&r));
            if (pl_main_cursor_next(&r) <= 0)
    	        break;
            if ( in_pjl ) {
		code = pl_process(pjl_instance, &r.cursor);
		if (code == e_ExitLanguage) {
		    in_pjl = false;
		    new_job = true;
		}
    	    } else {
		if ( new_job ) {
		    if ( (curr_instance = pl_main_universe_select(&universe, err_buf,
								  pl_select_implementation(pjl_instance, &inst, r),
								  inst.device, (gs_param_list *)&params, inst.pcl_personality) ) == 0) {
			fputs(err_buf, gs_stderr);
			exit(1);
		    }

		    if ( pl_init_job(curr_instance) < 0 ) {
			fprintf(gs_stderr, "Unable to init PDL job.\n");
			exit(1);
		    }
		    new_job = false;
		}
		code = pl_process(curr_instance, &r.cursor);
    	        if (code == e_ExitLanguage) {
    	            in_pjl = true;
                    /* Really should switch back to language auto-sense,*/
		    /* but don't have one...*/
		    /* Select desired language & device */
                    if ( pl_dnit_job(curr_instance) < 0 ) {
			fprintf(gs_stderr, "Unable to deinit PDL job.\n");
			exit(1);
		    }
		    if ( pl_init_job(pjl_instance) < 0 ) {
			fprintf(gs_stderr, "Unable to init PJL job.\n");
			exit(1);
                    }
		} else if ( code < 0 ) { /* error and not exit language */
		    dprintf1( "Warning interpreter exited with error code %d\n", code );
		    dprintf( "Flushing to end of job\n" );
		    /* flush eoj may require more data */
		    while ((pl_flush_to_eoj(curr_instance, &r.cursor)) == 0)
			if (pl_main_cursor_next(&r) <= 0)
			    goto next;
		    pl_report_errors(curr_instance, code, pl_main_cursor_position(&r),
				     inst.error_report > 0, gs_stdout);
		    /* Print PDL status if applicable, then dnit PDL job */
		    code = 0;
		    new_job = true;
		    /* go back to pjl */
		    in_pjl = true;
		}

	    }
	}
next:	if (code < 0)
	    /* Error: Print PDL status if applicable, dnit PDL job, & skip to eoj */
	    pl_report_errors(curr_instance, code, pl_main_cursor_position(&r),
			     inst.error_report > 0, gs_stdout);
        /* Print PDL status if applicable, then dnit PDL job */
        if (!in_pjl) {
	    pl_process_eof(curr_instance);
	    pl_report_errors(curr_instance, code, pl_main_cursor_position(&r),
			     inst.error_report > 0, gs_stdout);
	    if ( pl_dnit_job(curr_instance) < 0 ) {
		fprintf(gs_stderr, "Unable to deinit PDL job.\n");
		exit(1);
	    }
        } else {
	    pl_process_eof(pjl_instance);
	    if ( pl_dnit_job(pjl_instance) < 0 ) {
		fprintf(gs_stderr, "Unable to deinit PJL.\n");
		exit(1);
	    }
        }
        /* close input file */
        pl_dnit_job(pjl_instance);
        pl_main_cursor_close(&r);
    }

    /* ----- End Main loop ----- */

    /* Dnit PDLs */
    if (pl_main_universe_dnit(&universe, err_buf)) {
	fputs(err_buf, gs_stderr);
	exit(1);
    }

    /* dnit pjl */
    if ( pl_deallocate_interp_instance(pjl_instance) < 0
	 || pl_deallocate_interp(pjl_interp) < 0 ) {
	fprintf(gs_stderr, "Unable to close out PJL instance\n");
	exit(1);
    }

#ifdef DEBUG
    if ( gs_debug_c(':') ) {
        pl_print_usage(mem, &inst, "Final");
        dprintf1("%% Max allocated = %ld\n", gs_malloc_max);
    }
    if (gs_debug_c('!'))
        debug_dump_memory(imem, &dump_control_default);
#endif

    /* release param list */
    gs_c_param_list_release(&params);
    arg_finit(&args);

    if ( gs_debug_c('A') )
	dprintf( "Final time" );
    pl_platform_dnit(0);
    return 0;
#undef mem
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
	pl_interp_instance_t   *pjl_instance,        /* pjl to reference */
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

	/* Create & init PDL all instances. Could do this lazily to save memory, */
	/* but for now it's simpler to just create all instances up front. */
	for (index = 0; pdl_implementation[index] != 0; ++index) {
	  pl_interp_instance_t *instance;

	  if ( pl_allocate_interp(&universe->pdl_interp_array[index],
	    pdl_implementation[index], mem) < 0
	  || pl_allocate_interp_instance(&universe->pdl_instance_array[index],
	   universe->pdl_interp_array[index], mem) < 0 ) {
	      if (err_str)
	        sprintf(err_str, "Unable to create %s interpreter",
	         pl_characteristics(pdl_implementation[index])->language);
	      goto pmui_err;
	  }

	  instance = universe->pdl_instance_array[index];
	  if ( pl_set_client_instance(instance, pjl_instance) < 0
	   || pl_set_pre_page_action(instance, pl_pre_finish_page, inst) < 0
	   || pl_set_post_page_action(instance, pl_post_finish_page, inst) < 0 ) {
	    if (err_str)
	      sprintf(err_str, "Unable to init %s interpreter",
	       pl_characteristics(pdl_implementation[index])->language);
	    goto pmui_err;
	  }
	}
	return 0;

pmui_err:
	pl_main_universe_dnit(universe, 0);
	return -1;
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
        sprintf(err_str, "Unable to close out PDL instance\n");
      return -1;
    }

    /* dnit interps */
    for (index = 0; universe->pdl_implementation[index] != 0; ++index)
      if ( (universe->pdl_instance_array[index]
        && pl_deallocate_interp_instance(universe->pdl_instance_array[index]) < 0)
       || (universe->pdl_interp_array[index]
        && pl_deallocate_interp(universe->pdl_interp_array[index]) < 0 )) {
          if (err_str)
            sprintf(err_str, "Unable to close out %s instance\n",
             pl_characteristics(universe->pdl_implementation[index])->language);
          return -1;
      }

    /* dealloc device if sel'd */
    if (universe->curr_device) {
      gs_closedevice(universe->curr_device);
      gs_free_object(universe->mem, universe->curr_device,
       "pl_main_universe_dnit(gx_device)");
    }

    return 0;
}

/* Select new device and/or implementation, deselect one one (opt) */
pl_interp_instance_t *    /* rets current interp_instance, 0 if err */
pl_main_universe_select(
	pl_main_universe_t               *universe,              /* universe to select from */
	char                             *err_str,               /* RETURNS error str if error */
	pl_interp_implementation_t const *desired_implementation,/* impl to select */
	gx_device                        *desired_device,        /* device to select */
	gs_param_list                    *params,                /* device params to set */
	char *                           pcl_personality         /* an additional parameter
                                                                    for selecting pcl personality */
)
{	
    int params_are_set = 0;

    /* If new interpreter/device is different, deselect it from old interp */
    if ((universe->curr_implementation
	 && universe->curr_implementation != desired_implementation)
	|| (universe->curr_device && universe->curr_device != desired_device)) {
	if (universe->curr_instance
	    && pl_remove_device(universe->curr_instance) < 0) {
	    if (err_str)
		strcpy(err_str, "Unable to deselect device from interp instance\n");
	    return 0;
	}
	if (universe->curr_device && universe->curr_device != desired_device) {
	    /* Here, we close the device. Note that this is not an absolute */
	    /* requirement: we could have a pool of open devices & select them */
	    /* into interp_instances as needed. The reason we force a close */
	    /* here is that multiple *async* devices would need coordination */
	    /* since an async device is not guaranteed to have completed */
	    /* rendering until it is closed. So, we close devices here to */
	    /* avoid things like intermingling of output streams. */
	    if (gs_closedevice(universe->curr_device) < 0) {
		if (err_str)
		    strcpy(err_str, "Unable to close device\n");
		return 0;
	    } else {
		/* Delete the device. */
		gs_free_object(universe->mem,
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
		strcpy(err_str, "Unable to set params into device\n");
		return 0;
	    }
	    params_are_set = 1;

	    if (gs_opendevice(desired_device) < 0) {
		if (err_str)
		    strcpy(err_str, "Unable to open new device\n");
		return 0;
	    } else
		universe->curr_device = desired_device;
	}

	/* NB fix me this parameter should not be passed this way */
	universe->curr_instance->pcl_personality = pcl_personality;
	/* Select curr/new device into PDL instance */
	if ( pl_set_device(universe->curr_instance, universe->curr_device) < 0 ) {
	    if (err_str)
		strcpy(err_str, "Unable to install device into PDL interp");
	    return 0;
	}
    }

    /* Set latest params into device. Write them all in case any changed */
    if ( !params_are_set
	 && gs_putdeviceparams(universe->curr_device, params) < 0 ) {
	strcpy(err_str, "Unable to set params into device\n");
	return 0;
    }
    return universe->curr_instance;
}

/* ------- Functions related to pl_main_instance_t ------ */

/* Initialize the instance parameters. */
void
pl_main_init_instance(pl_main_instance_t *pti, gs_memory_t *mem)
{	pti->memory = mem;
	pti->error_report = -1;
	pti->pause = true;
	pti->device = 0;
	pti->implementation = 0;
	gp_get_usertime(pti->base_time);
	pti->first_page = 1;
	pti->last_page = max_int;
	pti->page_count = 0;
	strcpy(&pti->pcl_personality, "PCL");
}

/* -------- Command-line processing ------ */

/* Create a default device if not already defined. */
int
pl_top_create_device(pl_main_instance_t *pti, int index, bool is_default)
{	int code = 0;
	if ( !is_default || !pti->device )
	  { const gx_device **list;

	    gs_lib_device_list((const gx_device * const **)&list, NULL);
	    code = gs_copydevice(&pti->device, list[index], pti->memory);
	  }
	return code;
}

/* Process the options on the command line. */
private FILE *
pl_main_arg_fopen(const char *fname, void *ignore_data)
{	return fopen(fname, "r");
}

#define arg_heap_copy(str) arg_copy(str, &gs_memory_default)
int
pl_main_process_options(pl_main_instance_t *pmi, arg_list *pal,
 gs_c_param_list *params, pl_interp_implementation_t const * const impl_array[])
{	const gx_device **dev_list;
	int num_devs = gs_lib_device_list((const gx_device * const **)&dev_list, NULL);
	int code = 0;
	bool help = false;
	const char *arg;
#define plist ((gs_param_list *)params)

	gs_c_param_list_write_more(params);
	while ( (arg = arg_next(pal)) != 0 && *arg == '-' )
	  { /* just - read from stdin */
	    if ( arg[1] == NULL )
	      break;
	    arg += 2;
	    switch ( arg[-1] )
	      {
	      default:
		fprintf(gs_stderr, "Unrecognized switch: %s\n", arg);
		return -1;
	      case '\0':
		  /* read from stdin - must be last arg */
		  continue;
	      case 'd':
	      case 'D':
		if ( !strcmp(arg, "BATCH") )
		  continue;
		if ( !strcmp(arg, "NOPAUSE") )
		  { pmi->pause = false;
		    continue;
		  }
		{ /* We're setting a device parameter to a non-string value. */
		  /* Currently we only support integer values; */
		  /* in the future we may support Booleans and floats. */
		  char *eqp = strchr(arg, '=');
		  char eqchar;
		  const char *value;
		  int vi;
		  
		  if ( eqp || (eqp = strchr(arg, '#')) )
		    eqchar = *eqp, *eqp = 0, value = eqp + 1;
		  else
		    value = "true";
		  if ( sscanf(value, "%d", &vi) != 1 )
		    { fputs("Usage for -d is -d<option>=<integer>\n", gs_stderr);
		      return -1;
		    }
		  if ( !strcmp(arg, "FirstPage") )
		    pmi->first_page = max(vi, 1);
		  else if ( !strcmp(arg, "LastPage") )
		    pmi->last_page = vi;
		  else
		    code = param_write_int(plist, arg_heap_copy(arg), &vi);
		}
		break;
	      case 'E':
		if ( *arg == 0 )
		  gs_debug['#'] = 1;
		else
		  sscanf(arg, "%d", &pmi->error_report);
		break;
	      case 'g':
		{ int geom[2];
		  gs_param_int_array ia;

		  if ( sscanf(arg, "%ux%u", &geom[0], &geom[1]) != 2 )
		  { fputs("-g must be followed by <width>x<height>\n",
			  gs_stderr);
		    return -1;
		  }
		  ia.data = geom;
		  ia.size = 2;
		  ia.persistent = false;
		  code = param_write_int_array(plist, "HWSize", &ia);
		}
		break;
	      case 'h':
		help = true;
		goto out;
	      case 'K':		/* max memory in K */
		{ int maxk;

		  if ( sscanf(arg, "%d", &maxk) != 1 )
		    { fputs("-K must be followed by a number\n", gs_stderr);
		      return -1;
		    }
		  gs_malloc_limit = (long)maxk << 10;
		}
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
		{ float res[2];
		  gs_param_float_array fa;

		  switch ( sscanf(arg, "%fx%f", &res[0], &res[1]) )
		    {
		    default:
		      fputs("-r must be followed by <res> or <xres>x<yres>\n",
			    gs_stderr);
		      return -1;
		    case 1:	/* -r<res> */
		      res[1] = res[0];
		    case 2:	/* -r<xres>x<yres> */
		      ;
		    }
		  fa.data = res;
		  fa.size = 2;
		  fa.persistent = false;
		  code = param_write_float_array(plist, "HWResolution", &fa);
		}
		break;
	      case 's':
	      case 'S':
		{ /* We're setting a device parameter to a string. */
		  char *eqp = strchr(arg, '=');
		  char eqchar;
		  const char *value;
		  gs_param_string str;

		  if ( !(eqp || (eqp = strchr(arg, '#'))) )
		    { fputs("Usage for -s is -s<option>=<string>\n", gs_stderr);
		      return -1;
		    }
		  eqchar = *eqp, *eqp = 0, value = eqp + 1;
		  if ( !strcmp(arg, "DEVICE") )
		  { int di;

		    for ( di = 0; di < num_devs; ++di )
		      if ( !strcmp(gs_devicename(dev_list[di]), value) )
			break;
		    if ( di == num_devs )
		      { fprintf(gs_stderr, "Unknown device name %s.\n", value);
		        return -1;
		      }
		    pl_top_create_device(pmi, di, false);
		  }
		  else
		    { param_string_from_string(str, value);
		      code = param_write_string(plist, arg_heap_copy(arg),
						&str);
		    }
		}
		break;
	      case 'Z':
		{ const char *p = arg;
		  for ( ; *p; ++p )
		    gs_debug[(int)*p] = 1;
		}
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
	            fputs("Choose language in -L<language> from: ", gs_stderr);
	            for (index = 0; impl_array[index] != 0; ++index)
	              fprintf(gs_stderr, "%s ",
	               pl_characteristics(impl_array[index])->language);
	            fputs("\n", gs_stderr);
	            return -1;
	          }
	          break;
	        }
	      }
	  }
out:	if ( help )
	  { 
	    arg_finit(pal);
	    gs_c_param_list_release(params);
	    return -1;
	  }
	gs_c_param_list_read(params);
	pl_top_create_device(pmi, 0, true); /* create default device if needed */
#undef plist
	/* The last argument wasn't a switch, so push it back. */
	if (arg)
	  arg_push_string(pal, arg);
	return 0;
}

/* either the (1) implementation has been selected on the command line or
   (2) it has been selected in PJL or (3) we need to auto sense. */
private pl_interp_implementation_t const *
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
private pl_interp_implementation_t const *
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
private pl_interp_implementation_t const *
pl_auto_sense(
   const char*                      name,         /* stream  */
   int                              buffer_length, /* length of stream */
   pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
)
{
    /* Lookup this string in the auto sense field for each implementation */
    pl_interp_implementation_t const * const * impl;
    for (impl = impl_array; *impl != 0; ++impl) {
	if (buffer_length >= (strlen(pl_characteristics(*impl)->auto_sense_string) - 1) )
	    if ( !strncmp(pl_characteristics(*impl)->auto_sense_string,
			  name,
			  (strlen(pl_characteristics(*impl)->auto_sense_string) - 1)) )
		return *impl;
    }
    /* Defaults to PCL */
    return impl_array[0];
}

/* Print memory and time usage. */
void
pl_print_usage(gs_memory_t *mem, const pl_main_instance_t *pti,
  const char *msg)
{	gs_memory_status_t status;
	long utime[2];

	gs_memory_status(mem, &status);
	gp_get_usertime(utime);
	dprintf5("%% %s time = %g, pages = %d, memory allocated = %lu, used = %lu\n",
		 msg, utime[0] - pti->base_time[0] +
		 (utime[1] - pti->base_time[1]) / 1000000000.0,
		 pti->page_count, status.allocated, status.used);
	dprintf1("%% Max allocated = %ld\n", gs_malloc_max);
}

/* Log a string to console, optionally wait for input */
void
pl_log_string(char *str, int wait_for_key)
{	fputs(str, gs_stderr);
	if (wait_for_key)
	  getchar();
}

/* Pre-page portion of page finishing routine */
int	/* ret 0 if page should be printed, 1 if no print, else -ve error */
pl_pre_finish_page(pl_interp_instance_t *interp, void *closure)
{
	pl_main_instance_t *pti = (pl_main_instance_t *)closure;
	++(pti->page_count);
	if ( pti->page_count >= pti->first_page &&
	     pti->page_count <= pti->last_page
	   )
	  {
	    if ( !pti->pause && gs_debug_c(':') )
	      pl_print_usage(pti->memory, pti, "render:");
	    return 0;   /* allow printing to proceed */
	  }
	else
	  {
	    /* prevent printing (& pl_post_finish_page) from occurring */
	    return 1;
	  }
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
	    pl_log_string(strbuf, 1);
	  }
	else if ( gs_debug_c(':') )
	  pl_print_usage(pti->memory, pti, " done :");

	return 0;
}

/* ---------------- Stubs ---------------- */
/* Error termination, called back from plplatf.c */
/* Only called back if abnormal termination */
void
pl_exit(int exit_status)
{
	exit(exit_status);
}

/* -------------- Read file cursor operations ---------- */
/* Open a read cursor w/specified file */
int	/* returns 0 ok, else -ve error code */
pl_main_cursor_open(
	pl_top_cursor_t  *cursor,        /* cursor to init/open */
   const char       *fname,         /* name of file to open */
	byte             *buffer,        /* buffer to use for reading */
   unsigned         buffer_length   /* length of *buffer */
)
{
	/* try to open file */
        if (fname[0] == '-' && fname[1] == NULL)
	    cursor->strm = stdin;
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

