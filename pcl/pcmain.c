/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcmain.c - PCL5c main program */

/* HACK - avoid gsio redefinitions */
#define gsio_INCLUDED

#include "malloc_.h"
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"
#include "scommon.h"			/* for pparse.h */
#include "pcparse.h"
#include "pcstate.h"
#include "pcpage.h"
#include "pcident.h"
#include "gdebug.h"
#include "gp.h"
#include "gscdefs.h"
#include "gsgc.h"
#include "gslib.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsmatrix.h"		/* for gsstate.h */
#include "gspaint.h"
#include "gsparam.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gsrop.h"
#include "gspath.h"
#include "gxalloc.h"
#include "gxdevice.h"
#include "gxstate.h"
#include "gdevbbox.h"
#include "pjparse.h"
#include "pgmand.h"
#include "plmain.h"

/*#define BUFFER_SIZE 1024*/	/* minimum is 17 */
#define BUFFER_SIZE 17

#ifdef DEBUG
extern  FILE *  gs_debug_out;
#endif

/* Define the table of pointers to initialization data. */
#define init_(init) extern const pcl_init_t init;
#include "pconfig.h"
#undef init_

const pcl_init_t *    pcl_init_table[] = {
#define init_(init) &init,
#include "pconfig.h"
#undef init_
    0
};

/* Interim font initialization procedure */
extern  bool    pcl_load_built_in_fonts( pcl_state_t * );

/* Built-in symbol set initialization procedure */
extern  bool    pcl_load_built_in_symbol_sets( pcl_state_t * );

/* 
 * If inst.pause is set, pause at the end of each page.
 */
  private int
pause_end_page(
    pcl_state_t *           pcls,
    int                     num_copies,
    int                     flush
)
{
    pl_main_instance_t *    pmi = pcls->client_data;

    return pl_finish_page(pmi, pcls->pgs, num_copies, flush);
}

/*
 * Define the gstate client procedures.
 */
  private void *
pcl_gstate_client_alloc(
    gs_memory_t *   mem
)
{
    /*
     * We don't want to allocate anything here, but we don't have any way
     * to say we want to share the client data. Since this will only ever
     * be called once, return something random.
     */
    return (void *)1;
}

  private int
pcl_gstate_client_copy_for(
    void *                  to,
    void *                  from,
    gs_state_copy_reason_t  reason
)
{
    return 0;
}

  private void
pcl_gstate_client_free(
    void *          old,
    gs_memory_t *   mem
)
{}

private const gs_state_client_procs pcl_gstate_procs = {
    pcl_gstate_client_alloc,
    0,				/* copy -- superseded by copy_for */
    pcl_gstate_client_free,
    pcl_gstate_client_copy_for
};

/* 
 * Here is the real main program.
 */
  int
