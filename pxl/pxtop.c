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

/* pxtop.c */
/* Top-level API implementation of PCL/XL */

#include "stdio_.h"
#include "string_.h"
#include "gdebug.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsnogc.h"
#include "gsstate.h"		/* must precede gsdevice.h */
#include "gsdevice.h"
#include "gsstruct.h"		/* for gxalloc.h */
#include "gspaint.h"
#include "gsfont.h"
#include "gxalloc.h"
#include "gxstate.h"
#include "gxdevice.h"
#include "plparse.h"
#include "pxattr.h"     	/* for pxparse.h */
#include "pxerrors.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxfont.h"
#include "pxvalue.h"		/* for pxparse.h */
#include "pxparse.h"
#include "pxptable.h"
#include "pxstate.h"
#include "pltop.h"
#include "gsicc_manage.h"

/* Define the table of pointers to initialization data. */

typedef int  (*px_init_proc) ( px_state_t * );

int pxfont_init(px_state_t *pxs );

const px_init_proc px_init_table[] = {
    &pxfont_init,
    0
};

/* Imported operators */
px_operator_proc(pxEndPage);
px_operator_proc(pxBeginSession);

/* Forward decls */
static int pxl_end_page_top(px_state_t *pxs, int num_copies, int flush);
static int px_top_init(px_parser_state_t *st, px_state_t *pxs, bool big_endian);


/* ------------ PCXL stream header processor ------- */
/* State used to process an XL stream header */
typedef struct px_stream_header_process_s {
	enum {PSHPReady, PSHPSkipping, PSHPDone} state;
	px_parser_state_t         *st;         /* parser state to refer to */
   px_state_t                *pxs;        /* xl state to refer to */
} px_stream_header_process_t;


/* Initialize stream header processor */
static void
px_stream_header_init(
	px_stream_header_process_t *process,     /* header state to init */
	px_parser_state_t          *st,          /* parser state to refer to */
	px_state_t                 *pxs          /* xl state to refer to */
)
{
	process->state = PSHPReady;
	process->st = st;
	process->pxs = pxs;
}

/* Process stream header input */
static int	/* ret -ve error, 0 if needs more input, 1 if done successfully */
px_stream_header_process(
	px_stream_header_process_t *process,     /* header state to refer */
	stream_cursor_read         *cursor       /* cusor to read data */
)
{
	while (cursor->ptr != cursor->limit)
	  {
	  byte chr;
	  switch (process->state)
	    {
	  case PSHPReady:
	    process->state = PSHPSkipping;   /* switch to next state */
	    switch ( (chr = *++cursor->ptr) )
	      {
	    case '(':
	      px_top_init(process->st, process->pxs, true);
	      break;
	    case ')':
	      px_top_init(process->st, process->pxs, false);
	      break;
	    default:
	      /* Initialize state to avoid confusion */
	      px_top_init(process->st, process->pxs, true);
	      return gs_note_error(errorUnsupportedBinding);
	      }
	    break;
	  case PSHPSkipping:
	    if ( (chr = *++cursor->ptr) == '\n' )
	      {
	      process->state = PSHPDone;
	      return 1;
	      }
	    break;
	  case PSHPDone:
	  default:
	    /* Shouldn't ever come here */
	    return gs_note_error(errorIllegalStreamHeader);
	    }
	  }

	return 0;    /* need more input */
}

/* De-initialize stream header processor */
static void
px_stream_header_dnit(
	px_stream_header_process_t *process      /* header state to dnit */
)
{
	/* empty proc */
}

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

/*
 * PXL interpeter: derived from pl_interp_t
 */
typedef struct pxl_interp_s {
  pl_interp_t              pl;               /* common part: must be first */
  gs_memory_t              *memory;          /* memory allocator to use */
} pxl_interp_t;

/*
 * PXL interpreter instance: derived from pl_interp_instance_t
 */
