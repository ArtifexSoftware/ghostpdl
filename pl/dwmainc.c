/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id: dwmain.c 11973 2010-12-22 19:16:02Z robin $ */

/* dwmain.c */
/* Windows version of the main program command-line interpreter for PCL interpreters 
 */
#include "string_.h"
#include "gdebug.h"
#include "gscdefs.h"
#include "gsio.h"
#include "gstypes.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "plalloc.h"
#include "gsmalloc.h"
#include "gsmchunk.h"
#include "gsstruct.h"
#include "gxalloc.h"
#include "gsalloc.h"
#include "gsargs.h"
#include "gp.h"
#include "gsdevice.h"
#include "gxdevice.h"
#include "gsparam.h"
#include "gslib.h"
#include "pjtop.h"
#include "plparse.h"
#include "plplatf.h"
#include "plmain.h"
#include "pltop.h"
#include "pltoputl.h"
#include "plapi.h"
#include "gslibctx.h"
#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
#include "dwtrace.h"
#include "vdtrace.h"
#endif

/* includes for Windows and the display procedures */
#include <windows.h>
#include "dwimg.h"
#include "dwres.h"

/* includes for the display device */
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gdevdsp.h"
#include "gdevdsp2.h"

/* Define the usage message. */
/* Copied from plmain.c */
static const char *pl_usage = "\
Usage: %s [option* file]+...\n\
Options: -dNOPAUSE -E[#] -h -L<PCL|PCLXL> -K<maxK> -l<PCL5C|PCL5E|RTL> -Z...\n\
         -sDEVICE=<dev> -g<W>x<H> -r<X>[x<Y>] -d{First|Last}Page=<#>\n\
         -sOutputFile=<file> (-s<option>=<string> | -d<option>[=<value>])*\n\
         -J<PJL commands>\n";

/* ------ Pseudo-errors used internally ------ */
/* Copied from gs/psi/ierrors.h */
/*
 * Internal code for a fatal error.
 * gs_interpret also returns this for a .quit with a positive exit code.
 */
#define e_Fatal (-100)

/*
 * Internal code for the .quit operator.
 * The real quit code is an integer on the operand stack.
 * gs_interpret returns this only for a .quit with a zero exit code.
 */
#define e_Quit (-101)

/*
 * Internal code for a normal exit when usage info is displayed.
 * This allows Window versions of Ghostscript to pause until
 * the message can be read.
 */
#define e_Info (-110)

void *instance;
BOOL quitnow = FALSE;
HANDLE hthread;
DWORD thread_id;
HWND hwndforeground;	/* our best guess for our console window handle */


/* This structure is defined in plmain.c, but we need it here, because we have
 * our own version of pl_main(). Much of the following is copied from plmain.c
 * for this simple reason. Although it results in some code duplication it means
 * that we don't have to modify the existing code at all.
 */
/*
 * Define bookeeping for interpreters and devices
 */
typedef struct win_pl_main_universe_s {
    gs_memory_t             *mem;                /* mem alloc to dealloc devices */
    pl_interp_implementation_t const * const *
                            pdl_implementation;  /* implementations to choose from */
    pl_interp_instance_t *  pdl_instance_array[100];    /* parallel to pdl_implementation */
    pl_interp_t *           pdl_interp_array[100];      /* parallel to pdl_implementation */
    pl_interp_implementation_t const
                            *curr_implementation;
    pl_interp_instance_t *  curr_instance;
    gx_device               *curr_device;
} pl_main_universe_t;


/* Include the extern for the device list. */
extern_gs_lib_device_list();

/* Extern for PJL */
extern pl_interp_implementation_t pjl_implementation;

/* Extern for PDL(s): currently in one of: plimpl.c (XL & PCL), */
/* pcimpl.c (PCL only), or pximpl (XL only) depending on make configuration.*/
extern pl_interp_implementation_t const * const pdl_implementation[];   /* zero-terminated list */

void pl_print_usage(const pl_main_instance_t *pti, const char *msg);

/* Pre-page portion of page finishing routine */
int pl_pre_finish_page(pl_interp_instance_t *interp, void *closure);

/* Post-page portion of page finishing routine */
int pl_post_finish_page(pl_interp_instance_t *interp, void *closure);

/* Error termination, called back from plplatf.c */
/* Only called back if abnormal termination */
void pl_exit(int exit_status);

