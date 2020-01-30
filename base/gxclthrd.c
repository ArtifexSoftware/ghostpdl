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


/* Command list - Support for multiple rendering threads */
#include "memory_.h"
#include "gx.h"
#include "gp.h"
#include "gpcheck.h"
#include "gxsync.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsdevice.h"
#include "gxdevmem.h"           /* must precede gxcldev.h */
#include "gdevprn.h"            /* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxgetbit.h"
#include "gdevplnx.h"
#include "gdevppla.h"
#include "gsmemory.h"
#include "gsmchunk.h"
#include "gxclthrd.h"
#include "gdevdevn.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gstrans.h"
#include "gzht.h"		/* for gx_ht_cache_default_bits_size */

/* Forward reference prototypes */
static int clist_start_render_thread(gx_device *dev, int thread_index, int band);
static void clist_render_thread(void *param);

/* clone a device and set params and its chunk memory                   */
/* The chunk_base_mem MUST be thread safe                               */
/* Return NULL on error, or the cloned device with the dev->memory set  */
/* to the chunk allocator.                                              */
/* Exported for use by background printing.                             */
gx_device *
setup_device_and_mem_for_thread(gs_memory_t *chunk_base_mem, gx_device *dev, bool bg_print, gsicc_link_cache_t **cachep)
{
    int i, code;
    char fmode[4];
    gs_memory_t *thread_mem;
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_printer *pdev = (gx_device_printer *)dev;
    gx_device_clist_common *cdev = (gx_device_clist_common *)cldev;
    gx_device *ndev;
    gx_device_clist *ncldev;
    gx_device_clist_common *ncdev;
    gx_device_printer *npdev;
    gx_device *protodev;
    gs_c_param_list paramlist;
    gs_devn_params *pclist_devn_params;
    gx_device_buf_space_t buf_space;
    ulong state_size;


    /* Every thread will have a 'chunk allocator' to reduce the interaction
     * with the 'base' allocator which has 'mutex' (locking) protection.
     * This improves performance of the threads.
     */
    if ((code = gs_memory_chunk_wrap(&(thread_mem), chunk_base_mem )) < 0) {
        emprintf1(dev->memory, "chunk_wrap returned error code: %d\n", code);
        return NULL;
    }
    /* Find the prototype for this device (needed so we can copy from it) */
    for (i=0; (protodev = (gx_device *)gs_getdevice(i)) != NULL; i++)
        if (strcmp(protodev->dname, dev->dname) == 0)
            break;

    /* Clone the device from the prototype device */
    if (protodev == NULL ||
        (code = gs_copydevice((gx_device **) &ndev, protodev, thread_mem)) < 0 ||
        ndev == NULL) {				/* should only happen if copydevice failed */
        gs_memory_chunk_release(thread_mem);
        return NULL;
    }
    ncldev = (gx_device_clist *)ndev;
    ncdev = (gx_device_clist_common *)ndev;
    npdev = (gx_device_printer *)ndev;
    gx_device_fill_in_procs(ndev);
    ((gx_device_printer *)ncdev)->buffer_memory =
        ncdev->memory =
            ncdev->bandlist_memory =
               thread_mem;
    ndev->PageCount = dev->PageCount;       /* copy to prevent mismatch error */
    npdev->file = pdev->file;               /* For background printing when doing N copies with %d */
    strcpy((npdev->fname), (pdev->fname));
    ndev->color_info = dev->color_info;     /* copy before putdeviceparams */
    ndev->pad = dev->pad;
    ndev->log2_align_mod = dev->log2_align_mod;
    ndev->is_planar = dev->is_planar;
    ndev->icc_struct = NULL;

    /* If the device ICC profile (or proof) is OI_PROFILE, then that was not handled
     * by put/get params, and we cannot share the profiles between the 'parent' output device
     * and the devices created for each thread. Thus we also cannot share the icc_struct.
     * In this case we need to create a new icc_struct and clone the profiles.  The clone
     * operation also initializes some of the required data
     * We need to do this *before* the gs_getdeviceparams/gs_putdeviceparams so gs_putdeviceparams
     * will spot the same profile being used, and treat it as a no-op. Otherwise it will try to find
     * a profile with the 'special' name "OI_PROFILE" and throw an error.
     */
    if (!gscms_is_threadsafe() || (dev->icc_struct != NULL &&
        ((dev->icc_struct->device_profile[0] != NULL &&
          strncmp(dev->icc_struct->device_profile[0]->name, OI_PROFILE, strlen(OI_PROFILE)) == 0)
        || (dev->icc_struct->proof_profile != NULL &&
        strncmp(dev->icc_struct->proof_profile->name, OI_PROFILE, strlen(OI_PROFILE)) == 0)))) {
        ndev->icc_struct = gsicc_new_device_profile_array(ndev->memory);
        if (!ndev->icc_struct) {
            emprintf1(ndev->memory,
                  "Error setting up device profile array, code=%d. Rendering threads not started.\n",
                  code);
            goto out_cleanup;
        }
        if ((code = gsicc_clone_profile(dev->icc_struct->device_profile[0],
            &(ndev->icc_struct->device_profile[0]), ndev->memory)) < 0) {
            emprintf1(dev->memory,
                "Error setting up device profile, code=%d. Rendering threads not started.\n",
                code);
            goto out_cleanup;
        }
        if (dev->icc_struct->proof_profile &&
           (code = gsicc_clone_profile(dev->icc_struct->proof_profile,
            &ndev->icc_struct->proof_profile, ndev->memory)) < 0) {
            emprintf1(dev->memory,
                "Error setting up proof profile, code=%d. Rendering threads not started.\n",
                code);
            goto out_cleanup;

        }
    }
    else {
        /* safe to share the icc_struct among threads */
        ndev->icc_struct = dev->icc_struct;  /* Set before put params */
        rc_increment(ndev->icc_struct);
    }
    /* get the current device parameters to put into the cloned device */
    gs_c_param_list_write(&paramlist, thread_mem);
    if ((code = gs_getdeviceparams(dev, (gs_param_list *)&paramlist)) < 0) {
        emprintf1(dev->memory,
                  "Error getting device params, code=%d. Rendering threads not started.\n",
                  code);
        goto out_cleanup;
    }
    gs_c_param_list_read(&paramlist);
    if ((code = gs_putdeviceparams(ndev, (gs_param_list *)&paramlist)) < 0)
        goto out_cleanup;
    gs_c_param_list_release(&paramlist);

    /* In the case of a separation device, we need to make sure we get the
       devn params copied over */
    pclist_devn_params = dev_proc(dev, ret_devn_params)(dev);
    if (pclist_devn_params != NULL) {
        code = devn_copy_params(dev, ndev);
        if (code < 0) {
#ifdef DEBUG /* suppress a warning on a release build */
            (void)gs_note_error(gs_error_VMerror);
#endif
            goto out_cleanup;
        }
    }
    /* Also make sure supports_devn is set correctly */
    ndev->icc_struct->supports_devn = cdev->icc_struct->supports_devn;
    ncdev->page_uses_transparency = cdev->page_uses_transparency;
    if_debug3m(gs_debug_flag_icc, cdev->memory,
               "[icc] MT clist device = 0x%p profile = 0x%p handle = 0x%p\n",
               ncdev,
               ncdev->icc_struct->device_profile[0],
               ncdev->icc_struct->device_profile[0]->profile_handle);
    /* If the device is_planar, then set the flag in the new_device and the procs */
    if ((ncdev->is_planar = cdev->is_planar))
        gdev_prn_set_procs_planar(ndev);

    /* Make sure that the ncdev BandHeight matches what we used when writing the clist, but
     * re-calculate the BandBufferSpace so we don't over-allocate (in case the page uses
     * transparency so that the BandHeight was reduced.)
     */
    ncdev->space_params.band = cdev->page_info.band_params;
    ncdev->space_params.banding_type = BandingAlways;
    code = npdev->printer_procs.buf_procs.size_buf_device
                (&buf_space, (gx_device *)ncdev, NULL, ncdev->space_params.band.BandHeight, false);
    /* The 100 is bogus, we are just matching what is in clist_init_states */
    state_size = cdev->nbands * (ulong) sizeof(gx_clist_state) + sizeof(cmd_prefix) + cmd_largest_size + 100;
    ncdev->space_params.band.BandBufferSpace = buf_space.bits + buf_space.line_ptrs;
    if (state_size > ncdev->space_params.band.BandBufferSpace)
        ncdev->space_params.band.BandBufferSpace = state_size;
    ncdev->space_params.band.tile_cache_size = cdev->page_info.tile_cache_size;	/* must be the same */
    ncdev->space_params.band.BandBufferSpace += cdev->page_info.tile_cache_size;

    /* gdev_prn_allocate_memory sets the clist for writing, creating new files.
     * We need  to unlink those files and open the main thread's files, then
     * reset the clist state for reading/rendering
     */
    if ((code = gdev_prn_allocate_memory(ndev, NULL, ndev->width, ndev->height)) < 0)
        goto out_cleanup;

    if (ncdev->page_info.tile_cache_size != cdev->page_info.tile_cache_size) {
        emprintf2(thread_mem,
                   "clist_setup_render_threads: tile_cache_size mismatch. New size=%d, should be %d\n",
                   ncdev->page_info.tile_cache_size, cdev->page_info.tile_cache_size);
        goto out_cleanup;
    }

    /* close and unlink the temp files just created */
    ncdev->page_info.io_procs->fclose(ncdev->page_info.cfile, ncdev->page_info.cfname, true);
    ncdev->page_info.io_procs->fclose(ncdev->page_info.bfile, ncdev->page_info.bfname, true);
    ncdev->page_info.cfile = ncdev->page_info.bfile = NULL;

    /* open the main thread's files for this thread */
    strcpy(fmode, "r");                 /* read access for threads */
    strncat(fmode, gp_fmode_binary_suffix, 1);
    if ((code=cdev->page_info.io_procs->fopen(cdev->page_info.cfname, fmode, &ncdev->page_info.cfile,
                        thread_mem, thread_mem, true)) < 0 ||
         (code=cdev->page_info.io_procs->fopen(cdev->page_info.bfname, fmode, &ncdev->page_info.bfile,
                        thread_mem, thread_mem, false)) < 0)
        goto out_cleanup;

    strcpy((ncdev->page_info.cfname), (cdev->page_info.cfname));
    strcpy((ncdev->page_info.bfname), (cdev->page_info.bfname));
    clist_render_init(ncldev);      /* Initialize clist device for reading */
    ncdev->page_info.bfile_end_pos = cdev->page_info.bfile_end_pos;

    /* The threads are maintained until clist_finish_page.  At which
       point, the threads are torn down, the master clist reader device
       is changed to writer, and the icc_table and the icc_cache_cl freed */
    if (dev->icc_struct == ndev->icc_struct) {
    /* safe to share the link cache */
        ncdev->icc_cache_cl = cdev->icc_cache_cl;
        rc_increment(cdev->icc_cache_cl);		/* FIXME: needs to be incdemented safely */
    } else {
        /* each thread needs its own link cache */
        if (cachep != NULL) {
            if (*cachep == NULL) {
                /* We don't have one cached that we can reuse, so make one. */
                if ((*cachep = gsicc_cache_new(thread_mem->thread_safe_memory)) == NULL)
                    goto out_cleanup;
            }
            rc_increment(*cachep);
                ncdev->icc_cache_cl = *cachep;
        } else if ((ncdev->icc_cache_cl = gsicc_cache_new(thread_mem)) == NULL)
            goto out_cleanup;
    }
    if (bg_print) {
        gx_device_clist_reader *ncrdev = (gx_device_clist_reader *)ncdev;

        if (cdev->icc_table != NULL) {
            /* This is a background printing thread, so it cannot share the icc_table  */
            /* since this probably was created with a GC'ed allocator and the bg_print */
            /* thread can't deal with the relocation. Free the cdev->icc_table and get */
            /* a new one from the clist.                                               */
            clist_free_icc_table(cdev->icc_table, cdev->memory);
            cdev->icc_table = NULL;
            if ((code = clist_read_icctable((gx_device_clist_reader *)ncdev)) < 0)
                goto out_cleanup;
        }
        /* Similarly for the color_usage_array, when the foreground device switches to */
        /* writer mode, the foreground's array will be freed.                          */
        if ((code = clist_read_color_usage_array(ncrdev)) < 0)
            goto out_cleanup;
    } else {
    /* Use the same profile table and color usage array in each thread */
        ncdev->icc_table = cdev->icc_table;		/* OK for multiple rendering threads */
        ((gx_device_clist_reader *)ncdev)->color_usage_array =
                ((gx_device_clist_reader *)cdev)->color_usage_array;
    }
    /* Needed for case when the target has cielab profile and pdf14 device
       has a RGB profile stored in the profile list of the clist */
    ncdev->trans_dev_icc_hash = cdev->trans_dev_icc_hash;

    /* success */
    return ndev;

out_cleanup:
    /* Close the file handles, but don't delete (unlink) the files */
    if (ncdev->page_info.bfile != NULL)
        ncdev->page_info.io_procs->fclose(ncdev->page_info.bfile, ncdev->page_info.bfname, false);
    if (ncdev->page_info.cfile != NULL)
        ncdev->page_info.io_procs->fclose(ncdev->page_info.cfile, ncdev->page_info.cfname, false);
    ncdev->do_not_open_or_close_bandfiles = true; /* we already closed the files */

    /* we can't get here with ndev == NULL */
    gdev_prn_free_memory(ndev);
    gs_free_object(thread_mem, ndev, "setup_device_and_mem_for_thread");
    gs_memory_chunk_release(thread_mem);
    return NULL;
}

