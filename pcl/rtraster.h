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

/* rtraster.h - interface to raster rendering code */

#ifndef rtraster_INCLUDED
#define rtraster_INCLUDED

#include "pcstate.h"
#include "pcommand.h"

/*
 * Create a PCL raster object (on entering raster graphics mode).
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
extern  int     pcl_start_raster(
    uint                src_width,
    uint                src_height,
    pcl_state_t *       pcs
);

/* complete a raster (when exiting raster graphics mode) */
extern  void    pcl_complete_raster( void );

extern  const pcl_init_t    rtraster_init;

#endif			/* rtraster_INCLUDED */