/* -------------- Read file cursor operations ---------- */
/* Open a read cursor w/specified file */
int pl_main_cursor_open(const gs_memory_t *mem,
                    pl_top_cursor_t   *cursor,        /* cursor to init/open */
                    const char        *fname,         /* name of file to open */
                    byte              *buffer,        /* buffer to use for reading */
                    unsigned          buffer_length   /* length of *buffer */
);

#ifdef DEBUG
/* Refill from input */
int pl_main_cursor_next(pl_top_cursor_t *cursor);
#endif /* DEBUG */

/* Read back curr file position */
long pl_main_cursor_position(pl_top_cursor_t *cursor);

/* Close read cursor */
void pl_main_cursor_close(pl_top_cursor_t *cursor);

/* --------- Functions operating on pl_main_universe_t ----- */
/* Init main_universe from pdl_implementation */
extern int   /* 0 ok, else -1 error */
pl_main_universe_init(
        pl_main_universe_t     *universe,            /* universe to init */
        char                   *err_str,             /* RETURNS error str if error */
        gs_memory_t            *mem,                 /* deallocator for devices */
        pl_interp_implementation_t const * const
                               pdl_implementation[], /* implementations to choose from */
        pl_interp_instance_t   *pjl_instance,        /* pjl to
                                                        reference */
        pl_main_instance_t     *inst,                /* instance for pre/post print */
        pl_page_action_t       pl_pre_finish_page,   /* pre-page action */
        pl_page_action_t       pl_post_finish_page   /* post-page action */
);

extern pl_interp_instance_t *get_interpreter_from_memory( const gs_memory_t *mem );

/* Undo pl_main_universe_init */
extern int pl_main_universe_dnit(pl_main_universe_t *universe, char *err_str);

/* Select new device and/or implementation, deselect one one (opt) */
extern pl_interp_instance_t *    /* rets current interp_instance, 0 if err */
pl_main_universe_select(
        pl_main_universe_t               *universe,              /* universe to select from */
        char                             *err_str,               /* RETURNS error str if error */
        pl_interp_instance_t             *pjl_instance,          /* pjl */
        pl_interp_implementation_t const *desired_implementation,/* impl to select */
        pl_main_instance_t               *pti,                   /* inst contains device */
        gs_param_list                    *params                 /* device params to set */
);

/* ------- Functions related to pl_main_instance_t ------ */

/* Initialize the instance parameters. */
extern void pl_main_init_instance(pl_main_instance_t *pti, gs_memory_t *mem);

/* ---------------- Static data for memory management ------------------ */

static gs_gc_root_t device_root;

#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
void *hwndtext; /* Hack: Should be of HWND type. */
#endif

/* ---------------- Forward decls ------------------ */
/* Functions to encapsulate pl_main_universe_t */
extern int   /* 0 ok, else -1 error */
pl_main_universe_init(
        pl_main_universe_t     *universe,            /* universe to init */
        char                   *err_str,             /* RETURNS error str if error */
        gs_memory_t            *mem,                 /* deallocator for devices */
        pl_interp_implementation_t const * const
                               pdl_implementation[], /* implementations to choose from */
        pl_interp_instance_t   *pjl_instance,        /* pjl to reference */
        pl_main_instance_t     *inst,                /* instance for pre/post print */
        pl_page_action_t       pl_pre_finish_page,   /* pre-page action */
        pl_page_action_t       pl_post_finish_page   /* post-page action */
);
extern int   /* 0 ok, else -1 error */
pl_main_universe_dnit(
        pl_main_universe_t     *universe,            /* universe to dnit */
        char                   *err_str              /* RETRUNS errmsg if error return */
);
extern pl_interp_instance_t *    /* rets current interp_instance, 0 if err */
pl_main_universe_select(
        pl_main_universe_t               *universe,              /* universe to select from */
        char                             *err_str,               /* RETURNS error str if error */
        pl_interp_instance_t             *pjl_instance,          /* pjl */
        pl_interp_implementation_t const *desired_implementation,/* impl to select */
        pl_main_instance_t               *pti,                   /* inst contains device */
        gs_param_list                    *params                 /* device params to use */
);

static pl_interp_implementation_t const *
pl_auto_sense(
   const char*                      name,         /* stream  */
   int                              buffer_length, /* length of stream */
   pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
);

static pl_interp_implementation_t const *
pl_select_implementation(
  pl_interp_instance_t *pjl_instance,
  pl_main_instance_t *pmi,
  pl_top_cursor_t r
);


