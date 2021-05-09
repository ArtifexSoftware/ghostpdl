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


/* Generic printer driver support */
#include "ctype_.h"
#include "gdevprn.h"
#include "gp.h"
#include "gdevdevn.h"           /* for gs_devn_params_s */
#include "gsdevice.h"		/* for gs_deviceinitialmatrix */
#include "gxdevsop.h"		/* for gxdso_* */
#include "gsfname.h"
#include "gsparam.h"
#include "gxclio.h"
#include "gxgetbit.h"
#include "gdevplnx.h"
#include "gstrans.h"
#include "gxdownscale.h"
#include "gsbitops.h"

#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently First/Last Page and obejct filter */

/*#define DEBUGGING_HACKS*/

/* GC information */
static
ENUM_PTRS_WITH(device_printer_enum_ptrs, gx_device_printer *pdev);
    ENUM_PREFIX(st_device_clist_mutatable, 2);
    break;
case 0:ENUM_RETURN(gx_device_enum_ptr(pdev->parent));
case 1:ENUM_RETURN(gx_device_enum_ptr(pdev->child));
ENUM_PTRS_END
static
RELOC_PTRS_WITH(device_printer_reloc_ptrs, gx_device_printer *pdev)
{
    pdev->parent = gx_device_reloc_ptr(pdev->parent, gcst);
    pdev->child = gx_device_reloc_ptr(pdev->child, gcst);
    RELOC_PREFIX(st_device_clist_mutatable);
} RELOC_PTRS_END
public_st_device_printer();

/* ---------------- Standard device procedures ---------------- */

/* Forward references */
int gdev_prn_maybe_realloc_memory(gx_device_printer *pdev,
                                  gdev_space_params *old_space,
                                  int old_width, int old_height,
                                  bool old_page_uses_transparency);

static int
gdev_prn_output_page_aux(gx_device * pdev, int num_copies, int flush, bool seekable, bool bg_print_ok);

extern dev_proc_open_device(pattern_clist_open_device);
extern dev_proc_open_device(clist_open);
extern dev_proc_close_device(clist_close);

/* The function run in a background thread */
static void prn_print_page_in_background(void *data);

/* wait for a background thread to finish and clean up background printing */
static void prn_finish_bg_print(gx_device_printer *ppdev);

/* ------ Open/close ------ */
/* Open a generic printer device. */
/* Specific devices may wish to extend this. */
int
gdev_prn_open(gx_device * pdev)
{
    gx_device_printer * ppdev;
    int code;
    bool update_procs = false;

    code = install_internal_subclass_devices(&pdev, &update_procs);
    if (code < 0)
        return code;

    ppdev = (gx_device_printer *)pdev;

    ppdev->file = NULL;
    code = gdev_prn_allocate_memory(pdev, NULL, 0, 0);
    if (update_procs) {
        if (pdev->ObjectHandlerPushed) {
            gx_copy_device_procs(pdev->parent, pdev, &gs_obj_filter_device);
            pdev = pdev->parent;
        }
        if (pdev->PageHandlerPushed) {
            gx_copy_device_procs(pdev->parent, pdev, &gs_flp_device);
            pdev = pdev->parent;
        }
        if (pdev->NupHandlerPushed)
            gx_copy_device_procs(pdev->parent, pdev, &gs_nup_device);
    }
    if (code < 0)
        return code;
    if (ppdev->OpenOutputFile)
        code = gdev_prn_open_printer(pdev, 1);
    return code;
}

/* This is called various places to wait for any pending bg print thread and */
/* perform its cleanup                                                       */
static void
prn_finish_bg_print(gx_device_printer *ppdev)
{
    /* if we have a a bg printing device that was created, then wait for its	*/
    /* semaphore (it may already have been signalled, but that's OK.) then	*/
    /* close and unlink the files and free the device and its private allocator	*/
    if (ppdev->bg_print && (ppdev->bg_print->device != NULL)) {
        int closecode;
        gx_device_printer *bgppdev = (gx_device_printer *)ppdev->bg_print->device;

        gx_semaphore_wait(ppdev->bg_print->sema);
        /* If numcopies > 1, then the bg_print->device will have closed and reopened
         * the output file, so the pointer in the original device is now stale,
         * so copy it back.
         * If numcopies == 1, this is pointless, but benign.
         */
        ppdev->file = bgppdev->file;
        closecode = gdev_prn_close_printer((gx_device *)ppdev);
        if (ppdev->bg_print->return_code == 0)
            ppdev->bg_print->return_code = closecode;	/* return code here iff there wasn't another error */
        teardown_device_and_mem_for_thread(ppdev->bg_print->device,
                                           ppdev->bg_print->thread_id, true);
        ppdev->bg_print->device = NULL;
        if (ppdev->bg_print->ocfile) {
            closecode = ppdev->bg_print->oio_procs->fclose(ppdev->bg_print->ocfile, ppdev->bg_print->ocfname, true);
            if (ppdev->bg_print->return_code == 0)
               ppdev->bg_print->return_code = closecode;
        }
        if (ppdev->bg_print->ocfname) {
            gs_free_object(ppdev->memory->non_gc_memory, ppdev->bg_print->ocfname, "prn_finish_bg_print(ocfname)");
        }
        if (ppdev->bg_print->obfile) {
            closecode = ppdev->bg_print->oio_procs->fclose(ppdev->bg_print->obfile, ppdev->bg_print->obfname, true);
            if (ppdev->bg_print->return_code == 0)
               ppdev->bg_print->return_code = closecode;
        }
        if (ppdev->bg_print->obfname) {
            gs_free_object(ppdev->memory->non_gc_memory, ppdev->bg_print->obfname, "prn_finish_bg_print(obfname)");
        }
        ppdev->bg_print->ocfile = ppdev->bg_print->obfile =
          ppdev->bg_print->ocfname = ppdev->bg_print->obfname = NULL;
    }
}
/* Generic closing for the printer device. */
/* Specific devices may wish to extend this. */
int
gdev_prn_close(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int code = 0;

    prn_finish_bg_print(ppdev);
    if (ppdev->bg_print != NULL && ppdev->bg_print->sema != NULL) {
        gx_semaphore_free(ppdev->bg_print->sema);
        ppdev->bg_print->sema = NULL;		/* prevent double free */
    }
    gdev_prn_free_memory(pdev);
    if (ppdev->file != NULL) {
        code = gx_device_close_output_file(pdev, ppdev->fname, ppdev->file);
        ppdev->file = NULL;
    }
    return code;
}

int
gdev_prn_forwarding_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    gx_device_printer *ppdev = (gx_device_printer *)pdev;
    int code;

    /* if the device is a printer device, we will get here, but allow for a printer device */
    /* that has a non-default dev_spec_op that may want to not support saved_pages, if so  */
    /* that device can return an error from the supports_saved_pages spec_op.              */
    code = ppdev->orig_procs.dev_spec_op(pdev, dev_spec_op, data, size);
    if (dev_spec_op == gxdso_supports_saved_pages)	/* printer devices support saved pages */
        return code == 0 ? 1: code;	/*default returns 0, but we still want saved-page support */
    return code;
}

int
gdev_prn_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    if (dev_spec_op == gxdso_supports_saved_pages)
        return 1;

    if (dev_spec_op == gxdso_get_dev_param) {
        int code;
        dev_param_req_t *request = (dev_param_req_t *)data;

        code = gdev_prn_get_param(pdev, request->Param, request->list);
        if (code != gs_error_undefined)
            return code;
    }

#ifdef DEBUG
    if (dev_spec_op == gxdso_debug_printer_check)
        return 1;
#endif

    return gx_default_dev_spec_op(pdev, dev_spec_op, data, size);
}

