/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

/* prevent gp.h redefining sprintf */
#define sprintf sprintf

/* dwmainc.c */

#include "windows_.h"
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include "ierrors.h"
#include "iapi.h"
#include "gdevdsp.h"
#include "dwdll.h"
#include "dwimg.h"
#include "dwtrace.h"

/* Patch by Rod Webster (rodw) */
/* Added conditional below to allow Borland Compilation (Tested on 5.5) */
/* It would be better to place this code in an include file but no dwmainc.h exists */
#ifdef __BORLANDC__
#define _isatty isatty
#define _setmode setmode
#endif

GSDLL gsdll;
void *instance = NULL;
#ifndef METRO
BOOL quitnow = FALSE;
HANDLE hthread;
DWORD thread_id;
HWND hwndforeground;    /* our best guess for our console window handle */
#endif

char start_string[] = "systemdict /start get exec\n";

#ifndef NDEBUG
static void windows_debug_out(const char *str, int len)
{
    char buf[256];
    int n;

    /* Stupid windows only lets us have a way to print
     * null terminated strings. This means we need to
     * perform all sorts of operations on the data
     * we get in. */
    while (len)
    {
        /* Skip leading 0's */
        while (len > 0 && *str == 0)
            len--, str++;
        /* Copy a run of as many non-zeros as we can into the buffer
         * without overflowing it. */
        for (n = 0; n < sizeof(buf)-1 && n < len && *str != 0; str++, n++)
            buf[n] = *str;
        /* NULL terminate the buffer */
        buf[n] = 0;
        /* And output it */
        len -= n;
        OutputDebugStringA(buf);
    }
}
#endif

/*********************************************************************/
/* stdio functions */

static int GSDLLCALL
gsdll_stdin(void *instance, char *buf, int len)
{
    return _read(fileno(stdin), buf, len);
}

static int GSDLLCALL
gsdll_stdout(void *instance, const char *str, int len)
{
#ifndef NDEBUG
    windows_debug_out(str, len);
#endif
    fwrite(str, 1, len, stdout);
    fflush(stdout);
    return len;
}

static int GSDLLCALL
gsdll_stderr(void *instance, const char *str, int len)
{
#ifndef NDEBUG
    windows_debug_out(str, len);
#endif
    fwrite(str, 1, len, stderr);
    fflush(stderr);
    return len;
}

/* make a file readable, for drag'n'drop */
/* In this version (for the console app) TCHAR is just char */
int dwmain_add_file_control_path(const TCHAR *pathfile)
{
    int i, code;
    char *p = (char *)pathfile;

    for (i = 0; i < strlen(p); i++) {
        if (p[i] == '\\') {
            p[i] = '/';
        }
    }
    code = gsdll.add_control_path(instance, GS_PERMIT_FILE_READING, (const char *)p);
    for (i = 0; i < strlen(p); i++) {
        if (p[i] == '/') {
            p[i] = '\\';
        }
    }
    return code;
}
void dwmain_remove_file_control_path(const TCHAR *pathfile)
{
    int i;
    char *p = (char *)pathfile;

    for (i = 0; i < strlen(p); i++) {
        if (p[i] == '\\') {
            p[i] = '/';
        }
    }
    gsdll.remove_control_path(instance, GS_PERMIT_FILE_READING, (const char *)p);
    for (i = 0; i < strlen(p); i++) {
        if (p[i] == '/') {
            p[i] = '\\';
        }
    }
}

