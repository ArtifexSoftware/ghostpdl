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


/* Page object management */
#include "gdevprn.h"
#include "gdevdevn.h"
#include "gxcldev.h"
#include "gxclpage.h"
#include "gsicc_cache.h"
#include "gsparams.h"
#include "string_.h"
#include <ctype.h>	/* for isalpha, etc. */

/* Save the current clist state into a saved page structure,
 * and optionally stashes the files into the given save_files
 * pointers.
 * Does NOT alter the clist files. */
/* RJW: Does too. The opendevice call at the end calls clist_open */
static int
do_page_save(gx_device_printer * pdev, gx_saved_page * page, clist_file_ptr *save_files)
{
    gx_device_clist *cdev = (gx_device_clist *) pdev;
    gx_device_clist_writer * const pcldev = (gx_device_clist_writer *)pdev;
    int code;
    gs_c_param_list paramlist;
    gs_devn_params *pdevn_params;

    /* Save the device information. */
    strncpy(page->dname, pdev->dname, sizeof(page->dname)-1);
    page->color_info = pdev->color_info;
    page->tag = pdev->graphics_type_tag;
    page->io_procs = cdev->common.page_info.io_procs;
    /* Save the page information. */
    strncpy(page->cfname, pcldev->page_info.cfname, sizeof(page->cfname)-1);
    strncpy(page->bfname, pcldev->page_info.bfname, sizeof(page->bfname)-1);
    page->bfile_end_pos = pcldev->page_info.bfile_end_pos;
    if (save_files != NULL) {
      save_files[0] =  pcldev->page_info.cfile;
      save_files[1] =  pcldev->page_info.bfile;
      pcldev->page_info.cfile = NULL;
      pcldev->page_info.bfile = NULL;
    }
    pcldev->page_info.cfname[0] = 0;
    pcldev->page_info.bfname[0] = 0;
    page->tile_cache_size = pcldev->page_info.tile_cache_size;
    page->band_params = pcldev->page_info.band_params;
    /* Now serialize and save the rest of the information from the device params */
    /* we count on this to correctly set the color_info, devn_params and icc_struct */
    page->mem = pdev->memory->non_gc_memory;
    gs_c_param_list_write(&paramlist, pdev->memory);
    if ((code = gs_getdeviceparams((gx_device *)pdev, (gs_param_list *)&paramlist)) < 0) {
        goto params_out;
    }
    /* fetch bytes needed for param list */
    gs_c_param_list_read(&paramlist);
    if ((code = gs_param_list_serialize((gs_param_list *)&paramlist, NULL, 0)) < 0) {
        goto params_out;
    }
    page->paramlist_len = code;
    if ((page->paramlist = gs_alloc_bytes(page->mem,
                                           page->paramlist_len,
                                           "saved_page paramlist")) == NULL) {
        code = gs_error_VMerror;
        goto params_out;
    }
    code = gs_param_list_serialize((gs_param_list *)&paramlist, page->paramlist,
                                   page->paramlist_len);
params_out:
    gs_c_param_list_release(&paramlist);
    if (code < 0)
        return code;			/* all device param errors collect here */

    /* Save other information. */
    /* If this device has spot colors that were added dynamically, we need to pass the names */
    /* through as well. These are from the devn_params->separations->names array.            */
    if ((pdevn_params = dev_proc(pdev, ret_devn_params)((gx_device *)pdev)) != NULL) {
        int i;

        page->num_separations = pdevn_params->separations.num_separations;
        for (i=0; i < page->num_separations; i++) {
            page->separation_name_sizes[i] = pdevn_params->separations.names[i].size;
            page->separation_names[i] = gs_alloc_bytes(page->mem, page->separation_name_sizes[i],
                                                       "saved_page separation_names");
            if (page->separation_names[i] == NULL) {
                gs_free_object(page->mem, page->paramlist, "saved_page paramlist");
                while (--i > 0)
                    gs_free_object(page->mem, page->separation_names[i],
                                   "saved_page separation_names");
                return gs_error_VMerror;
            }
            memcpy(page->separation_names[i], pdevn_params->separations.names[i].data,
                   page->separation_name_sizes[i]);
        }
    }
    /* Now re-open the clist device so that we get new files for the next page */
    return (*gs_clist_device_procs.open_device) ((gx_device *) pdev);
}