/* Process the options on the command line. */
static FILE *pl_main_arg_fopen(const char *fname, void *ignore_data);

/* Initialize the instance parameters. */
void pl_main_init_instance(pl_main_instance_t *pmi, gs_memory_t *memory);
void pl_main_reinit_instance(pl_main_instance_t *pmi);

/* Process the options on the command line, including making the
   initial device and setting its parameters.  */
extern int pl_main_process_options(pl_main_instance_t *pmi, arg_list *pal,
                            gs_c_param_list *params,
                            pl_interp_instance_t *pjl_instance,
                            pl_interp_implementation_t const * const impl_array[],
                            char **filename);

/* Find default language implementation */
pl_interp_implementation_t const *
pl_auto_sense(const char* buf, int buf_len, pl_interp_implementation_t const * const impl_array[]);

static pl_interp_implementation_t const *
pl_pjl_select(pl_interp_instance_t *pjl_instance,
              pl_interp_implementation_t const * const impl_array[]);

/* Pre-page portion of page finishing routine */
int     /* ret 0 if page should be printed, 1 if no print, else -ve error */
pl_pre_finish_page(pl_interp_instance_t *interp, void *closure);

/* Post-page portion of page finishing routine */
int     /* ret 0, else -ve error */
pl_post_finish_page(pl_interp_instance_t *interp, void *closure);

      /* -------------- Read file cursor operations ---------- */
/* Open a read cursor w/specified file */
int pl_main_cursor_open(const gs_memory_t *, pl_top_cursor_t *, const char *, byte *, unsigned);

#ifdef DEBUG
/* Refill from input, avoid extra call level for efficiency */
int pl_main_cursor_next(pl_top_cursor_t *cursor);
#else
 #define pl_main_cursor_next(curs) (pl_top_cursor_next(curs))
#endif

/* Read back curr file position */
long pl_main_cursor_position(pl_top_cursor_t *cursor);

/* Close read cursor */
void pl_main_cursor_close(pl_top_cursor_t *cursor);


/* return index in gs device list -1 if not found */
static inline int
get_device_index(const gs_memory_t *mem, const char *value)
{
    const gx_device *const *dev_list;
    int num_devs = gs_lib_device_list(&dev_list, NULL);
    int di;

    for ( di = 0; di < num_devs; ++di )
        if ( !strcmp(gs_devicename(dev_list[di]), value) )
            break;
    if ( di == num_devs ) {
        errprintf(mem, "Unknown device name %s.\n", value);
        return -1;
    }
    return di;
}

static int
close_job(pl_main_universe_t *universe, pl_main_instance_t *pti)
{
    return pl_dnit_job(universe->curr_instance);
}

/* End of copy from plmain.c */

/* This section copied in large part from the Ghostscript Windows client, to
 * support the Windows display device.
 */
/*********************************************************************/
/* We must run windows from another thread, since main thread */
/* is running Ghostscript and blocks on stdin. */

/* We notify second thread of events using PostThreadMessage()
 * with the following messages. Apparently Japanese Windows sends
 * WM_USER+1 with lParam == 0 and crashes. So we use WM_USER+101.
 * Fix from Akira Kakuto
 */
#define DISPLAY_OPEN WM_USER+101
#define DISPLAY_CLOSE WM_USER+102
#define DISPLAY_SIZE WM_USER+103
#define DISPLAY_SYNC WM_USER+104
#define DISPLAY_PAGE WM_USER+105
#define DISPLAY_UPDATE WM_USER+106

/*
#define DISPLAY_DEBUG
*/

