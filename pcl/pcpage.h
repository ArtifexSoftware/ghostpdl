/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcpage.c - PCL5 page and transformation control interface */

#ifndef pcpage_INCLUDED
#define pcpage_INCLUDED

#include "pcstate.h"
#include "pcommand.h"

/* set the page output procedure */
extern  void    pcl_set_end_page( int (*procp)(pcl_state_t *, int, int) );

/*
 * End a page, either unconditionally or only if there are marks on it.
 * Return 1 if the page was actually printed and erased.
 */
typedef enum {
    pcl_print_always,
    pcl_print_if_marked
} pcl_print_condition_t;

extern  int     pcl_end_page(
    pcl_state_t *           pcs,
    pcl_print_condition_t   condition
);

#define pcl_end_page_always(pcs)    pcl_end_page((pcs), pcl_print_always)
#define pcl_end_page_if_marked(pcs) pcl_end_page((pcs), pcl_print_if_marked)

extern  const pcl_init_t    pcpage_init;

#endif			/* pcpage_INCLUDED */
