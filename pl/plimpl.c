/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcimpl.c - PCL5c & PCL/XL pl_interp_implementation_t descriptor */

#include "memory_.h"
#include "scommon.h"
#include "gxdevice.h"
#include "pltop.h"

extern pl_interp_implementation_t pcl_implementation;
extern pl_interp_implementation_t pxl_implementation;

/* Zero-terminated list of pointers to implementations */
pl_interp_implementation_t const * const pdl_implementation[] = {
	&pcl_implementation,
	&pxl_implementation,
	0
};