/* Set up and start the render threads */
static int
clist_setup_render_threads(gx_device *dev, int y, gx_process_page_options_t *options)
{
    gx_device_printer *pdev = (gx_device_printer *)dev;
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_common *cdev = (gx_device_clist_common *)cldev;
    gx_device_clist_reader *crdev = &cldev->reader;
    gs_memory_t *mem = cdev->bandlist_memory;
    gs_memory_t *chunk_base_mem = mem->thread_safe_memory;
    gs_memory_status_t mem_status;
    int i, j, band;
    int code = 0;
    int band_count = cdev->nbands;
    int band_height = crdev->page_info.band_params.BandHeight;
    byte **reserve_memory_array = NULL;
    int reserve_pdf14_memory_size = 0;
    /* space for the halftone cache plus 2Mb for other allocations during rendering (paths, etc.) */
    /* this will be increased by the measured profile storage and icclinks (estimated).		  */
    int reserve_size = 2 * 1024 * 1024 + (gx_ht_cache_default_bits_size() * dev->color_info.num_components);
    clist_icctable_entry_t *curr_entry;
    bool deep = device_is_deep(dev);

    crdev->num_render_threads = pdev->num_render_threads_requested;

    if(gs_debug[':'] != 0)
        dmprintf1(mem, "%% %d rendering threads requested.\n", pdev->num_render_threads_requested);

    if (crdev->page_uses_transparency) {
        reserve_pdf14_memory_size = (ESTIMATED_PDF14_ROW_SPACE(max(1, crdev->width), crdev->color_info.num_components, deep ? 16 : 8) >> 3);
        reserve_pdf14_memory_size *= crdev->page_info.band_params.BandHeight;	/* BandHeight set by writer */
    }
    /* scan the profile table sizes to get the total each thread will need */
    if (crdev->icc_table != NULL) {
        for (curr_entry = crdev->icc_table->head; curr_entry != NULL; curr_entry = curr_entry->next) {
            reserve_size += curr_entry->serial_data.size;
            /* FIXME: Should actually measure the icclink size to device (or pdf14 blend space) */
            reserve_size += 2 * 1024 * 1024;		/* a worst case estimate */
        }
    }
    if (crdev->num_render_threads > band_count)
        crdev->num_render_threads = band_count; /* don't bother starting more threads than bands */
    /* don't exceed our limit (allow for BGPrint and main thread) */
    if (crdev->num_render_threads > MAX_THREADS - 2)
        crdev->num_render_threads = MAX_THREADS - 2;

    /* Allocate and initialize an array of thread control structures */
    crdev->render_threads = (clist_render_thread_control_t *)
              gs_alloc_byte_array(mem, crdev->num_render_threads,
                                  sizeof(clist_render_thread_control_t),
                                  "clist_setup_render_threads");
    /* fallback to non-threaded if allocation fails */
    if (crdev->render_threads == NULL) {
        emprintf(mem, " VMerror prevented threads from starting.\n");
        return_error(gs_error_VMerror);
    }
    reserve_memory_array = (byte **)gs_alloc_byte_array(mem,
                                                        crdev->num_render_threads,
                                                        sizeof(void *),
                                                        "clist_setup_render_threads");
    if (reserve_memory_array == NULL) {
        gs_free_object(mem, crdev->render_threads, "clist_setup_render_threads");
        crdev->render_threads = NULL;
        emprintf(mem, " VMerror prevented threads from starting.\n");
        return_error(gs_error_VMerror);
    }
    memset(reserve_memory_array, 0, crdev->num_render_threads * sizeof(void *));
    memset(crdev->render_threads, 0, crdev->num_render_threads *
            sizeof(clist_render_thread_control_t));

    crdev->main_thread_data = cdev->data;               /* save data area */
    /* Based on the line number requested, decide the order of band rendering */
    /* Almost all devices go in increasing line order (except the bmp* devices ) */
    crdev->thread_lookahead_direction = (y < (cdev->height - 1)) ? 1 : -1;
    band = y / band_height;

    /* If the 'mem' is not thread safe, we need to wrap it in a locking memory */
    gs_memory_status(chunk_base_mem, &mem_status);
    if (mem_status.is_thread_safe == false) {
            return_error(gs_error_VMerror);
    }

    /* If we don't have one large enough already, create an icc cache list */
    if (crdev->num_render_threads > crdev->icc_cache_list_len) {
        gsicc_link_cache_t **old = crdev->icc_cache_list;
        crdev->icc_cache_list = (gsicc_link_cache_t **)gs_alloc_byte_array(mem->thread_safe_memory,
                                    crdev->num_render_threads,
                                    sizeof(void*), "clist_render_setup_threads");
        if (crdev->icc_cache_list == NULL) {
            crdev->icc_cache_list = NULL;
            return_error(gs_error_VMerror);
        }
        if (crdev->icc_cache_list_len > 0)
            memcpy(crdev->icc_cache_list, old, crdev->icc_cache_list_len * sizeof(gsicc_link_cache_t *));
        memset(&(crdev->icc_cache_list[crdev->icc_cache_list_len]), 0,
            (crdev->num_render_threads - crdev->icc_cache_list_len) * sizeof(void *));
        crdev->icc_cache_list_len = crdev->num_render_threads;
        gs_free_object(mem, old, "clist_render_setup_threads");
    }

    /* Loop creating the devices and semaphores for each thread, then start them */
    for (i=0; (i < crdev->num_render_threads) && (band >= 0) && (band < band_count);
            i++, band += crdev->thread_lookahead_direction) {
        gx_device *ndev;
        clist_render_thread_control_t *thread = &(crdev->render_threads[i]);

        /* arbitrary extra reserve for other allocation by threads (paths, etc.) */
        /* plus the amount estimated for the pdf14 buffers */
        reserve_memory_array[i] = (byte *)gs_alloc_bytes(mem, reserve_size + reserve_pdf14_memory_size,
                                                         "clist_render_setup_threads");
        if (reserve_memory_array[i] == NULL) {
            code = gs_error_VMerror;	/* set code to an error for cleanup after the loop */
        break;
        }
        ndev = setup_device_and_mem_for_thread(chunk_base_mem, dev, false, &crdev->icc_cache_list[i]);
        if (ndev == NULL) {
            code = gs_error_VMerror;	/* set code to an error for cleanup after the loop */
            break;
        }

        thread->cdev = ndev;
        thread->memory = ndev->memory;
        thread->band = -1;              /* a value that won't match any valid band */
        thread->options = options;
        thread->buffer = NULL;
        if (options && options->init_buffer_fn) {
            code = options->init_buffer_fn(options->arg, dev, thread->memory, dev->width, band_height, &thread->buffer);
            if (code < 0)
                break;
        }

        /* create the buf device for this thread, and allocate the semaphores */
        if ((code = gdev_create_buf_device(cdev->buf_procs.create_buf_device,
                                &(thread->bdev), ndev,
                                band*crdev->page_band_height, NULL,
                                thread->memory, &(crdev->color_usage_array[0]))) < 0)
            break;
        if ((thread->sema_this = gx_semaphore_label(gx_semaphore_alloc(thread->memory), "Band")) == NULL ||
            (thread->sema_group = gx_semaphore_label(gx_semaphore_alloc(thread->memory), "Group")) == NULL) {
            code = gs_error_VMerror;
            break;
        }
        /* We don't start the threads yet until we  free up the */
        /* reserve memory we have allocated for that band. */
        thread->band = band;
    }
    /* If the code < 0, the last thread creation failed -- clean it up */
    if (code < 0) {
        /* NB: 'band' will be the one that failed, so will be the next_band needed to start */
        /* the following relies on 'free' ignoring NULL pointers */
        gx_semaphore_free(crdev->render_threads[i].sema_group);
        gx_semaphore_free(crdev->render_threads[i].sema_this);
        if (crdev->render_threads[i].bdev != NULL)
            cdev->buf_procs.destroy_buf_device(crdev->render_threads[i].bdev);
        if (crdev->render_threads[i].cdev != NULL) {
            gx_device_clist_common *thread_cdev = (gx_device_clist_common *)crdev->render_threads[i].cdev;

            /* Close the file handles, but don't delete (unlink) the files */
            thread_cdev->page_info.io_procs->fclose(thread_cdev->page_info.bfile, thread_cdev->page_info.bfname, false);
            thread_cdev->page_info.io_procs->fclose(thread_cdev->page_info.cfile, thread_cdev->page_info.cfname, false);
            thread_cdev->do_not_open_or_close_bandfiles = true; /* we already closed the files */

            gdev_prn_free_memory((gx_device *)thread_cdev);
            gs_free_object(crdev->render_threads[i].memory, thread_cdev,
            "clist_setup_render_threads");
        }
        if (crdev->render_threads[i].buffer != NULL && options && options->free_buffer_fn != NULL) {
            options->free_buffer_fn(options->arg, dev, crdev->render_threads[i].memory, crdev->render_threads[i].buffer);
            crdev->render_threads[i].buffer = NULL;
        }
        if (crdev->render_threads[i].memory != NULL) {
            gs_memory_chunk_release(crdev->render_threads[i].memory);
            crdev->render_threads[i].memory = NULL;
        }
    }
    /* If we weren't able to create at least one thread, punt   */
    /* Although a single thread isn't any more efficient, the   */
    /* machinery still works, so that's OK.                     */
    if (i == 0) {
        if (crdev->render_threads[0].memory != NULL) {
            gs_memory_chunk_release(crdev->render_threads[0].memory);
            if (chunk_base_mem != mem) {
                gs_free_object(mem, chunk_base_mem, "clist_setup_render_threads(locked allocator)");
            }
        }
        gs_free_object(mem, crdev->render_threads, "clist_setup_render_threads");
        crdev->render_threads = NULL;
        /* restore the file pointers */
        if (cdev->page_info.cfile == NULL) {
            char fmode[4];

            strcpy(fmode, "a+");        /* file already exists and we want to re-use it */
            strncat(fmode, gp_fmode_binary_suffix, 1);
            cdev->page_info.io_procs->fopen(cdev->page_info.cfname, fmode, &cdev->page_info.cfile,
                                mem, cdev->bandlist_memory, true);
            cdev->page_info.io_procs->fseek(cdev->page_info.cfile, 0, SEEK_SET, cdev->page_info.cfname);
            cdev->page_info.io_procs->fopen(cdev->page_info.bfname, fmode, &cdev->page_info.bfile,
                                mem, cdev->bandlist_memory, false);
            cdev->page_info.io_procs->fseek(cdev->page_info.bfile, 0, SEEK_SET, cdev->page_info.bfname);
        }
        emprintf1(mem, "Rendering threads not started, code=%d.\n", code);
        return_error(code);
    }
    /* Free up any "reserve" memory we may have allocated, and start the
     * threads since we deferred that in the thread setup loop above.
     * We know if we get here we can start at least 1 thread.
     */
    for (j=0, code = 0; j<crdev->num_render_threads; j++) {
        gs_free_object(mem, reserve_memory_array[j], "clist_setup_render_threads");
        if (code == 0 && j < i)
            code = clist_start_render_thread(dev, j, crdev->render_threads[j].band);
    }
    gs_free_object(mem, reserve_memory_array, "clist_setup_render_threads");
    crdev->num_render_threads = i;
    crdev->curr_render_thread = 0;
    crdev->next_band = band;

    if(gs_debug[':'] != 0)
        dmprintf1(mem, "%% Using %d rendering threads\n", i);

    return code;
}