/* The second thread is the message loop */
static void winthread(void *arg)
{
    MSG msg;
    thread_id = GetCurrentThreadId();
    hthread = GetCurrentThread();

    while (!quitnow && GetMessage(&msg, (HWND)NULL, 0, 0)) {
	switch (msg.message) {
	    case DISPLAY_OPEN:
		image_open((IMAGE *)msg.lParam);
		break;
	    case DISPLAY_CLOSE:
		{
		    HANDLE hmutex;
		    IMAGE *img = (IMAGE *)msg.lParam; 
		    hmutex = img->hmutex;
		    image_close(img);
		    CloseHandle(hmutex);
		}
		break;
	    case DISPLAY_SIZE:
		image_updatesize((IMAGE *)msg.lParam);
		break;
	    case DISPLAY_SYNC:
		image_sync((IMAGE *)msg.lParam);
		break;
	    case DISPLAY_PAGE:
		image_page((IMAGE *)msg.lParam);
		break;
	    case DISPLAY_UPDATE:
		image_poll((IMAGE *)msg.lParam);
		break;
	    default:
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
    }
}


/* New device has been opened */
/* Tell user to use another device */
int display_open(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_open(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_new(handle, device);	/* create and add to list */
    img->hmutex = CreateMutex(NULL, FALSE, NULL);
    if (img)
	PostThreadMessage(thread_id, DISPLAY_OPEN, 0, (LPARAM)img);
    return 0;
}

int display_preclose(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_preclose(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_find(handle, device);
    if (img) {
	/* grab mutex to stop other thread using bitmap */
	WaitForSingleObject(img->hmutex, 120000);
    }
    return 0;
}

int display_close(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_close(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_find(handle, device);
    if (img) {
	/* This is a hack to pass focus from image window to console */
	if (GetForegroundWindow() == img->hwnd)
	    SetForegroundWindow(hwndforeground);

	image_delete(img);	/* remove from list, but don't free */
	PostThreadMessage(thread_id, DISPLAY_CLOSE, 0, (LPARAM)img);
    }
    return 0;
}

int display_presize(void *handle, void *device, int width, int height, 
	int raster, unsigned int format)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_presize(0x%x 0x%x, %d, %d, %d, %d, %ld)\n",
	handle, device, width, height, raster, format);
#endif
    img = image_find(handle, device);
    if (img) {
	/* grab mutex to stop other thread using bitmap */
	WaitForSingleObject(img->hmutex, 120000);
    }
    return 0;
}
   
int display_size(void *handle, void *device, int width, int height, 
	int raster, unsigned int format, unsigned char *pimage)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_size(0x%x 0x%x, %d, %d, %d, %d, %ld, 0x%x)\n",
	handle, device, width, height, raster, format, pimage);
#endif
    img = image_find(handle, device);
    if (img) {
	image_size(img, width, height, raster, format, pimage);
	/* release mutex to allow other thread to use bitmap */
	ReleaseMutex(img->hmutex);
	PostThreadMessage(thread_id, DISPLAY_SIZE, 0, (LPARAM)img);
    }
    return 0;
}
   
int display_sync(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_sync(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_find(handle, device);
    if (img && !img->pending_sync) {
	img->pending_sync = 1;
	PostThreadMessage(thread_id, DISPLAY_SYNC, 0, (LPARAM)img);
    }
    return 0;
}

int display_page(void *handle, void *device, int copies, int flush)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_page(0x%x, 0x%x, copies=%d, flush=%d)\n", 
	handle, device, copies, flush);
#endif
    img = image_find(handle, device);
    if (img)
	PostThreadMessage(thread_id, DISPLAY_PAGE, 0, (LPARAM)img);
    return 0;
}

int display_update(void *handle, void *device, 
    int x, int y, int w, int h)
{
    IMAGE *img;
    img = image_find(handle, device);
    if (img && !img->pending_update && !img->pending_sync) {
	img->pending_update = 1;
	PostThreadMessage(thread_id, DISPLAY_UPDATE, 0, (LPARAM)img);
    }
    return 0;
}

/*
#define DISPLAY_DEBUG_USE_ALLOC
*/
#ifdef DISPLAY_DEBUG_USE_ALLOC
/* This code isn't used, but shows how to use this function */
void *display_memalloc(void *handle, void *device, unsigned long size)
{
    void *mem;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_memalloc(0x%x 0x%x %d)\n", 
	handle, device, size);
#endif
    mem = malloc(size);
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "  returning 0x%x\n", (int)mem);
#endif
    return mem;
}

int display_memfree(void *handle, void *device, void *mem)
{
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_memfree(0x%x, 0x%x, 0x%x)\n", 
	handle, device, mem);
#endif
    free(mem);
    return 0;
}
#endif

int display_separation(void *handle, void *device, 
   int comp_num, const char *name,
   unsigned short c, unsigned short m,
   unsigned short y, unsigned short k)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_separation(0x%x, 0x%x, %d '%s' %d,%d,%d,%d)\n", 
	handle, device, comp_num, name, (int)c, (int)m, (int)y, (int)k);