/* Save a page. The elements are allocated by this function in non_gc_memory */
int
gdev_prn_save_page(gx_device_printer * pdev, gx_saved_page * page)
{
    gx_device_clist *cdev = (gx_device_clist *) pdev;
    gx_device_clist_writer * const pcldev = (gx_device_clist_writer *)pdev;
    int code;

    /* Make sure we are banding. */
    if (!PRINTER_IS_CLIST(pdev))
        return_error(gs_error_rangecheck);

    if ((code = clist_end_page(pcldev)) < 0 ||
        (code = cdev->common.page_info.io_procs->fclose(pcldev->page_info.cfile, pcldev->page_info.cfname, false)) < 0 ||
        (code = cdev->common.page_info.io_procs->fclose(pcldev->page_info.bfile, pcldev->page_info.bfname, false)) < 0
        )
        return code;
    return do_page_save(pdev, page, NULL);
}

/* Render an array of saved pages. */
int
gdev_prn_render_pages(gx_device_printer * pdev,
                      const gx_placed_page * ppages, int count)
{
    gx_device_clist_reader * const pcldev =
        (gx_device_clist_reader *)pdev;

    /* Check to make sure the pages are compatible with the device. */
    {
        int i;

        for (i = 0; i < count; ++i) {
            const gx_saved_page *page = ppages[i].page;

            /* We would like to fully check the color representation, */
            /* but we don't have enough information to do that. */
            if (strcmp(page->dname, pdev->dname) != 0 ||
                !gx_color_info_equal(&page->color_info, &pdev->color_info)
                )
                return_error(gs_error_rangecheck);
            /* Currently we don't allow translation in Y. */
            if (ppages[i].offset.y != 0)
                return_error(gs_error_rangecheck);
            /* Make sure the band parameters are compatible. */
            if (page->band_params.BandBufferSpace !=
                pdev->buffer_space ||
                page->band_params.BandWidth !=
                pdev->width
                )
                return_error(gs_error_rangecheck);
            /* Currently we require all band heights to be the same. */
            if (i > 0 && page->band_params.BandHeight !=
                         ppages[0].page->band_params.BandHeight)
                return_error(gs_error_rangecheck);
        }
    }
    /* Set up the page list in the device. */
    /****** SHOULD FACTOR THIS OUT OF clist_render_init ******/
    pcldev->ymin = pcldev->ymax = 0;
    pcldev->pages = ppages;
    pcldev->num_pages = count;
    pcldev->offset_map = NULL;
    pcldev->icc_table = NULL;		/* FIXME: output_page doesn't load these */
    pcldev->icc_cache_cl = NULL;	/* FIXME: output_page doesn't load these */
    /* Render the pages. */
    {
        int code = (*dev_proc(pdev, output_page))
            ((gx_device *) pdev, (pdev->IgnoreNumCopies || pdev->NumCopies_set <= 0) ? 1 : pdev->NumCopies, true);

        /* Delete the temporary files and free the paramlist. */
        int i;

        for (i = 0; i < count; ++i) {
            gx_saved_page *page = ppages[i].page;

            pcldev->page_info.io_procs->unlink(page->cfname);
            pcldev->page_info.io_procs->unlink(page->bfname);
            gs_free_object(page->mem, page->paramlist, "gdev_prn_render_pages");
            page->paramlist = NULL;
        }
        return code;
    }
}

/*
 * Allocate and initialize the list structure
 */
gx_saved_pages_list *
gx_saved_pages_list_new(gx_device_printer *pdev)
{
    gx_saved_pages_list *newlist;
    gs_memory_t *non_gc_mem = pdev->memory->non_gc_memory;

    if ((newlist =
         (gx_saved_pages_list *)gs_alloc_bytes(non_gc_mem,
                                               sizeof(gx_saved_pages_list),
                                               "gx_saved_pages_list_new")) == NULL)
        return NULL;

    memset(newlist, 0, sizeof(gx_saved_pages_list));
    newlist->mem = non_gc_mem;
    newlist->PageCount = pdev->PageCount;	/* PageCount when list created */
    newlist->collated_copies = 1;
    return newlist;
}

/*
 * Add a new saved page to the end of an in memory list. Refer to the
 * documentation for gx_saved_pages_list. This allocates the saved_
 * page as well as the saved_pages_list_element and relies on the
 * gdev_prn_save_page to use non_gc_memory since this list is never
 * in GC memory.
 */
