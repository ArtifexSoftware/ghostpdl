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
/* $Id$ */

/* psitop.c */
/* Top-level API implementation of PS Language Interface */

#include "stdio_.h"
#include "ghost.h"
#include "imain.h"
#include "imainarg.h"
#include "iapi.h"
#include "string_.h"
#include "gdebug.h"
#include "gp.h"
#include "gserrors.h"
#include "../gs/src/errors.h"	/* FIXME: Microsoft seems to pull in <errors.h> */
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsstate.h"		/* must precede gsdevice.h */
#include "gxdevice.h"		/* must precede gsdevice.h */
#include "gsdevice.h"
#include "icstate.h"		/* for i_ctx_t */
#include "iminst.h"
#include "gsstruct.h"		/* for gxalloc.h */
#include "gspaint.h"
#include "gxalloc.h"
#include "gxstate.h"
#include "plparse.h"
#include "pltop.h"
#include "gzstate.h"
/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

/*
 * PS interpreter instance: derived from pl_interp_instance_t
 */
typedef struct ps_interp_instance_s {
    pl_interp_instance_t     pl;                 /* common part: must be first */
    gs_memory_t              *plmemory;          /* memory allocator to use with pl objects */
    gs_main_instance	     *minst;	         /* PS interp main instance */
    pl_page_action_t         pre_page_action;    /* action before page out */
    void                     *pre_page_closure;  /* closure to call pre_page_action with */
    pl_page_action_t         post_page_action;   /* action before page out */
    void                     *post_page_closure; /* closure to call post_page_action with */
    bool                     fresh_job;          /* true if we are starting a new job */
    bool                     pdf_stream;         /* current stream is pdf */
    char                     pdf_file_name[gp_file_name_sizeof];
    FILE *                   pdf_filep;          /* temporary file for writing out pdf file */
    ref                      job_save;
} ps_interp_instance_t;

/* Get implemtation's characteristics */
const pl_interp_characteristics_t * /* always returns a descriptor */
ps_impl_characteristics(
  const pl_interp_implementation_t *impl     /* implementation of interpereter to alloc */
)
{
    /* version and build date are not currently used */
#define PSVERSION NULL
#define PSBUILDDATE NULL
  static const pl_interp_characteristics_t ps_characteristics = {
    "PS",
    /* NOTE - we don't look for %! because we want to recognize pdf as well */
    "%",
    "Artifex",
    PSVERSION,
    PSBUILDDATE,
    1				/* minimum input size to PostScript */
  };
#undef PSVERSION
#undef PSBUILDDATE

  return &ps_characteristics;
}


/* Don't need to do anything to PS interpreter */
private int   /* ret 0 ok, else -ve error code */
ps_impl_allocate_interp(
  pl_interp_t                      **interp,       /* RETURNS abstract interpreter struct */
  const pl_interp_implementation_t *impl,  /* implementation of interpereter to alloc */
  gs_memory_t                      *mem            /* allocator to allocate interp from */
)
{

    static pl_interp_t pi;	/* there's only one interpreter */
    /* There's only one PS interp, so return the static */
    *interp = &pi;
    return 0;   /* success */
}

/* NB FIX ME fold into the instance like other languages. */
ps_interp_instance_t *global_psi = NULL;

/* Do per-instance interpreter allocation/init. No device is set yet */
private int   /* ret 0 ok, else -ve error code */
ps_impl_allocate_interp_instance(
  pl_interp_instance_t   **instance,     /* RETURNS instance struct */
  pl_interp_t            *interp,        /* dummy interpreter */
  gs_memory_t            *mem            /* allocator to allocate instance from */
)
{
	int code = 0, exit_code;
	int argc = 3;
	char *argv[] = { {""}, {"-dNOPAUSE"}, {"-dQUIET"}, {""}};

	ps_interp_instance_t *psi  /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
	    = (ps_interp_instance_t *)gs_alloc_bytes( mem,
						      sizeof(ps_interp_instance_t),
						      "ps_allocate_interp_instance(ps_interp_instance_t)"
						      );
	/* If allocation error, deallocate & return */
	if (!psi) {
	  return gs_error_VMerror;
	}
        global_psi = psi;
	/* Setup pointer to mem used by PostScript */
	psi->plmemory = mem;
	psi->minst = gs_main_instance_default();
	code = gs_main_init_with_args(psi->minst, argc, argv);
	if (code<0)
	    return code;

	/* General init of PS interp instance */
	
	code = gsapi_run_string_begin(psi->minst, 0, &exit_code);

	/* Because PS uses its own 'heap' allocator, we restore to the pl 
	 * allocator before returning (PostScript will use minst->heap)	  
	 */
	if (code<0)
	    return exit_code;

	/* Return success */
	*instance = (pl_interp_instance_t *)psi;
        /* inialize fresh job to false so that we can check for a pdf
           file next job. */
        psi->fresh_job = true;
        /* default is a postscript stream */
        psi->pdf_stream = false;
	return 0;
}

