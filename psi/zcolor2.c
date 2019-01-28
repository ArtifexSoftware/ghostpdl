/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


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
static int
zusealternate(i_ctx_t * i_ctx_p)
{
    os_ptr                  op = osp;
    const gs_color_space *  pcs = gs_currentcolorspace(igs);

    push(1);
    make_bool(op, pcs->base_space != 0);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def    zcolor2_l2_op_defs[] = {
    op_def_begin_level2(),
    { "0.usealternate", zusealternate },
    op_def_end(0)
};
