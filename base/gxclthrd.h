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


/* Command list multiple rendering threads */
/* Requires gxsync.h */

#ifndef gxclthrd_INCLUDED
#  define gxclthrd_INCLUDED

#include "gscms.h"
#include "gxdevcli.h"
#include "gxclist.h"

/* clone a device and set params and its chunk memory                   */
/* The chunk_base_mem MUST be thread safe                               */
/* Return NULL on error, or the cloned device with the dev->memory set  */
/* to the chunk allocator.                                              */
/* Exported for use by background printing.                             */
/* When called to setup for background printing, bg_print is true       */
gx_device * setup_device_and_mem_for_thread(gs_memory_t *chunk_base_mem, gx_device *dev, bool bg_print, gsicc_link_cache_t **cachep);

/* Close and free the thread's device, finish the thread, free up the   */
/* thread's memory and its chunk allocator and close the clist files    */
/* if 'unlink' is true, also delete the clist files (for bg printing)   */
/* Exported for use by background printing.                             */
void teardown_device_and_mem_for_thread(gx_device *dev, gp_thread_id thread_id, bool bg_print);

/* Following is used for clist background printing and multi-threaded rendering */
typedef enum {
    THREAD_ERROR = -1,
    THREAD_IDLE = 0,
    THREAD_DONE = 1,
    THREAD_BUSY = 2
} thread_status;

struct clist_render_thread_control_s {
    thread_status status;	/* 0: not started, 1: done, 2: busy, < 0: error */
                                /* values allow waiting until status < 2 */
    gs_memory_t *memory;	/* thread's 'chunk' memory allocator */
    gx_semaphore_t *sema_this;
    gx_semaphore_t *sema_group;
    gx_device *cdev;	/* clist device copy */
    gx_device *bdev;	/* this thread's buffer device */
    int band;
    gp_thread_id thread;

    /* For process_page mode */
    gx_process_page_options_t *options;
    void *buffer;
#ifdef DEBUG
    ulong cputime;
#endif
};

#endif /* gxclthrd_INCLUDED */