typedef struct pxl_interp_instance_s {
  pl_interp_instance_t     pl;                /* common part: must be first */
  gs_memory_t              *memory;           /* memory allocator to use */
  px_parser_state_t        *st;               /* parser state */
  px_state_t               *pxs;              /* interp state */
  gs_state                 *pgs;              /* grafix state */
  pl_page_action_t         pre_page_action;   /* action before page out */
  void                     *pre_page_closure; /* closure to call pre_page_action with */
  pl_page_action_t         post_page_action;  /* action before page out */
  void                     *post_page_closure;/* closure to call post_page_action with */
  enum {PSHeader, PSXL, PSDone}
                           processState;      /* interp's processing state */
  px_stream_header_process_t
                           headerState;       /* used to decode stream header */
} pxl_interp_instance_t;


/* Get implemtation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
pxl_impl_characteristics(
  const pl_interp_implementation_t *impl     /* implementation of interpereter to alloc */
)
{
    /* version and build date are not currently used */
#define PXLVERSION NULL
#define PXLBUILDDATE NULL
  static pl_interp_characteristics_t pxl_characteristics = {
    "PCLXL",
    ") HP-PCL XL",
    "Artifex",
    PXLVERSION,
    PXLBUILDDATE,
    px_parser_min_input_size
  };
  return &pxl_characteristics;
}

/* Don't need to do anything to PXL interpreter */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_allocate_interp(
  pl_interp_t                      **interp,       /* RETURNS abstract interpreter struct */
  const pl_interp_implementation_t *impl,  /* implementation of interpereter to alloc */
  gs_memory_t                      *mem            /* allocator to allocate interp from */
)
{
	static pl_interp_t pi;	/* there's only one interpreter */

	/* There's only one PXL interp, so return the static */
	*interp = &pi;
	return 0;   /* success */
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_allocate_interp_instance(
  pl_interp_instance_t   **instance,     /* RETURNS instance struct */
  pl_interp_t            *interp,        /* dummy interpreter */
  gs_memory_t            *mem            /* allocator to allocate instance from */
)
{
	/* Allocate everything up front */
	pxl_interp_instance_t *pxli  /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
	 = (pxl_interp_instance_t *)gs_alloc_bytes( mem,
	                                            sizeof(pxl_interp_instance_t),
	                                            "pxl_allocate_interp_instance(pxl_interp_instance_t)"
                                              );
	gs_state *pgs = gs_state_alloc(mem);
	px_parser_state_t *st = px_process_alloc(mem);	/* parser init, cheap */
	px_state_t *pxs = px_state_alloc(mem);	/* inits interp state, potentially expensive */
	/* If allocation error, deallocate & return */
	if (!pxli || !pgs || !st || !pxs) {
	  if (pxli)
	    gs_free_object(mem, pxli, "pxl_impl_allocate_interp_instance(pxl_interp_instance_t)");
	  if (pgs)
	    gs_state_free(pgs);
	  if (st)
	    px_process_release(st);
	  if (pxs)
	    px_state_release(pxs);
	  return gs_error_VMerror;
	}
#ifdef ICCBRANCH
        gsicc_init_iccmanager(pgs);
#endif

	/* Setup pointers to allocated mem within instance */
	pxli->memory = mem;
	pxli->pgs = pgs;
	pxli->pxs = pxs;
	pxli->st = st;

	/* zero-init pre/post page actions for now */
	pxli->pre_page_action = 0;
	pxli->post_page_action = 0;

	/* General init of pxl_state */
	px_state_init(pxs, pgs);	/*pgs only needed to set pxs as pgs' client */
	pxs->client_data = pxli;
	pxs->end_page = pxl_end_page_top;	/* after px_state_init */

	/* Return success */
	*instance = (pl_interp_instance_t *)pxli;
	return 0;
}

/* Set a client language into an interperter instance */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_set_client_instance(
  pl_interp_instance_t         *instance,     /* interp instance to use */
  pl_interp_instance_t         *client,       /* client to set */
  pl_interp_instance_clients_t which_client
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
        if ( which_client == PCL_CLIENT )
            pxli->pxs->pcls = client;
        else if ( which_client == PJL_CLIENT )
            pxli->pxs->pjls = client;
        /* ignore unknown clients */
	return 0;
}

/* Set an interpreter instance's pre-page action */
static int   /* ret 0 ok, else -ve err */
pxl_impl_set_pre_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute (rets 1 to abort w/o err) */
  void                   *closure       /* closure to call action with */
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	pxli->pre_page_action = action;
	pxli->pre_page_closure = closure;
	return 0;
}

