/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

/*$Id: dwmain.c 11973 2010-12-22 19:16:02Z robin $ */

/* dwmain.c */
/* Windows version of the main program command-line interpreter for PCL interpreters
 */

#include "windows_.h"
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include "gserrors.h"
#include "gdevdsp.h"
#include "plwimg.h"
#include "plapi.h"

#define PJL_UEL "\033%-12345X"

#if 0
/* FIXME: this is purely because the gsdll.h requires psi/iapi.h and
 * we don't want that required here. But as a couple of Windows specific
 * devices depend upon pgsdll_callback being defined, having a compatible
 * set of declarations here saves having to have different device lists
 * for Ghostscript and the other languages, and as both devices are
 * deprecated, a simple solution seems best = for now.
 */
#ifdef __IBMC__
#define GSPLDLLCALLLINK _System
#else
#define GSPLDLLCALLLINK
#endif

typedef int (* GSPLDLLCALLLINK GS_PL_DLL_CALLBACK) (int, char *, unsigned long);
GS_PL_DLL_CALLBACK pgsdll_callback = NULL;
#endif

void *instance = NULL;

#ifndef METRO
BOOL quitnow = FALSE;

HANDLE hthread;

DWORD thread_id;

HWND hwndforeground;            /* our best guess for our console window handle */

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
                    IMAGE *img = (IMAGE *)msg.lParam;
                    HANDLE hmutex = img->hmutex;
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
    img = image_new(handle, device);    /* create and add to list */
    if (img) {
        img->hmutex = CreateMutex(NULL, FALSE, NULL);
        PostThreadMessage(thread_id, DISPLAY_OPEN, 0, (LPARAM)img);
    }
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

        image_delete(img);      /* remove from list, but don't free */
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
    display_memalloc,   /* memalloc */
    display_memfree,    /* memfree */
#else
    NULL,       /* memalloc */
    NULL,       /* memfree */
#endif
    display_separation
};
#endif /* !METRO */
typedef BOOL (SetProcessDPIAwareFn)(void);

static void
avoid_windows_scale(void)
{
    /* Fetch the function address and only call it if it is there; this keeps
     * compatability with Windows < 8.1 */
    HMODULE hUser32 = LoadLibrary(TEXT("user32.dll"));
    SetProcessDPIAwareFn *ptr;

    ptr = (SetProcessDPIAwareFn *)GetProcAddress(hUser32, "SetProcessDPIAware");
    if (ptr != NULL)
        ptr();
    FreeLibrary(hUser32);
}

/* Our 'main' routine sets up the separate thread to look after the
 * display window, and inserts the relevant defaults for the display device.
 * If the user specifies a different device, or different parameters to
 * the display device, the later ones should take precedence.
 */
static int
main_utf8(int argc, char *argv[])
{
    int code, code1;
    int exit_code;
    int exit_status;
    int nargc;
    char **nargv;
    char dformat[64];
    char ddpi[64];
    size_t uel_len = strlen(PJL_UEL);
    int dummy;

    /* Mark us as being 'system dpi aware' to avoid horrid scaling */
    avoid_windows_scale();

    if (!_isatty(fileno(stdin)))
        _setmode(fileno(stdin), _O_BINARY);
    _setmode(fileno(stdout), _O_BINARY);
    _setmode(fileno(stderr), _O_BINARY);
#ifndef METRO
    hwndforeground = GetForegroundWindow();     /* assume this is ours */
#endif

    if (gsapi_new_instance(&instance, NULL) < 0) {
        fprintf(stderr, "Cannot create instance\n");
        return 1;
    }

#ifndef METRO
    if (_beginthread(winthread, 65535, NULL) == -1) {
        fprintf(stderr, "GUI thread creation failed\n");
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
            fprintf(stderr, "Can't post message to GUI thread\n");
    }

#endif

#ifdef METRO
    nargc = argc;
    nargv = argv;
#else
    gsapi_set_display_callback(instance, &display);
    {
        int format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
            DISPLAY_DEPTH_1 | DISPLAY_BIGENDIAN | DISPLAY_BOTTOMFIRST;
        HDC hdc = GetDC(NULL);  /* get hdc for desktop */
        int depth = GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL);
        sprintf(ddpi, "-dDisplayResolution=%d", GetDeviceCaps(hdc, LOGPIXELSY));
        ReleaseDC(NULL, hdc);
        if (depth == 32)
            format = DISPLAY_COLORS_RGB |
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
    if (nargv == NULL) {
        fprintf(stderr, "Malloc failure!\n");
    } else {
        nargv[0] = argv[0];
        nargv[1] = dformat;
        nargv[2] = ddpi;
        memcpy(&nargv[3], &argv[1], argc * sizeof(char *));
#endif
        code = gsapi_init_with_args(instance, nargc, nargv);
        if (code >= 0)
            code = gsapi_run_string_begin(instance, 0, &dummy);
        if (code >= 0)
            code = gsapi_run_string_continue(instance, PJL_UEL, uel_len, 0, &dummy);
        if (code >= 0)
            code = gsapi_run_string_end(instance, 0, &dummy);
        if (code == gs_error_InterpreterExit)
            code = 0;
        code1 = gsapi_exit(instance);
        if (code == 0 || (code == gs_error_Quit && code1 != 0))
            code = code1;
        gsapi_delete_instance(instance);

#ifndef METRO
    free(nargv);
#endif
    }
#ifndef METRO
    /* close other thread */
    quitnow = TRUE;
    PostThreadMessage(thread_id, WM_QUIT, 0, (LPARAM) 0);
    Sleep(0);
#endif

    exit_status = 0;
    switch (code) {
        case 0:
        case gs_error_Info:
        case gs_error_Quit:
            break;
        case gs_error_Fatal:
            exit_status = 1;
            break;
        default:
            exit_status = 255;
    }

    return exit_status;
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[]) {
    /* Duplicate args as utf8 */
    char **nargv;
    int i, code, cp;

    nargv = calloc(argc, sizeof(nargv[0]));
    if (nargv == NULL)
        goto err;
    for (i=0; i < argc; i++) {
        nargv[i] = malloc(wchar_to_utf8(NULL, argv[i]));
        if (nargv[i] == NULL)
            goto err;
        (void)wchar_to_utf8(nargv[i], argv[i]);
    }

    /* Switch console code page to CP_UTF8 (65001) as we may send utf8 strings
     * to stdout/stderr.
     */
    cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    code = main_utf8(argc, nargv);

    SetConsoleOutputCP(cp);

    if (0) {
err:
        fprintf(stderr,
                "Ghostscript failed to initialise due to malloc failure\n");
        code = -1;
    }

    if (nargv) {
        for (i=0; i<argc; i++) {
            free(nargv[i]);
        }
        free(nargv);
    }

    return code;
}