/* NB this pointer should be placed in the ps instance */

/* Set a client language into an interperter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_set_client_instance(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_interp_instance_t   *client        /* client to set */
)
{
	return 0;
}

/* Set an interpreter instance's pre-page action */
private int   /* ret 0 ok, else -ve err */
ps_impl_set_pre_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute (rets 1 to abort w/o err) */
  void                   *closure       /* closure to call action with */
)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    psi->pre_page_action = action;
    psi->pre_page_closure = closure;
    return 0;
}

/* Set an interpreter instance's post-page action */
private int   /* ret 0 ok, else -ve err */
ps_impl_set_post_page_action(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_page_action_t       action,        /* action to execute */
  void                   *closure       /* closure to call action with */
)
{
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
	psi->post_page_action = action;
	psi->post_page_closure = closure;
	return 0;
}

/* Set a device into an interperter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_set_device(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gx_device              *device        /* device to set (open or closed) */
)
{
    int code;
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    gs_state *pgs = psi->minst->i_ctx_p->pgs;
    gs_opendevice(device);
    /* Set the device into the gstate */
    code = gs_setdevice_no_erase(pgs, device);
    return code;
}

/* Prepare interp instance for the next "job" */
private int	/* ret 0 ok, else -ve error code */
ps_impl_init_job(
	pl_interp_instance_t   *instance         /* interp instance to start job in */
)
{
    int code = 0; 
    int exit_code = 0;
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

    /* starting a new job */
    psi->fresh_job = true;
    return 0; /* should be return zsave(psi->minst->i_ctx_p); */
}

/* Parse a buffer full of data */
private int	/* ret 0 or +ve if ok, else -ve error code */
ps_impl_process(
	pl_interp_instance_t *instance,        /* interp instance to process data job in */
	stream_cursor_read   *cursor           /* data to process */
)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    int code, exit_code;
    uint avail = cursor->limit - cursor->ptr;
    /* if we are at the beginning of a job check for pdf and set
       appropriate state variables to process either a pdf or ps
       job */
    if ( psi->fresh_job ) {
        const char pdf_idstr[] = "%PDF-1.2";
        /* do we have enough data? */
        const uint pdf_idstr_len = strlen(pdf_idstr);
        if ( avail < pdf_idstr_len )
            /* more data.  NB update ptr ?? */
            return 0;
        else
            /* compare beginning of stream with pdf id */
            if ( !strncmp(pdf_idstr, cursor->ptr + 1, pdf_idstr_len) ) {
                char fmode[4];
                /* open the temporary pdf file.  If the file open
                   fails PDF fails and we allow the job to be sent
                   to postscript and generate an error.  It turns
                   out this is easier than restoring the state and
                   returning */
                strcpy(fmode, "w+");
                strcat(fmode, gp_fmode_binary_suffix);
                psi->pdf_filep = gp_open_scratch_file(gp_scratch_file_name_prefix,
                                                      psi->pdf_file_name, fmode);
                if ( psi->pdf_filep == NULL )
                    psi->pdf_stream = false;
                else
                    psi->pdf_stream = true;
            }
            else
                psi->pdf_stream = false;
        /* we only check for pdf at the beginning of the job */
        psi->fresh_job = false;
    }
            
    /* for a pdf stream we append to the open pdf file but for
       postscript we hand it directly to the ps interpreter.  PDF
       files are processed subsequently, at end job time */
    code = 0;
    if ( psi->pdf_stream ) {
        uint bytes_written = fwrite((cursor->ptr + 1), 1, avail, psi->pdf_filep);
        if ( bytes_written != avail )
            code = gs_error_invalidfileaccess;
    } else {
        /* Send the buffer to Ghostscript */
        code = gsapi_run_string_continue(psi->minst, (const char *)(cursor->ptr + 1),
                                         avail, 0, &exit_code);
        /* needs more input this is not an error */
        if ( code == e_NeedInput )
            code = 0;
        /* error - I guess it gets "exit code" - nonsense */
        if ( code < 0 )
            code = exit_code;
    }
    /* update the cursor */
    cursor->ptr += avail;
    /* return the exit code */
    return code;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
