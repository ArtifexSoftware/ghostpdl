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


/* Command list procedures for saved pages */
/* Requires gdevprn.h, gxclist.h */

#ifndef gxclpage_INCLUDED
#  define gxclpage_INCLUDED

#include "gxclist.h"		/* for gx_saved_page struct */

typedef struct gx_saved_pages_list_element_s gx_saved_pages_list_element;

struct gx_saved_pages_list_element_s {
    int sequence_number;		/* to cross check position in list */
    gx_saved_pages_list_element *prev;
    gx_saved_pages_list_element *next;
    gx_saved_page *page;
};

typedef struct gx_saved_pages_list_s {
    int PageCount;		        /* Page Count to start with on next 'print' action */
    int count;				/* number of pages in the list */
    int collated_copies;		/* how many copies of the job to print */
    int save_banding_type;		/* to restore when we "end" */
    gx_saved_pages_list_element *head;
    gx_saved_pages_list_element *tail;
    gs_memory_t *mem;			/* allocator used for this struct and list_elements */
} gx_saved_pages_list;


/* ---------------- Procedures ---------------- */

/*
 * Package up the current page in a banding device as a page object.
 * The client must provide storage for the page object.
 * The client may retain the object in memory, or may write it on a file
 * for later retrieval; in the latter case, the client should free the
 * in-memory structure.
 */
int gdev_prn_save_page(gx_device_printer * pdev, gx_saved_page * page);

/*
 * Render an array of saved pages by setting up a modified get_bits
 * procedure and then calling the device's normal output_page procedure.
 * Any current page in the device's buffers is lost.
 * The (0,0) point of each saved page is translated to the corresponding
 * specified offset on the combined page.  (Currently the Y offset must be 0.)
 * The client is responsible for freeing the saved and placed pages.
 *
 * Note that the device instance for rendering need not be, and normally is
 * not, the same as the device from which the pages were saved, but it must
 * be an instance of the same device.  The client is responsible for
 * ensuring that the rendering device's buffer size (BufferSpace value) is
 * the same as the BandBufferSpace value of all the saved pages, and that
 * the device width is the same as the BandWidth value of the saved pages.
 */
int gdev_prn_render_pages(gx_device_printer * pdev,
                          const gx_placed_page * ppages, int count);

/*
 * Allocate and initialize the list structure
 */
gx_saved_pages_list *gx_saved_pages_list_new(gx_device_printer *);

/*
 * Add a saved page to the end of an in memory list. This allocates
 * a new gx_saved_page structure, saves the page to it and links it
 * on to the end of the list. The list->mem is used as the allocator
 * so that list_free can free the contents. This relies on save_page
 * to use non_gc_memory for the stuff it allocates.
 */
int gx_saved_pages_list_add(gx_device_printer * pdev);

/* Free the contents of all saved pages, unlink the files and free the
 * saved_page structures and free the gx_saved_pages_list struct.
 */
void gx_saved_pages_list_free(gx_saved_pages_list *list);

/*
 * Process the param control string.
 * Returns < 0 if an error, > 0 if OK, but erasepage is needed, otherwise 0.
 */
int gx_saved_pages_param_process(gx_device_printer *pdev, byte *param, int param_size);

/*
 * Print selected pages from the list to on the selected device. The
 * saved_pages_list is NOT modified, allowing for reprint / recovery
 * print. Each saved_page is printed on a separate page (unlike the
 * gdev_prn_render_pages above which prints several saved_pages on
 * a single "master" page, performing imposition).
 *
 * This is primarily intended to allow printing in non-standard order
 * (reverse, odd then even) or producing collated copies for a job.
 *
 * On success return the number of bytes consumed or error code < 0.
 * The printed_count will contain the number of pages printed.
 *
 */
int gx_saved_pages_list_print(gx_device_printer *pdev, gx_saved_pages_list *list,
                              byte *control, int control_size, int *printed_count);
#endif /* gxclpage_INCLUDED */
