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
#include "../gs/base/errors.h"	/* FIXME: MSVC seems to pull in <errors.h> */
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
#include "uconfig.h"	/* for UFSTFONTDIR */

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

/* Import operator procedures */
extern int zflush(i_ctx_t *);

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
    "POSTSCRIPT",
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
static int   /* ret 0 ok, else -ve error code */
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

/* defaults for locations of font collection objects (fco's) and
   plugins the root data directory.  These are internally separated with
   ':' but environment variable use the gp separator */
#ifndef UFSTFONTDIR
	/* not using UFST */
#  define UFSTFONTDIR ""
#endif

/* Do per-instance interpreter allocation/init. No device is set yet */
static int   /* ret 0 ok, else -ve error code */
ps_impl_allocate_interp_instance(
  pl_interp_instance_t   **instance,     /* RETURNS instance struct */
  pl_interp_t            *interp,        /* dummy interpreter */
  gs_memory_t            *mem            /* allocator to allocate instance from */
)
{
#ifdef DEBUG_WITH_EXPERIMENTAL_GSOPTIONS_FILE
#   define MAX_ARGS 40
#else
#   define MAX_ARGS /* unspecified */
#endif
	int code = 0, exit_code;
	const char *argv[MAX_ARGS] = { 
	    "",
	    "-dNOPAUSE",
#ifndef DEBUG
	    "-dQUIET",
#else
	    "-dOSTACKPRINT", // NB: debuggging postscript Needs to be removed. 
	    "-dESTACKPRINT", // NB: debuggging postscript Needs to be removed. 
#endif
#if UFST_BRIDGE==1
	    "-dJOBSERVER", 
	    "-sUFST_PlugIn=" UFSTFONTDIR "mtfonts/pcl45/mt3/plug__xi.fco",
            "-sFCOfontfile=" UFSTFONTDIR "mtfonts/pclps2/mt3/pclp2_xj.fco",
            "-sFCOfontfile2=" UFSTFONTDIR "mtfonts/pcl45/mt3/wd____xh.fco",
	    "-sFAPIfontmap=FCOfontmap-PCLPS2",
	    "-sFAPIconfig=FAPIconfig-FCO",
#endif
	    0
	};
#ifndef DEBUG
	int argc = 9;
#else
	int argc = 10;
#endif
#ifdef DEBUG_WITH_EXPERIMENTAL_GSOPTIONS_FILE
	char argbuf[1024];
#endif
#   undef MAX_ARGS
	ps_interp_instance_t *psi  /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
	    = (ps_interp_instance_t *)
	    gs_alloc_bytes( mem,
			    sizeof(ps_interp_instance_t),
			    "ps_allocate_interp_instance(ps_interp_instance_t)"	
			    );

#if UFST_BRIDGE!=1
    argc -= 6;
#endif


	/* If allocation error, deallocate & return */
	if (!psi) {
	  return gs_error_VMerror;
	}
	/* Initialize for pl_main_universe_dnit/pl_deallocate_interp_instance
	   in case of gs_main_init_with_args returns with error code. */
	psi->pl.interp = interp;
	/* Setup pointer to mem used by PostScript */
	psi->plmemory = mem;
	psi->minst = gs_main_alloc_instance(mem->non_gc_memory);

#ifdef DEBUG_WITH_EXPERIMENTAL_GSOPTIONS_FILE
	{   /* Fetch more GS arguments (debug purposes only).
	       Pulling debugging arguments from a file allows easy additions
	       of postscript arguments to a debug system, it is not recommended for 
	       production systems since some options will conflict with commandline 
	       arguments in unpleasant ways.  
	    */
	    FILE *f = fopen("gsoptions", "rb"); /* Sorry we handle 
					          the current directory only.
						  Assuming it always fails with no crash
						  in a real embedded system. */

	    if (f != NULL) {
		int i;
		int l = fread(argbuf, 1, sizeof(argbuf) - 1, f);

		if (l >= sizeof(argbuf) - 1)
		    errprintf("The gsoptions file is too big. Truncated to the buffer length %d.\n", l - 1);
		if (l > 0) {
		    argbuf[l] = 0;
		    if (argbuf[0] && argbuf[0] != '\r' && argbuf[0] != '\n') /* Silently skip empty lines. */
			argv[argc++] = argbuf;
		    for (i = 0; i < l; i++)
			if (argbuf[i] == '\r' || argbuf[i] == '\n') {
			    argbuf[i] = 0;
			    if (argbuf[i + 1] == 0 || argbuf[i + 1] == '\r' || argbuf[i + 1] == '\n')
				continue; /* Silently skip empty lines. */
			    if (argc >= count_of(argv)) {
				errprintf("The gsoptions file contains too many options. "
					  "Truncated to the buffer length %d.\n", argc);
				break;
			    }
			    argv[argc++] = argbuf + i + 1;
			}
		}
		fclose(f);
	    }
	}
#endif

	*instance = (pl_interp_instance_t *)psi;
	code = gs_main_init_with_args(psi->minst, argc, (char**)argv);
	if (code<0)
	    return code;

	/* General init of PS interp instance */
	
	if ((code = gs_main_run_string_begin(psi->minst, 0, &exit_code, &psi->minst->error_object)) < 0)
	    return exit_code;

        /* inialize fresh job to false so that we can check for a pdf
           file next job. */
        psi->fresh_job = true;
        /* default is a postscript stream */
        psi->pdf_stream = false;

	/* Return success */
	return 0;
}