int
gx_saved_pages_list_add(gx_device_printer * pdev)
{
    gx_saved_pages_list *list = pdev->saved_pages_list;
    gx_saved_pages_list_element *new_list_element;
    gx_saved_page *newpage;
    int code;

    if ((newpage =
         (gx_saved_page *)gs_alloc_bytes(list->mem,
                                         sizeof(gx_saved_page),
                                         "gx_saved_pages_list_add")) == NULL)
        return_error (gs_error_VMerror);

    if ((new_list_element =
         (gx_saved_pages_list_element *)gs_alloc_bytes(list->mem,
                                                      sizeof(gx_saved_pages_list_element),
                                                      "gx_saved_pages_list_add")) == NULL) {
        gs_free_object(list->mem, newpage, "gx_saved_pages_list_add");
        return_error (gs_error_VMerror);
    }

    if ((code = gdev_prn_save_page(pdev, newpage)) < 0) {
        gs_free_object(list->mem, new_list_element, "gx_saved_pages_list_add");
        gs_free_object(list->mem, newpage, "gx_saved_pages_list_add");
        return code;
    }
    new_list_element->sequence_number = ++list->count;
    new_list_element->page = newpage;
    new_list_element->next = NULL;
    if (list->tail == NULL) {
        /* list was empty, start it */
        new_list_element->prev = NULL;
        list->head = list->tail = new_list_element;
    } else {
        /* place as new tail */
        new_list_element->prev = list->tail;
        list->tail->next = new_list_element;
        list->tail = new_list_element;
    }
    return 0;			/* success */
}

/* Free the contents of all saved pages, unlink the files and free the
 * saved_page structures. Does not free the gx_saved_pages_list struct.
 */
void
gx_saved_pages_list_free(gx_saved_pages_list *list)
{
    gx_saved_pages_list_element *curr_elem = list->head;
    while (curr_elem != NULL) {
        gx_saved_page *curr_page;
        gx_saved_pages_list_element *next_elem;

        curr_page  = curr_elem->page;
        curr_page->io_procs->unlink(curr_page->cfname);
        curr_page->io_procs->unlink(curr_page->bfname);
        gs_free_object(curr_page->mem, curr_page->paramlist, "gx_saved_pages_list_free");
        gs_free_object(list->mem, curr_page, "gx_saved_pages_list_free");

        next_elem = curr_elem->next;
        gs_free_object(list->mem, curr_elem, "gx_saved_pages_list_free");
        curr_elem = next_elem;
    };

    /* finally free the list itself */
    gs_free_object(list->mem, list, "gx_saved_pages_list_free");
}


/* This enum has to be in the same order as saved_pages_keys */
typedef enum {
    PARAM_UNKNOWN = 0,
    PARAM_BEGIN,
    PARAM_END,
    PARAM_FLUSH,
    PARAM_PRINT,
    PARAM_COPIES,
    PARAM_NORMAL,
    PARAM_REVERSE,
    PARAM_EVEN,
    PARAM_EVEN0PAD,
    PARAM_ODD,
    /* any new keywords precede these */
    PARAM_NUMBER,
    PARAM_DASH,
    PARAM_STAR
} saved_pages_key_enum;

static saved_pages_key_enum
param_find_key(byte *token, int token_size)
{
    int i;
    static const char *saved_pages_keys[] = {
        "begin", "end", "flush", "print", "copies", "normal", "reverse", "even", "even0pad", "odd"
    };
    saved_pages_key_enum found = PARAM_UNKNOWN;

    if (*token >= '0' && *token <= '9')
        return PARAM_NUMBER;
    if (*token == '-')
        return PARAM_DASH;
    if (*token == (byte)'*')
        return PARAM_STAR;

    for (i=0; i < (sizeof(saved_pages_keys)/sizeof(saved_pages_keys[0])); i++) {
        if (strncasecmp((char *)token, saved_pages_keys[i], token_size) == 0) {
            found = (saved_pages_key_enum) (i + 1);
            break;
         }
    }
    return found;
}

/* Find next token, skipping any leading whitespace or non-alphanumeric */
/* Returns pointer to next token and updates 'token_size'. Caller can   */
/* use (token - param) + token_size  to update to next token in the     */
/* param string (param) and remaining char count (param_left)           */
/* Returns NULL and token_size =0 if no more alphanumeric tokens        */
static byte *
param_parse_token(byte *param, int param_left, int *token_size)
{
    int token_len = 0;
    byte *token = param;
    bool single_char_token = true;

    /* skip leading junk */
    while (param_left > 0) {
        if (isalnum(*token)) {
            single_char_token = false;	/* we'll scan past this keyword */
            break;
        }
        if (*token == (byte)'-')	/* valid in page range */
            break;
        if (*token == (byte)'*')	/* valid in page range */
            break;
        /* skip any other junk */
        token++;
        param_left--;
    }
    if (param_left == 0) {
        *token_size = 0;	/* no token found */
        return NULL;		/* No more items */
    }
    if (single_char_token) {
        param_left--;		/* we've consumed one character */
        *token_size = 1;
        return token;
    }

    /* token points to start, skip valid alphanumeric characters after */
    /* the first. Single character tokens terminate this scan.         */
    while (param_left > 0) {
        if (!isalnum(token[token_len]))
            break;
        token_len++;
        param_left--;
    }
    *token_size = token_len;
    return token;
}