/* stdio functions - versions that translate to/from utf-8 */
static int GSDLLCALL
gsdll_stdin_utf8(void *instance, char *buf, int len)
{
    static WCHAR thiswchar = 0; /* wide character to convert to multiple bytes */
    static int nmore = 0;       /* number of additional encoding bytes to generate */
    UINT consolecp = 0;
    int nret = 0;               /* number of bytes returned to caller */
    int i;

    /* protect against caller passing invalid len */
    while (len > 0) {
        while (len && nmore) {
            nmore--;
            *buf++ = 0x80 | ((thiswchar >> (6 * nmore)) & 0x3F), nret++;
            len--;
        }
        while (len) {
            /* Previously the code here has always just checked for whether
             * _read returns <= 0 to see whether we should exit. According
             * to the docs -1 means error, 0 means EOF. Unfortunately,
             * building using VS2015 there appears to be a bug in the
             * runtime, whereby a line with a single return on it (on an
             * ANSI encoded Text file at least) causes a 0 return value.
             * We hack around this by second guessing the code. We clear
             * the buffer to start with, and (if we get a zero return
             * value) we check to see if we have a '\n' in the buffer. If
             * we do, then we disbelieve the return value. */
            int num_read;
            buf[0] = 0; /* Clear buffer */
            num_read = _read(fileno(stdin), buf, 1);
            if (num_read == 0 && buf[0] == 0x0a)
                num_read = 1; /* Stupid VS2015 */
            if (0 >= num_read)
                return nret;
            nret++, buf++, len--;
            if (buf[-1] == '\n')
                /* return at end of line (note: no traslation needed) */
                return nret;
            else if ((unsigned char)buf[-1] <= 0x7F)
                /* no translation needed for 7-bit ASCII codes */
                continue;
            else {
                /* extended character, may be double */
                BYTE dbcsstr[2];

                dbcsstr[0] = buf[-1];
                if (!consolecp)
                    consolecp = GetConsoleCP();
                thiswchar = L'?'; /* initialize in case the conversion below fails */
                if (IsDBCSLeadByteEx(consolecp, dbcsstr[0])) {
                    /* double-byte character code, fetch the trail byte */
                    _read(fileno(stdin), &dbcsstr[1], 1);
                    MultiByteToWideChar(consolecp, 0, dbcsstr, 2, &thiswchar, 1);
                }
                else {
                    MultiByteToWideChar(consolecp, 0, dbcsstr, 1, &thiswchar, 1);
                }
                /* convert thiswchar to utf-8 */
                if (thiswchar <= 0x007F) {          /* encoded as single byte */
                    buf[-1] = (char)thiswchar;
                } else if (thiswchar <= 0x07FF) {   /* encoded as 2 bytes */
                    buf[-1] = 0xC0 | ((thiswchar >> 6) & 0x1F);
                    nmore = 1;
                    break;
                } else if (thiswchar <= 0xFFFF) {   /* encoded as 3 bytes */
                    buf[-1] = 0xE0 | ((thiswchar >> 12) & 0xF);
                    nmore = 2;
                    break;
                } else
                    /* note: codes outside the BMP not handled */
                    buf[-1] = '?';
            }
        }
    }
    return nret;
}

static void
gsdll_utf8write(FILE *stdwr, const char *str, int len, WCHAR *thiswchar, int *nmore)
{
    UINT consolecp = 0;

    /* protect against caller passing invalid len */
    while (len > 0) {
        const char *str0;

        /* write ASCII chars without translation */
        for (str0 = str; len && !(*str & 0x80); str++, len--);
        if (str > str0) {
            if (*nmore) {
                /* output previous, incomplete utf-8 sequence as ASCII "?" */
                fwrite("?", 1, 1, stdwr);
                *nmore = 0, *thiswchar = 0;
            }
            fwrite(str0, 1, str - str0, stdwr);
        }
        /* accumulate lead/trail bytes into *thiswchar */
        for (; len; str++, len--) {
            switch (*str & 0xC0) {
                case 0x80:      /* trail byte */
                    if (*nmore) {
                        (*nmore)--;
                        *thiswchar |= (WCHAR)(unsigned char)(*str & 0x3F) << (6 * *nmore);
                        }
                    else {
                        /* lead byte missing; output unexpected trail byte as ASCII "?" */
                        *nmore = 0;
                        *thiswchar = L'?';
                    }
                    break;
                case 0xC0:      /* lead byte */
                    if (*nmore)
                        /* output previous, incomplete utf-8 sequence as ASCII "?" */
                        fwrite("?", 1, 1, stdwr);
                    if (!(*str & 0x20))
                        *nmore = 1;     /* 2-byte encoding */
                    else if (!(*str & 0x10))
                        *nmore = 2;     /* 3-byte encoding */
                    else if (!(*str & 0x08))
                        *nmore = 3;     /* 4-byte encoding */
                    else
                        *nmore = 0;     /* restricted (> 4) or invalid encodings */
                    if (*nmore)
                        *thiswchar = (WCHAR)(unsigned char)(*str & (0x3F >> *nmore)) << (6 * *nmore);
                    else {
                        /* output invalid encoding as ASCII "?" */
                        *thiswchar = L'?';
                    }
                    break;
                default:        /* cannot happen because *str has MSB set */
                    break;
            }
            /* output wide character if finished */
            if (!*nmore) {
                char mbstr[8];
                int n_mbstr;

                if (!consolecp)
                    consolecp = GetConsoleOutputCP();
                n_mbstr = WideCharToMultiByte(consolecp, 0, thiswchar, 1, mbstr, sizeof mbstr, NULL, NULL);
                if (n_mbstr <= 0)
                    fwrite("?", 1, 1, stdwr);
                else
                    fwrite(mbstr, 1, n_mbstr, stdwr);
                *thiswchar = 0; /* cleanup */
                str++, len--;
                break;
            }
        }
    }
    fflush(stdwr);
}

static int GSDLLCALL
gsdll_stdout_utf8(void *instance, const char *utf8str, int bytelen)
{
    static WCHAR thiswchar = 0; /* accumulates the bits from multiple encoding bytes */
    static int nmore = 0;       /* expected number of additional encoding bytes */

#ifndef NDEBUG
    windows_debug_out(utf8str, bytelen);
#endif

    gsdll_utf8write(stdout, utf8str, bytelen, &thiswchar, &nmore);
    return bytelen;
}