#endif
    img = image_find(handle, device);
    if (img)
        image_separation(img, comp_num, name, c, m, y, k);
    return 0;
}


display_callback display = { 
    sizeof(display_callback),
    DISPLAY_VERSION_MAJOR,
    DISPLAY_VERSION_MINOR,
    display_open,
    display_preclose,
    display_close,
    display_presize,
    display_size,
    display_sync,
    display_page,
    display_update,
#ifdef DISPLAY_DEBUG_USE_ALLOC
    display_memalloc,	/* memalloc */
    display_memfree,	/* memfree */
#else
    NULL,	/* memalloc */
    NULL,	/* memfree */
#endif
    display_separation
};

/* End of code copied from the Windows Ghostscript client for the display
 * device
 */

/* This is our redefinition of pl_main(). The main difference is that we 
 * check for the existence of teh display device and if it is present we
 * set it up as the initial device, and populate the 'callback' member of
 * the device structure with the callbacks for the display device support above.
 */
/* ----------- Command-line driver for pl_interp's  ------ */
/*
 * Here is the real main program.
 */
GSDLLEXPORT int GSDLLAPI
win_pl_main(
    int                 argc,
    char *        argv[]
)
{
    gs_memory_t *           mem;
    gs_memory_t *           pjl_mem;
    pl_main_instance_t      inst, *pinst = &inst;
    arg_list                args;
    char *                  filename = NULL;
    char                    err_buf[256];
    pl_interp_t *           pjl_interp;
    pl_interp_instance_t *  pjl_instance;
    pl_main_universe_t      universe;
    pl_interp_instance_t *  curr_instance = 0;
    gs_c_param_list         params;

    mem = pl_alloc_init();

    pl_platform_init(mem->gs_lib_ctx->fstdout);


    pjl_mem = mem;

    gs_lib_init1(pjl_mem);

    /* Create a memory allocator to allocate various states from */
    {
        /*
         * gs_iodev_init has to be called here (late), rather than
         * with the rest of the library init procedures, because of
         * some hacks specific to MS Windows for patching the
         * stdxxx IODevices.
         */
        extern void gs_iodev_init(gs_memory_t *);
        gs_iodev_init(pjl_mem);
    }

    /* Init the top-level instance */
    gs_c_param_list_write(&params, pjl_mem);
    gs_param_list_set_persistent_keys((gs_param_list*)&params, false);
    pl_main_init_instance(&inst, mem);
    arg_init(&args, (const char **)argv, argc, pl_main_arg_fopen, NULL);


    /* Create PJL instance */
    if ( pl_allocate_interp(&pjl_interp, &pjl_implementation, pjl_mem) < 0
         || pl_allocate_interp_instance(&pjl_instance, pjl_interp, pjl_mem) < 0 ) {
        errprintf(mem, "Unable to create PJL interpreter.");
        return -1;
    }

    /* Create PDL instances, etc */
    if (pl_main_universe_init(&universe, err_buf, mem, pdl_implementation,
                              pjl_instance, &inst, &pl_pre_finish_page, &pl_post_finish_page) < 0) {
        errprintf(mem, err_buf);
        return -1;
    }

#ifdef DEBUG
    if (gs_debug_c(':'))
        pl_print_usage(&inst, "Start");
#endif

    /* ------ Begin Main LOOP ------- */
    for (;;) {
        /* Process one input file. */
        /* for debugging we test the parser with a small 256 byte
           buffer - for production systems use 8192 bytes */
#ifdef DEBUG
        byte                buf[1<<9];
#else
        byte                buf[1<<13];
#endif
        pl_top_cursor_t     r;
        int                 code = 0;
        bool                in_pjl = true;
        bool                new_job = false;


        if ( pl_init_job(pjl_instance) < 0 ) {
            errprintf(mem, "Unable to init PJL job.\n");
            return -1;
        }

        /* Process any new options. May request new device. */
        if (argc==3 ||
            pl_main_process_options(&inst,
                                    &args,
                                    &params,
				    pjl_instance, pdl_implementation, &filename) < 0) {

            /* Print error verbage and return */
            int i;
            const gx_device **dev_list;
            int num_devs = gs_lib_device_list((const gx_device * const **)&dev_list, NULL);

            errprintf(mem, pl_usage, argv[0]);

            if (pl_characteristics(&pjl_implementation)->version)
                errprintf(mem, "Version: %s\n",
                          pl_characteristics(&pjl_implementation)->version);
            if (pl_characteristics(&pjl_implementation)->build_date)
                errprintf(mem, "Build date: %s\n",
                          pl_characteristics(&pjl_implementation)->build_date);
            errprintf(mem, "Devices:");
            for ( i = 0; i < num_devs; ++i ) {
                if ( ( (i + 1) )  % 9 == 0 )
                    errprintf(mem, "\n");
                errprintf(mem, " %s", gs_devicename(dev_list[i]));
            }
            errprintf(mem, "\n");
	}
        if (!filename)
            break;  /* no nore files to process */

	/* If the display device is selected (default), set up the callback */
	if (strcmp(inst.device->dname, "display") == 0) {
	    gx_device_display *ddev = (gx_device_display *)inst.device;
	    ddev->callback = &display;
	}
	/* open file for reading - NB we should respect the minimum
           requirements specified by each implementation in the
           characteristics structure */
        if (pl_main_cursor_open(mem, &r, filename, buf, sizeof(buf)) < 0) {
            errprintf(mem, "Unable to open %s for reading.\n", filename);
            return -1;
        }
#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
        vd_trace0 = visual_tracer_init();
#endif

#ifdef DEBUG
        if (gs_debug_c(':'))
            dprintf1("%% Reading %s:\n", filename);
#endif
        /* pump data thru PJL/PDL until EOD or error */
        new_job = false;
        in_pjl = true;
        for (;;) {
            if_debug1('i', "[i][file pos=%ld]\n", pl_main_cursor_position(&r));
            /* end of data - if we are not back in pjl the job has
               ended in the middle of the data stream. */
            if (pl_main_cursor_next(&r) <= 0) {
                if_debug0('|', "End of of data\n");
                if ( !in_pjl ) {
                    if_debug0('|', "end of data stream found in middle of job\n");
                    pl_process_eof(curr_instance);
                    if ( close_job(&universe, &inst) < 0 ) {
                        dprintf("Unable to deinit PDL job.\n");
                        return -1;
                    }
                }
                break;
            }
            if ( in_pjl ) {
                if_debug0('|', "Processing pjl\n");
                code = pl_process(pjl_instance, &r.cursor);
                if (code == e_ExitLanguage) {
                    if_debug0('|', "Exiting pjl\n" );
                    in_pjl = false;
                    new_job = true;
                }
            }
            if ( new_job ) {
                if (mem->gs_lib_ctx->gs_next_id > 0xFF000000) {
                    dprintf("Once a year reset the gs_next_id.\n");
                    return -1;
                }

                if_debug0('|', "Selecting PDL\n" );
                curr_instance = pl_main_universe_select(&universe, err_buf,
                                                        pjl_instance,
                                                        pl_select_implementation(pjl_instance, &inst, r),
                                                        &inst, (gs_param_list *)&params);
                if ( curr_instance == NULL ) {
                    dprintf(err_buf);
                    return -1;
                }

                if ( pl_init_job(curr_instance) < 0 ) {
                    dprintf("Unable to init PDL job.\n");
                    return -1;
                }
                if_debug1('|', "selected and initializing (%s)\n",
                          pl_characteristics(curr_instance->interp->implementation)->language);
                new_job = false;
            }
            if ( curr_instance ) {

                /* Special case when the job resides in a seekable file and
                   the implementation has a function to process a file at a
                   time. */
                if (curr_instance->interp->implementation->proc_process_file &&
                    r.strm != mem->gs_lib_ctx->fstdin) {
                    if_debug1('|', "processing job from file (%s)\n", filename);
                    code = pl_process_file(curr_instance, filename);
                    if (code < 0) {
                        dprintf1("Warning interpreter exited with error code %d\n", code);
                    }
                    if (close_job(&universe, &inst) < 0) {
                        dprintf("Unable to deinit PJL.\n");
                        return -1;
                    }
                    if_debug0('|', "exiting job and proceeding to next file\n");
                    break; /* break out of the loop to process the next file */
                }

                code = pl_process(curr_instance, &r.cursor);
                if_debug1('|', "processing (%s) job\n",
                          pl_characteristics(curr_instance->interp->implementation)->language);
                if (code == e_ExitLanguage) {
                    in_pjl = true;
                    if_debug1('|', "exiting (%s) job back to pjl\n",
                              pl_characteristics(curr_instance->interp->implementation)->language);
                    if ( close_job(&universe, &inst) < 0 ) {
                        dprintf( "Unable to deinit PDL job.\n");
                        return -1;
                    }
                    if ( pl_init_job(pjl_instance) < 0 ) {
                        dprintf("Unable to init PJL job.\n");
                        return -1;
                    }
                    pl_renew_cursor_status(&r);
                } else if ( code < 0 ) { /* error and not exit language */
                    dprintf1("Warning interpreter exited with error code %d\n", code );
                    dprintf("Flushing to end of job\n" );
                    /* flush eoj may require more data */
                    while ((pl_flush_to_eoj(curr_instance, &r.cursor)) == 0) {
                        if_debug1('|', "flushing to eoj for (%s) job\n",
                                  pl_characteristics(curr_instance->interp->implementation)->language);
                        if (pl_main_cursor_next(&r) <= 0) {
                            if_debug0('|', "end of data found while flushing\n");
                            break;
                        }
                    }
                    pl_report_errors(curr_instance, code,
                                     pl_main_cursor_position(&r),
                                     inst.error_report > 0);
                    if ( close_job(&universe, &inst) < 0 ) {
                        dprintf("Unable to deinit PJL.\n");
                        return -1;
                    }
                    /* Print PDL status if applicable, then dnit PDL job */
                    code = 0;
                    new_job = true;
                    /* go back to pjl */
                    in_pjl = true;
                }
            }
        }
        pl_main_cursor_close(&r);
    }

    /* ----- End Main loop ----- */

    /* Dnit PDLs */
    if (pl_main_universe_dnit(&universe, err_buf)) {
        dprintf(err_buf);
        return -1;
    }
    /* dnit pjl */
    if ( pl_deallocate_interp_instance(pjl_instance) < 0
         || pl_deallocate_interp(pjl_interp) < 0 ) {
        dprintf("Unable to close out PJL instance.\n");
        return -1;
    }

    /* We lost the ability to print peak memory usage with the loss
     * of the memory wrappers.
     */
    /* release param list */
    gs_c_param_list_release(&params);
    arg_finit(&args);

#if defined(DEBUG) && defined(ALLOW_VD_TRACE)
    visual_tracer_close();
#endif
    if ( gs_debug_c('A') )
        dprintf("Final time" );
    pl_platform_dnit(0);
    return 0;
}

