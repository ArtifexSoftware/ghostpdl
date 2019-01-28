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


/* RasterOp / transparency accessing for library */
#include "gx.h"
#include "gserrors.h"
#include "gzstate.h"
#include "gsrop.h"

/* setrasterop */
int
gs_setrasterop(gs_gstate * pgs, gs_rop3_t rop)
{
    if (pgs->in_cachedevice)
        return_error(gs_error_undefined);
    pgs->log_op = (rop & rop3_1) | (pgs->log_op & ~rop3_1);
    return 0;
}

/* currentrasterop */
gs_rop3_t
gs_currentrasterop(const gs_gstate * pgs)
{
    return lop_rop(pgs->log_op);
}

/* setsourcetransparent */
int
gs_setsourcetransparent(gs_gstate * pgs, bool transparent)
{
    if (pgs->in_cachedevice)
        return_error(gs_error_undefined);
    pgs->log_op =
        (transparent ? pgs->log_op | lop_S_transparent :
         pgs->log_op & ~lop_S_transparent);
    return 0;
}

/* currentsourcetransparent */
bool
gs_currentsourcetransparent(const gs_gstate * pgs)
{
    return (pgs->log_op & lop_S_transparent) != 0;
}

/* settexturetransparent */
int
gs_settexturetransparent(gs_gstate * pgs, bool transparent)
{
    if (pgs->in_cachedevice)
        return_error(gs_error_undefined);
    pgs->log_op =
        (transparent ? pgs->log_op | lop_T_transparent :
         pgs->log_op & ~lop_T_transparent);
    return 0;
}

/* currenttexturetransparent */
bool
gs_currenttexturetransparent(const gs_gstate * pgs)
{
    return (pgs->log_op & lop_T_transparent) != 0;
}

/* Save/restore logical operation.  (For internal use only.) */
int
gs_set_logical_op(gs_gstate * pgs, gs_logical_operation_t lop)
{
    pgs->log_op = lop;
    return 0;
}
gs_logical_operation_t
gs_current_logical_op(const gs_gstate * pgs)
{
    return pgs->log_op;
}