static int GSDLLCALL
gsdll_stderr_utf8(void *instance, const char *utf8str, int bytelen)
{
    static WCHAR thiswchar = 0; /* accumulates the bits from multiple encoding bytes */
    static int nmore = 0;       /* expected number of additional encoding bytes */

#ifndef NDEBUG
    windows_debug_out(utf8str, bytelen);
#endif

    gsdll_utf8write(stderr, utf8str, bytelen, &thiswchar, &nmore);
    return bytelen;
}

/*********************************************************************/
/* dll device */

#ifndef METRO
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
    fprintf(stdout, "display_presize(0x%x 0x%x, %d, %d, %d, %ld)\n",
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
    fprintf(stdout, "display_size(0x%x 0x%x, %d, %d, %d, %ld, 0x%x)\n",
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

/*********************************************************************/

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

static int main_utf8(int argc, char *argv[])
{
    int code, code1;
    int exit_code;
    int exit_status;
    int nargc;
    char **nargv;
    char buf[256];
    char dformat[64];
    char ddpi[64];

    /* Mark us as being 'system dpi aware' to avoid horrid scaling */
    avoid_windows_scale();

    if (!_isatty(fileno(stdin)))
        _setmode(fileno(stdin), _O_BINARY);
    _setmode(fileno(stdout), _O_BINARY);
    _setmode(fileno(stderr), _O_BINARY);

#ifndef METRO
    hwndforeground = GetForegroundWindow();     /* assume this is ours */
#endif
    memset(buf, 0, sizeof(buf));
    if (load_dll(&gsdll, buf, sizeof(buf))) {
        fprintf(stderr, "Can't load Ghostscript DLL\n");
        fprintf(stderr, "%s\n", buf);
        return 1;
    }

    if (gsdll.new_instance(&instance, NULL) < 0) {
        fprintf(stderr, "Can't create Ghostscript instance\n");
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

    gsdll.set_stdio(instance,
        _isatty(fileno(stdin)) ?  gsdll_stdin_utf8 : gsdll_stdin,
        _isatty(fileno(stdout)) ?  gsdll_stdout_utf8 : gsdll_stdout,
        _isatty(fileno(stderr)) ?  gsdll_stderr_utf8 : gsdll_stderr);
#ifdef METRO
    /* For metro, no insertion of display device args */
    nargc = argc;
    nargv = argv;
    {
#else
    gsdll.set_display_callback(instance, &display);

    {   int format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
                DISPLAY_DEPTH_1 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
        HDC hdc = GetDC(NULL);  /* get hdc for desktop */
        int depth = GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL);
        sprintf(ddpi, "-dDisplayResolution=%d", GetDeviceCaps(hdc, LOGPIXELSY));
        ReleaseDC(NULL, hdc);
        if (depth == 32)
            format = DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST |
                DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
        else if (depth == 16)
            format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
                DISPLAY_DEPTH_16 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST |
                DISPLAY_NATIVE_555;
        else if (depth > 8)
            format = DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE |
                DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
        else if (depth >= 8)
            format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
                DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
        else if (depth >= 4)
            format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
                DISPLAY_DEPTH_4 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
        sprintf(dformat, "-dDisplayFormat=%d", format);
    }
    nargc = argc + 2;
    nargv = (char **)malloc(nargc * sizeof(char *));
    if (nargv == NULL) {
        fprintf(stderr, "Malloc failure!\n");
    } else {
        nargv[0] = argv[0];
        nargv[1] = dformat;
        nargv[2] = ddpi;
        memcpy(&nargv[3], &argv[1], (argc-1) * sizeof(char *));
#endif

#if defined(_MSC_VER) || defined(__BORLANDC__)
        __try {
#endif
            code = gsdll.set_arg_encoding(instance, GS_ARG_ENCODING_UTF8);
            if (code == 0)
            code = gsdll.init_with_args(instance, nargc, nargv);
            if (code == 0)
                code = gsdll.run_string(instance, start_string, 0, &exit_code);
            code1 = gsdll.exit(instance);
            if (code == 0 || (code == gs_error_Quit && code1 != 0))
                code = code1;
#if defined(_MSC_VER) || defined(__BORLANDC__)
        } __except(exception_code() == EXCEPTION_STACK_OVERFLOW) {
            code = gs_error_Fatal;
            fprintf(stderr, "*** C stack overflow. Quiting...\n");
        }
#endif

        gsdll.delete_instance(instance);

        unload_dll(&gsdll);

#ifndef METRO
        free(nargv);
#endif
    }
#ifndef METRO
    /* close other thread */
    quitnow = TRUE;
    PostThreadMessage(thread_id, WM_QUIT, 0, (LPARAM)0);
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
    int i, code;

    nargv = calloc(argc, sizeof(nargv[0]));
    if (nargv == NULL)
        goto err;
    for (i=0; i < argc; i++) {
        nargv[i] = malloc(gp_uint16_to_utf8(NULL, argv[i]));
        if (nargv[i] == NULL)
            goto err;
        (void)gp_uint16_to_utf8(nargv[i], argv[i]);
    }
    code = main_utf8(argc, nargv);

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