static bool	/* ret true if device was cmd list, else false */
gdev_prn_tear_down(gx_device *pdev, byte **the_memory)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    gx_device_memory * const pmemdev = (gx_device_memory *)pdev;
    gx_device_clist *const pclist_dev = (gx_device_clist *)pdev;
    gx_device_clist_common * const pcldev = &pclist_dev->common;
    gx_device_clist_reader * const pcrdev = &pclist_dev->reader;
    bool was_command_list;

    if (PRINTER_IS_CLIST(ppdev)) {
        /* Close cmd list device & point to the storage */
        clist_close( (gx_device *)pcldev );
        *the_memory = ppdev->buf;
        ppdev->buf = 0;
        ppdev->buffer_space = 0;
        was_command_list = true;

        prn_finish_bg_print(ppdev);

        gs_free_object(pcldev->memory->non_gc_memory, pcldev->cache_chunk, "free tile cache for clist");
        pcldev->cache_chunk = 0;

        rc_decrement(pcldev->icc_cache_cl, "gdev_prn_tear_down");
        pcldev->icc_cache_cl = NULL;

        clist_free_icc_table(pcldev->icc_table, pcldev->memory);
        pcldev->icc_table = NULL;

        /* If the clist is a reader clist, free any color_usage_array
         * memory used by same.
         */
        if (!CLIST_IS_WRITER(pclist_dev))
            gs_free_object(pcrdev->memory, pcrdev->color_usage_array, "clist_color_usage_array");

    } else {
        /* point at the device bitmap, no need to close mem dev */
        *the_memory = pmemdev->base;
        pmemdev->base = 0;
        was_command_list = false;
    }

    /* Reset device proc vector to default */
    if (ppdev->orig_procs.open_device != NULL)
        pdev->procs = ppdev->orig_procs;
    ppdev->orig_procs.open_device = NULL; /* prevent uninit'd restore of procs */

    return was_command_list;
}

static int
gdev_prn_allocate(gx_device *pdev, gdev_space_params *new_space_params,
                  int new_width, int new_height, bool reallocate)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    gx_device_memory * const pmemdev = (gx_device_memory *)pdev;
    byte *the_memory = 0;
    gdev_space_params save_params = ppdev->space_params;
    int save_width = 0x0badf00d; /* Quiet compiler */
    int save_height = 0x0badf00d; /* Quiet compiler */
    bool is_command_list = false; /* Quiet compiler */
    bool save_is_command_list = false; /* Quiet compiler */
    bool size_ok = 0;
    int ecode = 0;
    int pass;
    gs_memory_t *buffer_memory =
        (ppdev->buffer_memory == 0 ? pdev->memory->non_gc_memory :
         ppdev->buffer_memory);
    bool deep = device_is_deep(pdev);

    /* If reallocate, find allocated memory & tear down buffer device */
    if (reallocate)
        save_is_command_list = gdev_prn_tear_down(pdev, &the_memory);


    /* bg_print allocation is not fatal, we just continue (as far as possible) without BGPrint */
    if (ppdev->bg_print == NULL)
        ppdev->bg_print = (bg_print_t *)gs_alloc_bytes(pdev->memory->non_gc_memory, sizeof(bg_print_t), "prn bg_print");
    if (ppdev->bg_print == NULL) {
        emprintf(pdev->memory, "Failed to allocate memory for BGPrint, attempting to continue without BGPrint\n");
    } else {
        memset(ppdev->bg_print, 0, sizeof(bg_print_t));
    }

    /* Re/allocate memory */
    ppdev->orig_procs = pdev->procs;
    for ( pass = 1; pass <= (reallocate ? 2 : 1); ++pass ) {
        ulong mem_space;
        ulong pdf14_trans_buffer_size = 0;
        byte *base = 0;
        bool bufferSpace_is_default = false;
        gdev_space_params space_params;
        gx_device_buf_space_t buf_space;

        if (reallocate)
            switch (pass)
                {
                case 1:
                    /* Setup device to get reallocated */
                    ppdev->space_params = *new_space_params;
                    save_width = ppdev->width;
                    ppdev->width = new_width;
                    save_height = ppdev->height;
                    ppdev->height = new_height;
                    break;
                case 2:	/* only comes here if reallocate */
                    /* Restore device to previous contents */
                    ppdev->space_params = save_params;
                    ppdev->width = save_width;
                    ppdev->height = save_height;
                    break;
                }

        /* Init clist/mem device-specific fields */
        memset(ppdev->skip, 0, sizeof(ppdev->skip));
        size_ok = ppdev->printer_procs.buf_procs.size_buf_device
            (&buf_space, pdev, NULL, pdev->height, false) >= 0;
        mem_space = buf_space.bits + buf_space.line_ptrs;
        if (ppdev->page_uses_transparency) {
            pdf14_trans_buffer_size = (ESTIMATED_PDF14_ROW_SPACE(max(1, pdev->width), pdev->color_info.num_components, deep ? 16 : 8) >> 3);
            if (new_height < (max_ulong - mem_space) / pdf14_trans_buffer_size) {
                pdf14_trans_buffer_size *= pdev->height;
            } else {
                size_ok = 0;
            }
        }

        /* Compute desired space params: never use the space_params as-is. */
        /* Rather, give the dev-specific driver a chance to adjust them. */
        space_params = ppdev->space_params;
        space_params.BufferSpace = 0;
        (*ppdev->printer_procs.get_space_params)(ppdev, &space_params);
        if (space_params.BufferSpace == 0) {
            if (space_params.band.BandBufferSpace > 0)
                space_params.BufferSpace = space_params.band.BandBufferSpace;
            else {
                space_params.BufferSpace = ppdev->space_params.BufferSpace;
                bufferSpace_is_default = true;
            }
        }

        /* Determine if we can use a full bitmap buffer, or have to use banding */
        if (pass > 1)
            is_command_list = save_is_command_list;
        else {
            is_command_list = space_params.banding_type == BandingAlways ||
                ppdev->saved_pages_list != NULL ||
                mem_space + pdf14_trans_buffer_size >= space_params.MaxBitmap ||
                !size_ok;	    /* too big to allocate */
        }
        if (!is_command_list) {
            byte *trans_buffer_reserve_space = NULL;

            /* Try to allocate memory for full memory buffer, then allocate the
               pdf14_trans_buffer_size to make sure we have enough space for that */
            /* NOTE: Assumes that caller won't normally call this function if page
               size didn't actually change, so we can free/alloc without checking
               that the new size is different than old size.
            */
            if (reallocate) {
                gs_free_object(buffer_memory, the_memory, "printer_buffer");
            }
            base = gs_alloc_bytes(buffer_memory, (uint)mem_space, "printer_buffer");
            if (base == 0)
                is_command_list = true;
            else
                the_memory = base;
            trans_buffer_reserve_space = gs_alloc_bytes(buffer_memory, (uint)pdf14_trans_buffer_size,
                                                        "pdf14_trans_buffer_reserve test");
            if (trans_buffer_reserve_space == NULL) {
                /* the pdf14 reserve test failed, switch to clist mode, the 'base' memory freed below */
                is_command_list = true;
            } else {
                gs_free_object(buffer_memory, trans_buffer_reserve_space, "pdf14_trans_buffer_reserve OK");
            }
        }
        if (!is_command_list && pass == 1 && PRN_MIN_MEMORY_LEFT != 0
            && buffer_memory == pdev->memory->non_gc_memory) {
            /* before using full memory buffer, ensure enough working mem left */
            byte * left = gs_alloc_bytes( buffer_memory,
                                          PRN_MIN_MEMORY_LEFT, "printer mem left");
            if (left == 0)
                is_command_list = true;
            else
                gs_free_object(buffer_memory, left, "printer mem left");
        }

        if (is_command_list) {
            /* Buffer the image in a command list. */
            /* Release the buffer if we allocated it. */
            int code;
            gx_device_printer * const ppdev = (gx_device_printer *)pdev;

            if (!reallocate) {
                gs_free_object(buffer_memory, the_memory,
                               "printer buffer(open)");
                the_memory = 0;
            }
            if (space_params.banding_type == BandingNever) {
                ecode = gs_note_error(gs_error_VMerror);
                continue;
            }
            if (ppdev->bg_print) {
                ppdev->bg_print->ocfname = ppdev->bg_print->obfname =
                    ppdev->bg_print->obfile = ppdev->bg_print->ocfile = NULL;
            }

            code = clist_mutate_to_clist((gx_device_clist_mutatable *)pdev,
                                         buffer_memory,
                                         &the_memory, &space_params,
                                         !bufferSpace_is_default,
                                         &ppdev->printer_procs.buf_procs,
                                         gdev_prn_forwarding_dev_spec_op,
                                         PRN_MIN_BUFFER_SPACE);
            if (ecode == 0)
                ecode = code;

            if (code >= 0 || (reallocate && pass > 1)) {
                ppdev->initialize_device_procs = clist_initialize_device_procs;
                /* Hacky - we know this can't fail. */
                (void)ppdev->initialize_device_procs((gx_device *)ppdev);
                gx_device_fill_in_procs((gx_device *)ppdev);
            }
        } else {
            /* Render entirely in memory. */
            gx_device *bdev = (gx_device *)pmemdev;
            int code;

            ppdev->buffer_space = 0;
            if ((code = gdev_create_buf_device
                 (ppdev->printer_procs.buf_procs.create_buf_device,
                  &bdev, pdev, 0, NULL, NULL, NULL)) < 0 ||
                (code = ppdev->printer_procs.buf_procs.setup_buf_device
                 (bdev, base, buf_space.raster,
                  (byte **)(base + buf_space.bits), 0, pdev->height,
                  pdev->height)) < 0
                ) {
                /* Catastrophic. Shouldn't ever happen */
                gs_free_object(buffer_memory, base, "printer buffer");
                pdev->procs = ppdev->orig_procs;
                ppdev->orig_procs.open_device = 0;	/* prevent uninit'd restore of procs */
                return_error(code);
            }
        }
        if (ecode == 0)
            break;
    }

    if (ecode >= 0 || reallocate) {	/* even if realloc failed */
        /* Synthesize the procedure vector. */
        /* Rendering operations come from the memory or clist device, */
        /* non-rendering come from the printer device. */
#define COPY_PROC(p) set_dev_proc(ppdev, p, ppdev->orig_procs.p)
        COPY_PROC(get_initial_matrix);
        COPY_PROC(output_page);
        COPY_PROC(close_device);
        COPY_PROC(map_rgb_color);
        COPY_PROC(map_color_rgb);
        COPY_PROC(get_params);
        COPY_PROC(put_params);
        COPY_PROC(map_cmyk_color);
        /* All printers are page devices, even if they didn't use the */
        /* standard macros for generating their procedure vectors. */
        set_dev_proc(ppdev, get_page_device, gx_page_device_get_page_device);
        COPY_PROC(get_clipping_box);
        COPY_PROC(get_hardware_params);
        COPY_PROC(get_color_mapping_procs);
        COPY_PROC(get_color_comp_index);
        COPY_PROC(encode_color);
        COPY_PROC(decode_color);
        COPY_PROC(update_spot_equivalent_colors);
        COPY_PROC(ret_devn_params);
        /* This can be set from the memory device (planar) or target */
        if ( dev_proc(ppdev, put_image) == gx_default_put_image )
            set_dev_proc(ppdev, put_image, ppdev->orig_procs.put_image);
#undef COPY_PROC
        /* If using a command list, already opened the device. */
        if (is_command_list)
            return ecode;
        else
            return (*dev_proc(pdev, open_device))(pdev);
    } else {
        pdev->procs = ppdev->orig_procs;
        ppdev->orig_procs.open_device = 0;	/* prevent uninit'd restore of procs */
        return ecode;
    }
}

