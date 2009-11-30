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
    gs_memory_t *device_memory;
    long base_time[2];		/* starting usertime */
    int error_report;		/* -E# */
    bool pause;			/* -dNOPAUSE => false */
    bool print_page_count;      /* print the page number to stdout at
                                   the end of the job. */
    int first_page;		/* -dFirstPage= */
    int last_page;		/* -dLastPage= */
    gx_device *device;
    vm_spaces spaces;           /* spaces for "ersatz" garbage collector */

    pl_interp_implementation_t const *implementation; /*-L<Language>*/
    /* The following are updated dynamically. */
    int page_count;		/* # of pages printed */

    bool saved_hwres;
    float hwres[2];
    bool viewer; /* speed optimizations for viewer; NB MAY not always be correct! */
    char pcl_personality[6];      /* a character string to set pcl's
                                     personality - rtl, pcl5c, pcl5e, and
                                     pcl == default.  NB doesn't belong here. */
    bool interpolate;
    bool leak_check;
} pl_main_instance_t;

/* initialize gs_stdin, gs_stdout, and gs_stderr.  Eventually the gs
   library should provide an interface for doing this */
void pl_main_init_standard_io(void);

/* Initialize the instance parameters. */
void pl_main_init(pl_main_instance_t *pmi, gs_memory_t *memory);

/* Allocate and initialize the first graphics state. */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif
int pl_main_make_gstate(pl_main_instance_t *pmi, gs_state **ppgs);

#ifdef DEBUG
/* Print memory and time usage. */
void pl_print_usage(const pl_main_instance_t *pmi,
                    const char *msg);
#endif

/* Finish a page, possibly printing usage statistics and/or pausing. */
int pl_finish_page(pl_main_instance_t *pmi, gs_state *pgs,
                   int num_copies, int flush);

#endif				/* plmain_INCLUDED */
