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


/* pcpage.c - PCL5 page and transformation control interface */

#ifndef pcpage_INCLUDED
#define pcpage_INCLUDED

#include "pcstate.h"
#include "pcommand.h"

/* set the page output procedure */
void pcl_set_end_page(int (*procp) (pcl_state_t *, int, int));

/*
 * End a page, either unconditionally or only if there are marks on it.
 * Return 1 if the page was actually printed and erased.
 */
typedef enum
{
    pcl_print_always,
    pcl_print_if_marked
} pcl_print_condition_t;

int pcl_end_page(pcl_state_t * pcs, pcl_print_condition_t condition);

int new_logical_page(pcl_state_t * pcs,
                      int lp_orient,
                      const pcl_paper_size_t * psize,
                      bool reset_initial, bool for_passthrough);

int pcl_getdevice_initial_matrix(pcl_state_t * pcs, gs_matrix * mat);

bool pcl_page_marked(pcl_state_t * pcs);

bool pcl_cursor_moved(pcl_state_t * pcs);

int pcl_mark_page_for_path(pcl_state_t * pcs);

int pcl_mark_page_for_current_pos(pcl_state_t * pcs);

int pcl_mark_page_for_character(pcl_state_t * pcs, gs_fixed_point *org);

int new_logical_page_for_passthrough_snippet(pcl_state_t * pcs, int orient,
                                             int tag);
pcl_paper_size_t *pcl_get_default_paper(pcl_state_t * pcs);

int pcl_new_logical_page_for_passthrough(pcl_state_t * pcs, int orient,
                                         gs_point * pdims);

/* We export this for HPGL/2 which uses the PCL custom paper
   size code to set up plots of arbitrary size */
int pcl_set_custom_paper_size(pcl_state_t *pcs, pcl_paper_size_t *p);

#define pcl_end_page_always(pcs)    pcl_end_page((pcs), pcl_print_always)
#define pcl_end_page_if_marked(pcs) pcl_end_page((pcs), pcl_print_if_marked)

extern const pcl_init_t pcpage_init;

#endif /* pcpage_INCLUDED */
