/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcpage.c - PCL5 page and transformation control interface */

#ifndef pcpage_INCLUDED
#define pcpage_INCLUDED

#include "pcstate.h"
#include "pcommand.h"

/* set the page output procedure */
void pcl_set_end_page(P1(int (*procp)(pcl_state_t *, int, int)));

/*
 * End a page, either unconditionally or only if there are marks on it.
 * Return 1 if the page was actually printed and erased.
 */
typedef enum {
    pcl_print_always,
    pcl_print_if_marked
} pcl_print_condition_t;

int pcl_end_page(P2(
    pcl_state_t *           pcs,
    pcl_print_condition_t   condition
));

void new_logical_page(P4(
    pcl_state_t *               pcs,
    int                         lp_orient,
    const pcl_paper_size_t *    psize,
    bool                        reset_initial
));

 int
pcl_getdevice_initial_matrix(P2(pcl_state_t *       pcs,
				gs_matrix *         mat
));

 int
pcl_current_bounding_box(P2(pcl_state_t *           pcs,
			    gs_rect *               pbbox
));

bool pcl_page_marked(P1(
    pcl_state_t *           pcs
));;

#define pcl_end_page_always(pcs)    pcl_end_page((pcs), pcl_print_always)
#define pcl_end_page_if_marked(pcs) pcl_end_page((pcs), pcl_print_if_marked)

extern  const pcl_init_t    pcpage_init;

#endif			/* pcpage_INCLUDED */
