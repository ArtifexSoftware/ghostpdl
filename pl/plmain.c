/* plmain.c */
/* Main program utilities for PCL interpreters */
#undef DEBUG
#define DEBUG			/* always enable debug output */
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
#include "gsio.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsstruct.h"
#include "gp.h"
#include "gsmatrix.h"
#include "gscdefs.h"
#include "gsdevice.h"
#include "gslib.h"
#include "gsparam.h"
#include "gsstate.h"
#include "plmain.h"

/* Include the extern for the device list. */
extern_gs_lib_device_list();

/* Define the usage message. */
private const char *pl_usage = "\
Usage: %s option* file...\n\
Options: -dNOPAUSE -E[#] -h -K<maxK> -Z...\n\
         -sDEVICE=<dev> -g<W>x<H> -r<X>[x<Y>] -d{First|Last}Page=<#>\n\
	 -sOutputFile=<file> (-s<option>=<string> | -d<option>[=<value>])*\n\
";

/* ---------------- Public procedures ---------------- */

void
pl_main_init_standard_io()
{
    /* set up in, our err - not much we can do here if this fails */
    pl_get_real_stdio(&gs_stdin, &gs_stdout, &gs_stderr);
}
/* Initialize the instance parameters. */
void
pl_main_init(pl_main_instance_t *pmi, gs_memory_t *mem)
{	pmi->memory = mem;
	{ int i;
	  for ( i = 0; i < countof(pmi->spaces.indexed); ++i )
	    pmi->spaces.indexed[i] = 0;
	  pmi->spaces.named.local = pmi->spaces.named.global =
	    (gs_ref_memory_t *)mem;
	}
	gp_get_usertime(pmi->base_time);
	pmi->error_report = -1;
	pmi->pause = true;
	pmi->first_page = 1;
	pmi->last_page = max_int;
	pmi->device = 0;
	pmi->page_count = 0;
	{ gs_memory_status_t status;
	  gs_memory_status(mem, &status);
	  pmi->prev_allocated = status.allocated;
	}
}

/* Create a default device if necessary. */
private int
pl_main_create_device(pl_main_instance_t *pmi, int index)
{	if ( !pmi->device )
	  { const gx_device **list;
	    int code;

	    gs_lib_device_list((const gx_device * const **)&list, NULL);
	    code = gs_copydevice(&pmi->device, list[index], pmi->memory);
	    if ( code < 0 )
	      { fprintf(gs_stderr, "Fatal error: gs_copydevice returned %d\n",
			code);
	        exit(1);
	      }
	  }
	return 0;
}

/* Process the options on the command line. */
private FILE *
pl_main_arg_fopen(const char *fname, void *ignore_data)
{	return fopen(fname, "r");
}
#define arg_heap_copy(str) arg_copy(str, &gs_memory_default)
int
pl_main_process_options(pl_main_instance_t *pmi, arg_list *pal, char **argv,
  int argc, char *version, char *build_date)
{	gs_memory_t *mem = pmi->memory;
	const gx_device **dev_list;
	int num_devs = gs_lib_device_list((const gx_device * const **)&dev_list, NULL);
	int code = 0;
	bool help = false;
	const char *arg;
	gs_c_param_list params;
#define plist ((gs_param_list *)&params)

	gs_c_param_list_write(&params, mem);
	arg_init(pal, (const char **)argv, argc, pl_main_arg_fopen, NULL);
	while ( (arg = arg_next(pal)) != 0 && *arg == '-' )
	  { arg += 2;
	    switch ( arg[-1] )
	      {
	      default:
		fprintf(gs_stderr, "Unrecognized switch: %s\n", arg);
		exit(1);
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
		    eqchar = *eqp, value = eqp + 1;
		  else
		    value = "true";
		  if ( sscanf(value, "%d", &vi) != 1 )
		    { fputs("Usage for -d is -d<option>=<integer>\n", gs_stderr);
		      exit(1);
		    }
		  if ( !strncmp(arg, "FirstPage", strlen("FirstPage")) )
		    pmi->first_page = max(vi, 1);
		  else if ( !strncmp(arg, "LastPage", strlen("FirstPage")) )
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
		    exit(1);
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
		      exit(1);
		    }
		  gs_malloc_limit = (long)maxk << 10;
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
		      exit(1);
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
		      exit(1);
		    }
		  eqchar = *eqp, value = eqp + 1;
		  if ( !strncmp(arg, "DEVICE", strlen("DEVICE")) )
		  { int di;

		    if ( pmi->device )
		      { fputs("-sDEVICE= must precede any input files\n",
			      gs_stderr);
		        exit(1);
		      }
		    for ( di = 0; di < num_devs; ++di )
		      if ( !strcmp(gs_devicename(dev_list[di]), value) )
			break;
		    if ( di == num_devs )
		      { fprintf(gs_stderr, "Unknown device name %s.\n", value);
		        exit(1);
		      }
		    pl_main_create_device(pmi, di);
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
	      }
	  }