static int
do_page_load(gx_device_printer *pdev, gx_saved_page *page, clist_file_ptr *save_files)
{
    int code;
    gx_device_clist_reader *crdev = (gx_device_clist_reader *)pdev;
    gs_c_param_list paramlist;
    gs_devn_params *pdevn_params;

    /* fetch and put the params we saved with the page */
    gs_c_param_list_write(&paramlist, pdev->memory);
    if ((code = gs_param_list_unserialize((gs_param_list *)&paramlist, page->paramlist)) < 0)
        goto out;
    gs_c_param_list_read(&paramlist);
    code = gs_putdeviceparams((gx_device *)pdev, (gs_param_list *)&paramlist);
    gs_c_param_list_release(&paramlist);
    if (code < 0) {
        goto out;
    }
    /* if this is a DeviceN device (that supports spot colors), we need to load the */
    /* devn_params saved in the page (num_separations, separations[])               */
    if ((pdevn_params = dev_proc(pdev, ret_devn_params)((gx_device *)pdev)) != NULL) {
        int i;

        pdevn_params->separations.num_separations = page->num_separations;
        for (i=0; i < page->num_separations; i++) {
            pdevn_params->separations.names[i].size = page->separation_name_sizes[i];
            pdevn_params->separations.names[i].data = gs_alloc_bytes(pdev->memory->stable_memory,
                                                                     page->separation_name_sizes[i],
                                                                     "saved_page separation_names");
            if (pdevn_params->separations.names[i].data == NULL) {
                while (--i > 0)
                    gs_free_object(pdev->memory->stable_memory,
                                   pdevn_params->separations.names[i].data,
                                   "saved_page separation_names");
                code = gs_error_VMerror;
                goto out;
            }
            memcpy(pdevn_params->separations.names[i].data, page->separation_names[i],
                   page->separation_name_sizes[i]);
        }
    }
    if (code > 0)
        if ((code = gs_opendevice((gx_device *)pdev)) < 0)
            goto out;

    /* If the device is now a writer, that means putparams realloced the device */
    /* so we need to get back to reader mode and remove the extra clist files  */
    if (CLIST_IS_WRITER((gx_device_clist *)pdev)) {
        if ((code = clist_close_writer_and_init_reader((gx_device_clist *)crdev)) < 0)
            goto out;
        /* close and unlink the temp files just created */
        if (crdev->page_info.cfile != NULL)
            crdev->page_info.io_procs->fclose(crdev->page_info.cfile, crdev->page_info.cfname, true);
        if (crdev->page_info.bfile != NULL)
            crdev->page_info.io_procs->fclose(crdev->page_info.bfile, crdev->page_info.bfname, true);
        crdev->page_info.cfile = crdev->page_info.bfile = NULL;
    }

    /* set up the page_info, after putdeviceparams that may have changed things */
    crdev->page_info.io_procs = page->io_procs;
    crdev->page_info.tile_cache_size = page->tile_cache_size;
    crdev->page_info.bfile_end_pos = page->bfile_end_pos;
    crdev->page_info.band_params = page->band_params;
    crdev->graphics_type_tag = page->tag;

    crdev->yplane.index = -1;
    crdev->pages = NULL;
    crdev->num_pages = 1;		/* single page at a time */
    crdev->offset_map = NULL;
    crdev->render_threads = NULL;
    crdev->ymin = crdev->ymax = 0;      /* invalidate buffer contents to force rasterizing */

    /* We probably don't need to copy in the filenames, but do it in case something expects it */
    strncpy(crdev->page_info.cfname, page->cfname, sizeof(crdev->page_info.cfname)-1);
    strncpy(crdev->page_info.bfname, page->bfname, sizeof(crdev->page_info.bfname)-1);
    if (save_files != NULL)
    {
        crdev->page_info.cfile = save_files[0];
        crdev->page_info.bfile = save_files[1];
    }
out:
    return code;
}

static int
gx_saved_page_load(gx_device_printer *pdev, gx_saved_page *page)
{
    int code;
    gx_device_clist_reader *crdev = (gx_device_clist_reader *)pdev;

    code = do_page_load(pdev, page, NULL);
    if (code < 0)
        return code;

    /* Now open this page's files */
    code = crdev->page_info.io_procs->fopen(crdev->page_info.cfname,
               gp_fmode_rb, &(crdev->page_info.cfile), crdev->bandlist_memory,
               crdev->bandlist_memory, true);
    if (code >= 0) {
        code = crdev->page_info.io_procs->fopen(crdev->page_info.bfname,
                   gp_fmode_rb, &(crdev->page_info.bfile), crdev->bandlist_memory,
                   crdev->bandlist_memory, false);
    }

    return code;
}

