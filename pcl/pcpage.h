/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pcpage.c - PCL5 page and transformation control interface */

#ifndef pcpage_INCLUDED
#define pcpage_INCLUDED

#include "pcstate.h"
#include "pcommand.h"

/* set the page output procedure */
void pcl_set_end_page(int (*procp)(pcl_state_t *, int, int));

/*
 * End a page, either unconditionally or only if there are marks on it.
 * Return 1 if the page was actually printed and erased.
 */
typedef enum {
    pcl_print_always,
    pcl_print_if_marked
} pcl_print_condition_t;

int pcl_end_page(
    pcl_state_t *           pcs,
    pcl_print_condition_t   condition
);

void new_logical_page(
    pcl_state_t *               pcs,
    int                         lp_orient,
    const pcl_paper_size_t *    psize,
    bool                        reset_initial,
    bool                        for_passthrough
);

 int
pcl_getdevice_initial_matrix(
     pcl_state_t *       pcs,
     gs_matrix *         mat
);

bool pcl_page_marked(
    pcl_state_t *           pcs
);

void pcl_mark_page_for_path(pcl_state_t *pcs);
void pcl_mark_page_for_current_pos(pcl_state_t *pcs);
int new_logical_page_for_passthrough_snippet(pcl_state_t *pcs, int orient, int tag);
pcl_paper_size_t *pcl_get_default_paper(pcl_state_t *pcs);
int pcl_new_logical_page_for_passthrough(pcl_state_t *pcs, int orient, gs_point *pdims);

#define pcl_end_page_always(pcs)    pcl_end_page((pcs), pcl_print_always)
#define pcl_end_page_if_marked(pcs) pcl_end_page((pcs), pcl_print_if_marked)

extern  const pcl_init_t    pcpage_init;

#endif			/* pcpage_INCLUDED */
