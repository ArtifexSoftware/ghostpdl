/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pxmain.c */
/* Main (test) program for PCL XL */
#undef DEBUG
#define DEBUG			/* always enable debug output */
#include "malloc_.h"
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"
#include "string_.h"
#include "gdebug.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gsio.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsargs.h"
#include "gp.h"
#include "gsmatrix.h"
#include "gsnogc.h"
#include "gsstate.h"		/* must precede gsdevice.h */
#include "gscoord.h"
#include "gsdevice.h"
#include "gslib.h"
#include "gspaint.h"		/* for gs_erasepage */
#include "gsparam.h"
#include "gsstruct.h"		/* for gxalloc.h */
#include "gxalloc.h"
#include "gxstate.h"
#include "plparse.h"
#include "pjparse.h"
#include "plmain.h"
#include "pxattr.h"		/* for pxparse.h */
#include "pxerrors.h"
#include "pxvalue.h"		/* for pxparse.h */
#include "pxparse.h"
#include "pxptable.h"
#include "pxstate.h"
#include "pxlver.h"

/* Define the input buffer size. */
#define buf_size max(px_parser_min_input_size, 1000)	/* see pxparse.h */

extern FILE *gs_debug_out;

/* Imported operators */
px_operator_proc(pxEndPage);
px_operator_proc(pxBeginSession);

/* Define the table of pointers to initialization data. */
#define init_(init) int init(P1(px_state_t *));
typedef init_((*px_init_proc));
#include "pconfig.h"
#undef init_
const px_init_proc px_init_table[] = {
#define init_(init) &init,
#include "pconfig.h"
#undef init_
	    0
};

/* If inst.pause is set, pause at the end of each page. */
private int
pause_end_page(px_state_t *pxs, int num_copies, int flush)
{	pl_main_instance_t *pmi = pxs->client_data;
	gs_memory_t *mem = pxs->memory;

	/*
	 * Check whether it's worth doing a garbage collection.
	 * Note that this only works if we don't relocate pointers,
	 * because pxs might get relocated.
	 */
	{ gs_memory_status_t status;
	  gs_memory_status(mem, &status);
	  if ( status.allocated > pmi->prev_allocated + 250000 ) {
	    if_debug2(':', "[:]%lu > %lu + 250K, garbage collecting\n",
		      (ulong)status.allocated, (ulong)pmi->prev_allocated);
	    gs_nogc_reclaim(&pmi->spaces, true);
	    gs_memory_status(mem, &status);
	    pmi->prev_allocated = status.allocated;
	  }
	}
	return pl_finish_page(pmi, pxs->pgs, num_copies, flush);
}

/* Initialize the parser state, and session parameters in case we get */
/* an error before the session is established. */
private int
px_main_init(px_parser_state_t *st, px_state_t *pxs, bool big_endian)
{	px_process_init(st, big_endian);
	{ px_args_t args;
	  px_value_t v[3];

		/* Measure */
	  v[0].type = pxd_scalar | pxd_ubyte;
	  v[0].value.i = eInch;
	  args.pv[0] = &v[0];
		/* UnitsPerMeasure */
	  v[1].type = pxd_xy | pxd_uint16;
	  v[1].value.ia[0] = v[1].value.ia[1] = 100; /* arbitrary */
	  args.pv[1] = &v[1];
		/* ErrorReporting */
	  v[2].type = pxd_scalar | pxd_ubyte;
	  v[2].value.i = eErrorPage;
	  args.pv[2] = &v[2];
	  return pxBeginSession(&args, pxs);
	}
}

