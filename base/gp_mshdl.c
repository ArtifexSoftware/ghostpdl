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


/* %handle% IODevice */
#include "errno_.h"
#include "stdio_.h"
#include "string_.h"
#include "ctype_.h"
#include <io.h>
#include "gstypes.h"
#include "gsmemory.h"		/* for gxiodev.h */
#include "gxiodev.h"
#include "gserrors.h"

/* The MS-Windows handle IODevice */

/* This allows an MS-Windows file handle to be passed in to
 * Ghostscript as %handle%NNNNNNNN where NNNNNNNN is the hexadecimal
 * value of the handle.
 * The typical use is for another program to create a pipe,
 * pass the write end into Ghostscript using
 *  -sOutputFile="%handle%NNNNNNNN"
 * so that Ghostscript printer output can be captured by the
 * other program.  The handle would be created with CreatePipe().
 * If Ghostscript is not a DLL, the pipe will have to be inheritable
 * by the Ghostscript process.
 */

static iodev_proc_fopen(mswin_handle_fopen);
static iodev_proc_fclose(mswin_handle_fclose);
const gx_io_device gs_iodev_handle = {
    "%handle%", "FileSystem",
    {iodev_no_init, iodev_no_finit, iodev_no_open_device,
     NULL /*iodev_os_open_file */ , mswin_handle_fopen, mswin_handle_fclose,
     iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
     iodev_no_enumerate_files, NULL, NULL,
     iodev_no_get_params, iodev_no_put_params
    },
    NULL,
    NULL
};

/* The file device procedures */
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (-1)
#endif

/* Allow printer filename specified by -sOutputFile="..."
 * to contain a hexadecimal encoded OS file handle in the
 * form -sOutputFile="\\handle\\000001c5"
 *
 * Returns:
 *   The OS file handle on success.
 *   INVALID_HANDLE_VALUE on failure.
 *
 * This allows the caller to create an OS pipe and give us a
 * handle to the write end of the pipe.
 * If we are called as an EXE, the OS file handle must be
 * inherited by Ghostscript.
 * Pipes aren't supported under Win32s.
 */
static long
get_os_handle(const char *name)
{
    ulong hfile;	/* This must be as long as the longest handle. */
                        /* This is correct for Win32, maybe wrong for Win64. */
    int i, ch;

    for (i = 0; (ch = name[i]) != 0; ++i)
        if (!isxdigit(ch))
            return (long)INVALID_HANDLE_VALUE;
    if (sscanf(name, "%lx", &hfile) != 1)
        return (long)INVALID_HANDLE_VALUE;
    return (long)hfile;
}

static int
mswin_handle_fopen(gx_io_device * iodev, const char *fname, const char *access,
                   gp_file ** pfile, char *rfname, uint rnamelen, gs_memory_t *mem)
{
    int fd;
    long hfile;	/* Correct for Win32, may be wrong for Win64 */
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;
    gs_fs_list_t *fs = ctx->core->fs;
    char f[gp_file_name_sizeof];
    const size_t preflen = strlen(iodev->dname);
    const size_t nlen = strlen(fname);

    if (preflen + nlen >= gp_file_name_sizeof)
        return_error(gs_error_invalidaccess);

    memcpy(f, iodev->dname, preflen);
    memcpy(f + preflen, fname, nlen + 1);

    if (gp_validate_path(mem, f, access) != 0)
        return gs_error_invalidfileaccess;

    /* First we try the open_handle method. */
    /* Note that the loop condition here ensures we don't
     * trigger on the last registered fs entry (our standard
     * 'file' one). */
    *pfile = NULL;
    for (fs = ctx->core->fs; fs != NULL && fs->next != NULL; fs = fs->next)
    {
        int code = 0;
        if (fs->fs.open_handle)
            code = fs->fs.open_handle(mem, fs->secret, fname, access, pfile);
        if (code < 0)
            return code;
        if (*pfile != NULL)
            return code;
    }

    /* If nothing claimed that, then continue with the
     * standard MS way of working. */
    errno = 0;
    *pfile = gp_file_FILE_alloc(mem);
    if (*pfile == NULL) {
        return gs_error_VMerror;
    }

    if ((hfile = get_os_handle(fname)) == (long)INVALID_HANDLE_VALUE) {
        gp_file_dealloc(*pfile);
        return_error(gs_fopen_errno_to_code(EBADF));
    }

    /* associate a C file handle with an OS file handle */
    fd = _open_osfhandle((long)hfile, 0);
    if (fd == -1) {
        gp_file_dealloc(*pfile);
        return_error(gs_fopen_errno_to_code(EBADF));
    }

    /* associate a C file stream with C file handle */
    if (gp_file_FILE_set(*pfile, fdopen(fd, (char *)access), NULL)) {
        *pfile = NULL;
        return_error(gs_fopen_errno_to_code(errno));
    }

    if (rfname != NULL)
        strcpy(rfname, fname);

    return 0;
}

static int
mswin_handle_fclose(gx_io_device * iodev, gp_file * file)
{
    gp_fclose(file);
    return 0;
}
