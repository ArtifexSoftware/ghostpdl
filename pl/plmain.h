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

/*
 * Define the parameters for running the interpreter.
 */
typedef struct pl_main_instance_s {
	/* The following are set at initialization time. */
  gs_memory_t *memory;
  long base_time[2];		/* starting usertime */
  int error_report;		/* -E# */
  bool pause;			/* -dNOPAUSE => false */
  int first_page;		/* -dFirstPage= */
  int last_page;		/* -dLastPage= */
  gx_device *device;
  pl_interp_implementation_t const *implementation; /*-L<Language>*/
	/* The following are updated dynamically. */
  int page_count;		/* # of pages printed */

  char pcl_personality[6];      /* a character string to set pcl's
				   personality - rtl, pcl5c, pcl5e, and
				   pcl == default.  NB doesn't belong here. */

} pl_main_instance_t;

/* initialize gs_stdin, gs_stdout, and gs_stderr.  Eventually the gs
   library should provide an interface for doing this */
void pl_main_init_standard_io(void);

/* Initialize the instance parameters. */
void pl_main_init(P2(pl_main_instance_t *pmi, gs_memory_t *memory));

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
