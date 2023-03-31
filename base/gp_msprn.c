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


/* %printer% IODevice */

#include "windows_.h"
#include "errno_.h"
#include "stdio_.h"
#include "string_.h"
#include "ctype_.h"
#include "fcntl_.h"
#include <io.h>
#include "gp.h"
#include "gscdefs.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"		/* for gxiodev.h */
#include "gxiodev.h"

/* The MS-Windows printer IODevice */

/*
 * This allows a MS-Windows printer to be specified as an
 * output using
 *  -sOutputFile="%printer%HP DeskJet 500"
 *
 * To write to a remote printer on another server
 *  -sOutputFile="%printer%\\server\printer name"
 *
 * If you don't supply a printer name you will get
 *  Error: /undefinedfilename in --.outputpage--
 * If the printer name is invalid you will get
 *  Error: /invalidfileaccess in --.outputpage--
 *
 * This is implemented by returning the file pointer
 * for the write end of a pipe, and starting a thread
 * which reads the pipe and writes to a Windows printer.
 *
 * The old method provided by gp_open_printer()
 *  -sOutputFile="\\spool\HP DeskJet 500"
 * should not be used.
 * The "\\spool\" is not a UNC name and causes confusion.
 */

static iodev_proc_init(mswin_printer_init);
static iodev_proc_finit(mswin_printer_finit);
static iodev_proc_fopen(mswin_printer_fopen);
static iodev_proc_fclose(mswin_printer_fclose);
const gx_io_device gs_iodev_printer = {
    "%printer%", "FileSystem",
    {mswin_printer_init, mswin_printer_finit, iodev_no_open_device,
     NULL /*iodev_os_open_file */ , mswin_printer_fopen, mswin_printer_fclose,
     iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
     iodev_no_enumerate_files, NULL, NULL,
     iodev_no_get_params, iodev_no_put_params
    },
    NULL,
    NULL
};

typedef struct tid_s {
    uintptr_t tid;
} tid_t;

void mswin_printer_thread(void *arg)
{
    int fd = (int)(intptr_t)arg;
    char pname[gp_file_name_sizeof];
    char data[4096];
    HANDLE hprinter = INVALID_HANDLE_VALUE;
    int count;
    DWORD written;
    DOC_INFO_1 di;

    /* Read from pipe and write to Windows printer.
     * First gp_file_name_sizeof bytes are name of the printer.
     */
    if (read(fd, pname, sizeof(pname)) != sizeof(pname)) {
        /* Didn't get the printer name */
        close(fd);
        return;
    }

    while ( (count = read(fd, data, sizeof(data))) > 0 ) {
        if (hprinter == INVALID_HANDLE_VALUE) {
            if (!gp_OpenPrinter(pname, &hprinter)) {
                close(fd);
                return;
            }
            di.pDocName = (LPTSTR)gs_product;
            di.pOutputFile = NULL;
            di.pDatatype = "RAW";
            if (!StartDocPrinter(hprinter, 1, (LPBYTE) & di)) {
                AbortPrinter(hprinter);
                close(fd);
                return;
            }
        }
        if (!StartPagePrinter(hprinter)) {
                AbortPrinter(hprinter);
                close(fd);
                return;
        }
        if (!WritePrinter(hprinter, (LPVOID) data, count, &written)) {
            AbortPrinter(hprinter);
            close(fd);
            return;
        }
    }
    if (hprinter != INVALID_HANDLE_VALUE) {
        if (count == 0) {
            /* EOF */
            EndPagePrinter(hprinter);
            EndDocPrinter(hprinter);
            ClosePrinter(hprinter);
        }
        else {
            /* Error */
            AbortPrinter(hprinter);
        }
    }
    close(fd);
}

/* The file device procedures */
static int
mswin_printer_init(gx_io_device * iodev, gs_memory_t * mem)
{
    /* state -> structure containing thread handle */
    iodev->state = gs_alloc_bytes(mem, sizeof(tid_t), "mswin_printer_init");
    if (iodev->state == NULL)
        return_error(gs_error_VMerror);
    ((tid_t *)iodev->state)->tid = -1;
    return 0;
}

static void
mswin_printer_finit(gx_io_device * iodev, gs_memory_t * mem)
{
    gs_free_object(mem, iodev->state, "mswin_printer_finit");
    iodev->state = NULL;
    return;
}

