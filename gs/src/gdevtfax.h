/* Copyright (C) 1999, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Entry points to the TIFF/fax writing driver */

#ifndef gdevtfax_INCLUDED
#  define gdevtfax_INCLUDED

int gdev_fax_print_page_stripped(P4(gx_device_printer *pdev, FILE *prn_stream,
				    stream_CFE_state *ss, long rows_per_strip));

#endif /* gdevtfax_INCLUDED */
