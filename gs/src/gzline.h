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
/* Line parameter implementation */

#ifndef gzline_INCLUDED
#  define gzline_INCLUDED

#include "gxline.h"

/*
 * The GC procedures are complex because we must not attempt to follow
 * the pattern pointer iff the pattern size is 0.
 */
#define private_st_line_params() /* in gsistate.c */\
  gs_private_st_complex_only(st_line_params, gx_line_params, "line_params",\
    0, line_params_enum_ptrs, line_params_reloc_ptrs, 0)
#define st_line_params_num_ptrs 1

/* Internal accessor for line parameters in graphics state */
const gx_line_params *gs_currentlineparams(const gs_imager_state *);

#endif /* gzline_INCLUDED */