/* -------- Command-line processing ------ */

/* Create a default device if not already defined. */
static int
pl_top_create_device(pl_main_instance_t *pti, int index, bool is_default)
{
    int code = 0;
    if ( index < 0 )
        return -1;
    if ( !is_default || !pti->device ) {
        const gx_device **list;

        /* We assume that nobody else changes pti->device,
           and this function is called from this module only.
           Due to that device_root is always consistent with pti->device,
           and it is regisrtered if and only if pti->device != NULL.
        */
        if (pti->device != NULL) {
            pti->device = NULL;
            gs_unregister_root(pti->device_memory, &device_root, "pl_main_universe_select");
        }
        gs_lib_device_list((const gx_device * const **)&list, NULL);
        code = gs_copydevice(&pti->device, list[index],
                             pti->device_memory);
        if (pti->device != NULL)
          gs_register_struct_root(pti->device_memory, &device_root,
                                  (void **)&pti->device, "pl_top_create_device");

    }
    return code;
}


/* Process the options on the command line. */
static FILE *
pl_main_arg_fopen(const char *fname, void *ignore_data)
{       return fopen(fname, "r");
}

static void
set_debug_flags(const char *arg, char *flags)
{
    byte value = (*arg == '-' ? (++arg, 0) : 0xff);

    while (*arg)
        flags[*arg++ & 127] = value;
}