/* Here is the main program. */
int
main(int argc, char *argv[])
{	gs_ref_memory_t *imem;
#define mem ((gs_memory_t *)imem)
	pl_main_instance_t inst;
	gs_state *pgs;
	px_parser_state_t *st;
	px_state_t *pxs;
	const char *arg;
	arg_list args;

	/* Initialize the library. */
	pl_main_init_standard_io();
	gp_init();
	gs_lib_init(gs_stderr);
	gs_debug_out = gs_stdout;
	imem = ialloc_alloc_state((gs_raw_memory_t *)&gs_memory_default, 20000);
	imem->space = 0;		/****** WRONG ******/
	pl_main_init(&inst, mem);
	pl_main_process_options(&inst, &args, argv, argc, PXLVERSION, PXLBUILDDATE);
	pl_main_make_gstate(&inst, &pgs);
	st = px_process_alloc(mem);
	pxs = px_state_alloc(mem);
	px_state_init(pxs, pgs);
	pxs->client_data = &inst;
	px_initgraphics(pxs);
	/* gsave and grestore (among other places) assume that */
	/* there are at least 2 gstates on the graphics stack. */
	/* Ensure that now. */
	gs_gsave(pgs);
	pxs->end_page = pause_end_page;

	/* gs_reclaim may change some procedures in the allocator; */
	/* get them set up now. */
	gs_nogc_reclaim(&inst.spaces, true);

	while ( (arg = arg_next(&args)) != 0 )
	  { /* Process one input file. */
	    FILE *in = fopen(arg, "rb");
	    byte buf[buf_size];
#define buf_last (buf + (buf_size - 1))
	    int len;
	    int code = 0;
	    stream_cursor_read r;
	    int in_pjl = 1;	/* -1 = skipping after error, */
				/* 0 = in PCL XL, 1 = in PJL */
	    int end_left = -1;

	    if ( gs_debug_c(':') )
	      { dprintf1("%% Reading %s:\n", arg);
	        pl_print_usage(mem, &inst, "Start");
	      }
	    if ( in == 0 )
	      { fprintf(gs_stderr, "Unable to open %s for reading.\n", arg);
	        exit(1);
	      }

	    pxs->pjls = pjl_process_init(mem);
	    r.limit = buf - 1;
	  for ( ; ; )
	    { if_debug1('i', "%% file pos=%ld\n", (long)ftell(in));
	      r.ptr = buf - 1;
	      len = fread(r.limit + 1, 1, buf_last - r.limit, in);
	      if ( len <= 0 )
		{ if ( r.limit - r.ptr == end_left )
		    break;
		  end_left = r.limit - r.ptr;
		}
	      else
		r.limit += len;
process:      switch ( in_pjl )
		{
		case -1:	/* skipping after error */
		  if ( pjl_skip_to_uel(&r) )
		    in_pjl = 1;
		  goto move;
		case 1:		/* PJL */
		  code = pjl_process(pxs->pjls, NULL, &r);
		  if ( code > 0 )
		    { byte chr;
		      in_pjl = 0;
		      chr = (r.ptr == r.limit ? fgetc(in) : *++r.ptr);
		      switch ( chr )
			{
			case '(': px_main_init(st, pxs, true); break;
			case ')': px_main_init(st, pxs, false); break;
			default:
			  /* Initialize state to avoid confusion */
			  px_main_init(st, pxs, true);
			  code = gs_note_error(errorUnsupportedBinding);
			  goto err;
			}
		      while ( (r.ptr == r.limit ? fgetc(in) : *++r.ptr) != '\n' )
			;
		      px_reset_errors(pxs);
		      goto process;
		    }
		  goto move;
		case 0:		/* PCL XL */
		  code = px_process(st, pxs, &r);
		  if ( code == e_ExitLanguage )
		    { in_pjl = 1;
		      px_state_cleanup(pxs);
		      code = 0;
		    }
		}
err:	      if ( code < 0 )
		{ /* Print an error report. */
		  int report =
		    (inst.error_report < 0 ? pxs->error_report :
		     inst.error_report);
		  const char *subsystem =
		    (code <= px_error_next ? "MAIN" : "GRAPHICS");
		  char message[px_max_error_line+1];
		  int N = 0;
		  int y;

		  /* Map library error codes to PCL XL codes when possible. */
		  switch ( code )
		    {
#define subst(gs_error, px_error)\
  case gs_error: code = px_error; break
		    subst(gs_error_invalidfont, errorIllegalFontData);
		    subst(gs_error_limitcheck, errorInternalOverflow);
		    subst(gs_error_nocurrentpoint, errorCurrentCursorUndefined);
		    subst(gs_error_rangecheck, errorIllegalAttributeValue);
		    subst(gs_error_VMerror, errorInsufficientMemory);
#undef subst
		    }
		  if ( report & eErrorPage )
		    y = px_begin_error_page(pxs);
		  while ( (N = px_error_message_line(message, N, subsystem,
						     code, st, pxs)) >= 0
			)
		    { if ( report & eBackChannel )
			fputs(message, gs_stderr);
		      if ( report & eErrorPage )
			y = px_error_page_show(message, y, pxs);
		    }
		if ( report & pxeErrorReport_next )
		  { long pos = ftell(in) - (r.limit - r.ptr);
		    fprintf(gs_stderr, "file position of error = %ld\n", pos);
		  }
		  if ( report & eBackChannel )
		    fflush(gs_stderr);
		  if ( report & eErrorPage )
		    { px_args_t args;
		      args.pv[0] = 0;
		      pxEndPage(&args, pxs);
		    }
		  px_reset_errors(pxs);
		  if ( code == errorWarningsReported )
		    { /* The parser doesn't skip over the EndSession */
		      /* operator, because an "error" occurred. */
		      r.ptr++;
		    }
		  else
		    { px_state_cleanup(pxs);
		      in_pjl = -1;	/* skip to end of job */
		    }
		}
	      if ( code != 0 )
		{ if_debug1('i', "%% Return code = %d\n", code);
		}
move:	      /* Move unread data to the start of the buffer. */
	      len = r.limit - r.ptr;
	      if ( len > 0 )
		memmove(buf, r.ptr + 1, len);
	      r.limit = buf + (len - 1);
	    }
	  gs_nogc_reclaim(&inst.spaces, true);
	  pjl_process_destroy(pxs->pjls, mem);
	  /* Reset the GC trigger. */
	  { gs_memory_status_t status;
	    gs_memory_status(mem, &status);
	    inst.prev_allocated = status.allocated;
	  }
	  if ( gs_debug_c(':') )
	    dprintf3("%% Final file position = %ld, exit code = %d, mode = %s\n",
		     (long)ftell(in) - (r.limit - r.ptr), code,
		     (in_pjl > 0 ? "PJL" : in_pjl < 0 ? "skipping error" :
		      "PCL XL"));
	  fclose(in);
	}
	if ( gs_debug_c(':') )
	  { pl_print_usage(mem, &inst, "Final");
	    dprintf1("%% Max allocated = %ld\n", gs_malloc_max);
	  }
	px_state_finit(pxs);
	/* At the library level, we currently can't count on */
	/* the memory manager finalizing (closing) devices. */
	gs_closedevice(gs_currentdevice(pgs));
	gs_lib_finit(0, 0);
	return 0;
}
