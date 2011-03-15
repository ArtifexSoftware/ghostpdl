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
#include "gdevdsp.h"
#include "gdevdsp2.h"
#if 0
#include "gsequivc.h"
#endif

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

    code = pl_main_aux(nargc, nargv, &display);

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