/* Set an interpreter instance's post-page action */
static int   /* ret 0 ok, else -ve err */
pxl_impl_set_post_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute */
  void                   *closure       /* closure to call action with */
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	pxli->post_page_action = action;
	pxli->post_page_closure = closure;
	return 0;
}

static bool
pxl_get_interpolation(pl_interp_instance_t *instance)
{
    return instance->interpolate;
}

/* Set a device into an interperter instance */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_set_device(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gx_device              *device        /* device to set (open or closed) */
)
{
	int code;
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	px_state_t *pxs = pxli->pxs;

	enum {Sbegin, Ssetdevice, Sinitg, Sgsave, Serase, Sdone} stage;
	stage = Sbegin;
	gs_opendevice(device);

        pxs->interpolate = pxl_get_interpolation(instance);
	/* Set the device into the gstate */
	stage = Ssetdevice;
	if ((code = gs_setdevice_no_erase(pxli->pgs, device)) < 0)	/* can't erase yet */
	  goto pisdEnd;
        /* Initialize device ICC profile  */
#ifdef ICCBRANCH
        code = gsicc_init_device_profile(pxli->pgs, device);
        if (code < 0)
            return code;
#endif
	/* Init XL graphics */
	stage = Sinitg;
	if ((code = px_initgraphics(pxli->pxs)) < 0)
	  goto pisdEnd;

	/* Do inits of gstate that may be reset by setdevice */
	gs_setaccuratecurves(pxli->pgs, true);	/* All H-P languages want accurate curves. */
        /* disable hinting at high res */
        if (gs_currentdevice(pxli->pgs)->HWResolution[0] >= 300)
            gs_setgridfittt(pxs->font_dir, 0);


	/* gsave and grestore (among other places) assume that */
	/* there are at least 2 gstates on the graphics stack. */
	/* Ensure that now. */
	stage = Sgsave;
	if ( (code = gs_gsave(pxli->pgs)) < 0)
	  goto pisdEnd;

    /* PXL always sets up a page and erases it so the following is not
       needed in normal operation. */
#ifdef PRELIMINARY_ERASE
	stage = Serase;
	if ( (code = gs_erasepage(pxli->pgs)) < 0 )
		goto pisdEnd;
#endif
	stage = Sdone;	/* success */
	/* Unwind any errors */
pisdEnd:
	switch (stage) {
	case Sdone:	/* don't undo success */
	  break;

	case Serase:	/* gs_erasepage failed */
	  /* undo  gsave */
	  gs_grestore_only(pxli->pgs);	 /* destroys gs_save stack */
	  /* fall thru to next */
	case Sgsave:	/* gsave failed */
	case Sinitg:
	  /* undo setdevice */
	  gs_nulldevice(pxli->pgs);
	  /* fall thru to next */

	case Ssetdevice:	/* gs_setdevice failed */
	case Sbegin:	/* nothing left to undo */
	  break;
	}
	return code;
}

static int   
pxl_impl_get_device_memory(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gs_memory_t **pmem)
{
    return 0;
}

/* Prepare interp instance for the next "job" */
static int	/* ret 0 ok, else -ve error code */
pxl_impl_init_job(
	pl_interp_instance_t   *instance         /* interp instance to start job in */
)
{
	int code = 0;
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	px_reset_errors(pxli->pxs);
	px_process_init(pxli->st, true);

	/* set input status to: expecting stream header */
	px_stream_header_init(&pxli->headerState, pxli->st, pxli->pxs);
	pxli->processState = PSHeader;

	return code;
}