out:	if ( arg == 0 || help )
	  { int i;
	    arg_finit(pal);
	    gs_c_param_list_release(&params);
	    fprintf(gs_stderr, pl_usage, argv[0]);
	    if (version)
		fprintf(gs_stderr, "Version: %s\n", version);
	    if (build_date)
		fprintf(gs_stderr, "Build date: %s\n", build_date);
	    fputs("Devices:", gs_stderr);
	    for ( i = 0; i < num_devs; ++i ) {
		if ( ( (i + 1) )  % 9 == 0 )
		    fputs("\n", gs_stderr);
		fprintf(gs_stderr, " %s", gs_devicename(dev_list[i]));
	    }
	    fputs("\n", gs_stderr);
	    exit(1);
	  }
	gs_c_param_list_read(&params);
	pl_main_create_device(pmi, 0); /* create default device if needed */
	code = gs_putdeviceparams(pmi->device, plist);
	gs_c_param_list_release(&params);
#undef plist
	/* The last argument wasn't a switch, so push it back. */
	arg_push_string(pal, arg);
	return 0;
}

/* Allocate and initialize the first graphics state. */
int
pl_main_make_gstate(pl_main_instance_t *pmi, gs_state **ppgs)
{	gs_state *pgs;
	int code = pl_main_create_device(pmi, 0);

	if ( code < 0 )
	  return code;
	pgs = gs_state_alloc(pmi->memory);
	gs_setdevice_no_erase(pgs, pmi->device);	/* can't erase yet */
	/* All H-P languages want accurate curves. */
	gs_setaccuratecurves(pgs, true);
	*ppgs = pgs;
	return 0;
}

/* Print memory and time usage. */
void
pl_print_usage(gs_memory_t *mem, const pl_main_instance_t *pmi,
  const char *msg)
{	gs_memory_status_t status;
	long utime[2];

	gs_memory_status(mem, &status);
	gp_get_usertime(utime);
	dprintf5("%% %s time = %g, pages = %d, memory allocated = %lu, used = %lu\n",
		 msg, utime[0] - pmi->base_time[0] +
		 (utime[1] - pmi->base_time[1]) / 1000000000.0,
		 pmi->page_count, status.allocated, status.used);
}

/* Finish a page, possibly printing usage statistics and/or pausing. */
int
pl_finish_page(pl_main_instance_t *pmi, gs_state *pgs, int num_copies,
  int flush)
{	int code = 0;

	++(pmi->page_count);
	if ( pmi->page_count >= pmi->first_page &&
	     pmi->page_count <= pmi->last_page
	   )
	  {
	    if ( !pmi->pause && gs_debug_c(':') )
	      pl_print_usage(pmi->memory, pmi, "render:");
	    code = gs_output_page(pgs, num_copies, flush);
	    if ( pmi->pause )
	      { fprintf(gs_stderr, "End of page %d, press <enter> to continue.\n",
			pmi->page_count);
	        getchar();
	      }
	    else if ( gs_debug_c(':') )
	      pl_print_usage(pmi->memory, pmi, " done :");
	  }
	return code;
}

/* ---------------- Stubs ---------------- */

/* Stubs for GC */
const gs_ptr_procs_t ptr_struct_procs = { NULL, NULL, NULL };
const gs_ptr_procs_t ptr_string_procs = { NULL, NULL, NULL };
const gs_ptr_procs_t ptr_const_string_procs = { NULL, NULL, NULL };
void * /* obj_header_t * */
gs_reloc_struct_ptr(const void * /* obj_header_t * */ obj, gc_state_t *gcst)
{	return (void *)obj;
}
void
gs_reloc_string(gs_string *sptr, gc_state_t *gcst)
{
}
void
gs_reloc_const_string(gs_const_string *sptr, gc_state_t *gcst)
{
}

/* Other stubs */
void
gs_exit(int exit_status)
{	gs_lib_finit(exit_status, 0);
	exit(exit_status);
}