main(
    int                 argc,
    char *              argv[]
)
{
    gs_ref_memory_t *   imem;
#define mem ((gs_memory_t *)imem)
    pl_main_instance_t  inst;
    gs_state *          pgs;
    pcl_state_t *       pcls;
    pcl_parser_state_t  pstate;
    arg_list            args;
    const char *        arg;

    /* Initialize the library. */
    gp_init();
    gs_lib_init(stdout);
#ifdef DEBUG
    gs_debug_out = stdout;
#endif

    imem = ialloc_alloc_state((gs_raw_memory_t *)&gs_memory_default, 20000);
    imem->space = 0;		/****** WRONG ******/
    pl_main_init(&inst, mem);
    pl_main_process_options(&inst, &args, argv, argc);

    /* call once to set up the free list handlers */
    gs_reclaim(&inst.spaces, true);

    /* Insert a bounding box device so we can detect empty pages. */
    {
        gx_device_bbox *    bdev = gs_alloc_struct_immovable(
                                                    mem,
                                                    gx_device_bbox,
                                                    &st_device_bbox,
    			                            "main(bbox device)"
                                                              );

        gx_device_fill_in_procs(inst.device);
        gx_device_bbox_init(bdev, inst.device);
        inst.device = (gx_device *)bdev;
    }

    pl_main_make_gstate(&inst, &pgs);

    /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
    pcls = (pcl_state_t *)gs_alloc_bytes( mem,
                                          sizeof(pcl_state_t),
    				          "main(pcl_state_t)"
                                          );
    pcl_init_state(pcls, mem);
    gs_state_set_client(pgs, pcls, &pcl_gstate_procs);
    gs_clippath(pgs);

    /* PCL no longer uses the graphic library transparency mechanism */
    gs_setsourcetransparent(pgs, false);
    gs_settexturetransparent(pgs, false);

    gs_gsave(pgs);

    /* We want all gstates to share the same "client data".... */
    gs_state_set_client(pgs, pcls, &pcl_gstate_procs);
    gs_erasepage(pgs);
    pcls->client_data = &inst;
    pcls->pgs = pgs;

    /* assume no appletalk configuration routine */
    pcls->configure_appletalk = 0;

    /* initialize pjl */
    pcls->pjls = pjl_process_init(mem);

    /* Run initialization code. */
    {
        const pcl_init_t ** init;

        hpgl_init_command_index();
        for (init = pcl_init_table; *init; ++init) {
            if ( (*init)->do_init ) {
                int     code = (*(*init)->do_init)(mem);

                if (code < 0) {
    	            lprintf1("Error %d during initialization!\n", code);
    	            exit(code);
    	        }
            }
        }
        pcl_do_resets(pcls, pcl_reset_initial);
    }

    pcl_set_end_page(pause_end_page);

    /*
     * Intermediate initialization: after state is initialized, may
     * allocate memory, but we won't re-run this level of init.
     */
    if (!pcl_load_built_in_fonts(pcls)) {
        lprintf("No built-in fonts found during initialization\n");
        exit(1);
    }
    pcl_load_built_in_symbol_sets(pcls);

#ifdef DEBUG
    if (gs_debug_c(':'))
        pl_print_usage(mem, &inst, "Start");
#endif

    /* provide a graphic state we can return to */
    pcl_gsave(pcls);

    /* call once more to get rid of any temporary objects */
    gs_reclaim(&inst.spaces, true);

    while ((arg = arg_next(&args)) != 0) {
        /* Process one input file. */
        FILE *              in = fopen(arg, "rb");
        byte                buf[BUFFER_SIZE];

#define buf_last    (buf + (BUFFER_SIZE - 1))

        int                 len;
        int                 code = 0;
        stream_cursor_read  r;
        bool                in_pjl = true;

#ifdef DEBUG
        if (gs_debug_c(':'))
            dprintf1("%% Reading %s:\n", arg);
#endif

        if (in == 0) {
            fprintf(stderr, "Unable to open %s for reading.\n", arg);
            exit(1);
        }

        r.limit = buf - 1;
        for (;;) {
            if_debug1('i', "[i][file pos=%ld]\n", (long)ftell(in));
            r.ptr = buf - 1;
            len = fread((void *)(r.limit + 1), 1, buf_last - r.limit, in);
            if (len <= 0)
    	        break;
            r.limit += len;
process:
            if (in_pjl) {
                code = pjl_process(pcls->pjls, NULL, &r);
    	        if (code < 0)
    	            break;
    	        else if (code > 0) {
    	            in_pjl = false;
    	            pcl_process_init(&pstate);
    	            goto process;
    	        }
    	    } else {
                code = pcl_process(&pstate, pcls, &r);
    	        if (code == e_ExitLanguage)
    	            in_pjl = true;
    	        else if (code < 0)
    	            break;
    	    }

            /* Move unread data to the start of the buffer. */
            len = r.limit - r.ptr;
            if (len > 0)
    	        memmove(buf, r.ptr + 1, len);
            r.limit = buf + (len - 1);
        }
#if 0
#ifdef DEBUG
        if (gs_debug_c(':'))
            dprintf3( "Final file position = %ld, exit code = %d, mode = %s\n",
    	              (long)ftell(in) - (r.limit - r.ptr),
                      code,
    	              (in_pjl ? "PJL" : pcls->parse_other ? "HP-GL/2" : "PCL")
                      );
#endif
#endif
        fclose(in);

        /* Read out any status responses. */
        {
            byte    buf[200];
            uint    count;

            while ( (count = pcl_status_read(buf, sizeof(buf), pcls)) != 0 )
                fwrite(buf, 1, count, stdout);
            fflush(stdout);
        }

        gs_reclaim(&inst.spaces, true);
    }

    /* to help with memory leak detection, issue a reset */
    pcl_do_resets(pcls, pcl_reset_printer);

    /* return to the original graphic state, reclaim memory once more */
    pcl_grestore(pcls);
    gs_reclaim(&inst.spaces, true);
    pjl_process_destroy(pcls->pjls, mem);

#ifdef DEBUG
    if ( gs_debug_c(':') ) {
        typedef struct dump_control_s   dump_control_t;
        extern  const dump_control_t    dump_control_default;

        extern  void    debug_dump_memory( gs_ref_memory_t *,
                                           const dump_control_t *
                                           );

        pl_print_usage(mem, &inst, "Final");
        dprintf1("%% Max allocated = %ld\n", gs_malloc_max);
        debug_dump_memory(imem, &dump_control_default);
    }
#endif

    gs_lib_finit(0, 0);
    exit(0);
    /* NOTREACHED */
}
