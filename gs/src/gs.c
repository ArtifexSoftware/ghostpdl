/* Copyright (C) 1989, 1995, 1996, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* 'main' program for Ghostscript */
#include "ghost.h"
#include "imain.h"
#include "imainarg.h"
#include "iminst.h"

/* Define an optional array of strings for testing. */
/*#define RUN_STRINGS */
#ifdef RUN_STRINGS
private const char *run_strings[] =
{
    "2 vmreclaim /SAVE save def 2 vmreclaim",
    "(saved\n) print flush",
    "SAVE restore (restored\n) print flush 2 vmreclaim",
    "(done\n) print flush quit",
    0
};

#endif

int
main(int argc, char *argv[])
{
    gs_main_instance *minst = gs_main_instance_default();
    int code = gs_main_init_with_args(minst, argc, argv);

    if (code < 0) {
	gs_exit_with_code(255, code);
	/* NOTREACHED */
    }
#ifdef RUN_STRINGS
    {				/* Run a list of strings (for testing). */
	const char **pstr = run_strings;

	for (; *pstr; ++pstr) {
	    int exit_code;
	    ref error_object;
	    int code;

	    fprintf(stdout, "{%s} =>\n", *pstr);
	    fflush(stdout);
	    code = gs_main_run_string(minst, *pstr, 0,
				      &exit_code, &error_object);
	    zflush(osp);
	    fprintf(stdout, " => code = %d\n", code);
	    fflush(stdout);
	    if (code < 0) {
		gs_exit(1);
		/* NOTREACHED */
		return 1;	/* pacify compilers */
	    }
	}
    }
#endif

    if (minst->run_start)
	gs_main_run_start(minst);

    gs_exit(0);			/* exit */
    /* NOTREACHED */
    return 0;			/* pacify compilers */
}
