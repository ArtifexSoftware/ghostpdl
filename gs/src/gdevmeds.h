/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Interface for gdevmeds.c */

#ifndef gdevmeds_INCLUDED
#  define gdevmeds_INCLUDED

#include "gdevprn.h"

int select_medium(P3(gx_device_printer *pdev, const char **available,
		     int default_index));

#endif /* gdevmeds_INCLUDED */