int
gdev_prn_allocate_memory(gx_device *pdev,
                         gdev_space_params *new_space_params,
                         int new_width, int new_height)
{
    return gdev_prn_allocate(pdev, new_space_params,
                             new_width, new_height, false);
}

int
gdev_prn_reallocate_memory(gx_device *pdev,
                         gdev_space_params *new_space_params,
                         int new_width, int new_height)
{
    return gdev_prn_allocate(pdev, new_space_params,
                             new_width, new_height, true);
}

int
gdev_prn_free_memory(gx_device *pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    byte *the_memory = 0;
    gs_memory_t *buffer_memory =
        (ppdev->buffer_memory == 0 ? pdev->memory->non_gc_memory :
         ppdev->buffer_memory);

    gdev_prn_tear_down(pdev, &the_memory);
    gs_free_object(pdev->memory->non_gc_memory, ppdev->bg_print, "gdev_prn_free_memory");
    ppdev->bg_print = NULL;
    gs_free_object(buffer_memory, the_memory, "gdev_prn_free_memory");
    return 0;
}

/* for saved pages, we need to make sure and free up the saved_pages_list */
void
gdev_prn_finalize(gx_device *pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;

    if (ppdev->saved_pages_list != NULL) {
        gx_saved_pages_list_free(ppdev->saved_pages_list);
        ppdev->saved_pages_list = NULL;
    }
}


/* ------ Get/put parameters ------ */

int
gdev_prn_get_param(gx_device *dev, char *Param, void *list)
{
    gx_device_printer * const ppdev = (gx_device_printer *)dev;
    gs_param_list * plist = (gs_param_list *)list;
    bool pageneutralcolor = false;

    if (strcmp(Param, "Duplex") == 0) {
        if (ppdev->Duplex_set >= 0) {
            if (ppdev->Duplex_set)
                return param_write_bool(plist, "Duplex", &ppdev->Duplex);
            else
                return param_write_null(plist, "Duplex");
        }
    }
    if (strcmp(Param, "NumRenderingThreads") == 0) {
        return param_write_int(plist, "NumRenderingThreads", &ppdev->num_render_threads_requested);
    }
    if (strcmp(Param, "OpenOutputFile") == 0) {
        return param_write_bool(plist, "OpenOutputFile", &ppdev->OpenOutputFile);
    }
    if (strcmp(Param, "BGPrint") == 0) {
        return param_write_bool(plist, "BGPrint", &ppdev->bg_print_requested);
    }
    if (strcmp(Param, "ReopenPerPage") == 0) {
        return param_write_bool(plist, "ReopenPerPage", &ppdev->ReopenPerPage);
    }
    if (strcmp(Param, "BandListStorage") == 0) {
        gs_param_string bls;
        /* Force the default to 'memory' if clist file I/O is not included in this build */
        if (clist_io_procs_file_global == NULL)
            ppdev->BLS_force_memory = true;
        if (ppdev->BLS_force_memory) {
            bls.data = (byte *)"memory";
            bls.size = 6;
            bls.persistent = false;
        } else {
            bls.data = (byte *)"file";
            bls.size = 4;
            bls.persistent = false;
        }
        return param_write_string(plist, "BandListStorage", &bls);
    }
    if (strcmp(Param, "OutputFile") == 0) {
        gs_param_string ofns;

        ofns.data = (const byte *)ppdev->fname,
        ofns.size = strlen(ppdev->fname),
        ofns.persistent = false;
        return param_write_string(plist, "OutputFile", &ofns);
    }
    if (strcmp(Param, "saved-pages") == 0) {
        gs_param_string saved_pages;
        /* Always return an empty string for saved-pages */
        saved_pages.data = (const byte *)"";
        saved_pages.size = 0;
        saved_pages.persistent = false;
        return param_write_string(plist, "saved-pages", &saved_pages);
    }
    if (dev->icc_struct != NULL)
        pageneutralcolor = dev->icc_struct->pageneutralcolor;
    if (strcmp(Param, "pageneutralcolor") == 0) {
        return param_write_bool(plist, "pageneutralcolor", &pageneutralcolor);
    }
    return gx_default_get_param(dev, Param, list);
}

/* Get parameters.  Printer devices add several more parameters */
/* to the default set. */
int
gdev_prn_get_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int code = gx_default_get_params(pdev, plist);
    gs_param_string ofns;
    gs_param_string bls;
    gs_param_string saved_pages;
    bool pageneutralcolor = false;

    if (pdev->icc_struct != NULL)
        pageneutralcolor = pdev->icc_struct->pageneutralcolor;
    if (code < 0 ||
        (ppdev->Duplex_set >= 0 &&
        (code = (ppdev->Duplex_set ?
                  param_write_bool(plist, "Duplex", &ppdev->Duplex) :
                  param_write_null(plist, "Duplex"))) < 0) ||
        (code = param_write_int(plist, "NumRenderingThreads", &ppdev->num_render_threads_requested)) < 0 ||
        (code = param_write_bool(plist, "OpenOutputFile", &ppdev->OpenOutputFile)) < 0 ||
        (code = param_write_bool(plist, "BGPrint", &ppdev->bg_print_requested)) < 0 ||
        (code = param_write_bool(plist, "ReopenPerPage", &ppdev->ReopenPerPage)) < 0 ||
        (code = param_write_bool(plist, "pageneutralcolor", &pageneutralcolor)) < 0
        )
        return code;

    /* Force the default to 'memory' if clist file I/O is not included in this build */
    if (clist_io_procs_file_global == NULL)
        ppdev->BLS_force_memory = true;
    if (ppdev->BLS_force_memory) {
        bls.data = (byte *)"memory";
        bls.size = 6;
        bls.persistent = false;
    } else {
        bls.data = (byte *)"file";
        bls.size = 4;
        bls.persistent = false;
    }
    if( (code = param_write_string(plist, "BandListStorage", &bls)) < 0 )
        return code;

    ofns.data = (const byte *)ppdev->fname,
        ofns.size = strlen(ppdev->fname),
        ofns.persistent = false;
    if ((code = param_write_string(plist, "OutputFile", &ofns)) < 0)
        return code;

    /* Always return an empty string for saved-pages so that get_params followed */
    /* by put_params will have no effect.                                       */
    saved_pages.data = (const byte *)"";
    saved_pages.size = 0;
    saved_pages.persistent = false;
    return param_write_string(plist, "saved-pages", &saved_pages);
}

