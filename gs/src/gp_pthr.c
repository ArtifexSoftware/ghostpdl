/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* pthreads interface */
#include "std.h"
#include <pthread.h>
#include "gserror.h"
#include "gserrors.h"
#include "gpsync.h"

int
gp_create_thread(gp_thread_creation_callback_t proc, void *proc_data)
{
    pthread_t ignore_thread;
    pthread_attr_t attr;
    int code;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, 1);
    code = pthread_create(&ignore_thread, &attr, proc, proc_data);
    return (code ? gs_note_error(gs_error_ioerror) : 0);
}
