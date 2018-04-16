/* Copyright (C) 2001-2018 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* plmain.h */
/* Interface to main program utilities for PCL interpreters */

#ifndef plmain_INCLUDED
#  define plmain_INCLUDED

#include "stdpre.h"
#include "gsmemory.h"
#include "pltop.h"
#include "gsparam.h"
#include "gsargs.h"

/*
 * Main instance for all interpreters.
 */
struct pl_main_instance_s
{
    /* The following are set at initialization time. */
    gs_memory_t *memory;
    long base_time[2];          /* starting time */
    int error_report;           /* -E# */
    bool pause;                 /* -dNOPAUSE => false */
    int first_page;             /* -dFirstPage= */
    int last_page;              /* -dLastPage= */
    bool pdfdebug;
    bool pdfstoponerror;
    bool pdfstoponwarning;
    bool notransparency;
    bool nocidfallback;
    bool no_pdfmark_outlines;
    bool no_pdfmark_dests;
    bool pdffitpage;
    bool usecropbox;
    bool useartbox;
    bool usebleedbox;
    bool usetrimbox;
    bool printed;
    bool showacroform;
    bool showannots;
    bool nouserunit;
    bool renderttnotdef;

    gx_device *device;

    pl_interp_implementation_t *implementation; /*-L<Language>*/

    char pcl_personality[6];    /* a character string to set pcl's
                                   personality - rtl, pcl5c, pcl5e, and
                                   pcl == default.  NB doesn't belong here. */
    bool interpolate;
    bool nocache;
    bool page_set_on_command_line;
    bool res_set_on_command_line;
    bool high_level_device;
#ifndef OMIT_SAVED_PAGES_TEST
    bool saved_pages_test_mode;
#endif
    bool pjl_from_args; /* pjl was passed on the command line */
    int scanconverter;
    /* we have to store these in the main instance until the languages
       state is sufficiently initialized to set the parameters. */
    char *piccdir;
    char *pdefault_gray_icc;
    char *pdefault_rgb_icc;
    char *pdefault_cmyk_icc;
    char *PDFPassword;
    char *PageList;
    gs_c_param_list params;
    arg_list args;
    pl_interp_implementation_t **implementations;
    pl_interp_implementation_t *curr_implementation;
    pl_interp_implementation_t *desired_implementation;
    byte buf[8192]; /* languages read buffer */
    void *disp; /* display device pointer NB wrong - remove */
};

typedef struct pl_main_instance_s pl_main_instance_t;

/* initialize gs_stdin, gs_stdout, and gs_stderr.  Eventually the gs
   library should provide an interface for doing this */
void pl_main_init_standard_io(void);

/* Initialize the instance parameters. */
void pl_main_init(pl_main_instance_t * pmi, gs_memory_t * memory);

/* Allocate and initialize the first graphics state. */
#ifndef gs_gstate_DEFINED
#  define gs_gstate_DEFINED
typedef struct gs_gstate_s gs_gstate;
#endif
int pl_main_make_gstate(pl_main_instance_t * pmi, gs_gstate ** ppgs);

/* Print memory and time usage. */
void pl_print_usage(const pl_main_instance_t * pmi, const char *msg);

/* Finish a page, possibly printing usage statistics and/or pausing. */
int pl_finish_page(pl_main_instance_t * pmi, gs_gstate * pgs,
                   int num_copies, int flush);

/* common routine to set icc parameters usually passed from the command line. */
int pl_set_icc_params(const gs_memory_t *mem, gs_gstate *pgs);

pl_main_instance_t *pl_main_alloc_instance(gs_memory_t * memory);
int pl_main_set_display_callback(pl_main_instance_t *inst, void *callback);
int pl_main_run_file(pl_main_instance_t *minst, const char *filename);
int pl_main_init_with_args(pl_main_instance_t *inst, int argc, char *argv[]);
int pl_main_delete_instance(pl_main_instance_t *minst);
int pl_main_run_string_begin(void *instance);
int pl_main_run_string_continue(void *instance, const char *str, unsigned int length);
int pl_main_run_string_end(void *instance);
int pl_to_exit(gs_memory_t *mem);

/* instance accessors */
bool pl_main_get_interpolate(const gs_memory_t *mem);
bool pl_main_get_nocache(const gs_memory_t *mem);
bool pl_main_get_page_set_on_command_line(const gs_memory_t *mem);
bool pl_main_get_res_set_on_command_line(const gs_memory_t *mem);
bool pl_main_get_high_level_device(const gs_memory_t *mem);
int pl_main_get_scanconverter(const gs_memory_t *mem);
pl_main_instance_t *pl_main_get_instance(const gs_memory_t *mem);

/* retrieve the PJL instance so languages can query PJL. */
bool pl_main_get_pjl_from_args(const gs_memory_t *mem); /* pjl was passed on the command line */

/* retrieve the PCL instance, used by PXL for pass through mode */
char *pl_main_get_pcl_personality(const gs_memory_t *mem);

pl_interp_implementation_t *pl_main_get_pcl_instance(const gs_memory_t *mem);
pl_interp_implementation_t *pl_main_get_pjl_instance(const gs_memory_t *mem);
#endif /* plmain_INCLUDED */
