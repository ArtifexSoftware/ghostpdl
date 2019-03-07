/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* %pipe% IODevice */
#include "errno_.h"
#include "pipe_.h"
#include "stdio_.h"
#include "string_.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"		/* for gxiodev.h */
#include "gxiodev.h"

/* The pipe IODevice */
static iodev_proc_fopen(pipe_fopen);
static iodev_proc_fclose(pipe_fclose);
const gx_io_device gs_iodev_pipe = {
    "%pipe%", "Special",
    {iodev_no_init, iodev_no_finit, iodev_no_open_device,
     NULL /*iodev_os_open_file */ , pipe_fopen, pipe_fclose,
     iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
     iodev_no_enumerate_files, NULL, NULL,
     iodev_no_get_params, iodev_no_put_params
    }
};

/* The file device procedures */

static int
do_pclose(FILE *file)
{
#ifndef GS_NO_FILESYSTEM
    return pclose(file);
#endif
}

static int
pipe_fopen(gx_io_device * iodev, const char *fname, const char *access,
           gp_file ** pfile, char *rfname, uint rnamelen, gs_memory_t *mem)
{
#ifdef GS_NO_FILESYSTEM
    return 0;
#else
    gp_file *file;

    errno = 0;
    /*
     * Some platforms allow opening a pipe with a '+' in the access
     * mode, even though pipes are not positionable.  Detect this here.
     */
    if (strchr(access, '+'))
        return_error(gs_error_invalidfileaccess);

    file = gp_file_FILE_alloc(mem);
    if (file == NULL) {
        *pfile = NULL;
        return gs_error_VMerror;
    }
    /*
     * The OSF/1 1.3 library doesn't include const in the
     * prototype for popen, so we have to break const here.
     */
    if (gp_file_FILE_set(file, popen((char *)fname, (char *)access), do_pclose)) {
        *pfile = NULL;
        return_error(gs_fopen_errno_to_code(errno));
    } else {
        if (rfname != NULL)
            strcpy(rfname, fname);
        *pfile = file;
    }
    return 0;
#endif
}

static int
pipe_fclose(gx_io_device * iodev, gp_file * file)
{
#ifndef GS_NO_FILESYSTEM
    gp_fclose(file);
#endif
    return 0;
}