static int
gx_output_saved_page(gx_device_printer *pdev, gx_saved_page *page)
{
    int code, ecode;
    /* Note that banding_type is NOT a device parameter handled in the paramlist */
    gdev_banding_type save_banding_type = pdev->space_params.banding_type;
    gx_device_clist_reader *crdev = (gx_device_clist_reader *)pdev;

    pdev->space_params.banding_type = BandingAlways;

    if ((code = gx_saved_page_load(pdev, page)) < 0)
        goto out;

    /* don't want the band files touched after printing */
    crdev->do_not_open_or_close_bandfiles = true;

    /* load the color_usage_array */
    if ((code = clist_read_color_usage_array(crdev)) < 0)
        goto out;
    if ((code = clist_read_icctable(crdev)) < 0)
        goto out;
    code = (crdev->icc_cache_cl = gsicc_cache_new(crdev->memory)) == NULL ?
           gs_error_VMerror : code;
    if (code < 0)
        goto out;

    /* After setting params, make sure bg_printing is off */
    pdev->bg_print_requested = false;

    /* Note: we never flush pages allowing for re-printing from the list */
    /* data (files) will be deleted when the list is flushed or freed.   */
    code = (*dev_proc(pdev, output_page)) ((gx_device *) pdev,
               (pdev->IgnoreNumCopies || pdev->NumCopies_set <= 0) ? 1 : pdev->NumCopies, false);

    clist_free_icc_table(crdev->icc_table, crdev->memory);
    crdev->icc_table = NULL;
    rc_decrement(crdev->icc_cache_cl,"clist_finish_page");
    crdev->icc_cache_cl = NULL;

    /* Close the clist files */
    ecode = crdev->page_info.io_procs->fclose(crdev->page_info.cfile, crdev->page_info.cfname, false);
    if (ecode >= 0) {
        crdev->page_info.cfile = NULL;
        ecode = crdev->page_info.io_procs->fclose(crdev->page_info.bfile, crdev->page_info.bfname, false);
    }
    if (ecode < 0) {
        code = ecode;
        goto out;
    }
    crdev->page_info.bfile = NULL;

out:
    pdev->space_params.banding_type = save_banding_type;
    return code;
}

/*
 * Print selected pages from the list to on the selected device. The
 * saved_pages_list is NOT modified, allowing for reprint / recovery
 * print. Each saved_page is printed on a separate page (unlike the
 * gdev_prn_render_pages above which prints several saved_pages on
 * a single "master" page, performing imposition).
 *
 * This is primarily intended to allow printing in non-standard order
 * (reverse, odd, even) or producing collated copies for a job.
 *
 * On success return the number of bytes consumed or error code < 0.
 * The printed_count will contain the number of pages printed.
 *
 * -------------------------------------------------------------------
 *
 * The param string may begin with whitespace. The string is parsed
 * and the selected pages are printed. There may be any number of ranges
 * and or keywords separated by whitespace and/or comma ','.
 *
 * NB: The pdev printer device's PageCount is updated to reflect the
 *     total number of pages produced (per the spec for this parameter)
 *     since we may be producing multiple collated copies.
 *     Also the list PageCount is updated after printing since
 *     there may be a subsequent 'print' action.
 * keywords:
 *	copies #	Set the number of copies for subsequent printing actions
 *                      "copies 1" resets to a single copy
 *	normal		All pages are printed in normal order
 *	reverse		All pages are printed in reverse order
 *         The following two options may be useful for duplexing.
 *	odd		All odd pages are printed, e.g. 1, 3, 5, ...
 *	even		All even pages are printed, e.g. 2, 4, 6, ...
 *                      NB: an extra blank page will be printed if the list
 *                      has an odd number of pages.
 *      even0pad        All even pages, but no extra blank page if there are
 *                      an odd number of pages on the list.
 * range syntax:
 *	range range	multiple ranges are separated by commas ','
 *			and/or whitespace.
 *	#		a specific page number is a valid range
 *
 *	inclusive ranges below can have whitespace before
 *	or after the dash '-'.
 *	#-#		a range consisting of all pages from the first
 *			page # up to (and including) the second page #.
 *	#-*		all pages from # up through the last page
 *			"1-*" is equivalent to "normal"
 *	*-#		all pages from the last up through # page.
 *			"reverse" is equivalent to "*-1"
 */