private int
ps_impl_flush_to_eoj(
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
private int	/* ret 0 or +ve if ok, else -ve error code */
ps_impl_process_eof(
	pl_interp_instance_t *instance        /* interp instance to process data job in */
)
{
    int code = 0, exit_code;
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    /* put endjob logic here (finish encapsulation) */
    return code;
}

/* Report any errors after running a job */
private int   /* ret 0 ok, else -ve error code */
ps_impl_report_errors(
	pl_interp_instance_t *instance,         /* interp instance to wrap up job in */
   int                  code,              /* prev termination status */
   long                 file_position,     /* file position of error, -1 if unknown */
   bool                 force_to_cout,     /* force errors to cout */
   FILE                 *cout              /* stream for back-channel reports */
)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    return code;
}

/* Wrap up interp instance after a "job" */
private int	/* ret 0 ok, else -ve error code */
ps_impl_dnit_job(
	pl_interp_instance_t *instance         /* interp instance to wrap up job in */
)
{
    int code = 0; 
    int exit_code = 0;
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    /* take care of a stored pdf file */
    if ( psi->pdf_stream ) {
        uint max_command_length = gp_file_name_sizeof +
            7; /* 6 = space + (run) + new line */
        byte buf[max_command_length];
        /* at this point we have finished writing the spooled pdf file
           and we need to close it */
        fclose(psi->pdf_filep);
        /* run the temporary pdf spool file */
        sprintf(buf, "(%s) run\n", psi->pdf_file_name);
        /* Send the buffer to Ghostscript */
        code = gsapi_run_string_continue(psi->minst, buf, strlen(buf), 0, &exit_code);

        /* indicate we are done with the pdf stream */
        psi->pdf_stream = false;
        unlink(psi->pdf_file_name);
        /* handle errors... normally job deinit failures are
           considered fatal but pdf runs the spooled job when the job
           is deinitialized so handle error processing here and return code is always 0. */
        if ( code < 0 ) {
            fprintf(gs_stderr, "PDF interpreter exited with exit code %d\n", exit_code);
            fprintf(gs_stderr, "Flushing to EOJ\n", exit_code);
        }
        code = 0;
    }
    return 0; /* should be return zrestore(psi->minst->i_ctx_p); */
}

/* Remove a device from an interperter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_remove_device(
  pl_interp_instance_t   *instance     /* interp instance to use */
)
{
	int code = 0;	/* first error status encountered */
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

	return code;
}

/* Deallocate a interpreter instance */
private int   /* ret 0 ok, else -ve error code */
ps_impl_deallocate_interp_instance(
  pl_interp_instance_t   *instance     /* instance to dealloc */
)
{
	int code = 0, exit_code;
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
	gs_memory_t *mem = psi->plmemory;
	
	/* do total dnit of interp state */
	code = gsapi_run_string_end(psi->minst, 0, &exit_code);

	gs_free_object(mem, psi, "ps_impl_deallocate_interp_instance(ps_interp_instance_t)");

	return (code < 0) ? exit_code : 0;
}

/* Do static deinit of PS interpreter */
private int   /* ret 0 ok, else -ve error code */
ps_impl_deallocate_interp(
  pl_interp_t        *interp       /* interpreter to deallocate */
)
{
	/* nothing to do */
	return 0;
}

/* 
 * End-of-page called back by PS
 */
int
ps_end_page_top(int num_copies, bool flush)
{
    /* NB access instance through the global */
    ps_interp_instance_t *psi = global_psi;
    pl_interp_instance_t *instance = (pl_interp_instance_t *)psi;
    int code = 0;

    /* do pre-page action */
    if (psi->pre_page_action) {
        code = psi->pre_page_action(instance, psi->pre_page_closure);
        if (code < 0)
	    return code;
        if (code != 0)
	    return 0;    /* code > 0 means abort w/no error */
    }

    /* output the page */
    code = gs_output_page(psi->minst->i_ctx_p->pgs, num_copies, flush);
    if (code < 0)
        return code;

    /* do post-page action */
    if (psi->post_page_action) {
        code = psi->post_page_action(instance, psi->post_page_closure);
        if (code < 0)
	    return code;
    }

    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t ps_implementation = {
  ps_impl_characteristics,
  ps_impl_allocate_interp,
  ps_impl_allocate_interp_instance,
  ps_impl_set_client_instance,
  ps_impl_set_pre_page_action,
  ps_impl_set_post_page_action,
  ps_impl_set_device,
  ps_impl_init_job,
  ps_impl_process,
  ps_impl_flush_to_eoj,
  ps_impl_process_eof,
  ps_impl_report_errors,
  ps_impl_dnit_job,
  ps_impl_remove_device,
  ps_impl_deallocate_interp_instance,
  ps_impl_deallocate_interp,
};