/* either the (1) implementation has been selected on the command line or
   (2) it has been selected in PJL or (3) we need to auto sense. */
static pl_interp_implementation_t const *
pl_select_implementation(pl_interp_instance_t *pjl_instance, pl_main_instance_t *pmi, pl_top_cursor_t r)
{
    /* Determine language of file to interpret. We're making the incorrect */
    /* assumption that any file only contains jobs in one PDL. The correct */
    /* way to implement this would be to have a language auto-detector. */
    pl_interp_implementation_t const *impl;
    if (pmi->implementation)
        return pmi->implementation;  /* was specified as cmd opt */
    /* select implementation */
    if ( (impl = pl_pjl_select(pjl_instance, pdl_implementation)) != 0 )
        return impl;
    /* lookup string in name field for each implementation */
    return pl_auto_sense((const char *)r.cursor.ptr + 1, (r.cursor.limit - r.cursor.ptr), pdl_implementation);
}

/* Find default language implementation */
static pl_interp_implementation_t const *
pl_pjl_select(pl_interp_instance_t *pjl_instance,
              pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
)
{
    pjl_envvar_t *language;
    pl_interp_implementation_t const * const * impl;
    language = pjl_proc_get_envvar(pjl_instance, "language");
    for (impl = impl_array; *impl != 0; ++impl) {
        if ( !strcmp(pl_characteristics(*impl)->language, language) )
            return *impl;
    }
    /* Defaults to NULL */
    return 0;
}