/* This is also exported for teardown after background printing */
void
teardown_device_and_mem_for_thread(gx_device *dev, gp_thread_id thread_id, bool bg_print)
{
    gx_device_clist_common *thread_cdev = (gx_device_clist_common *)dev;
    gx_device_clist_reader *thread_crdev = (gx_device_clist_reader *)dev;
    gs_memory_t *thread_memory = dev->memory;

    /* First finish the thread */
    gp_thread_finish(thread_id);

    if (bg_print) {
        /* we are cleaning up a background printing thread, so we clean up similarly to */
        /* what is done  by clist_finish_page, but without re-opening the clist files.  */
        clist_teardown_render_threads(dev);	/* we may have used multiple threads */
        /* free the thread's icc_table since this was not done by clist_finish_page */
        clist_free_icc_table(thread_crdev->icc_table, thread_memory);
        thread_crdev->icc_table = NULL;
        /* NB: gdev_prn_free_memory below will free the color_usage_array */
    } else {
        /* make sure this doesn't get freed by gdev_prn_free_memory below */
        ((gx_device_clist_reader *)thread_cdev)->color_usage_array = NULL;

        /* For non-bg_print cases the icc_table is shared between devices, but
         * is not reference counted or anything. We rely on it being shared with
         * and owned by the "parent" device in the interpreter thread, hence
         * null it here to avoid it being freed as we cleanup the thread device.
         */
        thread_crdev->icc_table = NULL;
    }
    rc_decrement(thread_crdev->icc_cache_cl, "teardown_render_thread");
    thread_crdev->icc_cache_cl = NULL;
    /*
     * Free the BufferSpace, close the band files, optionally unlinking them.
     * We unlink the files if this call is cleaning up from bg printing.
     * Note that the BufferSpace is freed using 'ppdev->buf' so the 'data'
     * pointer doesn't need to be the one that the thread started with
     */
    /* If this thread was being used for background printing and NumRenderingThreads > 0 */
    /* the clist_setup_render_threads may have already closed these files                */
    /* Note that in the case of back ground printing, we only want to close the instance  */
    /* of the files for the reader (hence the final parameter being false). We'll clean  */
    /* the original instance of the files in prn_finish_bg_print()                       */
    if (thread_cdev->page_info.bfile != NULL)
        thread_cdev->page_info.io_procs->fclose(thread_cdev->page_info.bfile, thread_cdev->page_info.bfname, false);
    if (thread_cdev->page_info.cfile != NULL)
        thread_cdev->page_info.io_procs->fclose(thread_cdev->page_info.cfile, thread_cdev->page_info.cfname, false);
    thread_cdev->page_info.bfile = thread_cdev->page_info.cfile = NULL;
    thread_cdev->do_not_open_or_close_bandfiles = true; /* we already closed the files */

    gdev_prn_free_memory((gx_device *)thread_cdev);
    /* Free the device copy this thread used.  Note that the
       deviceN stuff if was allocated and copied earlier for the device
       will be freed with this call and the icc_struct ref count will be decremented. */
    gs_free_object(thread_memory, thread_cdev, "clist_teardown_render_threads");
#ifdef DEBUG
    dmprintf(thread_memory, "rendering thread ending memory state...\n");
    gs_memory_chunk_dump_memory(thread_memory);
    dmprintf(thread_memory, "                                    memory dump done.\n");
#endif
    gs_memory_chunk_release(thread_memory);
}

