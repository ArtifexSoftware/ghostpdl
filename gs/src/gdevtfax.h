/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Entry points to the TIFF/fax writing driver */

#ifndef gdevtfax_INCLUDED
#  define gdevtfax_INCLUDED

int gdev_fax_open(P1(gx_device *));
void gdev_fax_init_state(P2(stream_CFE_state *, const gx_device_printer *));
int gdev_fax_print_page(P3(gx_device_printer *, FILE *, stream_CFE_state *));

#endif /* gdevtfax_INCLUDED */
