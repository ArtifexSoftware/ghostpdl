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


/* %pipe% IODevice */
#include "errno_.h"
#include "pipe_.h"
#include "stdio_.h"
#include "string_.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"		/* for gxiodev.h */
#include "gxiodev.h"

/* The file device procedures */

static int
do_pclose(FILE *file)
{
#ifdef GS_NO_FILESYSTEM
    return gs_error_ok;
#else
    int status = pclose(file);
    if (status < 0 || status > 0)
	    return_error(gs_error_ioerror);

    return gs_error_ok;
#endif
}

static int
fs_file_open_pipe(const gs_memory_t *mem, void *secret, const char *fname, char *rfname, const char *mode, gp_file **file)
{
    *file = gp_file_FILE_alloc(mem);
    if (*file == NULL)
        return gs_error_VMerror;

    errno = 0;
    /*
     * The OSF/1 1.3 library doesn't include const in the
     * prototype for popen, so we have to break const here.
     */
    if (gp_file_FILE_set(*file, popen((char *)fname, (char *)mode), do_pclose)) {
        *file = NULL;
        return_error(gs_fopen_errno_to_code(errno));
    }

    if (rfname != NULL && rfname != fname)
        strcpy(rfname, fname);

    return 0;
}

static int
pipe_fopen(gx_io_device * iodev, const char *fname, const char *access,
           gp_file ** pfile, char *rfname, uint rnamelen, gs_memory_t *mem)
{
#ifdef GS_NO_FILESYSTEM
    return 0;
#else
    gs_lib_ctx_t *ctx = mem->gs_lib_ctx;
    gs_fs_list_t *fs = ctx->core->fs;
    /* The pipe device can be reached in two ways, explicltly with %pipe%
       or implicitly with "|", so we have to check for both
     */
    char f[gp_file_name_sizeof];
    const char *pipestr = "|";
    const size_t pipestrlen = strlen(pipestr);
    const size_t preflen = strlen(iodev->dname);
    const size_t nlen = strlen(fname);
    int code1;

    if (preflen + nlen >= gp_file_name_sizeof)
        return_error(gs_error_invalidaccess);

    memcpy(f, iodev->dname, preflen);
    memcpy(f + preflen, fname, nlen + 1);

    code1 = gp_validate_path(mem, f, access);

    memcpy(f, pipestr, pipestrlen);
    memcpy(f + pipestrlen, fname, nlen + 1);

    if (code1 != 0 && gp_validate_path(mem, f, access) != 0 )
        return gs_error_invalidfileaccess;

    /*
     * Some platforms allow opening a pipe with a '+' in the access
     * mode, even though pipes are not positionable.  Detect this here.
     */
    if (strchr(access, '+'))
        return_error(gs_error_invalidfileaccess);

    *pfile = NULL;
    for (fs = ctx->core->fs; fs != NULL; fs = fs->next)
    {
        int code = 0;
        if (fs->fs.open_pipe)
            code = fs->fs.open_pipe(mem, fs->secret, fname, rfname, access, pfile);
        if (code < 0)
            return code;
        if (*pfile != NULL)
            break;
    }

    return 0;
#endif
}

static int
pipe_fclose(gx_io_device * iodev, gp_file * file)
{
#ifdef GS_NO_FILESYSTEM
    return 0;
#else
    return gp_fclose(file);
#endif
}

static int
pipe_init(gx_io_device * iodev, gs_memory_t * mem)
{
    gs_fs_list_t *fs = mem->gs_lib_ctx->core->fs;

    while (fs->next)
        fs = fs->next;

    /* Last one is out file device */
    fs->fs.open_pipe = fs_file_open_pipe;

    return 0;
}

/* The pipe IODevice */
const gx_io_device gs_iodev_pipe = {
    "%pipe%", "Special",
    {pipe_init, iodev_no_finit, iodev_no_open_device,
     NULL /*iodev_os_open_file */ , pipe_fopen, pipe_fclose,
     iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
     iodev_no_enumerate_files, NULL, NULL,
     iodev_no_get_params, iodev_no_put_params
    },
    NULL,
    NULL
};