/* Find default language implementation */
static pl_interp_implementation_t const *
pl_auto_sense(
   const char*                      name,         /* stream  */
   int                              buffer_length, /* length of stream */
   pl_interp_implementation_t const * const impl_array[] /* implementations to choose from */
)
{
    /* Lookup this string in the auto sense field for each implementation */
    pl_interp_implementation_t const * const * impl;
    for (impl = impl_array; *impl != 0; ++impl) {
        if ( buffer_length >= (strlen(pl_characteristics(*impl)->auto_sense_string)) )
            if ( !strncmp(pl_characteristics(*impl)->auto_sense_string,
                          name,
                          (strlen(pl_characteristics(*impl)->auto_sense_string))) )
                return *impl;
    }
    /* Defaults to PCL */
    return impl_array[0];
}



/*********************************************************************/
/* stdio functions */


/*********************************************************************/

/* Our 'main' routine sets up the separate thread to look after the 
 * display window, and inserts the relevant defaults for the display device.
 * If the user specifies a different device, or different parameters to
 * the display device, the later ones should take precedence.
 */
int main(int argc, char *argv[])
{
    int code, code1;
    int exit_code;
    int exit_status;
    int nargc;
    char **nargv;
    char buf[256];
    char dformat[64];
    char ddpi[64];

    hwndforeground = GetForegroundWindow();	/* assume this is ours */
    memset(buf, 0, sizeof(buf));

    if (_beginthread(winthread, 65535, NULL) == -1) {
	wprintf(L"GUI thread creation failed\n");
    }
    else {
	int n = 30;
	/* wait for thread to start */
	Sleep(0);
	while (n && (hthread == INVALID_HANDLE_VALUE)) {
	    n--;
	    Sleep(100);
        }
	while (n && (PostThreadMessage(thread_id, WM_USER, 0, 0) == 0)) {
	    n--;
	    Sleep(100);
	}
	if (n == 0)
	    wprintf(L"Can't post message to GUI thread\n");
    }

    {   int format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | 
		DISPLAY_DEPTH_1 | DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST;
	HDC hdc = GetDC(NULL);	/* get hdc for desktop */
	int depth = GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL);
	sprintf(ddpi, "-dDisplayResolution=%d", GetDeviceCaps(hdc, LOGPIXELSY));
        ReleaseDC(NULL, hdc);
	if (depth == 32)
 	    format = DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST | 
		DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST;
	else if (depth == 16)
 	    format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | 
		DISPLAY_DEPTH_16 | DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST |
		DISPLAY_NATIVE_555;
	else if (depth > 8)
 	    format = DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | 
		DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST;
	else if (depth >= 8)
 	    format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | 
		DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST;
	else if (depth >= 4)
 	    format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE | 
		DISPLAY_DEPTH_4 | DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST;
        sprintf(dformat, "-dDisplayFormat=%d", format);
    }
    nargc = argc + 2;
    nargv = (char **)malloc((nargc + 1) * sizeof(char *));
    nargv[0] = argv[0];
    nargv[1] = dformat;
    nargv[2] = ddpi;
    memcpy(&nargv[3], &argv[1], argc * sizeof(char *));

    code = win_pl_main(nargc, nargv);

    free(nargv);

    /* close other thread */
    quitnow = TRUE; 
    PostThreadMessage(thread_id, WM_QUIT, 0, (LPARAM)0);
    Sleep(0);

    exit_status = 0;
    switch (code) {
	case 0:
	case e_Info:
	case e_Quit:
	    break;
	case e_Fatal:
	    exit_status = 1;
	    break;
	default:
	    exit_status = 255;
    }


    return exit_status;
}