int
gx_saved_pages_list_print(gx_device_printer *pdev, gx_saved_pages_list *list,
                          byte *param, int param_size, int *printed_count)
{
    byte *param_scan = NULL;
    int param_left;
    int start_page = 0;				/* 0 means start_page unknown */
    int end_page = 0;				/* < 0 means waiting for the end of a # - # range */
                                                /* i.e, a '-' was scanned. 0 means unknown */
    int tmp_num;				/* during token scanning loop */
    int code = 0, endcode = 0;
    byte *token;
    int copy, token_size;
    gx_device_clist_reader *crdev = (gx_device_clist_reader *)pdev;
    /* the following are used so we can go back to the original state */
    bool save_bg_print = false;                 /* arbitrary, silence warning */
    bool save_bandfile_open_close = false;      /* arbitrary, silence warning */
    gx_saved_page saved_page;
    clist_file_ptr saved_files[2];

    /* save the current (empty) page while we print  */
    if ((code = do_page_save(pdev, &saved_page, saved_files)) < 0) {
        emprintf(pdev->memory, "gx_saved_pages_list_print: Error getting device params\n");
        goto out;
    }

    /* save_page leaves the clist in writer mode, so prepare for reading clist */
    /* files. When we are done with printing, we'll go back to write mode.     */
    if ((code = clist_close_writer_and_init_reader((gx_device_clist *)pdev)) < 0)
        goto out;

    /* While printing, disable the saved_pages mode and bg_print */
    pdev->saved_pages_list = NULL;
    save_bg_print = pdev->bg_print_requested;

    /* Inhibits modifying the clist at end of output_page */
    save_bandfile_open_close = crdev->do_not_open_or_close_bandfiles;
    crdev->do_not_open_or_close_bandfiles = true;

    pdev->PageCount = list->PageCount;		/* adjust to value last printed */

    /* loop producing the number of copies */
    /* Note: list->collated_copies may change if 'copies' param follows the 'print' */
    for (copy = 1; copy <= list->collated_copies; copy++) {
        int page_skip = 0;
        bool do_blank_page_pad;
        int page;

        /* Set to start of 'print' params to do collated copies */
        param_scan = param;
        param_left = param_size;
        while ((token = param_parse_token(param_scan, param_left, &token_size)) != NULL) {
            saved_pages_key_enum key;

            page = 0;
            do_blank_page_pad = false;			/* only set for EVEN */
            key = param_find_key(token, token_size);
            switch (key) {
              case PARAM_DASH:
                if (start_page == 0) {
                    emprintf(pdev->memory, "gx_saved_pages_list_print: '-' unexpected\n");
                    code = gs_error_typecheck;
                    goto out;
                }
                end_page = -1;		/* the next number will end a range */
                break;

              case PARAM_STAR:
                page = list->count;		/* last page */
              case PARAM_NUMBER:
                if (page == 0) {
                     if (sscanf((const char *)token, "%d", &page) != 1) {
                         emprintf1(pdev->memory, "gx_saved_pages_list_print: Number format error '%s'\n", token);
                         code = gs_error_typecheck;
                         goto out;
                     }
                }
                if (start_page == 0) {
                    start_page = page;		/* first number seen */
                } else {
                    /* a second number was seen after the start_page was set */
                    if (end_page < 0) {
                        end_page = page;		/* end of a '# - #' range */
                        page_skip = (end_page >= start_page) ? 1 : -1;
                    } else {
                        /* 'page' was NOT part of a range after printing 'page' will start a new range */
                        end_page = start_page;	/* single page */
                        page_skip = 1;
                    }
                }
                break;

              case PARAM_COPIES:			/* copies requires a number next */
                /* Move to past 'copies' token */
                param_left -= token - param_scan + token_size;
                param_scan = token + token_size;

                if ((token = param_parse_token(param_scan, param_left, &token_size)) == NULL ||
                     param_find_key(token, token_size) != PARAM_NUMBER) {
                    emprintf(pdev->memory, "gx_saved_pages_list_print: copies not followed by number.\n");
                    code = gs_error_typecheck;
                    goto out;
                }
                if (sscanf((const char *)token, "%d", &tmp_num) != 1) {
                    emprintf1(pdev->memory, "gx_saved_pages_list_print: Number format error '%s'\n", token);
                    code = gs_error_typecheck;
                    goto out;
                }
                list->collated_copies = tmp_num;	/* save it for our loop */
                break;

              case PARAM_NORMAL:			/* sets both start and end */
                start_page = 1;
                end_page = list->count;
                page_skip = 1;
                break ;

              case PARAM_REVERSE:			/* sets both start and end */
                start_page = list->count;
                end_page = 1;
                page_skip = -1;
                break;

              case PARAM_EVEN:			/* sets both start and end */
                do_blank_page_pad = (list->count & 1) != 0;	/* pad if odd */
              case PARAM_EVEN0PAD:		/* sets both start and end */
                start_page = 2;
                end_page = list->count;
                page_skip = 2;
                break ;

              case PARAM_ODD:			/* sets both start and end */
                start_page = 1;
                end_page = list->count;
                page_skip = 2;
                break ;

              case PARAM_UNKNOWN:
              case PARAM_BEGIN:
              case PARAM_END:
              case PARAM_FLUSH:
              case PARAM_PRINT:
                token_size = 0;			/* non-print range token seen */
            }
            if (end_page > 0) {
                /* Here we have a range to print since start and end are known */
                int curr_page = start_page;
                gx_saved_pages_list_element *curr_elem = NULL;

                /* get the start_page saved_page */
                if (start_page <= list->count) {
                    for (curr_elem = list->head; curr_elem->sequence_number != start_page;
                                curr_elem = curr_elem->next) {
                        if (curr_elem->next == NULL) {
                             emprintf1(pdev->memory, "gx_saved_pages_list_print: page %d not found.\n", start_page);
                             code = gs_error_rangecheck;;
                             goto out;
                        }
                    }
                }

                for ( ; curr_elem != NULL; ) {

                    /* print the saved page from the current curr_elem */

                    if ((code = gx_output_saved_page(pdev, curr_elem->page)) < 0)
                        goto out;

                    curr_page += page_skip;
                    if (page_skip >= 0) {
                        if (curr_page > end_page)
                            break;
                        curr_elem = curr_elem->next;
                        if (page_skip > 1)
                            curr_elem = curr_elem->next;
                    } else {
                        /* reverse print order */
                        if (curr_page < end_page)
                            break;
                        curr_elem = curr_elem->prev;
                        /* Below is not needed since we never print reverse even/odd */
                        if (page_skip < -1)
                            curr_elem = curr_elem->prev;
                    }
                    if (curr_elem == NULL) {
                         emprintf1(pdev->memory, "gx_saved_pages_list_print: page %d not found.\n", curr_page);
                         code = gs_error_rangecheck;;
                         goto out;
                    }
                }

                /* If we were printing EVEN, we may need to spit out a blank 'pad' page */
                if (do_blank_page_pad) {
                    /* print the empty page we had upon entry */
                    /* FIXME: Note that the page size may not match the last odd page */
                    if ((code = gx_output_saved_page(pdev, &saved_page)) < 0)
                        goto out;
                }

                /* After printing, reset to handle next page range */
                start_page = (start_page == end_page) ? page : 0;   /* if single page, set start_page */
                                                                    /* from the number scanned above  */
                end_page = 0;
            }
            if (token_size == 0)
                break;				/* finished with print ranges */

            /* Move to next token */
            param_left -= token - param_scan + token_size;
            param_scan = token + token_size;
        }
    }
out:
    /* restore the device parameters saved upon entry */
    *printed_count = pdev->PageCount - list->PageCount;
    list->PageCount = pdev->PageCount;		/* retain for subsequent print action */
    pdev->saved_pages_list = list;
    pdev->bg_print_requested = save_bg_print;
    crdev->do_not_open_or_close_bandfiles = save_bandfile_open_close;

    /* load must be after we've set saved_pages_list which forces clist mode. */
    do_page_load(pdev, &saved_page, saved_files);

    /* Finally, do the finish page which will reset the clist to empty and write mode */
    endcode = clist_finish_page((gx_device *)pdev, true);
    return code < 0 ? code : endcode < 0 ? endcode : param_scan - param;
}