void
clist_teardown_render_threads(gx_device *dev)
{
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_common *cdev = (gx_device_clist_common *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    gs_memory_t *mem = cdev->bandlist_memory;
    int i;

    if (crdev->render_threads != NULL) {
        /* Wait for all threads to finish */
        for (i = (crdev->num_render_threads - 1); i >= 0; i--) {
            clist_render_thread_control_t *thread = &(crdev->render_threads[i]);

            if (thread->status == THREAD_BUSY)
                gx_semaphore_wait(thread->sema_this);
        }
        /* then free each thread's memory */
        for (i = (crdev->num_render_threads - 1); i >= 0; i--) {
            clist_render_thread_control_t *thread = &(crdev->render_threads[i]);
            gx_device_clist_common *thread_cdev = (gx_device_clist_common *)thread->cdev;

            /* Free control semaphores */
            gx_semaphore_free(thread->sema_group);
            gx_semaphore_free(thread->sema_this);
            /* destroy the thread's buffer device */
            thread_cdev->buf_procs.destroy_buf_device(thread->bdev);

            if (thread->options) {
                if (thread->options->free_buffer_fn && thread->buffer) {
                    thread->options->free_buffer_fn(thread->options->arg, dev, thread->memory, thread->buffer);
                    thread->buffer = NULL;
                }
                thread->options = NULL;
            }

            /* before freeing this device's memory, swap with cdev if it was the main_thread_data */
            if (thread_cdev->data == crdev->main_thread_data) {
                thread_cdev->data = cdev->data;
                cdev->data = crdev->main_thread_data;
            }
#ifdef DEBUG
            if (gs_debug[':'])
                dmprintf2(thread->memory, "%% Thread %d total usertime=%ld msec\n", i, thread->cputime);
            dmprintf1(thread->memory, "\nThread %d ", i);
#endif
            teardown_device_and_mem_for_thread((gx_device *)thread_cdev, thread->thread, false);
        }
        gs_free_object(mem, crdev->render_threads, "clist_teardown_render_threads");
        crdev->render_threads = NULL;

        /* Now re-open the clist temp files so we can write to them */
        if (cdev->page_info.cfile == NULL) {
            char fmode[4];

            strcpy(fmode, "a+");        /* file already exists and we want to re-use it */
            strncat(fmode, gp_fmode_binary_suffix, 1);
            cdev->page_info.io_procs->fopen(cdev->page_info.cfname, fmode, &cdev->page_info.cfile,
                                mem, cdev->bandlist_memory, true);
            cdev->page_info.io_procs->fseek(cdev->page_info.cfile, 0, SEEK_SET, cdev->page_info.cfname);
            cdev->page_info.io_procs->fopen(cdev->page_info.bfname, fmode, &cdev->page_info.bfile,
                                mem, cdev->bandlist_memory, false);
            cdev->page_info.io_procs->fseek(cdev->page_info.bfile, 0, SEEK_SET, cdev->page_info.bfname);
        }
    }
}

static int
clist_start_render_thread(gx_device *dev, int thread_index, int band)
{
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    int code;

    crdev->render_threads[thread_index].band = band;
    crdev->render_threads[thread_index].status = THREAD_BUSY;

    /* Finally, fire it up */
    code = gp_thread_start(clist_render_thread,
                           &(crdev->render_threads[thread_index]),
                           &(crdev->render_threads[thread_index].thread));
    gp_thread_label(crdev->render_threads[thread_index].thread, "Band");

    return code;
}

static void
clist_render_thread(void *data)
{
    clist_render_thread_control_t *thread = (clist_render_thread_control_t *)data;
    gx_device *dev = thread->cdev;
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    gx_device *bdev = thread->bdev;
    gs_int_rect band_rect;
    byte *mdata = crdev->data + crdev->page_tile_cache_size;
    byte *mlines = (crdev->page_line_ptrs_offset == 0 ? NULL : mdata + crdev->page_line_ptrs_offset);
    uint raster = gx_device_raster_plane(dev, NULL);
    int code;
    int band_height = crdev->page_band_height;
    int band = thread->band;
    int band_begin_line = band * band_height;
    int band_end_line = band_begin_line + band_height;
    int band_num_lines;
#ifdef DEBUG
    long starttime[2], endtime[2];

    gp_get_usertime(starttime); /* thread start time */
#endif
    if (band_end_line > dev->height)
        band_end_line = dev->height;
    band_num_lines = band_end_line - band_begin_line;

    code = crdev->buf_procs.setup_buf_device
            (bdev, mdata, raster, (byte **)mlines, 0, band_num_lines, band_num_lines);
    band_rect.p.x = 0;
    band_rect.p.y = band_begin_line;
    band_rect.q.x = dev->width;
    band_rect.q.y = band_end_line;
    if (code >= 0)
        code = clist_render_rectangle(cldev, &band_rect, bdev, NULL, true);

    if (code >= 0 && thread->options && thread->options->process_fn)
        code = thread->options->process_fn(thread->options->arg, dev, bdev, &band_rect, thread->buffer);

    /* Reset the band boundaries now */
    crdev->ymin = band_begin_line;
    crdev->ymax = band_end_line;
    crdev->offset_map = NULL;
    if (code < 0)
        thread->status = THREAD_ERROR;          /* shouldn't happen */
    else
        thread->status = THREAD_DONE;    /* OK */

#ifdef DEBUG
    gp_get_usertime(endtime);
    thread->cputime += (endtime[0] - starttime[0]) * 1000 +
             (endtime[1] - starttime[1]) / 1000000;
#endif
    /*
     * Signal the semaphores. We signal the 'group' first since even if
     * the waiter is released on the group, it still needs to check
     * status on the thread
     */
    gx_semaphore_signal(thread->sema_group);
    gx_semaphore_signal(thread->sema_this);
}

/*
 * Copy the raster data from the completed thread to the caller's
 * device (the main thread)
 * Return 0 if OK, < 0 is the error code from the thread
 *
 * After swapping the pointers, start up the completed thread with the
 * next band remaining to do (if any)
 */
static int
clist_get_band_from_thread(gx_device *dev, int band_needed, gx_process_page_options_t *options)
{
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_common *cdev = (gx_device_clist_common *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    int i, code = 0;
    int thread_index = crdev->curr_render_thread;
    clist_render_thread_control_t *thread = &(crdev->render_threads[thread_index]);
    gx_device_clist_common *thread_cdev = (gx_device_clist_common *)thread->cdev;
    int band_height = crdev->page_info.band_params.BandHeight;
    int band_count = cdev->nbands;
    byte *tmp;                  /* for swapping data areas */

    /* We expect that the thread needed will be the 'current' thread */
    if (thread->band != band_needed) {
        int band = band_needed;

        emprintf3(thread->memory,
                  "thread->band = %d, band_needed = %d, direction = %d, ",
                  thread->band, band_needed, crdev->thread_lookahead_direction);

        /* Probably we went in the wrong direction, so let the threads */
        /* all complete, then restart them in the opposite direction   */
        /* If the caller is 'bouncing around' we may end up back here, */
        /* but that is a VERY rare case (we haven't seen it yet).      */
        for (i=0; i < crdev->num_render_threads; i++) {
            clist_render_thread_control_t *thread = &(crdev->render_threads[i]);

            if (thread->status == THREAD_BUSY)
                gx_semaphore_wait(thread->sema_this);
        }
        crdev->thread_lookahead_direction *= -1;      /* reverse direction (but may be overruled below) */
        if (band_needed == band_count-1)
            crdev->thread_lookahead_direction = -1;   /* assume backwards if we are asking for the last band */
        if (band_needed == 0)
            crdev->thread_lookahead_direction = 1;    /* force forward if we are looking for band 0 */

        dmprintf1(thread->memory, "new_direction = %d\n", crdev->thread_lookahead_direction);

        /* Loop starting the threads in the new lookahead_direction */
        for (i=0; (i < crdev->num_render_threads) && (band >= 0) && (band < band_count);
                i++, band += crdev->thread_lookahead_direction) {
            thread = &(crdev->render_threads[i]);
            thread->band = -1;          /* a value that won't match any valid band */
            /* Start thread 'i' to do band */
            if ((code = clist_start_render_thread(dev, i, band)) < 0)
                break;
        }
        crdev->next_band = i;			/* may be < 0 or == band_count, but that is handled later */
        crdev->curr_render_thread = thread_index = 0;
        thread = &(crdev->render_threads[0]);
        thread_cdev = (gx_device_clist_common *)thread->cdev;
    }
    /* Wait for this thread */
    gx_semaphore_wait(thread->sema_this);
    gp_thread_finish(thread->thread);
    thread->thread = NULL;
    if (thread->status == THREAD_ERROR)
        return_error(gs_error_unknownerror);          /* FAIL */

    if (options && options->output_fn) {
        code = options->output_fn(options->arg, dev, thread->buffer);
        if (code < 0)
            return code;
    }

    /* Swap the data areas to avoid the copy */
    tmp = cdev->data;
    cdev->data = thread_cdev->data;
    thread_cdev->data = tmp;
    thread->status = THREAD_IDLE;        /* the data is no longer valid */
    thread->band = -1;
    /* Update the bounds for this band */
    cdev->ymin =  band_needed * band_height;
    cdev->ymax =  cdev->ymin + band_height;
    if (cdev->ymax > dev->height)
        cdev->ymax = dev->height;

    if (crdev->next_band >= 0 && crdev->next_band < band_count) {
        code = clist_start_render_thread(dev, thread_index, crdev->next_band);
        crdev->next_band += crdev->thread_lookahead_direction;
    }
    /* bump the 'curr' to the next thread */
    crdev->curr_render_thread = crdev->curr_render_thread == crdev->num_render_threads - 1 ?
                0 : crdev->curr_render_thread + 1;

    return code;
}

/* Copy a rasterized rectangle to the client, rasterizing if needed. */
/* The first invocation starts multiple threads to perform "look ahead" */
/* rendering adjacent to the first band (forward or backward) */
static int
clist_get_bits_rect_mt(gx_device *dev, const gs_int_rect * prect,
                         gs_get_bits_params_t *params, gs_int_rect **unread)
{
    gx_device_printer *pdev = (gx_device_printer *)dev;
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_common *cdev = (gx_device_clist_common *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    gs_memory_t *mem = cdev->bandlist_memory;
    gs_get_bits_options_t options = params->options;
    int y = prect->p.y;
    int end_y = prect->q.y;
    int line_count = end_y - y;
    int band_height = crdev->page_info.band_params.BandHeight;
    int band = y / band_height;
    gs_int_rect band_rect;
    int lines_rasterized;
    gx_device *bdev;
    byte *mdata;
    uint raster = gx_device_raster(dev, 1);
    int my;
    int code = 0;

    /* This page might not want multiple threads */
    /* Also we don't support plane extraction using multiple threads */
    if (pdev->num_render_threads_requested < 1 || (options & GB_SELECT_PLANES))
        return clist_get_bits_rectangle(dev, prect, params, unread);

    if (prect->p.x < 0 || prect->q.x > dev->width ||
        y < 0 || end_y > dev->height
        )
        return_error(gs_error_rangecheck);
    if (line_count <= 0 || prect->p.x >= prect->q.x)
        return 0;

    if (crdev->ymin < 0)
        if ((code = clist_close_writer_and_init_reader(cldev)) < 0)
            return code;	/* can't recover from this */

    if (crdev->ymin == 0 && crdev->ymax == 0 && crdev->render_threads == NULL) {
        /* Haven't done any rendering yet, try to set up the threads */
        if (clist_setup_render_threads(dev, y, NULL) < 0)
            /* problem setting up the threads, revert to single threaded */
            return clist_get_bits_rectangle(dev, prect, params, unread);
    } else {
        if (crdev->render_threads == NULL) {
            /* If we get here with with ymin and ymax > 0 it's because we closed the threads */
            /* while doing a page due to an error. Use single threaded mode.         */
            return clist_get_bits_rectangle(dev, prect, params, unread);
        }
    }
    /* If we already have the band's data, just return it */
    if (y < crdev->ymin || end_y > crdev->ymax)
        code = clist_get_band_from_thread(dev, band, NULL);
    if (code < 0)
        goto free_thread_out;
    mdata = crdev->data + crdev->page_tile_cache_size;
    if ((code = gdev_create_buf_device(cdev->buf_procs.create_buf_device,
                                  &bdev, cdev->target, y, NULL,
                                  mem, &(crdev->color_usage_array[band]))) < 0 ||
        (code = crdev->buf_procs.setup_buf_device(bdev, mdata, raster, NULL,
                            y - crdev->ymin, line_count, crdev->ymax - crdev->ymin)) < 0)
        goto free_thread_out;

    lines_rasterized = min(band_height, line_count);
    /* Return as much of the rectangle as falls within the rasterized lines. */
    band_rect = *prect;
    band_rect.p.y = 0;
    band_rect.q.y = lines_rasterized;
    code = dev_proc(bdev, get_bits_rectangle)
        (bdev, &band_rect, params, unread);
    cdev->buf_procs.destroy_buf_device(bdev);
    if (code < 0)
        goto free_thread_out;

    /* Note that if called via 'get_bits', the line count will always be 1 */
    if (lines_rasterized == line_count) {
        return code;
    }

/***** TODO: Handle the below with data from the threads *****/
    /*
     * We'll have to return the rectangle in pieces.  Force GB_RETURN_COPY
     * rather than GB_RETURN_POINTER, and require all subsequent pieces to
     * use the same values as the first piece for all of the other format
     * options.  If copying isn't allowed, or if there are any unread
     * rectangles, punt.
     */
    if (!(options & GB_RETURN_COPY) || code > 0)
        return gx_default_get_bits_rectangle(dev, prect, params, unread);
    options = params->options;
    if (!(options & GB_RETURN_COPY)) {
        /* Redo the first piece with copying. */
        params->options = (params->options & ~GB_RETURN_ALL) | GB_RETURN_COPY;
        lines_rasterized = 0;
    }
    {
        gs_get_bits_params_t band_params;
        uint raster = gx_device_raster(bdev, true);

        code = gdev_create_buf_device(cdev->buf_procs.create_buf_device,
                                      &bdev, cdev->target, y, NULL,
                                      mem, &(crdev->color_usage_array[band]));
        if (code < 0)
            return code;
        band_params = *params;
        while ((y += lines_rasterized) < end_y) {
            /* Increment data pointer by lines_rasterized. */
            band_params.data[0] += raster * lines_rasterized;
            line_count = end_y - y;
            code = clist_rasterize_lines(dev, y, line_count, bdev, NULL, &my);
            if (code < 0)
                break;
            lines_rasterized = min(code, line_count);
            band_rect.p.y = my;
            band_rect.q.y = my + lines_rasterized;
            code = dev_proc(bdev, get_bits_rectangle)
                (bdev, &band_rect, &band_params, unread);
            if (code < 0)
                break;
            params->options = band_params.options;
            if (lines_rasterized == line_count)
                break;
        }
        cdev->buf_procs.destroy_buf_device(bdev);
    }
    return code;

/* Free up thread stuff */
free_thread_out:
    clist_teardown_render_threads(dev);
    return code;
}

int
clist_process_page(gx_device *dev, gx_process_page_options_t *options)
{
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    gx_device_clist_common *cdev = (gx_device_clist_common *)dev;
    int y;
    int line_count;
    int band_height = crdev->page_band_height;
    gs_int_rect band_rect;
    int lines_rasterized;
    gx_device *bdev;
    gx_render_plane_t render_plane;
    int my;
    int code;
    void *buffer = NULL;

    if (0 > (code = clist_close_writer_and_init_reader(cldev)))
        return code;

    if (options->init_buffer_fn) {
        code = options->init_buffer_fn(options->arg, dev, dev->memory, dev->width, band_height, &buffer);
        if (code < 0)
            return code;
    }

    render_plane.index = -1;
    for (y = 0; y < dev->height; y += lines_rasterized)
    {
        line_count = band_height;
        if (line_count > dev->height - y)
            line_count = dev->height - y;
        code = gdev_create_buf_device(cdev->buf_procs.create_buf_device,
                                      &bdev, cdev->target, y, &render_plane,
                                      dev->memory,
                                      &(crdev->color_usage_array[y/band_height]));
        if (code < 0)
            return code;
        code = clist_rasterize_lines(dev, y, line_count, bdev, &render_plane, &my);
        if (code >= 0)
        {
            lines_rasterized = min(code, line_count);

            /* Return as much of the rectangle as falls within the rasterized lines. */
            band_rect.p.x = 0;
            band_rect.p.y = y;
            band_rect.q.x = dev->width;
            band_rect.q.y = y + lines_rasterized;
            if (options->process_fn)
                code = options->process_fn(options->arg, dev, bdev, &band_rect, buffer);
        }
        if (code >= 0 && options->output_fn)
            code = options->output_fn(options->arg, dev, buffer);
        cdev->buf_procs.destroy_buf_device(bdev);
        if (code < 0)
            break;
    }

    if (options->free_buffer_fn) {
        options->free_buffer_fn(options->arg, dev, dev->memory, buffer);
    }

    return code;
}

static int
clist_process_page_mt(gx_device *dev, gx_process_page_options_t *options)
{
    gx_device_printer *pdev = (gx_device_printer *)dev;
    gx_device_clist *cldev = (gx_device_clist *)dev;
    gx_device_clist_reader *crdev = &cldev->reader;
    int band_height = crdev->page_info.band_params.BandHeight;
    int band;
    int num_bands = (dev->height + band_height-1)/band_height;
    int code;
    int reverse = !!(options->options & GX_PROCPAGE_BOTTOM_UP);

    /* This page might not want multiple threads */
    /* Also we don't support plane extraction using multiple threads */
    if (pdev->num_render_threads_requested < 1)
        return clist_process_page(dev, options);

    if ((code = clist_close_writer_and_init_reader(cldev)) < 0)
        return code;	/* can't recover from this */

    /* Haven't done any rendering yet, try to set up the threads */
    if (clist_setup_render_threads(dev, reverse ? dev->height-1 : 0, options) < 0)
        /* problem setting up the threads, revert to single threaded */
        return clist_process_page(dev, options);

    if (reverse)
    {
        for (band = num_bands-1; band > 0; band--)
        {
            code = clist_get_band_from_thread(dev, band, options);
            if (code < 0)
                goto free_thread_out;
        }
    }
    else
    {
        for (band = 0; band < num_bands; band++)
        {
            code = clist_get_band_from_thread(dev, band, options);
            if (code < 0)
                goto free_thread_out;
        }
    }

    /* Always free up thread stuff before exiting*/
free_thread_out:
    clist_teardown_render_threads(dev);
    return code;
}

static void
test_threads(void *dummy)
{
}

int
clist_enable_multi_thread_render(gx_device *dev)
{
    int code = -1;
    gp_thread_id thread;

    if (dev->procs.get_bits_rectangle == clist_get_bits_rect_mt)
        return 1;	/* no need to test again */
    /* We need to test gp_thread_start since we may be on a platform  */
    /* built without working threads, i.e., using gp_nsync.c dummy    */
    /* routines. The nosync gp_thread_start returns a -ve error code. */
    if ((code = gp_thread_start(test_threads, NULL, &thread)) < 0 ) {
        return code;    /* Threads don't work */
    }
    gp_thread_label(thread, "test_thread");
    gp_thread_finish(thread);
    set_dev_proc(dev, get_bits_rectangle, clist_get_bits_rect_mt);
    set_dev_proc(dev, process_page, clist_process_page_mt);

    return 1;
}