/* Parse a cursor-full of data */
static int	/* ret 0 or +ve if ok, else -ve error code */
pxl_impl_process(
	pl_interp_instance_t *instance,        /* interp instance to process data job in */
	stream_cursor_read   *cursor           /* data to process */
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	int code;

	/* Process some input */
	switch (pxli->processState)
	  {
	case PSDone:
	  return e_ExitLanguage;
	case PSHeader:		/* Input stream header */
	  code = px_stream_header_process(&pxli->headerState, cursor);
	  if (code == 0)
	    break;    /* need more input later */
	  else
	  	 /* stream header termination */
	    if (code < 0)
	      {
	      pxli->processState = PSDone;
	      return code;   /* return error */
	      }
	    else
	      {
	      code = 0;
	      pxli->processState = PSXL;
	      }
	    /* fall thru to PSXL */
	case PSXL:		/* PCL XL */
	  code = px_process(pxli->st, pxli->pxs, cursor);
	  if ( code == e_ExitLanguage )
	    { pxli->processState = PSDone;
	      code = 0;
	    }
	  else if ( code == errorWarningsReported )
	    { /* The parser doesn't skip over the EndSession */
	      /* operator, because an "error" occurred. */
	      cursor->ptr++;
	    }
	  else if (code < 0)
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
	  break;   /* may need more input later */
	  }

	return code;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
static int
pxl_impl_flush_to_eoj(
	pl_interp_instance_t *instance,        /* interp instance to flush for */
	stream_cursor_read   *cursor           /* data to process */
)
{
	const byte *p = cursor->ptr;
	const byte *rlimit = cursor->limit;

	/* Skip to, but leave UEL in buffer for PJL to find later */
	for (; p < rlimit; ++p)
	  if (p[1] == '\033') {
	    uint avail = rlimit - p;

	  if (memcmp(p + 1, "\033%-12345X", min(avail, 9)))
	    continue;
	  if (avail < 9)
	    break;
	  cursor->ptr = p;
	  return 1;  /* found eoj */
	}
	cursor->ptr = p;
	return 0;  /* need more */
}

/* Parser action for end-of-file */
static int	/* ret 0 or +ve if ok, else -ve error code */
pxl_impl_process_eof(
	pl_interp_instance_t *instance        /* interp instance to process data job in */
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;

	px_state_cleanup(pxli->pxs);

	return 0;
}

/* Report any errors after running a job */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_report_errors(
	pl_interp_instance_t *instance,         /* interp instance to wrap up job in */
   int                  code,              /* prev termination status */
   long                 file_position,     /* file position of error, -1 if unknown */
   bool                 force_to_cout      /* force errors to cout */
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	px_parser_state_t *st = pxli->st;
	px_state_t *pxs = pxli->pxs;
	int report = pxs->error_report;
	const char *subsystem =
	  (code <= px_error_next ? "KERNEL" : "GRAPHICS");
	char message[px_max_error_line+1];
	int N = 0;
	int y;

	if (code >= 0)
	  return code;  /* not really an error */
	if ( report & eErrorPage )
	  y = px_begin_error_page(pxs);
	while ( (N = px_error_message_line(message, N, subsystem,
	   code, st, pxs)) >= 0
	)
	  { if ( (report & eBackChannel) || force_to_cout )
	      errprintf(pxli->memory, message);
	    if ( report & eErrorPage )
	      y = px_error_page_show(message, y, pxs);
	  }
	if ( ((report & pxeErrorReport_next) && file_position != -1L) || force_to_cout )
	  errprintf(pxli->memory, "file position of error = %ld\n", file_position);
	if ( report & eErrorPage )
	  { px_args_t args;
	    args.pv[0] = 0;
	    pxEndPage(&args, pxs);
	  }
	px_reset_errors(pxs);

	return code;
}

/* Wrap up interp instance after a "job" */
static int	/* ret 0 ok, else -ve error code */
pxl_impl_dnit_job(
	pl_interp_instance_t *instance         /* interp instance to wrap up job in */
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	px_stream_header_dnit(&pxli->headerState);
	px_state_cleanup(pxli->pxs);
	return 0;
}

/* Remove a device from an interperter instance */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_remove_device(
  pl_interp_instance_t   *instance     /* interp instance to use */
)
{
	int code = 0;	/* first error status encountered */
	int error;
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	/* return to original gstate  */
	gs_grestore_only(pxli->pgs);	/* destroys gs_save stack */
	/* Deselect device */
	/* NB */
	error = gs_nulldevice(pxli->pgs);
	if (code >= 0)
	  code = error;

	return code;
}

/* Deallocate a interpreter instance */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_deallocate_interp_instance(
  pl_interp_instance_t   *instance     /* instance to dealloc */
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)instance;
	gs_memory_t *mem = pxli->memory;
	
	px_dict_release(&pxli->pxs->font_dict);
	px_dict_release(&pxli->pxs->builtin_font_dict);
	/* do total dnit of interp state */
	px_state_finit(pxli->pxs);
	/* free halftone cache */
	gs_state_free(pxli->pgs);
	px_process_release(pxli->st);
	px_state_release(pxli->pxs);
	gs_free_object(mem, pxli, "pxl_impl_deallocate_interp_instance(pxl_interp_instance_t)");
	return 0;
}

