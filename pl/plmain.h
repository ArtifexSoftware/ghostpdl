/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plmain.h */
/* Interface to main program utilities for PCL interpreters */

#ifndef plmain_INCLUDED
#  define plmain_INCLUDED

#include "gsargs.h"
#include "gsgc.h"
/*
 * Define the parameters for running the interpreter.
 */
#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif
typedef struct pl_main_instance_s {
	/* The following are set at initialization time. */
  gs_memory_t *memory;
  vm_spaces spaces;		/* spaces for GC */
  long base_time[2];		/* starting usertime */
  int error_report;		/* -E# */
  bool pause;			/* -dNOPAUSE => false */
  int first_page;		/* -dFirstPage= */
  int last_page;		/* -dLastPage= */
  gx_device *device;
	/* The following are updated dynamically. */
  int page_count;		/* # of pages printed */
  long prev_allocated;		/* memory allocated as of startup or last GC */
} pl_main_instance_t;

/* initialize gs_stdin, gs_stdout, and gs_stderr.  Eventually the gs
   library should provide an interface for doing this */
void pl_main_init_standard_io(void);

/* Initialize the instance parameters. */
void pl_main_init(P2(pl_main_instance_t *pmi, gs_memory_t *memory));

/* Process the options on the command line, including making the
   initial device and setting its parameters.  Clients can also pass
   in a version number and build date that will be printed as part of
   the "usage" statement */
int pl_main_process_options(P4(pl_main_instance_t *pmi, arg_list *pal,
			       char **argv, int argc));

/* Allocate and initialize the first graphics state. */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif
int pl_main_make_gstate(P2(pl_main_instance_t *pmi, gs_state **ppgs));

#ifdef DEBUG
/* Print memory and time usage. */
void pl_print_usage(P3(gs_memory_t *mem, const pl_main_instance_t *pmi,
		       const char *msg));
#endif

/* Finish a page, possibly printing usage statistics and/or pausing. */
int pl_finish_page(P4(pl_main_instance_t *pmi, gs_state *pgs,
		      int num_copies, int flush));

#endif				/* plmain_INCLUDED */
