/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pximpl.c */
/* pl_interp_implementation descriptor for PCL XL */

#include "memory_.h"
#include "scommon.h"
#include "gxdevice.h"
#include "pltop.h"

extern const pl_interp_implementation_t pxl_implementation;

/* Zero-terminated list of pointers to implementations */
pl_interp_implementation_t const * const pdl_implementation[] = {
	&pxl_implementation,
	0
};