static int
mswin_printer_fopen(gx_io_device * iodev, const char *fname, const char *access,
                    gp_file ** pfile, char *rfname, uint rnamelen, gs_memory_t *mem)
{
    DWORD version = GetVersion();
    HANDLE hprinter;
    int pipeh[2];
    uintptr_t tid;
    HANDLE hthread;
    char pname[gp_file_name_sizeof];
    uintptr_t *ptid = &((tid_t *)(iodev->state))->tid;
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;
    gs_fs_list_t *fs = ctx->core->fs;
    const size_t preflen = strlen(iodev->dname);
    const size_t nlen = strlen(fname);

    if (preflen + nlen >= gp_file_name_sizeof)
        return_error(gs_error_invalidaccess);

    memcpy(pname, iodev->dname, preflen);
    memcpy(pname + preflen, fname, nlen + 1);

    if (gp_validate_path(mem, pname, access) != 0)
        return gs_error_invalidfileaccess;

    /* First we try the open_printer method. */
    /* Note that the loop condition here ensures we don't
     * trigger on the last registered fs entry (our standard
     * 'file' one). */
    if (access[0] == 'w' || access[0] == 'a')
    {
        *pfile = NULL;
        for (fs = ctx->core->fs; fs != NULL && fs->next != NULL; fs = fs->next)
        {
            int code = 0;
            if (fs->fs.open_printer)
                code = fs->fs.open_printer(mem, fs->secret, fname, 1, pfile);
            if (code < 0)
                return code;
            if (*pfile != NULL)
                return code;
        }
    } else
        return gs_error_invalidfileaccess;

    /* If nothing claimed that, then continue with the
     * standard MS way of working. */

    /* Win32s supports neither pipes nor Win32 printers. */
    if (((HIWORD(version) & 0x8000) != 0) &&
        ((HIWORD(version) & 0x4000) == 0))
        return_error(gs_error_invalidfileaccess);

    /* Make sure that printer exists. */
    if (!gp_OpenPrinter((LPTSTR)fname, &hprinter))
        return_error(gs_error_invalidfileaccess);
    ClosePrinter(hprinter);

    *pfile = gp_file_FILE_alloc(mem);
    if (*pfile == NULL)
        return_error(gs_error_VMerror);

    /* Create a pipe to connect a FILE pointer to a Windows printer. */
    if (_pipe(pipeh, 4096, _O_BINARY) != 0) {
        gp_file_dealloc(*pfile);
        *pfile = NULL;
        return_error(gs_fopen_errno_to_code(errno));
    }

    if (gp_file_FILE_set(*pfile, fdopen(pipeh[1], (char *)access), NULL)) {
        *pfile = NULL;
        close(pipeh[0]);
        close(pipeh[1]);
        return_error(gs_fopen_errno_to_code(errno));
    }

    /* start a thread to read the pipe */
    tid = _beginthread(&mswin_printer_thread, 32768, (void *)(intptr_t)(pipeh[0]));
    if (tid == -1) {
        gp_fclose(*pfile);
        *pfile = NULL;
        close(pipeh[0]);
        return_error(gs_error_invalidfileaccess);
    }
    /* Duplicate thread handle so we can wait on it
     * even if original handle is closed by CRTL
     * when the thread finishes.
     */
    if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)tid,
        GetCurrentProcess(), &hthread,
        0, FALSE, DUPLICATE_SAME_ACCESS)) {
        gp_fclose(*pfile);
        *pfile = NULL;
        return_error(gs_error_invalidfileaccess);
    }
    *ptid = (uintptr_t)hthread;

    /* Give the name of the printer to the thread by writing
     * it to the pipe.  This is avoids elaborate thread
     * synchronisation code.
     */
    strncpy(pname, fname, sizeof(pname));
    gp_fwrite(pname, 1, sizeof(pname), *pfile);

    return 0;
}

static int
mswin_printer_fclose(gx_io_device * iodev, gp_file * file)
{
    uintptr_t *ptid = &((tid_t *)(iodev->state))->tid;
    HANDLE hthread;
    gp_fclose(file);
    if (*ptid != -1) {
        /* Wait until the print thread finishes before continuing */
        hthread = (HANDLE)*ptid;
        WaitForSingleObject(hthread, 60000);
        CloseHandle(hthread);
        *ptid = -1;
    }
    return 0;
}