/* NB this pointer should be placed in the ps instance */

/* Set a client language into an interpreter instance */
static int   /* ret 0 ok, else -ve error code */
ps_impl_set_client_instance(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  pl_interp_instance_t   *client,       /* client to set */
  pl_interp_instance_clients_t which_client
)
{
	return 0;
}

/* Set an interpreter instance's pre-page action */
static int   /* ret 0 ok, else -ve err */
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
static int   /* ret 0 ok, else -ve err */
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

/* Set a device into an interpreter instance */
static int   /* ret 0 ok, else -ve error code */
ps_impl_set_device(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gx_device              *device        /* device to set (open or closed) */
)
{
    int code = 0;
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    gs_state *pgs = psi->minst->i_ctx_p->pgs;

    /* Set the device into the gstate */
    code = gs_setdevice_no_erase(pgs, device);
    if (code >= 0 )
	code = gs_erasepage(pgs);
    return code;
}

/* fetch the gs_memory_t ptr so that the device and ps use the same 
 * garbage collection aware a memory
 */
static int   
ps_impl_get_device_memory(
  pl_interp_instance_t   *instance,     /* interp instance to use */
  gs_memory_t **pmem)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    gs_dual_memory_t *dmem = &psi->minst->i_ctx_p->memory;
    gs_ref_memory_t *mem = dmem->spaces.memories.named.global;

    *pmem = mem->stable_memory;
    /* Lock against alloc_restore_all to release the device when called from gsapi_exit : */
    mem->num_contexts++;
    return 0;
}

gs_main_instance *ps_impl_get_minst( const gs_memory_t *mem )
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)get_interpreter_from_memory(mem);
    return psi->minst;
}
  
/* Prepare interp instance for the next "job" */
static int	/* ret 0 ok, else -ve error code */
ps_impl_init_job(
	pl_interp_instance_t   *instance         /* interp instance to start job in */
)
{
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    static const char *buf = "\004";  /* use ^D to start a new encapsulated job */
    int exit_code;

    /* starting a new job */
    psi->fresh_job = true;
    gsapi_run_string_continue(psi->plmemory->gs_lib_ctx, buf, strlen(buf), 0, &exit_code); /* ^D */
    return 0;
}

