/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plmain.c */
/* Main program utilities for PCL interpreters */
#include "stdio_.h"
#include "string_.h"
#include "gdebug.h"
#include "gstypes.h"
#include "gsmemory.h"
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
Options: -dNOPAUSE -E[#] -h -Z...\n\
         -sDEVICE=<dev> -g<W>x<H> -r<X>[x<Y>] -d{First|Last}Page=<#>\n\
	 -sOutputFile=<file> (-s<option>=<string> | -d<option>[=<value>])*\n\
";

/* ---------------- Public procedures ---------------- */

/* Initialize the instance parameters. */
void
pl_main_init(pl_main_instance_t *pmi)
{	pmi->error_report = -1;
	pmi->pause = true;
	pmi->first_page = 1;
	pmi->last_page = max_int;
	pmi->device = 0;
}

/* Create a default device if necessary. */
private int
pl_main_create_device(pl_main_instance_t *pmi, int index)
{	if ( !pmi->device )
	  { const gx_device **list;
	    int code;

	    gs_lib_device_list(&list, NULL);
	    code = gs_copydevice(&pmi->device, list[index], pmi->memory);
	    if ( code < 0 )
	      { fprintf(stderr, "Fatal error: gs_copydevice returned %d\n",
			code);
	        exit(1);
	      }
	  }
	return 0;
}

/* Process the options on the command line. */
int
pl_main_process_options(pl_main_instance_t *pmi, char *argv[], int argc,
  gs_memory_t *memory)
{	const gx_device **dev_list;
	int num_devs = gs_lib_device_list(&dev_list, NULL);
	gs_c_param_list params;
#define plist ((gs_param_list *)&params)
	int i, code = 0;

	pmi->memory = memory;
	gs_c_param_list_write(&params, memory);
	for ( i = 1; i < argc && argv[i][0] == '-'; ++i )
	  { const char *arg = argv[i] + 2;

	    switch ( arg[-1] )
	      {
	      default:
		fprintf(stderr, "Unrecognized switch: %s\n", argv[i]);
		exit(1);
	      case 'd':
	      case 'D':
		if ( !strcmp(arg, "NOPAUSE") )
		  { pmi->pause = false;
		    continue;
		  }
		{ /* We're setting a device parameter to a non-string value. */
		  /* Currently we only support integer values; */
		  /* in the future we may support Booleans and floats. */
		  char *eqp = strchr(arg, '=');
		  const char *value;
		  int vi;
		  
		  if ( eqp )
		    *eqp = 0, value = eqp + 1;
		  else
		    value = "true";
		  if ( sscanf(value, "%d", &vi) != 1 )
		    { fputs("Usage for -d is -d<option>=<integer>\n", stderr);
		      exit(1);
		    }
		  if ( !strcmp(arg, "FirstPage") )
		    pmi->first_page = max(vi, 1);
		  else if ( !strcmp(arg, "LastPage") )
		    pmi->last_page = vi;
		  else
		    code = param_write_int(plist, arg, &vi);
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
			  stderr);
		    exit(1);
		  }
		  ia.data = geom;
		  ia.size = 2;
		  ia.persistent = false;
		  code = param_write_int_array(plist, "HWSize", &ia);
		}
		break;
	      case 'h':
		i = argc - 1;	/* force exit */
		break;
	      case 'r':
		{ float res[2];
		  gs_param_float_array fa;

		  switch ( sscanf(arg, "%fx%f", &res[0], &res[1]) )
		    {
		    default:
		      fputs("-r must be followed by <res> or <xres>x<yres>\n",
			    stderr);
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
		if ( !strncmp(arg, "DEVICE=", 7) )
		  { const char *device_name = arg + 7;
		    int di;

		    if ( pmi->device )
		      { fputs("-sDEVICE= must precede any input files\n",
			      stderr);
		        exit(1);
		      }
		    for ( di = 0; di < num_devs; ++di )
		      if ( !strcmp(gs_devicename(dev_list[di]), device_name) )
			break;
		    if ( di == num_devs )
		      { fprintf(stderr, "Unknown device name %s.\n",
				device_name);
		        exit(1);
		      }
		    pl_main_create_device(pmi, di);
		    continue;
		  }
		{ /* We're setting a device parameter to a string. */
		  char *eqp = strchr(arg, '=');
		  gs_param_string str;

		  if ( eqp == NULL )
		    { fputs("Usage for -s is -s<option>=<string>\n", stderr);
		      exit(1);
		    }
		  *eqp = 0;
		  param_string_from_string(str, eqp + 1);
		  code = param_write_string(plist, arg, &str);
		}
		break;
	      case 'Z':
		{ const char *p = arg;
		  for ( ; *p; ++p )
		    gs_debug[(int)*p] = 1;
		}
	      }
	  }
	if ( argc < 2 || i == argc )
	  { gs_c_param_list_release(&params);
	    fprintf(stderr, pl_usage, argv[0]);
	    fputs("Devices:", stderr);
	    for ( i = 0; i < num_devs; ++i )
	      fprintf(stderr, " %s", gs_devicename(dev_list[i]));
	    fputs("\n", stderr);
	    exit(1);
	  }
	gs_c_param_list_read(&params);
	pl_main_create_device(pmi, 0); /* create default device if needed */
	code = gs_putdeviceparams(pmi->device, plist);
	gs_c_param_list_release(&params);
#undef plist
	return i;
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
	*ppgs = pgs;
	return 0;
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
