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

/*$RCSfile$ $Revision$ */
/* Definition of run-length encoded memory device */

#ifndef gdevmrun_INCLUDED
#  define gdevmrun_INCLUDED

/*
 * This memory device stores full-size pixels with run-length
 * encoding if possible, switching to the standard uncompressed
 * representation if necessary.
 */

#include "gxdevmem.h"

/*
 * Define the device, built on a memory device.
 */
typedef struct gx_device_run_s {
    gx_device_memory md;	/* must be first */
    uint runs_per_line;
    int umin, umax1;		/* some range of uninitialized lines */
    int smin, smax1;		/* some range in standard (not run) form */
    /*
     * Save memory device procedures that we replace with run-oriented
     * ones, for use with the uncompressed representation.
     */
    struct sp_ {
	dev_proc_copy_mono((*copy_mono));
	dev_proc_copy_color((*copy_color));
	dev_proc_fill_rectangle((*fill_rectangle));
	dev_proc_copy_alpha((*copy_alpha));
	dev_proc_strip_tile_rectangle((*strip_tile_rectangle));
	dev_proc_strip_copy_rop((*strip_copy_rop));
	dev_proc_get_bits_rectangle((*get_bits_rectangle));
    } save_procs;
} gx_device_run;

/*
 * Convert a memory device to run-length form.  The mdev argument should be
 * const, but it isn't because we need to call gx_device_white.
 */
int gdev_run_from_mem(gx_device_run *rdev, gx_device_memory *mdev);

#endif /* gdevmrun_INCLUDED */