/* Validate an OutputFile name by checking any %-formats. */
static int
validate_output_file(const gs_param_string * ofs, gs_memory_t *memory)
{
    gs_parsed_file_name_t parsed;
    const char *fmt;

    return gx_parse_output_file_name(&parsed, &fmt, (const char *)ofs->data,
                                     ofs->size, memory) >= 0;
}

/* Put parameters. */
int
gdev_prn_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int ecode = 0;
    int code;
    const char *param_name;
    bool is_open = pdev->is_open;
    bool oof = ppdev->OpenOutputFile;
    bool rpp = ppdev->ReopenPerPage;
    bool old_page_uses_transparency = ppdev->page_uses_transparency;
    bool bg_print_requested = ppdev->bg_print_requested;
    bool duplex;
    int duplex_set = -1;
    int width = pdev->width;
    int height = pdev->height;
    int nthreads = ppdev->num_render_threads_requested;
    gdev_space_params save_sp;
    gs_param_string ofs;
    gs_param_string bls;
    gs_param_dict mdict;
    gs_param_string saved_pages;
    bool pageneutralcolor = false;

    memset(&saved_pages, 0, sizeof(gs_param_string));
    save_sp = ppdev->space_params;

    switch (code = param_read_bool(plist, (param_name = "OpenOutputFile"), &oof)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

    switch (code = param_read_bool(plist, (param_name = "ReopenPerPage"), &rpp)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

    if (ppdev->Duplex_set >= 0)	/* i.e., Duplex is supported */
        switch (code = param_read_bool(plist, (param_name = "Duplex"),
                                       &duplex)) {
            case 0:
                duplex_set = 1;
                break;
            default:
                if ((code = param_read_null(plist, param_name)) == 0) {
                    duplex_set = 0;
                    break;
                }
                ecode = code;
                param_signal_error(plist, param_name, ecode);
            case 1:
                ;
        }
    switch (code = param_read_string(plist, (param_name = "BandListStorage"), &bls)) {
        case 0:
            /* Only accept 'file' if the file procs are include in the build */
            if ((bls.size > 1) && (bls.data[0] == 'm' ||
                 (clist_io_procs_file_global != NULL && bls.data[0] == 'f')))
                break;
            /* fall through */
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
            /* fall through */
        case 1:
            bls.data = 0;
            break;
    }

    switch (code = param_read_string(plist, (param_name = "OutputFile"), &ofs)) {
        case 0:
            if (pdev->LockSafetyParams &&
                    bytes_compare(ofs.data, ofs.size,
                        (const byte *)ppdev->fname, strlen(ppdev->fname))) {
                code = gs_error_invalidaccess;
            }
            else
                code = validate_output_file(&ofs, pdev->memory);
            if (code >= 0)
                break;
            /* fall through */
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
            /* fall through */
        case 1:
            ofs.data = 0;
            break;
    }

    /* Read InputAttributes and OutputAttributes just for the type */
    /* check and to indicate that they aren't undefined. */
#define read_media(pname)\
        switch ( code = param_begin_read_dict(plist, (param_name = pname), &mdict, true) )\
          {\
          case 0:\
                param_end_read_dict(plist, pname, &mdict);\
                break;\
          default:\
                ecode = code;\
                param_signal_error(plist, param_name, ecode);\
          case 1:\
                ;\
          }

    read_media("InputAttributes");
    read_media("OutputAttributes");

    switch (code = param_read_int(plist, (param_name = "NumRenderingThreads"), &nthreads)) {
        case 0:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 1:
            ;
    }
    switch (code = param_read_bool(plist, (param_name = "BGPrint"),
                                                        &bg_print_requested)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

    switch (code = param_read_string(plist, (param_name = "saved-pages"),
                                                        &saved_pages)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

    /* NB: put_params to change pageneutralcolor is allowed, but has no effect */
    /*     It will be set according the GrayDetection. Allowing it prevents errors */
    if (pdev->icc_struct != NULL)
        pageneutralcolor = pdev->icc_struct->pageneutralcolor;
    if ((code = param_read_bool(plist, (param_name = "pageneutralcolor"),
                                                        &pageneutralcolor)) < 0) {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }

    if (ecode < 0)
        return ecode;
    /* Prevent gx_default_put_params from closing the printer. */
    pdev->is_open = false;
    code = gx_default_put_params(pdev, plist);
    pdev->is_open = is_open;
    if (code < 0)
        return code;

    ppdev->OpenOutputFile = oof;
    ppdev->ReopenPerPage = rpp;

    /* If BGPrint was previously true and it is being turned off, wait for the BG thread */
    if (ppdev->bg_print_requested && !bg_print_requested) {
        prn_finish_bg_print(ppdev);
    }

    ppdev->bg_print_requested = bg_print_requested;
    if (duplex_set >= 0) {
        ppdev->Duplex = duplex;
        ppdev->Duplex_set = duplex_set;
    }
    ppdev->num_render_threads_requested = nthreads;
    if (bls.data != 0) {
        ppdev->BLS_force_memory = (bls.data[0] == 'm');
    }

    /* If necessary, free and reallocate the printer memory. */
    /* Formerly, would not reallocate if device is not open: */
    /* we had to patch this out (see News for 5.50). */
    code = gdev_prn_maybe_realloc_memory(ppdev, &save_sp, width, height,
                                         old_page_uses_transparency);
    if (code < 0)
        return code;

    /* If filename changed, close file. */
    if (ofs.data != 0 &&
        bytes_compare(ofs.data, ofs.size,
                      (const byte *)ppdev->fname, strlen(ppdev->fname))
        ) {
        /* Close the file if it's open. */
        if (ppdev->file != NULL) {
            gx_device_close_output_file(pdev, ppdev->fname, ppdev->file);
        }
        ppdev->file = NULL;
        if (sizeof(ppdev->fname) <= ofs.size)
            return_error(gs_error_limitcheck);
        memcpy(ppdev->fname, ofs.data, ofs.size);
        ppdev->fname[ofs.size] = 0;
    }
    /* If the device is open and OpenOutputFile is true, */
    /* open the OutputFile now.  (If the device isn't open, */
    /* this will happen when it is opened.) */
    if (pdev->is_open && oof) {
        code = gdev_prn_open_printer(pdev, 1);
        if (code < 0)
            return code;
    }

    /* Processing the saved_pages string MAY have side effects, such as printing, */
    /* allocating or freeing a list. This is (sort of) a write-only parameter, so */
    /* the get_params will always return an empty string (a no-op action).        */
    if (saved_pages.data != 0 && saved_pages.size != 0) {
        return gx_saved_pages_param_process(ppdev, (byte *)saved_pages.data, saved_pages.size);
    }
    return 0;
}

/* ------ Others ------ */

/* Default routine to (not) override current space_params. */
void
gx_default_get_space_params(const gx_device_printer *printer_dev,
                            gdev_space_params *space_params)
{
    return;
}

/* Common routine to send the page to the printer.                               */
/* If seekable is true, then the printer outputfile must be seekable.            */
/* If bg_print_ok is true, the device print_page_copies is compatible with the   */
/* background printing, i.e., thread safe and does not change the device.        */
/* If the printer device is in 'saved_pages' mode, then background printing is   */
/* irrelevant and is ignored. In this case, pages are saved to the list.         */
static int	/* 0 ok, -ve error                                               */
gdev_prn_output_page_aux(gx_device * pdev, int num_copies, int flush, bool seekable, bool bg_print_ok)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    gs_devn_params *pdevn_params;
    int outcode = 0, errcode = 0, endcode, closecode = 0;
    int code;

    prn_finish_bg_print(ppdev);		/* finish any previous background printing */

    if (num_copies > 0 && ppdev->saved_pages_list != NULL) {
        /* We are putting pages on a list */
        if ((code = gx_saved_pages_list_add(ppdev)) < 0)
            return code;

    } else if (num_copies > 0 || !flush) {
        if ((code = gdev_prn_open_printer_seekable(pdev, 1, seekable)) < 0)
            return code;

        if (num_copies > 0) {
            int threads_enabled = 0;
            int print_foreground = 1;		/* default to foreground printing */

            if (bg_print_ok && PRINTER_IS_CLIST(ppdev) && ppdev->bg_print &&
                (ppdev->bg_print_requested || ppdev->num_render_threads_requested > 0)) {
                threads_enabled = clist_enable_multi_thread_render(pdev);
            }
            /* NB: we leave the semaphore allocated until foreground printing or close */
            /* If there was an error, abort on this page -- no good way to handle this */
            /* but it means that the error will be reported AFTER another page was     */
            /* interpreted and written to clist files. FIXME: ???                      */
            if (ppdev->bg_print && (ppdev->bg_print->return_code < 0)) {
                outcode = ppdev->bg_print->return_code;
                threads_enabled = 0;	/* and allow current page to try foreground */
            }
            /* Use 'while' instead of 'if' to avoid nesting */
            while (ppdev->bg_print_requested && ppdev->bg_print && threads_enabled) {
                gx_device *ndev;
                gx_device_printer *npdev;
                gx_device_clist_reader *crdev = (gx_device_clist_reader *)ppdev;

                if ((code = clist_close_writer_and_init_reader((gx_device_clist *)ppdev)) < 0)
                    /* should not happen -- do foreground print */
                    break;

                /* We need to hang onto references to these files, so we can ensure the main file data
                 * gets freed with the correct allocator.
                 */
                ppdev->bg_print->ocfname =
                     (char *)gs_alloc_bytes(ppdev->memory->non_gc_memory,
                           strnlen(crdev->page_info.cfname, gp_file_name_sizeof - 1) + 1, "gdev_prn_output_page_aux(ocfname)");
                ppdev->bg_print->obfname =
                     (char *)gs_alloc_bytes(ppdev->memory->non_gc_memory,
                           strnlen(crdev->page_info.bfname, gp_file_name_sizeof - 1) + 1,"gdev_prn_output_page_aux(ocfname)");

                if (!ppdev->bg_print->ocfname || !ppdev->bg_print->obfname)
                    break;

                strncpy(ppdev->bg_print->ocfname, crdev->page_info.cfname, strnlen(crdev->page_info.cfname, gp_file_name_sizeof - 1) + 1);
                strncpy(ppdev->bg_print->obfname, crdev->page_info.bfname, strnlen(crdev->page_info.bfname, gp_file_name_sizeof - 1) + 1);
                ppdev->bg_print->obfile = crdev->page_info.bfile;
                ppdev->bg_print->ocfile = crdev->page_info.cfile;
                ppdev->bg_print->oio_procs = crdev->page_info.io_procs;
                crdev->page_info.cfile = crdev->page_info.bfile = NULL;

                if (ppdev->bg_print->sema == NULL)
                {
                    ppdev->bg_print->sema = gx_semaphore_label(gx_semaphore_alloc(ppdev->memory->non_gc_memory), "BGPrint");
                    if (ppdev->bg_print->sema == NULL)
                        break;			/* couldn't create the semaphore */
                }

                ndev = setup_device_and_mem_for_thread(pdev->memory->thread_safe_memory, pdev, true, NULL);
                if (ndev == NULL) {
                    break;
                }
                ppdev->bg_print->device = ndev;
                ppdev->bg_print->num_copies = num_copies;
                npdev = (gx_device_printer *)ndev;
                npdev->bg_print_requested = 0;
                npdev->num_render_threads_requested = ppdev->num_render_threads_requested;
                /* The bgprint's device was created with normal procs, so multi-threaded */
                /* rendering was turned off. Re-enable it now if it is needed.           */
                if (npdev->num_render_threads_requested > 0) {
                    /* ignore return code - even if it fails, we'll output the page */
                    (void)clist_enable_multi_thread_render(ndev);
                }

                /* Now start the thread to print the page */
                if ((code = gp_thread_start(prn_print_page_in_background,
                                            (void *)(ppdev->bg_print),
                                            &(ppdev->bg_print->thread_id))) < 0) {
                    /* Did not start cleanly - clean up is in print_foreground block below */
                    break;
                }
                gp_thread_label(ppdev->bg_print->thread_id, "BG print thread");
                /* Page was succesfully started in bg_print mode */
                print_foreground = 0;
                /* Now we need to set up the next page so it will use new clist files */
                if ((code = clist_open(pdev)) < 0) 	/* this should do it */
                    /* OOPS! can't proceed with the next page */
                    return code;	/* probably ioerror */
                break;				/* exit the while loop */
            }
            if (print_foreground) {
                if (ppdev->bg_print) {
                     gs_free_object(ppdev->memory->non_gc_memory, ppdev->bg_print->ocfname, "gdev_prn_output_page_aux(ocfname)");
                     gs_free_object(ppdev->memory->non_gc_memory, ppdev->bg_print->obfname, "gdev_prn_output_page_aux(obfname)");
                     ppdev->bg_print->ocfname = ppdev->bg_print->obfname = NULL;

                    /* either bg_print was not requested or was not able to start */
                    if (ppdev->bg_print->sema != NULL && ppdev->bg_print->device != NULL) {
                        /* There was a problem. Teardown the device and its allocator, but */
                        /* leave the semaphore for possible later use.                     */
                        teardown_device_and_mem_for_thread(ppdev->bg_print->device,
                                                           ppdev->bg_print->thread_id, true);
                        ppdev->bg_print->device = NULL;
                    }
                }
                /* Here's where we actually let the device's print_page_copies work */
                /* Print the accumulated page description. */
                outcode = (*ppdev->printer_procs.print_page_copies)(ppdev, ppdev->file,
                                                          num_copies);
                gp_fflush(ppdev->file);
                errcode = (gp_ferror(ppdev->file) ? gs_note_error(gs_error_ioerror) : 0);
                /* NB: background printing does this differently in its thread */
                closecode = gdev_prn_close_printer(pdev);

            }
        }
    }
    /* In the case of a separation device, we need to make sure we */
    /* clear the separation info before the next page starts.      */
    pdevn_params = dev_proc(pdev, ret_devn_params)(pdev);
    if (pdevn_params != NULL) {
        /* Free up the list of spot names as they were only relevent to that page */
        free_separation_names(pdev->memory, &(pdevn_params->separations));
        pdevn_params->num_separation_order_names = 0;
    }
    endcode = (PRINTER_IS_CLIST(ppdev) &&
              !((gx_device_clist_common *)ppdev)->do_not_open_or_close_bandfiles ?
              clist_finish_page(pdev, flush) : 0);

    if (outcode < 0)
        return outcode;
    if (errcode < 0)
        return errcode;
    if (endcode < 0)
        return endcode;
    endcode = gx_finish_output_page(pdev, num_copies, flush);
    code = (endcode < 0 ? endcode : closecode < 0 ? closecode : 0);
    return code;
}

int
gdev_prn_output_page(gx_device * pdev, int num_copies, int flush)
{
    return(gdev_prn_output_page_aux(pdev, num_copies, flush, false, false));
}

int
gdev_prn_output_page_seekable(gx_device * pdev, int num_copies, int flush)
{
    return(gdev_prn_output_page_aux(pdev, num_copies, flush, true, false));
}

int
gdev_prn_bg_output_page(gx_device * pdev, int num_copies, int flush)
{
    return(gdev_prn_output_page_aux(pdev, num_copies, flush, false, true));
}

int
gdev_prn_bg_output_page_seekable(gx_device * pdev, int num_copies, int flush)
{
    return(gdev_prn_output_page_aux(pdev, num_copies, flush, true, true));
}

/* Print a single copy of a page by calling print_page_copies. */
int
gx_print_page_single_copy(gx_device_printer * pdev, gp_file * prn_stream)
{
    return pdev->printer_procs.print_page_copies(pdev, prn_stream, 1);
}

/* Print multiple copies of a page by calling print_page multiple times. */
int
gx_default_print_page_copies(gx_device_printer * pdev, gp_file * prn_stream,
                             int num_copies)
{
    int i = 1;
    int code = 0;

    for (; i < num_copies; ++i) {
        int errcode, closecode;

        code = (*pdev->printer_procs.print_page) (pdev, prn_stream);
        if (code < 0)
            return code;
        /*
         * Close and re-open the printer, to reset is_new and do the
         * right thing if we're producing multiple output files.
         * Code is mostly copied from gdev_prn_output_page.
         */
        gp_fflush(pdev->file);
        errcode =
            (gp_ferror(pdev->file) ? gs_note_error(gs_error_ioerror) : 0);
        closecode = gdev_prn_close_printer((gx_device *)pdev);
        pdev->PageCount++;
        code = (errcode < 0 ? errcode : closecode < 0 ? closecode :
                gdev_prn_open_printer((gx_device *)pdev, true));
        if (code < 0) {
            pdev->PageCount -= i;
            return code;
        }
        prn_stream = pdev->file;
    }
    /* Print the last (or only) page. */
    pdev->PageCount -= num_copies - 1;
    return (*pdev->printer_procs.print_page) (pdev, prn_stream);
}

/*
 * Print a page in the background. When printing is complete,
 * post the return code and signal the foreground (semaphore).
 * This is the procedure that is run in the background thread.
 */
static void
prn_print_page_in_background(void *data)
{
    bg_print_t *bg_print = (bg_print_t *)data;
    int code, errcode = 0;
    int num_copies = bg_print->num_copies;
    gx_device_printer *ppdev = (gx_device_printer *)bg_print->device;

    code = (*ppdev->printer_procs.print_page_copies)(ppdev, ppdev->file,
                                                          num_copies);
    gp_fflush(ppdev->file);

    errcode = (gp_ferror(ppdev->file) ? gs_note_error(gs_error_ioerror) : 0);
    bg_print->return_code = code < 0 ? code : errcode;

    /* Finally, release the foreground that may be waiting */
    gx_semaphore_signal(bg_print->sema);
}
/* ---------------- Driver services ---------------- */

/* Initialize a rendering plane specification. */
int
gx_render_plane_init(gx_render_plane_t *render_plane, const gx_device *dev,
                     int index)
{
    /*
     * Eventually the computation of shift and depth from dev and index
     * will be made device-dependent.
     */
    int num_planes = dev->color_info.num_components;
    int plane_depth = dev->color_info.depth / num_planes;

    if (index < 0 || index >= num_planes)
        return_error(gs_error_rangecheck);
    render_plane->index = index;
    render_plane->depth = plane_depth;
    render_plane->shift = plane_depth * (num_planes - 1 - index);
    return 0;
}

/* Clear trailing bits in the last byte of (a) scan line(s). */
void
gdev_prn_clear_trailing_bits(byte *data, uint raster, int height,
                             const gx_device *dev)
{
    int first_bit = dev->width * dev->color_info.depth;

    if (first_bit & 7)
        bits_fill_rectangle(data, first_bit, raster, mono_fill_make_pattern(0),
                            -first_bit & 7, height);
}

/* Return the number of scan lines that should actually be passed */
/* to the device. */
int
gdev_prn_print_scan_lines(gx_device * pdev)
{
    int height = pdev->height;
    gs_matrix imat;
    float yscale;
    int top, bottom, offset, end;

    (*dev_proc(pdev, get_initial_matrix)) (pdev, &imat);
    yscale = imat.yy * 72.0;	/* Y dpi, may be negative */
    top = (int)(dev_t_margin(pdev) * yscale);
    bottom = (int)(dev_b_margin(pdev) * yscale);
    offset = (int)(dev_y_offset(pdev) * yscale);
    if (yscale < 0) {		/* Y=0 is top of page */
        end = -offset + height + bottom;
    } else {			/* Y=0 is bottom of page */
        end = offset + height - top;
    }
    return min(height, end);
}

/* Open the current page for printing. */
int
gdev_prn_open_printer_seekable(gx_device *pdev, bool binary_mode,
                               bool seekable)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;

    if (ppdev->file != 0) {
        ppdev->file_is_new = false;
        return 0;
    }
    {
        int code = gx_device_open_output_file(pdev, ppdev->fname,
                                              binary_mode, seekable,
                                              &ppdev->file);
        if (code < 0)
            return code;

        if (seekable && !gp_fseekable(ppdev->file)) {
            errprintf(pdev->memory, "I/O Error: Output File \"%s\" must be seekable\n", ppdev->fname);
            if (!IS_LIBCTX_STDOUT(pdev->memory, gp_get_file(ppdev->file))
              && !IS_LIBCTX_STDERR(pdev->memory, gp_get_file(ppdev->file))) {

                code = gx_device_close_output_file(pdev, ppdev->fname, ppdev->file);
                if (code < 0)
                    return code;
            }
            ppdev->file = NULL;

            return_error(gs_error_ioerror);
        }
    }
    ppdev->file_is_new = true;

    return 0;
}
int
gdev_prn_open_printer(gx_device *pdev, bool binary_mode)
{
    return gdev_prn_open_printer_seekable(pdev, binary_mode, false);
}

/*
 * Test whether the printer's output file was just opened, i.e., whether
 * this is the first page being written to this file.  This is only valid
 * at the entry to a driver's print_page procedure.
 */
bool
gdev_prn_file_is_new(const gx_device_printer *pdev)
{
    return pdev->file_is_new;
}

/* Determine the colors used in a range of lines. */
/* FIXME: Currently, the page_info is ignored and the page_info from
 * the 'dev' parameter is used. For saved pages, the 'dev' may not
 * be a clist and the page_info should be used for clist file info
 * which may require a dummy gx_device_printer with the page_info
 * copied from the caller's data.
 */
int
gx_page_info_color_usage(const gx_device *dev,
                         const gx_band_page_info_t *page_info,
                         int y, int height,
                         gx_color_usage_t *color_usage, int *range_start)
{
    gx_device_clist_reader *crdev = (gx_device_clist_reader *)dev;
    int start, end, i;
    int band_height = page_info->band_params.BandHeight;
    gx_color_usage_bits or = 0;
    bool slow_rop = false;

    if (y < 0 || height < 0 || height > dev->height - y)
        return -1;
    start = y / band_height;
    end = (y + height + band_height - 1) / band_height;
    if (crdev->color_usage_array == NULL) {
        return -1;
    }
    for (i = start; i < end; ++i) {
        or |= crdev->color_usage_array[i].or;
        slow_rop |= crdev->color_usage_array[i].slow_rop;
    }
    color_usage->or = or;
    color_usage->slow_rop = slow_rop;
    *range_start = start * band_height;
    return min(end * band_height, dev->height) - *range_start;
}
int
gdev_prn_color_usage(gx_device *dev, int y, int height,
                     gx_color_usage_t *color_usage, int *range_start)
{
    gx_device_printer *pdev = (gx_device_printer *)dev;
    gx_device_clist *cdev = (gx_device_clist *)dev;
    gx_device_clist_writer *cldev = (gx_device_clist_writer *)dev;

    /* If this isn't a banded device, return default values. */
    if (!PRINTER_IS_CLIST(pdev)) {
        *range_start = 0;
        color_usage->or = gx_color_usage_all(dev);
        return dev->height;
    }
    if (y < 0 || height < 0 || height > dev->height - y)
        return -1;
    if (CLIST_IS_WRITER(cdev)) {
        /* Not expected to be used since usually this is called during reading */
        return clist_writer_color_usage(cldev, y, height, color_usage, range_start);
    } else
        return gx_page_info_color_usage(dev, &cldev->page_info,
                                 y, height, color_usage, range_start);
}

/*
 * Create the buffer device for a printer device.  Clients should always
 * call this, and never call the device procedure directly.
 */
int
gdev_create_buf_device(create_buf_device_proc_t cbd_proc, gx_device **pbdev,
                       gx_device *target, int y,
                       const gx_render_plane_t *render_plane,
                       gs_memory_t *mem, gx_color_usage_t *color_usage)
{
    int code = cbd_proc(pbdev, target, y, render_plane, mem, color_usage);

    if (code < 0)
        return code;
    /* Retain this device -- it will be freed explicitly. */
    gx_device_retain(*pbdev, true);
    return code;
}

/*
 * Create an ordinary memory device for page or band buffering,
 * possibly preceded by a plane extraction device.
 */
int
gx_default_create_buf_device(gx_device **pbdev, gx_device *target, int y,
    const gx_render_plane_t *render_plane, gs_memory_t *mem, gx_color_usage_t *color_usage)
{
    int plane_index = (render_plane ? render_plane->index : -1);
    int depth;
    const gx_device_memory *mdproto;
    gx_device_memory *mdev;
    gx_device *bdev;

    if (plane_index >= 0)
        depth = render_plane->depth;
    else {
        depth = target->color_info.depth;
        if (target->is_planar)
            depth /= target->color_info.num_components;
    }

    mdproto = gdev_mem_device_for_bits(depth);
    if (mdproto == 0)
        return_error(gs_error_rangecheck);
    if (mem) {
        mdev = gs_alloc_struct(mem, gx_device_memory, &st_device_memory,
                               "create_buf_device");
        if (mdev == 0)
            return_error(gs_error_VMerror);
    } else {
        mdev = (gx_device_memory *)*pbdev;
    }
    if (target == (gx_device *)mdev) {
        dev_t_proc_dev_spec_op((*orig_dso), gx_device) = dev_proc(mdev, dev_spec_op);
        /* The following is a special hack for setting up printer devices. */
        assign_dev_procs(mdev, mdproto);
        mdev->initialize_device_procs = mdproto->initialize_device_procs;
        mdev->initialize_device_procs((gx_device *)mdev);
        /* We know mdev->procs.initialize_device is NULL! */
        /* Do not override the dev_spec_op! */
        dev_proc(mdev, dev_spec_op) = orig_dso;
        check_device_separable((gx_device *)mdev);
        /* In order for saved-pages to work, we need to hook the dev_spec_op */
        if (mdev->procs.dev_spec_op == NULL || mdev->procs.dev_spec_op == gx_default_dev_spec_op)
            set_dev_proc(mdev, dev_spec_op, gdev_prn_dev_spec_op);
#ifdef DEBUG
        /* scanning sources didn't show anything, but if a device gets changed or added */
        /* that has its own dev_spec_op, it should call the gdev_prn_spec_op as well    */
        else {
            if (dev_proc(mdev, dev_spec_op)((gx_device *)mdev, gxdso_debug_printer_check, NULL, 0) < 0)
                errprintf(mdev->memory, "Warning: printer device has private dev_spec_op\n");
        }
#endif
        gx_device_fill_in_procs((gx_device *)mdev);
    } else {
        gs_make_mem_device(mdev, mdproto, mem, (color_usage == NULL ? 1 : 0),
                           target);
    }
    mdev->width = target->width;
    mdev->band_y = y;
    mdev->log2_align_mod = target->log2_align_mod;
    mdev->pad = target->pad;
    mdev->is_planar = target->is_planar;
    /*
     * The matrix in the memory device is irrelevant,
     * because all we do with the device is call the device-level
     * output procedures, but we may as well set it to
     * something halfway reasonable.
     */
    gs_deviceinitialmatrix(target, &mdev->initial_matrix);
    if (plane_index >= 0) {
        gx_device_plane_extract *edev;

        /* Guard against potential NULL dereference in gs_alloc_struct */
        if (!mem)
            return_error(gs_error_undefined);

        edev = gs_alloc_struct(mem, gx_device_plane_extract,
                            &st_device_plane_extract, "create_buf_device");

        if (edev == 0) {
            gx_default_destroy_buf_device((gx_device *)mdev);
            return_error(gs_error_VMerror);
        }
        edev->memory = mem;
        plane_device_init(edev, target, (gx_device *)mdev, render_plane,
                          false);
        bdev = (gx_device *)edev;
    } else
        bdev = (gx_device *)mdev;
    /****** QUESTIONABLE, BUT BETTER THAN OMITTING ******/
    if (&bdev->color_info != &target->color_info) /* Pacify Valgrind */
        bdev->color_info = target->color_info;
    *pbdev = bdev;
    return 0;
}

/* Determine the space needed by the buffer device. */
int
gx_default_size_buf_device(gx_device_buf_space_t *space, gx_device *target,
                           const gx_render_plane_t *render_plane,
                           int height, bool not_used)
{
    gx_device_memory mdev;

    space->line_ptrs = 0;	/*				*/
    space->bits = 0;		/* clear in case of failure	*/
    space->raster = 0;		/*				*/
    mdev.color_info.depth =
        (render_plane && render_plane->index >= 0 ? render_plane->depth :
         target->color_info.depth);
    mdev.color_info.num_components = target->color_info.num_components;
    mdev.width = target->width;
    mdev.is_planar = target->is_planar;
    mdev.pad = target->pad;
    mdev.log2_align_mod = target->log2_align_mod;
    if (gdev_mem_bits_size(&mdev, target->width, height, &(space->bits)) < 0)
        return_error(gs_error_VMerror);
    space->line_ptrs = gdev_mem_line_ptrs_size(&mdev, target->width, height);
    space->raster = gdev_mem_raster(&mdev);
    return 0;
}

/* Set up the buffer device. */
int
gx_default_setup_buf_device(gx_device *bdev, byte *buffer, int raster,
                            byte **line_ptrs, int y, int setup_height,
                            int full_height)
{
    gx_device_memory *mdev =
        (gs_device_is_memory(bdev) ? (gx_device_memory *)bdev :
         (gx_device_memory *)(((gx_device_plane_extract *)bdev)->plane_dev));
    byte **ptrs = line_ptrs;
    int code;

    if (ptrs == 0) {
        /*
         * Before allocating a new line pointer array, if there is a previous
         * array, free it to prevent leaks.
         */
        if (mdev->line_ptrs != NULL)
            gs_free_object(mdev->line_pointer_memory, mdev->line_ptrs,
                       "mem_close");
        /*
         * Allocate line pointers now; free them when we close the device.
         * Note that for multi-planar devices, we have to allocate using
         * full_height rather than setup_height.
         */
        ptrs = (byte **)
            gs_alloc_byte_array(mdev->memory,
                                (mdev->is_planar ?
                                 full_height * mdev->color_info.num_components :
                                 setup_height),
                                sizeof(byte *), "setup_buf_device");
        if (ptrs == 0)
            return_error(gs_error_VMerror);
        mdev->line_pointer_memory = mdev->memory;
        mdev->foreign_line_pointers = false;
    }
    mdev->height = full_height;
    code = gdev_mem_set_line_ptrs(mdev, buffer + raster * y, raster,
                                  ptrs, setup_height);
    mdev->height = setup_height;
    bdev->height = setup_height; /* do here in case mdev == bdev */
    return code;
}

/* Destroy the buffer device. */
void
gx_default_destroy_buf_device(gx_device *bdev)
{
    gx_device *mdev = bdev;

    if (!gs_device_is_memory(bdev)) {
        /* bdev must be a plane extraction device. */
        mdev = ((gx_device_plane_extract *)bdev)->plane_dev;
        gs_free_object(bdev->memory, bdev, "destroy_buf_device");
    }
    /* gs_free_object will do finalize which will decrement icc profile */
    dev_proc(mdev, close_device)(mdev);
    gs_free_object(mdev->memory, mdev, "destroy_buf_device");
}

/*
 * Copy one or more rasterized scan lines to a buffer, or return a pointer
 * to them.  See gdevprn.h for detailed specifications.
 */
int
gdev_prn_get_lines(gx_device_printer *pdev, int y, int height,
                   byte *buffer, uint bytes_per_line,
                   byte **actual_buffer, uint *actual_bytes_per_line,
                   const gx_render_plane_t *render_plane)
{
    int code;
    gs_int_rect rect;
    gs_get_bits_params_t params;
    int plane;

    if (y < 0 || height < 0 || y + height > pdev->height)
        return_error(gs_error_rangecheck);
    rect.p.x = 0, rect.p.y = y;
    rect.q.x = pdev->width, rect.q.y = y + height;
    params.options =
        GB_RETURN_POINTER | GB_ALIGN_STANDARD | GB_OFFSET_0 |
        GB_RASTER_ANY |
        /* No depth specified, we always use native colors. */
        GB_COLORS_NATIVE | GB_ALPHA_NONE;
    if (render_plane) {
        params.options |= GB_PACKING_PLANAR | GB_SELECT_PLANES;
        memset(params.data, 0,
               sizeof(params.data[0]) * pdev->color_info.num_components);
        plane = render_plane->index;
        params.data[plane] = buffer;
    } else {
        params.options |= GB_PACKING_CHUNKY;
        params.data[0] = buffer;
        plane = 0;
    }
    params.x_offset = 0;
    params.raster = bytes_per_line;
    code = dev_proc(pdev, get_bits_rectangle)
        ((gx_device *)pdev, &rect, &params);
    if (code < 0 && actual_buffer) {
        /*
         * RETURN_POINTER might not be implemented for this
         * combination of parameters: try RETURN_COPY.
         */
        params.options &= ~(GB_RETURN_POINTER | GB_RASTER_ALL);
        params.options |= GB_RETURN_COPY | GB_RASTER_SPECIFIED;
        code = dev_proc(pdev, get_bits_rectangle)
            ((gx_device *)pdev, &rect, &params);
    }
    if (code < 0)
        return code;
    if (actual_buffer)
        *actual_buffer = params.data[plane];
    if (actual_bytes_per_line)
        *actual_bytes_per_line = params.raster;
    return code;
}

/* Copy a scan line from the buffer to the printer. */
int
gdev_prn_get_bits(gx_device_printer * pdev, int y, byte * str, byte ** actual_data)
{
    int code;
    uint line_size = gdev_prn_raster(pdev);
    int last_bits = -(pdev->width * pdev->color_info.depth) & 7;
    gs_int_rect rect;
    gs_get_bits_params_t params;

    rect.p.x = 0;
    rect.p.y = y;
    rect.q.x = pdev->width;
    rect.q.y = y+1;

    params.options = (GB_ALIGN_ANY |
                      GB_RETURN_COPY |
                      GB_OFFSET_0 |
                      GB_RASTER_STANDARD | GB_PACKING_CHUNKY |
                      GB_COLORS_NATIVE | GB_ALPHA_NONE);
    if (actual_data)
        params.options |=  GB_RETURN_POINTER;
    params.x_offset = 0;
    params.raster = bitmap_raster(pdev->width * pdev->color_info.depth);
    params.data[0] = str;
    code = (*dev_proc(pdev, get_bits_rectangle))((gx_device *)pdev, &rect,
                                                 &params);
    if (code < 0)
        return code;
    if (actual_data)
        *actual_data = params.data[0];
    if (last_bits != 0) {
        byte *dest = (actual_data != NULL ? *actual_data : str);

        dest[line_size - 1] &= 0xff << last_bits;
    }
    return 0;
}
/* Copy scan lines to a buffer.  Return the number of scan lines, */
/* or <0 if error.  This procedure is DEPRECATED. */
/* Some old and contrib drivers ignore error codes, so make sure and fill */
/* remaining lines if we get an error (and for lines past end of page).   */
int
gdev_prn_copy_scan_lines(gx_device_printer * pdev, int y, byte * str, uint size)
{
    uint line_size = gdev_prn_raster(pdev);
    int requested_count = size / line_size;
    int i, count;
    int code = 0;
    byte *dest = str;

    /* Clamp count between 0 and remaining lines on page so we don't return < 0 */
    /* unless gdev_prn_get_bits returns an error */
    count = max(0, min(requested_count, pdev->height - y));
    for (i = 0; i < count; i++, dest += line_size) {
        code = gdev_prn_get_bits(pdev, y + i, dest, NULL);
        if (code < 0)
            break;	/* will fill remaining lines and return code outside the loop */
    }
    /* fill remaining lines with 0's to prevent printing garbage */
    memset(dest, 0, (size_t)line_size * (requested_count - i));
    return (code < 0 ) ? code : count;
}

/* Close the current page. */
int
gdev_prn_close_printer(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    gs_parsed_file_name_t parsed;
    const char *fmt;
    int code = gx_parse_output_file_name(&parsed, &fmt, ppdev->fname,
                                         strlen(ppdev->fname), pdev->memory);

    if ((code >= 0 && fmt) /* file per page */ ||
        ppdev->ReopenPerPage	/* close and reopen for each page */
        ) {
        gx_device_close_output_file(pdev, ppdev->fname, ppdev->file);
        ppdev->file = NULL;
    }
    return 0;
}

/* If necessary, free and reallocate the printer memory after changing params */
int
gdev_prn_maybe_realloc_memory(gx_device_printer *prdev,
                              gdev_space_params *old_sp,
                              int old_width, int old_height,
                              bool old_page_uses_transparency)
{
    int code = 0;
    gx_device *const pdev = (gx_device *)prdev;
    /*gx_device_memory * const mdev = (gx_device_memory *)prdev;*/

    /*
     * The first test was changed to mdev->base != 0 in 5.50 (per Artifex).
     * Not only was this test wrong logically, it was incorrect in that
     * casting pdev to a (gx_device_memory *) is only meaningful if banding
     * is not being used.  The test was changed back to prdev->is_open in
     * 5.67 (also per Artifex).  For more information, see the News items
     * for these filesets.
     */
    if (prdev->is_open &&
        (gdev_space_params_cmp(prdev->space_params, *old_sp) != 0 ||
         prdev->width != old_width || prdev->height != old_height ||
         prdev->page_uses_transparency != old_page_uses_transparency)
        ) {
        int new_width = prdev->width;
        int new_height = prdev->height;
        gdev_space_params new_sp;

#ifdef DEBUGGING_HACKS
debug_dump_bytes(pdev->memory, (const byte *)old_sp, (const byte *)(old_sp + 1), "old");
debug_dump_bytes(pddev->memory, (const byte *)&prdev->space_params,
                 (const byte *)(&prdev->space_params + 1), "new");
dmprintf4(pdev->memory, "w=%d/%d, h=%d/%d\n", old_width, new_width, old_height, new_height);
#endif /*DEBUGGING_HACKS*/
        new_sp = prdev->space_params;
        prdev->width = old_width;
        prdev->height = old_height;
        prdev->space_params = *old_sp;
        code = gdev_prn_reallocate_memory(pdev, &new_sp,
                                          new_width, new_height);
        /* If this fails, device should be usable w/old params, but */
        /* band files may not be open. */
    }
    return code;
}

void
gdev_prn_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, gdev_prn_open);
    set_dev_proc(dev, close_device, gdev_prn_close);
    set_dev_proc(dev, output_page, gdev_prn_output_page);
    set_dev_proc(dev, get_params, gdev_prn_get_params);
    set_dev_proc(dev, put_params, gdev_prn_put_params);
    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
    set_dev_proc(dev, dev_spec_op, gdev_prn_dev_spec_op);
    set_dev_proc(dev, map_rgb_color, gdev_prn_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gdev_prn_map_color_rgb);
    set_dev_proc(dev, encode_color, gdev_prn_map_rgb_color);
    set_dev_proc(dev, decode_color, gdev_prn_map_color_rgb);
}

