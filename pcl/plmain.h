/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* plmain.h */
/* Interface to main program utilities for PCL interpreters */

#ifndef plmain_INCLUDED
#  define plmain_INCLUDED

/*
 * Define the parameters for running the interpreter.
 */
#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif
typedef struct pl_main_instance_s {
  gs_memory_t *memory;
  int error_report;		/* -E# */
  bool pause;			/* -dNOPAUSE => false */
  int first_page;		/* -dFirstPage= */
  int last_page;		/* -dLastPage= */
  gx_device *device;
} pl_main_instance_t;

/* Initialize the instance parameters. */
void pl_main_init(P1(pl_main_instance_t *pmi));

/* Process the options on the command line, */
/* including making the initial device and setting its parameters. */
int pl_main_process_options(P4(pl_main_instance_t *pmi, char *argv[],
			       int argc, gs_memory_t *memory));

/* Allocate and initialize the first graphics state. */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif
int pl_main_make_gstate(P2(pl_main_instance_t *pmi, gs_state **ppgs));

#endif				/* plmain_INCLUDED */
