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
/* Level 2 color operators */
#include "ghost.h"
#include "string_.h"
#include "oper.h"
#include "gsmatrix.h"
#include "gsstruct.h"
#include "gxcspace.h"
#include "gscolor2.h"
#include "igstate.h"
#include "store.h"


/*
 *  -   .useralternate   <bool>
 *
 * Push true if the current color space contains a base or alternate
 * color space and makes use of that color space (e.g.: a Separation
 * color space for a component not supported by the process color model.
 */
private int
zusealternate(i_ctx_t * i_ctx_p)
{
    os_ptr                  op = osp;
    const gs_color_space *  pcs = gs_currentcolorspace(igs);

    push(1);
    make_bool(op, cs_base_space(pcs) != 0);
    return 0;
}


/* ------ Initialization procedure ------ */

const op_def    zcolor2_l2_op_defs[] = {
    op_def_begin_level2(),
    { "0.usealternate", zusealternate },
    op_def_end(0)
};