void gdev_prn_initialize_device_procs_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}

void
gdev_prn_initialize_device_procs_mono(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, gdev_prn_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gdev_prn_map_color_rgb);
    set_dev_proc(dev, encode_color, gdev_prn_map_rgb_color);
    set_dev_proc(dev, decode_color, gdev_prn_map_color_rgb);
}

void gdev_prn_initialize_device_procs_mono_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs_mono(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}

void
gdev_prn_initialize_device_procs_gray(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, gx_default_gray_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_gray_map_color_rgb);
    set_dev_proc(dev, encode_color, gx_default_gray_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_gray_map_color_rgb);
}

void gdev_prn_initialize_device_procs_gray_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}

void gdev_prn_initialize_device_procs_rgb(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, map_color_rgb, gx_default_rgb_map_color_rgb);
    set_dev_proc(dev, encode_color, gx_default_rgb_map_rgb_color);
    set_dev_proc(dev, decode_color, gx_default_rgb_map_color_rgb);
}

void gdev_prn_initialize_device_procs_rgb_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}

void
gdev_prn_initialize_device_procs_gray8(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_rgb_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, map_color_rgb, gx_default_8bit_map_color_gray);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

void gdev_prn_initialize_device_procs_gray8_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray8(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}

void gdev_prn_initialize_device_procs_cmyk1(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_cmyk_color, cmyk_1bit_map_cmyk_color);
    set_dev_proc(dev, map_color_rgb, cmyk_1bit_map_color_rgb);
    set_dev_proc(dev, encode_color, cmyk_1bit_map_cmyk_color);
    set_dev_proc(dev, decode_color, cmyk_1bit_map_color_cmyk);
}

void gdev_prn_initialize_device_procs_cmyk1_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk1(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}

void gdev_prn_initialize_device_procs_cmyk8(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_cmyk_color, cmyk_8bit_map_cmyk_color);
    set_dev_proc(dev, map_color_rgb, cmyk_8bit_map_color_rgb);
    set_dev_proc(dev, encode_color, cmyk_8bit_map_cmyk_color);
    set_dev_proc(dev, decode_color, cmyk_8bit_map_color_cmyk);
}

void gdev_prn_initialize_device_procs_cmyk8_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}

void gdev_prn_initialize_device_procs_cmyk16(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, map_cmyk_color, cmyk_16bit_map_cmyk_color);
    set_dev_proc(dev, map_color_rgb, cmyk_16bit_map_color_rgb);
    set_dev_proc(dev, encode_color, cmyk_16bit_map_cmyk_color);
    set_dev_proc(dev, decode_color, cmyk_16bit_map_color_cmyk);
}

void gdev_prn_initialize_device_procs_cmyk16_bg(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk16(dev);

    set_dev_proc(dev, output_page, gdev_prn_bg_output_page);
}