/* Parse a buffer full of data */
static int	/* ret 0 or +ve if ok, else -ve error code */
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
        const char pdf_idstr[] = "%PDF-1.";
        /* do we have enough data? */
        const uint pdf_idstr_len = strlen(pdf_idstr);
        if ( avail < pdf_idstr_len )
            /* more data.  NB update ptr ?? */
            return 0;
        else
            /* compare beginning of stream with pdf id */
            if ( !strncmp(pdf_idstr, (const char *)cursor->ptr + 1, pdf_idstr_len) ) {
                char fmode[4];
                /* open the temporary pdf file.  If the file open
                   fails PDF fails and we allow the job to be sent
                   to postscript and generate an error.  It turns
                   out this is easier than restoring the state and
                   returning */
                strcpy(fmode, "w+");
                strcat(fmode, gp_fmode_binary_suffix);
                psi->pdf_filep = gp_open_scratch_file(psi->plmemory,
                                                      gp_scratch_file_name_prefix,
                                                      psi->pdf_file_name,
                                                      fmode);
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
        code = gsapi_run_string_continue(psi->plmemory->gs_lib_ctx, (const char *)(cursor->ptr + 1),
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
    /* flush stdout on error. */
    if (code < 0)
	zflush(psi->minst->i_ctx_p);
    /* return the exit code */
    return code;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
static int
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
static int	/* ret 0 or +ve if ok, else -ve error code */
ps_impl_process_eof(
	pl_interp_instance_t *instance        /* interp instance to process data job in */
)
{
    int code = 0;

    return code;
}

/* Report any errors after running a job */
static int   /* ret 0 ok, else -ve error code */
ps_impl_report_errors(pl_interp_instance_t *instance,      /* interp instance to wrap up job in */
		      int                  code,           /* prev termination status */
		      long                 file_position,  /* file position of error, -1 if unknown */
		      bool                 force_to_cout    /* force errors to cout */
)
{
    /*    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    */
    return code;
}

/* Wrap up interp instance after a "job" */
static int	/* ret 0 ok, else -ve error code */
ps_impl_dnit_job(
	pl_interp_instance_t *instance         /* interp instance to wrap up job in */
)
{
    int code = 0; 
    int exit_code = 0;
    static const char *buf = "\n.endjob\n";  /* restore to initial state, non-encapsualted */
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;

    /* take care of a stored pdf file */
    if ( psi->pdf_stream ) {

	/* 
         * Hex encode to avoid problems with window's directory
	 * separators '\' being interpreted in postscript as escape
	 * sequences.  The buffer length is the maximum file name size
	 * (gp_file_name_sizeof) * 2 (hex encoding) + 2 (hex
	 * delimiters '<' and '>') + the rest of the run command 7
	 * (space + (run) + new line + null).
	 */
        char buf[gp_file_name_sizeof * 2 + 2 + 7];
        const char *run_str = " run\n";
        const char *hex_digits = "0123456789ABCDEF";
        char *pd = buf;                       /* destination */
        const char *ps = psi->pdf_file_name;  /* source */
        
        *pd = '<'; pd++;
        while (*ps) {
            *pd = hex_digits[*ps >> 4 ]; pd++;
            *pd = hex_digits[*ps & 0xf]; pd++;
            ps++;
        }
        *pd = '>'; pd++;
        strcpy(pd, run_str);

        /* at this point we have finished writing the spooled pdf file
           and we need to close it */
        fclose(psi->pdf_filep);

        /* Send the buffer to Ghostscript */
        code = gsapi_run_string_continue(psi->plmemory->gs_lib_ctx, buf, strlen(buf), 0, &exit_code);

        /* indicate we are done with the pdf stream */
        psi->pdf_stream = false;
        unlink(psi->pdf_file_name);
        /* handle errors... normally job deinit failures are
           considered fatal but pdf runs the spooled job when the job
           is deinitialized so handle error processing here and return code is always 0. */
        if (( code < 0) && (code != e_NeedInput)) {
            errprintf(psi->plmemory, "PDF interpreter exited with exit code %d\n", exit_code);
            errprintf(psi->plmemory, "Flushing to EOJ\n");
        }
        code = 0;
    }

    /* We use string_end to send an EOF in case the job was reading in a loop */
    gsapi_run_string_end(psi->plmemory->gs_lib_ctx, 0, &exit_code);	/* sends EOF to PS process */
    gsapi_run_string_begin(psi->plmemory->gs_lib_ctx, 0, &exit_code);	/* prepare to send .endjob */
    gsapi_run_string_continue(psi->plmemory->gs_lib_ctx, buf, strlen(buf), 0, &exit_code); /* .endjob */
    /* Note the above will restore to the server save level and will not be encapsulated */
    
    /* Flush stdout. */
    zflush(psi->minst->i_ctx_p);

    return 0;
}

/* Remove a device from an interpreter instance */
static int   /* ret 0 ok, else -ve error code */
ps_impl_remove_device(
  pl_interp_instance_t   *instance     /* interp instance to use */
)
{
    /* Assuming the interpreter's stack contains a single graphic state.
       Otherwise this procedure is not effective.
       The Postscript job server logic must provide that.
     */
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    gs_state *pgs = psi->minst->i_ctx_p->pgs;
    int code = gs_nulldevice(pgs);

    if ( code < 0 )
	dprintf1("error code %d installing nulldevice, continuing\n", code );
    return 0;
}

/* Deallocate a interpreter instance */
static int   /* ret 0 ok, else -ve error code */
ps_impl_deallocate_interp_instance(
  pl_interp_instance_t   *instance     /* instance to dealloc */
)
{
	int code = 0, exit_code;
	ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
	gs_memory_t *mem = psi->plmemory;
	
	/* do total dnit of interp state */
	code = gsapi_run_string_end(mem->gs_lib_ctx, 0, &exit_code);

	gsapi_exit(psi->minst);

	gs_free_object(mem, psi, "ps_impl_deallocate_interp_instance(ps_interp_instance_t)");

	return (code < 0) ? exit_code : 0;
}

/* Do static deinit of PS interpreter */
static int   /* ret 0 ok, else -ve error code */
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
ps_end_page_top(const gs_memory_t *mem, int num_copies, bool flush)
{
    pl_interp_instance_t *instance = get_interpreter_from_memory(mem);
    ps_interp_instance_t *psi = (ps_interp_instance_t *)instance;
    int code = 0;

    if (psi == 0) 
	return 0;

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

    /* Flush stdout. */
    zflush(psi->minst->i_ctx_p);

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
  NULL, /* process_file */
  ps_impl_process,
  ps_impl_flush_to_eoj,
  ps_impl_process_eof,
  ps_impl_report_errors,
  ps_impl_dnit_job,
  ps_impl_remove_device,
  ps_impl_deallocate_interp_instance,
  ps_impl_deallocate_interp,
  ps_impl_get_device_memory,
};