/* Do static deinit of PXL interpreter */
static int   /* ret 0 ok, else -ve error code */
pxl_impl_deallocate_interp(
  pl_interp_t        *interp       /* interpreter to deallocate */
)
{
	/* nothing to do */
	return 0;
}

/* 
 * End-of-page called back by PXL
 */
static int
pxl_end_page_top(
    px_state_t *            pxls,
    int                     num_copies,
    int                     flush
)
{
	pxl_interp_instance_t *pxli = (pxl_interp_instance_t *)(pxls->client_data);
	pl_interp_instance_t *instance = (pl_interp_instance_t *)pxli;
	int code = 0;

	/* do pre-page action */
	if (pxli->pre_page_action)
	  {
	  code = pxli->pre_page_action(instance, pxli->pre_page_closure);
	  if (code < 0)
	    return code;
	  if (code != 0)
	    return 0;    /* code > 0 means abort w/no error */
	  }

	/* output the page */
	code = gs_output_page(pxli->pgs, num_copies, flush);
	if (code < 0)
	  return code;

	/* do post-page action */
	if (pxli->post_page_action)
	  {
	  code = pxli->post_page_action(instance, pxli->post_page_closure);
	  if (code < 0)
	    return code;
	  }

	return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t pxl_implementation = {
  pxl_impl_characteristics,
  pxl_impl_allocate_interp,
  pxl_impl_allocate_interp_instance,
  pxl_impl_set_client_instance,
  pxl_impl_set_pre_page_action,
  pxl_impl_set_post_page_action,
  pxl_impl_set_device,
  pxl_impl_init_job,
  NULL, /* process_file */
  pxl_impl_process,
  pxl_impl_flush_to_eoj,
  pxl_impl_process_eof,
  pxl_impl_report_errors,
  pxl_impl_dnit_job,
  pxl_impl_remove_device,
  pxl_impl_deallocate_interp_instance,
  pxl_impl_deallocate_interp,
  pxl_impl_get_device_memory,
};

/* ---------- Utility Procs ----------- */
/* Initialize the parser state, and session parameters in case we get */
/* an error before the session is established. */
static int
px_top_init(px_parser_state_t *st, px_state_t *pxs, bool big_endian)
{
    px_args_t args;
    px_value_t v[3];

    px_process_init(st, big_endian);

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
    {
	int code = pxBeginSession(&args, pxs);
	if ( code < 0 )
	    return code;
    }
    return 0;
}