/*
 * Caller should make sure that this device supports saved_pages:
 * dev_proc(dev, dev_spec_op)(dev, gxdso_supports_saved_pages, NULL, 0) == 1
 *
 * Returns < 0 if error, 1 if erasepage needed, 0 if no action needed.
 */
int
gx_saved_pages_param_process(gx_device_printer *pdev, byte *param, int param_size)
{
    byte *param_scan = param;
    int param_left = param_size;
    byte *token;
    int token_size, code, printed_count, collated_copies = 1;
    int tmp_num;			/* during token scanning loop */
    int erasepage_needed = 0;

    while (pdev->child)
        pdev = (gx_device_printer *)pdev->child;

    while ((token = param_parse_token(param_scan, param_left, &token_size)) != NULL) {

        switch (param_find_key(token, token_size)) {
          case PARAM_BEGIN:
            if (pdev->saved_pages_list == NULL) {
                if ((pdev->saved_pages_list = gx_saved_pages_list_new(pdev)) == NULL)
                    return_error(gs_error_VMerror);
                pdev->finalize = gdev_prn_finalize;	/* set to make sure the list gets freed */

                /* We need to change to clist mode. Always uses clist when saving pages */
                pdev->saved_pages_list->save_banding_type = pdev->space_params.banding_type;
                pdev->space_params.banding_type = BandingAlways;
                if ((code = gdev_prn_reallocate_memory((gx_device *)pdev, &pdev->space_params, pdev->width, pdev->height)) < 0)
                    return code;
                erasepage_needed |= 1;
            }
            break;

          case PARAM_END:
            if (pdev->saved_pages_list != NULL) {
                /* restore to what was set before the "begin" */
                pdev->space_params.banding_type = pdev->saved_pages_list->save_banding_type;
                gx_saved_pages_list_free(pdev->saved_pages_list);
                pdev->saved_pages_list = NULL;
                /* We may need to change from clist mode since we forced clist when saving pages */
                code = gdev_prn_reallocate_memory((gx_device *)pdev, &pdev->space_params, pdev->width, pdev->height);
                if (code < 0)
                    return code;
                erasepage_needed |= 1;       /* make sure next page is erased */
            }
            break;

          case PARAM_FLUSH:
            if (pdev->saved_pages_list != NULL) {
                /* Save the collated copy count so the list we return will have it */
                collated_copies = pdev->saved_pages_list->collated_copies;
                gx_saved_pages_list_free(pdev->saved_pages_list);
            }
            /* Always return with an empty list, even if we weren't saving previously */
            if ((pdev->saved_pages_list = gx_saved_pages_list_new(pdev)) == NULL)
                return_error(gs_error_VMerror);
            pdev->finalize = gdev_prn_finalize;	/* set to make sure the list gets freed */
            /* restore the original count */
            pdev->saved_pages_list->collated_copies = collated_copies;
            break;

          case PARAM_COPIES:			/* copies requires a number next */
            /* make sure that we have a list */
            if (pdev->saved_pages_list == NULL) {
                return_error(gs_error_rangecheck);	/* copies not allowed before a 'begin' */
            }
            /* Move to past 'copies' token */
            param_left -= token - param_scan + token_size;
            param_scan = token + token_size;

            if ((token = param_parse_token(param_scan, param_left, &token_size)) == NULL ||
                 param_find_key(token, token_size) != PARAM_NUMBER) {
                emprintf(pdev->memory, "gx_saved_pages_param_process: copies not followed by number.\n");
                return_error(gs_error_typecheck);
            }
            if (sscanf((const char *)token, "%d", &tmp_num) != 1) {
                emprintf1(pdev->memory, "gx_saved_pages_list_print: Number format error '%s'\n", token);
                code = gs_error_typecheck;
                return code;
            }
            pdev->saved_pages_list->collated_copies = tmp_num;	/* save it for our loop */
            break;

          case PARAM_PRINT:
            /* Move to past 'print' token */
            param_left -= token - param_scan + token_size;
            param_scan = token + token_size;

            code = param_left;			/* in case list is NULL, skip rest of string */
            if (pdev->saved_pages_list != NULL) {
                if ((code = gx_saved_pages_list_print(pdev, pdev->saved_pages_list,
                                                 param_scan, param_left, &printed_count)) < 0)
                    return code;
                erasepage_needed |= 1;       /* make sure next page is erased */
            }
            /* adjust for all of the print parameters */
            token_size += code;
            break;

          /* We are expecting an action keyword, so other keywords and tokens */
          /* are not valid here (mostly the 'print' parameters).              */
          default:
            {
                byte *bad_token = gs_alloc_string(pdev->memory, token_size+1, "saved_pages_param_process");
                byte *param_string = gs_alloc_string(pdev->memory, param_size+1, "saved_pages_param_process");
                if (bad_token != NULL && param_string != NULL) {
                    memcpy(bad_token, token, token_size);
                    bad_token[token_size] = 0;          /* terminate string */
                    memcpy(param_string, param, param_size);
                    param_string[param_size] = 0;          /* terminate string */
                    emprintf2(pdev->memory, "*** Invalid saved-pages token '%s'\n *** in param string '%s'\n",
                              bad_token, param_string);
                    gs_free_string(pdev->memory, bad_token, token_size+1,  "saved_pages_param_process");
                    gs_free_string(pdev->memory, param_string, param_size+1,  "saved_pages_param_process");
                }
            }
        }
        /* Move to next token */
        param_left -= token - param_scan + token_size;
        param_scan = token + token_size;

    }
    return erasepage_needed;
}
