/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Structures for ICCBased color space parameters */
/* requires: gspsace.h, gscolor2.h */

#ifndef gsicc_INCLUDED
#  define gsicc_INCLUDED

#include "gscie.h"
#include "gxcspace.h"		
/*
 * Build an ICCBased color space.
 *
 */
extern  int     gs_cspace_build_ICC( gs_color_space **   ppcspace,
					void *              client_data,
					gs_memory_t *       pmem );

extern const gs_color_space_type gs_color_space_type_ICC;

#endif /* gsicc_INCLUDED */
