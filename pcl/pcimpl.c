/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcimpl.c - PCL5c pl_interp_implementation_t descriptor */

#include "memory_.h"
#include "scommon.h"
#include "gxdevice.h"
#include "pltop.h"

extern const pl_interp_implementation_t pcl_implementation;

/* Zero-terminated list of pointers to implementations */
pl_interp_implementation_t const * const pdl_implementation[] = {
	&pcl_implementation,
	0
};

